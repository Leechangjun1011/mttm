#!/bin/bash

eval_data_path=./evaluation/fig10
PR_local=''
BC_local=''
XSBENCH_local=''
fotonik_local=''
dlrm_local=''
xindex_local=''
btree_local=''
silo_local=''

PR=''
BC=''
XSBENCH=''
fotonik=''
dlrm=''
xindex=''
btree=''
silo=''

PR_p=''
BC_p=''
XSBENCH_p=''
fotonik_p=''
dlrm_p=''
xindex_p=''
btree_p=''
silo_p=''

avg_mix1=''
avg_mix2=''
avg_mix3=''
avg_mix4=''
avg_all=''

function get_perf
{
	# mix, system, args(dram size, latency...)
	if [[ "$2" == "local" ]]; then
		perf=$(./extract.sh $1 ${eval_data_path}/local/$1.txt)
	elif [[ "$2" == "mttm" ]]; then
		perf=$(./extract.sh $1 ${eval_data_path}/mttm/$1/$3.txt)
	elif [[ "$2" == "static" ]]; then
		perf=$(./extract.sh $1 ${eval_data_path}/static/$1/$3.txt)
	elif [[ "$2" == "vtmm" ]]; then
		perf=$(./extract.sh $1 ${eval_data_path}/vtmm/$1/$3.txt)
	elif [[ "$2" == "memstrata" ]]; then
		perf=$(./extract.sh $1 ${eval_data_path}/memstrata/$1/$3.txt)
	elif [[ "$2" == "tpp" ]]; then
		perf=$(./extract.sh $1 ${eval_data_path}/tpp/$1/$3.txt)
	elif [[ "$2" == "colloid" ]]; then
		perf=$(./extract.sh $1 ${eval_data_path}/colloid/$1/$3.txt)
	elif [[ "$2" == "memtis" ]]; then
		if [[ "$3" == "1_1" ]]; then
			perf=$(./extract.sh $1 ${eval_data_path}/memtis/$1-1/$4.txt)
		elif [[ "$3" == "1_4" ]]; then
			perf=$(./extract.sh $1 ${eval_data_path}/memtis/$1-4/$4.txt)
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
	elif [[ "$1" == "mix4" ]]; then
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

function parse_avg_perf
{
	# mix, target, memory config, remote latency
	get_perf $1 local
	if [[ "$2" == "memtis" ]]; then
		get_perf $1 $2 $3 $4
	else
		get_perf $1 $2 $3_$4
	fi
	if [[ "$1" == "mix1" ]]; then
		python3 ./cal_perf.py PR $PR_local $PR BC $BC_local $BC xsbench $XSBENCH_local $XSBENCH
	elif [[ "$1" == "mix2" ]]; then
		python3 ./cal_perf.py fotonik $fotonik_local $fotonik dlrm $dlrm_local $dlrm xindex $xindex_local $xindex
	elif [[ "$1" == "mix3" ]]; then
		python3 ./cal_perf.py btree $btree_local $btree dlrm $dlrm_local $dlrm xindex $xindex_local $xindex
	elif [[ "$1" == "mix4" ]]; then
		python3 ./cal_perf.py PR $PR_local $PR fotonik $fotonik_local $fotonik silo $silo_local $silo
	fi
}


function print_perf
{
	# system, memory config
	if [[ "$2" == "1_1" ]]; then
		mix1_dram=54G
		mix2_dram=34G
		mix3_dram=45G
		mix4_dram=51G
	elif [[ "$2" == "1_4" ]]; then
		mix1_dram=21G
		mix2_dram=13G
		mix3_dram=18G
		mix4_dram=20G
	fi
	echo -e "remote_latency\tmix1\tmix2\tmix3\tmix4\tavg" > ./fig10_$1_$2.dat

	mix1_perf=$(parse_avg_perf mix1 $1 $mix1_dram 160)
	mix2_perf=$(parse_avg_perf mix2 $1 $mix2_dram 160)
	mix3_perf=$(parse_avg_perf mix3 $1 $mix3_dram 160)
	mix4_perf=$(parse_avg_perf mix4 $1 $mix4_dram 160)
	avg_perf=$(python3 ./cal_perf.py average $mix1_perf $mix2_perf $mix3_perf $mix4_perf)
	echo -e "160	$mix1_perf	$mix2_perf	$mix3_perf	$mix4_perf	$avg_perf" >> ./fig10_$1_$2.dat

	mix1_perf=$(parse_avg_perf mix1 $1 $mix1_dram 190)
	mix2_perf=$(parse_avg_perf mix2 $1 $mix2_dram 190)
	mix3_perf=$(parse_avg_perf mix3 $1 $mix3_dram 190)
	mix4_perf=$(parse_avg_perf mix4 $1 $mix4_dram 190)
	avg_perf=$(python3 ./cal_perf.py average $mix1_perf $mix2_perf $mix3_perf $mix4_perf)
	echo -e "190	$mix1_perf	$mix2_perf	$mix3_perf	$mix4_perf	$avg_perf" >> ./fig10_$1_$2.dat

	mix1_perf=$(parse_avg_perf mix1 $1 $mix1_dram 220)
	mix2_perf=$(parse_avg_perf mix2 $1 $mix2_dram 220)
	mix3_perf=$(parse_avg_perf mix3 $1 $mix3_dram 220)
	mix4_perf=$(parse_avg_perf mix4 $1 $mix4_dram 220)
	avg_perf=$(python3 ./cal_perf.py average $mix1_perf $mix2_perf $mix3_perf $mix4_perf)
	echo -e "220	$mix1_perf	$mix2_perf	$mix3_perf	$mix4_perf	$avg_perf" >> ./fig10_$1_$2.dat

}

function print_perf_memtis
{
	# memory config
	echo -e "remote_latency\tmix1\tmix2\tmix3\tmix4\tavg" > ./fig10_memtis_$1.dat

	mix1_perf=$(parse_avg_perf mix1 memtis $1 160)
	mix2_perf=$(parse_avg_perf mix2 memtis $1 160)
	mix3_perf=$(parse_avg_perf mix3 memtis $1 160)
	mix4_perf=$(parse_avg_perf mix4 memtis $1 160)
	avg_perf=$(python3 ./cal_perf.py average $mix1_perf $mix2_perf $mix3_perf $mix4_perf)
	echo -e "160	$mix1_perf	$mix2_perf	$mix3_perf	$mix4_perf	$avg_perf" >> ./fig10_memtis_$1.dat

	mix1_perf=$(parse_avg_perf mix1 memtis $1 190)
	mix2_perf=$(parse_avg_perf mix2 memtis $1 190)
	mix3_perf=$(parse_avg_perf mix3 memtis $1 190)
	mix4_perf=$(parse_avg_perf mix4 memtis $1 190)
	avg_perf=$(python3 ./cal_perf.py average $mix1_perf $mix2_perf $mix3_perf $mix4_perf)
	echo -e "190	$mix1_perf	$mix2_perf	$mix3_perf	$mix4_perf	$avg_perf" >> ./fig10_memtis_$1.dat

	mix1_perf=$(parse_avg_perf mix1 memtis $1 220)
	mix2_perf=$(parse_avg_perf mix2 memtis $1 220)
	mix3_perf=$(parse_avg_perf mix3 memtis $1 220)
	mix4_perf=$(parse_avg_perf mix4 memtis $1 220)
	avg_perf=$(python3 ./cal_perf.py average $mix1_perf $mix2_perf $mix3_perf $mix4_perf)
	echo -e "220	$mix1_perf	$mix2_perf	$mix3_perf	$mix4_perf	$avg_perf" >> ./fig10_memtis_$1.dat

}

print_perf mttm 1_1
print_perf mttm 1_4

print_perf static 1_1
print_perf static 1_4

print_perf vtmm 1_1
print_perf vtmm 1_4

print_perf memstrata 1_1
print_perf memstrata 1_4

print_perf_memtis 1_1
print_perf_memtis 1_4


