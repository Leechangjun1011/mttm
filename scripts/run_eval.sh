#!/bin/bash

cur_path=$PWD
emul_path=/home/cjlee/CXL-emulation.code/NUMA_setting/slow-memory-emulation

function run_mttm_region
{
	dmesg --clear
	echo 1 > /proc/sys/vm/use_region_separation
	echo 0 > /proc/sys/vm/use_hotness_intensity
	echo $1 > /proc/sys/vm/mttm_local_dram_string
	./run_multi_tenants.sh $2 2>&1 | cat > ./evaluation/region_$1_$2_$3.txt
	dmesg > ./evaluation/region_$1_$2_$3_dmesg.txt
}

function run_mttm_hi
{
	dmesg --clear
	echo 0 > /proc/sys/vm/use_region_separation
	echo 1 > /proc/sys/vm/use_hotness_intensity
	echo $1 > /proc/sys/vm/mttm_local_dram_string
	./run_multi_tenants.sh $2 2>&1 | cat > ./evaluation/hi_$1_$2_$3.txt
	dmesg > ./evaluation/hi_$1_$2_$3_dmesg.txt
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
}

function set_192
{
	cd $emul_path
	./reset.sh
	./emulate.sh 24 0
	cd $cur_path
}

function set_250
{
	cd $emul_path
	./reset.sh
	./emulate.sh 24 0 0x80be
	cd $cur_path
}

: << END
conda activate dlrm_cpu
#run_mttm_region 48G config10 130
#run_mttm_hi 48G config10 130
#run_mttm_region 19G config10 130
#run_mttm_hi 19G config10 130

conda deactivate
set_192
conda activate dlrm_cpu
run_mttm_region 48G config10 192
run_mttm_hi 48G config10 192
run_mttm_region 19G config10 192
run_mttm_hi 19G config10 192

conda deactivate
set_250
conda activate dlrm_cpu
run_mttm_region 48G config10 250
#run_mttm_hi 48G config10 250
run_mttm_region 19G config10 250
run_mttm_hi 19G config10 250
END


set_130
run_vtmm 51G config6 130
run_vtmm 21G config6 130

set_192
run_vtmm 51G config6 192
run_vtmm 21G config6 192

set_130


