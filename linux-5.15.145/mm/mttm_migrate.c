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
extern unsigned long classification_threshold;
extern unsigned int hotset_size_threshold;
extern unsigned int dram_size_tolerance;

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
	max_nr_pages = max_nr_pages * 2 / 100;
	if(max_nr_pages < MIN_WMARK_LOWER_LIMIT)
		return MIN_WMARK_LOWER_LIMIT;
	else if(max_nr_pages > MIN_WMARK_UPPER_LIMIT)
		return MIN_WMARK_UPPER_LIMIT;
	return max_nr_pages;
}

unsigned long get_memcg_promotion_wmark(unsigned long max_nr_pages)
{
	max_nr_pages = max_nr_pages * 3 / 100;
	if(max_nr_pages < MAX_WMARK_LOWER_LIMIT)
		return MAX_WMARK_LOWER_LIMIT;
	else if(max_nr_pages > MAX_WMARK_UPPER_LIMIT)
		return MAX_WMARK_UPPER_LIMIT;
	return max_nr_pages;
}

static unsigned long nr_promotion_target(pg_data_t *pgdat, struct mem_cgroup *memcg)
{
	struct lruvec *lruvec;
	unsigned long lruvec_size;

	lruvec = mem_cgroup_lruvec(memcg, pgdat);
	lruvec_size = lruvec_lru_size(lruvec, LRU_ACTIVE_ANON, MAX_NR_ZONES);

	return lruvec_size;
}

static bool need_direct_demotion(pg_data_t *pgdat, struct mem_cgroup *memcg)
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

	max_nr_pages = memcg->nodeinfo[pgdat->node_id]->max_nr_base_pages;
	nr_lru_pages = get_nr_lru_pages_node(memcg, pgdat);

	fmem_max_wmark = get_memcg_promotion_wmark(max_nr_pages); // if free mem is larger than this wmark, promotion allowed.
	fmem_min_wmark = get_memcg_demotion_wmark(max_nr_pages); // if free mem is less than this wmark, demotion required.

	if(need_direct_demotion(pgdat, memcg)) { // only set at mempolicy.c
		if(nr_lru_pages + fmem_max_wmark <= max_nr_pages)
			goto check_nr_need_promoted;
		else if(nr_lru_pages < max_nr_pages)
			*nr_exceeded = fmem_max_wmark - (max_nr_pages - nr_lru_pages);
		else
			*nr_exceeded = nr_lru_pages + fmem_max_wmark - max_nr_pages;
		*nr_exceeded += 1U * 128 * 100;// 100MB
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
	else
		newpage = __alloc_pages_node(nid, mask, 0);

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
								bool promotion)
{
	int target_nid = promotion ? 0 : 1;
	unsigned int nr_succeeded = 0;

	if(list_empty(migrate_list))
		return 0;
		
	migrate_pages(migrate_list, alloc_migrate_page, NULL, target_nid,
			MIGRATE_ASYNC, MR_NUMA_MISPLACED, &nr_succeeded);

	return nr_succeeded;
}

static unsigned long shrink_page_list(struct list_head *page_list, pg_data_t *pgdat,
		struct mem_cgroup *memcg, bool shrink_active, unsigned long nr_to_reclaim)
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

		if(memcg->use_warm && PageAnon(page)) {//check page is warm
			if(PageTransHuge(page)) {
				struct page *meta_page = get_meta_page(page);
				if(get_idx(meta_page->nr_accesses) >= memcg->warm_threshold)
					goto keep_locked;
			}
			else {
				unsigned int idx = get_pginfo_idx(page);
				if(idx >= memcg->warm_threshold)
					goto keep_locked;
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

	trace_shrink_page_list(nr_taken, nr_demotion_cand, nr_reclaimed);
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
			if(nr_to_scan < nr_to_reclaim)
				nr_to_scan = nr_to_reclaim * 11 / 10; // because warm pages are not demoted
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
	unsigned long nr_smem_active = nr_promotion_target(NODE_DATA(target_nid), memcg);
	unsigned long max_dram = memcg->nodeinfo[pgdat->node_id]->max_nr_base_pages;
		
	for_each_evictable_lru(lru) {
		if(!is_file_lru(lru) && is_active_lru(lru))
			continue;
		nr_evictable_pages += lruvec_lru_size(lruvec, lru, MAX_NR_ZONES);
	}

	nr_to_reclaim = nr_exceeded;
	if(nr_exceeded > nr_evictable_pages && need_direct_demotion(pgdat, memcg))
		shrink_active = true;

	do {	//nr_reclaimed starts with small number and 
		//increases exponentially by decreasing priority
		nr_reclaimed += demote_lruvec(nr_to_reclaim - nr_reclaimed, priority,
						pgdat, lruvec, shrink_active);
		if(nr_reclaimed >= nr_to_reclaim)
			break;
		priority--;
	} while (priority);
	
	//if smem active still exists, remove warm
	if(memcg->use_warm) {
		nr_smem_active = nr_smem_active < nr_to_reclaim ?
					nr_smem_active : nr_to_reclaim;
		if(nr_smem_active && nr_reclaimed < nr_smem_active)
			memcg->warm_threshold = memcg->active_threshold;
	}

	if(get_nr_lru_pages_node(memcg, pgdat) +
		get_memcg_demotion_wmark(max_dram) < max_dram)
		WRITE_ONCE(memcg->nodeinfo[pgdat->node_id]->need_demotion, false);

	return nr_reclaimed;
}


static unsigned long promote_page_list(struct list_head *page_list,
						pg_data_t *pgdat)
{
	LIST_HEAD(promote_pages);
	LIST_HEAD(ret_pages);
	unsigned long nr_promoted = 0;

	cond_resched();

	while(!list_empty(page_list)) {
		struct page *page;

		page = lru_to_page(page_list);
		list_del(&page->lru);

		if(!trylock_page(page))
			goto keep;
		if(!PageActive(page))
			goto keep_locked;
		if(unlikely(!page_evictable(page)))
			goto keep_locked;
		if(PageWriteback(page))
			goto keep_locked;
		if(PageTransHuge(page) && !thp_migration_supported())
			goto keep_locked;

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

	return nr_promoted;
}

static unsigned long promote_active_list(unsigned long nr_to_scan,
			struct lruvec *lruvec, enum lru_list lru)
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

	nr_promoted = promote_page_list(&page_list, pgdat);

	spin_lock_irq(&lruvec->lru_lock);
	move_pages_to_lru(lruvec, &page_list);
	__mod_node_page_state(pgdat, NR_ISOLATED_ANON, -nr_taken);
	spin_unlock_irq(&lruvec->lru_lock);

	mem_cgroup_uncharge_list(&page_list);
	free_unref_page_list(&page_list);

	return nr_promoted;
}


static unsigned long promote_lruvec(unsigned long nr_to_promote, short priority,
		pg_data_t *pgdat, struct lruvec *lruvec, enum lru_list lru)
{
	unsigned long nr_promoted = 0, nr;

	nr = nr_to_promote >> priority;
	if(nr)
		nr_promoted += promote_active_list(nr, lruvec, lru);

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

static bool promotion_available(int target_nid, struct mem_cgroup *memcg,
						unsigned long *nr_to_promote)
{
	pg_data_t *pgdat;
	unsigned long max_nr_pages, cur_nr_pages;
	unsigned long nr_isolated;
	unsigned long fmem_min_wmark;

	if(target_nid == NUMA_NO_NODE)
		return false;

	pgdat = NODE_DATA(target_nid);

	cur_nr_pages = get_nr_lru_pages_node(memcg, pgdat);
	max_nr_pages = memcg->nodeinfo[target_nid]->max_nr_base_pages;
	//isolated on node vs isolated on memcg .. at multi tenants
	//demotion & promotion are coupled, so nr_isolated may be close to 0.
	nr_isolated = node_page_state(pgdat, NR_ISOLATED_ANON) +
			node_page_state(pgdat, NR_ISOLATED_FILE);

	fmem_min_wmark = get_memcg_demotion_wmark(max_nr_pages);

	if(max_nr_pages == ULONG_MAX) {
		*nr_to_promote = node_free_pages(pgdat);
		return true;
	}
	else if(cur_nr_pages + nr_isolated < max_nr_pages - fmem_min_wmark) {
		*nr_to_promote = max_nr_pages - fmem_min_wmark - cur_nr_pages - nr_isolated;
		return true;
	}

	return false;
}

static unsigned long promote_node(pg_data_t *pgdat, struct mem_cgroup *memcg, bool *promotion_denied)
{
	struct lruvec *lruvec = mem_cgroup_lruvec(memcg, pgdat);
	unsigned long nr_to_promote, nr_promoted = 0, tmp;
	enum lru_list lru = LRU_ACTIVE_ANON;
	short priority = DEF_PRIORITY;
	int target_nid = 0;

	if(!promotion_available(target_nid, memcg, &nr_to_promote))
		return 0;

	*promotion_denied = false;
	nr_to_promote = min(nr_to_promote,
			lruvec_lru_size(lruvec, lru, MAX_NR_ZONES));

	do {
		nr_promoted += promote_lruvec(nr_to_promote, priority,
						pgdat, lruvec, lru);
		if(nr_promoted >= nr_to_promote)
			break;
		priority--;
	} while (priority);

	return nr_promoted;
}

static unsigned long cooling_lru_list(unsigned long nr_to_scan, struct lruvec *lruvec,
	enum lru_list lru, unsigned long *nr_active_cooled, unsigned long *nr_active_still_hot)
{
	unsigned long nr_taken;
	struct mem_cgroup *memcg = lruvec_memcg(lruvec);
	pg_data_t *pgdat = lruvec_pgdat(lruvec);
	LIST_HEAD(l_hold);
	LIST_HEAD(l_active);
	LIST_HEAD(l_inactive);
	int file = is_file_lru(lru);
	unsigned long nr_cooled = 0, nr_still_hot = 0;

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

		if(!file) {
			int still_hot;

			if(PageTransHuge(compound_head(page))) {
				struct page *meta_page = get_meta_page(page);

				check_transhuge_cooling_reset((void *)memcg, page);
				if(get_idx(meta_page->nr_accesses) >= memcg->active_threshold)
					still_hot = 2;
				else {
					still_hot = 1;
				}
			}
			else {
				still_hot = cooling_page(page, lruvec_memcg(lruvec));
			}

			if(still_hot == 2) {
				// page is still hot after cooling
				if(!PageActive(page))
					SetPageActive(page);
				list_add(&page->lru, &l_active);
				if(lru == LRU_ACTIVE_ANON)
					nr_still_hot += PageTransHuge(compound_head(page)) ? HPAGE_PMD_NR : 1;
				continue;
			}
			else if(still_hot == 0) {
				// not cooled due to page clock is same with memcg clock
				if(PageActive(page))
					list_add(&page->lru, &l_active);
				else
					list_add(&page->lru, &l_inactive);
				continue;
			}
		}
		// Cold or file page
		ClearPageActive(page);
		SetPageWorkingset(page);
		list_add(&page->lru, &l_inactive);
		if(lru == LRU_ACTIVE_ANON)
			nr_cooled += PageTransHuge(compound_head(page)) ? HPAGE_PMD_NR : 1;
	}

	spin_lock_irq(&lruvec->lru_lock);
	move_pages_to_lru(lruvec, &l_active);
	move_pages_to_lru(lruvec, &l_inactive);
	list_splice(&l_inactive, &l_active);

	__mod_node_page_state(pgdat, NR_ISOLATED_ANON + file, -nr_taken);
	spin_unlock_irq(&lruvec->lru_lock);

	mem_cgroup_uncharge_list(&l_active);	
	free_unref_page_list(&l_active);

	if(nr_active_cooled)
		*nr_active_cooled = nr_cooled;
	if(nr_active_still_hot)
		*nr_active_still_hot = nr_still_hot;

	return nr_taken;
}

static void cooling_node(pg_data_t *pgdat, struct mem_cgroup *memcg,
		unsigned long *nr_cooled, unsigned long *nr_still_hot)
{
	unsigned long nr_to_scan, nr_scanned = 0, nr_max_scan = 12;
	struct lruvec *lruvec = mem_cgroup_lruvec(memcg, pgdat);
	struct mem_cgroup_per_node *pn = memcg->nodeinfo[pgdat->node_id];
	enum lru_list lru = LRU_ACTIVE_ANON;
	unsigned long nr_active_cooled = 0, nr_active_still_hot = 0;

re_cooling:
	nr_to_scan = lruvec_lru_size(lruvec, lru, MAX_NR_ZONES);
	do {
		unsigned long scan = nr_to_scan >> 3; // 12.5%
		if(!scan)
			scan = nr_to_scan;
		nr_scanned += cooling_lru_list(scan, lruvec, lru,
					&nr_active_cooled, &nr_active_still_hot);
		if(lru == LRU_ACTIVE_ANON) {
			if(nr_cooled)
				*nr_cooled += nr_active_cooled;
			if(nr_still_hot)
				*nr_still_hot += nr_active_still_hot;
		}
		nr_max_scan--;
	} while (nr_scanned < nr_to_scan && nr_max_scan);

	if(is_active_lru(lru)) {
		lru = LRU_INACTIVE_ANON;
		nr_max_scan = 12;
		nr_scanned = 0;
		goto re_cooling;
	}

	// active file list
	cooling_lru_list(lruvec_lru_size(lruvec, LRU_ACTIVE_FILE, MAX_NR_ZONES),
			lruvec, LRU_ACTIVE_FILE, NULL, NULL);

	WRITE_ONCE(pn->need_cooling, false);
}

static unsigned long adjusting_lru_list(unsigned long nr_to_scan, struct lruvec *lruvec,
					enum lru_list lru)
{
	unsigned long nr_taken;
	pg_data_t *pgdat = lruvec_pgdat(lruvec);
	struct mem_cgroup *memcg = lruvec_memcg(lruvec);
	LIST_HEAD(l_hold);
	LIST_HEAD(l_active);
	LIST_HEAD(l_inactive);
	int file = is_file_lru(lru);
	bool active = is_active_lru(lru);

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

			if(get_idx(meta_page->nr_accesses) >= memcg->active_threshold)
				status = 2;
			else
				status = 1;
		}
		else {
			status = page_check_hotness(page, memcg);
		}

		if(status == 2) {
			if(active) {
				list_add(&page->lru, &l_active);
				continue;
			}
			SetPageActive(page);
			list_add(&page->lru, &l_active);
		}
		else if (status == 0) {
			if(PageActive(page))
				list_add(&page->lru, &l_active);
			else
				list_add(&page->lru, &l_inactive);
		}
		else {
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

	return nr_taken;
}

static void adjusting_node(pg_data_t *pgdat, struct mem_cgroup *memcg, bool active, unsigned long *nr_adjusted)
{
	struct lruvec *lruvec = mem_cgroup_lruvec(memcg, pgdat);
	struct mem_cgroup_per_node *pn = memcg->nodeinfo[pgdat->node_id];
	enum lru_list lru = active ? LRU_ACTIVE_ANON : LRU_INACTIVE_ANON;
	unsigned long nr_to_scan, nr_scanned = 0, nr_max_scan = 12;
	unsigned int nr_pages = 0;

	nr_to_scan = lruvec_lru_size(lruvec, lru, MAX_NR_ZONES);
	do {
		unsigned long scan = nr_to_scan >> 3;
		if(!scan)
			scan = nr_to_scan;
		nr_scanned += adjusting_lru_list(scan, lruvec, lru);
		nr_max_scan--;
	} while(nr_scanned < nr_to_scan && nr_max_scan);

	if(nr_scanned >= nr_to_scan)
		WRITE_ONCE(pn->need_adjusting, false);
	if(nr_scanned >= nr_to_scan && !active)
		WRITE_ONCE(pn->need_adjusting_all, false);
	
	if(nr_adjusted)
		*nr_adjusted = nr_scanned;
}

static bool active_lru_overflow(struct mem_cgroup *memcg)
{
	int fmem_nid = 0;
	unsigned long fmem_active, smem_active;
	unsigned long max_nr_pages = memcg->max_nr_dram_pages -
		get_memcg_promotion_wmark(memcg->max_nr_dram_pages);


	fmem_active = lruvec_lru_size(mem_cgroup_lruvec(memcg, NODE_DATA(0)),
					LRU_ACTIVE_ANON, MAX_NR_ZONES);
	smem_active = lruvec_lru_size(mem_cgroup_lruvec(memcg, NODE_DATA(1)),
					LRU_ACTIVE_ANON, MAX_NR_ZONES);
		
	if(max_nr_pages < fmem_active + smem_active)
		return true;
	else
		return false;
}


static int kmigrated(void *p)
{
	//TODO : cpu affinity
	//int kmigrated_cpu = 2;
	//const struct cpumask *cpumask = cpumask_of(kmigrated_cpu);
	struct mem_cgroup *memcg = (struct mem_cgroup *)p;
	unsigned long tot_promoted = 0, tot_demoted = 0;
	unsigned long strong_hot_size = 0;
	unsigned int strong_hot_checked = 0;
	unsigned long min_hot_size, max_hot_size;
	unsigned int active_lru_overflow_cnt = 0;

	/*
	if(!cpumask_empty(cpumask)) {
		set_cpus_allowed_ptr(memcg->kmigrated, cpumask);
		pr_info("[%s] kmigrated bind to cpu%d\n",__func__, kmigrated_cpu);
	}*/

	for(;;) {
		struct mem_cgroup_per_node *pn0, *pn1;
		unsigned long nr_exceeded = 0;
		unsigned long hot0, cold0, hot1, cold1;		
		unsigned long tot_nr_adjusted = 0, nr_adjusted_active = 0, nr_adjusted_inactive = 0;
		unsigned long tot_nr_cooled = 0, tot_nr_cool_failed = 0, nr_cooled = 0, nr_still_hot = 0;
		bool promotion_denied = true;

		if(kthread_should_stop())
			break;
		if(!memcg)
			break;

		pn0 = memcg->nodeinfo[0];
		pn1 = memcg->nodeinfo[1];
		if(!pn0 || !pn1) 
			break;

		if(use_dram_determination) {
			// Classify workload type
			if(memcg->workload_type == NOT_CLASSIFIED) {
				if((need_lru_cooling(pn0) || need_lru_cooling(pn1)) &&
					(strong_hot_checked < WORKLOAD_TYPE_CLASSIFICATION_COOLING)) {
					hot0 = lruvec_lru_size(mem_cgroup_lruvec(memcg, NODE_DATA(0)),
						LRU_ACTIVE_ANON, MAX_NR_ZONES);
					hot1 = lruvec_lru_size(mem_cgroup_lruvec(memcg, NODE_DATA(1)),
						LRU_ACTIVE_ANON, MAX_NR_ZONES);
					strong_hot_size += (hot0 + hot1);
					strong_hot_checked++;
					pr_info("[%s] workload classification. strong_hot_size : %lu MB\n",
						__func__, (hot0 + hot1) >> 8);
				}

				if(strong_hot_checked >= WORKLOAD_TYPE_CLASSIFICATION_COOLING) {
					unsigned long classification_threshold_in_size;
					unsigned long tot_pages = get_nr_lru_pages_node(memcg, NODE_DATA(0))
								+ get_nr_lru_pages_node(memcg, NODE_DATA(1));

					classification_threshold_in_size = (tot_pages * classification_threshold) / 1000;
					if(classification_threshold_in_size < MIN_CLASSIFICATION_THRESHOLD)
						classification_threshold_in_size = MIN_CLASSIFICATION_THRESHOLD;
					else if(classification_threshold_in_size > MAX_CLASSIFICATION_THRESHOLD)
						classification_threshold_in_size = MAX_CLASSIFICATION_THRESHOLD;

					strong_hot_size = strong_hot_size / strong_hot_checked;
					pr_info("[%s] workload classification. average strong_hot_size : %lu MB, classification threshold : %lu MB\n",
						__func__, strong_hot_size >> 8, classification_threshold_in_size >> 8);
					
					if(strong_hot_size >= classification_threshold_in_size) {
						WRITE_ONCE(memcg->workload_type, STRONG_HOT);
						WRITE_ONCE(memcg->cooling_period, MTTM_DRAM_DETER_COOLING_PERIOD);
						WRITE_ONCE(memcg->active_threshold, hotset_size_threshold);
						WRITE_ONCE(memcg->warm_threshold, hotset_size_threshold);
						pr_info("[%s] strong hot workload\n",__func__);
					}
					else {
						//TODO : weak hot dram determination
						WRITE_ONCE(memcg->workload_type, WEAK_HOT);
						WRITE_ONCE(memcg->cooling_period, MTTM_STABLE_COOLING_PERIOD);
						WRITE_ONCE(memcg->adjust_period, MTTM_STABLE_ADJUST_PERIOD);
						WRITE_ONCE(memcg->dram_determined, true);
						pr_info("[%s] weak hot workload\n",__func__);
					}
					strong_hot_size = 0;
					strong_hot_checked = 0;
				}
			}

			// Measure strong hot set size
			else if(memcg->workload_type == STRONG_HOT && !READ_ONCE(memcg->dram_determined)) {
				if((need_lru_cooling(pn0) || need_lru_cooling(pn1)) &&
					(strong_hot_checked < WORKLOAD_DRAM_DETERMINATION_COOLING)) {
					hot0 = lruvec_lru_size(mem_cgroup_lruvec(memcg, NODE_DATA(0)),
						LRU_ACTIVE_ANON, MAX_NR_ZONES);
					hot1 = lruvec_lru_size(mem_cgroup_lruvec(memcg, NODE_DATA(1)),
						LRU_ACTIVE_ANON, MAX_NR_ZONES);
					strong_hot_size += (hot0 + hot1);

					if(strong_hot_checked == 0) {
						min_hot_size = hot0 + hot1;
						max_hot_size = hot0 + hot1;
					}
					else {
						if(hot0 + hot1 < min_hot_size)
							min_hot_size = hot0 + hot1;
						else if(hot0 + hot1 > max_hot_size)
							max_hot_size = hot0 + hot1;

						if(max_hot_size > 4*min_hot_size) {
							pr_info("[%s] unstable hot size. min : %lu MB, max : %lu MB. weak hot workload\n",
								__func__, min_hot_size >> 8, max_hot_size >> 8);

							WRITE_ONCE(memcg->workload_type, WEAK_HOT);
							WRITE_ONCE(memcg->cooling_period, MTTM_STABLE_COOLING_PERIOD);
							WRITE_ONCE(memcg->adjust_period, MTTM_STABLE_ADJUST_PERIOD);
							WRITE_ONCE(memcg->dram_determined, true);
							goto dram_deter_end;
						}

					}

					strong_hot_checked++;
					pr_info("[%s] DRAM determination. strong_hot_size : %lu MB\n",__func__, (hot0 + hot1) >> 8);
				}

				if(strong_hot_checked >= WORKLOAD_DRAM_DETERMINATION_COOLING) {
					strong_hot_size = strong_hot_size / strong_hot_checked;
					pr_info("[%s] average strong_hot_size : %lu MB, final : %lu MB\n",
						__func__, strong_hot_size >> 8, (strong_hot_size * (100 + dram_size_tolerance) / 100)>>8);
					
					WRITE_ONCE(memcg->cooling_period, MTTM_STABLE_COOLING_PERIOD);
					WRITE_ONCE(memcg->adjust_period, MTTM_STABLE_ADJUST_PERIOD);
					WRITE_ONCE(memcg->nodeinfo[0]->max_nr_base_pages, strong_hot_size * (100 + dram_size_tolerance) / 100);
					WRITE_ONCE(memcg->max_nr_dram_pages, strong_hot_size * (100 + dram_size_tolerance) / 100);
					WRITE_ONCE(memcg->dram_determined, true);
				}
			}
		}

dram_deter_end:
		// Cool & adjust node 0
		if(need_lru_cooling(pn0)) {
			nr_cooled = 0;
			nr_still_hot = 0;
			cooling_node(NODE_DATA(0), memcg, &nr_cooled, &nr_still_hot);
			tot_nr_cooled += nr_cooled;
			tot_nr_cool_failed += nr_still_hot;
		}
		else if(need_lru_adjusting(pn0)) {
			adjusting_node(NODE_DATA(0), memcg, true, &nr_adjusted_active);
			if(pn0->need_adjusting_all == true)
				adjusting_node(NODE_DATA(0), memcg, false, &nr_adjusted_inactive);
		}
		tot_nr_adjusted += nr_adjusted_active + nr_adjusted_inactive;

		// Cool & adjust node 1
		if(need_lru_cooling(pn1)) {
			nr_cooled = 0;
			nr_still_hot = 0;
			cooling_node(NODE_DATA(1), memcg, &nr_cooled, &nr_still_hot);
			tot_nr_cooled += nr_cooled;
			tot_nr_cool_failed += nr_still_hot;
		}
		else if(need_lru_adjusting(pn1)) {
			adjusting_node(NODE_DATA(1), memcg, true, &nr_adjusted_active);
			if(pn1->need_adjusting_all == true)
				adjusting_node(NODE_DATA(1), memcg, false, &nr_adjusted_inactive);
		}
		tot_nr_adjusted += nr_adjusted_active + nr_adjusted_inactive;

		// Handle active lru overflow
		if(!use_dram_determination || (use_dram_determination && memcg->dram_determined)) {
			if(active_lru_overflow(memcg)) {
				// It may not fix the active lru overflow immediately.
				unsigned long fmem_active, smem_active, nr_active_cur;
				unsigned long max_nr_pages = memcg->max_nr_dram_pages -
							get_memcg_promotion_wmark(memcg->max_nr_dram_pages);

				WRITE_ONCE(memcg->hg_mismatch, true);
			
				memcg->active_threshold++;
				adjusting_node(NODE_DATA(0), memcg, true, &nr_adjusted_active);
				tot_nr_adjusted += nr_adjusted_active;
				adjusting_node(NODE_DATA(1), memcg, true, &nr_adjusted_active);
				tot_nr_adjusted += nr_adjusted_active;

				fmem_active = lruvec_lru_size(mem_cgroup_lruvec(memcg, NODE_DATA(0)),
						LRU_ACTIVE_ANON, MAX_NR_ZONES);
				smem_active = lruvec_lru_size(mem_cgroup_lruvec(memcg, NODE_DATA(1)),
						LRU_ACTIVE_ANON, MAX_NR_ZONES);
				nr_active_cur = fmem_active + smem_active;

				if(nr_active_cur < (max_nr_pages * 75 / 100)) 
					memcg->warm_threshold = memcg->active_threshold - 1;
				else
					memcg->warm_threshold = memcg->active_threshold;
				active_lru_overflow_cnt++;
				pr_info("[%s] active_lru_overflow_cnt : %u\n",__func__, active_lru_overflow_cnt);
			}
		}

		// Migration
		if(memcg->use_mig && !active_lru_overflow(memcg)) {
			if(need_fmem_demotion(NODE_DATA(0), memcg, &nr_exceeded)) {
				if(memcg->use_warm && (READ_ONCE(memcg->warm_threshold) == 0)) {
					goto skip_migration;
				}
				tot_demoted += demote_node(NODE_DATA(0), memcg, nr_exceeded);
			}
			
			if(nr_promotion_target(NODE_DATA(1), memcg)) {
				tot_promoted += promote_node(NODE_DATA(1), memcg, &promotion_denied);
				if(promotion_denied) {
					//WRITE_ONCE(memcg->need_adjusting_from_kmigrated, true);
				}
			}
		}	
skip_migration:

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
			memcg->cooling_clock, memcg->cooling_period,
			memcg->active_threshold, memcg->warm_threshold,
			tot_nr_adjusted, promotion_denied, nr_exceeded,
			memcg->nr_sampled, memcg->nr_load, memcg->nr_store);

		wait_event_interruptible_timeout(memcg->kmigrated_wait,
				need_direct_demotion(NODE_DATA(0), memcg),
				msecs_to_jiffies(KMIGRATED_PERIOD_IN_MS));
	}

	memcg->promoted_pages = tot_promoted;
	memcg->demoted_pages = tot_demoted;

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
