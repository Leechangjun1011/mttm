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

#echo 80G > /proc/sys/vm/mttm_local_dram_string
echo 0 > /proc/sys/vm/use_dram_determination
echo 0 > /proc/sys/vm/use_memstrata_policy
#echo 1 > /proc/sys/vm/use_region_separation
#echo 0 > /proc/sys/vm/use_hotness_intensity
echo 200 > /proc/sys/vm/hotness_intensity_threshold

echo 1 > /proc/sys/vm/use_lru_manage_reduce
echo 1 > /proc/sys/vm/use_pingpong_reduce
echo 5 > /proc/sys/vm/pingpong_reduce_limit
echo 500 > /proc/sys/vm/pingpong_reduce_threshold
echo 300 > /proc/sys/vm/mig_cputime_threshold
echo 50 > /proc/sys/vm/manage_cputime_threshold

echo 1000 > /proc/sys/vm/kmigrated_period_in_ms
echo 2000 > /proc/sys/vm/ksampled_trace_period_in_ms
echo 1 > /proc/sys/vm/check_stable_sample_rate
#echo 0 > /proc/sys/vm/print_more_info

echo 1 > /proc/sys/vm/use_dma_migration
echo 1 > /proc/sys/vm/use_dma_completion_interrupt
echo 0 > /proc/sys/vm/scanless_cooling
echo 0 > /proc/sys/vm/reduce_scan

echo 0 > /proc/sys/vm/use_all_stores
#echo always > /sys/kernel/mm/transparent_hugepage/enabled
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
	#echo 63G > /proc/sys/vm/mttm_local_dram_string #63G, 25G, 14G
elif [[ "$1" == "config5" ]]; then
	workload[1]="gapbs-pr"
	workload[2]="silo"
	workload[3]="cpu_dlrm_med_low"
	echo 54G > /proc/sys/vm/mttm_local_dram_string
elif [[ "$1" == "config6" ]]; then
	workload[1]="gapbs-pr"
	workload[2]="fotonik"
	workload[3]="silo"
	#echo 51G > /proc/sys/vm/mttm_local_dram_string
elif [[ "$1" == "config7" ]]; then
	workload[1]="gapbs-tc"
	workload[2]="gapbs-bc"
	workload[3]="xindex"
	echo 20G > /proc/sys/vm/mttm_local_dram_string #51G, 20G
elif [[ "$1" == "config8" ]]; then
	workload[1]="cpu_dlrm_small_high"
	workload[2]="gapbs-bc"
	workload[3]="xindex"
	#echo 20G > /proc/sys/vm/mttm_local_dram_string #50G, 20G
elif [[ "$1" == "config9" ]]; then
	workload[1]="cpu_dlrm_small_low"
	workload[2]="silo"
	workload[3]="nas_cg.d"
elif [[ "$1" == "config10" ]]; then
	workload[1]="cpu_dlrm_small_low"
	workload[2]="silo"
	workload[3]="fotonik"
elif [[ "$1" == "6tenants" ]]; then
	workload[1]="gapbs-bc"
	workload[2]="xsbench"
	workload[3]="gapbs-pr"
	workload[4]="silo"
	workload[5]="fotonik"
	workload[6]="cpu_dlrm_small_low"
elif [[ "$1" == "12tenants" ]]; then
	workload[1]="gapbs-bc"
	workload[2]="xsbench"
	workload[3]="gapbs-pr"
	workload[4]="silo"
	workload[5]="fotonik"
	workload[6]="cpu_dlrm_small_low_1"
	workload[7]="gapbs-bc"
	workload[8]="xsbench"
	workload[9]="gapbs-pr"
	workload[10]="silo"
	workload[11]="fotonik"
	workload[12]="cpu_dlrm_small_low_2"
elif [[ "$1" == "motiv" ]]; then
	workload[1]="gapbs-pr"
	workload[2]="xsbench"
	workload[3]="cpu_dlrm_small_low"
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

