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
#echo 101 > /proc/sys/vm/pebs_sample_period #10007, 4999, 1999, 997, 499, 199, 101
#echo 50000 > /proc/sys/vm/store_sample_period

#echo 80G > /proc/sys/vm/mttm_local_dram_string
#echo 1 > /proc/sys/vm/use_dram_determination
#echo 0 > /proc/sys/vm/use_memstrata_policy
#echo 1 > /proc/sys/vm/use_region_separation
#echo 0 > /proc/sys/vm/use_hotness_intensity
#echo 200 > /proc/sys/vm/hotness_intensity_threshold

#echo 50 > /proc/sys/vm/manage_cputime_threshold
#echo 1 > /proc/sys/vm/use_pingpong_reduce
echo 3 > /proc/sys/vm/pingpong_reduce_limit
#echo 200 > /proc/sys/vm/pingpong_reduce_threshold #500
echo 300 > /proc/sys/vm/mig_cputime_threshold

echo 1000 > /proc/sys/vm/kmigrated_period_in_ms
echo 2000 > /proc/sys/vm/ksampled_trace_period_in_ms
echo 1 > /proc/sys/vm/check_stable_sample_rate
#echo 0 > /proc/sys/vm/print_more_info

echo 1 > /proc/sys/vm/use_dma_migration
echo 0 > /proc/sys/vm/use_dma_completion_interrupt
#echo 1 > /proc/sys/vm/scanless_cooling
#echo 1 > /proc/sys/vm/reduce_scan
#echo 10 > /proc/sys/vm/basepage_shift_factor
#echo 40 > /proc/sys/vm/basepage_period_factor

echo 0 > /proc/sys/vm/use_all_stores

#echo always > /sys/kernel/mm/transparent_hugepage/enabled
sudo sysctl kernel.perf_event_max_sample_rate=100000
sudo sysctl vm.enable_ksampled=0
sudo sysctl vm.enable_ksampled=1

if [[ "$1" == "config1" ]]; then
	workload[1]="gapbs-pr"
	workload[2]="gapbs-bc"
	workload[3]="xsbench"
elif [[ "$1" == "config1-bw1" ]]; then
	workload[1]="gapbs-pr"
	workload[2]="gapbs-bc"
	workload[3]="xsbench"
elif [[ "$1" == "config1-bw2" ]]; then
	workload[1]="gapbs-pr"
	workload[2]="gapbs-bc"
	workload[3]="xsbench"
elif [[ "$1" == "config1-static1" ]]; then
	workload[1]="gapbs-pr"
	workload[2]="gapbs-bc"
	workload[3]="xsbench"
elif [[ "$1" == "config1-static4" ]]; then
	workload[1]="gapbs-pr"
	workload[2]="gapbs-bc"
	workload[3]="xsbench"
elif [[ "$1" == "config2" ]]; then
	workload[1]="fotonik"
	workload[2]="xindex"
	workload[3]="cpu_dlrm_small_high"
elif [[ "$1" == "config2-static1" ]]; then
	workload[1]="fotonik"
	workload[2]="xindex"
	workload[3]="cpu_dlrm_small_high"
elif [[ "$1" == "config2-static4" ]]; then
	workload[1]="fotonik"
	workload[2]="xindex"
	workload[3]="cpu_dlrm_small_high"
elif [[ "$1" == "config2-bw1" ]]; then
	workload[1]="fotonik"
	workload[2]="xindex"
	workload[3]="cpu_dlrm_small_high"
elif [[ "$1" == "config2-bw2" ]]; then
	workload[1]="fotonik"
	workload[2]="xindex"
	workload[3]="cpu_dlrm_small_high"
elif [[ "$1" == "config3" ]]; then
	workload[1]="silo"
	workload[2]="cpu_dlrm_small_low_1"
	workload[3]="cpu_dlrm_large_low_2"
elif [[ "$1" == "config12" ]]; then
	workload[1]="btree"
	workload[2]="xindex"
	workload[3]="cpu_dlrm_small_low"
elif [[ "$1" == "rebuttal" ]]; then
	workload[1]="btree"
	workload[2]="xindex"
	workload[3]="cpu_dlrm_large_low"
elif [[ "$1" == "config12-static1" ]]; then
	workload[1]="btree"
	workload[2]="xindex"
	workload[3]="cpu_dlrm_small_low"
elif [[ "$1" == "config12-static4" ]]; then
	workload[1]="btree"
	workload[2]="xindex"
	workload[3]="cpu_dlrm_small_low"
elif [[ "$1" == "config13" || "$1" == "config13-basepage" ]]; then
	workload[1]="gapbs-pr"
	workload[2]="fotonik"
	workload[3]="silo"
elif [[ "$1" == "config13-static1" ]]; then
	workload[1]="gapbs-pr"
	workload[2]="fotonik"
	workload[3]="silo"
elif [[ "$1" == "config13-bw1" ]]; then
	workload[1]="gapbs-pr"
	workload[2]="fotonik"
	workload[3]="silo"
elif [[ "$1" == "config13-bw2" ]]; then
	workload[1]="gapbs-pr"
	workload[2]="fotonik"
	workload[3]="silo"
elif [[ "$1" == "config13-bw3" ]]; then
	workload[1]="gapbs-pr"
	workload[2]="fotonik"
	workload[3]="silo"
elif [[ "$1" == "config13-bw4" ]]; then
	workload[1]="gapbs-pr"
	workload[2]="fotonik"
	workload[3]="silo"
elif [[ "$1" == "config13-static4" ]]; then
	workload[1]="gapbs-pr"
	workload[2]="fotonik"
	workload[3]="silo"
elif [[ "$1" == "6tenants" ]]; then
	workload[1]="gapbs-pr"
	workload[2]="xsbench"
	workload[3]="xindex_tiny"
	workload[4]="fotonik"
	workload[5]="silo"
	workload[6]="cpu_dlrm_small_low"
elif [[ "$1" == "12tenants" ]]; then
	workload[1]="gapbs-pr"
	workload[2]="gapbs-pr"
	workload[3]="xsbench"
	workload[4]="xsbench"
	workload[5]="fotonik"
	workload[6]="fotonik"
	workload[7]="xindex_tiny"
	workload[8]="xindex_tiny"
	workload[9]="silo"
	workload[10]="silo"
	workload[11]="cpu_dlrm_small_low_1"
	workload[12]="cpu_dlrm_small_low_2"
elif [[ "$1" == "motiv" ]]; then
	workload[1]="gapbs-pr"
	workload[2]="xsbench"
	workload[3]="cpu_dlrm_small_low"
elif [[ "$1" == "microbench" || "$1" == "microbench-dynamic" || "$1" == "microbench-sensitivity1" || "$1" == "microbench-sensitivity2" ]]; then
	workload[1]="gups-1"
	workload[2]="gups-2"
	workload[3]="gups-3"
elif [[ "$1" == "cooling" ]]; then
	workload[1]="gups-2g-8t"
	workload[2]="gups-4g-8t"
	workload[3]="gups-16g"
elif [[ "$1" == "motiv-pr" ]]; then
	workload[1]="gapbs-pr"
elif [[ "$1" == "motiv-xsbench" ]]; then
	workload[1]="xsbench"
elif [[ "$1" == "motiv-xindex" ]]; then
	workload[1]="xindex"
elif [[ "$1" == "motiv-cpu_dlrm_small_high" ]]; then
	workload[1]="cpu_dlrm_small_high"
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

