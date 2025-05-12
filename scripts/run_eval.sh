#!/bin/bash
cur_path=$PWD
emul_path=/home/cjlee/CXL-emulation.code/NUMA_setting/slow-memory-emulation
conda_activate=/root/anaconda3/bin/activate

function run_mttm_hugepage
{
	#config, dram size, remote latency
	dmesg --clear
	echo 1 > /proc/sys/vm/use_dram_determination
	echo 1 > /proc/sys/vm/use_region_separation
	echo 0 > /proc/sys/vm/use_hotness_intensity
	echo 0 > /proc/sys/vm/use_memstrata_policy
	echo $2 > /proc/sys/vm/mttm_local_dram_string
	echo 0 > /proc/sys/vm/print_more_info

	echo 1 > /proc/sys/vm/mar_weight
	echo 1 > /proc/sys/vm/hi_weight
	echo 3 > /proc/sys/vm/hugepage_shift_factor
	echo 1 > /proc/sys/vm/hugepage_period_factor

#	echo 4999 > /proc/sys/vm/pebs_sample_period
	echo 1 > /proc/sys/vm/use_pingpong_reduce
	echo 500 > /proc/sys/vm/pingpong_reduce_threshold
	echo 1 > /proc/sys/vm/scanless_cooling
	echo 1 > /proc/sys/vm/reduce_scan
	echo always > /sys/kernel/mm/transparent_hugepage/enabled

	./run_multi_tenants.sh $1 2>&1 | cat > ./evaluation/main/mttm/$1/$2_$3_test.txt
	dmesg > ./evaluation/main/mttm/$1/$2_$3_dmesg_test.txt
}

function run_static_hugepage
{
	#config, dram size, remote latency
	dmesg --clear
	echo 0 > /proc/sys/vm/use_dram_determination
	echo 0 > /proc/sys/vm/use_region_separation
	echo 0 > /proc/sys/vm/use_hotness_intensity
	echo 0 > /proc/sys/vm/use_memstrata_policy
	echo $2 > /proc/sys/vm/mttm_local_dram_string
	echo 0 > /proc/sys/vm/print_more_info

	echo 1 > /proc/sys/vm/mar_weight
	echo 1 > /proc/sys/vm/hi_weight
	echo 3 > /proc/sys/vm/hugepage_shift_factor
	echo 1 > /proc/sys/vm/hugepage_period_factor

	echo 10007 > /proc/sys/vm/pebs_sample_period
	echo 1 > /proc/sys/vm/use_pingpong_reduce
	echo 500 > /proc/sys/vm/pingpong_reduce_threshold
	echo 1 > /proc/sys/vm/scanless_cooling
	echo 1 > /proc/sys/vm/reduce_scan
	echo always > /sys/kernel/mm/transparent_hugepage/enabled

	./run_multi_tenants.sh $1 2>&1 | cat > ./evaluation/main/static/$1_$2_$3.txt
	dmesg > ./evaluation/main/static/$1_$2_$3_dmesg.txt
}

function run_mttm_sensitivity_mar_hi
{
	#config, dram size, mar weight, hi weight
	dmesg --clear
	echo 1 > /proc/sys/vm/use_dram_determination
	echo 1 > /proc/sys/vm/use_region_separation
	echo 0 > /proc/sys/vm/use_hotness_intensity
	echo 0 > /proc/sys/vm/use_memstrata_policy
	echo $2 > /proc/sys/vm/mttm_local_dram_string
	echo 0 > /proc/sys/vm/print_more_info

	echo $3 > /proc/sys/vm/mar_weight
	echo $4 > /proc/sys/vm/hi_weight
	echo 3 > /proc/sys/vm/hugepage_shift_factor
	echo 1 > /proc/sys/vm/hugepage_period_factor

	echo 10007 > /proc/sys/vm/pebs_sample_period
	echo 1 > /proc/sys/vm/use_pingpong_reduce
	echo 500 > /proc/sys/vm/pingpong_reduce_threshold
	echo 1 > /proc/sys/vm/scanless_cooling
	echo 1 > /proc/sys/vm/reduce_scan
	echo always > /sys/kernel/mm/transparent_hugepage/enabled

	./run_multi_tenants.sh $1 2>&1 | cat > ./evaluation/sensitivity/mar_hi/$1_$2_$3_$4.txt
	dmesg > ./evaluation/sensitivity/mar_hi/$1_$2_$3_$4_dmesg.txt
}

function run_mttm_sensitivity_cooling
{
	#config, dram size, shift_factor
	dmesg --clear
	echo 1 > /proc/sys/vm/use_dram_determination
	echo 1 > /proc/sys/vm/use_region_separation
	echo 0 > /proc/sys/vm/use_hotness_intensity
	echo 0 > /proc/sys/vm/use_memstrata_policy
	echo $2 > /proc/sys/vm/mttm_local_dram_string
	echo 0 > /proc/sys/vm/print_more_info

	echo 1 > /proc/sys/vm/mar_weight
	echo 1 > /proc/sys/vm/hi_weight
	echo $3 > /proc/sys/vm/hugepage_shift_factor
	echo 1 > /proc/sys/vm/hugepage_period_factor

	echo 10007 > /proc/sys/vm/pebs_sample_period
	echo 1 > /proc/sys/vm/use_pingpong_reduce
	echo 500 > /proc/sys/vm/pingpong_reduce_threshold
	echo 1 > /proc/sys/vm/scanless_cooling
	echo 1 > /proc/sys/vm/reduce_scan
	echo always > /sys/kernel/mm/transparent_hugepage/enabled

	./run_multi_tenants.sh $1 2>&1 | cat > ./evaluation/sensitivity/cooling/$1_$2_$3.txt
	dmesg > ./evaluation/sensitivity/cooling/$1_$2_$3_dmesg.txt
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

function run_hi_first_hugepage 
{
	dmesg --clear
	echo 1 > /proc/sys/vm/use_dram_determination
	echo 0 > /proc/sys/vm/use_region_separation
	echo 1 > /proc/sys/vm/use_hotness_intensity
	echo 1 > /proc/sys/vm/use_hi_first
	echo 0 > /proc/sys/vm/use_mar_first
	echo 0 > /proc/sys/vm/use_memstrata_policy
	echo $2 > /proc/sys/vm/mttm_local_dram_string
	echo 0 > /proc/sys/vm/print_more_info

	echo 1 > /proc/sys/vm/mar_weight
	echo 1 > /proc/sys/vm/hi_weight
	echo 3 > /proc/sys/vm/hugepage_shift_factor
	echo 1 > /proc/sys/vm/hugepage_period_factor

	echo 10007 > /proc/sys/vm/pebs_sample_period
	echo 1 > /proc/sys/vm/use_pingpong_reduce
	echo 500 > /proc/sys/vm/pingpong_reduce_threshold
	echo 1 > /proc/sys/vm/scanless_cooling
	echo 1 > /proc/sys/vm/reduce_scan
	echo always > /sys/kernel/mm/transparent_hugepage/enabled

	./run_multi_tenants.sh $1 2>&1 | cat > ./evaluation/main/hi_first/$1_$2_$3_3.txt
	dmesg > ./evaluation/main/hi_first/$1_$2_$3_3_dmesg.txt
}

function run_mar_first_hugepage 
{
	dmesg --clear
	echo 1 > /proc/sys/vm/use_dram_determination
	echo 0 > /proc/sys/vm/use_region_separation
	echo 1 > /proc/sys/vm/use_hotness_intensity
	echo 0 > /proc/sys/vm/use_hi_first
	echo 1 > /proc/sys/vm/use_mar_first
	echo 0 > /proc/sys/vm/use_memstrata_policy
	echo $2 > /proc/sys/vm/mttm_local_dram_string
	echo 0 > /proc/sys/vm/print_more_info

	echo 1 > /proc/sys/vm/mar_weight
	echo 1 > /proc/sys/vm/hi_weight
	echo 3 > /proc/sys/vm/hugepage_shift_factor
	echo 1 > /proc/sys/vm/hugepage_period_factor

	echo 10007 > /proc/sys/vm/pebs_sample_period
	echo 1 > /proc/sys/vm/use_pingpong_reduce
	echo 500 > /proc/sys/vm/pingpong_reduce_threshold
	echo 1 > /proc/sys/vm/scanless_cooling
	echo 1 > /proc/sys/vm/reduce_scan
	echo always > /sys/kernel/mm/transparent_hugepage/enabled

	./run_multi_tenants.sh $1 2>&1 | cat > ./evaluation/main/mar_first/$1_$2_$3.txt
	dmesg > ./evaluation/main/mar_first/$1_$2_$3_dmesg.txt
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
	./run_multi_tenants_native.sh $1 2>&1 | cat > ./evaluation/main/local/$1.txt
}

function run_local_basepage
{
	echo madvise > /sys/kernel/mm/transparent_hugepage/enabled
	./run_multi_tenants_native.sh $1 2>&1 | cat > ./evaluation/basepage/local_$1.txt
}


function run_mttm_qos
{
	# workload, qos_wss_factor, remote latency
	dmesg --clear
	echo 1 > /proc/sys/vm/use_dram_determination
	echo 1 > /proc/sys/vm/use_region_separation
	echo 0 > /proc/sys/vm/use_hotness_intensity
	echo 0 > /proc/sys/vm/use_memstrata_policy
	echo 32G > /proc/sys/vm/mttm_local_dram_string
	echo 0 > /proc/sys/vm/print_more_info
	echo $2 > /proc/sys/vm/qos_wss_factor

	echo 1 > /proc/sys/vm/mar_weight
	echo 1 > /proc/sys/vm/hi_weight
	echo 3 > /proc/sys/vm/hugepage_shift_factor
	echo 1 > /proc/sys/vm/hugepage_period_factor

#	echo 4999 > /proc/sys/vm/pebs_sample_period
	echo 1 > /proc/sys/vm/use_pingpong_reduce
	echo 500 > /proc/sys/vm/pingpong_reduce_threshold
	echo 1 > /proc/sys/vm/scanless_cooling
	echo 1 > /proc/sys/vm/reduce_scan
	echo always > /sys/kernel/mm/transparent_hugepage/enabled

	./run_multi_tenants.sh 1 $1 2>&1 | cat > ./evaluation/qos/wss/$1_$2_$3.txt
	dmesg > ./evaluation/qos/wss/$1_$2_$3_dmesg.txt
}



function run_qos
{
	#echo always > /sys/kernel/mm/transparent_hugepage/enabled
	#./run_multi_tenants_native.sh 1 $1 2>&1 | cat > ./evaluation/qos/local/$1.txt

	run_mttm_qos $1 80 190
	run_mttm_qos $1 100 190
	run_mttm_qos $1 120 190
}

function run_static_bw
{
	#config, remote latency
	dmesg --clear
	echo 0 > /proc/sys/vm/use_dram_determination
	echo 0 > /proc/sys/vm/use_region_separation
	echo 0 > /proc/sys/vm/use_hotness_intensity
	echo 0 > /proc/sys/vm/use_memstrata_policy
	echo 1 > /proc/sys/vm/print_more_info

	echo 1 > /proc/sys/vm/mar_weight
	echo 1 > /proc/sys/vm/hi_weight
	echo 3 > /proc/sys/vm/hugepage_shift_factor
	echo 1 > /proc/sys/vm/hugepage_period_factor

	echo 10007 > /proc/sys/vm/pebs_sample_period
	echo 1 > /proc/sys/vm/use_pingpong_reduce
	echo 500 > /proc/sys/vm/pingpong_reduce_threshold
	echo 1 > /proc/sys/vm/scanless_cooling
	echo 1 > /proc/sys/vm/reduce_scan
	echo always > /sys/kernel/mm/transparent_hugepage/enabled

	./run_multi_tenants.sh $1 2>&1 | cat > ./evaluation/bw/$1_$2.txt
	dmesg > ./evaluation/bw/$1_$2_dmesg.txt
}


function run_tpp_hugepage
{
	#config, dram size, remote latency
	dmesg --clear
	COLLOID_MODULE=/home/cjlee/colloid/tpp
	if [[ "$2" == "54G" ]]; then
		memeater_size=135000
	elif [[ "$2" == "21G" ]]; then
		memeater_size=168000
	elif [[ "$2" == "34G" ]]; then
		memeater_size=155000
	elif [[ "$2" == "13G" ]]; then
		memeater_size=176000
	elif [[ "$2" == "45G" ]]; then
		memeater_size=144000
	elif [[ "$2" == "18G" ]]; then
		memeater_size=171000
	elif [[ "$2" == "51G" ]]; then
		memeater_size=138000
	elif [[ "$2" == "20G" ]]; then
		memeater_size=169000
	fi

	echo 1 > /proc/sys/vm/drop_caches	
	insmod ${COLLOID_MODULE}/memeater/memeater.ko sizeMiB=${memeater_size}
	swapoff -a
	echo 1 > /sys/kernel/mm/numa/demotion_enabled
	echo 2 > /proc/sys/kernel/numa_balancing

	echo always > /sys/kernel/mm/transparent_hugepage/enabled
	cat /proc/vmstat | grep -e promote -e demote -e migrate -e hint > ./evaluation/main/tpp/$1/$2_$3_before_vmstat.txt
	./run_multi_tenants_tpp.sh $1 2>&1 | cat > ./evaluation/main/tpp/$1/$2_$3.txt
	dmesg > ./evaluation/main/tpp/$1/$2_$3_dmesg.txt
	cat /proc/vmstat | grep -e promote -e demote -e migrate -e hint > ./evaluation/main/tpp/$1/$2_$3_after_vmstat.txt

	rmmod memeater
}

function run_colloid_hugepage
{
	#config, dram size, remote latency
	dmesg --clear
	COLLOID_MODULE=/home/cjlee/colloid/tpp
	if [[ "$2" == "54G" ]]; then
		memeater_size=135000
	elif [[ "$2" == "21G" ]]; then
		memeater_size=168000
	elif [[ "$2" == "34G" ]]; then
		memeater_size=155000
	elif [[ "$2" == "13G" ]]; then
		memeater_size=176000
	elif [[ "$2" == "45G" ]]; then
		memeater_size=144000
	elif [[ "$2" == "18G" ]]; then
		memeater_size=171000
	elif [[ "$2" == "51G" ]]; then
		memeater_size=138000
	elif [[ "$2" == "20G" ]]; then
		memeater_size=169000
	fi

	echo 1 > /proc/sys/vm/drop_caches	
	insmod ${COLLOID_MODULE}/memeater/memeater.ko sizeMiB=${memeater_size}
	insmod ${COLLOID_MODULE}/colloid-mon/colloid-mon.ko
	swapoff -a
	echo 1 > /sys/kernel/mm/numa/demotion_enabled
	echo 6 > /proc/sys/kernel/numa_balancing

	echo always > /sys/kernel/mm/transparent_hugepage/enabled
	cat /proc/vmstat | grep -e promote -e demote -e migrate -e hint > ./evaluation/main/colloid/$1/$2_$3_before_vmstat.txt
	./run_multi_tenants_tpp.sh $1 2>&1 | cat > ./evaluation/main/colloid/$1/$2_$3.txt
	dmesg > ./evaluation/main/colloid/$1/$2_$3_dmesg.txt
	cat /proc/vmstat | grep -e promote -e demote -e migrate -e hint > ./evaluation/main/colloid/$1/$2_$3_after_vmstat.txt

	rmmod memeater
	rmmod colloid-mon
}


function run_memstrata_hugepage
{
	dmesg --clear
	echo 0 > /proc/sys/vm/use_region_separation
	echo 0 > /proc/sys/vm/use_hotness_intensity
	echo 0 > /proc/sys/vm/use_dram_determination
	echo 1 > /proc/sys/vm/use_memstrata_policy
	echo 20 > /proc/sys/vm/acceptor_threshold

	echo 3 > /proc/sys/vm/hugepage_shift_factor
	echo 1 > /proc/sys/vm/hugepage_period_factor

	echo 10007 > /proc/sys/vm/pebs_sample_period
	echo 1 > /proc/sys/vm/use_pingpong_reduce
	echo 500 > /proc/sys/vm/pingpong_reduce_threshold
	echo 1 > /proc/sys/vm/scanless_cooling
	echo 1 > /proc/sys/vm/reduce_scan
	echo always > /sys/kernel/mm/transparent_hugepage/enabled

	echo 1 > /proc/sys/vm/print_more_info
	echo $2 > /proc/sys/vm/mttm_local_dram_string

	./run_multi_tenants.sh $1 2>&1 | cat > ./evaluation/main/fmmr/$1/$2_$3.txt
	dmesg > ./evaluation/main/fmmr/$1/$2_$3_dmesg.txt
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
	./run_multi_tenants_vtmm.sh $1 2>&1 | cat > ./evaluation/main/vtmm/$1/$2_$3.txt
	dmesg > ./evaluation/main/vtmm/$1/$2_$3_dmesg.txt
}

function run_memtis_hugepage
{
	memtis_script=/home/cjlee/memtis/memtis-userspace/scripts/run_bench.sh
	${memtis_script} -C $1 2>&1 | cat > ./evaluation/main/memtis/$1/$2.txt
}

function set_130
{
	cd $emul_path
	./reset.sh
	cd $cur_path
	echo 130 > /proc/sys/vm/remote_latency
}

function set_160
{
	cd $emul_path
	./reset.sh
	./emulate.sh 6 4000
	cd $cur_path
	echo 160 > /proc/sys/vm/remote_latency
}

function set_190
{
	cd $emul_path
	./reset.sh
	./emulate.sh 11 4000
	cd $cur_path
	echo 190 > /proc/sys/vm/remote_latency
}

function set_220
{
	cd $emul_path
	./reset.sh
	./emulate.sh 24 0 0x8124
	cd $cur_path
	echo 220 > /proc/sys/vm/remote_latency
}
: << end
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
end

function run_sensitivity_mar_hi
{
	set_160
	run_mttm_sensitivity_mar_hi 3gups 24G 3 1
	run_mttm_sensitivity_mar_hi 3gups 24G 2 1
	run_mttm_sensitivity_mar_hi 3gups 24G 1 1
	run_mttm_sensitivity_mar_hi 3gups 24G 1 2
	run_mttm_sensitivity_mar_hi 3gups 24G 1 3
}

function run_sensitivity_cooling
{
	set_160
	run_mttm_sensitivity_cooling cooling 24G 1
	run_mttm_sensitivity_cooling cooling 24G 2
	run_mttm_sensitivity_cooling cooling 24G 3
	run_mttm_sensitivity_cooling cooling 24G 4
}

function run_main_mttm
{

	set_160
	#run_mttm_hugepage config1 54G 160
	run_mttm_hugepage config1 21G 160
	run_mttm_hugepage config2 34G 160
	run_mttm_hugepage config2 13G 160
	run_mttm_hugepage config12 45G 160
	run_mttm_hugepage config12 18G 160
	run_mttm_hugepage config13 51G 160
	run_mttm_hugepage config13 20G 160


	set_190
	run_mttm_hugepage config1 54G 190
	run_mttm_hugepage config1 21G 190
	run_mttm_hugepage config2 34G 190
	run_mttm_hugepage config2 13G 190
	run_mttm_hugepage config12 45G 190
	run_mttm_hugepage config12 18G 190
	run_mttm_hugepage config13 51G 190
	run_mttm_hugepage config13 20G 190

	set_220
	run_mttm_hugepage config1 54G 220
	run_mttm_hugepage config1 21G 220
	run_mttm_hugepage config2 34G 220
	run_mttm_hugepage config2 13G 220
	run_mttm_hugepage config12 45G 220
	run_mttm_hugepage config12 18G 220
	run_mttm_hugepage config13 51G 220
	run_mttm_hugepage config13 20G 220


	set_130
}

function run_main_static
{
: << end

	set_160
	run_static_hugepage config1-static1 54G 160
	run_static_hugepage config1-static4 21G 160
	run_static_hugepage config2-static1 34G 160
	run_static_hugepage config2-static4 13G 160
	run_static_hugepage config12-static1 60G 160
	run_static_hugepage config12-static4 24G 160
	run_static_hugepage config13-static1 51G 160
	run_static_hugepage config13-static4 20G 160

	set_190
	run_static_hugepage config1-static1 54G 190
	run_static_hugepage config1-static4 21G 190
	run_static_hugepage config2-static1 34G 190
	run_static_hugepage config2-static4 13G 190
	run_static_hugepage config12-static1 60G 190
	run_static_hugepage config12-static4 24G 190
	run_static_hugepage config13-static1 51G 190
	run_static_hugepage config13-static4 20G 190

end

	set_220
#	run_static_hugepage config1-static1 54G 220
#	run_static_hugepage config1-static4 21G 220
#	run_static_hugepage config2-static1 34G 220
#	run_static_hugepage config2-static4 13G 220
	run_static_hugepage config12-static1 45G 220
	run_static_hugepage config12-static4 18G 220
#	run_static_hugepage config13-static1 51G 220
#	run_static_hugepage config13-static4 20G 220


	set_130

}

function run_main_mar_first
{
: << end
	set_160
	run_mar_first_hugepage config1 54G 160
	run_mar_first_hugepage config1 21G 160
	run_mar_first_hugepage config2 34G 160
	run_mar_first_hugepage config2 13G 160
	run_mar_first_hugepage config12 60G 160
	run_mar_first_hugepage config12 24G 160
	run_mar_first_hugepage config13 51G 160
	run_mar_first_hugepage config13 20G 160
	set_190
	run_mar_first_hugepage config1 54G 190
	run_mar_first_hugepage config1 21G 190
	run_mar_first_hugepage config2 34G 190
	run_mar_first_hugepage config2 13G 190
	run_mar_first_hugepage config12 60G 190
	run_mar_first_hugepage config12 24G 190
	run_mar_first_hugepage config13 51G 190
	run_mar_first_hugepage config13 20G 190

end

	set_220
#	run_mar_first_hugepage config1 54G 220
#	run_mar_first_hugepage config1 21G 220
#	run_mar_first_hugepage config2 34G 220
#	run_mar_first_hugepage config2 13G 220
	run_mar_first_hugepage config12 45G 220
	run_mar_first_hugepage config12 18G 220
#	run_mar_first_hugepage config13 51G 220
#	run_mar_first_hugepage config13 20G 220


	set_130
}

function run_main_hi_first
{
: << end

	set_160
#	run_hi_first_hugepage config1 54G 160
#	run_hi_first_hugepage config1 21G 160
#	run_hi_first_hugepage config2 34G 160
#	run_hi_first_hugepage config2 13G 160
	run_hi_first_hugepage config12 45G 160
#	run_hi_first_hugepage config12 24G 160
#	run_hi_first_hugepage config13 51G 160
#	run_hi_first_hugepage config13 20G 160
	set_190
	run_hi_first_hugepage config1 54G 190
	run_hi_first_hugepage config1 21G 190
	run_hi_first_hugepage config2 34G 190
	run_hi_first_hugepage config2 13G 190
	run_hi_first_hugepage config12 60G 190
	run_hi_first_hugepage config12 24G 190
	run_hi_first_hugepage config13 51G 190
	run_hi_first_hugepage config13 20G 190
end

	set_220
#	run_hi_first_hugepage config1 54G 220
#	run_hi_first_hugepage config1 21G 220
#	run_hi_first_hugepage config2 34G 220
#	run_hi_first_hugepage config2 13G 220
	run_hi_first_hugepage config12 45G 220
	run_hi_first_hugepage config12 18G 220
#	run_hi_first_hugepage config13 51G 220
#	run_hi_first_hugepage config13 20G 220

	set_130
}

function run_main_local
{
	set_130
	#run_local_hugepage config1
	#run_local_hugepage config2
	run_local_hugepage config12
	#run_local_hugepage config13
}

function run_main_tpp
{
	set_160
	#run_tpp_hugepage config1 54G 160
	#run_tpp_hugepage config1 21G 160
	run_tpp_hugepage config2 34G 160
	run_tpp_hugepage config2 13G 160
	#run_tpp_hugepage config12 45G 160
	#run_tpp_hugepage config12 18G 160
	run_tpp_hugepage config13 51G 160
	run_tpp_hugepage config13 20G 160

	set_190
	#run_tpp_hugepage config1 54G 190
	#run_tpp_hugepage config1 21G 190
	run_tpp_hugepage config2 34G 190
	run_tpp_hugepage config2 13G 190
	#run_tpp_hugepage config12 45G 190
	#run_tpp_hugepage config12 18G 190
	run_tpp_hugepage config13 51G 190
	run_tpp_hugepage config13 20G 190

	set_220
	#run_tpp_hugepage config1 54G 220
	#run_tpp_hugepage config1 21G 220
	run_tpp_hugepage config2 34G 220
	run_tpp_hugepage config2 13G 220
	#run_tpp_hugepage config12 45G 220
	#run_tpp_hugepage config12 18G 220
	run_tpp_hugepage config13 51G 220
	run_tpp_hugepage config13 20G 220
}

function run_main_colloid
{
	set_160
	#run_colloid_hugepage config1 54G 160
	#run_colloid_hugepage config1 21G 160
	run_colloid_hugepage config2 34G 160
	run_colloid_hugepage config2 13G 160
	#run_colloid_hugepage config12 45G 160
	#run_colloid_hugepage config12 18G 160
	run_colloid_hugepage config13 51G 160
	run_colloid_hugepage config13 20G 160

	set_190
	#run_colloid_hugepage config1 54G 190
	#run_colloid_hugepage config1 21G 190
	run_colloid_hugepage config2 34G 190
	run_colloid_hugepage config2 13G 190
	#run_colloid_hugepage config12 45G 190
	#run_colloid_hugepage config12 18G 190
	run_colloid_hugepage config13 51G 190
	run_colloid_hugepage config13 20G 190

	set_220
	#run_colloid_hugepage config1 54G 220
	#run_colloid_hugepage config1 21G 220
	run_colloid_hugepage config2 34G 220
	run_colloid_hugepage config2 13G 220
	#run_colloid_hugepage config12 45G 220
	#run_colloid_hugepage config12 18G 220
	run_colloid_hugepage config13 51G 220
	run_colloid_hugepage config13 20G 220
}


function run_main_memstrata
{
	set_160
	#run_memstrata_hugepage config1 54G 160
	#run_memstrata_hugepage config1 21G 160
	#run_memstrata_hugepage config2 34G 160
	#run_memstrata_hugepage config2 13G 160
	run_memstrata_hugepage config12 45G 160
	run_memstrata_hugepage config12 18G 160
	#run_memstrata_hugepage config13 51G 160
	#run_memstrata_hugepage config13 20G 160


	set_190
	#run_memstrata_hugepage config1 54G 190
	#run_memstrata_hugepage config1 21G 190
	#run_memstrata_hugepage config2 34G 190
	#run_memstrata_hugepage config2 13G 190
	run_memstrata_hugepage config12 45G 190
	run_memstrata_hugepage config12 18G 190
	#run_memstrata_hugepage config13 51G 190
	#run_memstrata_hugepage config13 20G 190

	set_220
	#run_memstrata_hugepage config1 54G 220
	#run_memstrata_hugepage config1 21G 220
	#run_memstrata_hugepage config2 34G 220
	#run_memstrata_hugepage config2 13G 220
	run_memstrata_hugepage config12 45G 220
	run_memstrata_hugepage config12 18G 220
	#run_memstrata_hugepage config13 51G 220
	#run_memstrata_hugepage config13 20G 220


	set_130
}

function run_main_vtmm
{
	set_160
	#run_vtmm_hugepage config1 54G 160
	#run_vtmm_hugepage config1 21G 160
	#run_vtmm_hugepage config2 34G 160
	#run_vtmm_hugepage config2 13G 160
	#run_vtmm_hugepage config12 45G 160
	#run_vtmm_hugepage config12 18G 160
	#run_vtmm_hugepage config13 51G 160
	run_vtmm_hugepage config13 20G 160


	set_190
	run_vtmm_hugepage config1 54G 190
	run_vtmm_hugepage config1 21G 190
	run_vtmm_hugepage config2 34G 190
	run_vtmm_hugepage config2 13G 190
	run_vtmm_hugepage config12 45G 190
	run_vtmm_hugepage config12 18G 190
	run_vtmm_hugepage config13 51G 190
	run_vtmm_hugepage config13 20G 190

	set_220
	run_vtmm_hugepage config1 54G 220
	run_vtmm_hugepage config1 21G 220
	run_vtmm_hugepage config2 34G 220
	run_vtmm_hugepage config2 13G 220
	run_vtmm_hugepage config12 45G 220
	run_vtmm_hugepage config12 18G 220
	run_vtmm_hugepage config13 51G 220
	run_vtmm_hugepage config13 20G 220

	set_130
}

function run_main_memtis
{
: << end
	set_160
	run_memtis_hugepage config1-1 160
	run_memtis_hugepage config1-4 160
	run_memtis_hugepage config2-1 160
	run_memtis_hugepage config2-4 160
	run_memtis_hugepage config12-1 160
	run_memtis_hugepage config12-4 160
	run_memtis_hugepage config13-1 160
	run_memtis_hugepage config13-4 160
end
	set_190
	run_memtis_hugepage config1-1 190
	run_memtis_hugepage config1-4 190
	run_memtis_hugepage config2-1 190
	run_memtis_hugepage config2-4 190
	run_memtis_hugepage config12-1 190
	run_memtis_hugepage config12-4 190
	run_memtis_hugepage config13-1 190
	run_memtis_hugepage config13-4 190

	set_220
	run_memtis_hugepage config1-1 220
	run_memtis_hugepage config1-4 220
	run_memtis_hugepage config2-1 220
	run_memtis_hugepage config2-4 220
	run_memtis_hugepage config12-1 220
	run_memtis_hugepage config12-4 220
	run_memtis_hugepage config13-1 220
	run_memtis_hugepage config13-4 220

	set_130
}




#set_220
#run_static_bw config13-bw2 220
#run_static_bw config13-bw4 220


#run_main_memtis
#run_main_memstrata
#run_main_vtmm
#run_main_tpp
#run_main_colloid
run_main_mttm


#run_static_bw config1-bw1 130


#run_main_mttm
#run_main_mar_first
#run_main_hi_first
#run_main_static

#config1, 2, 12, 13


set_130


