/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM	mttm

#if !defined(_TRACE_MTTM_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_MTTM_H

#include <linux/tracepoint.h>

TRACE_EVENT(lru_distribution,

	TP_PROTO(unsigned long hot0, unsigned long cold0,
		unsigned long hot1, unsigned long cold1),  

	TP_ARGS(hot0, cold0, hot1, cold1),

	TP_STRUCT__entry(
		__field(unsigned long,	hot0)
		__field(unsigned long,	cold0)
		__field(unsigned long,	hot1)
		__field(unsigned long,	cold1)
	),

	TP_fast_assign(
		__entry->hot0 = hot0;
		__entry->cold0 = cold0;
		__entry->hot1 = hot1;
		__entry->cold1 = cold1;
	),

	TP_printk("Hot: %lu MB [%lu + %lu] Cold: %lu MB [%lu + %lu]",
			(__entry->hot0 + __entry->hot1) >> 8,
			__entry->hot0 >> 8, __entry->hot1 >> 8,
			(__entry->cold0 + __entry->cold1) >> 8,
			__entry->cold0 >> 8, __entry->cold1 >> 8)
);

TRACE_EVENT(migration_stats,

	TP_PROTO(unsigned long promoted, unsigned long demoted,
		unsigned int cooling_clock, unsigned long cooling_period,
		unsigned int active_threshold, unsigned int warm_threshold,
		bool promotion_denied,
		unsigned long nr_exceeded, unsigned long nr_sampled,
		unsigned long nr_load, unsigned long nr_store),  

	TP_ARGS(promoted, demoted, cooling_clock, cooling_period, active_threshold, warm_threshold, promotion_denied, nr_exceeded, nr_sampled, nr_load, nr_store),

	TP_STRUCT__entry(
		__field(unsigned long,	promoted)
		__field(unsigned long,	demoted)
		__field(unsigned int,	cooling_clock)
		__field(unsigned long,	cooling_period)
		__field(unsigned int,	active_threshold)
		__field(unsigned int,	warm_threshold)
		__field(bool,		promotion_denied)
		__field(unsigned long,	nr_exceeded)
		__field(unsigned long,	nr_sampled)
		__field(unsigned long,	nr_load)
		__field(unsigned long,	nr_store)
	),

	TP_fast_assign(
		__entry->promoted = promoted;
		__entry->demoted = demoted;
		__entry->cooling_clock = cooling_clock;
		__entry->cooling_period = cooling_period;
		__entry->active_threshold = active_threshold;
		__entry->warm_threshold = warm_threshold;
		__entry->promotion_denied = promotion_denied;
		__entry->nr_exceeded = nr_exceeded;
		__entry->nr_sampled = nr_sampled;
		__entry->nr_load = nr_load;
		__entry->nr_store = nr_store;
	),

	TP_printk("Promoted: %lu MB Demoted: %lu MB Cooling [period : %lu, clock: %u] active: %u warm: %u nr_exceeded: %lu promotion [%s] nr_sampled: %lu (load : %lu, store : %lu)",
		__entry->promoted >> 8, __entry->demoted >> 8,
		__entry->cooling_period, __entry->cooling_clock,
		__entry->active_threshold, __entry->warm_threshold,
		__entry->nr_exceeded, __entry->promotion_denied ? "denied" : "available",
		__entry->nr_sampled, __entry->nr_load, __entry->nr_store)
);

TRACE_EVENT(lru_stats,

	TP_PROTO(unsigned long tot_nr_adjusted,
		unsigned long tot_nr_to_active, unsigned long tot_nr_to_inactive,
		unsigned long tot_nr_cooled),  

	TP_ARGS(tot_nr_adjusted, tot_nr_to_active, tot_nr_to_inactive, tot_nr_cooled),

	TP_STRUCT__entry(
		__field(unsigned long,	tot_nr_adjusted)
		__field(unsigned long,	tot_nr_to_active)
		__field(unsigned long,	tot_nr_to_inactive)
		__field(unsigned long,	tot_nr_cooled)
	),

	TP_fast_assign(
		__entry->tot_nr_adjusted = tot_nr_adjusted;
		__entry->tot_nr_to_active = tot_nr_to_active;
		__entry->tot_nr_to_inactive = tot_nr_to_inactive;
		__entry->tot_nr_cooled = tot_nr_cooled;
	),

	TP_printk("Cooled %lu Adjusted %lu [To_active %lu, To_inactive %lu]",
		__entry->tot_nr_cooled, __entry->tot_nr_adjusted, __entry->tot_nr_to_active,
		__entry->tot_nr_to_inactive)
);


TRACE_EVENT(hotness_hg_1,

	TP_PROTO(unsigned long lv0, unsigned long lv1, unsigned long lv2, unsigned long lv3,
		unsigned long lv4, unsigned long lv5, unsigned long lv6, unsigned long lv7),

	TP_ARGS(lv0, lv1, lv2, lv3, lv4, lv5, lv6, lv7),

	TP_STRUCT__entry(
		__field(unsigned long,	lv0)
		__field(unsigned long,	lv1)
		__field(unsigned long,	lv2)
		__field(unsigned long,	lv3)
		__field(unsigned long,	lv4)
		__field(unsigned long,	lv5)
		__field(unsigned long,	lv6)
		__field(unsigned long,	lv7)
	),

	TP_fast_assign(
		__entry->lv0 = lv0;
		__entry->lv1 = lv1;
		__entry->lv2 = lv2;
		__entry->lv3 = lv3;
		__entry->lv4 = lv4;
		__entry->lv5 = lv5;
		__entry->lv6 = lv6;
		__entry->lv7 = lv7;
	),

	TP_printk("hotness_hg 0 ~ 7 [MB] [%lu, %lu, %lu, %lu, %lu, %lu, %lu, %lu]",
		__entry->lv0 >> 8, __entry->lv1 >> 8, __entry->lv2 >> 8, __entry->lv3 >> 8,
		__entry->lv4 >> 8, __entry->lv5 >> 8, __entry->lv6 >> 8, __entry->lv7 >> 8)
);

TRACE_EVENT(hotness_hg_2,

	TP_PROTO(unsigned long lv8, unsigned long lv9, unsigned long lv10, unsigned long lv11,
		unsigned long lv12, unsigned long lv13, unsigned long lv14, unsigned long lv15),

	TP_ARGS(lv8, lv9, lv10, lv11, lv12, lv13, lv14, lv15),

	TP_STRUCT__entry(
		__field(unsigned long,	lv8)
		__field(unsigned long,	lv9)
		__field(unsigned long,	lv10)
		__field(unsigned long,	lv11)
		__field(unsigned long,	lv12)
		__field(unsigned long,	lv13)
		__field(unsigned long,	lv14)
		__field(unsigned long,	lv15)
	),

	TP_fast_assign(
		__entry->lv8 = lv8;
		__entry->lv9 = lv9;
		__entry->lv10 = lv10;
		__entry->lv11 = lv11;
		__entry->lv12 = lv12;
		__entry->lv13 = lv13;
		__entry->lv14 = lv14;
		__entry->lv15 = lv15;
	),

	TP_printk("hotness_hg 8 ~ 15 [MB] [%lu, %lu, %lu, %lu, %lu, %lu, %lu, %lu]",
		__entry->lv8 >> 8, __entry->lv9 >> 8, __entry->lv10 >> 8, __entry->lv11 >> 8,
		__entry->lv12 >> 8, __entry->lv13 >> 8, __entry->lv14 >> 8, __entry->lv15 >> 8)
);

TRACE_EVENT(shrink_page_list,

	TP_PROTO(unsigned long nr_isolated, unsigned long nr_demotion_cand, unsigned long nr_reclaimed),

	TP_ARGS(nr_isolated, nr_demotion_cand, nr_reclaimed),

	TP_STRUCT__entry(
		__field(unsigned long,	nr_isolated)
		__field(unsigned long,	nr_demotion_cand)
		__field(unsigned long,	nr_reclaimed)
	),

	TP_fast_assign(
		__entry->nr_isolated = nr_isolated;
		__entry->nr_demotion_cand = nr_demotion_cand;
		__entry->nr_reclaimed = nr_reclaimed;
	),

	TP_printk("nr_isolated: %lu nr_demotion_cand: %lu nr_reclaimed: %lu",
		__entry->nr_isolated, __entry->nr_demotion_cand, __entry->nr_reclaimed)
);

TRACE_EVENT(migrate_fail,

	TP_PROTO(unsigned long nr_enosys, unsigned long nr_enomem, unsigned long nr_eagain, unsigned long nr_others),

	TP_ARGS(nr_enosys, nr_enomem, nr_eagain, nr_others),

	TP_STRUCT__entry(
		__field(unsigned long,	nr_enosys)
		__field(unsigned long,	nr_enomem)
		__field(unsigned long,	nr_eagain)
		__field(unsigned long,	nr_others)
	),

	TP_fast_assign(
		__entry->nr_enosys = nr_enosys;
		__entry->nr_enomem = nr_enomem;
		__entry->nr_eagain = nr_eagain;
		__entry->nr_others = nr_others;
	),

	TP_printk("nr_ENOSYS: %lu nr_ENOMEM: %lu nr_EAGAIN: %lu nr_OTHERS: %lu",
		__entry->nr_enosys, __entry->nr_enomem, __entry->nr_eagain, __entry->nr_others)
);


TRACE_EVENT(lru_move,

	TP_PROTO(bool to_active, int nr_pages, unsigned long lru_size),

	TP_ARGS(to_active, nr_pages, lru_size),

	TP_STRUCT__entry(
		__field(bool,	to_active)
		__field(int,	nr_pages)
		__field(unsigned long,	lru_size)
	),

	TP_fast_assign(
		__entry->to_active = to_active;
		__entry->nr_pages = nr_pages;
		__entry->lru_size = lru_size;
	),

	TP_printk("%s. nr_pages: %d lru_size: %lu",
		__entry->to_active ? "to_active" : "to_inactive",
		__entry->nr_pages, __entry->lru_size)
);

TRACE_EVENT(lru_size,

	TP_PROTO(bool active, unsigned long prev_size, unsigned long cur_size),

	TP_ARGS(active, prev_size, cur_size),

	TP_STRUCT__entry(
		__field(bool,	active)
		__field(unsigned long,	prev_size)
		__field(unsigned long,	cur_size)
	),

	TP_fast_assign(
		__entry->active = active;
		__entry->prev_size = prev_size;
		__entry->cur_size = cur_size;
	),

	TP_printk("%s. prev_lru_size: %lu --> cur_lru_size: %lu",
		__entry->active ? "active" : "inactive",
		__entry->prev_size, __entry->cur_size)
);

TRACE_EVENT(page_check_hotness,

	TP_PROTO(unsigned long address, unsigned long page,
		unsigned long pte, unsigned long pte_val,
		unsigned long pte_page, unsigned long pginfo,
		unsigned int cur_idx, unsigned int active_threshold, int is_hot),

	TP_ARGS(address, page, pte, pte_val, pte_page, pginfo, cur_idx, active_threshold, is_hot),

	TP_STRUCT__entry(
		__field(unsigned long,	address)
		__field(unsigned long,	page)	
		__field(unsigned long,	pte)
		__field(unsigned long,	pte_val)
		__field(unsigned long,	pte_page)
		__field(unsigned long,	pginfo)
		__field(unsigned int,	cur_idx)
		__field(unsigned int,	active_threshold)
		__field(int,		is_hot)
	),

	TP_fast_assign(
		__entry->address = address;
		__entry->page = page;
		__entry->pte = pte;
		__entry->pte_val = pte_val;
		__entry->pte_page = pte_page;
		__entry->pginfo = pginfo;
		__entry->cur_idx = cur_idx;
		__entry->active_threshold = active_threshold;
		__entry->is_hot = is_hot;
	),

	TP_printk("address 0x%lx page 0x%lx pte [ptr: 0x%lx, val: 0x%lx, page: 0x%lx] pginfo 0x%lx cur_idx %u active_threshold %u is_hot %d",
		__entry->address, __entry->page,
		__entry->pte, __entry->pte_val, __entry->pte_page,
		__entry->pginfo,
		__entry->cur_idx, __entry->active_threshold, __entry->is_hot)
);

TRACE_EVENT(first_touch,

	TP_PROTO(unsigned long address, unsigned long page, unsigned long pte, unsigned long pginfo),

	TP_ARGS(address, page, pte, pginfo),

	TP_STRUCT__entry(
		__field(unsigned long,	address)
		__field(unsigned long,	page)
		__field(unsigned long,	pte)
		__field(unsigned long,	pginfo)
	),

	TP_fast_assign(
		__entry->address = address;
		__entry->page = page;
		__entry->pte = pte;
		__entry->pginfo = pginfo;
	),

	TP_printk("address 0x%lx page 0x%lx pte 0x%lx pginfo 0x%lx",
		__entry->address, __entry->page, __entry->pte, __entry->pginfo)
);

TRACE_EVENT(alloc_pginfo,

	TP_PROTO(unsigned long pte_page, unsigned long pginfo),

	TP_ARGS(pte_page, pginfo),

	TP_STRUCT__entry(
		__field(unsigned long,	pte_page)
		__field(unsigned long,	pginfo)
	),

	TP_fast_assign(
		__entry->pte_page = pte_page;
		__entry->pginfo = pginfo;
	),

	TP_printk("pte_page 0x%lx pginfo 0x%lx",
		__entry->pte_page, __entry->pginfo)
);

#endif /* _TRACE_MTTM_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
