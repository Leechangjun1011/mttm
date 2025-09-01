#!/bin/bash
cur_path=$PWD
emul_path=$PWD/cxl-emulation


function run_memtis_hugepage
{
	memtis_script=${cur_path}/memtis_script/run_bench_memtis.sh

	echo always > /sys/kernel/mm/transparent_hugepage/enabled
	echo always > /sys/kernel/mm/transparent_hugepage/defrag

	mkdir -p ./evaluation/fig10/memtis/$1
	${memtis_script} -C $1 2>&1 | cat > ./evaluation/fig10/memtis/$1/$2.txt
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





set_160
run_memtis_hugepage mix4-1 160
: << end
run_memtis_hugepage mix1-4 160
run_memtis_hugepage mix2-1 160
run_memtis_hugepage mix2-4 160
run_memtis_hugepage mix3-1 160
run_memtis_hugepage mix3-4 160
run_memtis_hugepage mix4-1 160
run_memtis_hugepage mix4-4 160

set_190
run_memtis_hugepage mix1-1 190
run_memtis_hugepage mix1-4 190
run_memtis_hugepage mix2-1 190
run_memtis_hugepage mix2-4 190
run_memtis_hugepage mix3-1 190
run_memtis_hugepage mix3-4 190
run_memtis_hugepage mix4-1 190
run_memtis_hugepage mix4-4 190

set_220
run_memtis_hugepage mix1-1 220
run_memtis_hugepage mix1-4 220
run_memtis_hugepage mix2-1 220
run_memtis_hugepage mix2-4 220
run_memtis_hugepage mix3-1 220
run_memtis_hugepage mix3-4 220
run_memtis_hugepage mix4-1 220
run_memtis_hugepage mix4-4 220
end
reset_latency



