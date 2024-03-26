#!/bin/bash

#input : [# of workloads] [1st workload] [2nd workload] ...
re='^[0-9]+$'
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

echo 1 > /proc/sys/vm/drop_caches
echo 10007 > /proc/sys/vm/pebs_sample_period #10007, 4999
echo always > /sys/kernel/mm/transparent_hugepage/enabled
sudo sysctl kernel.perf_event_max_sample_rate=100000
sudo sysctl vm.enable_ksampled=1

i=1
for arg in "$@"
do
	if ! [[ $arg =~ $re ]]
	then
		workload[$i]=$arg
		((i++))
	fi
done

for i in "${!workload[@]}"
do
	if [ $i -ne $1 ]
	then
		./run_single_tenant.sh $i ${workload[i]} &
	else
		./run_single_tenant.sh $i ${workload[i]}
	fi
done

echo madvise > /sys/kernel/mm/transparent_hugepage/enabled
sudo sysctl vm.enable_ksampled=0

