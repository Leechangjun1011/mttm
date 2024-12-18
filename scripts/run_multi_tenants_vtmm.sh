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

if [[ "$1" == "config1" ]]; then
	workload[1]="gapbs-pr"
	workload[2]="gapbs-bc"
	workload[3]="xsbench"
elif [[ "$1" == "config2" ]]; then
	workload[1]="fotonik"
	workload[2]="xindex"
	workload[3]="cpu_dlrm_small_high"
elif [[ "$1" == "config3" ]]; then
	workload[1]="silo"
	workload[2]="cpu_dlrm_small_low_1"
	workload[3]="cpu_dlrm_large_low_2"
elif [[ "$1" == "config9" ]]; then
	workload[1]="xsbench"
	workload[2]="xindex"
	workload[3]="cpu_dlrm_small_low"
elif [[ "$1" == "config4" ]]; then
	workload[1]="xsbench"
	workload[2]="roms"
	workload[3]="cpu_dlrm_large_low"
elif [[ "$1" == "config5" ]]; then
	workload[1]="gapbs-pr"
	workload[2]="roms"
	workload[3]="cpu_dlrm_large_low"
elif [[ "$1" == "config6" ]]; then
	workload[1]="gapbs-pr"
	workload[2]="fotonik"
	workload[3]="silo"
elif [[ "$1" == "config7" ]]; then
	workload[1]="xsbench"
	workload[2]="fotonik"
	workload[3]="silo"
elif [[ "$1" == "config8" ]]; then
	workload[1]="cpu_dlrm_small_high"
	workload[2]="gapbs-bc"
	workload[3]="xindex"
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

