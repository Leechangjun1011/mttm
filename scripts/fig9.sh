#!/bin/bash
cur_path=$PWD
emul_path=$PWD/cxl-emulation


function run_mttm_sensitivity_mar_hi
{
	#config, dram size, mar weight, hi weight
	dmesg --clear
	echo 1 > /proc/sys/vm/use_dram_determination
	echo 1 > /proc/sys/vm/use_region_separation
	echo 0 > /proc/sys/vm/use_memstrata_policy
	echo 0 > /proc/sys/vm/use_static_dram
	echo $2 > /proc/sys/vm/mttm_local_dram_string
	echo 1 > /proc/sys/vm/use_rxc_monitoring
	echo 0 > /proc/sys/vm/print_more_info

	echo $3 > /proc/sys/vm/mar_weight
	echo $4 > /proc/sys/vm/hi_weight
	echo 3 > /proc/sys/vm/hugepage_shift_factor
	echo 1 > /proc/sys/vm/hugepage_period_factor

	echo 4999 > /proc/sys/vm/pebs_sample_period
	echo 1 > /proc/sys/vm/use_pingpong_reduce
	echo 500 > /proc/sys/vm/pingpong_reduce_threshold
	echo 1 > /proc/sys/vm/reduce_scan
	echo always > /sys/kernel/mm/transparent_hugepage/enabled

	mkdir -p ./evaluation/fig9/mttm
	./run_multi_tenants.sh $1 2>&1 | cat > ./evaluation/fig9/mttm/$1_$2_$3_$4.txt
	dmesg > ./evaluation/fig9/mttm/$1_$2_$3_$4_dmesg.txt
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
run_mttm_sensitivity_mar_hi microbench-sensitivity1 24G 2 1
run_mttm_sensitivity_mar_hi microbench-sensitivity1 24G 1 1
run_mttm_sensitivity_mar_hi microbench-sensitivity1 24G 1 2

run_mttm_sensitivity_mar_hi microbench-sensitivity2 24G 2 1
run_mttm_sensitivity_mar_hi microbench-sensitivity2 24G 1 1
run_mttm_sensitivity_mar_hi microbench-sensitivity2 24G 1 2

reset_latency

result_path=./evaluation/fig9/mttm

echo -e "### Same # of threads" > ./fig9_gups.dat
echo -e "alpha/beta = 0.5" >> ./fig9_gups.dat
cat ${result_path}/microbench-sensitivity1_24G_2_1.txt | grep GUPS >> ./fig9_gups.dat
echo -e "\nalpha/beta = 1" >> ./fig9_gups.dat
cat ${result_path}/microbench-sensitivity1_24G_1_1.txt | grep GUPS >> ./fig9_gups.dat
echo -e "\nalpha/beta = 2" >> ./fig9_gups.dat
cat ${result_path}/microbench-sensitivity1_24G_1_2.txt | grep GUPS >> ./fig9_gups.dat

echo -e "\n### Same hot set size" >> ./fig9_gups.dat
echo -e "alpha/beta = 0.5" >> ./fig9_gups.dat
cat ${result_path}/microbench-sensitivity2_24G_2_1.txt | grep GUPS >> ./fig9_gups.dat
echo -e "\nalpha/beta = 1" >> ./fig9_gups.dat
cat ${result_path}/microbench-sensitivity2_24G_1_1.txt | grep GUPS >> ./fig9_gups.dat
echo -e "\nalpha/beta = 2" >> ./fig9_gups.dat
cat ${result_path}/microbench-sensitivity2_24G_1_2.txt | grep GUPS >> ./fig9_gups.dat

