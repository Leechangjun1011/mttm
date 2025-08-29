#!/bin/bash

# input : [# of workloads] [1st workload] [2nd workload] ...
# or just [mix number]
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
echo 3 > /proc/sys/vm/pingpong_reduce_limit
echo 300 > /proc/sys/vm/mig_cputime_threshold

echo 1000 > /proc/sys/vm/kmigrated_period_in_ms
echo 2000 > /proc/sys/vm/ksampled_trace_period_in_ms
echo 1 > /proc/sys/vm/check_stable_sample_rate

echo 1 > /proc/sys/vm/use_dma_migration

sudo sysctl kernel.perf_event_max_sample_rate=100000
sudo sysctl vm.enable_ksampled=0
sudo sysctl vm.enable_ksampled=1

if [[ "$1" == "mix1" ]]; then
	workload[1]="gapbs-pr"
	workload[2]="gapbs-bc"
	workload[3]="xsbench"
elif [[ "$1" == "mix2" ]]; then
	workload[1]="fotonik"
	workload[2]="xindex"
	workload[3]="cpu_dlrm_small_high"
elif [[ "$1" == "mix3" ]]; then
	workload[1]="btree"
	workload[2]="xindex"
	workload[3]="cpu_dlrm_small_low"
elif [[ "$1" == "mix4" || "$1" == "config13-basepage" ]]; then
	workload[1]="gapbs-pr"
	workload[2]="fotonik"
	workload[3]="silo"
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

