/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM	mttm

#if !defined(_TRACE_MTTM_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_MTTM_H

#include <linux/tracepoint.h>

TRACE_EVENT(lru_distribution,

	TP_PROTO(unsigned long hot0, unsigned long cold0, unsigned long hot1, unsigned long cold1),  

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

	TP_printk("Hot: %lu [%lu + %lu] Cold: %lu [%lu + %lu]\n",
			__entry->hot0 + __entry->hot1,
			__entry->hot0, __entry->hot1,
			__entry->cold0 + __entry->cold1,
			__entry->cold0, __entry->cold1)
);

#endif /* _TRACE_MTTM_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
