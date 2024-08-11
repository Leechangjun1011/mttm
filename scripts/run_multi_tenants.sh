#!/bin/bash

#input : [# of workloads] [1st workload] [2nd workload] ...
re='^[0-9]+$'
: << END
if ! [[ $1 =~ $re ]]
then
	echo "1st input should be a number"
	exit 0
fi

if [ $# -lt 2 ]
then
	echo "at least one workload required"
	exit 0
fi

if [ $(($# - 1)) -ne $1 ]
then
	echo "number of workloads is not matched"
	exit 0
fi
END

echo 1 > /proc/sys/vm/drop_caches
echo 10007 > /proc/sys/vm/pebs_sample_period #10007, 4999, 1999, 997, 499, 199
echo 50000 > /proc/sys/vm/store_sample_period

echo 80G > /proc/sys/vm/mttm_local_dram_string
echo 1 > /proc/sys/vm/use_dram_determination
echo 1 > /proc/sys/vm/use_region_separation
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
echo 0 > /proc/sys/vm/use_xa_basepage
echo 0 > /proc/sys/vm/use_all_stores
echo always > /sys/kernel/mm/transparent_hugepage/enabled
sudo sysctl kernel.perf_event_max_sample_rate=100000
sudo sysctl vm.enable_ksampled=0
sudo sysctl vm.enable_ksampled=1

if [[ "$1" == "config1" ]]; then
	workload[1]="gapbs-pr"
	workload[2]="gapbs-bc"
	workload[3]="cpu_dlrm_small_low"
	echo 70G > /proc/sys/vm/mttm_local_dram_string
elif [[ "$1" == "config3" ]]; then
	workload[1]="xsbench"
	workload[2]="xindex"
	workload[3]="cpu_dlrm_large_low"
	echo 73G > /proc/sys/vm/mttm_local_dram_string #73G, 16G
elif [[ "$1" == "config4" ]]; then
	workload[1]="xsbench"
	workload[2]="roms"
	workload[3]="cpu_dlrm_large_low"
	echo 14G > /proc/sys/vm/mttm_local_dram_string #63G, 25G, 14G
elif [[ "$1" == "config5" ]]; then
	workload[1]="gapbs-pr"
	workload[2]="silo"
	workload[3]="cpu_dlrm_med_low"
	echo 54G > /proc/sys/vm/mttm_local_dram_string
elif [[ "$1" == "config6" ]]; then
	workload[1]="gapbs-pr"
	workload[2]="fotonik"
	workload[3]="silo"
	echo 51G > /proc/sys/vm/mttm_local_dram_string
else
	i=1
	for arg in "$@"
	do
		if ! [[ $arg =~ $re ]]
		then
			workload[$i]=$arg
			((i++))
		fi
	done
fi

for i in "${!workload[@]}"
do
	./run_single_tenant.sh $i ${workload[i]} $1 &	
done
wait

echo madvise > /sys/kernel/mm/transparent_hugepage/enabled
sudo sysctl vm.enable_ksampled=0

