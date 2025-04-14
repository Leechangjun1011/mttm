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
	echo 0 > /proc/sys/vm/print_more_info

	echo 10007 > /proc/sys/vm/pebs_sample_period
	echo 1 > /proc/sys/vm/use_pingpong_reduce
	echo 500 > /proc/sys/vm/pingpong_reduce_threshold
	echo 1 > /proc/sys/vm/scanless_cooling
	echo 1 > /proc/sys/vm/reduce_scan
	echo always > /sys/kernel/mm/transparent_hugepage/enabled

	./run_multi_tenants.sh $1 2>&1 | cat > ./evaluation/region_$1_$2_$3.txt
	dmesg > ./evaluation/region_$1_$2_$3_dmesg.txt
}


function run_mttm_region_hugepage_pingpong
{
	#config, dram size, remote latency
	dmesg --clear
	echo 1 > /proc/sys/vm/use_dram_determination
	echo 1 > /proc/sys/vm/use_region_separation
	echo 0 > /proc/sys/vm/use_hotness_intensity
	echo 0 > /proc/sys/vm/use_memstrata_policy
	echo $2 > /proc/sys/vm/mttm_local_dram_string
	echo 0 > /proc/sys/vm/print_more_info

	echo 10007 > /proc/sys/vm/pebs_sample_period
	echo 0 > /proc/sys/vm/use_pingpong_reduce
	echo 500 > /proc/sys/vm/pingpong_reduce_threshold
	echo 1 > /proc/sys/vm/scanless_cooling
	echo 1 > /proc/sys/vm/reduce_scan
	echo always > /sys/kernel/mm/transparent_hugepage/enabled

	./run_multi_tenants.sh $1 2>&1 | cat > ./evaluation/region_pingpong_$1_$2_$3.txt
	dmesg > ./evaluation/region_pingpong_$1_$2_$3_dmesg.txt
}



function run_mttm_region_basepage_opt
{
	dmesg --clear
	echo 1 > /proc/sys/vm/use_dram_determination
	echo 1 > /proc/sys/vm/use_region_separation
	echo 0 > /proc/sys/vm/use_hotness_intensity
	echo 0 > /proc/sys/vm/use_memstrata_policy
	echo $2 > /proc/sys/vm/mttm_local_dram_string
	echo 0 > /proc/sys/vm/print_more_info

	echo 199 > /proc/sys/vm/pebs_sample_period
	echo 1 > /proc/sys/vm/use_pingpong_reduce
	echo 200 > /proc/sys/vm/pingpong_reduce_threshold
	echo 1 > /proc/sys/vm/scanless_cooling
	echo 1 > /proc/sys/vm/reduce_scan
	echo madvise > /sys/kernel/mm/transparent_hugepage/enabled
	echo 9 > /proc/sys/vm/basepage_shift_factor #target cooling period
	echo 40 > /proc/sys/vm/basepage_period_factor #increasing granularity

	./run_multi_tenants.sh $1 2>&1 | cat > ./evaluation/basepage/region_$1_$2_$3_9_test.txt
	dmesg > ./evaluation/basepage/region_$1_$2_$3_9_dmesg_test.txt
}

function run_mttm_region_basepage_scan
{
	dmesg --clear
	echo 1 > /proc/sys/vm/use_dram_determination
	echo 1 > /proc/sys/vm/use_region_separation
	echo 0 > /proc/sys/vm/use_hotness_intensity
	echo 0 > /proc/sys/vm/use_memstrata_policy
	echo $2 > /proc/sys/vm/mttm_local_dram_string
	echo 0 > /proc/sys/vm/print_more_info

	echo 199 > /proc/sys/vm/pebs_sample_period
	echo 1 > /proc/sys/vm/use_pingpong_reduce
	echo 200 > /proc/sys/vm/pingpong_reduce_threshold
	echo 0 > /proc/sys/vm/scanless_cooling
	echo 0 > /proc/sys/vm/reduce_scan
	echo madvise > /sys/kernel/mm/transparent_hugepage/enabled
	echo 9 > /proc/sys/vm/basepage_shift_factor #target cooling period
	echo 40 > /proc/sys/vm/basepage_period_factor #increasing granularity

	./run_multi_tenants.sh $1 2>&1 | cat > ./evaluation/basepage/region_scan_$1_$2_$3.txt
	dmesg > ./evaluation/basepage/region_scan_$1_$2_$3_dmesg.txt
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
	echo 0 > /proc/sys/vm/print_more_info

	echo 10007 > /proc/sys/vm/pebs_sample_period
	echo 1 > /proc/sys/vm/use_pingpong_reduce
	echo 500 > /proc/sys/vm/pingpong_reduce_threshold
	echo 1 > /proc/sys/vm/scanless_cooling
	echo 1 > /proc/sys/vm/reduce_scan
	echo always > /sys/kernel/mm/transparent_hugepage/enabled

	./run_multi_tenants.sh $1 2>&1 | cat > ./evaluation/hi_$1_$2_$3.txt
	dmesg > ./evaluation/hi_$1_$2_$3_dmesg.txt
}

function run_naive_hi_basepage_opt
{
	dmesg --clear
	echo 1 > /proc/sys/vm/use_dram_determination
	echo 0 > /proc/sys/vm/use_region_separation
	echo 1 > /proc/sys/vm/use_hotness_intensity
	echo 1 > /proc/sys/vm/use_naive_hi
	echo 0 > /proc/sys/vm/use_memstrata_policy
	echo $2 > /proc/sys/vm/mttm_local_dram_string
	echo 0 > /proc/sys/vm/print_more_info

	echo 199 > /proc/sys/vm/pebs_sample_period
	echo 1 > /proc/sys/vm/use_pingpong_reduce
	echo 200 > /proc/sys/vm/pingpong_reduce_threshold
	echo 1 > /proc/sys/vm/scanless_cooling
	echo 1 > /proc/sys/vm/reduce_scan
	echo madvise > /sys/kernel/mm/transparent_hugepage/enabled
	echo 9 > /proc/sys/vm/basepage_shift_factor #target cooling period
	echo 40 > /proc/sys/vm/basepage_period_factor #increasing granularity

	./run_multi_tenants.sh $1 2>&1 | cat > ./evaluation/basepage/hi_$1_$2_$3_9.txt
	dmesg > ./evaluation/basepage/hi_$1_$2_$3_9_dmesg.txt
}



function run_vtmm_validation
{

	echo always > /sys/kernel/mm/transparent_hugepage/enabled
	./run_multi_tenants_native.sh vtmm_valid_pr-27 2>&1 | cat > ./evaluation/vtmm_validation/local_pr-27.txt

	echo always > /sys/kernel/mm/transparent_hugepage/enabled
	./run_multi_tenants_native.sh vtmm_valid_pr-29 2>&1 | cat > ./evaluation/vtmm_validation/local_pr-29.txt

	echo always > /sys/kernel/mm/transparent_hugepage/enabled
	./run_multi_tenants_native.sh vtmm_valid_bc-27 2>&1 | cat > ./evaluation/vtmm_validation/local_bc-27.txt

	echo always > /sys/kernel/mm/transparent_hugepage/enabled
	./run_multi_tenants_native.sh vtmm_valid_bc-29 2>&1 | cat > ./evaluation/vtmm_validation/local_bc-29.txt

	echo always > /sys/kernel/mm/transparent_hugepage/enabled
	./run_multi_tenants_native.sh vtmm_valid_graph500-25 2>&1 | cat > ./evaluation/vtmm_validation/local_graph500-25.txt

	echo always > /sys/kernel/mm/transparent_hugepage/enabled
	./run_multi_tenants_native.sh vtmm_valid_graph500-27 2>&1 | cat > ./evaluation/vtmm_validation/local_graph500-27.txt
: << end
	dmesg --clear
	echo 32G > /proc/sys/vm/mttm_local_dram_string
	./run_multi_tenants_vtmm.sh 1 gapbs-pr-27 2>&1 | cat > ./evaluation/vtmm_validation/vtmm_pr-27_32G.txt
	dmesg > ./evaluation/vtmm_validation/vtmm_pr-27_32G_dmesg.txt

	dmesg --clear
	echo 32G > /proc/sys/vm/mttm_local_dram_string
	./run_multi_tenants_vtmm.sh 1 gapbs-pr-29 2>&1 | cat > ./evaluation/vtmm_validation/vtmm_pr-29_32G.txt
	dmesg > ./evaluation/vtmm_validation/vtmm_pr-29_32G_dmesg.txt

	dmesg --clear
	echo 32G > /proc/sys/vm/mttm_local_dram_string
	./run_multi_tenants_vtmm.sh 1 gapbs-bc-27 2>&1 | cat > ./evaluation/vtmm_validation/vtmm_bc-27_32G.txt
	dmesg > ./evaluation/vtmm_validation/vtmm_bc-27_32G_dmesg.txt

	dmesg --clear
	echo 32G > /proc/sys/vm/mttm_local_dram_string
	./run_multi_tenants_vtmm.sh 1 gapbs-bc-29 2>&1 | cat > ./evaluation/vtmm_validation/vtmm_bc-29_32G.txt
	dmesg > ./evaluation/vtmm_validation/vtmm_bc-29_32G_dmesg.txt

	dmesg --clear
	echo 32G > /proc/sys/vm/mttm_local_dram_string
	./run_multi_tenants_vtmm.sh 1 graph500-25 2>&1 | cat > ./evaluation/vtmm_validation/vtmm_graph500-25_32G.txt
	dmesg > ./evaluation/vtmm_validation/vtmm_graph500-25_32G_dmesg.txt

	dmesg --clear
	echo 32G > /proc/sys/vm/mttm_local_dram_string
	./run_multi_tenants_vtmm.sh 1 graph500-27 2>&1 | cat > ./evaluation/vtmm_validation/vtmm_graph500-27_32G.txt
	dmesg > ./evaluation/vtmm_validation/vtmm_graph500-27_32G_dmesg.txt
end

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



function run_memstrata_hugepage
{
	dmesg --clear
	echo 0 > /proc/sys/vm/use_region_separation
	echo 0 > /proc/sys/vm/use_hotness_intensity
	echo 0 > /proc/sys/vm/use_naive_hi
	echo 0 > /proc/sys/vm/use_dram_determination
	echo 1 > /proc/sys/vm/use_memstrata_policy
	echo 40 > /proc/sys/vm/acceptor_threshold

	echo 10007 > /proc/sys/vm/pebs_sample_period
	echo 0 > /proc/sys/vm/use_pingpong_reduce
	echo 1 > /proc/sys/vm/scanless_cooling
	echo 1 > /proc/sys/vm/reduce_scan
	echo always > /sys/kernel/mm/transparent_hugepage/enabled

	echo 1 > /proc/sys/vm/print_more_info
	echo $2 > /proc/sys/vm/mttm_local_dram_string

	./run_multi_tenants.sh $1 2>&1 | cat > ./evaluation/fmmr_$1_$2_$3.txt
	dmesg > ./evaluation/fmmr_$1_$2_$3_dmesg.txt
}

function run_memstrata_basepage_opt
{
	dmesg --clear
	echo 0 > /proc/sys/vm/use_dram_determination
	echo 0 > /proc/sys/vm/use_region_separation
	echo 0 > /proc/sys/vm/use_hotness_intensity
	echo 0 > /proc/sys/vm/use_naive_hi
	echo 1 > /proc/sys/vm/use_memstrata_policy
	echo 40 > /proc/sys/vm/acceptor_threshold

	echo 1 > /proc/sys/vm/print_more_info
	echo $2 > /proc/sys/vm/mttm_local_dram_string

	echo 199 > /proc/sys/vm/pebs_sample_period
	echo 0 > /proc/sys/vm/use_pingpong_reduce
	echo 1 > /proc/sys/vm/scanless_cooling
	echo 1 > /proc/sys/vm/reduce_scan
	echo madvise > /sys/kernel/mm/transparent_hugepage/enabled
	echo 9 > /proc/sys/vm/basepage_shift_factor #target cooling period
	echo 40 > /proc/sys/vm/basepage_period_factor #increasing granularity

	./run_multi_tenants.sh $1 2>&1 | cat > ./evaluation/basepage/fmmr_$1_$2_$3_9.txt
	dmesg > ./evaluation/basepage/fmmr_$1_$2_$3_9_dmesg.txt
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

#config1, 2, 3, 12

set_192
source $conda_activate dlrm_cpu
run_mttm_region_hugepage config12 60G 192
#run_mttm_region_basepage_opt config1 54G 130
#run_mttm_region_basepage_opt config2 13G 130
#run_mttm_region_basepage_opt config12 23G 130
#run_mttm_region_basepage_opt config3 33G 130

: << end
set_130
source $conda_activate dlrm_cpu
run_memstrata_hugepage 12tenants 85G 130
run_vtmm_hugepage 12tenants 85G 130

conda deactivate
set_192
source $conda_activate dlrm_cpu
run_memstrata_hugepage 12tenants 85G 192
run_vtmm_hugepage 12tenants 85G 192

conda deactivate
set_250
source $conda_activate dlrm_cpu
#run_memstrata_hugepage 12tenants 85G 250
#run_vtmm_hugepage 12tenants 85G 250


#run_naive_hi_hugepage 12tenants 85G 130
end


: << end
set_192
source $conda_activate dlrm_cpu
#run_mttm_region_hugepage 6tenants 42G 192
run_naive_hi_hugepage 6tenants 42G 192
run_memstrata_hugepage 6tenants 42G 192
run_vtmm_hugepage 6tenants 42G 192

conda deactivate
set_250
source $conda_activate dlrm_cpu
run_mttm_region_hugepage 6tenants 42G 250
run_naive_hi_hugepage 6tenants 42G 250
run_memstrata_hugepage 6tenants 42G 250
run_vtmm_hugepage 6tenants 42G 250
end

# FMMR
: << end
set_130
run_memstrata_hugepage config1 54G 130
run_memstrata_hugepage config1 21G 130
source $conda_activate dlrm_cpu
#run_memstrata_hugepage config2 34G 130
#run_memstrata_hugepage config2 13G 130
run_memstrata_hugepage config3 84G 130
run_memstrata_hugepage config3 33G 130
run_memstrata_hugepage config12 60G 130
run_memstrata_hugepage config12 24G 130

conda deactivate
set_192
run_memstrata_hugepage config1 54G 192
run_memstrata_hugepage config1 21G 192
source $conda_activate dlrm_cpu
run_memstrata_hugepage config2 34G 192
run_memstrata_hugepage config2 13G 192
run_memstrata_hugepage config3 84G 192
run_memstrata_hugepage config3 33G 192
run_memstrata_hugepage config12 60G 192
run_memstrata_hugepage config12 24G 192

conda deactivate
set_250
run_memstrata_hugepage config1 54G 250
run_memstrata_hugepage config1 21G 250
source $conda_activate dlrm_cpu
run_memstrata_hugepage config2 34G 250
run_memstrata_hugepage config2 13G 250
run_memstrata_hugepage config3 84G 250
run_memstrata_hugepage config3 33G 250
run_memstrata_hugepage config12 60G 250
run_memstrata_hugepage config12 24G 250
end

# Hotness intensity
#130
: << end
set_130
run_naive_hi_hugepage config1 54G 130
run_naive_hi_hugepage config1 21G 130
source $conda_activate dlrm_cpu
run_naive_hi_hugepage config2 34G 130
run_naive_hi_hugepage config2 13G 130
run_naive_hi_hugepage config3 84G 130
run_naive_hi_hugepage config3 33G 130
run_naive_hi_hugepage config12 60G 130
run_naive_hi_hugepage config12 24G 130


#192
conda deactivate
set_192
run_naive_hi_hugepage config1 54G 192
run_naive_hi_hugepage config1 21G 192
source $conda_activate dlrm_cpu
run_naive_hi_hugepage config2 34G 192
run_naive_hi_hugepage config2 13G 192
run_naive_hi_hugepage config3 84G 192
run_naive_hi_hugepage config3 33G 192
run_naive_hi_hugepage config12 60G 192
run_naive_hi_hugepage config12 24G 192


#250
conda deactivate
set_250
run_naive_hi_hugepage config1 54G 250
run_naive_hi_hugepage config1 21G 250
source $conda_activate dlrm_cpu
run_naive_hi_hugepage config2 34G 250
run_naive_hi_hugepage config2 13G 250
run_naive_hi_hugepage config3 84G 250
run_naive_hi_hugepage config3 33G 250
run_naive_hi_hugepage config12 60G 250
run_naive_hi_hugepage config12 24G 250
end

# MTTM region
#130
: << end

conda deactivate
set_130
run_mttm_region_hugepage config1 54G 130
run_mttm_region_hugepage config1 21G 130
source $conda_activate dlrm_cpu
run_mttm_region_hugepage config2 50G 130
run_mttm_region_hugepage config2 20G 130
run_mttm_region_hugepage config3 84G 130
run_mttm_region_hugepage config3 33G 130

run_mttm_region_hugepage config9 46G 130
run_mttm_region_hugepage config9 18G 130

run_mttm_region_hugepage config11 45G 130
run_mttm_region_hugepage config11 18G 130
source $conda_activate dlrm_cpu
run_mttm_region_hugepage config12 60G 130
run_mttm_region_hugepage config12 24G 130
end

# 192
: << end

conda deactivate
set_192
run_mttm_region_hugepage config1 54G 192
run_mttm_region_hugepage config1 21G 192
source $conda_activate dlrm_cpu
run_mttm_region_hugepage config2 50G 192
run_mttm_region_hugepage config2 20G 192
run_mttm_region_hugepage config3 84G 192
run_mttm_region_hugepage config3 33G 192

run_mttm_region_hugepage config9 46G 192
run_mttm_region_hugepage config9 18G 192

run_mttm_region_hugepage config11 45G 192
run_mttm_region_hugepage config11 18G 192
source $conda_activate dlrm_cpu
run_mttm_region_hugepage config12 60G 192
run_mttm_region_hugepage config12 24G 192
end

# 250
: << end
conda deactivate
set_250

run_mttm_region_hugepage config1 54G 250
run_mttm_region_hugepage config1 21G 250
source $conda_activate dlrm_cpu
run_mttm_region_hugepage config2 50G 250
run_mttm_region_hugepage config2 20G 250
run_mttm_region_hugepage config3 84G 250
run_mttm_region_hugepage config3 33G 250

run_mttm_region_hugepage config9 46G 250
run_mttm_region_hugepage config9 18G 250
set_250
source $conda_activate dlrm_cpu
run_mttm_region_hugepage config11 45G 250
#run_mttm_region_hugepage config11 18G 250
source $conda_activate dlrm_cpu
#run_mttm_region_hugepage config12 60G 250
run_mttm_region_hugepage config12 24G 250
end





# vTMM
: << END
#130
#conda deactivate
set_130
: << end
run_vtmm_hugepage config1 54G 130
run_vtmm_hugepage config1 21G 130
end
source $conda_activate dlrm_cpu
run_vtmm_hugepage config2 50G 130
run_vtmm_hugepage config2 20G 130

run_vtmm_hugepage config12 60G 130
run_vtmm_hugepage config12 24G 130

: << end
run_vtmm_hugepage config3 84G 130
run_vtmm_hugepage config3 33G 130

run_vtmm_hugepage config9 46G 130
run_vtmm_hugepage config9 18G 130

source $conda_activate dlrm_cpu
run_vtmm_hugepage config11 45G 130
run_vtmm_hugepage config11 18G 130
end

#192
conda deactivate
set_192
#run_vtmm_hugepage config1 54G 192
#run_vtmm_hugepage config1 21G 192

source $conda_activate dlrm_cpu
run_vtmm_hugepage config2 50G 192
run_vtmm_hugepage config2 20G 192

run_vtmm_hugepage config12 60G 192
run_vtmm_hugepage config12 24G 192

: << end
run_vtmm_hugepage config3 84G 192
run_vtmm_hugepage config3 33G 192

run_vtmm_hugepage config9 46G 192
run_vtmm_hugepage config9 18G 192

source $conda_activate dlrm_cpu
run_vtmm_hugepage config11 45G 192
run_vtmm_hugepage config11 18G 192
end

#250
conda deactivate
set_250
#run_vtmm_hugepage config1 54G 250
#run_vtmm_hugepage config1 21G 250

source $conda_activate dlrm_cpu
run_vtmm_hugepage config2 50G 250
run_vtmm_hugepage config2 20G 250

run_vtmm_hugepage config12 60G 250
run_vtmm_hugepage config12 24G 250

: << end
run_vtmm_hugepage config3 84G 250
run_vtmm_hugepage config3 33G 250

run_vtmm_hugepage config9 46G 250
run_vtmm_hugepage config9 18G 250

source $conda_activate dlrm_cpu
run_vtmm_hugepage config11 45G 250
run_vtmm_hugepage config11 18G 250
end
END


# MTTM basepage
set_130
: << end
#run_local_basepage config1 130
run_mttm_region_basepage_opt config1 54G 130
run_mttm_region_basepage_opt config1 21G 130
source $conda_activate dlrm_cpu
#run_local_basepage config2 130
run_mttm_region_basepage_opt config2 34G 130
run_mttm_region_basepage_opt config2 13G 130
run_mttm_region_basepage_opt config12 58G 130
run_mttm_region_basepage_opt config12 23G 130

#run_local_basepage config3 130
run_mttm_region_basepage_opt config3 84G 130
run_mttm_region_basepage_opt config3 33G 130

source $conda_activate dlrm_cpu
#run_local_basepage config11 130
run_mttm_region_basepage_opt config11 42G 130
run_mttm_region_basepage_opt config11 17G 130
end

: << end
set_130
run_naive_hi_basepage_opt config1 54G 130
run_naive_hi_basepage_opt config1 21G 130
#source $conda_activate dlrm_cpu
run_naive_hi_basepage_opt config2 34G 130
run_naive_hi_basepage_opt config2 13G 130
run_naive_hi_basepage_opt config12 58G 130
run_naive_hi_basepage_opt config12 23G 130

run_naive_hi_basepage_opt config3 84G 130
run_naive_hi_basepage_opt config3 33G 130

run_naive_hi_basepage_opt config11 42G 130
run_naive_hi_basepage_opt config11 17G 130
end

set_130


