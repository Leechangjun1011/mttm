#!/bin/bash

cur_path=$PWD
emul_path=/home/cjlee/CXL-emulation.code/NUMA_setting/slow-memory-emulation
conda_activate=/root/anaconda3/bin/activate

function run_mttm_region_hugepage
{
	#config, dram size, remote latency
	dmesg --clear
	echo 1 > /proc/sys/vm/use_dram_determination
	echo 1 > /proc/sys/vm/use_region_separation
	echo 0 > /proc/sys/vm/use_hotness_intensity
	echo 0 > /proc/sys/vm/use_memstrata_policy
	echo $2 > /proc/sys/vm/mttm_local_dram_string

	echo 10007 > /proc/sys/vm/pebs_sample_period
	echo 500 > /proc/sys/vm/pingpong_reduce_threshold
	echo 1 > /proc/sys/vm/scanless_cooling
	echo 1 > /proc/sys/vm/reduce_scan
	echo always > /sys/kernel/mm/transparent_hugepage/enabled

	./run_multi_tenants.sh $1 2>&1 | cat > ./evaluation/region_$1_$2_$3.txt
	dmesg > ./evaluation/region_$1_$2_$3_dmesg.txt
}

function run_mttm_region_basepage_opt
{
	dmesg --clear
	echo 1 > /proc/sys/vm/use_dram_determination
	echo 1 > /proc/sys/vm/use_region_separation
	echo 0 > /proc/sys/vm/use_hotness_intensity
	echo 0 > /proc/sys/vm/use_memstrata_policy
	echo $2 > /proc/sys/vm/mttm_local_dram_string

	echo 101 > /proc/sys/vm/pebs_sample_period
	echo 200 > /proc/sys/vm/pingpong_reduce_threshold
	echo 1 > /proc/sys/vm/scanless_cooling
	echo 1 > /proc/sys/vm/reduce_scan
	echo madvise > /sys/kernel/mm/transparent_hugepage/enabled
	echo 9 > /proc/sys/vm/basepage_shift_factor #target cooling period
	echo 40 > /proc/sys/vm/basepage_period_factor #increasing granularity

	./run_multi_tenants.sh $1 2>&1 | cat > ./evaluation/basepage/region_$1_$2_$3.txt
	dmesg > ./evaluation/basepage/region_$1_$2_$3_dmesg.txt
}

function run_mttm_region_basepage_scan
{
	dmesg --clear
	echo 1 > /proc/sys/vm/use_dram_determination
	echo 1 > /proc/sys/vm/use_region_separation
	echo 0 > /proc/sys/vm/use_hotness_intensity
	echo 0 > /proc/sys/vm/use_memstrata_policy
	echo $2 > /proc/sys/vm/mttm_local_dram_string
	echo 0 > /proc/sys/vm/scanless_cooling
	echo 0 > /proc/sys/vm/reduce_scan
	./run_multi_tenants.sh $1 2>&1 | cat > ./evaluation/basepage/region_scan_$1_$2_$3.txt
	dmesg > ./evaluation/basepage/region_scan_$1_$2_$3_dmesg.txt
}

function run_local_hugepage
{
	echo always > /sys/kernel/mm/transparent_hugepage/enabled
	./run_multi_tenants_native.sh $1 #2>&1 | cat > ./evaluation/local_$1.txt
}

function run_local_basepage
{
	echo madvise > /sys/kernel/mm/transparent_hugepage/enabled
	./run_multi_tenants_native.sh $1 2>&1 | cat > ./evaluation/basepage/local_$1.txt
}

function run_naive_hi_hugepage 
{
	dmesg --clear
	echo 1 > /proc/sys/vm/use_dram_determination
	echo 0 > /proc/sys/vm/use_region_separation
	echo 1 > /proc/sys/vm/use_hotness_intensity
	echo 1 > /proc/sys/vm/use_naive_hi
	echo 0 > /proc/sys/vm/use_memstrata_policy
	echo $2 > /proc/sys/vm/mttm_local_dram_string

	echo 10007 > /proc/sys/vm/pebs_sample_period
	echo 500 > /proc/sys/vm/pingpong_reduce_threshold
	echo 0 > /proc/sys/vm/scanless_cooling
	echo 0 > /proc/sys/vm/reduce_scan
	echo always > /sys/kernel/mm/transparent_hugepage/enabled

	./run_multi_tenants.sh $1 2>&1 | cat > ./evaluation/hi_$1_$2_$3.txt
	dmesg > ./evaluation/hi_$1_$2_$3_dmesg.txt
}

function run_mttm_hi_hugepage
{
	dmesg --clear
	echo 1 > /proc/sys/vm/use_dram_determination
	echo 0 > /proc/sys/vm/use_region_separation
	echo 1 > /proc/sys/vm/use_hotness_intensity
	echo 1 > /proc/sys/vm/use_naive_hi
	echo 0 > /proc/sys/vm/use_memstrata_policy
	echo $2 > /proc/sys/vm/mttm_local_dram_string

	echo 10007 > /proc/sys/vm/pebs_sample_period
	echo 0 > /proc/sys/vm/scanless_cooling
	echo 0 > /proc/sys/vm/reduce_scan
	echo always > /sys/kernel/mm/transparent_hugepage/enabled

	./run_multi_tenants.sh $1 2>&1 | cat > ./evaluation/hi_$1_$2_$3.txt
	dmesg > ./evaluation/hi_$1_$2_$3_dmesg.txt
}

function run_mttm_hi_basepage
{
	dmesg --clear
	echo 1 > /proc/sys/vm/use_dram_determination
	echo 0 > /proc/sys/vm/use_region_separation
	echo 1 > /proc/sys/vm/use_hotness_intensity
	echo 1 > /proc/sys/vm/use_naive_hi
	echo 0 > /proc/sys/vm/use_memstrata_policy
	echo $2 > /proc/sys/vm/mttm_local_dram_string

	echo 101 > /proc/sys/vm/pebs_sample_period
	echo 0 > /proc/sys/vm/scanless_cooling
	echo 0 > /proc/sys/vm/reduce_scan
	echo madvise > /sys/kernel/mm/transparent_hugepage/enabled
	echo 9 > /proc/sys/vm/basepage_shift_factor #target cooling period
	echo 40 > /proc/sys/vm/basepage_period_factor #increasing granularity

	./run_multi_tenants.sh $1 2>&1 | cat > ./evaluation/basepage/hi_$1_$2_$3.txt
	dmesg > ./evaluation/basepage/hi_$1_$2_$3_dmesg.txt
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

function run_vtmm_hugepage
{
	dmesg --clear
	echo $2 > /proc/sys/vm/mttm_local_dram_string
	./run_multi_tenants_vtmm.sh $1 2>&1 | cat > ./evaluation/vtmm_$1_$2_$3.txt
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



# Hotness intensity
#130
set_130
#run_naive_hi_hugepage config1 54G 130
#run_naive_hi_hugepage config1 21G 130

source $conda_activate dlrm_cpu
#run_naive_hi_hugepage config2 34G 130
#run_naive_hi_hugepage config2 13G 130

#run_naive_hi_hugepage config3 84G 130
run_naive_hi_hugepage config3 33G 130
: << END

run_naive_hi_hugepage config9 46G 130
run_naive_hi_hugepage config9 18G 130

#192
conda deactivate
set_192
#run_naive_hi_hugepage config1 54G 192
#run_naive_hi_hugepage config1 21G 192

source $conda_activate dlrm_cpu
run_naive_hi_hugepage config2 34G 192
run_naive_hi_hugepage config2 13G 192

run_naive_hi_hugepage config3 84G 192
run_naive_hi_hugepage config3 33G 192

run_naive_hi_hugepage config9 46G 192
run_naive_hi_hugepage config9 18G 192

#250
conda deactivate
set_250
#run_naive_hi_hugepage config1 54G 250
#run_naive_hi_hugepage config1 21G 250

source $conda_activate dlrm_cpu
run_naive_hi_hugepage config2 34G 250
run_naive_hi_hugepage config2 13G 250

run_naive_hi_hugepage config3 84G 250
run_naive_hi_hugepage config3 33G 250

run_naive_hi_hugepage config9 46G 250
run_naive_hi_hugepage config9 18G 250



# MTTM region
#130
conda deactivate
set_130
run_mttm_region_hugepage config1 54G 130
run_mttm_region_hugepage config1 21G 130

source $conda_activate dlrm_cpu
run_mttm_region_hugepage config2 34G 130
run_mttm_region_hugepage config2 13G 130

run_mttm_region_hugepage config3 84G 130
run_mttm_region_hugepage config3 33G 130

run_mttm_region_hugepage config9 46G 130
run_mttm_region_hugepage config9 18G 130

# 192
conda deactivate
set_192
run_mttm_region_hugepage config1 54G 192
run_mttm_region_hugepage config1 21G 192


source $conda_activate dlrm_cpu
run_mttm_region_hugepage config2 34G 192
run_mttm_region_hugepage config2 13G 192

run_mttm_region_hugepage config3 84G 192
run_mttm_region_hugepage config3 33G 192

run_mttm_region_hugepage config9 46G 192
run_mttm_region_hugepage config9 18G 192

# 250
conda deactivate
set_250
run_mttm_region_hugepage config1 54G 250
run_mttm_region_hugepage config1 21G 250

source $conda_activate dlrm_cpu
run_mttm_region_hugepage config2 34G 250
run_mttm_region_hugepage config2 13G 250

run_mttm_region_hugepage config3 84G 250
run_mttm_region_hugepage config3 33G 250

run_mttm_region_hugepage config9 46G 250
run_mttm_region_hugepage config9 18G 250


# vTMM
#130
conda deactivate
set_130
run_vtmm_hugepage config1 54G 130
run_vtmm_hugepage config1 21G 130

source $conda_activate dlrm_cpu
run_vtmm_hugepage config2 34G 130
run_vtmm_hugepage config2 13G 130

run_vtmm_hugepage config3 84G 130
run_vtmm_hugepage config3 33G 130

run_vtmm_hugepage config9 46G 130
run_vtmm_hugepage config9 18G 130


#192
conda deactivate
set_192
run_vtmm_hugepage config1 54G 192
run_vtmm_hugepage config1 21G 192

source $conda_activate dlrm_cpu
run_vtmm_hugepage config2 34G 192
run_vtmm_hugepage config2 13G 192

run_vtmm_hugepage config3 84G 192
run_vtmm_hugepage config3 33G 192

run_vtmm_hugepage config9 46G 192
run_vtmm_hugepage config9 18G 192

#250
conda deactivate
set_250
run_vtmm_hugepage config1 54G 250
run_vtmm_hugepage config1 21G 250

source $conda_activate dlrm_cpu
run_vtmm_hugepage config2 34G 250
run_vtmm_hugepage config2 13G 250

run_vtmm_hugepage config3 84G 250
run_vtmm_hugepage config3 33G 250

run_vtmm_hugepage config9 46G 250
run_vtmm_hugepage config9 18G 250

END


# MTTM basepage
: << END

#run_mttm_region_basepage_opt 51G config6 130
#run_mttm_region_basepage_scan 51G config6 130

conda deactivate
source $conda_activate dlrm_cpu
run_mttm_region_basepage_opt 50G config8 130
run_mttm_region_basepage_scan 50G config8 130

conda deactivate
source $conda_activate dlrm_cpu
run_mttm_region_basepage_opt 48G config10 130
run_mttm_region_basepage_scan 48G config10 130

conda deactivate
run_local_basepage config6

source $conda_activate dlrm_cpu
run_local_basepage config8

conda deactivate
source $conda_activate dlrm_cpu
run_local_basepage config10
END

set_130


