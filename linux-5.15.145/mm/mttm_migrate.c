/*
 * kmigrated does cooling, lru adjusting, migration.
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
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/mm.h>

#define CREATE_TRACE_POINTS
#include <trace/events/mttm.h>

#include "internal.h"

#define MIN_WMARK_LOWER_LIMIT	128 * 100 // 50MB
#define MIN_WMARK_UPPER_LIMIT	2560 * 100 // 1000MB
#define MAX_WMARK_LOWER_LIMIT	256 * 100 // 100MB
#define MAX_WMARK_UPPER_LIMIT	3840 * 100 // 1500MB


#ifdef ARCH_HAS_PREFETCHW
#define prefetchw_prev_lru_page(_page, _base, _field)			\
	do {								\
		if((_page)->lru.prev != _base) {			\
			struct page *prev;				\
			prev = lru_to_page(&(_page->lru));		\
			prefetchw(&prev->_field);			\
		}							\
	} while (0)
#else
#define prefetchw_prev_lru_page(_page, _base, _field) do { } while (0)
#endif

extern unsigned int use_dram_determination;
extern unsigned int use_region_separation;
extern unsigned int use_pingpong_reduce;
extern unsigned long pingpong_reduce_threshold;
unsigned long pingpong_reduce_limit = 1;
extern unsigned long mig_cputime_threshold;
extern unsigned int check_stable_sample_rate;
extern unsigned int scanless_cooling;
extern unsigned int reduce_scan;

unsigned int hugepage_period_factor = 1;
unsigned int hugepage_shift_factor = 2;
unsigned int basepage_period_factor = 40;
unsigned int basepage_shift_factor = 9;
unsigned long kmigrated_period_in_ms = 1000;


static bool need_lru_cooling(struct mem_cgroup_per_node *pn)
{
	return READ_ONCE(pn->need_cooling);
}

static bool need_lru_adjusting(struct mem_cgroup_per_node *pn)
{
	return READ_ONCE(pn->need_adjusting);
}

unsigned long get_memcg_demotion_wmark(unsigned long max_nr_pages)
{
	unsigned long ret;
	ret = max_nr_pages * 2 / 100;
	if(ret < MIN_WMARK_LOWER_LIMIT)
		return MIN_WMARK_LOWER_LIMIT;
	else if(ret > MIN_WMARK_UPPER_LIMIT)
		return MIN_WMARK_UPPER_LIMIT;
	return ret;
}

unsigned long get_memcg_promotion_wmark(unsigned long max_nr_pages)
{
	unsigned long ret;
	ret = max_nr_pages * 3 / 100;
	if(ret < MAX_WMARK_LOWER_LIMIT)
		return MAX_WMARK_LOWER_LIMIT;
	else if(ret > MAX_WMARK_UPPER_LIMIT)
		return MAX_WMARK_UPPER_LIMIT;
	return ret;
}

unsigned long get_memcg_promotion_expanded_wmark(unsigned long max_nr_pages)
{
	unsigned long ret;
	ret = max_nr_pages * 5 / 100;
	if(ret < MAX_WMARK_LOWER_LIMIT)
		return MAX_WMARK_LOWER_LIMIT;
	else if(ret > MAX_WMARK_UPPER_LIMIT)
		return MAX_WMARK_UPPER_LIMIT;
	return ret;
}

unsigned long nr_promotion_target(pg_data_t *pgdat, struct mem_cgroup *memcg)
{
	struct lruvec *lruvec;
	unsigned long lruvec_size;

	lruvec = mem_cgroup_lruvec(memcg, pgdat);
	lruvec_size = lruvec_lru_size(lruvec, LRU_ACTIVE_ANON, MAX_NR_ZONES);

	return lruvec_size;
}

bool need_direct_demotion(pg_data_t *pgdat, struct mem_cgroup *memcg)
{
	return READ_ONCE(memcg->nodeinfo[pgdat->node_id]->need_demotion);
}



// Demote pages to get %fmem_max_wmark free pages 
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

	if(max_nr_pages < fmem_min_wmark) { // extremely low dram size
		if(nr_lru_pages < fmem_min_wmark) {
			//if(need_direct_demotion(pgdat, memcg))
			WRITE_ONCE(memcg->nodeinfo[pgdat->node_id]->need_demotion, false);
			return false;
		}
		*nr_exceeded = (max_nr_pages > nr_lru_pages) ? 0 : nr_lru_pages - max_nr_pages;
		return true;
	}

	if(need_direct_demotion(pgdat, memcg)) { // set at mempolicy.c and dram determination
		if(nr_lru_pages + fmem_max_wmark <= max_nr_pages)
			goto check_nr_need_promoted;
		if(nr_lru_pages < max_nr_pages)
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
		__prep_transhuge_page_for_mttm(NULL, newpage);
	}
	else {
		newpage = __alloc_pages_node(nid, mask, 0);
	}

	return newpage;
}

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

static unsigned long migrate_page_list(struct list_head *migrate_list, pg_data_t *pgdat,
					bool promotion, unsigned long *mig_cputime)
{
	int target_nid = promotion ? 0 : 1;
	unsigned int nr_succeeded = 0;
	unsigned long one_mig_cputime;

	if(list_empty(migrate_list)) {
		return 0;
	}

	one_mig_cputime = jiffies;
	migrate_pages(migrate_list, alloc_migrate_page, NULL, target_nid,
			MIGRATE_ASYNC, MR_NUMA_MISPLACED, &nr_succeeded);
	one_mig_cputime = jiffies - one_mig_cputime;

	if(mig_cputime)
		*mig_cputime = one_mig_cputime;

	return nr_succeeded;
}

static unsigned long shrink_page_list(struct list_head *page_list, pg_data_t *pgdat,
				struct mem_cgroup *memcg, bool shrink_active,
				unsigned long nr_to_reclaim, unsigned long *demote_cputime,
				unsigned long *demote_pingpong)
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
				struct page *meta_page = get_meta_page(page);
				uint32_t nr_accesses = 0;
				uint32_t *ac;

				if(!meta_page)
					goto keep_locked;

				if(scanless_cooling) {
					ac = get_ac_pointer(memcg, meta_page->giga_bitmap_idx,
						meta_page->huge_bitmap_idx, meta_page->base_bitmap_idx);
					if(ac)
						nr_accesses = *ac;
				}
				else
					nr_accesses = meta_page->nr_accesses;

				if(memcg->use_warm &&
					get_idx(nr_accesses) >= READ_ONCE(memcg->warm_threshold))
					goto keep_locked;
				if(!need_direct_demotion(NODE_DATA(0), memcg)) {
					meta_page->demoted = 1;
					if(meta_page->promoted > 0) {
						demote_list_pingpong += HPAGE_PMD_NR;
						memcg->nr_pingpong += HPAGE_PMD_NR;
						meta_page->promoted = 0;
					}
				}
				else {
					// demotion on dram limit
					meta_page->demoted = 0;
					meta_page->promoted = 0;
				}
			}
			else {
				unsigned int idx;
				pginfo_t *pginfo = NULL;
				uint32_t nr_accesses = 0;
				uint32_t *ac;

				pginfo = get_pginfo_from_page(page);	
				if(!pginfo) {
					goto keep_locked;
				}

				if(scanless_cooling) {
					ac = get_ac_pointer(memcg, pginfo->giga_bitmap_idx,
						pginfo->huge_bitmap_idx, pginfo->base_bitmap_idx);
					if(ac)
						nr_accesses = *ac;
				}
				else
					nr_accesses = pginfo->nr_accesses;

				idx = get_idx(nr_accesses);
				if(memcg->use_warm &&
					idx >= READ_ONCE(memcg->warm_threshold))
					goto keep_locked;
				if(!need_direct_demotion(NODE_DATA(0), memcg)) {
					pginfo->demoted = 1;
					if(pginfo->promoted > 0) {
						demote_list_pingpong++;
						memcg->nr_pingpong++;
						pginfo->promoted = 0;
					}
				}
				else {
					// demotion on dram limit
					pginfo->demoted = 0;
					pginfo->promoted = 0;
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

	nr_reclaimed = migrate_page_list(&demote_pages, pgdat, false, demote_cputime);
	if(!list_empty(&demote_pages))
		list_splice(&demote_pages, page_list);
	list_splice(&ret_pages, page_list);

	if(demote_pingpong)
		*demote_pingpong = demote_list_pingpong;

	return nr_reclaimed;
}


static unsigned long demote_inactive_list(unsigned long nr_to_scan, unsigned long nr_to_reclaim,
					struct lruvec *lruvec, enum lru_list lru, bool shrink_active,
					unsigned long *demote_cputime, unsigned long *demote_pingpong)
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
				shrink_active, nr_to_reclaim, demote_cputime, demote_pingpong);

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
				unsigned long *demote_cputime, unsigned long *demote_pingpong)
{
	enum lru_list lru, tmp;
	unsigned long nr_reclaimed = 0;
	long nr_to_scan;
	unsigned long demote_list_cputime = 0, demote_one_list_cputime = 0;
	unsigned long demote_list_pingpong = 0, demote_one_list_pingpong = 0;

	for_each_evictable_lru(tmp) {
		lru = (tmp + 2) % 4;// scan file lru first

		if(!shrink_active && !is_file_lru(lru) && is_active_lru(lru))
			continue;

		if(is_file_lru(lru)) 
			nr_to_scan = lruvec_lru_size(lruvec, lru, MAX_NR_ZONES);
		else {
			nr_to_scan = lruvec_lru_size(lruvec, lru, MAX_NR_ZONES) >> priority;
			if(nr_to_scan < nr_to_reclaim)
				nr_to_scan = nr_to_reclaim * 11 / 10;
		}

		if(!nr_to_scan)
			continue;

		while(nr_to_scan > 0) {
			unsigned long scan = min((unsigned long)nr_to_scan, SWAP_CLUSTER_MAX);
			demote_one_list_cputime = 0;
			demote_one_list_pingpong = 0;
			nr_reclaimed += demote_inactive_list(scan, scan, lruvec, lru, shrink_active,
							&demote_one_list_cputime, &demote_one_list_pingpong);
			demote_list_cputime += demote_one_list_cputime;
			demote_list_pingpong += demote_one_list_pingpong;
			nr_to_scan -= (long)scan;
			if(nr_reclaimed >= nr_to_reclaim)
				break;
		}

		if(nr_reclaimed >= nr_to_reclaim)
			break;
	}

	if(demote_cputime)
		*demote_cputime = demote_list_cputime;
	if(demote_pingpong)
		*demote_pingpong = demote_list_pingpong;

	return nr_reclaimed;
}


static unsigned long demote_node(pg_data_t *pgdat, struct mem_cgroup *memcg,
			unsigned long nr_exceeded, unsigned long *demote_cputime, unsigned long *demote_pingpong)
{
	struct lruvec *lruvec = mem_cgroup_lruvec(memcg, pgdat);
	short priority = DEF_PRIORITY;
	unsigned long nr_to_reclaim = 0, nr_evictable_pages = 0, nr_reclaimed = 0;
	enum lru_list lru;
	bool shrink_active = false;
	int target_nid = 1;
	unsigned long nr_smem_active = nr_promotion_target(NODE_DATA(target_nid), memcg);
	unsigned long max_dram = READ_ONCE(memcg->nodeinfo[pgdat->node_id]->max_nr_base_pages);
	unsigned long demote_lruvec_cputime = 0, demote_one_lruvec_cputime = 0;
	unsigned long demote_lruvec_pingpong = 0, demote_one_lruvec_pingpong = 0;

	for_each_evictable_lru(lru) {
		if(!is_file_lru(lru) && is_active_lru(lru))
			continue;
		nr_evictable_pages += lruvec_lru_size(lruvec, lru, MAX_NR_ZONES);
	}

	nr_to_reclaim = nr_exceeded;
	//if(nr_exceeded > nr_evictable_pages && need_direct_demotion(pgdat, memcg))
	//	shrink_active = true;

	do {	//nr_reclaimed starts with small number and 
		//increases exponentially by decreasing priority
		demote_one_lruvec_cputime = 0;
		demote_one_lruvec_pingpong = 0;
		nr_reclaimed += demote_lruvec(nr_to_reclaim - nr_reclaimed, priority,
						pgdat, lruvec, shrink_active, &demote_one_lruvec_cputime,
						&demote_one_lruvec_pingpong);
		demote_lruvec_cputime += demote_one_lruvec_cputime;
		demote_lruvec_pingpong += demote_one_lruvec_pingpong;
		if(nr_reclaimed >= nr_to_reclaim)
			break;
		priority--;
	} while (priority);
	
	//if smem active still exists, remove warm
	if(memcg->use_warm) {
		nr_smem_active = nr_smem_active < nr_to_reclaim ?
					nr_smem_active : nr_to_reclaim;
		if(nr_smem_active && nr_reclaimed < nr_smem_active)
			WRITE_ONCE(memcg->warm_threshold, memcg->active_threshold);
	}

	if(get_nr_lru_pages_node(memcg, pgdat) +
		get_memcg_demotion_wmark(max_dram) < max_dram)
		WRITE_ONCE(memcg->nodeinfo[pgdat->node_id]->need_demotion, false);
	//else if(get_nr_lru_pages_node(memcg, pgdat) < get_memcg_demotion_wmark(max_dram)) //extreme low dram, demote done
	//	WRITE_ONCE(memcg->nodeinfo[pgdat->node_id]->need_demotion, false);

	if(demote_cputime)
		*demote_cputime = demote_lruvec_cputime;
	if(demote_pingpong)
		*demote_pingpong = demote_lruvec_pingpong;
	
	return nr_reclaimed;
}


static unsigned long promote_page_list(struct list_head *page_list,
					pg_data_t *pgdat, enum lru_list lru,
					unsigned long *promote_cputime,
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
				struct page *meta_page = get_meta_page(page);
				if(is_active_lru(lru)) {
					meta_page->promoted = 1;
					if(meta_page->demoted > 0) {
						promote_list_pingpong += HPAGE_PMD_NR;
						memcg->nr_pingpong += HPAGE_PMD_NR;
						meta_page->demoted = 0;
					}
				}
				else if(READ_ONCE(memcg->dram_expanded)) {
					// promotion on dram expand
					meta_page->promoted = 0;
					meta_page->demoted = 0;
				}
			}
			else {
				pginfo_t *pginfo = NULL;
				pginfo = get_pginfo_from_page(page);
				
				if(!pginfo) {
					//pr_err("[%s] NULL pginfo\n",__func__);
					goto keep_locked;
				}
				if(is_active_lru(lru)) {
					pginfo->promoted = 1;
					if(pginfo->demoted > 0) {
						promote_list_pingpong++;
						memcg->nr_pingpong++;
						pginfo->demoted = 0;
					}
				}
				else if(READ_ONCE(memcg->dram_expanded)) {
					// promotion on dram expand
					pginfo->promoted = 0;
					pginfo->demoted = 0;
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

	nr_promoted = migrate_page_list(&promote_pages, pgdat, true, promote_cputime);
	if(!list_empty(&promote_pages))
		list_splice(&promote_pages, page_list);
	list_splice(&ret_pages, page_list);

	if(promote_pingpong)
		*promote_pingpong = promote_list_pingpong;

	return nr_promoted;
}

static unsigned long promote_lru_list(unsigned long nr_to_scan,
					struct lruvec *lruvec, enum lru_list lru,
					unsigned long *promote_cputime, unsigned long *promote_pingpong)
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

	nr_promoted = promote_page_list(&page_list, pgdat, lru, promote_cputime, promote_pingpong);

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
					enum lru_list lru, unsigned long *promote_cputime,
					unsigned long *promote_pingpong)
{
	unsigned long nr_promoted = 0, nr;
	unsigned long promote_list_cputime = 0, promote_one_list_cputime = 0;
	unsigned long promote_list_pingpong = 0, promote_one_list_pingpong = 0;

	nr = nr_to_promote >> priority;
	if(nr) {
		promote_one_list_cputime = 0;
		promote_one_list_pingpong = 0;
		nr_promoted += promote_lru_list(nr, lruvec, lru, &promote_one_list_cputime,
								&promote_one_list_pingpong);
		promote_list_cputime += promote_one_list_cputime;
		promote_list_pingpong += promote_one_list_pingpong;
	}

	if(promote_cputime)
		*promote_cputime = promote_list_cputime;
	if(promote_pingpong)
		*promote_pingpong = promote_list_pingpong;

	return nr_promoted;

}

static unsigned long node_free_pages(pg_data_t *pgdat)
{
	int z;
	long free_pages;
	long total = 0;

	for(z = pgdat->nr_zones - 1; z >= 0; z--) {
		struct zone *zone = pgdat->node_zones + z;
		long nr_high_wmark_pages;

		if(!populated_zone(zone))
			continue;

		free_pages = zone_page_state(zone, NR_FREE_PAGES);
		free_pages -= zone->nr_reserved_highatomic;
		free_pages -= zone->lowmem_reserve[ZONE_MOVABLE];

		nr_high_wmark_pages = high_wmark_pages(zone);
		if(free_pages >= nr_high_wmark_pages)
			total += (free_pages - nr_high_wmark_pages);
	}

	return (unsigned long)total;
}

bool promotion_available(int target_nid, struct mem_cgroup *memcg,
			unsigned long *nr_to_promote, bool expanded)
{
	pg_data_t *pgdat;
	unsigned long max_nr_pages, cur_nr_pages;
	unsigned long nr_isolated;
	unsigned long fmem_min_wmark;

	if(target_nid == NUMA_NO_NODE)
		return false;

	pgdat = NODE_DATA(target_nid);

	cur_nr_pages = get_nr_lru_pages_node(memcg, pgdat);
	max_nr_pages = READ_ONCE(memcg->nodeinfo[target_nid]->max_nr_base_pages);
	//isolated on node vs isolated on memcg .. at multi tenants
	//demotion & promotion are coupled, so nr_isolated may be close to 0.
	nr_isolated = node_page_state(pgdat, NR_ISOLATED_ANON) +
			node_page_state(pgdat, NR_ISOLATED_FILE);

	if(expanded)
		fmem_min_wmark = get_memcg_promotion_expanded_wmark(max_nr_pages);
	else
		fmem_min_wmark = get_memcg_demotion_wmark(max_nr_pages);

	if(max_nr_pages == ULONG_MAX) {
		*nr_to_promote = node_free_pages(pgdat);
		return true;
	}
	else {
		if(max_nr_pages < fmem_min_wmark)
			return false;
		else if(cur_nr_pages + nr_isolated < max_nr_pages - fmem_min_wmark) {
			*nr_to_promote = max_nr_pages - fmem_min_wmark - cur_nr_pages - nr_isolated;
			return true;
		}
	}

	return false;
}

static unsigned long promote_node(pg_data_t *pgdat, struct mem_cgroup *memcg, bool *promotion_denied,
					unsigned long *promote_cputime, unsigned long *promote_pingpong)
{
	struct lruvec *lruvec = mem_cgroup_lruvec(memcg, pgdat);
	unsigned long nr_to_promote, nr_promoted = 0;
	enum lru_list lru = LRU_ACTIVE_ANON;
	short priority = DEF_PRIORITY;
	int target_nid = 0;
	unsigned long promote_lruvec_cputime = 0, promote_one_lruvec_cputime = 0;
	unsigned long promote_lruvec_pingpong = 0, promote_one_lruvec_pingpong = 0;

	if(!promotion_available(target_nid, memcg, &nr_to_promote, false))
		return 0;
	if(promotion_denied)
		*promotion_denied = false;

	nr_to_promote = min(nr_to_promote, lruvec_lru_size(lruvec, lru, MAX_NR_ZONES));

	do {
		promote_one_lruvec_cputime = 0;
		promote_one_lruvec_pingpong = 0;
		nr_promoted += promote_lruvec(nr_to_promote, priority,
					pgdat, lruvec, lru, &promote_one_lruvec_cputime,
					&promote_one_lruvec_pingpong);
		promote_lruvec_cputime += promote_one_lruvec_cputime;
		promote_lruvec_pingpong += promote_one_lruvec_pingpong;
		if(nr_promoted >= nr_to_promote)
			break;
		priority--;
	} while (priority);

	if(promote_cputime)
		*promote_cputime = promote_lruvec_cputime;
	if(promote_pingpong)
		*promote_pingpong = promote_lruvec_pingpong;

	return nr_promoted;
}

static unsigned long promote_node_expanded(pg_data_t *pgdat, struct mem_cgroup *memcg,
					unsigned long *promote_cputime)
{
	struct lruvec *lruvec = mem_cgroup_lruvec(memcg, pgdat);
	unsigned long nr_to_promote, nr_promoted = 0;
	enum lru_list lru = LRU_INACTIVE_ANON;//promote inactive when dram expanded
	short priority = DEF_PRIORITY;
	int target_nid = 0;
	unsigned long promote_lruvec_cputime = 0, promote_one_lruvec_cputime = 0;

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
		promote_one_lruvec_cputime = 0;
		nr_promoted += promote_lruvec(nr_to_promote, priority,
					pgdat, lruvec, lru, &promote_one_lruvec_cputime,
					NULL);
		promote_lruvec_cputime += promote_one_lruvec_cputime;
		if(nr_promoted >= nr_to_promote)
			break;
		priority--;
	} while (priority);

	if(!promotion_available(target_nid, memcg, &nr_to_promote, true)) {
		WRITE_ONCE(memcg->dram_expanded, false);
	}

	if(promote_cputime)
		*promote_cputime = promote_lruvec_cputime;

	return nr_promoted;
}



static unsigned long move_to_inactive(unsigned long nr_to_scan, struct lruvec *lruvec,
					enum lru_list lru)
{
	unsigned long nr_taken;
	struct mem_cgroup *memcg = lruvec_memcg(lruvec);
	pg_data_t *pgdat = lruvec_pgdat(lruvec);
	LIST_HEAD(l_hold);
	LIST_HEAD(l_to_inactive);
	int file = is_file_lru(lru);

	lru_add_drain();

	spin_lock_irq(&lruvec->lru_lock);
	nr_taken = isolate_lru_pages(nr_to_scan, lruvec, lru, &l_hold, 0);
	__mod_node_page_state(pgdat, NR_ISOLATED_ANON + file, nr_taken);
	spin_unlock_irq(&lruvec->lru_lock);

	cond_resched();
	while(!list_empty(&l_hold)) {
		struct page *page;

		page = lru_to_page(&l_hold);
		list_del(&page->lru);
		if(unlikely(!page_evictable(page))) {
			putback_lru_page(page);
			continue;
		}

		ClearPageActive(page);
		SetPageWorkingset(page);
		list_add(&page->lru, &l_to_inactive);
	}

	spin_lock_irq(&lruvec->lru_lock);
	move_pages_to_lru(lruvec, &l_to_inactive);

	__mod_node_page_state(pgdat, NR_ISOLATED_ANON + file, -nr_taken);
	spin_unlock_irq(&lruvec->lru_lock);

	mem_cgroup_uncharge_list(&l_to_inactive);
	free_unref_page_list(&l_to_inactive);

	return nr_taken;
}


static void move_all_to_inactive(pg_data_t *pgdat, struct mem_cgroup *memcg)
{
	unsigned long nr_to_scan, nr_scanned = 0, nr_max_scan = 12;
	struct lruvec *lruvec = mem_cgroup_lruvec(memcg, pgdat);
	struct mem_cgroup_per_node *pn = memcg->nodeinfo[pgdat->node_id];
	enum lru_list lru = LRU_ACTIVE_ANON;

	nr_to_scan = lruvec_lru_size(lruvec, lru, MAX_NR_ZONES);
	do {
		unsigned long scan = nr_to_scan >> 3; // 12.5%
		if(!scan)
			scan = nr_to_scan;
	
		nr_scanned += move_to_inactive(scan, lruvec, lru);

		nr_max_scan--;
	} while (nr_scanned < nr_to_scan && nr_max_scan);

	// active file list
	move_to_inactive(lruvec_lru_size(lruvec, LRU_ACTIVE_FILE, MAX_NR_ZONES),
			lruvec, LRU_ACTIVE_FILE);

	WRITE_ONCE(pn->need_cooling, false);
}

static void scanless_cooling_node(struct mem_cgroup *memcg)
{
	unsigned int init_threshold = test_bit(TRANSPARENT_HUGEPAGE_FLAG, &transparent_hugepage_flags) ?
					MTTM_INIT_THRESHOLD : 9;

	// Zeroing ac_pages with DMA
	zeroing_ac_pages(memcg);

	// Move all active lru mttm pages to inactive lru.
	move_all_to_inactive(NODE_DATA(0), memcg);
	move_all_to_inactive(NODE_DATA(1), memcg);

	// not trigger adjust lru
	WRITE_ONCE(memcg->active_threshold, init_threshold + memcg->threshold_offset);
}


static unsigned long adjusting_lru_list(unsigned long nr_to_scan, struct lruvec *lruvec,
	enum lru_list lru, unsigned long *nr_to_active_one, unsigned long *nr_to_inactive_one)
{
	unsigned long nr_taken;
	pg_data_t *pgdat = lruvec_pgdat(lruvec);
	struct mem_cgroup *memcg = lruvec_memcg(lruvec);
	LIST_HEAD(l_hold);
	LIST_HEAD(l_active);
	LIST_HEAD(l_inactive);
	int file = is_file_lru(lru);
	bool active = is_active_lru(lru);
	unsigned long nr_to_active_ = 0, nr_to_inactive_ = 0;

	if(file)
		return 0;

	lru_add_drain();

	spin_lock_irq(&lruvec->lru_lock);
	nr_taken = isolate_lru_pages(nr_to_scan, lruvec, lru, &l_hold, 0);
	__mod_node_page_state(pgdat, NR_ISOLATED_ANON, nr_taken);
	spin_unlock_irq(&lruvec->lru_lock);

	cond_resched();
	while(!list_empty(&l_hold)) {
		struct page *page;
		int status;

		page = lru_to_page(&l_hold);
		list_del(&page->lru);

		if(unlikely(!page_evictable(page))) {
			putback_lru_page(page);
			continue;
		}
		
		if(PageTransHuge(compound_head(page))) {
			struct page *meta_page = get_meta_page(page);
			uint32_t nr_accesses = 0;
			uint32_t *ac;

			if(scanless_cooling) {
				ac = get_ac_pointer(memcg, meta_page->giga_bitmap_idx,
					meta_page->huge_bitmap_idx, meta_page->base_bitmap_idx);
				if(ac)
					nr_accesses = *ac;
			}
			else
				nr_accesses = meta_page->nr_accesses;

			if(get_idx(nr_accesses) >= READ_ONCE(memcg->active_threshold))
				status = 2;
			else
				status = 1;
		}
		else {
			status = page_check_hotness(page, memcg);
		}

		if(status == 2) {
			nr_to_active_ += thp_nr_pages(page);
			if(active) {
				list_add(&page->lru, &l_active);
				continue;
			}
			SetPageActive(page);
			list_add(&page->lru, &l_active);
		}
		else if (status == 0) {
			if(PageActive(page)) {
				list_add(&page->lru, &l_active);
				nr_to_active_ += thp_nr_pages(page);
			}
			else {
				list_add(&page->lru, &l_inactive);
				nr_to_inactive_ += thp_nr_pages(page);
			}
		}
		else {
			nr_to_inactive_ += thp_nr_pages(page);
			if(!active) {
				list_add(&page->lru, &l_inactive);
				continue;
			}
			ClearPageActive(page);
			SetPageWorkingset(page);
			list_add(&page->lru, &l_inactive);
		}
	}

	spin_lock_irq(&lruvec->lru_lock);
	move_pages_to_lru(lruvec, &l_active);
	move_pages_to_lru(lruvec, &l_inactive);
	list_splice(&l_inactive, &l_active);

	__mod_node_page_state(pgdat, NR_ISOLATED_ANON, -nr_taken);
	spin_unlock_irq(&lruvec->lru_lock);

	mem_cgroup_uncharge_list(&l_active);
	free_unref_page_list(&l_active);

	if(nr_to_active_one)
		*nr_to_active_one = nr_to_active_;
	if(nr_to_inactive_one)
		*nr_to_inactive_one = nr_to_inactive_;

	return nr_taken;
}

static void adjusting_node(pg_data_t *pgdat, struct mem_cgroup *memcg, bool active,
	unsigned long *nr_adjusted, unsigned long *nr_to_active, unsigned long *nr_to_inactive)
{
	struct lruvec *lruvec = mem_cgroup_lruvec(memcg, pgdat);
	struct mem_cgroup_per_node *pn = memcg->nodeinfo[pgdat->node_id];
	enum lru_list lru = active ? LRU_ACTIVE_ANON : LRU_INACTIVE_ANON;
	unsigned long nr_to_scan, nr_scanned = 0, nr_max_scan = 12;
	unsigned long nr_to_active_one = 0, nr_to_inactive_one = 0;
	unsigned long nr_to_active_tot = 0, nr_to_inactive_tot = 0;

	nr_to_scan = lruvec_lru_size(lruvec, lru, MAX_NR_ZONES);
	do {
		unsigned long scan = nr_to_scan >> 3;
		if(!scan)
			scan = nr_to_scan;
		nr_scanned += adjusting_lru_list(scan, lruvec, lru,
					&nr_to_active_one, &nr_to_inactive_one);
		nr_to_active_tot += nr_to_active_one;
		nr_to_inactive_tot += nr_to_inactive_one;
		nr_max_scan--;
	} while(nr_scanned < nr_to_scan && nr_max_scan);

	if(nr_scanned >= nr_to_scan)
		WRITE_ONCE(pn->need_adjusting, false);
	if(nr_scanned >= nr_to_scan && !active)
		WRITE_ONCE(pn->need_adjusting_all, false);
	
	if(nr_adjusted)
		*nr_adjusted = nr_scanned;
	if(nr_to_active)
		*nr_to_active = nr_to_active_tot;
	if(nr_to_inactive)
		*nr_to_inactive = nr_to_inactive_tot;
}

static unsigned long scan_hotness_lru_list(unsigned long nr_to_scan,
					struct lruvec *lruvec, enum lru_list lru,
					unsigned long *region_size, uint32_t *nr_region_access,
					unsigned long *lev_size)
{
	unsigned long nr_taken;
	pg_data_t *pgdat = lruvec_pgdat(lruvec);
	struct mem_cgroup *memcg = lruvec_memcg(lruvec);
	LIST_HEAD(l_hold);
	LIST_HEAD(l_scanned);
	int file = is_file_lru(lru);
	unsigned int idx;

	if(file)
		return 0;

	lru_add_drain();

	spin_lock_irq(&lruvec->lru_lock);
	nr_taken = isolate_lru_pages(nr_to_scan, lruvec, lru, &l_hold, 0);
	__mod_node_page_state(pgdat, NR_ISOLATED_ANON, nr_taken);
	spin_unlock_irq(&lruvec->lru_lock);

	cond_resched();
	while(!list_empty(&l_hold)) {
		struct page *page;

		page = lru_to_page(&l_hold);
		list_del(&page->lru);

		if(unlikely(!page_evictable(page))) {
			putback_lru_page(page);
			continue;
		}
		
		if(PageTransHuge(compound_head(page))) {
			struct page *meta_page = get_meta_page(page);
			uint32_t nr_accesses = 0;
			uint32_t *ac;
			unsigned int i;

			if(!PageMttm(meta_page)) {
				list_add(&page->lru, &l_scanned);
				continue;
			}

			if(scanless_cooling) {
				ac = get_ac_pointer(memcg, meta_page->giga_bitmap_idx,
					meta_page->huge_bitmap_idx, meta_page->base_bitmap_idx);
				if(ac)
					nr_accesses = *ac;
				else {
					list_add(&page->lru, &l_scanned);
					continue;
				}
			}
			else
				nr_accesses = meta_page->nr_accesses;
	

			idx = get_idx(nr_accesses);
			if(use_region_separation) {
				if(idx >= NR_REGION - 1) {
					i = NR_REGION - 1;	
				}
				else 
					i = idx;
				region_size[i] += HPAGE_PMD_NR;
				nr_region_access[i] += nr_accesses;
			}
		}
		else {
			pginfo_t *pginfo = NULL;
			uint32_t nr_accesses = 0; 
			uint32_t *ac;
			unsigned int i;
			pginfo = get_pginfo_from_page(page);
			
			if(!pginfo) {
				list_add(&page->lru, &l_scanned);
				continue;
			}

			if(scanless_cooling) {
				ac = get_ac_pointer(memcg, pginfo->giga_bitmap_idx,
					pginfo->huge_bitmap_idx, pginfo->base_bitmap_idx);
				if(ac)
					nr_accesses = *ac;
				else {
					list_add(&page->lru, &l_scanned);
					continue;
				}
			}
			else
				nr_accesses = pginfo->nr_accesses;

			idx = get_idx(nr_accesses / HPAGE_PMD_NR);
			if(use_region_separation) {
				if(idx >= NR_REGION - 1) {
					i = NR_REGION - 1;	
				}
				else
					i = idx;
				region_size[i] += 1;
				nr_region_access[i] += (nr_accesses / HPAGE_PMD_NR);
			}
		}

		list_add(&page->lru, &l_scanned);
	}

	spin_lock_irq(&lruvec->lru_lock);
	move_pages_to_lru(lruvec, &l_scanned);

	__mod_node_page_state(pgdat, NR_ISOLATED_ANON, -nr_taken);
	spin_unlock_irq(&lruvec->lru_lock);

	mem_cgroup_uncharge_list(&l_scanned);
	free_unref_page_list(&l_scanned);

	return nr_taken;
}


static void scan_hotness_node(pg_data_t *pgdat, struct mem_cgroup *memcg,
				unsigned long *region_size, uint32_t *nr_region_access,
				unsigned long *lev_size)

{
	struct lruvec *lruvec = mem_cgroup_lruvec(memcg, pgdat);
	unsigned long nr_to_scan, nr_max_scan = 12, nr_scanned;
	enum lru_list lru = LRU_ACTIVE_ANON;

re_scan:
	nr_to_scan = lruvec_lru_size(lruvec, lru, MAX_NR_ZONES);
	nr_scanned = 0;
	nr_max_scan = 12;
	do {
		unsigned long scan = nr_to_scan >> 3;
		if(!scan)
			scan = nr_to_scan;
		nr_scanned += scan_hotness_lru_list(scan, lruvec, lru,
						region_size, nr_region_access, lev_size);
		nr_max_scan--;	
	} while(nr_scanned < nr_to_scan && nr_max_scan);
	
	if(lru == LRU_ACTIVE_ANON && test_bit(TRANSPARENT_HUGEPAGE_FLAG, &transparent_hugepage_flags)) {
		lru = LRU_INACTIVE_ANON;
		goto re_scan;
	}

}

static bool active_lru_overflow(struct mem_cgroup *memcg)
{
	int fmem_nid = 0;
	unsigned long fmem_active, smem_active;
	unsigned long max_dram_pages = READ_ONCE(memcg->max_nr_dram_pages);
	unsigned long max_nr_pages = (max_dram_pages > get_memcg_promotion_wmark(max_dram_pages)) ?
					max_dram_pages - get_memcg_promotion_wmark(max_dram_pages) : 0;


	fmem_active = lruvec_lru_size(mem_cgroup_lruvec(memcg, NODE_DATA(0)),
					LRU_ACTIVE_ANON, MAX_NR_ZONES);
	smem_active = lruvec_lru_size(mem_cgroup_lruvec(memcg, NODE_DATA(1)),
					LRU_ACTIVE_ANON, MAX_NR_ZONES);
		
	if(max_nr_pages < fmem_active + smem_active)
		return true;
	else
		return false;
}


static void analyze_access_pattern(struct mem_cgroup *memcg, unsigned int *hotness_scanned, bool cooling)
{
	struct mem_cgroup_per_node *pn0, *pn1;
	unsigned long tot_pages;
	unsigned long tot_huge_pages;
	unsigned int target_cooling = 0;
	int shift_factor, period_factor;
	int i;
	
	pn0 = memcg->nodeinfo[0];
	pn1 = memcg->nodeinfo[1];
	if(!pn0 || !pn1) 
		return;

	tot_pages = get_anon_rss(memcg);
	tot_huge_pages = tot_pages >> 9;

	if(test_bit(TRANSPARENT_HUGEPAGE_FLAG, &transparent_hugepage_flags)) {
		shift_factor = hugepage_shift_factor;
		period_factor = hugepage_period_factor;
	}
	else {
		shift_factor = basepage_shift_factor;
		period_factor = basepage_period_factor;
	}

	if(tot_huge_pages > (memcg->cooling_period >> shift_factor)) {
		if(*hotness_scanned > 0 &&
			use_dram_determination && 
			(use_region_separation && !memcg->region_determined)){
			// reset
			*hotness_scanned = 0;
		}
		WRITE_ONCE(memcg->cooling_period, memcg->cooling_period + MTTM_INIT_COOLING_PERIOD * period_factor);
		WRITE_ONCE(memcg->adjust_period, memcg->adjust_period + MTTM_INIT_ADJUST_PERIOD * period_factor);
	}

	target_cooling = READ_ONCE(memcg->hotness_scan_cnt);

	if(use_dram_determination &&
		(use_region_separation && !memcg->region_determined)) {
		// Skip when sample rate is not stable
		if(!READ_ONCE(memcg->stable_status) && check_stable_sample_rate) {
			*hotness_scanned = 0;
			for(i = 0; i < NR_REGION; i++) {
				memcg->region_size[i] = 0;
				memcg->nr_region_access[i] = 0;
			}

			return;
		}

		if(cooling && (*hotness_scanned < target_cooling)) {
			unsigned long lev_size[NR_REGION] = {0,};
			unsigned long region_size[NR_REGION] = {0,};
			uint32_t nr_region_access[NR_REGION] = {0,};
			unsigned long tot_region_size = 0;

			scan_hotness_node(NODE_DATA(0), memcg, region_size, nr_region_access, lev_size);
			scan_hotness_node(NODE_DATA(1), memcg, region_size, nr_region_access, lev_size);

			if(use_region_separation) {
				for(i = 1; i < NR_REGION; i++)
					tot_region_size += region_size[i];

				if((tot_region_size >> 8) > 100UL) {
					(*hotness_scanned)++;

					for(i = 0; i < NR_REGION; i++) {
						memcg->region_size[i] += region_size[i];
						memcg->nr_region_access[i] += nr_region_access[i];
					}

					if(!test_bit(TRANSPARENT_HUGEPAGE_FLAG, &transparent_hugepage_flags)) {
						memcg->region_size[0] += (lruvec_lru_size(mem_cgroup_lruvec(memcg, NODE_DATA(0)),
										LRU_INACTIVE_ANON, MAX_NR_ZONES) +
									lruvec_lru_size(mem_cgroup_lruvec(memcg, NODE_DATA(1)),
										LRU_INACTIVE_ANON, MAX_NR_ZONES));
					}

					pr_info("[%s] [ %s ] scan: %u, region size [0: %lu MB, 1: %lu MB, 2: %lu MB, 3: %lu MB, 4: %lu MB]",
						__func__, memcg->tenant_name, *hotness_scanned,
						region_size[0] >> 8, region_size[1] >> 8, region_size[2] >> 8, region_size[3] >> 8,
						region_size[4] >> 8);
					pr_info("[%s] [ %s ] scan: %u, access [0: %u, 1: %u, 2: %u, 3: %u, 4: %u]\n",
						__func__, memcg->tenant_name, *hotness_scanned,
						nr_region_access[0], nr_region_access[1], nr_region_access[2], nr_region_access[3],
						nr_region_access[4]);
				}
			}	
		}

		if(*hotness_scanned >= target_cooling) {
			if(use_region_separation) {
				for(i = 0; i < NR_REGION; i++) {
					memcg->region_size[i] /= (*hotness_scanned);
					memcg->nr_region_access[i] /= (*hotness_scanned);
				}
				
				pr_info("[%s] [ %s ] avg region size [0: %lu MB, 1: %lu MB, 2: %lu MB, 3: %lu MB, 4: %lu MB]",
					__func__, memcg->tenant_name,
					memcg->region_size[0] >> 8, memcg->region_size[1] >> 8, memcg->region_size[2] >> 8,
					memcg->region_size[3] >> 8, memcg->region_size[4] >> 8);
				pr_info("[%s] [ %s ] avg access [0: %u, 1: %u, 2: %u, 3: %u, 4: %u]\n",
					__func__, memcg->tenant_name,
					memcg->nr_region_access[0], memcg->nr_region_access[1], memcg->nr_region_access[2],
					memcg->nr_region_access[3], memcg->nr_region_access[4]);

				WRITE_ONCE(memcg->region_determined, true);
			}
		}
	}
	
}

static void increase_active_threshold(struct mem_cgroup *memcg, unsigned int active_threshold)
{
	struct mem_cgroup_per_node *pn;
	int nid;

	for_each_node_state(nid, N_MEMORY) {
		pn = memcg->nodeinfo[nid];
		if(!pn)
			continue;
		WRITE_ONCE(pn->need_adjusting, true);
	}
}

static void decrease_active_threshold(struct mem_cgroup *memcg, unsigned int active_threshold)
{
	struct mem_cgroup_per_node *pn;
	int nid;

	for_each_node_state(nid, N_MEMORY) {
		pn = memcg->nodeinfo[nid];
		if(!pn)
			continue;
		WRITE_ONCE(pn->need_adjusting_all, true);
	}
}

static void adjust_active_threshold(struct mem_cgroup *memcg)
{
	int idx_hot;
	unsigned long nr_active = 0;
	unsigned long max_nr_pages = (memcg->max_nr_dram_pages > get_memcg_promotion_wmark(memcg->max_nr_dram_pages)) ?
			memcg->max_nr_dram_pages - get_memcg_promotion_wmark(memcg->max_nr_dram_pages) : 0;
	unsigned int prev_threshold = READ_ONCE(memcg->active_threshold);
	unsigned int init_threshold = test_bit(TRANSPARENT_HUGEPAGE_FLAG, &transparent_hugepage_flags) ?
					MTTM_INIT_THRESHOLD : 9;
	struct mem_cgroup_per_node *pn;
	int nid;


	if(!test_bit(TRANSPARENT_HUGEPAGE_FLAG, &transparent_hugepage_flags) &&
		use_dram_determination &&
		(use_region_separation && !memcg->region_determined))
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
	spin_unlock(&memcg->access_lock);

	if(idx_hot < init_threshold)
		idx_hot = init_threshold;
	idx_hot += memcg->threshold_offset;

	WRITE_ONCE(memcg->active_threshold, idx_hot);
	WRITE_ONCE(memcg->warm_threshold, idx_hot);

	if(prev_threshold < idx_hot) // threshold increase
		increase_active_threshold(memcg, idx_hot);
	
	else if(prev_threshold > idx_hot) // threshold decrease
		decrease_active_threshold(memcg, idx_hot);
}


static int kmigrated(void *p)
{
	struct mem_cgroup *memcg = (struct mem_cgroup *)p;
	unsigned long tot_promoted = 0, tot_demoted = 0;
	unsigned int hotness_scanned = 0;
	unsigned int init_threshold = test_bit(TRANSPARENT_HUGEPAGE_FLAG, &transparent_hugepage_flags) ? 
					MTTM_INIT_THRESHOLD : 9;

	unsigned long total_time, total_cputime = 0, total_adjusting_cputime = 0, total_cooling_cputime = 0, total_mig_cputime = 0;
	unsigned long one_mig_cputime, one_do_mig_cputime, one_manage_cputime, one_pingpong, one_cooling_cputime;
	unsigned long stamp;
	unsigned long trace_period = msecs_to_jiffies(10000);
	unsigned long cur, interval_start;
	unsigned long interval_mig_cputime = 0, interval_do_mig_cputime = 0;
	unsigned long interval_pingpong = 0;
	unsigned long demote_cputime = 0, demote_pingpong = 0;
	unsigned long promote_cputime = 0, promote_pingpong = 0;
	unsigned int high_pingpong_cnt = 0;

	bool cooling;

	total_time = jiffies;
	interval_start = jiffies;

	for(;;) {
		struct mem_cgroup_per_node *pn0, *pn1;
		unsigned long nr_exceeded = 0;
		unsigned long hot0, cold0, hot1, cold1;		
		unsigned int raw_threshold;
		bool promotion_denied = true;

		if(kthread_should_stop())
			break;
		if(!memcg)
			break;

		pn0 = memcg->nodeinfo[0];
		pn1 = memcg->nodeinfo[1];
		if(!pn0 || !pn1) 
			break;

		cooling = false;
		if(need_lru_cooling(pn0) || need_lru_cooling(pn1))
			cooling = true;
		
		analyze_access_pattern(memcg, &hotness_scanned, cooling);
	
		one_manage_cputime = 0;
		one_cooling_cputime = 0;

		// Cooling
		if(cooling) {
			stamp = jiffies;
			if(scanless_cooling)
				scanless_cooling_node(memcg);
			one_cooling_cputime += (jiffies - stamp);
		}

		adjust_active_threshold(memcg);

		// Adjust
		stamp = jiffies;
		if(READ_ONCE(pn0->need_adjusting))
			adjusting_node(NODE_DATA(0), memcg, true, NULL, NULL, NULL);
		else if(READ_ONCE(pn0->need_adjusting_all))
			adjusting_node(NODE_DATA(0), memcg, false, NULL, NULL, NULL);

		if(READ_ONCE(pn1->need_adjusting))
			adjusting_node(NODE_DATA(1), memcg, true, NULL, NULL, NULL);
		else if(READ_ONCE(pn1->need_adjusting_all))
			adjusting_node(NODE_DATA(1), memcg, false, NULL, NULL, NULL);
		one_manage_cputime += (jiffies - stamp);


		one_mig_cputime = jiffies;
		one_do_mig_cputime = 0;
		one_pingpong = 0;		
		demote_cputime = 0;
		demote_pingpong = 0;
		promote_cputime = 0;
		promote_pingpong = 0;

		// Migration
		if(memcg->use_mig/* && !active_lru_overflow(memcg)*/) {
			if(need_fmem_demotion(NODE_DATA(0), memcg, &nr_exceeded)) {	
				tot_demoted += demote_node(NODE_DATA(0), memcg, nr_exceeded, &demote_cputime, &demote_pingpong);
				one_do_mig_cputime += demote_cputime;
				one_pingpong += demote_pingpong;
			}
			
			if(nr_promotion_target(NODE_DATA(1), memcg)) {
				tot_promoted += promote_node(NODE_DATA(1), memcg, &promotion_denied, &promote_cputime, &promote_pingpong);
				one_do_mig_cputime += promote_cputime;
				one_pingpong += promote_pingpong;
			}

			if(READ_ONCE(memcg->dram_expanded)) {
				unsigned long expanded_promoted = 0;
				promote_cputime = 0;
				expanded_promoted = promote_node_expanded(NODE_DATA(1), memcg, &promote_cputime);
				one_do_mig_cputime += promote_cputime;
	
			}

		}
			
		one_mig_cputime = jiffies - one_mig_cputime;		

		hot0 = lruvec_lru_size(mem_cgroup_lruvec(memcg, NODE_DATA(0)),
					LRU_ACTIVE_ANON, MAX_NR_ZONES);
		cold0 = lruvec_lru_size(mem_cgroup_lruvec(memcg, NODE_DATA(0)),
					LRU_INACTIVE_ANON, MAX_NR_ZONES);
		hot1 = lruvec_lru_size(mem_cgroup_lruvec(memcg, NODE_DATA(1)),
					LRU_ACTIVE_ANON, MAX_NR_ZONES);
		cold1 = lruvec_lru_size(mem_cgroup_lruvec(memcg, NODE_DATA(1)),
					LRU_INACTIVE_ANON, MAX_NR_ZONES);

		trace_lru_distribution(hot0, cold0, hot1, cold1);
		trace_migration_stats(tot_promoted, tot_demoted,
			memcg->cooling_period,
			memcg->active_threshold, memcg->warm_threshold,
			/*promotion_denied*/READ_ONCE(memcg->nodeinfo[0]->need_demotion), nr_exceeded,
			memcg->nr_sampled);


		total_cputime += (one_cooling_cputime + one_manage_cputime + one_mig_cputime);
		total_adjusting_cputime += one_manage_cputime;
		total_cooling_cputime += one_cooling_cputime;
		total_mig_cputime += one_mig_cputime;
		interval_mig_cputime += one_mig_cputime;
		interval_do_mig_cputime += one_do_mig_cputime;
		interval_pingpong += one_pingpong;

		cur = jiffies;
		if(cur - interval_start >= trace_period) {		
			if(use_pingpong_reduce && interval_mig_cputime) {
				if(interval_mig_cputime >= mig_cputime_threshold &&
					div64_u64(interval_pingpong, interval_mig_cputime) >= pingpong_reduce_threshold &&
					(test_bit(TRANSPARENT_HUGEPAGE_FLAG, &transparent_hugepage_flags) || ((!test_bit(TRANSPARENT_HUGEPAGE_FLAG, &transparent_hugepage_flags)) && ((READ_ONCE(memcg->region_determined) && use_region_separation)))) &&
					memcg->threshold_offset < pingpong_reduce_limit) {

					high_pingpong_cnt++;
					pr_info("[%s] [ %s ] Pingpong overhead high. Pingpong_pages/mig_time : %llu, threshold : %lu\n",
						__func__, memcg->tenant_name,
						div64_u64(interval_pingpong, interval_mig_cputime), pingpong_reduce_threshold);
					
					raw_threshold = memcg->active_threshold - memcg->threshold_offset;
					WRITE_ONCE(memcg->threshold_offset, memcg->threshold_offset + 1);
					WRITE_ONCE(memcg->active_threshold, raw_threshold + memcg->threshold_offset);
					WRITE_ONCE(memcg->warm_threshold, memcg->active_threshold);
					increase_active_threshold(memcg, memcg->active_threshold);
					pr_info("[%s] [ %s ] Pingpong reduced. threshold_offset : %u, active_threshold : %u\n",
						__func__, memcg->tenant_name, memcg->threshold_offset, memcg->active_threshold);
					
				}
			}

			interval_start = cur;
			interval_mig_cputime = 0;
			interval_do_mig_cputime = 0;
			interval_pingpong = 0;
		}
		
	
		wait_event_interruptible_timeout(memcg->kmigrated_wait,
				need_direct_demotion(NODE_DATA(0), memcg),
				msecs_to_jiffies(kmigrated_period_in_ms));
	}

	total_time = jiffies - total_time;
	pr_info("[%s] name : %s. tot_promoted : %lu MB, tot_demoted : %lu MB, nr_pingpong : %lu MB, block_time : %llu ns\n",
		__func__, memcg->tenant_name, tot_promoted >> 8, tot_demoted >> 8,
		memcg->nr_pingpong >> 8, READ_ONCE(memcg->block_time));
	pr_info("[%s] name : %s. total_time : %lu, total_cputime : %lu [adjusting : %lu, cooling : %lu, mig : %lu]\n",
		__func__, memcg->tenant_name, total_time, total_cputime,
		total_adjusting_cputime, total_cooling_cputime, total_mig_cputime);

	return 0;
}

int kmigrated_init(struct mem_cgroup *memcg)
{
	if(!memcg)
		return -1;
	if(memcg->kmigrated)
		return -1;

	spin_lock_init(&memcg->kmigrated_lock);
	init_waitqueue_head(&memcg->kmigrated_wait);
	memcg->kmigrated = kthread_run(kmigrated, memcg, "kmigrated%d", mem_cgroup_id(memcg));
	if(IS_ERR(memcg->kmigrated)) {
		memcg->kmigrated = NULL;
		return -1;
	}
	return 0;
}

void kmigrated_stop(struct mem_cgroup *memcg)
{
	if(memcg) {
		if(memcg->kmigrated) {
			kthread_stop(memcg->kmigrated);
			memcg->kmigrated = NULL;
		}
	}
}

void kmigrated_wakeup(struct mem_cgroup *memcg)
{
	wake_up_interruptible(&memcg->kmigrated_wait);
}
