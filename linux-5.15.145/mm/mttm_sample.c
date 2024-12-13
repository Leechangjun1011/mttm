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
unsigned int check_stable_sample_rate = 1;
unsigned int use_dram_determination = 1;
unsigned int use_memstrata_policy = 0;
unsigned long donor_threshold = 4000;
unsigned long acceptor_threshold = 50;
unsigned int use_region_separation = 1;
unsigned int use_hotness_intensity = 0;
unsigned int use_naive_hi = 0;
unsigned int use_pingpong_reduce = 1;
unsigned int remote_latency = 130;
unsigned int print_more_info = 0;
unsigned long pingpong_reduce_threshold = 200;
unsigned long manage_cputime_threshold = 50;
unsigned long mig_cputime_threshold = 200;
char mttm_local_dram_string[16];
unsigned long mttm_local_dram = ((80UL << 30) >> 12);//80GB in # of pages
unsigned int ksampled_trace_period_in_ms = 5000;
unsigned int use_lru_manage_reduce = 1;
#define NUM_AVAIL_DMA_CHAN	16
#define DMA_CHAN_PER_PAGE	1
unsigned int use_dma_migration = 0;
struct dma_chan *copy_chan[NUM_AVAIL_DMA_CHAN];
struct dma_device *copy_dev[NUM_AVAIL_DMA_CHAN];
struct dma_chan *memset_chan[NUM_AVAIL_DMA_CHAN];
struct dma_device *memset_dev[NUM_AVAIL_DMA_CHAN];
unsigned int use_all_stores = 0;
int current_tenants = 0;
struct mem_cgroup **memcg_list = NULL;
struct task_struct *ksampled_thread = NULL;
struct perf_event ***pfe;
DEFINE_SPINLOCK(register_lock);

extern int enabled_kptscand;
extern struct task_struct *kptscand_thread;
unsigned int scanless_cooling = 1;
unsigned int reduce_scan = 1;

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

unsigned int get_idx(uint32_t num)
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

uint32_t *get_ac_pointer(struct mem_cgroup *memcg, unsigned int giga_bitmap_idx,
			unsigned int huge_bitmap_idx, unsigned int base_bitmap_idx)
{
	if(giga_bitmap_idx >= memcg->giga_bitmap_size ||
		huge_bitmap_idx >= memcg->huge_bitmap_size ||
		base_bitmap_idx >= memcg->base_bitmap_size)
		return NULL;
	return &memcg->ac_page_list[giga_bitmap_idx][huge_bitmap_idx][base_bitmap_idx];
}

void find_bitmap_idx(struct mem_cgroup *memcg, unsigned int *giga_idx,
			unsigned int *huge_idx, unsigned int *base_idx)
{
	*giga_idx = find_first_zero_bit(memcg->giga_bitmap, memcg->giga_bitmap_size);
	if(*giga_idx == memcg->giga_bitmap_size) {
		pr_info("[%s] No zero bit in giga_bitmap\n",
			__func__);
		*giga_idx = 0;
		*huge_idx = 0;
		*base_idx = 0;
	}
	else {
		*huge_idx = find_first_zero_bit(memcg->huge_bitmap[*giga_idx], memcg->huge_bitmap_size);
		*base_idx = find_first_zero_bit(memcg->base_bitmap[*giga_idx][*huge_idx], memcg->base_bitmap_size);
		
		set_bit(*base_idx, memcg->base_bitmap[*giga_idx][*huge_idx]);
		memcg->free_base_bits[*giga_idx][*huge_idx]--;
		if(memcg->free_base_bits[*giga_idx][*huge_idx] == 0) {
			set_bit(*huge_idx, memcg->huge_bitmap[*giga_idx]);
			memcg->free_huge_bits[*giga_idx]--;
			if(memcg->free_huge_bits[*giga_idx] == 0) {
				set_bit(*giga_idx, memcg->giga_bitmap);
				memcg->free_giga_bits--;
			}
		}	
	}

}




// Basepage initiallization
// Called after pte_alloc_one in memory.c to initialize pginfo in pte.
int set_page_coolstatus(struct page *page, pte_t *pte, struct mm_struct *mm)
{
	struct mem_cgroup *memcg = get_mem_cgroup_from_mm(mm);
	struct page *pte_page;
	pginfo_t *pginfo;
	int initial_hotness;
	unsigned int giga_idx = 0, huge_idx = 0, base_idx = 0, i;
	uint32_t *ac;

	if(!memcg)
		return 0;

	pte_page = virt_to_page((unsigned long)pte);
	if(!PageMttm(pte_page))
		return 0;

	pginfo = get_pginfo_from_pte(pte);
	if(!pginfo)
		return 0;

	initial_hotness = 0; //get_accesses_from_idx(memcg->active_threshold + 1);

	if(scanless_cooling) {
		spin_lock(&memcg->bitmap_lock);
		find_bitmap_idx(memcg, &giga_idx, &huge_idx, &base_idx);
		pginfo->giga_bitmap_idx = giga_idx;
		pginfo->huge_bitmap_idx = huge_idx;
		pginfo->base_bitmap_idx = base_idx;
		/*if(!memcg->ac_page_list[pginfo->meta_bitmap_idx]) {
			pr_info("pginfo->meta_bitmap_idx : %u\n", pginfo->meta_bitmap_idx);
			pr_info("meta_idx : %u\n", meta_idx);
			for(i = 0; i < memcg->ac_bitmap_max_rss; i++)
				pr_info("%dth meta_bitmap : %d\n",
					i, test_bit(i, memcg->meta_bitmap) ? 1 : 0);
			for(i = 0; i < 10; i++)
				pr_info("%dth ac_bitmap : %d\n",
					i, test_bit(i, memcg->ac_bitmap_list[0]) ? 1 : 0);
			BUG();
		}*/
		ac = get_ac_pointer(memcg, pginfo->giga_bitmap_idx,
			pginfo->huge_bitmap_idx, pginfo->base_bitmap_idx);
		if(ac)
			*ac = initial_hotness;

		spin_unlock(&memcg->bitmap_lock);
	}
	else
		pginfo->nr_accesses = initial_hotness;

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
	unsigned int giga_idx = 0, huge_idx = 0, base_idx = 0;
	uint32_t *ac;

	SetPageMttm(&page[3]);

	if(!memcg) // page alloc at migration
		return;

	if(scanless_cooling) {	
		spin_lock(&memcg->bitmap_lock);
		find_bitmap_idx(memcg, &giga_idx, &huge_idx, &base_idx);
		page[3].giga_bitmap_idx = giga_idx;
		page[3].huge_bitmap_idx = huge_idx;
		page[3].base_bitmap_idx = base_idx;
		ac = get_ac_pointer(memcg, page[3].giga_bitmap_idx,
			page[3].huge_bitmap_idx, page[3].base_bitmap_idx);
		if(ac)
			*ac = initial_hotness;
		spin_unlock(&memcg->bitmap_lock);
	}
	else
		page[3].nr_accesses = initial_hotness;
	
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

	if(scanless_cooling) {
		newpage[3].giga_bitmap_idx = page[3].giga_bitmap_idx;
		newpage[3].huge_bitmap_idx = page[3].huge_bitmap_idx;
		newpage[3].base_bitmap_idx = page[3].base_bitmap_idx;
	}
	else
		newpage[3].nr_accesses = page[3].nr_accesses;
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
	unsigned int giga_idx, huge_idx, base_idx;
	pginfo_t *pginfo;

	if(!memcg)
		return;
	if(!memcg->mttm_enabled)
		return;

	pte_page = virt_to_page((unsigned long)pte);
	if(!PageMttm(pte_page))
		return;

	pginfo = get_pginfo_from_pte(pte);
	if(!pginfo)
		return;

	if(scanless_cooling) {
		spin_lock(&memcg->bitmap_lock);
		giga_idx = pginfo->giga_bitmap_idx;
		huge_idx = pginfo->huge_bitmap_idx;
		base_idx = pginfo->base_bitmap_idx;
		if(!memcg->ac_page_list) {
			spin_unlock(&memcg->bitmap_lock);
			return;
		}
		idx = get_idx(memcg->ac_page_list[giga_idx][huge_idx][base_idx]);
		if(!test_bit(base_idx, memcg->base_bitmap[giga_idx][huge_idx])) {
			//pr_info("[%s] allocated bitmap is not set. gi : %u, hi : %u, bi : %u\n",
			//	__func__, giga_idx, huge_idx, base_idx);
		}
		else {
			clear_bit(base_idx, memcg->base_bitmap[giga_idx][huge_idx]);
			memcg->free_base_bits[giga_idx][huge_idx]++;
			if(memcg->free_base_bits[giga_idx][huge_idx] == 1) {
				clear_bit(huge_idx, memcg->huge_bitmap[giga_idx]);
				memcg->free_huge_bits[giga_idx]++;
				if(memcg->free_huge_bits[giga_idx] == 1) {
					clear_bit(giga_idx, memcg->giga_bitmap);
					memcg->free_giga_bits++;
				}
			}
		}
		spin_unlock(&memcg->bitmap_lock);
	}
	else
		idx = get_idx(pginfo->nr_accesses);

	if(idx > 15 || idx < 0)
		return;

	spin_lock(&memcg->access_lock);
	if(memcg->hotness_hg[idx] > 0)
		memcg->hotness_hg[idx]--;
	spin_unlock(&memcg->access_lock);	

}

void uncharge_mttm_page(struct page *page, struct mem_cgroup *memcg)
{
	unsigned int nr_pages = thp_nr_pages(page);
	unsigned int idx;
	unsigned int giga_idx, huge_idx, base_idx;


	if(!memcg)
		return;
	if(!memcg->mttm_enabled)
		return;

	page = compound_head(page);
	if(nr_pages != 1) {
		struct page *meta_page = get_meta_page(page);

		if(scanless_cooling) {
			spin_lock(&memcg->bitmap_lock);
			giga_idx = meta_page->giga_bitmap_idx;
			huge_idx = meta_page->huge_bitmap_idx;
			base_idx = meta_page->base_bitmap_idx;
			if(!memcg->ac_page_list) {
				spin_unlock(&memcg->bitmap_lock);
				return;
			}
			idx = get_idx(memcg->ac_page_list[giga_idx][huge_idx][base_idx]);
			if(!test_bit(base_idx, memcg->base_bitmap[giga_idx][huge_idx])) {
				//pr_info("[%s] allocated bitmap is not set. gi : %u, hi : %u, bi : %u\n",
				//	__func__, giga_idx, huge_idx, base_idx);
			}
			else {
				clear_bit(base_idx, memcg->base_bitmap[giga_idx][huge_idx]);
				memcg->free_base_bits[giga_idx][huge_idx]++;
				if(memcg->free_base_bits[giga_idx][huge_idx] == 1) {
					clear_bit(huge_idx, memcg->huge_bitmap[giga_idx]);
					memcg->free_huge_bits[giga_idx]++;
					if(memcg->free_huge_bits[giga_idx] == 1) {
						clear_bit(giga_idx, memcg->giga_bitmap);
						memcg->free_giga_bits++;
					}
				}
			}
			spin_unlock(&memcg->bitmap_lock);
		}
		else
			idx = get_idx(meta_page->nr_accesses);

		if(idx > 15 || idx < 0)
			return;

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
		case MEMSTORE:
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
				case MEMSTORE:
				default:
					ret = 0;
					break;
			}

			if(ret == -EINVAL)
				pr_err("[%s] failed to update sample period",__func__);
		}
	}
}

SYSCALL_DEFINE2(mttm_register_pid,
		pid_t, pid, const char __user *, u_name)
{
	int i, j;
	struct mem_cgroup *memcg = mem_cgroup_from_task(current);
	char name[PATH_MAX];
	
	size_t max_rss_in_GB = 100;//100GB when use only basepage
	size_t ac_page_size = 0;
	size_t giga_bitmap_size = 0, huge_bitmap_size = 0, base_bitmap_size = 0;
	bool alloc_fail = false;

	spin_lock(&register_lock);

	if(current_tenants == LIMIT_TENANTS) {
		pr_info("[%s] Can't register tenant due to limit\n",__func__);
		spin_unlock(&register_lock);
		return 0;
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
			if(memcg_list[i]->mttm_enabled && (use_dram_determination || use_memstrata_policy)) {
				WRITE_ONCE(memcg_list[i]->nodeinfo[0]->max_nr_base_pages, mttm_local_dram / current_tenants);
				WRITE_ONCE(memcg_list[i]->max_nr_dram_pages, mttm_local_dram / current_tenants);
				WRITE_ONCE(memcg_list[i]->init_dram_size, memcg_list[i]->max_nr_dram_pages);
				pr_info("[%s] [ %s ] dram size set to %lu MB\n",
					__func__, memcg_list[i]->tenant_name, memcg_list[i]->max_nr_dram_pages >> 8);
			}
		}
	}

	if(scanless_cooling) {
		spin_lock(&memcg->bitmap_lock);
		ac_page_size = ((1UL << 30) / HPAGE_SIZE) * sizeof(uint32_t);//single access count page size
		giga_bitmap_size = max_rss_in_GB;
		huge_bitmap_size = ((1UL << 30) / HPAGE_SIZE);
		base_bitmap_size = (HPAGE_SIZE / PAGE_SIZE);

		// Alloc ac_page_list
		memcg->ac_page_list = kzalloc(giga_bitmap_size * sizeof(uint32_t **), GFP_KERNEL);
		BUG_ON(!memcg->ac_page_list);
		for(i = 0; i < giga_bitmap_size; i++) {
			memcg->ac_page_list[i] = kzalloc(huge_bitmap_size * sizeof(uint32_t *), GFP_KERNEL);
			BUG_ON(!memcg->ac_page_list[i]);
		}
		for(i = 0; i < giga_bitmap_size; i++) {
			for(j = 0; j < huge_bitmap_size; j++) {
				memcg->ac_page_list[i][j] = kzalloc(base_bitmap_size * sizeof(uint32_t), GFP_KERNEL);
				BUG_ON(!memcg->ac_page_list[i][j]);
			}
		}

		// Alloc bitmaps
		memcg->giga_bitmap = bitmap_zalloc(giga_bitmap_size, GFP_KERNEL);
		BUG_ON(!memcg->giga_bitmap);
		memcg->huge_bitmap = kzalloc(giga_bitmap_size * sizeof(unsigned long *), GFP_KERNEL);
		BUG_ON(!memcg->huge_bitmap);
		for(i = 0; i < giga_bitmap_size; i++) {
			memcg->huge_bitmap[i] = bitmap_zalloc(huge_bitmap_size, GFP_KERNEL);
			BUG_ON(!memcg->huge_bitmap[i]);
		}
		memcg->base_bitmap = kzalloc(giga_bitmap_size * sizeof(unsigned long **), GFP_KERNEL);
		BUG_ON(!memcg->base_bitmap);
		for(i = 0; i < giga_bitmap_size; i++) {
			memcg->base_bitmap[i] = kzalloc(huge_bitmap_size * sizeof(unsigned long *), GFP_KERNEL);
			BUG_ON(!memcg->base_bitmap[i]);
		}
		for(i = 0; i < giga_bitmap_size; i++) {
			for(j = 0; j < huge_bitmap_size; j++) {
				memcg->base_bitmap[i][j] = bitmap_zalloc(base_bitmap_size, GFP_KERNEL);
				BUG_ON(!memcg->base_bitmap[i][j]);
			}
		}

		memcg->free_giga_bits = giga_bitmap_size;
		memcg->free_huge_bits = kzalloc(giga_bitmap_size * sizeof(unsigned int), GFP_KERNEL);
		BUG_ON(!memcg->free_huge_bits);
		for(i = 0; i < giga_bitmap_size; i++)
			memcg->free_huge_bits[i] = huge_bitmap_size;
		memcg->free_base_bits = kzalloc(giga_bitmap_size * sizeof(unsigned int *), GFP_KERNEL);
		BUG_ON(!memcg->free_base_bits);
		for(i = 0; i < giga_bitmap_size; i++) {
			memcg->free_base_bits[i] = kzalloc(huge_bitmap_size * sizeof(unsigned int), GFP_KERNEL);
			BUG_ON(!memcg->free_base_bits[i]);
		}
		for(i = 0; i < giga_bitmap_size; i++) {
			for(j = 0; j < huge_bitmap_size; j++) {
				memcg->free_base_bits[i][j] = base_bitmap_size;
			}
		}

		memcg->giga_bitmap_size = giga_bitmap_size;
		memcg->huge_bitmap_size = huge_bitmap_size;
		memcg->base_bitmap_size = base_bitmap_size;

		spin_unlock(&memcg->bitmap_lock);
	}


	pr_info("[%s] registered pid : %d. name : [ %s ], current_tenants : %d, dma_chan_start : %u, local_dram : %lu MB\n",
		__func__, pid, memcg->tenant_name, current_tenants, memcg->dma_chan_start, (mttm_local_dram / current_tenants) >> 8);

	spin_unlock(&register_lock);
	
	return 0;
}

SYSCALL_DEFINE1(mttm_unregister_pid,
		pid_t, pid)
{
	int i, j;
	struct mem_cgroup *memcg = mem_cgroup_from_task(current);
	spin_lock(&register_lock);

	current_tenants--;
	kmigrated_stop(memcg);

	for(i = 0; i < LIMIT_TENANTS; i++) {
		if(READ_ONCE(memcg_list[i]) == memcg) {
			WRITE_ONCE(memcg_list[i], NULL);
			break;
		}	
	}

	if(scanless_cooling) {
		spin_lock(&memcg->bitmap_lock);

		for(i = 0; i < memcg->giga_bitmap_size; i++) {
			for(j = 0; j < memcg->huge_bitmap_size; j++) {
				if(memcg->base_bitmap[i][j])
					bitmap_free(memcg->base_bitmap[i][j]);
				if(memcg->ac_page_list[i][j])
					kfree(memcg->ac_page_list[i][j]);
			}
			if(memcg->base_bitmap[i])
				kfree(memcg->base_bitmap[i]);
			if(memcg->huge_bitmap[i])
				bitmap_free(memcg->huge_bitmap[i]);
			if(memcg->ac_page_list[i])
				kfree(memcg->ac_page_list[i]);
			if(memcg->free_base_bits[i])
				kfree(memcg->free_base_bits[i]);
		}
		if(memcg->giga_bitmap)
			bitmap_free(memcg->giga_bitmap);
		if(memcg->huge_bitmap)
			kfree(memcg->huge_bitmap);
		if(memcg->base_bitmap)
			kfree(memcg->base_bitmap);
		if(memcg->ac_page_list) {
			kfree(memcg->ac_page_list);
			memcg->ac_page_list = NULL;
		}
		if(memcg->free_huge_bits)
			kfree(memcg->free_huge_bits);
		if(memcg->free_base_bits)
			kfree(memcg->free_base_bits);
		spin_unlock(&memcg->bitmap_lock);
	}

	// Re-distribute local DRAM
	for(i = 0; i < LIMIT_TENANTS; i++) {
		if(READ_ONCE(memcg_list[i])) {
			WRITE_ONCE(memcg_list[i]->dram_fixed, false);
			if(use_memstrata_policy) {
				set_dram_size(memcg_list[i],
						memcg_list[i]->max_nr_dram_pages + memcg->max_nr_dram_pages / current_tenants, false);
				pr_info("[%s] [ %s ] dram set to %lu MB due to unregister\n",
					__func__, memcg_list[i]->tenant_name,
					memcg_list[i]->max_nr_dram_pages >> 8);
			}
		}
	}

	spin_unlock(&register_lock);

	pr_info("[%s] unregistered pid : %d, name : [ %s ], current_tenants : %d, total sample : %lu [local : %lu, remote : %lu]\n",
		__func__, pid, memcg->tenant_name, current_tenants, memcg->nr_sampled, memcg->nr_tot_local, memcg->nr_sampled - memcg->nr_tot_local);
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
		if(inc_thres && !reduce_scan)
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
	unsigned int init_threshold = test_bit(TRANSPARENT_HUGEPAGE_FLAG, &transparent_hugepage_flags) ?
					MTTM_INIT_THRESHOLD : 9;

	if(READ_ONCE(memcg->hg_mismatch)) {
		// Not need to adjust since threshold not changed.
		//set_lru_adjusting(memcg, true);
		return;
	}
	if(!test_bit(TRANSPARENT_HUGEPAGE_FLAG, &transparent_hugepage_flags) &&
		use_dram_determination &&
		((use_region_separation && !memcg->region_determined) || (use_hotness_intensity && !memcg->hi_determined)))
		return;


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

	if(idx_hot < init_threshold) {
		idx_hot = init_threshold;
	}

	// histogram is reset before cooling
	// some pages may not be reflected in the histogram when cooling happens
	if(memcg->cooled) {
		WRITE_ONCE(memcg->active_threshold, init_threshold + memcg->threshold_offset);
		/*if(memcg->active_threshold > MTTM_INIT_THRESHOLD)
			WRITE_ONCE(memcg->active_threshold, memcg->active_threshold - 1);*/
		memcg->cooled = false;
	}
	else {
		WRITE_ONCE(memcg->active_threshold, idx_hot + memcg->threshold_offset);
	}

	if(memcg->active_threshold != prev_threshold)
		set_lru_adjusting(memcg, true);

	if(memcg->use_warm && (memcg->active_threshold > init_threshold))
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
	}*/
	pginfo->nr_accesses = 0;

	pginfo->cooling_clock = memcg_cclock;
	cur_idx = get_idx(pginfo->nr_accesses);
	memcg->hotness_hg[cur_idx]++;

	spin_unlock(&memcg->access_lock);
}

// Only invoked when update huge pginfo.
// Not supposed to be invoked when scanless cooling.
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
		meta_page->nr_accesses = 0;
	}
	meta_page->cooling_clock = memcg_cclock;

	cur_idx = 0;

	if(prev_idx != cur_idx) {
		if(memcg->hotness_hg[prev_idx] >= HPAGE_PMD_NR)
			memcg->hotness_hg[prev_idx] -= HPAGE_PMD_NR;
		else
			memcg->hotness_hg[prev_idx] = 0;
		memcg->hotness_hg[cur_idx] += HPAGE_PMD_NR;
	}

	spin_unlock(&memcg->access_lock);
}

// Only invoked when update base pginfo and migrate base page.
// Not supposed to be invoked when scanless cooling.
void check_base_cooling(pginfo_t *pginfo, struct page *page)
{
	struct mem_cgroup *memcg = page_memcg(page);
	unsigned int memcg_cclock;
	unsigned long prev_idx, cur_idx;

	if(!memcg)
		return;
	if(!memcg->mttm_enabled)
		return;
	if(scanless_cooling)
		return;

	spin_lock(&memcg->access_lock);
	prev_idx = get_idx(pginfo->nr_accesses);

	memcg_cclock = READ_ONCE(memcg->cooling_clock);
	if(memcg_cclock > pginfo->cooling_clock) {
		pginfo->nr_accesses = 0;
	}
	pginfo->cooling_clock = memcg_cclock;
	cur_idx = 0;
	
	if(prev_idx != cur_idx) {
		if(memcg->hotness_hg[prev_idx] > 0)
			memcg->hotness_hg[prev_idx]--;
		memcg->hotness_hg[cur_idx]++;
	}

	spin_unlock(&memcg->access_lock);
}

void update_base_page(struct vm_area_struct *vma, struct page *page,
				pginfo_t *pginfo, unsigned long address)
{
	struct mem_cgroup *memcg = get_mem_cgroup_from_mm(vma->vm_mm);
	unsigned int prev_idx, cur_idx;
	uint32_t *ac;


	if(scanless_cooling) {
		ac = get_ac_pointer(memcg, pginfo->giga_bitmap_idx,
			pginfo->huge_bitmap_idx, pginfo->base_bitmap_idx);
		if(!ac)
			return;
		prev_idx = get_idx(*ac);
		(*ac) += HPAGE_PMD_NR;
		cur_idx = get_idx(*ac);
	}
	else {
		check_base_cooling(pginfo, page);
		prev_idx = get_idx(pginfo->nr_accesses);
		pginfo->nr_accesses += HPAGE_PMD_NR;
		cur_idx = get_idx(pginfo->nr_accesses);
	}

	spin_lock(&memcg->access_lock);
	if(prev_idx != cur_idx) {
		if(memcg->hotness_hg[prev_idx] > 0)
			memcg->hotness_hg[prev_idx]--;
		memcg->hotness_hg[cur_idx]++;
	}
	spin_unlock(&memcg->access_lock);

	if(cur_idx >= READ_ONCE(memcg->active_threshold))
		move_page_to_active_lru(page);
	else if(PageActive(page))
		move_page_to_inactive_lru(page);

}

/*void debug_ac_page_list(struct mem_cgroup *memcg, struct page *meta_page,
			unsigned int *prev_idx, unsigned int *cur_idx)
{
	uint32_t *ac;

	if(!memcg->mttm_enabled)
		pr_info("[%s] memcg mttm disabled\n",__func__);
	if(!memcg->ac_page_list)
		pr_info("[%s] null ac_page_list. gi:%u, hi:%u, bi:%u\n",
			__func__, meta_page->giga_bitmap_idx, meta_page->huge_bitmap_idx,
			meta_page->base_bitmap_idx);
	else if(!memcg->ac_page_list[meta_page->giga_bitmap_idx])
		pr_info("[%s] null giga bitmap. gi:%u, hi:%u, bi:%u\n"
			,__func__, meta_page->giga_bitmap_idx, meta_page->huge_bitmap_idx,
			meta_page->base_bitmap_idx);
	else if(!memcg->ac_page_list[meta_page->giga_bitmap_idx][meta_page->huge_bitmap_idx])
		pr_info("[%s] null huge bitmap. gi:%u, hi:%u, bi:%u\n",
			__func__, meta_page->giga_bitmap_idx, meta_page->huge_bitmap_idx,
			meta_page->base_bitmap_idx);
	ac = &memcg->ac_page_list[meta_page->giga_bitmap_idx][meta_page->huge_bitmap_idx][meta_page->base_bitmap_idx];
	*prev_idx = get_idx(*ac);
	(*ac)++;
	*cur_idx = get_idx(*ac);
}

void debug_hg(struct mem_cgroup *memcg, unsigned int prev_idx, unsigned int cur_idx)
{
	spin_lock(&memcg->access_lock);
	if(prev_idx != cur_idx) {
		if(memcg->hotness_hg[prev_idx] >= HPAGE_PMD_NR)
			memcg->hotness_hg[prev_idx] -= HPAGE_PMD_NR;
		else
			memcg->hotness_hg[prev_idx] = 0;
		memcg->hotness_hg[cur_idx] += HPAGE_PMD_NR;
	}
	spin_unlock(&memcg->access_lock);
}

void debug_move_page(struct mem_cgroup *memcg, struct page *page, unsigned int cur_idx)
{
	if(cur_idx >= READ_ONCE(memcg->active_threshold))
		move_page_to_active_lru(page);
	else if(PageActive(page))
		move_page_to_inactive_lru(page);
}*/

void update_huge_page(struct vm_area_struct *vma, pmd_t *pmd,
			struct page *page, unsigned long address)
{
	struct mem_cgroup *memcg = get_mem_cgroup_from_mm(vma->vm_mm);
	struct page *meta_page;
	unsigned int prev_idx, cur_idx;
	uint32_t *ac;
	
	meta_page = get_meta_page(page);

	if(scanless_cooling) {	
		ac = get_ac_pointer(memcg, meta_page->giga_bitmap_idx,
			meta_page->huge_bitmap_idx, meta_page->base_bitmap_idx);
		if(!ac)
			return;
		prev_idx = get_idx(*ac);
		(*ac)++;
		cur_idx = get_idx(*ac);
	}
	else {
		check_transhuge_cooling((void *)memcg, page);
		prev_idx = get_idx(meta_page->nr_accesses);
		meta_page->nr_accesses++;
		cur_idx = get_idx(meta_page->nr_accesses);
	}

	spin_lock(&memcg->access_lock);
	if(prev_idx != cur_idx) {
		if(memcg->hotness_hg[prev_idx] >= HPAGE_PMD_NR)
			memcg->hotness_hg[prev_idx] -= HPAGE_PMD_NR;
		else
			memcg->hotness_hg[prev_idx] = 0;
		memcg->hotness_hg[cur_idx] += HPAGE_PMD_NR;
	}
	spin_unlock(&memcg->access_lock);

	if(cur_idx >= READ_ONCE(memcg->active_threshold))
		move_page_to_active_lru(page);
	else if(PageActive(page))
		move_page_to_inactive_lru(page);

}

int __update_pte_pginfo(struct vm_area_struct *vma, pmd_t *pmd,
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

int __update_pmd_pginfo(struct vm_area_struct *vma, pud_t *pud,
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


int __update_pginfo(struct vm_area_struct *vma, unsigned long address)
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

void update_pginfo(pid_t pid, unsigned long address, enum eventtype e)
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
			memcg->nr_tot_local++;
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
	}

	// adjust threshold
	else if((memcg->nr_sampled % READ_ONCE(memcg->adjust_period)) == 0)
		adjust_active_threshold(memcg);

mmap_unlock:
	mmap_read_unlock(mm);
put_task:
	put_pid(pid_struct);
}

void ksampled_do_work(void)
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




static void check_sample_rate_is_stable(struct mem_cgroup *memcg,
				unsigned long stdev, unsigned long mean,
				unsigned long mean_rate)
{
	unsigned long sampling_factor = 10007 / pebs_sample_period;
	unsigned long std_rate = 100 * sampling_factor;

	if(stdev >= mean) {
		if(memcg->stable_cnt > 0) {
			memcg->stable_cnt = 0;
			WRITE_ONCE(memcg->stable_status, false);
		}
	}
	else if(mean_rate > std_rate) {
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
	unsigned long sampling_factor = 10007 / pebs_sample_period;
	unsigned long std_rate = 2000 * min_t(unsigned long, sampling_factor, 50);//mar is 2000 when pebs_sample_period is 10007.
	unsigned long hard_max = 12;

	if(remote_latency > 200)
		std_rate /= 2;

	if((memcg->highest_rate * 6 / 5) < memcg->mean_rate) {
		// access rate increased more than 50% of highest one
		memcg->lowered_cnt = 0;

		WRITE_ONCE(memcg->highest_rate, memcg->mean_rate);		
		WRITE_ONCE(memcg->hotness_scan_cnt, min_t(unsigned long, hard_max, max_t(unsigned long, memcg->hotness_scan_cnt, memcg->highest_rate / std_rate)));

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
	//TODO lowered necessary?	
	/*else if(memcg->highest_rate / 20 > memcg->mean_rate &&
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
	}*/

}


void calculate_sample_rate_stat(void)
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

static void calculate_sample_rate_stat_mpki(void)
{
	unsigned long mean_rate;
	int i;
	struct mem_cgroup *memcg;

	for(i = 0; i < LIMIT_TENANTS; i++) {
		memcg = READ_ONCE(memcg_list[i]);
		if(memcg) {			
			mean_rate = div64_u64(memcg->interval_nr_sampled, 10);
			memcg->mean_rate = mean_rate;

			if(memcg->nr_remote == 0)
				memcg->nr_remote = 1;
			memcg->fmmr = (memcg->fmmr * 8 +
						(memcg->nr_remote * 100 / (memcg->nr_remote + memcg->nr_local)) * 2) / 10;
		
			if(print_more_info) {
				pr_info("[%s] [ %s ] mean_rate : %lu, FMMR : %lu\n",
					__func__, memcg->tenant_name, mean_rate, memcg->fmmr);
			}
	
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

static unsigned long calculate_valid_rate(unsigned long sample_rate)
{
	unsigned long sampling_factor = 10007 / pebs_sample_period;
	unsigned long std_rate = 6000 * sampling_factor;//threshold sample rate is 6000 when pebs_sample_period is 10007
	unsigned long extra_rate;

	if(sample_rate > std_rate) {
		extra_rate = sample_rate - std_rate;
		return std_rate + (extra_rate / 10);
	}
	return sample_rate;
}

void calculate_dram_sensitivity(unsigned long (*dram_sensitivity)[NR_REGION], int (*dram_determined)[NR_REGION])
{
	int i, j;
	struct mem_cgroup *memcg;
	bool all_region_determined = true;
	unsigned long region_access_rate[LIMIT_TENANTS][NR_REGION] = {0,};

	// Check that not classified workload exist.
	for(i = 0; i < LIMIT_TENANTS; i++) {
		memcg = READ_ONCE(memcg_list[i]);
		if(memcg) {
			if(!READ_ONCE(memcg->region_determined)) {
				all_region_determined = false;
			}	
		}
	}

	for(i = 0; i < LIMIT_TENANTS; i++) {
		memcg = READ_ONCE(memcg_list[i]);
		if(memcg) {
			if(READ_ONCE(memcg->region_determined) && !READ_ONCE(memcg->dram_fixed)) {
				unsigned long valid_rate = calculate_valid_rate(memcg->highest_rate);
				unsigned long region_access_rate;
				unsigned long tot_access = 0;
				
				for(j = 0; j < NR_REGION; j++)
					tot_access += memcg->nr_region_access[j];

				for(j = 0; j < NR_REGION; j++) {
					region_access_rate = valid_rate * memcg->nr_region_access[j] / tot_access;
					if((memcg->region_size[j] >> 8) > 0) {
						dram_sensitivity[i][j] = region_access_rate * 1000 / (memcg->region_size[j] >> 8);
						if(dram_sensitivity[i][j] == 0)
							dram_determined[i][j] = 1;//this region doesn't deserve dram
					}
					else
						dram_determined[i][j] = 1;
				}

				if(all_region_determined)
					pr_info("[%s] [ %s ] MAR: [raw: %lu, valid: %lu], dram sensitivity: [0: %lu, 1: %lu, 2: %lu, 3: %lu, 4: %lu]\n",
						__func__, memcg->tenant_name, memcg->highest_rate, valid_rate,
						dram_sensitivity[i][0], dram_sensitivity[i][1], dram_sensitivity[i][2],
						dram_sensitivity[i][3], dram_sensitivity[i][4]);	
			}
		}
	}
}



static unsigned long get_tot_dram_sensitivity(unsigned long (*dram_sensitivity)[NR_REGION])
{
	int i, j;
	struct mem_cgroup *memcg;
	unsigned long tot_dram_sensitivity = 0;

	for(i = 0; i < LIMIT_TENANTS; i++) {
		memcg = READ_ONCE(memcg_list[i]);
		if(memcg) {
			if(READ_ONCE(memcg->region_determined)) {
				for(j = 0; j < NR_REGION; j++)
					tot_dram_sensitivity += dram_sensitivity[i][j];;
			}
		}
	}

	return tot_dram_sensitivity;
}


static unsigned long calculate_region_dram_size(int tenant_idx, int region_idx,
			unsigned long tot_free_dram, unsigned long tot_dram_sensitivity,
			int (*dram_determined)[NR_REGION], unsigned long (*dram_sensitivity)[NR_REGION])
{
	struct mem_cgroup *memcg = READ_ONCE(memcg_list[tenant_idx]);
	unsigned long available_dram = tot_free_dram * dram_sensitivity[tenant_idx][region_idx] /
						tot_dram_sensitivity;
	unsigned long remained_required_dram = 0, expected_extra_dram = 0;
	int i, j;
	struct mem_cgroup *memcg_iter;

	// Calculate remained required dram
	for(i = 0; i < LIMIT_TENANTS; i++) {
		memcg_iter = READ_ONCE(memcg_list[i]);
		if(memcg_iter) {
			if(READ_ONCE(memcg_iter->region_determined)) {
				for(j = 0; j < NR_REGION; j++) {
					if(dram_sensitivity[i][j] > 0 && dram_determined[i][j] == 0)//only region with positive dram sensitivity deserves dram
						remained_required_dram += memcg_iter->region_size[j];
				}
			}
		}
	}
	remained_required_dram -= memcg->region_size[region_idx];//exclude current one

	if(memcg->region_size[region_idx] > available_dram && 
		remained_required_dram < tot_free_dram - available_dram) {
		// In this case, we can give more than available_dram
		expected_extra_dram = tot_free_dram - available_dram - remained_required_dram;
	}

	return min_t(unsigned long, available_dram + expected_extra_dram, memcg->region_size[region_idx]);
}
/*
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
*/
static unsigned long find_highest_dram_sensitivity(int *tenant_idx, int *region_idx,
		unsigned long (*dram_sensitivity)[NR_REGION], int (*dram_determined)[NR_REGION])
{
	int i, j;
	struct mem_cgroup *memcg;
	unsigned long cur_sensitivity = 0;

	for(i = 0; i < LIMIT_TENANTS; i++) {
		memcg = READ_ONCE(memcg_list[i]);
		if(memcg) {
			if(READ_ONCE(memcg->region_determined)) {
				for(j = 0; j < NR_REGION; j++) {
					if(dram_determined[i][j] == 0 &&
						cur_sensitivity < dram_sensitivity[i][j]) {
						cur_sensitivity = dram_sensitivity[i][j];
						*tenant_idx = i;
						*region_idx = j;
					}
				}
			}
		}
	}

	return cur_sensitivity;
}


void distribute_local_dram_region(void)
{
	int i, j;
	unsigned long required_dram = 0, tot_free_dram = mttm_local_dram;
	struct mem_cgroup *memcg;
	bool all_region_determined = true, all_fixed = true;
	unsigned long tot_dram_sensitivity = 0;

	unsigned long dram_sensitivity[LIMIT_TENANTS][NR_REGION] = {0,};
	int dram_determined[LIMIT_TENANTS][NR_REGION] = {0,};
	unsigned long dram_size[LIMIT_TENANTS][NR_REGION] = {0,};
	int tenant_idx, region_idx;
	unsigned long cur_sensitivity = 0;
	unsigned long extra_priority[LIMIT_TENANTS] = {0,};
	unsigned long extra_dram[LIMIT_TENANTS] = {0,};
	unsigned long tot_extra_priority = 0;

	// Check that not classified workload exist.
	for(i = 0; i < LIMIT_TENANTS; i++) {
		memcg = READ_ONCE(memcg_list[i]);
		if(memcg) {
			if(!READ_ONCE(memcg->region_determined)) {
				all_region_determined = false;
				tot_free_dram -= memcg->max_nr_dram_pages;
			}
			if(!READ_ONCE(memcg->dram_fixed))
				all_fixed = false;
		}
	}

	if(all_fixed)
		return;

	calculate_dram_sensitivity(dram_sensitivity, dram_determined);
	
	// Distribute local dram to sensitive region first
	tot_dram_sensitivity = get_tot_dram_sensitivity(dram_sensitivity);
	while(1) {
		// Find highest sensitivity region
		cur_sensitivity = 0;
		tenant_idx = 0;
		region_idx = 0;
		
		cur_sensitivity = find_highest_dram_sensitivity(&tenant_idx, &region_idx, dram_sensitivity, dram_determined);
		if(cur_sensitivity == 0)
			break;

		// Determine dram size for selected region
		memcg = READ_ONCE(memcg_list[tenant_idx]);
		dram_size[tenant_idx][region_idx] = calculate_region_dram_size(tenant_idx, region_idx, tot_free_dram, 
								tot_dram_sensitivity, dram_determined, dram_sensitivity);
		dram_determined[tenant_idx][region_idx] = 1;
		tot_free_dram -= dram_size[tenant_idx][region_idx];
		tot_dram_sensitivity -= dram_sensitivity[tenant_idx][region_idx];

		if(all_region_determined)
			pr_info("[%s] [ %s ] %dth region size [demand: %lu MB, set: %lu MB]\n",
				__func__, memcg->tenant_name, region_idx,
				memcg->region_size[region_idx] >> 8, dram_size[tenant_idx][region_idx] >> 8);
	}

	// When extra local DRAM exist
	if(tot_free_dram > 0) {
		for(i = 0; i < LIMIT_TENANTS; i++) {
			bool all_region_done = true;
			unsigned long tenant_dram = 0;
			memcg = READ_ONCE(memcg_list[i]);
			if(memcg) {
				if(READ_ONCE(memcg->region_determined)) {
					for(j = 0; j < NR_REGION; j++) {
						if(dram_determined[i][j] == 0)
							all_region_done = false;
					}
					if(all_region_done) {
						for(j = 0; j < NR_REGION; j++)
							tenant_dram += dram_size[i][j];
						extra_priority[i] = get_anon_rss(memcg) * 100 / tenant_dram;
						// give extra dram to tenant that receives small dram compared to rss.
						tot_extra_priority += extra_priority[i];
					}
				}
			}
		}
		
		for(i = 0; i < LIMIT_TENANTS; i++) {
			bool all_region_done = true;
			unsigned long tenant_dram = 0;
			memcg = READ_ONCE(memcg_list[i]);
			if(memcg) {
				if(READ_ONCE(memcg->region_determined)) {
					for(j = 0; j < NR_REGION; j++) {
						if(dram_determined[i][j] == 0)
							all_region_done = false;
					}
					if(all_region_done) {
						for(j = 0; j < NR_REGION; j++)
							tenant_dram += dram_size[i][j];

						extra_dram[i] = min_t(unsigned long, 
									extra_priority[i] * tot_free_dram / tot_extra_priority, 
									get_anon_rss(memcg) - tenant_dram);
						tot_extra_priority -= extra_priority[i];
						tot_free_dram -= extra_dram[i];
						if(all_region_determined)
							pr_info("[%s] [ %s ] get extra dram %lu MB (priority: %lu)\n",
								__func__, memcg->tenant_name, extra_dram[i] >> 8,
								extra_priority[i]);
					}
				}
			}
		}
		

	}


	// Set dram size
	for(i = 0; i < LIMIT_TENANTS; i++) {
		bool all_region_done = true;
		unsigned long tenant_dram = 0;
		memcg = READ_ONCE(memcg_list[i]);
		if(memcg) {
			if(READ_ONCE(memcg->region_determined)) {
				for(j = 0; j < NR_REGION; j++) {
					if(dram_determined[i][j] == 0)
						all_region_done = false;
				}
				if(all_region_done) {
					tenant_dram = 0;
					for(j = 0; j < NR_REGION; j++)
						tenant_dram += dram_size[i][j];
					tenant_dram += extra_dram[i];
					if(all_region_determined) {
						pr_info("[%s] [ %s ] dram set to %lu MB\n",
							__func__, memcg->tenant_name, tenant_dram >> 8);
					}

					set_dram_size(memcg, tenant_dram, all_region_determined);
				}
			}
		}
	}

	if(all_region_determined)
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
					tot_rate += calculate_valid_rate(memcg->highest_rate);
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
	unsigned long valid_rate = calculate_valid_rate(memcg->highest_rate);
	unsigned long available_dram;
	unsigned long remained_required_dram = 0, expected_extra_dram = 0;
	int i;
	struct mem_cgroup *memcg_iter;

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
	unsigned long valid_rate = calculate_valid_rate(memcg->highest_rate);
	unsigned long available_dram;
	unsigned long remained_required_dram = 0, expected_extra_dram = 0;
	int i;
	struct mem_cgroup *memcg_iter;

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
	bool all_hi_determined = true, all_fixed = true;
	unsigned long cur_highest_rate = 0;
	int dram_determined[LIMIT_TENANTS] = {0,};
	unsigned long dram_size[LIMIT_TENANTS] = {0,};
	unsigned long valid_rate;

	// Check that not classified workload exist.
	for(i = 0; i < LIMIT_TENANTS; i++) {
		memcg = READ_ONCE(memcg_list[i]);
		if(memcg) {
			if(!READ_ONCE(memcg->hi_determined)) {
				all_hi_determined = false;
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
		valid_rate = calculate_valid_rate(memcg->highest_rate);	
		dram_size[idx] = calculate_strong_hot_dram_size(memcg, tot_free_dram,
							tot_strong_hot_rate, dram_determined);
		dram_determined[idx] = 1;
		tot_free_dram -= dram_size[idx];
		tot_strong_hot_rate -= valid_rate;

		if(all_hi_determined)
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
		valid_rate = calculate_valid_rate(memcg->highest_rate);
		dram_size[idx] = calculate_weak_hot_dram_size(memcg, tot_free_dram,
							tot_weak_hot_rate, dram_determined);
		dram_determined[idx] = 1;
		tot_free_dram -= dram_size[idx];
		tot_weak_hot_rate -= valid_rate;

		if(all_hi_determined)
			pr_info("[%s] [ %s ] weak hot. dram set to %lu MB. rate : %lu\n",
				__func__, memcg->tenant_name, dram_size[idx] >> 8, valid_rate);

	}


	for(i = 0; i < LIMIT_TENANTS; i++) {
		memcg = READ_ONCE(memcg_list[i]);
		if(memcg) {
			if(READ_ONCE(memcg->hi_determined) &&
				dram_determined[i] == 1) {
				set_dram_size(memcg, dram_size[i], all_hi_determined);
			}
		}
	}

	if(all_hi_determined)
		pr_info("[%s] remained DRAM : %lu MB\n",
			__func__, tot_free_dram >> 8);
}



static unsigned long find_lowest_access_rate_level(int level, int *idx, int *ranked)
{
	int i;
	struct mem_cgroup *memcg;
	unsigned long cur_lowest_rate = ULONG_MAX;
	unsigned long lb, ub;

	for(i = 0; i < LIMIT_TENANTS; i++) {
		memcg = READ_ONCE(memcg_list[i]);
		if(memcg) {
			if(ranked[i] == 0 &&
				cur_lowest_rate > memcg->mean_rate) {
				lb = 10 * level;
				ub = (level < 9) ? 10 * (level + 1) : 101;	
				if(memcg->fmmr >= lb && memcg->fmmr < ub) {
					cur_lowest_rate = memcg->mean_rate;
					*idx = i;
				}
			}
		}
	}

	return cur_lowest_rate;
}



static void rank_vms(int level, int *ranked, struct mem_cgroup **vms, int *vms_i)
{
	int idx;
	unsigned long cur_lowest_rate;

	while(1) {
		cur_lowest_rate = ULONG_MAX;
		idx = 0;
		
		cur_lowest_rate = find_lowest_access_rate_level(level, &idx, ranked);
		if(cur_lowest_rate == ULONG_MAX)
			break;

		ranked[idx] = 1;
		vms[*vms_i] = memcg_list[idx];
		*vms_i = (*vms_i) + 1;
	}

}



static void distribute_local_dram_mpki(void)
{
	int i, idx, vms_i = 0;
	struct mem_cgroup *memcg;
	int ranked[LIMIT_TENANTS] = {0,};
	unsigned long dram_size[LIMIT_TENANTS] = {0,};
	struct mem_cgroup *vms[LIMIT_TENANTS] = {NULL,};
	int borrower, donor;
	unsigned long toBorrow, toDonate, toMigrate, donated;

	// vms[last] : highest FMMR, higher access rate
	// level n : FMMR is [10*n, 10*(n+1)), n is 0~9

	for(i = 0; i < 10; i++) {
		rank_vms(i, ranked, vms, &vms_i);
	}

	
	donor = 0;
	for(borrower = vms_i - 1; borrower > 0; borrower--) {
		if(vms[borrower]) {
			if(vms[borrower]->fmmr <= acceptor_threshold)
				break;

			toBorrow = vms[borrower]->max_nr_dram_pages / 10;
			while(donor < borrower && toBorrow > 0) {
				donated = (vms[donor]->init_dram_size > vms[donor]->max_nr_dram_pages) ?
						(vms[donor]->init_dram_size - vms[donor]->max_nr_dram_pages) : 0;
				toDonate = (vms[donor]->max_nr_dram_pages / 10 > donated) ? 
						(vms[donor]->max_nr_dram_pages / 10 - donated) : 0;
				toMigrate = min_t(unsigned long, toDonate, toBorrow);
				set_dram_size(vms[donor], vms[donor]->max_nr_dram_pages - toMigrate, false);
				set_dram_size(vms[borrower], vms[borrower]->max_nr_dram_pages + toMigrate, false);
				toBorrow -= toMigrate;
				pr_info("[%s] [ %s ] gives [ %s ] %lu MB dram\n",
					__func__, vms[donor]->tenant_name, vms[borrower]->tenant_name,
					toMigrate >> 8);
				if(toDonate == toMigrate)
					donor++;
			}
		}
	}

	for(i = 0; i < LIMIT_TENANTS; i++) {
		if(vms[i]) {
			pr_info("[%s] vms[%d : %s], dram set to %lu MB, FMMR : %lu, rate : %lu\n",
				__func__, i, vms[i]->tenant_name, vms[i]->max_nr_dram_pages >> 8,
				vms[i]->fmmr, vms[i]->mean_rate);
		}
	}

}



static int ksampled(void *dummy)
{
	unsigned long sleep_timeout = usecs_to_jiffies(200);
	unsigned long total_time, total_cputime = 0, one_cputime, cur;
	unsigned long cur_long, interval_start_long;
	unsigned long interval_start;
	unsigned long trace_period = msecs_to_jiffies(ksampled_trace_period_in_ms);
	unsigned long trace_period_long = msecs_to_jiffies(10000);
	struct mem_cgroup *memcg;
	int i;

	total_time = jiffies;
	interval_start = jiffies;
	interval_start_long = jiffies;
	while(!kthread_should_stop()) {
		one_cputime = jiffies;
		ksampled_do_work();

		cur = jiffies;
		cur_long = jiffies;
		if(use_memstrata_policy) {
			if(cur_long - interval_start_long >= trace_period_long) {
				calculate_sample_rate_stat_mpki();
				distribute_local_dram_mpki();

				interval_start_long = cur_long;	
			}

		}
		else {
			if(cur - interval_start >= trace_period) {
				calculate_sample_rate_stat();
				
				if(use_dram_determination) {
					if(use_region_separation) {
						distribute_local_dram_region();
					}
					else if(use_hotness_intensity) {
						//distribute_local_dram_hi();//hotness_intensity
					}
				}
				
				interval_start = cur;
				/*if(scanless_cooling) {
					for(i = 0; i < LIMIT_TENANTS; i++) {
						int j, k, tot_weight = 0;
						memcg = READ_ONCE(memcg_list[i]);
						if(memcg) {
							pr_info("[%s] [ %s ] giga bitmap util : %u / %u\n",
								__func__, memcg->tenant_name,
								bitmap_weight(memcg->giga_bitmap, memcg->giga_bitmap_size),
								 memcg->giga_bitmap_size);
							for(j = 0; j < memcg->giga_bitmap_size; j++) {
								for(k = 0; k < memcg->huge_bitmap_size; k++) {
									tot_weight += bitmap_weight(memcg->base_bitmap[j][k], memcg->base_bitmap_size);
								}
							}
							pr_info("[%s] [ %s ] tot ac bitmap util : %u / %u\n",
								__func__, memcg->tenant_name, tot_weight,
								memcg->giga_bitmap_size * memcg->huge_bitmap_size * memcg->base_bitmap_size);
						}
					}
				}*/
			}
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
	dma_cap_mask_t copy_mask, memset_mask;

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
				//dma_cap_set(DMA_MEMSET, copy_mask);
				//dma_cap_zero(memset_mask);
				//dma_cap_set(DMA_MEMSET, memset_mask);
				dmaengine_get();
			
				for(i = 0; i < NUM_AVAIL_DMA_CHAN; i++) {
					if(!copy_chan[i])
						copy_chan[i] = dma_request_channel(copy_mask, NULL, NULL);
					if(!copy_chan[i]) {
						pr_err("%s: cannot grap copy channel: %d\n", __func__, i);
						continue;
					}

					copy_dev[i] = copy_chan[i]->device;
					if(!copy_dev[i]) {
						pr_err("%s: no copy device: %d\n", __func__, i);
						continue;
					}
				}

				/*
				for(i = 0; i < NUM_AVAIL_DMA_CHAN/2; i++) {
					if(!memset_chan[i])
						memset_chan[i] = dma_request_channel(memset_mask, NULL, NULL);
					if(!memset_chan[i]) {
						pr_err("%s: cannot grap memset channel: %d\n", __func__, i);
						continue;
					}

					memset_dev[i] = memset_chan[i]->device;
					if(!memset_dev[i]) {
						pr_err("%s: no memset device: %d\n", __func__, i);
						continue;
					}
				}*/


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
			if(memset_chan[i]) {
				dma_release_channel(memset_chan[i]);
				memset_chan[i] = NULL;
				memset_dev[i] = NULL;
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





