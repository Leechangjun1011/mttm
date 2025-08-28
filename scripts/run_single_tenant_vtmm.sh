#!/bin/bash
BENCH_DIR=/home/cjlee/CXL-emulation.code/workloads

# Only called by run_multi_tenants_vtmm.sh
#1st input : order of tenant
#2nd input : workload
#3rd input : config
if [ $# -lt 2 ]
then
        echo "2 input required"
        exit 0
fi

CGMEM_DIR=/sys/fs/cgroup/memory/vtmm_$1
cgdelete -g memory:vtmm_$1
cgcreate -g memory:vtmm_$1
echo $$ > ${CGMEM_DIR}/cgroup.procs


CGCPU_DIR=/sys/fs/cgroup/cpuset/vtmm_$1
cgdelete -g cpuset:vtmm_$1
cgcreate -g cpuset:vtmm_$1

CPUSETS="0-7"

echo ${CPUSETS} > ${CGCPU_DIR}/cpuset.cpus
echo 0-1 > ${CGCPU_DIR}/cpuset.mems
echo $$ > ${CGCPU_DIR}/cgroup.procs


if [[ "$2" == "gapbs-bc" ]]; then
        BENCH_PATH="${BENCH_DIR}/gapbs"
	if [[ "$3" == "mix1" ]]; then
	        BENCH="${BENCH_PATH}/bc -f ${BENCH_PATH}/pregen_g28.sg -n 16"
	else
	        BENCH="${BENCH_PATH}/bc -f ${BENCH_PATH}/pregen_g28.sg -n 20"
	fi
elif [[ "$2" == "gapbs-pr" ]]; then
        BENCH_PATH="${BENCH_DIR}/gapbs"
	if [[ "$3" == "mix1" || "$3" == "mix4" ]]; then
	        BENCH="${BENCH_PATH}/pr -f ${BENCH_PATH}/pregen_g28.sg -i 1000 -t 1e-4 -n 11"
	else
	        BENCH="${BENCH_PATH}/pr -f ${BENCH_PATH}/pregen_g28.sg -i 1000 -t 1e-4 -n 14"
	fi
elif [[ "$2" == "xsbench" ]]; then
        BENCH_PATH="${BENCH_DIR}/XSBench/openmp-threading"
	if [[ "$3" == "config1" ]]; then
	        BENCH="${BENCH_PATH}/XSBench -t 8 -g 70000 -p 30000000"
	else
		BENCH="${BENCH_PATH}/XSBench -t 8 -g 70000 -p 25000000"
	fi
elif [[ "$2" == "xindex" ]]; then
        BENCH_PATH="${BENCH_DIR}/XIndex-H"
	if [[ "$3" == "mix2" ]]; then
	        BENCH="${BENCH_PATH}/build/ycsb_bench --fg 6 --iteration 25"
	elif [[ "$3" == "mix3" ]]; then
	        BENCH="${BENCH_PATH}/build/ycsb_bench --fg 6 --iteration 22"
	else
	        BENCH="${BENCH_PATH}/build/ycsb_bench --fg 6 --iteration 35"
	fi
elif [[ "$2" == "btree" ]]; then
        BENCH_PATH="${BENCH_DIR}/../../vmitosis-workloads/bin"
        BENCH="${BENCH_PATH}/bench_btree_mt"
elif [[ "$2" == "silo" ]]; then
        BENCH_PATH="${BENCH_DIR}/silo"
	if [[ "$3" == "mix4" ]]; then
	        BENCH="${BENCH_PATH}/out-perf.masstree/benchmarks/dbtest --verbose --bench ycsb --num-threads 8 --scale-factor 400000 --ops-per-worker=450000000"
	else
	        BENCH="${BENCH_PATH}/out-perf.masstree/benchmarks/dbtest --verbose --bench ycsb --num-threads 8 --scale-factor 200000 --ops-per-worker=1100000000"
	fi
elif [[ "$2" == "cpu_dlrm_small_low" ]]; then
        BENCH_PATH="${PWD}"
        BENCH="bash ${BENCH_PATH}/dp_ht_24c.sh small low"
	if [[ "$3" == "mix3" ]]; then
	        BENCH="bash ${BENCH_PATH}/dp_ht_24c.sh small low mix3"
	fi
elif [[ "$2" == "cpu_dlrm_small_high" ]]; then
        BENCH_PATH="${PWD}"
	if [[ "$3" == "mix2" ]]; then
	        BENCH="bash ${BENCH_PATH}/dp_ht_24c.sh small high mix2"
	else
	        BENCH="bash ${BENCH_PATH}/dp_ht_24c.sh small high"
	fi
#source shrc at SPECCPU_2017 before run SPECCPU
elif [[ "$2" == "fotonik" ]]; then
	cur_path=$PWD
	cd ${BENCH_DIR}/SPECCPU_2017
	source shrc
	cd ${cur_path}
	if [[ "$3" == "mix2" || "$3" == "mix4" ]]; then
	        BENCH="runcpu --config=mttm_1 --noreportable --iteration=2 649.fotonik3d_s"
	else
	        BENCH="runcpu --config=mttm_1 --noreportable --iteration=1 649.fotonik3d_s"
	fi
else
        echo "$2 benchmark is not supported"
        exit 0
fi

function release_cpuset
{
	sleep 6s
	echo "0-23" > ${CGCPU_DIR}/cpuset.cpus
}

if [ $1 -gt 6 ]
then
	sleep 2s
fi


./run_bench_vtmm ${BENCH} & release_cpuset
wait

