#!/bin/bash
EVENT=/sys/kernel/debug/tracing/events/mttm
echo 1 > ${EVENT}/lru_distribution/enable
echo 1 > ${EVENT}/migration_stats/enable
echo 0 > ${EVENT}/lru_stats/enable
echo 0 > ${EVENT}/page_check_hotness/enable
echo 0 > ${EVENT}/first_touch/enable
echo 0 > ${EVENT}/alloc_pginfo/enable
#echo 'is_hot==2' > ${EVENT}/page_check_hotness/filter

echo > ${EVENT}/../../trace
cat ${EVENT}/../../trace_pipe
