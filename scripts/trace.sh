#!/bin/bash
EVENT=/sys/kernel/debug/tracing/events/mttm
echo 1 > ${EVENT}/lru_distribution/enable
echo 1 > ${EVENT}/migration_stats/enable

echo > ${EVENT}/../../trace
cat ${EVENT}/../../trace_pipe
