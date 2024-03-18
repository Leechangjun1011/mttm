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
		unsigned int cooling_clock, unsigned int active_threshold, unsigned int warm_threshold),  

	TP_ARGS(promoted, demoted, cooling_clock, active_threshold, warm_threshold),

	TP_STRUCT__entry(
		__field(unsigned long,	promoted)
		__field(unsigned long,	demoted)
		__field(unsigned int,	cooling_clock)
		__field(unsigned int,	active_threshold)
		__field(unsigned int,	warm_threshold)
	),

	TP_fast_assign(
		__entry->promoted = promoted;
		__entry->demoted = demoted;
		__entry->cooling_clock = cooling_clock;
		__entry->active_threshold = active_threshold;
		__entry->warm_threshold = warm_threshold;
	),

	TP_printk("Promoted: %lu MB Demoted: %lu MB, cooling_clock: %u active_threshold: %u warm_threshold: %u",
		__entry->promoted >> 8, __entry->demoted >> 8,
		__entry->cooling_clock, __entry->active_threshold, __entry->warm_threshold)
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
#endif /* _TRACE_MTTM_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
