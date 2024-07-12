/*
 * kmigrated for vtmm
 */
#include <linux/kthread.h>
#include <linux/list.h>
#include <linux/memcontrol.h>
#include <linux/mempolicy.h>
#include <linux/mmzone.h>
#include <linux/mm_inline.h>
#include <linux/migrate.h>
#include <linux/swap.h>
#include <linux/rmap.h>
#include <linux/delay.h>
#include <linux/node.h>
#include <linux/mttm.h>
#include <linux/vtmm.h>
#include <linux/wait.h>
#include <linux/sched.h>

#include <trace/events/mttm.h>

#include "internal.h"



#ifdef ARCH_HAS_PREFETCHW
#define prefetchw_prev_lru_page(_page, _base, _field)                   \
        do {                                                            \
                if((_page)->lru.prev != _base) {                        \
                        struct page *prev;                              \
                        prev = lru_to_page(&(_page->lru));              \
                        prefetchw(&prev->_field);                       \
                }                                                       \
        } while (0)
#else
#define prefetchw_prev_lru_page(_page, _base, _field) do { } while (0)
#endif

static __always_inline void update_lru_sizes(struct lruvec *lruvec,
                enum lru_list lru, unsigned long *nr_zone_taken)
{
        int zid;
        for(zid = 0; zid < MAX_NR_ZONES; zid++) {
                if(!nr_zone_taken[zid])
                        continue;
                update_lru_size(lruvec, lru, zid, -nr_zone_taken[zid]);
        }
}

static unsigned long isolate_lru_pages(unsigned long nr_to_scan,
        struct lruvec *lruvec, enum lru_list lru, struct list_head *dst,
        isolate_mode_t mode)
{
        struct list_head *src = &lruvec->lists[lru];
        unsigned long nr_zone_taken[MAX_NR_ZONES] = { 0 };
        unsigned long scan = 0, nr_taken = 0;
        LIST_HEAD(busy_list);

        while(scan < nr_to_scan && !list_empty(src)) {
                struct page *page;
                unsigned long nr_pages;

                page = lru_to_page(src);
                prefetchw_prev_lru_page(page, src, flags);
                VM_WARN_ON(!PageLRU(page));

                nr_pages = compound_nr(page);
                scan += nr_pages;

                //__isolate_lru_page_prepare deprecated
                if(!PageLRU(page)) {
                        list_move(&page->lru, src);
                        continue;
                }
                if(unlikely(!get_page_unless_zero(page))) {
                        list_move(&page->lru, src);
                        continue;
                }
                if(!TestClearPageLRU(page)) {
                        put_page(page);
                        list_move(&page->lru, src);
                        continue;
                }

                nr_taken += nr_pages;
                nr_zone_taken[page_zonenum(page)] += nr_pages;
                list_move(&page->lru, dst);
        }

        update_lru_sizes(lruvec, lru, nr_zone_taken);
        return nr_taken;
}


static bool need_fmem_demotion(pg_data_t *pgdat, struct mem_cgroup *memcg,
				unsigned long *nr_exceeded)
{
	unsigned long nr_lru_pages, max_nr_pages;
        unsigned long nr_need_promoted;
        unsigned long fmem_max_wmark, fmem_min_wmark;
        int target_nid = 1;
        pg_data_t *target_pgdat = NODE_DATA(target_nid);

        max_nr_pages = READ_ONCE(memcg->nodeinfo[pgdat->node_id]->max_nr_base_pages);
        nr_lru_pages = get_nr_lru_pages_node(memcg, pgdat);

        fmem_max_wmark = get_memcg_promotion_wmark(max_nr_pages); // if free mem is larger than this wmark, promotion allowed.
        fmem_min_wmark = get_memcg_demotion_wmark(max_nr_pages); // if free mem is less than this wmark, demotion required.

        if(need_direct_demotion(pgdat, memcg)) { // set at mempolicy.c and dram determination
                if(nr_lru_pages + fmem_max_wmark <= max_nr_pages)
                        goto check_nr_need_promoted;
                else if(nr_lru_pages < max_nr_pages)
                        *nr_exceeded = fmem_max_wmark - (max_nr_pages - nr_lru_pages);
                else
                        *nr_exceeded = nr_lru_pages + fmem_max_wmark - max_nr_pages;
                *nr_exceeded += 1U * 64 * 100;// 50MB
                return true;
        }

check_nr_need_promoted:
        nr_need_promoted = nr_promotion_target(target_pgdat, memcg);
        if(nr_need_promoted) {
                if(nr_lru_pages + nr_need_promoted + fmem_max_wmark <= max_nr_pages)
                        return false;
        }
        else {
                if(nr_lru_pages + fmem_min_wmark <= max_nr_pages)
                        return false;
        }

        *nr_exceeded = nr_lru_pages + nr_need_promoted + fmem_max_wmark - max_nr_pages;
        return true;
}

static struct page *alloc_migrate_page(struct page *page, unsigned long node)
{
        int nid = (int)node;
        int zidx;
        struct page *newpage = NULL;
        gfp_t mask = (GFP_HIGHUSER_MOVABLE |
                        __GFP_THISNODE | __GFP_NOMEMALLOC |
                        __GFP_NORETRY | __GFP_NOWARN) & ~__GFP_RECLAIM;

        if(PageHuge(page))
                return NULL;

        zidx = zone_idx(page_zone(page));
        if(is_highmem_idx(zidx) || zidx == ZONE_MOVABLE)
                mask |= __GFP_HIGHMEM;

        if(thp_migration_supported() && PageTransHuge(page)) {
                mask |= GFP_TRANSHUGE_LIGHT;
                newpage = __alloc_pages_node(nid, mask, HPAGE_PMD_ORDER);
                if(!newpage)
                        return NULL;

                prep_transhuge_page(newpage);
                //__prep_transhuge_page_for_vtmm(page_memcg(newpage), newpage);
        }
        else {
                newpage = __alloc_pages_node(nid, mask, 0);
        }

        return newpage;
}


static unsigned long migrate_page_list(struct list_head *migrate_list, pg_data_t *pgdat,
                                        bool promotion)
{
        int target_nid = promotion ? 0 : 1;
        unsigned int nr_succeeded = 0;

        if(list_empty(migrate_list)) {
                return 0;
        }

        migrate_pages(migrate_list, alloc_migrate_page, NULL, target_nid,
                        MIGRATE_ASYNC, MR_NUMA_MISPLACED, &nr_succeeded);

        return nr_succeeded;
}



static unsigned long shrink_page_list(struct list_head *page_list, pg_data_t *pgdat,
                                struct mem_cgroup *memcg, bool shrink_active,
                                unsigned long nr_to_reclaim)
{
        LIST_HEAD(demote_pages);
        LIST_HEAD(ret_pages);
        unsigned long nr_reclaimed = 0;
        unsigned long nr_demotion_cand = 0;
        unsigned long nr_taken = 0;

        cond_resched();

        while(!list_empty(page_list)) {
                struct page *page;

                page = lru_to_page(page_list);
                list_del(&page->lru);
                nr_taken += compound_nr(page);

                if(!trylock_page(page))
                        goto keep;
                if(!shrink_active && PageAnon(page) && PageActive(page))
                        goto keep_locked;
                if(unlikely(!page_evictable(page)))
                        goto keep_locked;
                if(PageWriteback(page))
                        goto keep_locked;
                if(PageTransHuge(page) && !thp_migration_supported())
                        goto keep_locked;
                if(!PageAnon(page) && nr_demotion_cand > nr_to_reclaim + MTTM_MIN_FREE_PAGES)
                        goto keep_locked;

                if(PageAnon(page)) {
                        if(PageTransHuge(page)) {
				struct vtmm_page *vp = NULL;
			
				vp = get_vtmm_page(memcg, page);
				if(!vp)
					goto keep_locked;
				BUG_ON(!vp->is_thp);
                        }
			else {
				struct vtmm_page *vp = NULL;
			
				vp = get_vtmm_page(memcg, page);
				if(!vp)
					goto keep_locked;
				BUG_ON(vp->is_thp);
                        }
                }

                unlock_page(page);
                list_add(&page->lru, &demote_pages);
                nr_demotion_cand += compound_nr(page);
                continue;
keep_locked:
                unlock_page(page);
keep:
                list_add(&page->lru, &ret_pages);
        }

        nr_reclaimed = migrate_page_list(&demote_pages, pgdat, false);
        if(!list_empty(&demote_pages))
                list_splice(&demote_pages, page_list);
        list_splice(&ret_pages, page_list);

        return nr_reclaimed;
}


static unsigned long demote_inactive_list(unsigned long nr_to_scan, unsigned long nr_to_reclaim,
                                        struct lruvec *lruvec, enum lru_list lru, bool shrink_active)
{
        LIST_HEAD(page_list);
        pg_data_t *pgdat = lruvec_pgdat(lruvec);
        unsigned long nr_reclaimed = 0, nr_taken;
        int file = is_file_lru(lru);

        lru_add_drain();

        spin_lock_irq(&lruvec->lru_lock);
        nr_taken = isolate_lru_pages(nr_to_scan, lruvec, lru, &page_list, 0);
        __mod_node_page_state(pgdat, NR_ISOLATED_ANON + file, nr_taken);
        spin_unlock_irq(&lruvec->lru_lock);

        if(nr_taken == 0) {
                return 0;
        }

        nr_reclaimed = shrink_page_list(&page_list, pgdat, lruvec_memcg(lruvec),
                                	shrink_active, nr_to_reclaim);

        spin_lock_irq(&lruvec->lru_lock);
        move_pages_to_lru(lruvec, &page_list);
        __mod_node_page_state(pgdat, NR_ISOLATED_ANON + file, -nr_taken);
        spin_unlock_irq(&lruvec->lru_lock);

        mem_cgroup_uncharge_list(&page_list);
        free_unref_page_list(&page_list);

        return nr_reclaimed;
}


static unsigned long demote_lruvec(unsigned long nr_to_reclaim, short priority,
                                pg_data_t *pgdat, struct lruvec *lruvec, bool shrink_active)
{
        enum lru_list lru, tmp;
        unsigned long nr_reclaimed = 0;
        long nr_to_scan;

        for_each_evictable_lru(tmp) {
                lru = (tmp + 2) % 4;// scan file lru first

                if(!shrink_active && !is_file_lru(lru) && is_active_lru(lru))
                        continue;

                if(is_file_lru(lru))
                        nr_to_scan = lruvec_lru_size(lruvec, lru, MAX_NR_ZONES);
                else {
                        nr_to_scan = lruvec_lru_size(lruvec, lru, MAX_NR_ZONES) >> priority;
                }

                if(!nr_to_scan)
                        continue;

                while(nr_to_scan > 0) {
                        unsigned long scan = min((unsigned long)nr_to_scan, SWAP_CLUSTER_MAX); 
                        nr_reclaimed += demote_inactive_list(scan, scan, lruvec, lru, shrink_active);
                        nr_to_scan -= (long)scan;
                        if(nr_reclaimed >= nr_to_reclaim)
                                break;
                }

                if(nr_reclaimed >= nr_to_reclaim)
                        break;
        }

        return nr_reclaimed;
}




static unsigned long demote_node(pg_data_t *pgdat, struct mem_cgroup *memcg,
				unsigned long nr_exceeded)
{
	struct lruvec *lruvec = mem_cgroup_lruvec(memcg, pgdat);
        short priority = DEF_PRIORITY;
        unsigned long nr_to_reclaim = 0, nr_evictable_pages = 0, nr_reclaimed = 0;
        enum lru_list lru;
        bool shrink_active = false;
        int target_nid = 1;
        unsigned long max_dram = READ_ONCE(memcg->nodeinfo[pgdat->node_id]->max_nr_base_pages);
       
        for_each_evictable_lru(lru) {
                if(!is_file_lru(lru) && is_active_lru(lru))
                        continue;
                nr_evictable_pages += lruvec_lru_size(lruvec, lru, MAX_NR_ZONES);
        }

        nr_to_reclaim = nr_exceeded;
        //if(nr_exceeded > nr_evictable_pages && need_direct_demotion(pgdat, memcg))
        //      shrink_active = true;

        do {    //nr_reclaimed starts with small number and 
                //increases exponentially by decreasing priority
                nr_reclaimed += demote_lruvec(nr_to_reclaim - nr_reclaimed, priority,
                                                pgdat, lruvec, shrink_active);
                if(nr_reclaimed >= nr_to_reclaim)
                        break;
                priority--;
        } while (priority);

        if(get_nr_lru_pages_node(memcg, pgdat) +
                get_memcg_demotion_wmark(max_dram) < max_dram)
                WRITE_ONCE(memcg->nodeinfo[pgdat->node_id]->need_demotion, false);

        return nr_reclaimed;
}



static void vtmm_kmigrated_do_work(struct mem_cgroup *memcg)
{
	struct mem_cgroup_per_node *pn0, *pn1;
	unsigned long nr_exceeded = 0;
	unsigned long tot_demoted = 0, tot_promoted = 0;

	if(!memcg)
		return;

	pn0 = memcg->nodeinfo[0];
	pn1 = memcg->nodeinfo[1];
	if(!pn0 || !pn1)
		return;

	if(memcg->use_mig) {
		if(need_fmem_demotion(NODE_DATA(0), memcg, &nr_exceeded)) {
			tot_demoted += demote_node(NODE_DATA(0), memcg, nr_exceeded);
		}
	}


}


static int vtmm_kmigrated(void *p)
{
	struct mem_cgroup *memcg = (struct mem_cgroup *)p;
	unsigned long vtmm_kmigrated_period_in_ms = 2000;

	while(!kthread_should_stop()) {
		vtmm_kmigrated_do_work(memcg);

		wait_event_interruptible_timeout(memcg->kmigrated_wait,
			need_direct_demotion(NODE_DATA(0), memcg),
			msecs_to_jiffies(vtmm_kmigrated_period_in_ms));
	}

	return 0;
}


// Invoked on vtmm registration syscall
int vtmm_kmigrated_init(struct mem_cgroup *memcg)
{
	if(!memcg)
		return -1;
	if(memcg->kmigrated)
		return -1;

	spin_lock_init(&memcg->kmigrated_lock);
	init_waitqueue_head(&memcg->kmigrated_wait);
	memcg->kmigrated = kthread_run(vtmm_kmigrated, memcg,
				"vtmm_kmigrated%d", mem_cgroup_id(memcg));
	if(IS_ERR(memcg->kmigrated)) {
		memcg->kmigrated = NULL;
		return -1;
	}
	return 0;
}

void vtmm_kmigrated_stop(struct mem_cgroup *memcg)
{
	if(memcg) {
		if(memcg->kmigrated) {
			kthread_stop(memcg->kmigrated);
			memcg->kmigrated = NULL;
		}
	}
}

void vtmm_kmigrated_wakeup(struct mem_cgroup *memcg)
{
	wake_up_interruptible(&memcg->kmigrated_wait);
}

