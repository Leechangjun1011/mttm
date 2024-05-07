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
echo always > /sys/kernel/mm/transparent_hugepage/enabled
sudo sysctl kernel.perf_event_max_sample_rate=100000

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
	./run_single_tenant_native.sh $i ${workload[i]} &	
done
wait

echo madvise > /sys/kernel/mm/transparent_hugepage/enabled

