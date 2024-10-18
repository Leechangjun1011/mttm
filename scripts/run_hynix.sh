#!/bin/bash


function run_huge_nooffl
{
	dmesg --clear
	echo $2 > /proc/sys/vm/pebs_sample_period
	echo 0 > /proc/sys/vm/ksampled_cpu
	echo 1 > /proc/sys/vm/kmigrated_cpu
	echo always > /sys/kernel/mm/transparent_hugepage/enabled
	./run_multi_hynix.sh 1 $1 2>&1 | cat > ./hynix/$1_$2_huge_nooffl.txt
	echo madvise > /sys/kernel/mm/transparent_hugepage/enabled
	dmesg > ./hynix/$1_$2_huge_nooffl_dmesg.txt
}

function run_base_nooffl
{
	dmesg --clear
	echo $2 > /proc/sys/vm/pebs_sample_period
	echo 0 > /proc/sys/vm/ksampled_cpu
	echo 1 > /proc/sys/vm/kmigrated_cpu
	echo madvise > /sys/kernel/mm/transparent_hugepage/enabled
	./run_multi_hynix.sh 1 $1 2>&1 | cat > ./hynix/$1_$2_base_nooffl.txt
	dmesg > ./hynix/$1_$2_base_nooffl_dmesg.txt
}

function run_huge_ksampled_offl
{
	dmesg --clear
	echo $2 > /proc/sys/vm/pebs_sample_period
	echo 16 > /proc/sys/vm/ksampled_cpu
	echo 1 > /proc/sys/vm/kmigrated_cpu
	echo always > /sys/kernel/mm/transparent_hugepage/enabled
	./run_multi_hynix.sh 1 $1 2>&1 | cat > ./hynix/$1_$2_huge_ksampled_offl.txt
	echo madvise > /sys/kernel/mm/transparent_hugepage/enabled
	dmesg > ./hynix/$1_$2_huge_ksampled_offl_dmesg.txt
}

function run_base_ksampled_offl
{
	dmesg --clear
	echo $2 > /proc/sys/vm/pebs_sample_period
	echo 16 > /proc/sys/vm/ksampled_cpu
	echo 1 > /proc/sys/vm/kmigrated_cpu
	echo madvise > /sys/kernel/mm/transparent_hugepage/enabled
	./run_multi_hynix.sh 1 $1 2>&1 | cat > ./hynix/$1_$2_base_ksampled_offl.txt
	dmesg > ./hynix/$1_$2_base_ksampled_offl_dmesg.txt
}

function run_huge_kmigd_offl
{
	dmesg --clear
	echo $2 > /proc/sys/vm/pebs_sample_period
	echo 0 > /proc/sys/vm/ksampled_cpu
	echo 17 > /proc/sys/vm/kmigrated_cpu
	echo always > /sys/kernel/mm/transparent_hugepage/enabled
	./run_multi_hynix.sh 1 $1 2>&1 | cat > ./hynix/$1_$2_huge_kmigd_offl.txt
	echo madvise > /sys/kernel/mm/transparent_hugepage/enabled
	dmesg > ./hynix/$1_$2_huge_kmigd_offl_dmesg.txt
}

function run_base_kmigd_offl
{
	dmesg --clear
	echo $2 > /proc/sys/vm/pebs_sample_period
	echo 0 > /proc/sys/vm/ksampled_cpu
	echo 17 > /proc/sys/vm/kmigrated_cpu
	echo madvise > /sys/kernel/mm/transparent_hugepage/enabled
	./run_multi_hynix.sh 1 $1 2>&1 | cat > ./hynix/$1_$2_base_kmigd_offl.txt
	dmesg > ./hynix/$1_$2_base_kmigd_offl_dmesg.txt
}

function run_huge_offl
{
	dmesg --clear
	echo $2 > /proc/sys/vm/pebs_sample_period
	echo 24 > /proc/sys/vm/ksampled_cpu
	echo 25 > /proc/sys/vm/kmigrated_cpu
	echo always > /sys/kernel/mm/transparent_hugepage/enabled
	./run_multi_hynix.sh 1 $1 2>&1 | cat > ./hynix/$1_$2_huge_offl.txt
	echo madvise > /sys/kernel/mm/transparent_hugepage/enabled
	dmesg > ./hynix/$1_$2_huge_offl_dmesg.txt
}

function run_base_offl
{
	dmesg --clear
	echo $2 > /proc/sys/vm/pebs_sample_period
	echo 24 > /proc/sys/vm/ksampled_cpu
	echo 25 > /proc/sys/vm/kmigrated_cpu
	echo madvise > /sys/kernel/mm/transparent_hugepage/enabled
	./run_multi_hynix.sh 1 $1 2>&1 | cat > ./hynix/$1_$2_base_offl.txt
	dmesg > ./hynix/$1_$2_base_offl_dmesg.txt
}

function run_10007 {
	echo 1 > /proc/sys/vm/period_factor
	run_huge_nooffl gapbs-bc 10007
	run_base_nooffl gapbs-bc 10007
	run_huge_nooffl silo 10007
	run_base_nooffl silo 10007

	run_huge_ksampled_offl gapbs-bc 10007
	run_base_ksampled_offl gapbs-bc 10007
	run_huge_ksampled_offl silo 10007
	run_base_ksampled_offl silo 10007

	run_huge_kmigd_offl gapbs-bc 10007
	run_base_kmigd_offl gapbs-bc 10007
	run_huge_kmigd_offl silo 10007
	run_base_kmigd_offl silo 10007

	run_huge_offl gapbs-bc 10007
	run_base_offl gapbs-bc 10007
	run_huge_offl silo 10007
	run_base_offl silo 10007
}

function run_4999 {
	echo 2 > /proc/sys/vm/period_factor
	run_huge_nooffl gapbs-bc 4999
	run_base_nooffl gapbs-bc 4999
	run_huge_nooffl silo 4999
	run_base_nooffl silo 4999

	run_huge_ksampled_offl gapbs-bc 4999
	run_base_ksampled_offl gapbs-bc 4999
	run_huge_ksampled_offl silo 4999
	run_base_ksampled_offl silo 4999

	run_huge_kmigd_offl gapbs-bc 4999
	run_base_kmigd_offl gapbs-bc 4999
	run_huge_kmigd_offl silo 4999
	run_base_kmigd_offl silo 4999

	run_huge_offl gapbs-bc 4999
	run_base_offl gapbs-bc 4999
	run_huge_offl silo 4999
	run_base_offl silo 4999
}

function run_1999 {
	echo 5 > /proc/sys/vm/period_factor
	run_huge_nooffl gapbs-bc 1999
	run_base_nooffl gapbs-bc 1999
	run_huge_nooffl silo 1999
	run_base_nooffl silo 1999

	run_huge_ksampled_offl gapbs-bc 1999
	run_base_ksampled_offl gapbs-bc 1999
	run_huge_ksampled_offl silo 1999
	run_base_ksampled_offl silo 1999

	run_huge_kmigd_offl gapbs-bc 1999
	run_base_kmigd_offl gapbs-bc 1999
	run_huge_kmigd_offl silo 1999
	run_base_kmigd_offl silo 1999

	run_huge_offl gapbs-bc 1999
	run_base_offl gapbs-bc 1999
	run_huge_offl silo 1999
	run_base_offl silo 1999
}

function run_997 {
	echo 10 > /proc/sys/vm/period_factor
	run_huge_nooffl gapbs-bc 997
	run_base_nooffl gapbs-bc 997
	run_huge_nooffl silo 997
	run_base_nooffl silo 997

	run_huge_ksampled_offl gapbs-bc 997
	run_base_ksampled_offl gapbs-bc 997
	run_huge_ksampled_offl silo 997
	run_base_ksampled_offl silo 997

	run_huge_kmigd_offl gapbs-bc 997
	run_base_kmigd_offl gapbs-bc 997
	run_huge_kmigd_offl silo 997
	run_base_kmigd_offl silo 997

	run_huge_offl gapbs-bc 997
	run_base_offl gapbs-bc 997
	run_huge_offl silo 997
	run_base_offl silo 997
}

function run_499 {
	echo 20 > /proc/sys/vm/period_factor
	run_huge_nooffl gapbs-bc 499
	run_base_nooffl gapbs-bc 499
	run_huge_nooffl silo 499
	run_base_nooffl silo 499

	run_huge_ksampled_offl gapbs-bc 499
	run_base_ksampled_offl gapbs-bc 499
	run_huge_ksampled_offl silo 499
	run_base_ksampled_offl silo 499

	run_huge_kmigd_offl gapbs-bc 499
	run_base_kmigd_offl gapbs-bc 499
	run_huge_kmigd_offl silo 499
	run_base_kmigd_offl silo 499

	run_huge_offl gapbs-bc 499
	run_base_offl gapbs-bc 499
	run_huge_offl silo 499
	run_base_offl silo 499
}

function run_199 {
	echo 50 > /proc/sys/vm/period_factor
	run_huge_nooffl gapbs-bc 199
	run_base_nooffl gapbs-bc 199
	run_huge_nooffl silo 199
	run_base_nooffl silo 199

	run_huge_ksampled_offl gapbs-bc 199
	run_base_ksampled_offl gapbs-bc 199
	run_huge_ksampled_offl silo 199
	run_base_ksampled_offl silo 199

	run_huge_kmigd_offl gapbs-bc 199
	run_base_kmigd_offl gapbs-bc 199
	run_huge_kmigd_offl silo 199
	run_base_kmigd_offl silo 199

	run_huge_offl gapbs-bc 199
	run_base_offl gapbs-bc 199
	run_huge_offl silo 199
	run_base_offl silo 199
}

#10007, 4999, 1999, 997, 499, 199, 101

#run_10007

#run_4999
#run_1999
#run_997
#run_499
#run_199

echo 200 > /proc/sys/vm/period_factor
run_base_offl gapbs-bc 199
run_base_nooffl gapbs-bc 199


