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
                                unsigned long nr_to_reclaim, unsigned long *demote_pingpong)
{
        LIST_HEAD(demote_pages);
        LIST_HEAD(ret_pages);
        unsigned long nr_reclaimed = 0;
        unsigned long nr_demotion_cand = 0;
        unsigned long nr_taken = 0;
	unsigned long demote_list_pingpong = 0;

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
				if(!need_direct_demotion(NODE_DATA(0), memcg)) {
					vp->demoted = true;
					if(vp->promoted) {
						demote_list_pingpong += HPAGE_PMD_NR;
						memcg->nr_pingpong += HPAGE_PMD_NR;
						vp->promoted = false;
					}
				}
                        }
			else {
				struct vtmm_page *vp = NULL;
			
				vp = get_vtmm_page(memcg, page);
				if(!vp)
					goto keep_locked;
				if(!need_direct_demotion(NODE_DATA(0), memcg)) {
					vp->demoted = true;
					if(vp->promoted) {
						demote_list_pingpong++;
						memcg->nr_pingpong++;
						vp->promoted = false;
					}
				}
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

	if(demote_pingpong)
		*demote_pingpong = demote_list_pingpong;

        return nr_reclaimed;
}


static unsigned long demote_inactive_list(unsigned long nr_to_scan, unsigned long nr_to_reclaim,
                                        struct lruvec *lruvec, enum lru_list lru, bool shrink_active,
					unsigned long *demote_pingpong)
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
                                	shrink_active, nr_to_reclaim, demote_pingpong);

        spin_lock_irq(&lruvec->lru_lock);
        move_pages_to_lru(lruvec, &page_list);
        __mod_node_page_state(pgdat, NR_ISOLATED_ANON + file, -nr_taken);
        spin_unlock_irq(&lruvec->lru_lock);

        mem_cgroup_uncharge_list(&page_list);
        free_unref_page_list(&page_list);

        return nr_reclaimed;
}


static unsigned long demote_lruvec(unsigned long nr_to_reclaim, short priority,
                                pg_data_t *pgdat, struct lruvec *lruvec, bool shrink_active,
				unsigned long *demote_pingpong)
{
        enum lru_list lru, tmp;
        unsigned long nr_reclaimed = 0;
        long nr_to_scan;
	unsigned long demote_list_pingpong = 0, demote_one_list_pingpong = 0;

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
			demote_one_list_pingpong = 0;
                        nr_reclaimed += demote_inactive_list(scan, scan, lruvec, lru, shrink_active,
								&demote_one_list_pingpong);
			demote_list_pingpong += demote_one_list_pingpong;
                        nr_to_scan -= (long)scan;
                        if(nr_reclaimed >= nr_to_reclaim)
                                break;
                }

                if(nr_reclaimed >= nr_to_reclaim)
                        break;
        }

	if(demote_pingpong)
		*demote_pingpong = demote_list_pingpong;

        return nr_reclaimed;
}




static unsigned long demote_node(pg_data_t *pgdat, struct mem_cgroup *memcg,
				unsigned long nr_exceeded, unsigned long *demote_pingpong)
{
	struct lruvec *lruvec = mem_cgroup_lruvec(memcg, pgdat);
        short priority = DEF_PRIORITY;
        unsigned long nr_to_reclaim = 0, nr_evictable_pages = 0, nr_reclaimed = 0;
        enum lru_list lru;
        bool shrink_active = false;
        int target_nid = 1;
        unsigned long max_dram = READ_ONCE(memcg->nodeinfo[pgdat->node_id]->max_nr_base_pages);
	unsigned long demote_lruvec_pingpong = 0, demote_one_lruvec_pingpong = 0;
       
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
		demote_one_lruvec_pingpong = 0;
                nr_reclaimed += demote_lruvec(nr_to_reclaim - nr_reclaimed, priority,
                                                pgdat, lruvec, shrink_active,
						&demote_one_lruvec_pingpong);
		demote_lruvec_pingpong += demote_one_lruvec_pingpong;
                if(nr_reclaimed >= nr_to_reclaim)
                        break;
                priority--;
        } while (priority);

        if(get_nr_lru_pages_node(memcg, pgdat) +
                get_memcg_demotion_wmark(max_dram) < max_dram)
                WRITE_ONCE(memcg->nodeinfo[pgdat->node_id]->need_demotion, false);

	if(demote_pingpong)
		*demote_pingpong = demote_lruvec_pingpong;

        return nr_reclaimed;
}

static unsigned long promote_page_list(struct list_head *page_list,
                                        pg_data_t *pgdat, enum lru_list lru,
                                        unsigned long *promote_pingpong)
{
        LIST_HEAD(promote_pages);
        LIST_HEAD(ret_pages);
        unsigned long nr_promoted = 0;
        unsigned long promote_list_pingpong = 0;

        cond_resched();

        while(!list_empty(page_list)) {
                struct page *page;
                struct mem_cgroup *memcg;

                page = lru_to_page(page_list);
                list_del(&page->lru);

                if(!trylock_page(page))
                        goto keep;
                if(!PageActive(page) && is_active_lru(lru))
                        goto keep_locked;
                if(PageActive(page) && !is_active_lru(lru))
                        goto keep_locked;
                if(unlikely(!page_evictable(page)))
                        goto keep_locked;
                if(PageWriteback(page))
                        goto keep_locked;
                if(PageTransHuge(page) && !thp_migration_supported())
                        goto keep_locked;

                memcg = page_memcg(page);
                if(PageAnon(page)) {
                        if(PageTransHuge(page)) {
				struct vtmm_page *vp = NULL;
			
				vp = get_vtmm_page(memcg, page);
				if(!vp)
					goto keep_locked;
				if(is_active_lru(lru)) {
					vp->promoted = true;
					if(vp->demoted) {
						promote_list_pingpong += HPAGE_PMD_NR;
						memcg->nr_pingpong += HPAGE_PMD_NR;
						vp->demoted = false;
					}
				} 
                        }
                        else {
				struct vtmm_page *vp = NULL;
			
				vp = get_vtmm_page(memcg, page);
				if(!vp)
					goto keep_locked;
				//BUG_ON(vp->is_thp);
				if(is_active_lru(lru)) {
					vp->promoted = true;
					if(vp->demoted) {
						promote_list_pingpong++;
						memcg->nr_pingpong++;
						vp->demoted = false;
					}
				}
                        }
		}

                list_add(&page->lru, &promote_pages);
                unlock_page(page);
                continue;
keep_locked:
                unlock_page(page);
keep:
                list_add(&page->lru, &ret_pages);
        }

        nr_promoted = migrate_page_list(&promote_pages, pgdat, true);
        if(!list_empty(&promote_pages))
                list_splice(&promote_pages, page_list);
        list_splice(&ret_pages, page_list);

        if(promote_pingpong)
                *promote_pingpong = promote_list_pingpong;

        return nr_promoted;
}



static unsigned long promote_lru_list(unsigned long nr_to_scan,
                                        struct lruvec *lruvec, enum lru_list lru,
                                        unsigned long *promote_pingpong)
{
        LIST_HEAD(page_list);
        pg_data_t *pgdat = lruvec_pgdat(lruvec);
        unsigned long nr_taken, nr_promoted;

        lru_add_drain();

        spin_lock_irq(&lruvec->lru_lock);
        nr_taken = isolate_lru_pages(nr_to_scan, lruvec, lru, &page_list, 0);
        __mod_node_page_state(pgdat, NR_ISOLATED_ANON, nr_taken);
        spin_unlock_irq(&lruvec->lru_lock);

        if(nr_taken == 0)
                return 0;

        nr_promoted = promote_page_list(&page_list, pgdat, lru, promote_pingpong);

        spin_lock_irq(&lruvec->lru_lock);
        move_pages_to_lru(lruvec, &page_list);
        __mod_node_page_state(pgdat, NR_ISOLATED_ANON, -nr_taken);
        spin_unlock_irq(&lruvec->lru_lock);

        mem_cgroup_uncharge_list(&page_list);
        free_unref_page_list(&page_list);

        return nr_promoted;
}


static unsigned long promote_lruvec(unsigned long nr_to_promote, short priority,
                                        pg_data_t *pgdat, struct lruvec *lruvec,
                                        enum lru_list lru, unsigned long *promote_pingpong)
{
        unsigned long nr_promoted = 0, nr;
        unsigned long promote_list_pingpong = 0, promote_one_list_pingpong = 0;

        nr = nr_to_promote >> priority;
        if(nr) {
                promote_one_list_pingpong = 0;
                nr_promoted += promote_lru_list(nr, lruvec, lru, &promote_one_list_pingpong);
                promote_list_pingpong += promote_one_list_pingpong;
        }

        if(promote_pingpong)
                *promote_pingpong = promote_list_pingpong;

        return nr_promoted;

}


static unsigned long promote_node(pg_data_t *pgdat, struct mem_cgroup *memcg,
					unsigned long *promote_pingpong)
{
	struct lruvec *lruvec = mem_cgroup_lruvec(memcg, pgdat);
        unsigned long nr_to_promote, nr_promoted = 0;
        enum lru_list lru = LRU_ACTIVE_ANON;
        short priority = DEF_PRIORITY;
        int target_nid = 0;
        unsigned long promote_lruvec_pingpong = 0, promote_one_lruvec_pingpong = 0;

        if(!promotion_available(target_nid, memcg, &nr_to_promote, false))
                return 0;

        nr_to_promote = min(nr_to_promote, lruvec_lru_size(lruvec, lru, MAX_NR_ZONES));

        do {
                promote_one_lruvec_pingpong = 0;
                nr_promoted += promote_lruvec(nr_to_promote, priority,
                                        pgdat, lruvec, lru, &promote_one_lruvec_pingpong);
                promote_lruvec_pingpong += promote_one_lruvec_pingpong;
                if(nr_promoted >= nr_to_promote)
                        break;
                priority--;
        } while (priority);

        if(promote_pingpong)
                *promote_pingpong = promote_lruvec_pingpong;

        return nr_promoted;

}

static unsigned long promote_node_expanded(pg_data_t *pgdat, struct mem_cgroup *memcg)
{
        struct lruvec *lruvec = mem_cgroup_lruvec(memcg, pgdat);
        unsigned long nr_to_promote, nr_promoted = 0;
        enum lru_list lru = LRU_INACTIVE_ANON;//promote inactive when dram expanded
        short priority = DEF_PRIORITY;
        int target_nid = 0;

        if(!promotion_available(target_nid, memcg, &nr_to_promote, true)) {
                WRITE_ONCE(memcg->dram_expanded, false);
                /*pr_err("[%s] [ %s ] promote node expanded failed. dram : %lu MB, node0 lru : %lu MB\n",
                        __func__, memcg->tenant_name, memcg->max_nr_dram_pages >> 8,
                        get_nr_lru_pages_node(memcg, NODE_DATA(0)) >> 8);
                */
		return 0;
        }

        nr_to_promote = min(nr_to_promote, lruvec_lru_size(lruvec, lru, MAX_NR_ZONES));

        do {
                nr_promoted += promote_lruvec(nr_to_promote, priority,
                                        pgdat, lruvec, lru, NULL);
                if(nr_promoted >= nr_to_promote)
                        break;
                priority--;
        } while (priority);

        if(!promotion_available(target_nid, memcg, &nr_to_promote, true)) {
                WRITE_ONCE(memcg->dram_expanded, false);
        }

        return nr_promoted;
}


static void vtmm_kmigrated_do_work(struct mem_cgroup *memcg,
				unsigned long *demoted, unsigned long *promoted,
				unsigned long *demote_pingpong, unsigned long *promote_pingpong)
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
			tot_demoted += demote_node(NODE_DATA(0), memcg, nr_exceeded,
							demote_pingpong);
		}

		if(nr_promotion_target(NODE_DATA(1), memcg)) {
			tot_promoted += promote_node(NODE_DATA(1), memcg, promote_pingpong);
		}

		if(READ_ONCE(memcg->dram_expanded)) {
			unsigned long expanded_promoted = 0;
			expanded_promoted = promote_node_expanded(NODE_DATA(1), memcg);
		}

	}

	if(demoted)
		*demoted = tot_demoted;
	if(promoted)
		*promoted = tot_promoted;

}


static int vtmm_kmigrated(void *p)
{
	struct mem_cgroup *memcg = (struct mem_cgroup *)p;
	unsigned long vtmm_kmigrated_period_in_ms = 2000;
	unsigned long tot_demoted = 0, tot_promoted = 0;
	unsigned long one_demoted = 0, one_promoted = 0;
	unsigned long demote_pingpong = 0, promote_pingpong = 0;
	unsigned long one_demote_pingpong = 0, one_promote_pingpong = 0;
	unsigned long tot_mig_cputime = 0, one_mig_cputime = 0;
	unsigned long total_time = 0;

	total_time = jiffies;
	while(!kthread_should_stop()) {
		one_demoted = 0;
		one_promoted = 0;
		one_demote_pingpong = 0;
		one_promote_pingpong = 0;
		one_mig_cputime = jiffies;
		vtmm_kmigrated_do_work(memcg, &one_demoted, &one_promoted,
					&one_demote_pingpong, &one_promote_pingpong);
		tot_mig_cputime += jiffies - one_mig_cputime;
		tot_demoted += one_demoted;
		tot_promoted += one_promoted;
		demote_pingpong += one_demote_pingpong;
		promote_pingpong += one_promote_pingpong;

		wait_event_interruptible_timeout(memcg->kmigrated_wait,
			need_direct_demotion(NODE_DATA(0), memcg),
			msecs_to_jiffies(vtmm_kmigrated_period_in_ms));
	}

	total_time = jiffies - total_time;
	pr_info("[%s] [ %s ] tot [promoted : %lu MB, demoted : %lu MB], nr_pingpong : %lu MB\n",
		__func__, memcg->tenant_name, tot_promoted >> 8, tot_demoted >> 8,
		(demote_pingpong + promote_pingpong) >> 8);
	pr_info("[%s] [ %s ] total_time : %lu, total_mig_cputime : %lu\n",
		__func__, memcg->tenant_name, total_time, tot_mig_cputime);

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

