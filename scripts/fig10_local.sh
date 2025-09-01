#!/bin/bash
cur_path=$PWD
emul_path=$PWD/cxl-emulation


function run_local_hugepage
{
	echo always > /sys/kernel/mm/transparent_hugepage/enabled
	mkdir -p ./evaluation/fig10/local
	./run_multi_tenants_local.sh $1 2>&1 | cat > ./evaluation/fig10/local/$1.txt
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



reset_latency
#run_local_hugepage mix1
#run_local_hugepage mix2
run_local_hugepage mix3
#run_local_hugepage mix4



