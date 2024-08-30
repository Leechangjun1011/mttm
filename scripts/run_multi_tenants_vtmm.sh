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

#echo 80G > /proc/sys/vm/mttm_local_dram_string
echo 1 > /proc/sys/vm/use_dram_determination

echo 1 > /proc/sys/vm/use_dma_migration
echo 0 > /proc/sys/vm/use_dma_completion_interrupt
echo 0 > /proc/sys/vm/print_more_info
echo 600000 > /proc/sys/vm/kptscand_period_in_us
echo always > /sys/kernel/mm/transparent_hugepage/enabled
sudo sysctl kernel.perf_event_max_sample_rate=100000
sudo sysctl vm.enable_kptscand=0
sudo sysctl vm.enable_kptscand=1

if [[ "$1" == "config3" ]]; then
	workload[1]="xsbench"
	workload[2]="xindex"
	workload[3]="cpu_dlrm_large_low"
	echo 16G > /proc/sys/vm/mttm_local_dram_string #73G, 16G
elif [[ "$1" == "config4" ]]; then
	workload[1]="xsbench"
	workload[2]="roms"
	workload[3]="cpu_dlrm_large_low"
	#echo 63G > /proc/sys/vm/mttm_local_dram_string #63G, 25G, 14G
elif [[ "$1" == "config5" ]]; then
	workload[1]="gapbs-pr"
	workload[2]="silo"
	workload[3]="cpu_dlrm_med_low"
	echo 54G > /proc/sys/vm/mttm_local_dram_string #54G
elif [[ "$1" == "config6" ]]; then
	workload[1]="gapbs-pr"
	workload[2]="fotonik"
	workload[3]="silo"
	#echo 51G > /proc/sys/vm/mttm_local_dram_string #54G
elif [[ "$1" == "config7" ]]; then
	workload[1]="gapbs-tc"
	workload[2]="gapbs-bc"
	workload[3]="xindex"
	echo 51G > /proc/sys/vm/mttm_local_dram_string #51G, 20G
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

