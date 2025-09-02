#!/bin/bash

#input : [# of workloads] [1st workload] [2nd workload] ...
re='^[0-9]+$'

echo 1 > /proc/sys/vm/drop_caches
sudo sysctl kernel.perf_event_max_sample_rate=100000

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
elif [[ "$1" == "mix4" ]]; then
        workload[1]="gapbs-pr"
        workload[2]="fotonik"
        workload[3]="silo"
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
	./run_single_tenant_tpp.sh $i ${workload[i]} $1 &	
done
wait

echo madvise > /sys/kernel/mm/transparent_hugepage/enabled

