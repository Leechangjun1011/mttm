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
#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>

#include <trace/events/mttm.h>
#include "../kernel/events/internal.h"
#include "internal.h"
#include <linux/mttm.h>
#include <linux/xarray.h>

int enable_ksampled = 0;
unsigned long pebs_stable_period = 10007;
unsigned long pebs_sample_period = 10007;
unsigned long store_sample_period = 100003;
unsigned long hotness_intensity_threshold = 200;
unsigned int hotset_size_threshold = 2;
unsigned int check_stable_sample_rate = 1;
unsigned int use_dram_determination = 1;
unsigned int use_region_separation = 1;
unsigned int use_hotness_intensity = 0;
unsigned int use_pingpong_reduce = 1;
unsigned int remote_latency = 130;
unsigned int print_more_info = 0;
unsigned long pingpong_reduce_threshold = 200;
unsigned long manage_cputime_threshold = 50;
unsigned long mig_cputime_threshold = 200;
char mttm_local_dram_string[16];
unsigned long mttm_local_dram = ((80UL << 30) >> 12);//80GB on # of pages
unsigned int ksampled_trace_period_in_ms = 5000;
unsigned int use_lru_manage_reduce = 1;
unsigned int dram_deter_end = 0;
#define NUM_AVAIL_DMA_CHAN	16
#define DMA_CHAN_PER_PAGE	1
unsigned int use_dma_migration = 0;
struct dma_chan *copy_chan[NUM_AVAIL_DMA_CHAN];
struct dma_device *copy_dev[NUM_AVAIL_DMA_CHAN];
unsigned int use_all_stores = 0;
unsigned int use_xa_basepage = 1;
int current_tenants = 0;
struct mem_cgroup **memcg_list = NULL;
struct task_struct *ksampled_thread = NULL;
struct perf_event ***pfe;
DEFINE_SPINLOCK(register_lock);

extern int enabled_kptscand;
extern struct task_struct *kptscand_thread;


bool node_is_toptier(int nid)
{
	return (nid == 0) ? true : false;
}

struct page *get_meta_page(struct page *page)
{
	page = compound_head(page);
	return &page[3];
}

pginfo_t *get_pginfo_from_xa(struct xarray *xa, struct page *page)
{
	return (pginfo_t *)xa_load(xa, page_to_pfn(page));
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

	if(use_xa_basepage && memcg->basepage_xa)
		pginfo = get_pginfo_from_xa(memcg->basepage_xa, page);
	else
		pginfo = get_pginfo_from_pte(pte);
	if(!pginfo)
		return 0;

	initial_hotness = 0; //get_accesses_from_idx(memcg->active_threshold + 1);

	pginfo->nr_accesses = initial_hotness;
	pginfo->prev_accesses = 0;
	pginfo->cooling_clock = READ_ONCE(memcg->cooling_clock);//do not skip cooling
	pginfo->promoted = 0;
	pginfo->demoted = 0;

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
	page[3].prev_accesses = 0;
	SetPageMttm(&page[3]);
	
	if(!memcg)
		return;

	page[3].cooling_clock = memcg->cooling_clock;//do not skip cooling
	page[3].promoted = 0;
	page[3].demoted = 0;
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
	newpage[3].prev_accesses = page[3].prev_accesses;
	newpage[3].cooling_clock = page[3].cooling_clock;
	newpage[3].promoted = page[3].promoted;
	newpage[3].demoted = page[3].demoted;
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

void uncharge_mttm_pte(pte_t *pte, struct mem_cgroup *memcg, struct page *page)
{
	struct page *pte_page;
	unsigned int idx;
	pginfo_t *pginfo;

	if(!memcg)
		return;
	if(!memcg->mttm_enabled)
		return;

	pte_page = virt_to_page((unsigned long)pte);
	if(!PageMttm(pte_page))
		return;

	if(use_xa_basepage && memcg->basepage_xa)
		pginfo = get_pginfo_from_xa(memcg->basepage_xa, page);
	else
		pginfo = get_pginfo_from_pte(pte);
	if(!pginfo)
		return;

	idx = get_idx(pginfo->nr_accesses);

	spin_lock(&memcg->access_lock);
	if(memcg->hotness_hg[idx] > 0)
		memcg->hotness_hg[idx]--;
	spin_unlock(&memcg->access_lock);

	if(use_xa_basepage && memcg->basepage_xa) {
		pginfo_t *entry = xa_erase(memcg->basepage_xa, page_to_pfn(page));
		if(entry)
			kmem_cache_free(pginfo_cache_xa, entry);
	}

}

void uncharge_mttm_page(struct page *page, struct mem_cgroup *memcg)
{
	unsigned int nr_pages = thp_nr_pages(page);
	unsigned int idx;

	if(!memcg)
		return;
	if(!memcg->mttm_enabled)
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
		case MEMWRITE:
			return ALL_STORES;
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

	if(config == ALL_STORES)
		attr.sample_period = store_sample_period;
	else
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

static void pebs_update_period(uint64_t value)
{
	int cpu, event;

	for(cpu = 0; cpu < CORES_PER_SOCKET; cpu++) {
		for(event = 0; event < NR_EVENTTYPE; event++) {
			int ret;
			if(!pfe[cpu][event])
				continue;

			switch(event) {
				case DRAMREAD:
				case CXLREAD:
					ret = perf_event_period(pfe[cpu][event], value);
					break;
				case MEMWRITE:
				default:
					ret = 0;
					break;
			}

			if(ret == -EINVAL)
				pr_err("[%s] failed to update sample period",__func__);
		}
	}
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
SYSCALL_DEFINE2(mttm_register_pid,
		pid_t, pid, const char __user *, u_name)
{
	int i;
	struct mem_cgroup *memcg = mem_cgroup_from_task(current);
	char name[PATH_MAX];
	spin_lock(&register_lock);

	if(current_tenants == LIMIT_TENANTS) {
		pr_info("[%s] Can't register tenant due to limit\n",__func__);
		spin_unlock(&register_lock);
		return 0;
	}	

	if(use_xa_basepage && !memcg->basepage_xa) {
		memcg->basepage_xa = kmalloc(sizeof(struct xarray), GFP_KERNEL);
		xa_init(memcg->basepage_xa);
	}

	current->mm->mttm_enabled = true;
	memcg->mttm_enabled = true;
	if(kmigrated_init(memcg))
		pr_info("[%s] failed to start kmigrated\n",__func__);
	
	if(use_dma_migration) {
		memcg->dma_chan_start = current_tenants * DMA_CHAN_PER_PAGE;
		if(memcg->dma_chan_start + DMA_CHAN_PER_PAGE > NUM_AVAIL_DMA_CHAN)
			memcg->dma_chan_start = 0;
	}

	memcg_list[current_tenants] = memcg;
	current_tenants++;

	copy_from_user(name, u_name, strnlen_user(u_name, PATH_MAX));
	strlcpy(memcg->tenant_name, name, PATH_MAX);
	
	for(i = 0; i < LIMIT_TENANTS; i++) {
		if(memcg_list[i]) {
			if(memcg_list[i]->mttm_enabled && use_dram_determination) {
				WRITE_ONCE(memcg_list[i]->nodeinfo[0]->max_nr_base_pages, mttm_local_dram / current_tenants);
				WRITE_ONCE(memcg_list[i]->max_nr_dram_pages, mttm_local_dram / current_tenants);
				pr_info("[%s] [ %s ] dram size set to %lu MB\n",
					__func__, memcg_list[i]->tenant_name, memcg_list[i]->max_nr_dram_pages >> 8);
			}
		}
	}

	pr_info("[%s] registered pid : %d. name : [ %s ], current_tenants : %d, dma_chan_start : %u, local_dram : %lu MB\n",
		__func__, pid, memcg->tenant_name, current_tenants, memcg->dma_chan_start, (mttm_local_dram / current_tenants) >> 8);

	spin_unlock(&register_lock);
	
	return 0;
}

SYSCALL_DEFINE1(mttm_unregister_pid,
		pid_t, pid)
{
	int i;
	struct mem_cgroup *memcg = mem_cgroup_from_task(current);
	spin_lock(&register_lock);

	current_tenants--;
	kmigrated_stop(memcg);
	if(use_xa_basepage && memcg->basepage_xa) {
		xa_destroy(memcg->basepage_xa);
		kfree(memcg->basepage_xa);
	}

	for(i = 0; i < LIMIT_TENANTS; i++) {
		if(READ_ONCE(memcg_list[i]) == memcg) {
			WRITE_ONCE(memcg_list[i], NULL);
			break;
		}	
	}

	// Re-distribute local DRAM
	for(i = 0; i < LIMIT_TENANTS; i++) {
		if(READ_ONCE(memcg_list[i])) {
			WRITE_ONCE(memcg_list[i]->dram_fixed, false);
		}
	}

	spin_unlock(&register_lock);

	pr_info("[%s] unregistered pid : %d, name : [ %s ], current_tenants : %d, total sample : %lu\n",
		__func__, pid, memcg->tenant_name, current_tenants, memcg->nr_sampled);
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
	WRITE_ONCE(memcg->hg_mismatch, false);
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
	//bool need_warm = false;
	int idx_hot;
	unsigned int prev_threshold = READ_ONCE(memcg->active_threshold);

	if(READ_ONCE(memcg->hg_mismatch)) {
		// Not need to adjust since threshold not changed.
		//set_lru_adjusting(memcg, true);
		return;
	}

	spin_lock(&memcg->access_lock);

	for(idx_hot = 15; idx_hot >= 0; idx_hot--) {
		unsigned long nr_pages = memcg->hotness_hg[idx_hot];
		if(nr_active + nr_pages > max_nr_pages)
			break;
		nr_active += nr_pages;
	}
	if(idx_hot != 15)
		idx_hot++;

	/*if(nr_active < (max_nr_pages * 75 / 100))
		need_warm = true;*/

	spin_unlock(&memcg->access_lock);

	if(idx_hot < MTTM_INIT_THRESHOLD) {
		idx_hot = MTTM_INIT_THRESHOLD;
	}

	// histogram is reset before cooling
	// some pages may not be reflected in the histogram when cooling happens
	if(memcg->cooled) {
		WRITE_ONCE(memcg->active_threshold, MTTM_INIT_THRESHOLD + memcg->threshold_offset);
		/*if(memcg->active_threshold > MTTM_INIT_THRESHOLD)
			WRITE_ONCE(memcg->active_threshold, memcg->active_threshold - 1);*/
		memcg->cooled = false;
	}
	else {
		WRITE_ONCE(memcg->active_threshold, idx_hot + memcg->threshold_offset);
	}

	if(memcg->active_threshold != prev_threshold)
		set_lru_adjusting(memcg, true);

	if(memcg->use_warm && (memcg->active_threshold > MTTM_INIT_THRESHOLD))
		WRITE_ONCE(memcg->warm_threshold, memcg->active_threshold - 1);
	else
		WRITE_ONCE(memcg->warm_threshold, memcg->active_threshold);

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

// Invoked at kmigrated
void check_transhuge_cooling_reset(void *arg, struct page *page)
{
	struct mem_cgroup *memcg = arg ? (struct mem_cgroup *)arg : page_memcg(page);
	struct page *meta_page;
	unsigned int memcg_cclock;
	unsigned long cur_idx;

	if(!memcg)
		return;

	spin_lock(&memcg->access_lock);
	meta_page = get_meta_page(page);
	memcg_cclock = READ_ONCE(memcg->cooling_clock);
	
	/*if(memcg_cclock > meta_page->cooling_clock) {
		unsigned int diff = memcg_cclock - meta_page->cooling_clock;
		unsigned int cooled_accesses = meta_page->nr_accesses >> diff;
		meta_page->nr_accesses = cooled_accesses;
		meta_page->prev_accesses = cooled_accesses;
	}*/
	meta_page->nr_accesses = 0;

	meta_page->cooling_clock = memcg_cclock;
	cur_idx = get_idx(meta_page->nr_accesses);
	memcg->hotness_hg[cur_idx] += HPAGE_PMD_NR;

	spin_unlock(&memcg->access_lock);
}

// Invoked at kmigrated
void check_base_cooling_reset(pginfo_t *pginfo, struct page *page)
{
	struct mem_cgroup *memcg = page_memcg(page);
	unsigned int memcg_cclock;
	unsigned long cur_idx;

	if(!memcg)
		return;

	spin_lock(&memcg->access_lock);
	memcg_cclock = READ_ONCE(memcg->cooling_clock);
	/*if(memcg_cclock > pginfo->cooling_clock) {
		unsigned int diff = memcg_cclock - pginfo->cooling_clock;
		unsigned int cooled_accesses = pginfo->nr_accesses >> diff;	
		pginfo->nr_accesses = cooled_accesses;
		pginfo->prev_accesses = cooled_accesses;
	}*/
	pginfo->nr_accesses = 0;

	pginfo->cooling_clock = memcg_cclock;
	cur_idx = get_idx(pginfo->nr_accesses);
	memcg->hotness_hg[cur_idx]++;

	spin_unlock(&memcg->access_lock);
}


void check_transhuge_cooling(void *arg, struct page *page)
{
	struct mem_cgroup *memcg = arg ? (struct mem_cgroup *)arg : page_memcg(page);
	struct page *meta_page;
	unsigned int memcg_cclock;
	unsigned long prev_idx, cur_idx;

	if(!memcg)
		return;
	if(!memcg->mttm_enabled)
		return;

	spin_lock(&memcg->access_lock);
	meta_page = get_meta_page(page);
	prev_idx = get_idx(meta_page->nr_accesses);

	memcg_cclock = READ_ONCE(memcg->cooling_clock);
	if(memcg_cclock > meta_page->cooling_clock) {
		//unsigned int diff = memcg_cclock - meta_page->cooling_clock;		
		//meta_page->nr_accesses >>= diff;
		meta_page->nr_accesses = 0;
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


void check_base_cooling(pginfo_t *pginfo, struct page *page)
{
	struct mem_cgroup *memcg = page_memcg(page);
	unsigned int memcg_cclock;
	unsigned long prev_idx, cur_idx;

	if(!memcg)
		return;
	if(!memcg->mttm_enabled)
		return;

	spin_lock(&memcg->access_lock);
	prev_idx = get_idx(pginfo->nr_accesses);

	memcg_cclock = READ_ONCE(memcg->cooling_clock);
	if(memcg_cclock > pginfo->cooling_clock) {
		//unsigned int diff = memcg_cclock - pginfo->cooling_clock;
		//pginfo->nr_accesses >>= diff;
		pginfo->nr_accesses = 0;
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
				pginfo_t *pginfo, unsigned long address)
{
	struct mem_cgroup *memcg = get_mem_cgroup_from_mm(vma->vm_mm);
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

	if(cur_idx >= memcg->active_threshold)
		move_page_to_active_lru(page);
	else if(PageActive(page))
		move_page_to_inactive_lru(page);

}

static void update_huge_page(struct vm_area_struct *vma, pmd_t *pmd,
			struct page *page, unsigned long address)
{
	struct mem_cgroup *memcg = get_mem_cgroup_from_mm(vma->vm_mm);
	struct page *meta_page;
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

	if(cur_idx >= memcg->active_threshold)
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

	if(use_xa_basepage)
		pginfo = get_pginfo_from_xa(page_memcg(page)->basepage_xa, page);
	else
		pginfo = get_pginfo_from_pte(pte);
	if(!pginfo) {
		goto pte_unlock;
	}

	update_base_page(vma, page, pginfo, address);

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

	if(use_dram_determination &&
		!READ_ONCE(memcg->region_determined) &&
		(get_pebs_event(e) == ALL_STORES))
		goto mmap_unlock;
		
	
	ret = __update_pginfo(vma, address);

	if(ret == 0) { // invalid record
		goto mmap_unlock;
	}
	else {
		memcg->nr_sampled++;
		memcg->interval_nr_sampled++;
		if(get_pebs_event(e) == DRAM_LLC_LOAD_MISS) {
			memcg->nr_local++;
			memcg->nr_load++;
		}
		if(get_pebs_event(e) == REMOTE_DRAM_LLC_LOAD_MISS) {
			memcg->nr_remote++;
			memcg->nr_load++;
		}
		else if(get_pebs_event(e) == ALL_STORES)
			memcg->nr_store++;
	}

	// cooling
	if((memcg->nr_sampled % READ_ONCE(memcg->cooling_period)) == 0) /* ||
		need_memcg_cooling(memcg)) */{
		if(set_cooling(memcg)) {
			//nothing
		}
		if(use_xa_basepage && memcg->basepage_xa) {
			unsigned long xa_cnt = 0;
			unsigned long index;
			pginfo_t *entry;
			xa_for_each(memcg->basepage_xa, index, entry) {
				xa_cnt++;
			}
			pr_info("[%s] xarray size : %lu\n",__func__, xa_cnt);
		}
	}

	// adjust threshold
	else if((memcg->nr_sampled % READ_ONCE(memcg->adjust_period)) == 0)
		adjust_active_threshold(memcg);

mmap_unlock:
	mmap_read_unlock(mm);
put_task:
	put_pid(pid_struct);
}

static void ksampled_do_work(void)
{
	int cpu, event, i, cond = true;
	int nr_skip = 0;
	//unsigned long prev_active_lru_size = 0, cur_active_lru_size = 0;

	//prev_active_lru_size = get_active_lru_size();
	for(cpu = 0; cpu < CORES_PER_SOCKET; cpu++) {
		for(event = 0; event < NR_EVENTTYPE; event++) {
			if(!use_all_stores && (get_pebs_event(event) == ALL_STORES))
				continue;
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

}

static void set_dram_size(struct mem_cgroup *memcg, unsigned long required_dram, bool fixed)
{
	WRITE_ONCE(memcg->nodeinfo[0]->max_nr_base_pages, required_dram);
	WRITE_ONCE(memcg->max_nr_dram_pages, required_dram);
	WRITE_ONCE(memcg->dram_fixed, fixed);


	if(get_nr_lru_pages_node(memcg, NODE_DATA(0)) +
		get_memcg_demotion_wmark(required_dram) > required_dram)
		WRITE_ONCE(memcg->nodeinfo[0]->need_demotion, true);
	else if(required_dram - get_memcg_promotion_expanded_wmark(required_dram) >
		get_nr_lru_pages_node(memcg, NODE_DATA(0)))
		WRITE_ONCE(memcg->dram_expanded, true);

}


static void check_sample_rate_is_stable(struct mem_cgroup *memcg,
				unsigned long stdev, unsigned long mean,
				unsigned long mean_rate)
{
	if(stdev >= mean) {
		if(memcg->stable_cnt > 0) {
			memcg->stable_cnt = 0;
			WRITE_ONCE(memcg->stable_status, false);
		}
	}
	else if(mean_rate > 50) {
		memcg->stable_cnt++;
		if(memcg->stable_cnt >= SAMPLE_RATE_STABLE_CNT) {
			WRITE_ONCE(memcg->stable_status, true);
			pr_info("[%s] [ %s ] stable. mean_rate : %lu\n",
				__func__, memcg->tenant_name, mean_rate);
		}
	}
}

static void check_rate_change(struct mem_cgroup *memcg)
{
	int j;
	if((memcg->highest_rate * 6 / 5) < memcg->mean_rate) {
		// access rate increased more than 50% of highest one
		memcg->lowered_cnt = 0;

		WRITE_ONCE(memcg->highest_rate, memcg->mean_rate);
		if(remote_latency < 200)
			WRITE_ONCE(memcg->hotness_scan_cnt,
				max_t(unsigned long, memcg->hotness_scan_cnt, memcg->highest_rate / 2000));
		else
			WRITE_ONCE(memcg->hotness_scan_cnt,
				max_t(unsigned long, memcg->hotness_scan_cnt, memcg->highest_rate / 1000));

		pr_info("[%s] [ %s ] highest rate updated to %lu. scan_cnt updated to %lu\n",
			__func__, memcg->tenant_name, memcg->highest_rate, memcg->hotness_scan_cnt);
		/*if(use_dram_determination) {
			// Re-calculate dram size of each tenant
			for(j = 0; j < LIMIT_TENANTS; j++) {
				struct mem_cgroup *memcg_iter;
				memcg_iter = READ_ONCE(memcg_list[j]);
				if(memcg_iter) {
					WRITE_ONCE(memcg_iter->dram_fixed, false);
				}
			}
		}*/
	}	
	else if(memcg->highest_rate / 20 > memcg->mean_rate &&
		memcg->mean_rate > 50) {
		// access rate decreased to lower than 5% of highest one
		memcg->lowered_cnt++;
		if(memcg->lowered_cnt >= 10) {
			memcg->lowered_cnt = 0;
			memcg->stable_cnt = 0;
			WRITE_ONCE(memcg->stable_status, false);
			WRITE_ONCE(memcg->highest_rate, memcg->mean_rate);
			WRITE_ONCE(memcg->hotness_scan_cnt, 1);

			if(use_dram_determination) {
				WRITE_ONCE(memcg->dram_fixed, false);
				WRITE_ONCE(memcg->region_determined, false);
				WRITE_ONCE(memcg->hi_determined, false);
			}
			pr_info("[%s] [ %s ] highest rate lowered to %lu\n",
				__func__, memcg->tenant_name, memcg->mean_rate);
		}
	}
	else {
		memcg->lowered_cnt = 0;
	}

}


static void calculate_sample_rate_stat(int rate_cnt)
{
	unsigned long mean, std_deviation, sum_sample, variance, mean_rate;
	int i, j;
	struct mem_cgroup *memcg;

	for(i = 0; i < LIMIT_TENANTS; i++) {
		memcg = READ_ONCE(memcg_list[i]);
		if(memcg) {
			for(j = 4; j > 0; j--)
				memcg->interval_sample[j] = memcg->interval_sample[j-1];
			memcg->interval_sample[0] = memcg->interval_nr_sampled;

			// Calculate mean, std_devication
			sum_sample = 0;
			for(j = 0; j < 5; j++)
				sum_sample += memcg->interval_sample[j];
			mean = sum_sample / 5;
			variance = 0;
			for(j = 0; j < 5; j++) {
				if(memcg->interval_sample[j] > mean)
					variance += (memcg->interval_sample[j] - mean) * (memcg->interval_sample[j] - mean);
				else
					variance += (mean - memcg->interval_sample[j]) * (mean - memcg->interval_sample[j]);
			}
			variance = variance / 5;
			std_deviation = int_sqrt(variance);	

			mean_rate = div64_u64(sum_sample, (ksampled_trace_period_in_ms / 1000) * 5);
			memcg->mean_rate = mean_rate;
			if(print_more_info) {
				if(memcg->nr_remote)
					pr_info("[%s] [ %s ] mean_rate : %lu, local : %lu, remote : %lu, ratio : %lu\n",
						__func__, memcg->tenant_name, mean_rate,
						memcg->nr_local, memcg->nr_remote, memcg->nr_local * 100 / memcg->nr_remote);
				else
					pr_info("[%s] [ %s ] mean_rate : %lu, local : %lu, remote : %lu\n",
						__func__, memcg->tenant_name, mean_rate,
						memcg->nr_local, memcg->nr_remote);	
			}

			if(memcg->stable_cnt < SAMPLE_RATE_STABLE_CNT)
				check_sample_rate_is_stable(memcg, std_deviation, mean, mean_rate);
			check_rate_change(memcg);

			memcg->interval_nr_sampled = 0;
			memcg->nr_local = 0;
			memcg->nr_remote = 0;

			if(memcg->max_anon_rss < get_anon_rss(memcg))
				memcg->max_anon_rss = get_anon_rss(memcg);
		}
	}
}



unsigned long get_anon_rss(struct mem_cgroup *memcg)
{
	unsigned long hot0, hot1, cold0, cold1;

	cold0 = lruvec_lru_size(mem_cgroup_lruvec(memcg, NODE_DATA(0)),
			LRU_INACTIVE_ANON, MAX_NR_ZONES);
	cold1 = lruvec_lru_size(mem_cgroup_lruvec(memcg, NODE_DATA(1)),
			LRU_INACTIVE_ANON, MAX_NR_ZONES);
	hot0 = lruvec_lru_size(mem_cgroup_lruvec(memcg, NODE_DATA(0)),
			LRU_ACTIVE_ANON, MAX_NR_ZONES);
	hot1 = lruvec_lru_size(mem_cgroup_lruvec(memcg, NODE_DATA(1)),
			LRU_ACTIVE_ANON, MAX_NR_ZONES);

	return cold0 + cold1 + hot0 + hot1;
}

static void calculate_dram_sensitivity(void)
{
	int i;
	struct mem_cgroup *memcg;
	bool not_region_determined = false;

	// Check that not classified workload exist.
	for(i = 0; i < LIMIT_TENANTS; i++) {
		memcg = READ_ONCE(memcg_list[i]);
		if(memcg) {
			if(!READ_ONCE(memcg->region_determined)) {
				not_region_determined = true;
			}	
		}
	}

	for(i = 0; i < LIMIT_TENANTS; i++) {
		memcg = READ_ONCE(memcg_list[i]);
		if(memcg) {
			if(READ_ONCE(memcg->region_determined)) {
				if(!READ_ONCE(memcg->dram_fixed)) {
					unsigned long valid_rate = memcg->highest_rate;
					unsigned long extra_rate;
					if(valid_rate > 6000) {
						extra_rate = valid_rate - 6000;
						valid_rate = 6000 + (extra_rate / 10);
					}

					memcg->hot_region_access_rate = valid_rate * memcg->nr_hot_region_access /
									(memcg->nr_hot_region_access + memcg->nr_cold_region_access);
					memcg->cold_region_access_rate = valid_rate * memcg->nr_cold_region_access / 
									(memcg->nr_hot_region_access + memcg->nr_cold_region_access);
					memcg->hot_region_dram_sensitivity = memcg->hot_region_access_rate * 1000 / (memcg->hot_region >> 8);
					memcg->cold_region_dram_sensitivity = memcg->cold_region_access_rate * 1000 / (memcg->cold_region >> 8);

					if(!not_region_determined)
						pr_info("[%s] [ %s ] access rate : %lu, region access rate : [hot : %lu, cold : %lu], dram sensitivity : [hot : %lu, cold : %lu]\n",
							__func__, memcg->tenant_name, valid_rate,
							memcg->hot_region_access_rate, memcg->cold_region_access_rate,
							memcg->hot_region_dram_sensitivity, memcg->cold_region_dram_sensitivity);
				}
			}
		}
	}
}



static unsigned long get_tot_dram_sensitivity(void)
{
	int i;
	struct mem_cgroup *memcg;
	unsigned long tot_dram_sensitivity = 0;

	for(i = 0; i < LIMIT_TENANTS; i++) {
		memcg = READ_ONCE(memcg_list[i]);
		if(memcg) {
			if(READ_ONCE(memcg->region_determined) &&
				memcg->hot_region_dram_sensitivity > 0) {
				tot_dram_sensitivity += memcg->cold_region_dram_sensitivity;
				tot_dram_sensitivity += memcg->hot_region_dram_sensitivity;
			}
		}
	}

	return tot_dram_sensitivity;
}


static unsigned long calculate_hot_region_dram_size(struct mem_cgroup *memcg,
			unsigned long tot_free_dram, unsigned long tot_dram_sensitivity,
			int (*dram_determined)[2])
{
	unsigned long available_dram = tot_free_dram * memcg->hot_region_dram_sensitivity /
						tot_dram_sensitivity;
	unsigned long remained_required_dram = 0, expected_extra_dram = 0;
	int i;
	struct mem_cgroup *memcg_iter;

	// Calculate remained required dram
	for(i = 0; i < LIMIT_TENANTS; i++) {
		memcg_iter = READ_ONCE(memcg_list[i]);
		if(memcg_iter) {
			if(READ_ONCE(memcg_iter->region_determined) &&
				memcg_iter->hot_region_dram_sensitivity > 0) {
				if(dram_determined[i][0] == 0) {
					remained_required_dram += memcg_iter->hot_region;
				}
				if(dram_determined[i][1] == 0) {
					remained_required_dram += memcg_iter->cold_region;
				}
			}
		}
	}
	remained_required_dram -= memcg->hot_region;

	if(memcg->hot_region > available_dram && 
		remained_required_dram < tot_free_dram - available_dram) {
		// In this case, we can give more than available_dram
		expected_extra_dram = tot_free_dram - available_dram - remained_required_dram;
	}

	return min_t(unsigned long, available_dram + expected_extra_dram,
			memcg->hot_region/* * hot_region_tolerance(memcg->highest_rate) / 100*/);
}

static unsigned long calculate_cold_region_dram_size(struct mem_cgroup *memcg,
			unsigned long tot_free_dram, unsigned long tot_dram_sensitivity,
			int (*dram_determined)[2])
{
	unsigned long available_dram = tot_free_dram * memcg->cold_region_dram_sensitivity /
						tot_dram_sensitivity;
	unsigned long remained_required_dram = 0, expected_extra_dram = 0;
	int i;
	struct mem_cgroup *memcg_iter;

	// Calculate remained required dram
	for(i = 0; i < LIMIT_TENANTS; i++) {
		memcg_iter = READ_ONCE(memcg_list[i]);
		if(memcg_iter) {
			if(READ_ONCE(memcg_iter->region_determined) &&
				memcg_iter->hot_region_dram_sensitivity > 0) {
				if(dram_determined[i][0] == 0) {
					remained_required_dram += memcg_iter->hot_region;
				}
				if(dram_determined[i][1] == 0) {
					remained_required_dram += memcg_iter->cold_region;
				}
			}
		}
	}
	remained_required_dram -= memcg->cold_region;

	if(memcg->cold_region > available_dram && 
		remained_required_dram < tot_free_dram - available_dram) {
		// In this case, we can give more than available_dram
		expected_extra_dram = tot_free_dram - available_dram - remained_required_dram;
	}

	return min_t(unsigned long, available_dram + expected_extra_dram, memcg->cold_region);
}

static unsigned long find_highest_dram_sensitivity(int *hs_idx, bool *hot,
					int (*dram_determined)[2])
{
	int i;
	struct mem_cgroup *memcg;
	unsigned long cur_sensitivity = 0;

	for(i = 0; i < LIMIT_TENANTS; i++) {
		memcg = READ_ONCE(memcg_list[i]);
		if(memcg) {
			if(READ_ONCE(memcg->region_determined) &&
				memcg->hot_region_dram_sensitivity > 0) {
				if(dram_determined[i][0] == 0 &&
					cur_sensitivity < memcg->hot_region_dram_sensitivity) {
					cur_sensitivity = memcg->hot_region_dram_sensitivity;
					*hs_idx = i;
					*hot = true;
				}
				if(dram_determined[i][1] == 0 &&
					cur_sensitivity < memcg->cold_region_dram_sensitivity) {
					cur_sensitivity = memcg->cold_region_dram_sensitivity;
					*hs_idx = i;
					*hot = false;
				}
			}
		}
	}

	return cur_sensitivity;
}


static void distribute_local_dram_region(void)
{
	int i;
	unsigned long required_dram = 0, tot_free_dram = mttm_local_dram;
	struct mem_cgroup *memcg;
	bool not_region_determined = false, all_fixed = true;
	unsigned long tot_dram_sensitivity = 0;
	int dram_determined[LIMIT_TENANTS][2] = {0,};// 0 : hot( >= lev 2), 1 : cold (lev 1)
	unsigned long dram_size[LIMIT_TENANTS][2] = {0,};
	int hs_idx;
	bool hot;
	unsigned long cur_sensitivity = 0;

	// Check that not classified workload exist.
	for(i = 0; i < LIMIT_TENANTS; i++) {
		memcg = READ_ONCE(memcg_list[i]);
		if(memcg) {
			if(!READ_ONCE(memcg->region_determined)) {
				not_region_determined = true;
				tot_free_dram -= memcg->max_nr_dram_pages;
			}
			if(!READ_ONCE(memcg->dram_fixed))
				all_fixed = false;
		}
	}

	if(all_fixed)
		return;
	
	// Distribute local dram to sensitive region first
	tot_dram_sensitivity = get_tot_dram_sensitivity();
	while(1) {
		// Find highest sensitivity region
		cur_sensitivity = 0;
		hs_idx = 0;
		hot = false;
		
		cur_sensitivity = find_highest_dram_sensitivity(&hs_idx, &hot, dram_determined);
		if(cur_sensitivity == 0)
			break;

		// Determine dram size for selected region
		memcg = READ_ONCE(memcg_list[hs_idx]);
		if(hot) {
			dram_size[hs_idx][0] = calculate_hot_region_dram_size(memcg, tot_free_dram,
								tot_dram_sensitivity, dram_determined);
			dram_determined[hs_idx][0] = 1;
			tot_free_dram -= dram_size[hs_idx][0];
			tot_dram_sensitivity -= memcg->hot_region_dram_sensitivity;
		}
		else {
			dram_size[hs_idx][1] = calculate_cold_region_dram_size(memcg, tot_free_dram,
								tot_dram_sensitivity, dram_determined);
			dram_determined[hs_idx][1] = 1;
			tot_free_dram -= dram_size[hs_idx][1];
			tot_dram_sensitivity -= memcg->cold_region_dram_sensitivity;
		}
		if(!not_region_determined)
			pr_info("[%s] [ %s ] %s region size set to %lu MB\n",
				__func__, memcg->tenant_name, hot ? "hot" : "cold",
				hot ? (dram_size[hs_idx][0] >> 8) : (dram_size[hs_idx][1] >> 8));
	}

	// When extra local DRAM exist
	// TODO : give extra to 
	/*if(tot_free_dram > 0) {
		for(i = 0; i < LIMIT_TENANTS; i++) {
			memcg = READ_ONCE(memcg_list[i]);
			if(memcg) {
				if(READ_ONCE(memcg->region_determined) &&
					dram_determined[i][0] == 1 &&
					dram_determined[i][1] == 1) {
					unsigned long remained_rss = get_anon_rss(memcg)
								- dram_size[i][0] - dram_size[i][1];
					unsigned long additional_dram = min_t(unsigned long, remained_rss,
						tot_free_dram * memcg->highest_rate / get_tot_highest_rate());
					// Extra local DRAM is added to cold region size
					dram_size[i][1] += additional_dram;
					pr_info("[%s] [ %s ] %lu / %lu MB extra dram added\n",
						__func__, memcg->tenant_name, additional_dram >> 8,
						tot_free_dram >> 8);
				}
			}
		}
	}*/


	// Set dram size
	for(i = 0; i < LIMIT_TENANTS; i++) {
		memcg = READ_ONCE(memcg_list[i]);
		if(memcg) {
			if(READ_ONCE(memcg->region_determined) &&
				dram_determined[i][0] &&
				dram_determined[i][1]) {
				if(!not_region_determined)
					pr_info("[%s] [ %s ] dram set to %lu MB [hot : %lu MB, cold : %lu MB]\n",
						__func__, memcg->tenant_name, (dram_size[i][0] + dram_size[i][1]) >> 8,
						dram_size[i][0] >> 8, dram_size[i][1] >> 8);

				set_dram_size(memcg, dram_size[i][0] + dram_size[i][1], !not_region_determined);
			}
		}
	}

	if(!not_region_determined)
		pr_info("[%s] remained DRAM : %lu MB\n",__func__, tot_free_dram >> 8);

}


static unsigned long strong_hot_dram_tolerance(struct mem_cgroup *memcg)
{
	unsigned long tolerance;
	unsigned long rss_over_lev2 = get_anon_rss(memcg) * 100 / memcg->lev2_size;
	unsigned long max_tolerance = min_t(unsigned long, 100 + rss_over_lev2 / 5, 400);

	tolerance = 100 + 1000000 / memcg->hotness_intensity;
	//pr_info("[%s] [ %s ] tolerance : %lu, max_tolerance : %lu\n",
	//	__func__, memcg->tenant_name, tolerance, max_tolerance);

	return min_t(unsigned long, tolerance, max_tolerance);
}

static unsigned long strong_hot_dram_demand(struct mem_cgroup *memcg)
{
	return memcg->lev2_size * strong_hot_dram_tolerance(memcg) / 100;
}

static unsigned long weak_hot_dram_demand(struct mem_cgroup *memcg)
{
	unsigned long padding = ((100UL << 20) >> 12);//100MB
	if(get_anon_rss(memcg) < memcg->max_anon_rss / 2)
		return memcg->max_nr_dram_pages;

	return (get_anon_rss(memcg) * 100 / 97) + padding;//promotion wmark is 3%
}


static enum workload_type get_workload_type(struct mem_cgroup *memcg)
{
	if(READ_ONCE(memcg->hi_determined)) {
		if(READ_ONCE(memcg->hotness_intensity) >= hotness_intensity_threshold)
			return STRONG_HOT;
		else
			return WEAK_HOT;
	}
	else
		return NOT_CLASSIFIED;
}

static unsigned long get_dram_demand_hi(struct mem_cgroup *memcg)
{
	if(get_workload_type(memcg) == STRONG_HOT)
		return strong_hot_dram_demand(memcg);
	else if(get_workload_type(memcg) == WEAK_HOT)
		return weak_hot_dram_demand(memcg);
	else
		return memcg->max_nr_dram_pages;
}

static unsigned long get_tot_rate(enum workload_type w_type)
{
	int i;
	struct mem_cgroup *memcg;
	unsigned long tot_rate = 0;

	for(i = 0; i < LIMIT_TENANTS; i++) {
		memcg = READ_ONCE(memcg_list[i]);
		if(memcg) {
			if(READ_ONCE(memcg->hi_determined)) {
				if(get_workload_type(memcg) == w_type) {
					unsigned long valid_rate = memcg->highest_rate;
					if(valid_rate > 6000) {
						unsigned long extra_rate = valid_rate - 6000;
						valid_rate = 6000 + (extra_rate / 10);
					}
					tot_rate += valid_rate;
				}
			}
		}
	}

	return tot_rate;
}

static unsigned long calculate_strong_hot_dram_size(struct mem_cgroup *memcg,
			unsigned long tot_free_dram, unsigned long tot_rate,
			int *dram_determined)
{
	unsigned long dram_demand = get_dram_demand_hi(memcg);
	unsigned long valid_rate = memcg->highest_rate;
	unsigned long extra_rate;
	unsigned long available_dram;
	unsigned long remained_required_dram = 0, expected_extra_dram = 0;
	int i;
	struct mem_cgroup *memcg_iter;

	if(valid_rate > 6000) {
		extra_rate = valid_rate - 6000;
		valid_rate = 6000 + (extra_rate / 10);
	}
	available_dram = tot_free_dram * valid_rate / tot_rate;

	// Calculate remained required dram
	for(i = 0; i < LIMIT_TENANTS; i++) {
		memcg_iter = READ_ONCE(memcg_list[i]);
		if(memcg_iter) {
			if(READ_ONCE(memcg_iter->hi_determined) &&
				get_workload_type(memcg) == STRONG_HOT) {
				if(dram_determined[i] == 0) {
					remained_required_dram += get_dram_demand_hi(memcg_iter);
				}
			}
		}
	}
	remained_required_dram -= dram_demand;

	if(dram_demand > available_dram && 
		remained_required_dram < tot_free_dram - available_dram) {
		// In this case, we can give more than available_dram
		expected_extra_dram = tot_free_dram - available_dram - remained_required_dram;
	}

	return min_t(unsigned long, available_dram + expected_extra_dram, dram_demand);
}

static unsigned long calculate_weak_hot_dram_size(struct mem_cgroup *memcg,
			unsigned long tot_free_dram, unsigned long tot_rate,
			int *dram_determined)
{
	unsigned long dram_demand = get_dram_demand_hi(memcg);
	unsigned long valid_rate = memcg->highest_rate;
	unsigned long extra_rate;
	unsigned long available_dram;
	unsigned long remained_required_dram = 0, expected_extra_dram = 0;
	int i;
	struct mem_cgroup *memcg_iter;

	if(valid_rate > 6000) {
		extra_rate = valid_rate - 6000;
		valid_rate = 6000 + (extra_rate / 10);
	}
	available_dram = tot_free_dram * valid_rate / tot_rate;

	// Calculate remained required dram
	for(i = 0; i < LIMIT_TENANTS; i++) {
		memcg_iter = READ_ONCE(memcg_list[i]);
		if(memcg_iter) {
			if(READ_ONCE(memcg_iter->hi_determined) &&
				get_workload_type(memcg) == WEAK_HOT) {
				if(dram_determined[i] == 0) {
					remained_required_dram += get_dram_demand_hi(memcg_iter);
				}
			}
		}
	}
	remained_required_dram -= dram_demand;

	if(dram_demand > available_dram && 
		remained_required_dram < tot_free_dram - available_dram) {
		// In this case, we can give more than available_dram
		expected_extra_dram = tot_free_dram - available_dram - remained_required_dram;
	}

	return min_t(unsigned long, available_dram + expected_extra_dram, dram_demand);
}

static unsigned long find_highest_access_rate(int *idx, int *dram_determined,
						enum workload_type w_type)
{
	int i;
	struct mem_cgroup *memcg;
	unsigned long cur_highest_rate = 0;

	for(i = 0; i < LIMIT_TENANTS; i++) {
		memcg = READ_ONCE(memcg_list[i]);
		if(memcg) {
			if(READ_ONCE(memcg->hi_determined) &&
				get_workload_type(memcg) == w_type) {
				unsigned long valid_rate = (memcg->mean_rate > 50) ?
						memcg->mean_rate : memcg->highest_rate;
				if(dram_determined[i] == 0 &&
					cur_highest_rate < valid_rate) {
					cur_highest_rate = valid_rate;
					*idx = i;
				}
			}
		}
	}

	return cur_highest_rate;
}


static void distribute_local_dram_hi(void)
{
	int i, idx;
	unsigned long tot_free_dram = mttm_local_dram;
	unsigned long tot_strong_hot_rate = 0, tot_weak_hot_rate = 0;
	struct mem_cgroup *memcg;
	bool not_hi_determined = false, all_fixed = true;
	unsigned long cur_highest_rate = 0;
	int dram_determined[LIMIT_TENANTS] = {0,};
	unsigned long dram_size[LIMIT_TENANTS] = {0,};
	unsigned long valid_rate;

	// Check that not classified workload exist.
	for(i = 0; i < LIMIT_TENANTS; i++) {
		memcg = READ_ONCE(memcg_list[i]);
		if(memcg) {
			if(!READ_ONCE(memcg->hi_determined)) {
				not_hi_determined = true;
				tot_free_dram -= memcg->max_nr_dram_pages;
			}
			if(!READ_ONCE(memcg->dram_fixed))
				all_fixed = false;
		}
	}

	if(all_fixed)
		return;


	// Distribute local dram to strong hot which has higher access rate first
	tot_strong_hot_rate = get_tot_rate(STRONG_HOT);
	while(1) {
		// Find tenant with highest access rate 
		cur_highest_rate = 0;
		idx = 0;
		
		cur_highest_rate = find_highest_access_rate(&idx, dram_determined, STRONG_HOT);
		if(cur_highest_rate == 0)
			break;

		// Determine dram size 
		memcg = READ_ONCE(memcg_list[idx]);
		valid_rate = memcg->highest_rate;
		if(valid_rate > 6000) {
			unsigned long extra_rate = valid_rate - 6000;
			valid_rate = 6000 + (extra_rate / 10);
		}	
		dram_size[idx] = calculate_strong_hot_dram_size(memcg, tot_free_dram,
							tot_strong_hot_rate, dram_determined);
		dram_determined[idx] = 1;
		tot_free_dram -= dram_size[idx];
		tot_strong_hot_rate -= valid_rate;

		if(!not_hi_determined)
			pr_info("[%s] [ %s ] strong hot. dram set to %lu MB. rate : %lu\n",
				__func__, memcg->tenant_name, dram_size[idx] >> 8, valid_rate);

	}

	// Distribute local dram to weak hot which has higher access rate first
	tot_weak_hot_rate = get_tot_rate(WEAK_HOT);
	while(1) {
		// Find tenant with highest access rate 
		cur_highest_rate = 0;
		idx = 0;
		
		cur_highest_rate = find_highest_access_rate(&idx, dram_determined, WEAK_HOT);
		if(cur_highest_rate == 0)
			break;

		// Determine dram size 
		memcg = READ_ONCE(memcg_list[idx]);
		valid_rate = memcg->highest_rate;
		if(valid_rate > 6000) {
			unsigned long extra_rate = valid_rate - 6000;
			valid_rate = 6000 + (extra_rate / 10);
		}
		dram_size[idx] = calculate_weak_hot_dram_size(memcg, tot_free_dram,
							tot_weak_hot_rate, dram_determined);
		dram_determined[idx] = 1;
		tot_free_dram -= dram_size[idx];
		tot_weak_hot_rate -= valid_rate;

		if(!not_hi_determined)
			pr_info("[%s] [ %s ] weak hot. dram set to %lu MB. rate : %lu\n",
				__func__, memcg->tenant_name, dram_size[idx] >> 8, valid_rate);

	}


	for(i = 0; i < LIMIT_TENANTS; i++) {
		memcg = READ_ONCE(memcg_list[i]);
		if(memcg) {
			if(READ_ONCE(memcg->hi_determined) &&
				dram_determined[i] == 1) {
				set_dram_size(memcg, dram_size[i], !not_hi_determined);
			}
		}
	}

	if(!not_hi_determined)
		pr_info("[%s] remained DRAM : %lu MB\n",
			__func__, tot_free_dram >> 8);
}


static int ksampled(void *dummy)
{
	unsigned long sleep_timeout = usecs_to_jiffies(20000);
	unsigned long total_time, total_cputime = 0, one_cputime, cur;
	unsigned long interval_start;
	unsigned long trace_period = msecs_to_jiffies(ksampled_trace_period_in_ms);
	struct mem_cgroup *memcg;
	int rate_cnt = 0;

	total_time = jiffies;
	interval_start = jiffies;
	while(!kthread_should_stop()) {
		one_cputime = jiffies;
		ksampled_do_work();

		cur = jiffies;
		if(cur - interval_start >= trace_period) {
			rate_cnt++;
			calculate_sample_rate_stat(rate_cnt);
			
			if(use_dram_determination) {
				if(use_region_separation) {
					calculate_dram_sensitivity();
					distribute_local_dram_region();
				}
				else if(use_hotness_intensity) {
					distribute_local_dram_hi();//hotness_intensity
				}
			}

			interval_start = cur;
		}
		total_cputime += (jiffies - one_cputime);
		schedule_timeout_interruptible(sleep_timeout);
	}
	total_time = jiffies - total_time;

	pr_info("[%s] total_time : %lu, total_cputime : %lu\n",
		__func__, total_time, total_cputime);
	return 0;
}

static int ksampled_run(void)
{
	int ret = 0, cpu, event, i;
	dma_cap_mask_t copy_mask;

	if (!ksampled_thread) {
		pfe = kzalloc(sizeof(struct perf_event **) * CORES_PER_SOCKET, GFP_KERNEL);
		for(cpu = 0; cpu < CORES_PER_SOCKET; cpu++) {
			pfe[cpu] = kzalloc(sizeof(struct perf_event *) * NR_EVENTTYPE, GFP_KERNEL);
		}

		if(!memcg_list)
			memcg_list = kzalloc(sizeof(struct mem_cgroup *) * LIMIT_TENANTS, GFP_KERNEL);
		
		for(cpu = 0; cpu < CORES_PER_SOCKET; cpu++) {
			for(event = 0; event < NR_EVENTTYPE; event++) {
				if(get_pebs_event(event) == NR_EVENTTYPE) {
					pfe[cpu][event] = NULL;
					continue;
				}
				else if((get_pebs_event(event) == ALL_STORES) &&
					!use_all_stores) {
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
		//ksampled_thread = kthread_run(ksampled, NULL, "ksampled");
		if(IS_ERR(ksampled_thread)) {
			pr_err("Failed to start ksampled\n");
			ret = PTR_ERR(ksampled_thread);
			ksampled_thread = NULL;
		}
		else {
			if(use_dma_migration) {
				dma_cap_zero(copy_mask);
				dma_cap_set(DMA_MEMCPY, copy_mask);
				dmaengine_get();
			
				for(i = 0; i < NUM_AVAIL_DMA_CHAN; i++) {
					if(!copy_chan[i])
						copy_chan[i] = dma_request_channel(copy_mask, NULL, NULL);
					if(!copy_chan[i]) {
						pr_err("%s: cannot grap channel: %d\n", __func__, i);
						continue;
					}

					copy_dev[i] = copy_chan[i]->device;
					if(!copy_dev[i]) {
						pr_err("%s: no device: %d\n", __func__, i);
						continue;
					}
				}

				pr_info("[%s] dma channel opened\n",__func__);
			}
			pr_info("[%s] ksampled start\n",__func__);
		}
	}

	return ret;
}

static void ksampled_stop(void)
{
	int cpu, event, i;
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
	if(memcg_list) {
		kfree(memcg_list);
		memcg_list = NULL;
	}
	if(use_dma_migration) {
		for(i = 0; i < NUM_AVAIL_DMA_CHAN; i++) {
			if(copy_chan[i]) {
				dma_release_channel(copy_chan[i]);
				copy_chan[i] = NULL;
				copy_dev[i] = NULL;
			}
		}

		dmaengine_put();
	}
	
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

int sysctl_mttm_local_dram(struct ctl_table *table, int write,
			void *buffer, size_t *lenp, loff_t *ppos)
{
	int err = 0;
	unsigned long max;

	if (write && !capable(CAP_SYS_ADMIN))
		return -EPERM;

	err = proc_dostring(table, write, buffer, lenp, ppos);

	if (err < 0)
		return err;
	if (write) {
		err = page_counter_memparse(mttm_local_dram_string,
				"max_local_dram", &max);
		if(err)
			pr_err("[%s] Failed to set mttm_local_dram\n",
				__func__);
		else {
			WRITE_ONCE(mttm_local_dram, max);
			pr_info("[%s] mttm_local_dram set to %lu GB\n",
				__func__, max >> 18);
		}
	}
	return err;
}
#endif





