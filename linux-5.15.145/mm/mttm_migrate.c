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

	if(need_direct_demotion(pgdat, memcg)) {
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

static unsigned long cooling_lru_list(unsigned long nr_to_scan,
			struct lruvec *lruvec, enum lru_list lru)
{
	unsigned long nr_taken;
	struct mem_cgroup *memcg = lruvec_memcg(lruvec);
	pg_data_t *pgdat = lruvec_pgdat(lruvec);
	LIST_HEAD(l_hold);
	LIST_HEAD(l_active);
	LIST_HEAD(l_inactive);
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

		if(!file) {
			int still_hot;

			if(PageTransHuge(compound_head(page))) {
				struct page *meta_page = get_meta_page(page);

				check_transhuge_cooling((void *)memcg, page);
				if(get_idx(meta_page->nr_accesses) >= memcg->active_threshold)
					still_hot = 2;
				else
					still_hot = 1;
			}
			else {
				still_hot = cooling_page(page, lruvec_memcg(lruvec));
			}

			if(still_hot == 2) {
				// page is still hot after cooling
				if(!PageActive(page))
					SetPageActive(page);
				list_add(&page->lru, &l_active);
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
	}

	spin_lock_irq(&lruvec->lru_lock);
	move_pages_to_lru(lruvec, &l_active);
	move_pages_to_lru(lruvec, &l_inactive);
	list_splice(&l_inactive, &l_active);

	__mod_node_page_state(pgdat, NR_ISOLATED_ANON + file, -nr_taken);
	spin_unlock_irq(&lruvec->lru_lock);

	mem_cgroup_uncharge_list(&l_active);	
	free_unref_page_list(&l_active);

	return nr_taken;
}

static void cooling_node(pg_data_t *pgdat, struct mem_cgroup *memcg)
{
	unsigned long nr_to_scan, nr_scanned = 0, nr_max_scan = 12;
	struct lruvec *lruvec = mem_cgroup_lruvec(memcg, pgdat);
	struct mem_cgroup_per_node *pn = memcg->nodeinfo[pgdat->node_id];
	enum lru_list lru = LRU_ACTIVE_ANON;

re_cooling:
	nr_to_scan = lruvec_lru_size(lruvec, lru, MAX_NR_ZONES);
	do {
		unsigned long scan = nr_to_scan >> 3; // 12.5%
		if(!scan)
			scan = nr_to_scan;
		nr_scanned += cooling_lru_list(scan, lruvec, lru);
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
			lruvec, LRU_ACTIVE_FILE);

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

static void adjusting_node(pg_data_t *pgdat, struct mem_cgroup *memcg, bool active)
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

}

static int kmigrated(void *p)
{
	//TODO : cpu affinity
	int kmigrated_cpu = 2;
	const struct cpumask *cpumask = cpumask_of(kmigrated_cpu);
	struct mem_cgroup *memcg = (struct mem_cgroup *)p;
	if(!cpumask_empty(cpumask)) {
		set_cpus_allowed_ptr(memcg->kmigrated, cpumask);
		pr_info("[%s] kmigrated bind to cpu%d\n",__func__, kmigrated_cpu);
	}

	for(;;) {
		struct mem_cgroup_per_node *pn0, *pn1;
		unsigned long nr_exceeded = 0;
		unsigned long hot0, cold0, hot1, cold1;		
 
		if(kthread_should_stop())
			break;
		if(!memcg)
			break;

		pn0 = memcg->nodeinfo[0];
		pn1 = memcg->nodeinfo[1];
		if(!pn0 || !pn1) 
			break;

		
		if(need_lru_cooling(pn0))
			cooling_node(NODE_DATA(0), memcg);
		else if(need_lru_adjusting(pn0)) {
			adjusting_node(NODE_DATA(0), memcg, true);
			if(pn0->need_adjusting_all == true)
				adjusting_node(NODE_DATA(0), memcg, false);
		}

		if(need_lru_cooling(pn1))
			cooling_node(NODE_DATA(1), memcg);
		else if(need_lru_adjusting(pn1)) {
			adjusting_node(NODE_DATA(1), memcg, true);
			if(pn1->need_adjusting_all == true)
				adjusting_node(NODE_DATA(1), memcg, false);
		}
		
		/*
		if(need_fmem_demotion(pgdat, memcg, &nr_exceeded)) {
			//TODO
			demote_node(pgdat, memcg, nr_exceeded);
		}

		if(nr_promotion_target(pgdat, memcg)) {
			//TODO
			promote_node(pgdat, memcg);
		}*/

		hot0 = lruvec_lru_size(mem_cgroup_lruvec(memcg, NODE_DATA(0)),
					LRU_ACTIVE_ANON, MAX_NR_ZONES);
		cold0 = lruvec_lru_size(mem_cgroup_lruvec(memcg, NODE_DATA(0)),
					LRU_INACTIVE_ANON, MAX_NR_ZONES);
		hot1 = lruvec_lru_size(mem_cgroup_lruvec(memcg, NODE_DATA(1)),
					LRU_ACTIVE_ANON, MAX_NR_ZONES);
		cold1 = lruvec_lru_size(mem_cgroup_lruvec(memcg, NODE_DATA(1)),
					LRU_INACTIVE_ANON, MAX_NR_ZONES);

		trace_lru_distribution(hot0, cold0, hot1, cold1);

		wait_event_interruptible_timeout(memcg->kmigrated_wait,
				need_direct_demotion(NODE_DATA(0), memcg),
				msecs_to_jiffies(KMIGRATED_PERIOD_IN_MS));
	}

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
