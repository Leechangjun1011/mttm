/*
 * kptscand
 */

#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/hugetlb.h>
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
#include <linux/mttm.h>
#include <linux/xarray.h>
#include <linux/rmap.h>
#include <linux/vtmm.h>
#include <asm/tlb.h>

#include "internal.h"

int enable_kptscand = 0;
extern int current_tenants;
DEFINE_SPINLOCK(vtmm_register_lock);
struct task_struct *kptscand_thread = NULL;

#define NUM_AVAIL_DMA_CHAN	16
#define DMA_CHAN_PER_PAGE	2
extern unsigned int use_dma_migration;
extern struct dma_chan *copy_chan[NUM_AVAIL_DMA_CHAN];
extern struct dma_device *copy_dev[NUM_AVAIL_DMA_CHAN];

extern struct mem_cgroup **memcg_list;
extern unsigned int use_dram_determination;
extern unsigned long mttm_local_dram;

struct vtmm_page *get_vtmm_page(struct mem_cgroup *memcg, struct page *page)
{
	int i;
	struct vtmm_page *vp = NULL;
	for(i = 0; i < ML_QUEUE_MAX; i++) {
		vp = (struct vtmm_page *)xa_load(memcg->ml_queue[i], page_to_pfn(page));
		if(vp) {
			break;
		}
	}
	
	return vp;
}

/*struct vtmm_page *erase_vtmm_page(struct mem_cgroup *memcg, struct page *page, int *lev)
{
	int i;
	struct vtmm_page *vp = NULL;
	for(i = 0; i < ML_QUEUE_MAX; i++) {
		vp = (struct vtmm_page *)xa_erase(memcg->ml_queue[i], page_to_pfn(page));
		if(vp)
			break;
	}

	if(lev)
		*lev = i;

	return vp;
}*/

unsigned int page_degree_idx(struct vtmm_page *vp)
{
	unsigned int page_degree = 2 * bitmap_weight(&vp->read_count, BITMAP_MAX) +
					3 * bitmap_weight(&vp->write_count, BITMAP_MAX);
	return min_t(unsigned int, page_degree / 5, BUCKET_MAX - 1);
}

void move_vtmm_page_bucket(struct mem_cgroup *memcg, struct vtmm_page *vp,
				unsigned int from, unsigned int to)
{
	spin_lock(memcg->bucket_lock[from]);
	list_del(&vp->list);
	spin_unlock(memcg->bucket_lock[from]);

	spin_lock(memcg->bucket_lock[to]);
	list_add_tail(&vp->list, memcg->page_bucket[to]);
	spin_unlock(memcg->bucket_lock[to]);
}

void cmpxchg_vtmm_page(struct mem_cgroup *memcg, struct page *old_page, struct page *new_page)
{
	int i;
	struct vtmm_page *old_vp = NULL;
	if(!PageAnon(old_page) || !PageAnon(new_page))
		return;

	for(i = 0; i < ML_QUEUE_MAX; i++) {
		old_vp = (struct vtmm_page *)xa_load(memcg->ml_queue[i], page_to_pfn(old_page));
		if(old_vp) {
			unsigned int old_lev;
			old_lev = page_degree_idx(old_vp);
			xa_store(memcg->ml_queue[i], old_vp->addr, NULL, GFP_KERNEL);

			spin_lock(memcg->bucket_lock[old_lev]);
			list_del(&old_vp->list);
			spin_unlock(memcg->bucket_lock[old_lev]);
			break;
		}
	}

	if(old_vp) {
		void *xa_store_ret;
		
		old_vp->addr = page_to_pfn(new_page);
		xa_store_ret = xa_store(memcg->ml_queue[i], old_vp->addr, (void *)old_vp, GFP_KERNEL);
		if(xa_err(xa_store_ret)) {
			kmem_cache_free(vtmm_page_cache, old_vp);
			return;
		}
		else {
			unsigned int lev = page_degree_idx(old_vp);
			
			spin_lock(memcg->bucket_lock[lev]);
			list_add_tail(&old_vp->list, memcg->page_bucket[lev]);
			spin_unlock(memcg->bucket_lock[lev]);
			return;
		}	
	}

}


void copy_transhuge_vtmm_page(struct page *page, struct page *newpage)
{
	struct mem_cgroup *memcg = page_memcg(page);
	struct vtmm_page *vp = NULL;

	VM_BUG_ON_PAGE(!PageCompound(page), page);
	VM_BUG_ON_PAGE(!PageCompound(newpage), newpage);

	page = compound_head(page);
	newpage = compound_head(newpage);

	if(!memcg->vtmm_enabled)
		return;

	vp = get_vtmm_page(memcg, page);

	if(!vp) {
		pr_err("[%s] null vp\n",__func__);
		return;
	}

	vp->addr = page_to_pfn(newpage);
	
	return;
}


void __prep_transhuge_page_for_vtmm(struct mem_cgroup *memcg, struct page *page)
{
	unsigned long index = page_to_pfn(page);
	struct vtmm_page *vp = kmem_cache_alloc(vtmm_page_cache, GFP_KERNEL);
	if(vp) {
		void *xa_ret = NULL;

		/*
		vp->read_count = 0;
		vp->write_count = 0;
		*/
		bitmap_zero(&vp->read_count, BITMAP_MAX);
		bitmap_zero(&vp->write_count, BITMAP_MAX);
		vp->remained_dnd_time = 0;
		vp->ml_queue_lev = 0;
		vp->is_thp = true;
		vp->addr = index;

		xa_ret = xa_store(memcg->ml_queue[0], index, (void *)vp, GFP_KERNEL);	
		if(xa_err(xa_ret))
			kmem_cache_free(vtmm_page_cache, vp);
		else {
			spin_lock(memcg->bucket_lock[0]);
			list_add_tail(&vp->list, memcg->page_bucket[0]);
			spin_unlock(memcg->bucket_lock[0]);
		}
	}

}


void prep_transhuge_page_for_vtmm(struct vm_area_struct *vma, struct page *page)
{
	struct mem_cgroup *memcg = get_mem_cgroup_from_mm(vma->vm_mm);

	prep_transhuge_page(page);
	if(memcg) {
		if(memcg->vtmm_enabled)	
			__prep_transhuge_page_for_vtmm(memcg, page);
	}
}


void uncharge_vtmm_transhuge_page(struct page *page, struct mem_cgroup *memcg)
{
	struct vtmm_page *vp = NULL;
	int i;
	if(!memcg)
		return;
	if(!memcg->vtmm_enabled)
		return;

	for(i = 0; i < ML_QUEUE_MAX; i++) {
		vp = (struct vtmm_page *)xa_load(memcg->ml_queue[i], page_to_pfn(page));		
		if(vp) {
			unsigned int lev = page_degree_idx(vp);
			xa_store(memcg->ml_queue[i], vp->addr, NULL, GFP_KERNEL);

			spin_lock(memcg->bucket_lock[lev]);
			list_del(&vp->list);
			spin_unlock(memcg->bucket_lock[lev]);
			kmem_cache_free(vtmm_page_cache, vp);
			break;
		}
	}
}


void uncharge_vtmm_page(struct page *page, struct mem_cgroup *memcg)
{
	struct vtmm_page *vp = NULL;
	int i;
	if(!memcg)
		return;
	if(!memcg->vtmm_enabled)
		return;

	for(i = 0; i < ML_QUEUE_MAX; i++) {
		vp = (struct vtmm_page *)xa_load(memcg->ml_queue[i], page_to_pfn(page));		
		if(vp) {
			unsigned int lev = page_degree_idx(vp);
			xa_store(memcg->ml_queue[i], vp->addr, NULL, GFP_KERNEL);

			spin_lock(memcg->bucket_lock[lev]);
			list_del(&vp->list);
			spin_unlock(memcg->bucket_lock[lev]);
			kmem_cache_free(vtmm_page_cache, vp);
			break;
		}
	}

}

SYSCALL_DEFINE1(vtmm_register_pid,
                const char __user *, u_name)
{
        int i; 
        struct mem_cgroup *memcg = mem_cgroup_from_task(current);
        char name[PATH_MAX];
        spin_lock(&vtmm_register_lock);

        if(current_tenants == LIMIT_TENANTS) {
                pr_info("[%s] Can't register tenant due to limit\n",__func__);
                spin_unlock(&vtmm_register_lock);
                return 0;
        } 

	for(i = 0; i < ML_QUEUE_MAX; i++) {
		memcg->ml_queue[i] = kmalloc(sizeof(struct xarray), GFP_KERNEL);
		xa_init(memcg->ml_queue[i]);
	}	
 	for(i = 0; i < BUCKET_MAX; i++) {
		memcg->page_bucket[i] = kmalloc(sizeof(struct list_head), GFP_KERNEL);
		INIT_LIST_HEAD(memcg->page_bucket[i]);
		memcg->bucket_lock[i] = kmalloc(sizeof(spinlock_t), GFP_KERNEL);
		spin_lock_init(memcg->bucket_lock[i]);
	}

	memcg->vtmm_mm = current->mm;
	memcg->vtmm_enabled = true;

        if(use_dma_migration) {
                memcg->dma_chan_start = current_tenants * DMA_CHAN_PER_PAGE;
                if(memcg->dma_chan_start + DMA_CHAN_PER_PAGE > NUM_AVAIL_DMA_CHAN)
                        memcg->dma_chan_start = 0;
        }

        WRITE_ONCE(memcg_list[current_tenants], memcg);
        current_tenants++;

        copy_from_user(name, u_name, strnlen_user(u_name, PATH_MAX));
        strlcpy(memcg->tenant_name, name, PATH_MAX);

        for(i = 0; i < LIMIT_TENANTS; i++) {
                if(memcg_list[i]) {
                        if(memcg_list[i]->vtmm_enabled && use_dram_determination) {
                                WRITE_ONCE(memcg_list[i]->nodeinfo[0]->max_nr_base_pages, mttm_local_dram / current_tenants);
                                WRITE_ONCE(memcg_list[i]->max_nr_dram_pages, mttm_local_dram / current_tenants);
                                pr_info("[%s] [ %s ] dram size set to %lu MB\n",
                                        __func__, memcg_list[i]->tenant_name, memcg_list[i]->max_nr_dram_pages >> 8);
                        }
                }
        }

	if(vtmm_kmigrated_init(memcg))
		pr_info("[%s] failed to start vtmm_kmigrated\n",__func__);

        pr_info("[%s] name : [ %s ], current_tenants : %d, dma_chan_start : %u, local_dram : %lu MB\n",
                __func__, memcg->tenant_name, current_tenants, memcg->dma_chan_start, (mttm_local_dram / current_tenants) >> 8);

        spin_unlock(&vtmm_register_lock);

        return 0;
}


SYSCALL_DEFINE1(vtmm_unregister_pid,
                pid_t, pid)
{
        int i;
        struct mem_cgroup *memcg = mem_cgroup_from_task(current);

        spin_lock(&vtmm_register_lock);

        current_tenants--;
        vtmm_kmigrated_stop(memcg); 

	memcg->vtmm_enabled = false;

	for(i = 0; i < LIMIT_TENANTS; i++) {
                if(READ_ONCE(memcg_list[i]) == memcg) {
                        WRITE_ONCE(memcg_list[i], NULL);
                        break;
                }
        }

	for(i = 0; i < ML_QUEUE_MAX; i++) {
		xa_destroy(memcg->ml_queue[i]);
		kfree(memcg->ml_queue[i]);
	}

	for(i = 0; i < BUCKET_MAX; i++) {
		kfree(memcg->page_bucket[i]);
		kfree(memcg->bucket_lock[i]);
	}

        // Re-distribute local DRAM
        for(i = 0; i < LIMIT_TENANTS; i++) {
                if(READ_ONCE(memcg_list[i])) {
                        WRITE_ONCE(memcg_list[i]->dram_fixed, false);
                }
        }

        spin_unlock(&vtmm_register_lock);

        pr_info("[%s] unregistered pid : %d, name : [ %s ], current_tenants : %d\n",
                __func__, pid, memcg->tenant_name, current_tenants);
        return 0;
}

static void promote_ml_queue(struct mem_cgroup *memcg, struct vtmm_page *vp)
{
	void *xa_ret;
	if(vp->ml_queue_lev < ML_QUEUE_MAX - 1) {
		xa_ret = xa_erase(memcg->ml_queue[vp->ml_queue_lev], vp->addr);

		if(xa_ret && !xa_is_err(xa_ret)) {
			vp->ml_queue_lev++;
			vp->remained_dnd_time = (1U << (vp->ml_queue_lev - 1));
			vp->skip_scan = true;
			xa_store(memcg->ml_queue[vp->ml_queue_lev], vp->addr,
				(void *)vp, GFP_KERNEL);
		}
	}
	else {
		vp->remained_dnd_time = (1U << (vp->ml_queue_lev - 1));
	}
}


static void demote_ml_queue(struct mem_cgroup *memcg, struct vtmm_page *vp)
{
	void *xa_ret;
	if(vp->ml_queue_lev > 0) {
		xa_ret = xa_erase(memcg->ml_queue[vp->ml_queue_lev], vp->addr);

		if(xa_ret && !xa_is_err(xa_ret)) {
			vp->ml_queue_lev--;	
			if(vp->ml_queue_lev > 0)
				vp->remained_dnd_time = (1U << (vp->ml_queue_lev - 1));
			else
				vp->remained_dnd_time = 0;

			xa_store(memcg->ml_queue[vp->ml_queue_lev], vp->addr,
				(void *)vp, GFP_KERNEL);
		}
	}
	else {
		vp->remained_dnd_time = 0;
	}
}


static void scan_ad_bit(unsigned long pfn, struct vtmm_page *vp, unsigned int lev,
			struct mem_cgroup *memcg,
			struct list_head *promotion_list, struct list_head *demotion_list)
{
	struct page *page;
	spinlock_t *ptl;
	pmd_t *pmd = NULL;
	pte_t *pte = NULL;
	int accessed = 0, dirty = 0;
	struct vm_area_struct *vma = NULL;
	unsigned long va = 0;
	unsigned long prev_degree_idx = page_degree_idx(vp);
	unsigned long cur_degree_idx;
	
	if(vp->skip_scan) {
		vp->skip_scan = false;
		return;
	}
	if(vp->remained_dnd_time > 0) {
		vp->remained_dnd_time--;
	}

	// Get pte for basepage, pmd for hugepage
	page = pfn_to_page(pfn);
	if(vp->is_thp) {
		pmd = get_pmd_from_vtmm_page(page, &vma, &va);
		if(pmd) {
			ptl = pmd_lock(memcg->vtmm_mm, pmd);
			if(pmd_present(*pmd)) {
				accessed = pmd_young(*pmd);
				dirty = pmd_dirty(*pmd);
				bitmap_shift_left(&vp->read_count, &vp->read_count,
							1, BITMAP_MAX);
				bitmap_shift_left(&vp->write_count, &vp->write_count,
							1, BITMAP_MAX);
				if(accessed || dirty) {//accessed
					if(accessed) {
						if(vp->remained_dnd_time == 0)
							pmd_mkold(*pmd);
						//vp->read_count++;
						set_bit(0, &vp->read_count);
					}
					if(dirty) {
						if(vp->remained_dnd_time == 0)
							pmd_mkclean(*pmd);
						//vp->write_count++;
						set_bit(0, &vp->write_count);
					}
					if(vp->remained_dnd_time == 0) {
						flush_cache_range(vma, va, va + HPAGE_SIZE);
						flush_tlb_range(vma, va, va + HPAGE_SIZE);
						promote_ml_queue(memcg, vp);
					}
				}
				else {//not accessed
					if(vp->remained_dnd_time == 0)
						demote_ml_queue(memcg, vp);
				}
			}
			spin_unlock(ptl);
		}
	}
	else {
		pte = get_pte_from_vtmm_page(page, &vma, &va, &pmd);
		if(pte) {
			ptl = pte_lockptr(memcg->vtmm_mm, pmd);
			spin_lock(ptl);
			if(pte_present(*pte)) {
				accessed = pte_young(*pte);
				dirty = pte_dirty(*pte);
				bitmap_shift_left(&vp->read_count, &vp->read_count,
							1, BITMAP_MAX);
				bitmap_shift_left(&vp->write_count, &vp->write_count,
							1, BITMAP_MAX);
				if(accessed || dirty) {
					if(accessed) {
						if(vp->remained_dnd_time == 0)
							pte_mkold(*pte);
						//vp->read_count++;
						set_bit(0, &vp->read_count);
					}
					if(dirty) {
						if(vp->remained_dnd_time == 0)
							pte_mkclean(*pte);
						//vp->write_count++;
						set_bit(0, &vp->write_count);
					}
					if(vp->remained_dnd_time == 0) {
						flush_cache_range(vma, va, va + PAGE_SIZE);
						flush_tlb_range(vma, va, va + PAGE_SIZE);
						promote_ml_queue(memcg, vp);
					}
				}
				else {//not accessed
					if(vp->remained_dnd_time == 0)
						demote_ml_queue(memcg, vp);
				}
			}
			spin_unlock(ptl);
		}
	}

	cur_degree_idx = page_degree_idx(vp);
	if(prev_degree_idx != cur_degree_idx) {
		move_vtmm_page_bucket(memcg, vp, prev_degree_idx, cur_degree_idx);
	}
	if(cur_degree_idx >= memcg->active_threshold)
		move_page_to_active_lru(page);
	else if(PageActive(page))
		move_page_to_inactive_lru(page);

}

static void scan_ml_queue(struct mem_cgroup *memcg)
{
	int i;
	unsigned long pfn;
	struct vtmm_page *vp, *tmp;
	LIST_HEAD(promotion_list);//move pages to higher queue
	LIST_HEAD(demotion_list);//to lower queue

	for(i = 0; i < ML_QUEUE_MAX; i++) {
		xa_for_each(memcg->ml_queue[i], pfn, vp) {
			scan_ad_bit(pfn, vp, (unsigned int)i, memcg, &promotion_list, &demotion_list);	
		}
	}
	
}

static unsigned long get_nr_bucket_pages(struct list_head *page_bucket)
{
	unsigned long nr_pages = 0;
	struct vtmm_page *vp;

	list_for_each_entry(vp, page_bucket, list) {
		if(vp->is_thp)
			nr_pages += HPAGE_PMD_NR;
		else
			nr_pages ++;
	}

	return nr_pages;
}


static void determine_active_threshold(struct mem_cgroup *memcg)
{
	unsigned long nr_active = 0;
        unsigned long max_nr_pages = memcg->max_nr_dram_pages -
                get_memcg_promotion_wmark(memcg->max_nr_dram_pages);
        int idx_hot;

        if(READ_ONCE(memcg->hg_mismatch)) {
                set_lru_adjusting(memcg, true);
                return;
        }


        for(idx_hot = BUCKET_MAX - 1; idx_hot >= 0; idx_hot--) {
                unsigned long nr_pages = get_nr_bucket_pages(memcg->page_bucket[idx_hot]);
                if(nr_active + nr_pages > max_nr_pages)
                        break;
                nr_active += nr_pages;
        }
        if(idx_hot != BUCKET_MAX - 1)
                idx_hot++;


	if(idx_hot < MTTM_INIT_THRESHOLD) {
                idx_hot = MTTM_INIT_THRESHOLD;
        }

	WRITE_ONCE(memcg->active_threshold, idx_hot); 
        set_lru_adjusting(memcg, true);

}


static void kptscand_do_work(void)
{
	int i;
	struct mem_cgroup *memcg;

	for(i = 0; i < LIMIT_TENANTS; i++) {
		memcg = READ_ONCE(memcg_list[i]);
		if(memcg) {
			scan_ml_queue(memcg);
			determine_active_threshold(memcg);
		}
	}

}

static int kptscand(void *dummy)
{
	unsigned long sleep_timeout = msecs_to_jiffies(600);
	unsigned long total_time, total_cputime = 0, one_cputime;
	unsigned long cur, trace_period = msecs_to_jiffies(10000);
	struct mem_cgroup *memcg;
	int i;

	total_time = jiffies;
	cur = jiffies;
	while(!kthread_should_stop()) {
		one_cputime = jiffies;

		kptscand_do_work();

		if(jiffies - cur >= trace_period) {
			for(i = 0; i < LIMIT_TENANTS; i++) {
				memcg = READ_ONCE(memcg_list[i]);
				if(memcg) {
					unsigned long nr_xa_pages = 0, nr_xa_basepages = 0, nr_xa_tot = 0;
					unsigned long index;
					struct vtmm_page *vp;
					unsigned long nr_list_pages = 0, nr_list_basepages = 0;
					unsigned long nr_hot_pages = 0, nr_cold_pages = 0;
					int j;

					for(j = 0; j < ML_QUEUE_MAX; j++) {
						nr_xa_basepages = 0;
						nr_xa_pages = 0;
						xa_for_each(memcg->ml_queue[j], index, vp) {
							if(vp->is_thp)
								nr_xa_basepages += HPAGE_PMD_NR;
							else
								nr_xa_basepages++;
							nr_xa_pages++;
						}
						nr_xa_tot += nr_xa_basepages;
						if(nr_xa_basepages >> 8)
							pr_info("[%s] [ %s ] ML QUEUE %d. pages : %lu MB\n",
								__func__, memcg->tenant_name, j, nr_xa_basepages >> 8);
					}
					nr_hot_pages = lruvec_lru_size(mem_cgroup_lruvec(memcg, NODE_DATA(0)),
								LRU_ACTIVE_ANON, MAX_NR_ZONES) +
							lruvec_lru_size(mem_cgroup_lruvec(memcg, NODE_DATA(1)),
								LRU_ACTIVE_ANON, MAX_NR_ZONES);
					nr_cold_pages = lruvec_lru_size(mem_cgroup_lruvec(memcg, NODE_DATA(0)),
								LRU_INACTIVE_ANON, MAX_NR_ZONES) +
							lruvec_lru_size(mem_cgroup_lruvec(memcg, NODE_DATA(1)),
								LRU_INACTIVE_ANON, MAX_NR_ZONES);

					pr_info("[%s] [ %s ] tot pages : %lu MB (hot : %lu MB, cold : %lu MB, threshold : %u). max_nr_dram_pages : %lu MB\n",
						__func__, memcg->tenant_name, nr_xa_tot >> 8,
						nr_hot_pages >> 8, nr_cold_pages >> 8, memcg->active_threshold,
						memcg->max_nr_dram_pages >> 8);

					for(j = 0; j < BUCKET_MAX; j++) {
						nr_list_basepages = 0;
						nr_list_pages = 0;
						list_for_each_entry(vp, memcg->page_bucket[j], list) {
							if(vp->is_thp)
								nr_list_basepages += HPAGE_PMD_NR;
							else
								nr_list_basepages++;
							nr_list_pages++;
						}
						if(nr_list_basepages >> 8)
							pr_info("[%s] [ %s ] bucket %d. pages : %lu MB\n",
								__func__, memcg->tenant_name, j, nr_list_basepages >> 8);
					}
				}
			}

			cur = jiffies;
		}

		total_cputime += (jiffies - one_cputime);
		schedule_timeout_interruptible(sleep_timeout);
	}

	total_time = jiffies - total_time;
	pr_info("[%s] total_time : %lu, total_cputime : %lu\n",
		__func__, total_time, total_cputime);
	return 0;
}


static int kptscand_run(void)
{
	int ret = 0, i;
	dma_cap_mask_t copy_mask;

	if(!kptscand_thread) {
		if(!memcg_list)
			memcg_list = kzalloc(sizeof(struct mem_cgroup *) * LIMIT_TENANTS, GFP_KERNEL);

		kptscand_thread = kthread_run_on_cpu(kptscand, NULL, KPTSCAND_CPU, "kptscand");
		if(IS_ERR(kptscand_thread)) {
			pr_err("Failed to start kptscand\n");
			ret = PTR_ERR(kptscand_thread);
			kptscand_thread = NULL;
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
                        pr_info("[%s] kptscand start\n",__func__);
		}
	}

	return ret;
}


static void kptscand_stop(void)
{
	int i;
	if(kptscand_thread) {
		kthread_stop(kptscand_thread);
		kptscand_thread = NULL;
	}

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

	pr_info("[%s] kptscand stop\n",__func__);
}




#ifdef CONFIG_PROC_SYSCTL
int sysctl_enable_kptscand(struct ctl_table *table, int write,
                        void *buffer, size_t *lenp, loff_t *ppos)
{
        int err = 0;

        if (write && !capable(CAP_SYS_ADMIN))
                return -EPERM;

        err = proc_dointvec_minmax(table, write, buffer, lenp, ppos);

        if (err < 0)
                return err;
        if (write) {
                if(!kptscand_thread &&
                        enable_kptscand == 1 &&
                        current_tenants == 0) {
                        kptscand_run();
                }
                else if(kptscand_thread &&
                        enable_kptscand == 0) {
                        kptscand_stop();
                }
        }
        return err;
}

#endif
