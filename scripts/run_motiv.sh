#!/bin/bash

cur_path=$PWD
emul_path=/home/cjlee/CXL-emulation.code/NUMA_setting/slow-memory-emulation
conda_activate=/root/anaconda3/bin/activate

function run_mttm_region
{
	dmesg --clear
	echo 1 > /proc/sys/vm/use_dram_determination
	echo 1 > /proc/sys/vm/use_region_separation
	echo 0 > /proc/sys/vm/use_hotness_intensity
	echo $1 > /proc/sys/vm/mttm_local_dram_string
	./run_multi_tenants.sh $2 2>&1 | cat > ./evaluation/region_$1_$2_$3.txt
	dmesg > ./evaluation/region_$1_$2_$3_dmesg.txt
}

function run_mttm_hi
{
	dmesg --clear
	echo 1 > /proc/sys/vm/use_dram_determination
	echo 0 > /proc/sys/vm/use_region_separation
	echo 1 > /proc/sys/vm/use_hotness_intensity
	echo $1 > /proc/sys/vm/mttm_local_dram_string
	./run_multi_tenants.sh $2 2>&1 | cat > ./evaluation/hi_$1_$2_$3.txt
	dmesg > ./evaluation/hi_$1_$2_$3_dmesg.txt
}

function run_mttm_static
{
	dmesg --clear
	echo 0 > /proc/sys/vm/use_dram_determination
	echo 0 > /proc/sys/vm/use_region_separation
	echo 0 > /proc/sys/vm/use_hotness_intensity
	echo 0 > /proc/sys/vm/use_memstrata_policy
	echo 1 > /proc/sys/vm/use_lru_manage_reduce
	echo 1 > /proc/sys/vm/use_pingpong_reduce
	echo 1 > /proc/sys/vm/print_more_info

	./run_multi_tenants.sh $1 2>&1 | cat > ./motivation/$1_$2.txt
	dmesg > ./motivation/$1_$2_dmesg.txt
}

function run_memstrata
{
	dmesg --clear
	echo 0 > /proc/sys/vm/use_region_separation
	echo 0 > /proc/sys/vm/use_hotness_intensity
	echo 0 > /proc/sys/vm/use_dram_determination
	echo 1 > /proc/sys/vm/use_memstrata_policy
	echo 4000 > /proc/sys/vm/donor_threshold
	echo 4000 > /proc/sys/vm/acceptor_threshold
	echo 0 > /proc/sys/vm/use_lru_manage_reduce
	echo 0 > /proc/sys/vm/use_pingpong_reduce
	echo 0 > /proc/sys/vm/print_more_info
	echo $1 > /proc/sys/vm/mttm_local_dram_string
	./run_multi_tenants.sh $2 2>&1 | cat > ./motivation/memstrata_$1_$2_$3.txt
	dmesg > ./motivation/memstrata_$1_$2_$3_dmesg.txt
}

function run_vtmm
{
	dmesg --clear
	echo $1 > /proc/sys/vm/mttm_local_dram_string
	./run_multi_tenants_vtmm.sh $2 2>&1 | cat > ./evaluation/vtmm_$1_$2_$3.txt
	dmesg > ./evaluation/vtmm_$1_$2_$3_dmesg.txt
}

function set_130
{
	cd $emul_path
	./reset.sh
	cd $cur_path
	echo 130 > /proc/sys/vm/remote_latency
}

function set_192
{
	cd $emul_path
	./reset.sh
	./emulate.sh 24 0
	cd $cur_path
	echo 192 > /proc/sys/vm/remote_latency
}

function set_250
{
	cd $emul_path
	./reset.sh
	./emulate.sh 24 0 0x80be
	cd $cur_path
	echo 250 > /proc/sys/vm/remote_latency
}

set_130

#run_mttm_static motiv-pr 130
run_mttm_static motiv-xsbench 130
#run_mttm_static motiv-xindex 130
#run_mttm_static motiv-cpu_dlrm_small_high 130

#run_memstrata 65G config10 130
#run_memstrata 19G config10 130


set_130

