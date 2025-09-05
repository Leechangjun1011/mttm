#!/bin/bash

eval_data_path=./evaluation/fig13
rxc_log=${eval_data_path}/mttm/rxc/mix4/20G_220_dmesg.txt
norxc_log=${eval_data_path}/mttm/norxc/mix4/20G_220_dmesg.txt
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
		perf=$(./extract.sh $1 ${eval_data_path}/../fig10/local/$1.txt)
	elif [[ "$2" == "mttm" ]]; then
		perf=$(./extract.sh $1 ${eval_data_path}/mttm/rxc/$1/$3.txt)
	elif [[ "$2" == "mttm_norxc" ]]; then
		perf=$(./extract.sh $1 ${eval_data_path}/mttm/norxc/$1/$3.txt)
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


get_perf mix4 local
get_perf mix4 mttm 20G_220

pr=$(python3 ./cal_perf_fig12.py PR $PR_local $PR)
fotonik=$(python3 ./cal_perf_fig12.py fotonik $fotonik_local $fotonik)
silo=$(python3 ./cal_perf_fig12.py silo $silo_local $silo)

echo -e "### MTTM with rejection monitoring"
echo -e "PR: $pr"
echo -e "fotonik: $fotonik"
echo -e "Silo: $silo"


get_perf mix4 mttm_norxc 20G_220

pr=$(python3 ./cal_perf_fig12.py PR $PR_local $PR)
fotonik=$(python3 ./cal_perf_fig12.py fotonik $fotonik_local $fotonik)
silo=$(python3 ./cal_perf_fig12.py silo $silo_local $silo)
echo -e "\n### MTTM without rejection monitoring"
echo -e "PR: $pr"
echo -e "fotonik: $fotonik"
echo -e "Silo: $silo"


