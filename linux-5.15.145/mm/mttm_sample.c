/*
 * It includes ksampled and miscellaneous function.
 * ksampled handles PEBS samples.
 */

#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/perf_event.h>
#include <linux/memcontrol.h>
#include <linux/mempolicy.h>
#include <linux/syscalls.h>
#include <linux/mm.h>
#include <linux/kernel.h>
#include <linux/mm_inline.h>
#include <linux/swap.h>
#include <linux/pid.h>
#include <linux/sched/task.h>
#include <asm/pgtable.h>

#include <trace/events/mttm.h>
#include "../kernel/events/internal.h"
#include "internal.h"
#include <linux/mttm.h>

int enable_ksampled = 0;
unsigned long pebs_sample_period = 10007;
int current_tenants = 0;
struct task_struct *ksampled_thread = NULL;
struct perf_event ***pfe;
DEFINE_SPINLOCK(register_lock);

bool node_is_toptier(int nid)
{
	return (nid == 0) ? true : false;
}

struct page *get_meta_page(struct page *page)
{
	page = compound_head(page);
	return &page[3];
}

static unsigned int get_accesses_from_idx(unsigned int idx)
{
	unsigned int accesses = 1;
	if(idx == 0)
		return 0;

	while(idx--) {
		accesses <<= 1;
	}

	return accesses;
}

unsigned int get_idx(unsigned long num)
{
	unsigned int cnt = 0;

	num++;
	while (1) {
		num = num >> 1;
		if(num)
			cnt++;
		else
			return cnt;
		if(cnt == 15)
			break;
	}

	return cnt;
}

int set_page_coolstatus(struct page *page, pte_t *pte, struct mm_struct *mm)
{
	struct mem_cgroup *memcg = get_mem_cgroup_from_mm(mm);
	struct page *pte_page;
	pginfo_t *pginfo;
	int initial_hotness;

	if(!memcg)
		return 0;

	pte_page = virt_to_page((unsigned long)pte);
	if(!PageMttm(pte_page))
		return 0;

	pginfo = get_pginfo_from_pte(pte);
	if(!pginfo)
		return 0;

	initial_hotness = 0; //get_accesses_from_idx(memcg->active_threshold + 1);

	pginfo->nr_accesses = initial_hotness;
	pginfo->cooling_clock = READ_ONCE(memcg->cooling_clock);//do not skip cooling

	return 0;
}

void mttm_mm_init(struct mm_struct *mm)
{
	if(current->mm) {
		if(current->mm->mttm_enabled) {
			mm->mttm_enabled = true;
			return;
		}
	}
	mm->mttm_enabled = false;
}

void __prep_transhuge_page_for_mttm(struct mm_struct *mm, struct page *page)
{
	struct mem_cgroup *memcg = mm ? get_mem_cgroup_from_mm(mm) : NULL;
	int initial_hotness = 0; //memcg ? get_accesses_from_idx(memcg->active_threshold + 1) : 0;

	page[3].nr_accesses = initial_hotness;
	SetPageMttm(&page[3]);
	
	if(!memcg)
		return;

	page[3].cooling_clock = memcg->cooling_clock;//do not skip cooling
	ClearPageActive(page);
}

void prep_transhuge_page_for_mttm(struct vm_area_struct *vma,
					struct page *page)
{
	prep_transhuge_page(page);

	if(vma->vm_mm->mttm_enabled)
		__prep_transhuge_page_for_mttm(vma->vm_mm, page);
}

void copy_transhuge_pginfo(struct page *page, struct page *newpage)
{
	VM_BUG_ON_PAGE(!PageCompound(page), page);
	VM_BUG_ON_PAGE(!PageCompound(newpage), newpage);

	page = compound_head(page);
	newpage = compound_head(newpage);

	if(!PageMttm(&page[3]))
		return ;

	newpage[3].nr_accesses = page[3].nr_accesses;
	newpage[3].cooling_clock = page[3].cooling_clock;
	SetPageMttm(&newpage[3]);
}

void free_pginfo_pte(struct page *pte)
{
	if(!PageMttm(pte))
		return;
	BUG_ON(pte->pginfo == NULL);
	kmem_cache_free(pginfo_cache, pte->pginfo);
	pte->pginfo = NULL;
	ClearPageMttm(pte);
}

void uncharge_mttm_pte(pte_t *pte, struct mem_cgroup *memcg)
{
	struct page *pte_page;
	unsigned int idx;
	pginfo_t *pginfo;

	if(!memcg)
		return;

	pte_page = virt_to_page((unsigned long)pte);
	if(!PageMttm(pte_page))
		return;

	pginfo = get_pginfo_from_pte(pte);
	if(!pginfo)
		return;

	idx = get_idx(pginfo->nr_accesses);

	spin_lock(&memcg->access_lock);
	if(memcg->hotness_hg[idx] > 0)
		memcg->hotness_hg[idx]--;
	spin_unlock(&memcg->access_lock);
}

void uncharge_mttm_page(struct page *page, struct mem_cgroup *memcg)
{
	unsigned int nr_pages = thp_nr_pages(page);
	unsigned int idx;

	if(!memcg)
		return;

	page = compound_head(page);
	if(nr_pages != 1) {
		struct page *meta_page = get_meta_page(page);
		idx = get_idx(meta_page->nr_accesses);

		spin_lock(&memcg->access_lock);
		if(memcg->hotness_hg[idx] >= nr_pages)
			memcg->hotness_hg[idx] -= nr_pages;
		else
			memcg->hotness_hg[idx] = 0;
		spin_unlock(&memcg->access_lock);
	}
}

unsigned long get_nr_lru_pages_node(struct mem_cgroup *memcg, pg_data_t *pgdat)
{
	struct lruvec *lruvec;
	unsigned long nr_pages = 0;
	enum lru_list lru;

	lruvec = mem_cgroup_lruvec(memcg, pgdat);

	for_each_lru(lru)
		nr_pages += lruvec_lru_size(lruvec, lru, MAX_NR_ZONES);

	return nr_pages;
}

static bool valid_va(unsigned long addr)
{
	if(!(addr >> (PGDIR_SHIFT + 9)) && addr != 0)
		return true;
	else
		return false;
}

static __u64 get_pebs_event(enum eventtype e)
{
	switch(e) {
		case DRAMREAD:
			return DRAM_LLC_LOAD_MISS;
		case CXLREAD:
			return REMOTE_DRAM_LLC_LOAD_MISS;
		default:
			return NR_EVENTTYPE;
	}
}

static int __perf_event_open(__u64 config, __u64 config1, __u64 cpu,
        __u64 type)
{
	struct perf_event_attr attr;
	struct file *file;
	int event_fd;

	memset(&attr, 0, sizeof(struct perf_event_attr));

	attr.type = PERF_TYPE_RAW;
	attr.size = sizeof(struct perf_event_attr);
	attr.config = config;
	attr.config1 = config1;

	attr.sample_period = pebs_sample_period;

	attr.sample_type = PERF_SAMPLE_IP | PERF_SAMPLE_TID | PERF_SAMPLE_ADDR;

	attr.disabled = 0;
	attr.exclude_kernel = 1;
	attr.exclude_hv = 1;
	attr.exclude_callchain_kernel = 1;
	attr.exclude_callchain_user = 1;
	attr.precise_ip = 1;
	/*attr.enable_on_exec = 1;
	attr.comm = 1;
	attr.comm_exec = 1;
	*/

	event_fd = mttm_perf_event_open(&attr, -1, cpu, -1, 0);
	if (event_fd <= 0) {
		pr_info("[%s] perf_event_open fail. event_fd: %d\n",__func__, event_fd);
		return -1;
	}

	file = fget(event_fd);
	if (!file) {
		pr_info("[%s] invalid file\n",__func__);
		return -1;
	}

	pfe[cpu][type] = fget(event_fd)->private_data;
	return 0;
}
#if 0
pid_t test_pid;

static unsigned long get_active_lru_size(void) {
	struct pid *pid_struct = find_get_pid(test_pid);
	struct task_struct *p = pid_struct ? pid_task(pid_struct, PIDTYPE_PID) : NULL;
	struct mm_struct *mm = p ? p->mm : NULL;
	struct mem_cgroup *memcg;
	struct lruvec *lruvec;
	unsigned long ret = 0;
	
	if(!mm)
		goto put_task;
	if(!mm->mttm_enabled) {
		goto put_task;
	}
	if(!mmap_read_trylock(mm))
		goto put_task;

	memcg = get_mem_cgroup_from_mm(mm);
	if(!memcg)
		goto mmap_unlock;

	lruvec = mem_cgroup_lruvec(memcg, NODE_DATA(0));
	ret = lruvec_lru_size(lruvec, LRU_ACTIVE_ANON, MAX_NR_ZONES);
	lruvec = mem_cgroup_lruvec(memcg, NODE_DATA(1));
	ret += lruvec_lru_size(lruvec, LRU_ACTIVE_ANON, MAX_NR_ZONES);

mmap_unlock:
	mmap_read_unlock(mm);
put_task:
	put_pid(pid_struct);

	return ret;
}
#endif
SYSCALL_DEFINE1(mttm_register_pid,
		pid_t, pid)
{		
	spin_lock(&register_lock);

	if(current_tenants == LIMIT_TENANTS) {
		pr_info("[%s] Can't register tenant due to limit\n",__func__);
		spin_unlock(&register_lock);
		return 0;
	}

	current->mm->mttm_enabled = true;
	if(kmigrated_init(mem_cgroup_from_task(current)))
		pr_info("[%s] failed to start kmigrated\n",__func__);

	current_tenants++;
	//test_pid = pid;
	spin_unlock(&register_lock);
	pr_info("[%s] registered pid : %d. current_tenants : %d, memcg id : %d\n",
		__func__, pid, current_tenants, mem_cgroup_id(mem_cgroup_from_task(current)));
	
	return 0;
}

SYSCALL_DEFINE1(mttm_unregister_pid,
		pid_t, pid)
{
	spin_lock(&register_lock);

	current_tenants--;
	kmigrated_stop(mem_cgroup_from_task(current));

	spin_unlock(&register_lock);
	pr_info("[%s] unregistered pid : %d, current_tenants : %d\n",
		__func__, pid, current_tenants);
	return 0;
}

static bool need_memcg_cooling(struct mem_cgroup *memcg)
{
	unsigned long usage = page_counter_read(&memcg->memory);
	if(memcg->nr_alloc + MTTM_THRES_COOLING_ALLOC <= usage) {
		pr_info("[%s] memcg->nr_alloc: %lu, usage: %lu\n",
			__func__, memcg->nr_alloc, usage);
		memcg->nr_alloc = usage;
		return true;
	}
	return false;
}

static void set_lru_cooling(struct mem_cgroup *memcg)
{
	struct mem_cgroup_per_node *pn;
	int nid;

	if(!memcg)
		return;

	for_each_node_state(nid, N_MEMORY) {
		pn = memcg->nodeinfo[nid];
		if(!pn)
			continue;
		WRITE_ONCE(pn->need_cooling, true);
	}
}

static void reset_memcg_stat(struct mem_cgroup *memcg)
{
	int i;
	for(i = 0; i < 16; i++){
		memcg->hotness_hg[i] = 0;
	}
}

static bool set_cooling(struct mem_cgroup *memcg)
{
	int nid;
	
	for_each_node_state(nid, N_MEMORY) {
		struct mem_cgroup_per_node *pn = memcg->nodeinfo[nid];
		if(pn && READ_ONCE(pn->need_cooling)) { // previous cooling is not done yet.
			spin_lock(&memcg->access_lock);
			memcg->cooling_clock++;
			spin_unlock(&memcg->access_lock);
			return false;
		}
	}

	spin_lock(&memcg->access_lock);
	reset_memcg_stat(memcg);
	memcg->cooling_clock++;
	memcg->cooled = true;
	smp_mb();
	spin_unlock(&memcg->access_lock);

	set_lru_cooling(memcg);
	
	return true;
}

void set_lru_adjusting(struct mem_cgroup *memcg, bool inc_thres)
{
	struct mem_cgroup_per_node *pn;
	int nid;

	for_each_node_state(nid, N_MEMORY) {
		pn = memcg->nodeinfo[nid];
		if(!pn)
			continue;
		WRITE_ONCE(pn->need_adjusting, true);
		if(inc_thres)
			WRITE_ONCE(pn->need_adjusting_all, true);

	}

}

static void adjust_active_threshold(struct mem_cgroup *memcg)
{
	unsigned long nr_active = 0;
	unsigned long max_nr_pages = memcg->max_nr_dram_pages -
		get_memcg_promotion_wmark(memcg->max_nr_dram_pages);
	bool need_warm = false;
	int idx_hot;

	spin_lock(&memcg->access_lock);

	for(idx_hot = 0; idx_hot < 8; idx_hot++)
		nr_active += memcg->hotness_hg[idx_hot];
	pr_info("[%s] hotness_hg 0 ~ 7 : %lu %lu %lu %lu %lu %lu %lu %lu [%lu]\n",
		__func__, memcg->hotness_hg[0], memcg->hotness_hg[1], memcg->hotness_hg[2], memcg->hotness_hg[3],
		memcg->hotness_hg[4], memcg->hotness_hg[5], memcg->hotness_hg[6], memcg->hotness_hg[7], nr_active);

	nr_active = 0;
	for(idx_hot = 8; idx_hot < 16; idx_hot++)
		nr_active += memcg->hotness_hg[idx_hot];	
	pr_info("[%s] hotness_hg 8 ~ 15 : %lu %lu %lu %lu %lu %lu %lu %lu [%lu]\n",
		__func__, memcg->hotness_hg[8], memcg->hotness_hg[9], memcg->hotness_hg[10], memcg->hotness_hg[11],
		memcg->hotness_hg[12], memcg->hotness_hg[13], memcg->hotness_hg[14], memcg->hotness_hg[15], nr_active);
	nr_active = 0;

	for(idx_hot = 15; idx_hot >= 0; idx_hot--) {
		unsigned long nr_pages = memcg->hotness_hg[idx_hot];
		if(nr_active + nr_pages > max_nr_pages)
			break;
		nr_active += nr_pages;
	}
	if(idx_hot != 15)
		idx_hot++;

	if(nr_active < (max_nr_pages * 75 / 100))
		need_warm = true;

	spin_unlock(&memcg->access_lock);

	if(idx_hot < MTTM_INIT_THRESHOLD)
		idx_hot = MTTM_INIT_THRESHOLD;

	// some pages may not be reflected in the histogram when cooling happens
	if(memcg->cooled) {
		//when cooling happens, thres will be current - 1
		if(idx_hot < memcg->active_threshold)
			if(memcg->active_threshold > 1)
				memcg->active_threshold--;

		memcg->cooled = false;
		set_lru_adjusting(memcg, true);
	}
	else {
		memcg->active_threshold = idx_hot;
		set_lru_adjusting(memcg, true);
	}

	if(memcg->use_warm) {
		if(need_warm)
			memcg->warm_threshold = memcg->active_threshold - 1;
		else
			memcg->warm_threshold = memcg->active_threshold;
		}
	else
		memcg->warm_threshold = memcg->active_threshold;

	pr_info("[%s] active_threshold: %u, warm_threshold: %u, max_nr_pages: %lu, active_sum: %lu, warm: %s\n",
		__func__, memcg->active_threshold, memcg->warm_threshold, max_nr_pages, nr_active, need_warm ? "true" : "false");

}

void move_page_to_active_lru(struct page *page)
{
	struct lruvec *lruvec;
	LIST_HEAD(l_active);

	lruvec = mem_cgroup_page_lruvec(page);

	spin_lock_irq(&lruvec->lru_lock);
	if(PageActive(page))
		goto lru_unlock;

	if(!PageLRU(page))
		goto lru_unlock;	

	if(unlikely(!get_page_unless_zero(page)))
		goto lru_unlock;

	if(!TestClearPageLRU(page)) {
		put_page(page);
		goto lru_unlock;
	}

	list_move(&page->lru, &l_active);
	update_lru_size(lruvec, page_lru(page), page_zonenum(page),
			-thp_nr_pages(page));
	SetPageActive(page);

	if(!list_empty(&l_active))
		move_pages_to_lru(lruvec, &l_active);
lru_unlock:
	spin_unlock_irq(&lruvec->lru_lock);
	BUG_ON(!list_empty(&l_active));
}


void move_page_to_inactive_lru(struct page *page)
{
	struct lruvec *lruvec;
	LIST_HEAD(l_inactive);

	lruvec = mem_cgroup_page_lruvec(page);

	spin_lock_irq(&lruvec->lru_lock);
	if(!PageActive(page))
		goto lru_unlock;

	if(!PageLRU(page))
		goto lru_unlock;	

	if(unlikely(!get_page_unless_zero(page)))
		goto lru_unlock;

	if(!TestClearPageLRU(page)) {
		put_page(page);
		goto lru_unlock;
	}

	list_move(&page->lru, &l_inactive);
	update_lru_size(lruvec, page_lru(page), page_zonenum(page),
			-thp_nr_pages(page));
	ClearPageActive(page);

	if(!list_empty(&l_inactive))
		move_pages_to_lru(lruvec, &l_inactive);
lru_unlock:
	spin_unlock_irq(&lruvec->lru_lock);
	BUG_ON(!list_empty(&l_inactive));
}

void check_transhuge_cooling(void *arg, struct page *page)
{
	struct mem_cgroup *memcg = arg ? (struct mem_cgroup *)arg : page_memcg(page);
	struct page *meta_page;
	unsigned int memcg_cclock;
	unsigned long prev_idx, cur_idx;

	if(!memcg)
		return;

	spin_lock(&memcg->access_lock);	
	meta_page = get_meta_page(page);
	prev_idx = get_idx(meta_page->nr_accesses);

	memcg_cclock = READ_ONCE(memcg->cooling_clock);
	if(memcg_cclock > meta_page->cooling_clock) {
		unsigned int diff = memcg_cclock - meta_page->cooling_clock;		
		meta_page->nr_accesses >>= diff;
	}
	meta_page->cooling_clock = memcg_cclock;
	cur_idx = get_idx(meta_page->nr_accesses);

	if(prev_idx != cur_idx) {
		if(memcg->hotness_hg[prev_idx] >= HPAGE_PMD_NR)
			memcg->hotness_hg[prev_idx] -= HPAGE_PMD_NR;
		else
			memcg->hotness_hg[prev_idx] = 0;
		memcg->hotness_hg[cur_idx] += HPAGE_PMD_NR;
	}

	spin_unlock(&memcg->access_lock);
}


// It should modify hotness_hg since it is invoked at cooling_node
void check_base_cooling(pginfo_t *pginfo, struct page *page)
{
	struct mem_cgroup *memcg = page_memcg(page);
	unsigned int memcg_cclock;
	unsigned long prev_idx, cur_idx;

	if(!memcg)
		return;

	spin_lock(&memcg->access_lock);
	prev_idx = get_idx(pginfo->nr_accesses);

	memcg_cclock = READ_ONCE(memcg->cooling_clock);
	if(memcg_cclock > pginfo->cooling_clock) {
		unsigned int diff = memcg_cclock - pginfo->cooling_clock;
		pginfo->nr_accesses >>= diff;
	}
	pginfo->cooling_clock = memcg_cclock;
	cur_idx = get_idx(pginfo->nr_accesses);
	
	if(prev_idx != cur_idx) {
		if(memcg->hotness_hg[prev_idx] > 0)
			memcg->hotness_hg[prev_idx]--;
		memcg->hotness_hg[cur_idx]++;
	}

	spin_unlock(&memcg->access_lock);
}

static void update_base_page(struct vm_area_struct *vma, struct page *page,
							pginfo_t *pginfo)
{
	struct mem_cgroup *memcg = get_mem_cgroup_from_mm(vma->vm_mm);
	bool hot;
	unsigned long prev_idx, cur_idx;

	check_base_cooling(pginfo, page);
	prev_idx = get_idx(pginfo->nr_accesses);	
	pginfo->nr_accesses += HPAGE_PMD_NR;
	cur_idx = get_idx(pginfo->nr_accesses);

	spin_lock(&memcg->access_lock);
	if(prev_idx != cur_idx) {
		if(memcg->hotness_hg[prev_idx] > 0)
			memcg->hotness_hg[prev_idx]--;
		memcg->hotness_hg[cur_idx]++;
	}
	spin_unlock(&memcg->access_lock);

	hot = cur_idx >= memcg->active_threshold;
	if(hot)
		move_page_to_active_lru(page);
	else if(PageActive(page))
		move_page_to_inactive_lru(page);

}

static void update_huge_page(struct vm_area_struct *vma, pmd_t *pmd,
			struct page *page, unsigned long address)
{
	struct mem_cgroup *memcg = get_mem_cgroup_from_mm(vma->vm_mm);
	struct page *meta_page;
	bool hot;
	unsigned long prev_idx, cur_idx;
	
	meta_page = get_meta_page(page);
	check_transhuge_cooling((void *)memcg, page);

	prev_idx = get_idx(meta_page->nr_accesses);
	meta_page->nr_accesses++;
	cur_idx = get_idx(meta_page->nr_accesses);
	spin_lock(&memcg->access_lock);
	if(prev_idx != cur_idx) {
		if(memcg->hotness_hg[prev_idx] >= HPAGE_PMD_NR)
			memcg->hotness_hg[prev_idx] -= HPAGE_PMD_NR;
		else
			memcg->hotness_hg[prev_idx] = 0;
		memcg->hotness_hg[cur_idx] += HPAGE_PMD_NR;
	}
	spin_unlock(&memcg->access_lock);

	hot = cur_idx >= memcg->active_threshold;
	if(hot)
		move_page_to_active_lru(page);
	else if(PageActive(page))
		move_page_to_inactive_lru(page);

}

static int __update_pte_pginfo(struct vm_area_struct *vma, pmd_t *pmd,
				unsigned long address)
{
	pte_t *pte, pte_struct;
	spinlock_t *ptl;
	pginfo_t *pginfo;
	struct page *page, *pte_page;
	int ret = 0;

	pte = pte_offset_map_lock(vma->vm_mm, pmd, address, &ptl);
	pte_struct = *pte;
	if(!pte_present(pte_struct))
		goto pte_unlock;

	page = vm_normal_page(vma, address, pte_struct);
	if(!page || PageKsm(page))
		goto pte_unlock;

	if(page != compound_head(page))
		goto pte_unlock;

	pte_page = virt_to_page((unsigned long)pte);
	if(!PageMttm(pte_page)) {
		goto pte_unlock;
	}

	pginfo = get_pginfo_from_pte(pte);
	if(!pginfo) {
		goto pte_unlock;
	}

	update_base_page(vma, page, pginfo);

	pte_unmap_unlock(pte, ptl);

	if(page_to_nid(page) == 0)
		return 1;
	else 
		return 2;

pte_unlock:
	pte_unmap_unlock(pte, ptl);
	return ret;
}

static int __update_pmd_pginfo(struct vm_area_struct *vma, pud_t *pud,
				unsigned long address)
{
	pmd_t *pmd, pmdval;
	bool ret = 0;

	pmd = pmd_offset(pud, address);
	if(!pmd || pmd_none(*pmd))
		return ret;

	if(is_swap_pmd(*pmd))
		return ret;

	if(!pmd_trans_huge(*pmd) && !pmd_devmap(*pmd) && unlikely(pmd_bad(*pmd))) {
		pmd_clear_bad(pmd);
		return ret;
	}

	pmdval = *pmd;
	if(pmd_trans_huge(pmdval) || pmd_devmap(pmdval)) {
		struct page *page;

		if(is_huge_zero_pmd(pmdval))
			return ret;

		page = pmd_page(pmdval);
		if(!page)
			goto pmd_unlock;

		if(!PageCompound(page))
			goto pmd_unlock;

		update_huge_page(vma, pmd, page, address);

		if(page_to_nid(page) == 0)
			return 1;
		else
			return 2;

pmd_unlock:
		return 0;
	}

	return __update_pte_pginfo(vma, pmd, address);
}


static int __update_pginfo(struct vm_area_struct *vma, unsigned long address)
{
	pgd_t *pgd;
	p4d_t *p4d;
	pud_t *pud;

	pgd = pgd_offset(vma->vm_mm, address);
	if(pgd_none_or_clear_bad(pgd))
		return 0;

	p4d = p4d_offset(pgd, address);
	if(p4d_none_or_clear_bad(p4d))
		return 0;

	pud = pud_offset(p4d, address);
	if(pud_none_or_clear_bad(pud))
		return 0;

	return __update_pmd_pginfo(vma, pud, address);
}

static void update_pginfo(pid_t pid, unsigned long address, enum eventtype e)
{
	struct pid *pid_struct = find_get_pid(pid);
	struct task_struct *p = pid_struct ? pid_task(pid_struct, PIDTYPE_PID) : NULL;
	struct mm_struct *mm = p ? p->mm : NULL;
	struct vm_area_struct *vma;
	struct mem_cgroup *memcg;
	int ret;
	
	if(!mm)
		goto put_task;
	if(!mm->mttm_enabled) {
		goto put_task;
	}
	if(!mmap_read_trylock(mm))
		goto put_task;

	vma = find_vma(mm, address);
	if(unlikely(!vma))
		goto mmap_unlock;
	if(!vma->vm_mm || !vma_migratable(vma) ||
		(vma->vm_file && (vma->vm_flags & (VM_READ | VM_WRITE)) == (VM_READ)))
		goto mmap_unlock;

	memcg = get_mem_cgroup_from_mm(mm);
	if(!memcg)
		goto mmap_unlock;
	
	ret = __update_pginfo(vma, address);

	if(ret == 0) // invalid record
		goto mmap_unlock;
	else {
		memcg->nr_sampled++;
	}

	// cooling
	if(memcg->nr_sampled % memcg->cooling_period == 0) /* ||
		need_memcg_cooling(memcg)) */{
		if(set_cooling(memcg)) {
			//nothing
		}
	}

	// adjust threshold
	else if(memcg->nr_sampled % memcg->adjust_period == 0)
		adjust_active_threshold(memcg);

mmap_unlock:
	mmap_read_unlock(mm);
put_task:
	put_pid(pid_struct);
}

static void ksampled_do_work(void)
{
	int cpu, event, cond = true;
	int nr_sampled = 0, nr_skip = 0;
	//unsigned long prev_active_lru_size = 0, cur_active_lru_size = 0;

	//prev_active_lru_size = get_active_lru_size();
	for(cpu = 0; cpu < CORES_PER_SOCKET; cpu++) {
		nr_sampled = 0;
		for(event = 0; event < NR_EVENTTYPE; event++) {
			do {
				struct perf_buffer *rb;
				struct perf_event_mmap_page *up;
				struct perf_event_header *ph;
				struct mttm_event *me;
				unsigned long pg_index, offset;
				int page_shift;
				__u64 head;

				if(!pfe[cpu][event]) {
					break;
				}	

				__sync_synchronize();	

				rb = pfe[cpu][event]->rb;
				if(!rb) {
					pr_info("[%s] event->rb is NULL\n",__func__);
					break;
				}

				up = READ_ONCE(rb->user_page);
				head = READ_ONCE(up->data_head);
				if(head == up->data_tail) {
					nr_skip++;
					break;
				}	

				// It does not modify the up->data_head.
				
				head -= up->data_tail;
				if(head > (up->data_size * MAX_SAMPLE_RATIO / 100)) {	
					cond = true;
				}
				else if (head < (up->data_size * MIN_SAMPLE_RATIO / 100)) {
					cond = false;
				}

				smp_rmb();

				page_shift = PAGE_SHIFT + page_order(rb);
				offset = READ_ONCE(up->data_tail);
				pg_index = (offset >> page_shift) & (rb->nr_pages - 1);
				offset &= (1 << page_shift) - 1;
				ph = (void *)(rb->data_pages[pg_index] + offset);
				switch(ph->type) {
					case PERF_RECORD_SAMPLE:
						me = (struct mttm_event *)ph;
						if(!valid_va(me->addr)) {
							break;
						}
						update_pginfo(me->pid, me->addr, event);	
						break;
					case PERF_RECORD_THROTTLE:
					case PERF_RECORD_UNTHROTTLE:
						break;
					case PERF_RECORD_LOST_SAMPLES:
						pr_info("[%s] lost sample\n",__func__);
						break;
					default:
						pr_info("[%s] unknown sample\n",__func__);
						break;
				}	

				smp_mb();
				WRITE_ONCE(up->data_tail, up->data_tail + ph->size);
			} while(cond);
		}
	}

	//cur_active_lru_size = get_active_lru_size();
	//trace_lru_size(true, prev_active_lru_size, cur_active_lru_size);
}

static int ksampled(void *dummy)
{
	unsigned long sleep_timeout = usecs_to_jiffies(20000);

	while(!kthread_should_stop()) {
		ksampled_do_work();
		schedule_timeout_interruptible(sleep_timeout);
	}

	return 0;
}

static int ksampled_run(void)
{
	int ret = 0, cpu, event;
	if (!ksampled_thread) {
		pfe = kzalloc(sizeof(struct perf_event **) * CORES_PER_SOCKET, GFP_KERNEL);
		for(cpu = 0; cpu < CORES_PER_SOCKET; cpu++) {
			pfe[cpu] = kzalloc(sizeof(struct perf_event *) * NR_EVENTTYPE, GFP_KERNEL);
		}

		for(cpu = 0; cpu < CORES_PER_SOCKET; cpu++) {
			for(event = 0; event < NR_EVENTTYPE; event++) {
				if(get_pebs_event(event) == NR_EVENTTYPE) {
					pfe[cpu][event] = NULL;
					continue;
				}
				
				if(__perf_event_open(get_pebs_event(event), 0, cpu, event))
					return -1;
				if(mttm_perf_event_init(pfe[cpu][event], MTTM_PEBS_BUFFER_SIZE))
					return -1;
			}
		}

		ksampled_thread = kthread_run_on_cpu(ksampled, NULL, KSAMPLED_CPU, "ksampled");
		if(IS_ERR(ksampled_thread)) {
			pr_err("Failed to start ksampled\n");
			ret = PTR_ERR(ksampled_thread);
			ksampled_thread = NULL;
		}
	}
	return ret;
}

static void ksampled_stop(void)
{
	int cpu, event;
	if(ksampled_thread) {
		kthread_stop(ksampled_thread);	
		ksampled_thread = NULL;
	}
	
	for (cpu = 0; cpu < CORES_PER_SOCKET; cpu++) {
		for (event = 0; event < NR_EVENTTYPE; event++) {
			if (pfe[cpu][event])
				perf_event_disable(pfe[cpu][event]);
		}
	}

	for(cpu = 0; cpu < CORES_PER_SOCKET; cpu++)
		kfree(pfe[cpu]);
	kfree(pfe);

	pr_info("[%s] ksampled stop\n", __func__);

}


#ifdef CONFIG_PROC_SYSCTL
int sysctl_enable_ksampled(struct ctl_table *table, int write,
			void *buffer, size_t *lenp, loff_t *ppos)
{
	int err = 0;
	
	if (write && !capable(CAP_SYS_ADMIN))
		return -EPERM;

	err = proc_dointvec_minmax(table, write, buffer, lenp, ppos);

	if (err < 0)
		return err;
	if (write) {
		if(!ksampled_thread &&
			enable_ksampled == 1 &&
			current_tenants == 0) {
			ksampled_run();
		}
		else if(ksampled_thread &&
			enable_ksampled == 0) {
			ksampled_stop();
		}
	}
	return err;
}
#endif





