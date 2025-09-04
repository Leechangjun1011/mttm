#!/bin/bash
cur_path=$PWD
emul_path=$PWD/cxl-emulation


function run_vtmm_hugepage
{
	dmesg --clear
	echo $2 > /proc/sys/vm/mttm_local_dram_string

	mkdir -p ./evaluation/fig10/vtmm/$1
	./run_multi_tenants_vtmm.sh $1 2>&1 | cat > ./evaluation/fig10/vtmm/$1/$2_$3.txt
	dmesg > ./evaluation/fig10/vtmm/$1/$2_$3_dmesg.txt
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
run_vtmm_hugepage mix1 54G 160
run_vtmm_hugepage mix1 21G 160
run_vtmm_hugepage mix2 34G 160
run_vtmm_hugepage mix2 13G 160
run_vtmm_hugepage mix3 45G 160
run_vtmm_hugepage mix3 18G 160
run_vtmm_hugepage mix4 51G 160
run_vtmm_hugepage mix4 20G 160
end

set_190
: << end
run_vtmm_hugepage mix1 54G 190
run_vtmm_hugepage mix1 21G 190
run_vtmm_hugepage mix2 34G 190
run_vtmm_hugepage mix2 13G 190
run_vtmm_hugepage mix3 45G 190
run_vtmm_hugepage mix3 18G 190
run_vtmm_hugepage mix4 51G 190
end
run_vtmm_hugepage mix4 20G 190

set_220
run_vtmm_hugepage mix1 54G 220
run_vtmm_hugepage mix1 21G 220
run_vtmm_hugepage mix2 34G 220
run_vtmm_hugepage mix2 13G 220
run_vtmm_hugepage mix3 45G 220
run_vtmm_hugepage mix3 18G 220
run_vtmm_hugepage mix4 51G 220
run_vtmm_hugepage mix4 20G 220

reset_latency



