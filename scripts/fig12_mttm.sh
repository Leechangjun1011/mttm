#!/bin/bash
cur_path=$PWD
emul_path=$PWD/cxl-emulation


function run_mttm_basepage
{
	dmesg --clear
	echo 1 > /proc/sys/vm/use_dram_determination
	echo 1 > /proc/sys/vm/use_region_separation
	echo 0 > /proc/sys/vm/use_memstrata_policy
	echo 0 > /proc/sys/vm/use_static_dram
	echo $2 > /proc/sys/vm/mttm_local_dram_string
	echo 1 > /proc/sys/vm/use_rxc_monitoring
	echo 0 > /proc/sys/vm/print_more_info

	echo 1 > /proc/sys/vm/mar_weight
	echo 1 > /proc/sys/vm/hi_weight
	echo 3 > /proc/sys/vm/hugepage_shift_factor
	echo 1 > /proc/sys/vm/hugepage_period_factor

	echo 101 > /proc/sys/vm/pebs_sample_period
	echo 1 > /proc/sys/vm/use_pingpong_reduce
	echo 200 > /proc/sys/vm/pingpong_reduce_threshold
	echo 1 > /proc/sys/vm/reduce_scan
	echo madvise > /sys/kernel/mm/transparent_hugepage/enabled

	echo 9 > /proc/sys/vm/basepage_shift_factor #target cooling period
	echo 40 > /proc/sys/vm/basepage_period_factor #increasing granularity

	mkdir -p ./evaluation/fig12/mttm/$1	
	./run_multi_tenants.sh $1 2>&1 | cat > ./evaluation/fig12/mttm/$1/$2_$3.txt
	dmesg > ./evaluation/fig12/mttm/$1/$2_$3_dmesg.txt
}


function reset_latency
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


set_190
run_mttm_basepage mix4 51G 190
reset_latency
