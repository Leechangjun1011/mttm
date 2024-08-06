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

echo 102G > /proc/sys/vm/mttm_local_dram_string
echo 1 > /proc/sys/vm/use_dram_determination

echo 1 > /proc/sys/vm/use_dma_migration
echo 0 > /proc/sys/vm/use_dma_completion_interrupt
echo 600000 > /proc/sys/vm/kptscand_period_in_us
echo always > /sys/kernel/mm/transparent_hugepage/enabled
sudo sysctl kernel.perf_event_max_sample_rate=100000
sudo sysctl vm.enable_kptscand=0
sudo sysctl vm.enable_kptscand=1

if [[ "$1" == "config3" ]]; then
	workload[1]="xsbench"
	workload[2]="xindex"
	workload[3]="cpu_dlrm_large_low"
	echo 73G > /proc/sys/vm/mttm_local_dram_string #73G, 16G
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
	./run_single_tenant_vtmm.sh $i ${workload[i]} $1 &	
done
wait

echo madvise > /sys/kernel/mm/transparent_hugepage/enabled
sudo sysctl vm.enable_kptscand=0

