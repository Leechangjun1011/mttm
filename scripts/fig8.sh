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

        mkdir -p ./evaluation/fig8/mttm
        ./run_multi_tenants.sh $1 2>&1 | cat > ./evaluation/fig8/mttm/$1_$2_$3.txt
        dmesg > ./evaluation/fig8/mttm/$1_$2_$3_dmesg.txt
	
	mv tot_gups_1.txt tot_gups_1_mttm.txt
	mv tot_gups_2.txt tot_gups_2_mttm.txt
	mv tot_gups_3.txt tot_gups_3_mttm.txt

}

function run_local_hugepage
{
        #config
        dmesg --clear
        echo always > /sys/kernel/mm/transparent_hugepage/enabled

        mkdir -p ./evaluation/fig8/local
        ./run_multi_tenants_local.sh $1 2>&1 | cat > ./evaluation/fig8/local/$1.txt
        dmesg > ./evaluation/fig8/local/$1_dmesg.txt

	mv tot_gups_1.txt tot_gups_1_local.txt
	mv tot_gups_2.txt tot_gups_2_local.txt
	mv tot_gups_3.txt tot_gups_3_local.txt

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
run_mttm_hugepage microbench-dynamic 24G 160
reset_latency
run_local_hugepage microbench

echo -e "time\tgups1\tgups2\tgups3\tgups_local" > fig8_gups.dat
line1=$(cat tot_gups_1_mttm.txt | wc -l)
line2=$(cat tot_gups_2_mttm.txt | wc -l)

echo 0 > fig8_gups_3.txt
for i in $(seq 2 50); do
        echo 0 >> fig8_gups_3.txt
done

echo "$(cat tot_gups_3_mttm.txt)" >> fig8_gups_3.txt

line3=$(cat fig8_gups_3.txt | wc -l)
if [ "$line1" -gt "$line2" ]; then
        max_line=$line1
else
        max_line=$line2
fi
if [ "$line3" -gt "$max_line" ]; then
        max_line=$line3
fi

echo 1 > fig8_time.txt
for i in $(seq 2 $max_line); do
        echo $i >> fig8_time.txt
done

paste fig8_time.txt tot_gups_1_mttm.txt tot_gups_2_mttm.txt fig8_gups_3.txt tot_gups_1_local.txt >> fig8_gups.dat
