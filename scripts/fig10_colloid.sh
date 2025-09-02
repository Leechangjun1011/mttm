#!/bin/bash
cur_path=$PWD
emul_path=$PWD/cxl-emulation
COLLOID_MODULE=/home/cjlee/colloid/tpp # Modify this line


function run_colloid_hugepage
{
	#mix, dram size, remote latency
	dmesg --clear
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
	mkdir -p ./evaluation/fig10/colloid/$1
	cat /proc/vmstat | grep -e promote -e demote -e migrate -e hint > ./evaluation/fig10/colloid/$1/$2_$3_before_vmstat.txt
	./run_multi_tenants_tpp.sh $1 2>&1 | cat > ./evaluation/fig10/colloid/$1/$2_$3.txt
	dmesg > ./evaluation/fig10/colloid/$1/$2_$3_dmesg.txt
	cat /proc/vmstat | grep -e promote -e demote -e migrate -e hint > ./evaluation/fig10/colloid/$1/$2_$3_after_vmstat.txt

	rmmod colloid-mon
	rmmod memeater
}



function reset_latency
{
	cd $emul_path
	./reset.sh
	cd $cur_path
}

function set_160
{
	cd $emul_path
	./reset.sh
	./emulate.sh 6 4000
	cd $cur_path
}

function set_190
{
	cd $emul_path
	./reset.sh
	./emulate.sh 11 4000
	cd $cur_path
}

function set_220
{
	cd $emul_path
	./reset.sh
	./emulate.sh 24 0 0x8124
	cd $cur_path
}


sudo insmod ${COLLOID_MODULE}/tierinit/tierinit.ko
sudo insmod ${COLLOID_MODULE}/kswapdrst/kswapdrst.ko


set_160
run_colloid_hugepage mix1 54G 160
run_colloid_hugepage mix1 21G 160
run_colloid_hugepage mix2 34G 160
run_colloid_hugepage mix2 13G 160
run_colloid_hugepage mix3 45G 160
run_colloid_hugepage mix3 18G 160
run_colloid_hugepage mix4 51G 160
run_colloid_hugepage mix4 20G 160

set_190
run_colloid_hugepage mix1 54G 190
run_colloid_hugepage mix1 21G 190
run_colloid_hugepage mix2 34G 190
run_colloid_hugepage mix2 13G 190
run_colloid_hugepage mix3 45G 190
run_colloid_hugepage mix3 18G 190
run_colloid_hugepage mix4 51G 190
run_colloid_hugepage mix4 20G 190

set_220
run_colloid_hugepage mix1 54G 220
run_colloid_hugepage mix1 21G 220
run_colloid_hugepage mix2 34G 220
run_colloid_hugepage mix2 13G 220
run_colloid_hugepage mix3 45G 220
run_colloid_hugepage mix3 18G 220
run_colloid_hugepage mix4 51G 220
run_colloid_hugepage mix4 20G 220

reset_latency

sudo rmmod kswapdrst
sudo rmmod tierinit


