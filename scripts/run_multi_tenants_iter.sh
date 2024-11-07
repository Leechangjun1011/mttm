#!/bin/bash

#input : [workload] [local dram size]

echo 1 > /proc/sys/vm/drop_caches
echo 10007 > /proc/sys/vm/pebs_sample_period #10007, 4999, 1999, 997, 499, 199
echo 50000 > /proc/sys/vm/store_sample_period

echo 80G > /proc/sys/vm/mttm_local_dram_string
echo 0 > /proc/sys/vm/use_dram_determination
echo 0 > /proc/sys/vm/use_region_separation
echo 0 > /proc/sys/vm/use_hotness_intensity
echo 200 > /proc/sys/vm/hotness_intensity_threshold

echo 1 > /proc/sys/vm/use_lru_manage_reduce
echo 1 > /proc/sys/vm/use_pingpong_reduce
echo 500 > /proc/sys/vm/pingpong_reduce_threshold
echo 300 > /proc/sys/vm/mig_cputime_threshold
echo 50 > /proc/sys/vm/manage_cputime_threshold

echo 1000 > /proc/sys/vm/kmigrated_period_in_ms
echo 2000 > /proc/sys/vm/ksampled_trace_period_in_ms
echo 1 > /proc/sys/vm/check_stable_sample_rate
echo 0 > /proc/sys/vm/print_more_info

echo 1 > /proc/sys/vm/use_dma_migration
echo 0 > /proc/sys/vm/use_dma_completion_interrupt
echo 0 > /proc/sys/vm/use_all_stores
echo always > /sys/kernel/mm/transparent_hugepage/enabled
sudo sysctl kernel.perf_event_max_sample_rate=100000
sudo sysctl vm.enable_ksampled=0
sudo sysctl vm.enable_ksampled=1

./run_single_tenant_iter.sh $1 $2

echo madvise > /sys/kernel/mm/transparent_hugepage/enabled
sudo sysctl vm.enable_ksampled=0

