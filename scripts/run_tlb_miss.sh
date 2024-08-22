#!/bin/bash

: << END
echo always > /sys/kernel/mm/transparent_hugepage/enabled
perf stat -o bc_tlb_huge.txt -a -e LLC-load-misses,LLC-store-misses,dTLB-load-misses,dTLB-store-misses,iTLB-load-misses ./run_multi_tenants_native.sh 1 gapbs-bc

echo madvise > /sys/kernel/mm/transparent_hugepage/enabled
perf stat -o bc_tlb_base.txt -a -e LLC-load-misses,LLC-store-misses,dTLB-load-misses,dTLB-store-misses,iTLB-load-misses ./run_multi_tenants_native.sh 1 gapbs-bc

echo always > /sys/kernel/mm/transparent_hugepage/enabled
perf stat -o ./tlb_result/pr_tlb_huge.txt -a -e LLC-load-misses,LLC-store-misses,dTLB-load-misses,dTLB-store-misses,iTLB-load-misses ./run_multi_tenants_native.sh 1 gapbs-pr > ./tlb_result/pr_perf_tlb_huge.txt
echo madvise > /sys/kernel/mm/transparent_hugepage/enabled
perf stat -o ./tlb_result/pr_tlb_base.txt -a -e LLC-load-misses,LLC-store-misses,dTLB-load-misses,dTLB-store-misses,iTLB-load-misses ./run_multi_tenants_native.sh 1 gapbs-pr > ./tlb_result/pr_perf_tlb_base.txt
END

function run_perf
{
	echo always > /sys/kernel/mm/transparent_hugepage/enabled
	perf stat -o ./tlb_result/$1_tlb_huge.txt -a -e LLC-load-misses,LLC-store-misses,dTLB-load-misses,dTLB-store-misses,iTLB-load-misses ./run_multi_tenants_native.sh 1 $1 > ./tlb_result/$1_perf_tlb_huge.txt
	
	echo madvise > /sys/kernel/mm/transparent_hugepage/enabled
	perf stat -o ./tlb_result/$1_tlb_base.txt -a -e LLC-load-misses,LLC-store-misses,dTLB-load-misses,dTLB-store-misses,iTLB-load-misses ./run_multi_tenants_native.sh 1 $1 > ./tlb_result/$1_perf_tlb_base.txt

}

#run_perf gapbs-cc_sv
run_perf gapbs-tc
run_perf graph500
run_perf xsbench
run_perf xindex
run_perf silo
run_perf cpu_dlrm_small_low
run_perf cpu_dlrm_small_high
run_perf cpu_dlrm_large_low
run_perf cpu_dlrm_large_high
run_perf fotonik
run_perf roms
run_perf nas_cg.d
run_perf nas_bt.d
run_perf nas_sp.d
run_perf nas_mg.d
run_perf nas_lu.d
run_perf nas_ua.d




