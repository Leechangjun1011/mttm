#!/bin/bash

eval_data_path=./evaluation/fig12
memtis_log=${eval_data_path}/memtis/mix4-1-basepage/dmesg.txt
mttm_log=${eval_data_path}/mttm/mix4-basepage/51G_190_dmesg.txt
PR_local=''
fotonik_local=''
silo_local=''

PR=''
fotonik=''
silo=''

PR_p=''
fotonik_p=''
silo_p=''


function get_perf
{
	# mix, system, args(dram size, latency...)
	if [[ "$2" == "local" ]]; then
		perf=$(./extract.sh $1 ${eval_data_path}/local/$1.txt)
	elif [[ "$2" == "mttm" ]]; then
		perf=$(./extract.sh $1 ${eval_data_path}/mttm/$1/$3.txt)
	elif [[ "$2" == "memtis" ]]; then
		if [[ "$3" == "1_1" ]]; then
			perf=$(./extract.sh $1 ${eval_data_path}/memtis/$1-1-basepage/$4.txt)
		fi
	fi
	#echo "$perf"

	if [[ "$1" == "mix1" ]]; then
		PR=$(echo "${perf}" | grep Average | tail -n 1 | cut -d':' -f2)
		BC=$(echo "${perf}" | grep Average | head -n 1 | cut -d':' -f2)
		XSBENCH=$(echo "${perf}" | grep Runtime | cut -d':' -f2 | cut -d's' -f1)
	
		if [[ $(echo "$BC > $PR" | bc -l) -eq 1 ]]; then
			tmp=$BC
			BC=$PR
			PR=$tmp
		fi
	elif [[ "$1" == "mix2" ]]; then
		fotonik=$(echo "${perf}" | grep runcpu | cut -d';' -f2 | cut -d't' -f1)
		dlrm=$(echo "${perf}" | grep fps | cut -d':' -f2 | cut -d'f' -f1)
		xindex=$(echo "${perf}" | grep ycsb | cut -d':' -f2)
	elif [[ "$1" == "mix3" ]]; then
		btree=$(echo "${perf}" | grep Took | head -n 1 | cut -d':' -f2)
		dlrm=$(echo "${perf}" | grep fps | cut -d':' -f2 | cut -d'f' -f1)
		xindex=$(echo "${perf}" | grep ycsb | cut -d':' -f2)
	elif [[ "$1" == "mix4" || "$1" == "mix4-basepage" ]]; then
		PR=$(echo "${perf}" | grep Average | cut -d':' -f2)
		fotonik=$(echo "${perf}" | grep runcpu | cut -d';' -f2 | cut -d't' -f1)
		silo=$(echo "${perf}" | grep agg_throughput | cut -d':' -f2 | cut -d'o' -f1)
	fi

	if [[ "$2" == "local" ]]; then
		PR_local=$PR
		BC_local=$BC
		XSBENCH_local=$XSBENCH
		fotonik_local=$fotonik
		dlrm_local=$dlrm
		xindex_local=$xindex
		btree_local=$btree
		silo_local=$silo
	fi

}



get_perf mix4-basepage local
get_perf mix4-basepage mttm 51G_190

pr=$(python3 ./cal_perf_fig12.py PR $PR_local $PR)
fotonik=$(python3 ./cal_perf_fig12.py fotonik $fotonik_local $fotonik)
silo=$(python3 ./cal_perf_fig12.py silo $silo_local $silo)

echo -e "### MTTM"
echo -e "PR: $pr"
echo -e "fotonik: $fotonik"
echo -e "Silo: $silo"

tot_time=$(cat ${mttm_log} | grep cputime | grep sample | cut -d':' -f2 | cut -d',' -f1)
sample_time=$(cat ${mttm_log} | grep cputime | grep sample | cut -d':' -f3)
sample=$(echo "scale=2; $sample_time * 100 / $tot_time" | bc -l)
echo -e "Sampling: $sample %"

tot_time=$(cat ${mttm_log} | grep cputime | grep runcpu | cut -d':' -f3 | cut -d',' -f1)
adj_time=$(cat ${mttm_log} | grep cputime | grep runcpu | cut -d':' -f5 | cut -d',' -f1)
cool_time=$(cat ${mttm_log} | grep cputime | grep runcpu | cut -d':' -f6 | cut -d',' -f1)
mig_time=$(cat ${mttm_log} | grep cputime | grep runcpu | cut -d':' -f7 | cut -d']' -f1)
fotonik_scan=$(echo "scale=2; ($adj_time + $cool_time)*100/$tot_time" | bc -l)
fotonik_mig=$(echo "scale=2; ($mig_time)*100/$tot_time" | bc -l)

tot_time=$(cat ${mttm_log} | grep cputime | grep pr | cut -d':' -f3 | cut -d',' -f1)
adj_time=$(cat ${mttm_log} | grep cputime | grep pr | cut -d':' -f5 | cut -d',' -f1)
cool_time=$(cat ${mttm_log} | grep cputime | grep pr | cut -d':' -f6 | cut -d',' -f1)
mig_time=$(cat ${mttm_log} | grep cputime | grep pr | cut -d':' -f7 | cut -d']' -f1)
pr_scan=$(echo "scale=2; ($adj_time + $cool_time)*100/$tot_time" | bc -l)
pr_mig=$(echo "scale=2; ($mig_time)*100/$tot_time" | bc -l)

tot_time=$(cat ${mttm_log} | grep cputime | grep dbtest | cut -d':' -f3 | cut -d',' -f1)
adj_time=$(cat ${mttm_log} | grep cputime | grep dbtest | cut -d':' -f5 | cut -d',' -f1)
cool_time=$(cat ${mttm_log} | grep cputime | grep dbtest | cut -d':' -f6 | cut -d',' -f1)
mig_time=$(cat ${mttm_log} | grep cputime | grep dbtest | cut -d':' -f7 | cut -d']' -f1)
silo_scan=$(echo "scale=2; ($adj_time + $cool_time)*100/$tot_time" | bc -l)
silo_mig=$(echo "scale=2; ($mig_time)*100/$tot_time" | bc -l)

cpu_util=$(echo "scale=2; $fotonik_mig + $pr_mig + $silo_mig" | bc -l)
echo -e "Migration: $cpu_util %"

cpu_util=$(echo "scale=2; $fotonik_scan + $pr_scan + $silo_scan" | bc -l)
echo -e "Scanning: $cpu_util %"



#get_perf mix4 local
get_perf mix4 memtis 1_1 190

pr=$(python3 ./cal_perf_fig12.py PR $PR_local $PR)
fotonik=$(python3 ./cal_perf_fig12.py fotonik $fotonik_local $fotonik)
silo=$(python3 ./cal_perf_fig12.py silo $silo_local $silo)

echo -e "\n### Memtis"
echo -e "PR: $pr"
echo -e "fotonik: $fotonik"
echo -e "Silo: $silo"

tot_time=$(cat ${memtis_log} | grep 'cpu usage' | cut -d':' -f3 | cut -d'u' -f1)
sample_time=$(cat ${memtis_log} | grep 'cpu usage' | cut -d':' -f2 | cut -d'n' -f1)
sample=$(echo "scale=2; $sample_time * 100 / ($tot_time * 1000)" | bc -l)
echo -e "Sampling: $sample %"


tot_promote_time=$(cat ${memtis_log} | grep promotion | cut -d':' -f2 | cut -d',' -f1)
tot_demote_time=$(cat ${memtis_log} | grep demotion | cut -d':' -f2 | cut -d',' -f1)
scan_promote_time=$(cat ${memtis_log} | grep promotion | cut -d':' -f5)
scan_demote_time=$(cat ${memtis_log} | grep demotion | cut -d':' -f5)
mig_promote_time=$(cat ${memtis_log} | grep promotion | cut -d':' -f4 | cut -d',' -f1)
mig_demote_time=$(cat ${memtis_log} | grep demotion | cut -d':' -f4 | cut -d',' -f1)

cpu_util=$(echo "scale=2; ($mig_promote_time * 100 / $tot_promote_time) + ($mig_demote_time * 100 / $tot_demote_time)" | bc -l)
echo -e "Migration: $cpu_util %"

cpu_util=$(echo "scale=2;($scan_promote_time * 100 / $tot_promote_time) + ($scan_demote_time * 100 / $tot_demote_time)" | bc -l)
echo -e "Scanning: $cpu_util %"

