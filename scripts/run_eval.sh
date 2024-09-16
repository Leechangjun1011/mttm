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

function run_memstrata
{
	dmesg --clear
	echo 0 > /proc/sys/vm/use_region_separation
	echo 0 > /proc/sys/vm/use_hotness_intensity
	echo 0 > /proc/sys/vm/use_dram_determination
	echo 1 > /proc/sys/vm/use_memstrata_policy
	#echo 4000 > /proc/sys/vm/donor_threshold
	#echo 4000 > /proc/sys/vm/acceptor_threshold
	echo 0 > /proc/sys/vm/use_lru_manage_reduce
	echo 0 > /proc/sys/vm/use_pingpong_reduce
	echo 1 > /proc/sys/vm/print_more_info
	echo $1 > /proc/sys/vm/mttm_local_dram_string
	./run_multi_tenants.sh $2 2>&1 | cat > ./evaluation/fmmr/memstrata_$1_$2_$3.txt
	dmesg > ./evaluation/fmmr/memstrata_$1_$2_$3_dmesg.txt
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
: << END
#1st exp
set_130
source $conda_activate dlrm_cpu
run_mttm_region 63G config4 130
run_mttm_hi 63G config4 130
run_mttm_region 25G config4 130
run_mttm_hi 25G config4 130

conda deactivate
set_192
source $conda_activate dlrm_cpu
run_mttm_region 63G config4 192
run_mttm_hi 63G config4 192
run_mttm_region 25G config4 192
run_mttm_hi 25G config4 192

conda deactivate
set_250
source $conda_activate dlrm_cpu
run_mttm_region 63G config4 250
run_mttm_hi 63G config4 250
run_mttm_region 25G config4 250
run_mttm_hi 25G config4 250


#2nd exp
conda deactivate
set_130
run_mttm_region 51G config6 130
run_mttm_hi 51G config6 130
run_mttm_region 21G config6 130
run_mttm_hi 21G config6 130

set_192
run_mttm_region 51G config6 192
run_mttm_hi 51G config6 192
run_mttm_region 21G config6 192
run_mttm_hi 21G config6 192

set_250
run_mttm_region 51G config6 250
run_mttm_hi 51G config6 250
run_mttm_region 21G config6 250
run_mttm_hi 21G config6 250


#3rd exp
set_130
source $conda_activate dlrm_cpu
run_mttm_region 50G config8 130
run_mttm_hi 50G config8 130
run_mttm_region 20G config8 130
run_mttm_hi 20G config8 130

conda deactivate
set_192
source $conda_activate dlrm_cpu
run_mttm_region 50G config8 192
run_mttm_hi 50G config8 192
run_mttm_region 20G config8 192
run_mttm_hi 20G config8 192

conda deactivate
set_250
source $conda_activate dlrm_cpu
run_mttm_region 50G config8 250
run_mttm_hi 50G config8 250
run_mttm_region 20G config8 250
run_mttm_hi 20G config8 250


#4th exp
conda deactivate
set_130
source $conda_activate dlrm_cpu
run_mttm_region 48G config10 130
run_mttm_hi 48G config10 130
run_mttm_region 19G config10 130
run_mttm_hi 19G config10 130

conda deactivate
set_250
source $conda_activate dlrm_cpu
run_mttm_region 48G config10 250
run_mttm_hi 48G config10 250
run_mttm_region 19G config10 250
run_mttm_hi 19G config10 250
END

#1st exp
set_130
source $conda_activate dlrm_cpu
run_memstrata 63G config4 130
run_memstrata 25G config4 130

conda deactivate
set_192
source $conda_activate dlrm_cpu
run_memstrata 63G config4 192
run_memstrata 25G config4 192

conda deactivate
set_250
source $conda_activate dlrm_cpu
run_memstrata 63G config4 250
run_memstrata 25G config4 250

#2nd exp
conda deactivate
set_130
source $conda_activate dlrm_cpu
run_memstrata 51G config6 130
run_memstrata 21G config6 130

conda deactivate
set_192
source $conda_activate dlrm_cpu
run_memstrata 51G config6 192
run_memstrata 21G config6 192

conda deactivate
set_250
source $conda_activate dlrm_cpu
run_memstrata 51G config6 250
run_memstrata 21G config6 250

#3rd exp
conda deactivate
set_130
source $conda_activate dlrm_cpu
run_memstrata 50G config8 130
run_memstrata 20G config8 130

conda deactivate
set_192
source $conda_activate dlrm_cpu
run_memstrata 50G config8 192
run_memstrata 20G config8 192

conda deactivate
set_250
source $conda_activate dlrm_cpu
run_memstrata 50G config8 250
run_memstrata 20G config8 250

#4th exp
conda deactivate
set_130
source $conda_activate dlrm_cpu
run_memstrata 48G config10 130
run_memstrata 19G config10 130

conda deactivate
set_192
source $conda_activate dlrm_cpu
run_memstrata 48G config10 192
run_memstrata 19G config10 192

conda deactivate
set_250
source $conda_activate dlrm_cpu
run_memstrata 48G config10 250
run_memstrata 19G config10 250

set_130


