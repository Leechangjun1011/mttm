#!/bin/bash

cur_path=$PWD
emul_path=/home/cjlee/CXL-emulation.code/NUMA_setting/slow-memory-emulation
conda_activate=/root/anaconda3/bin/activate

function run_mttm_region
{
	dmesg --clear
	echo 1 > /proc/sys/vm/use_region_separation
	echo 0 > /proc/sys/vm/use_hotness_intensity
	echo $1 > /proc/sys/vm/mttm_local_dram_string
	echo 1 > /proc/sys/vm/use_lru_manage_reduce
	echo 1 > /proc/sys/vm/use_pingpong_reduce
	./run_multi_tenants.sh $2 2>&1 | cat > ./evaluation/scalability/region_$1_$2_$3.txt
	dmesg > ./evaluation/scalability/region_$1_$2_$3_dmesg.txt
}


function run_mttm_hi
{
	dmesg --clear
	echo 0 > /proc/sys/vm/use_region_separation
	echo 1 > /proc/sys/vm/use_hotness_intensity
	echo $1 > /proc/sys/vm/mttm_local_dram_string
	echo 1 > /proc/sys/vm/use_lru_manage_reduce
	echo 1 > /proc/sys/vm/use_pingpong_reduce
	./run_multi_tenants.sh $2 2>&1 | cat > ./evaluation/scalability/hi_$1_$2_$3.txt
	dmesg > ./evaluation/scalability/hi_$1_$2_$3_dmesg.txt
}

function run_vtmm
{
	dmesg --clear
	echo $1 > /proc/sys/vm/mttm_local_dram_string
	./run_multi_tenants_vtmm.sh $2 2>&1 | cat > ./evaluation/scalability/vtmm_$1_$2_$3.txt
	dmesg > ./evaluation/scalability/vtmm_$1_$2_$3_dmesg.txt
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


#set_130
#source $conda_activate dlrm_cpu
#run_vtmm 41G 6tenants 130

#conda deactivate
set_192
source $conda_activate dlrm_cpu
#run_mttm_region 41G 6tenants 192
run_vtmm 41G 6tenants 192

conda deactivate
set_250
source $conda_activate dlrm_cpu
run_vtmm 41G 6tenants 250
#run_mttm_region 41G 6tenants 250

#run_mttm_region 81G 12tenants 250


set_130


