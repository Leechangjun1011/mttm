#!/bin/bash
cur_path=$PWD
emul_path=$PWD/cxl-emulation

function run_mttm_hugepage
{
	#config, dram size, remote latency
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

	echo 4999 > /proc/sys/vm/pebs_sample_period
	echo 1 > /proc/sys/vm/use_pingpong_reduce
	echo 500 > /proc/sys/vm/pingpong_reduce_threshold
	echo 1 > /proc/sys/vm/reduce_scan
	echo always > /sys/kernel/mm/transparent_hugepage/enabled

	mkdir -p ./evaluation/fig10/mttm/$1
	./run_multi_tenants.sh $1 2>&1 | cat > ./evaluation/fig10/mttm/$1/$2_$3.txt
	dmesg > ./evaluation/fig10/mttm/$1/$2_$3_dmesg.txt
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


set_160
: << end
run_mttm_hugepage mix1 54G 160
run_mttm_hugepage mix1 21G 160
run_mttm_hugepage mix2 34G 160
run_mttm_hugepage mix2 13G 160
run_mttm_hugepage mix3 45G 160
run_mttm_hugepage mix3 18G 160
run_mttm_hugepage mix4 51G 160
run_mttm_hugepage mix4 20G 160


set_190
run_mttm_hugepage mix1 54G 190
run_mttm_hugepage mix1 21G 190
run_mttm_hugepage mix2 34G 190
run_mttm_hugepage mix2 13G 190
run_mttm_hugepage mix3 45G 190
run_mttm_hugepage mix3 18G 190
run_mttm_hugepage mix4 51G 190
run_mttm_hugepage mix4 20G 190

set_220
run_mttm_hugepage mix1 54G 220
run_mttm_hugepage mix1 21G 220
run_mttm_hugepage mix2 34G 220
run_mttm_hugepage mix2 13G 220
run_mttm_hugepage mix3 45G 220
run_mttm_hugepage mix3 18G 220
run_mttm_hugepage mix4 51G 220
run_mttm_hugepage mix4 20G 220
end
run_mttm_hugepage mix2 34G 160

reset_latency




