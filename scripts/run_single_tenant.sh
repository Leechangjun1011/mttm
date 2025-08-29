#!/bin/bash
BENCH_DIR=/home/cjlee/CXL-emulation.code/workloads

# Only called by run_multi_tenants.sh
#1st input : order of tenant
#2nd input : workload
#3rd input : config



if [ $# -lt 2 ]
then
        echo "more than 2 input required"
        exit 0
fi

CGMEM_DIR=/sys/fs/cgroup/memory/mttm_$1
cgdelete -g memory:mttm_$1
cgcreate -g memory:mttm_$1
echo enabled > ${CGMEM_DIR}/memory.use_mig
echo enabled > ${CGMEM_DIR}/memory.use_warm
echo $$ > ${CGMEM_DIR}/cgroup.procs

CGCPU_DIR=/sys/fs/cgroup/cpuset/mttm_$1
cgdelete -g cpuset:mttm_$1
cgcreate -g cpuset:mttm_$1

CPUSETS="1-8"

echo ${CPUSETS} > ${CGCPU_DIR}/cpuset.cpus
echo 0-1 > ${CGCPU_DIR}/cpuset.mems
echo $$ > ${CGCPU_DIR}/cgroup.procs

if [[ "$2" == "gapbs-bc" ]]; then
        BENCH_PATH="${BENCH_DIR}/gapbs"
	if [[ "$3" == "mix1" ]]; then
	        BENCH="${BENCH_PATH}/bc -f ${BENCH_PATH}/pregen_g28.sg -n 16"
	elif [[ "$3" == "config1-bw1" ]]; then
	        BENCH="${BENCH_PATH}/bc -f ${BENCH_PATH}/pregen_g28.sg -n 16"
		echo 200G > ${CGMEM_DIR}/memory.max_at_node0
	elif [[ "$3" == "config1-bw2" ]]; then
	        BENCH="${BENCH_PATH}/bc -f ${BENCH_PATH}/pregen_g28.sg -n 16"
		echo 6G > ${CGMEM_DIR}/memory.max_at_node0
	else
	        BENCH="${BENCH_PATH}/bc -f ${BENCH_PATH}/pregen_g28.sg -n 8"
	fi
elif [[ "$2" == "gapbs-pr" ]]; then
        BENCH_PATH="${BENCH_DIR}/gapbs"
	if [[ "$3" == "mix1" || "$3" == "mix4" ]]; then
	        BENCH="${BENCH_PATH}/pr -f ${BENCH_PATH}/pregen_g28.sg -i 1000 -t 1e-4 -n 11"
	elif [[ "$3" == "config1-bw1" ]]; then
	        BENCH="${BENCH_PATH}/pr -f ${BENCH_PATH}/pregen_g28.sg -i 1000 -t 1e-4 -n 11"
		echo 200G > ${CGMEM_DIR}/memory.max_at_node0
	elif [[ "$3" == "config1-bw2" ]]; then
	        BENCH="${BENCH_PATH}/pr -f ${BENCH_PATH}/pregen_g28.sg -i 1000 -t 1e-4 -n 11"
		echo 4G > ${CGMEM_DIR}/memory.max_at_node0
	elif [[ "$3" == "config13-basepage" ]]; then
	        BENCH="${BENCH_PATH}/pr -f ${BENCH_PATH}/pregen_g28.sg -i 1000 -t 1e-4 -n 20"
	elif [[ "$3" == "config13-bw1" ]]; then
	        BENCH="${BENCH_PATH}/pr -f ${BENCH_PATH}/pregen_g28.sg -i 1000 -t 1e-4 -n 11"
		echo 17G > ${CGMEM_DIR}/memory.max_at_node0
	elif [[ "$3" == "config13-bw2" ]]; then
	        BENCH="${BENCH_PATH}/pr -f ${BENCH_PATH}/pregen_g28.sg -i 1000 -t 1e-4 -n 11"
		echo 17G > ${CGMEM_DIR}/memory.max_at_node0
	elif [[ "$3" == "config13-bw3" ]]; then
	        BENCH="${BENCH_PATH}/pr -f ${BENCH_PATH}/pregen_g28.sg -i 1000 -t 1e-4 -n 11"
		echo 17G > ${CGMEM_DIR}/memory.max_at_node0
	elif [[ "$3" == "config13-bw4" ]]; then
	        BENCH="${BENCH_PATH}/pr -f ${BENCH_PATH}/pregen_g28.sg -i 1000 -t 1e-4 -n 11"
		echo 17G > ${CGMEM_DIR}/memory.max_at_node0
	else
	        BENCH="${BENCH_PATH}/pr -f ${BENCH_PATH}/pregen_g28.sg -i 1000 -t 1e-4 -n 8"
	fi
elif [[ "$2" == "xsbench" ]]; then
        BENCH_PATH="${BENCH_DIR}/XSBench/openmp-threading"
	if [[ "$3" == "mix1" ]]; then
	        BENCH="${BENCH_PATH}/XSBench -t 8 -g 70000 -p 30000000"
	elif [[ "$3" == "config1-bw1" ]]; then
	        BENCH="${BENCH_PATH}/XSBench -t 8 -g 70000 -p 30000000"
		echo 200G > ${CGMEM_DIR}/memory.max_at_node0
	elif [[ "$3" == "config1-bw2" ]]; then
	        BENCH="${BENCH_PATH}/XSBench -t 8 -g 70000 -p 30000000"
		echo 3G > ${CGMEM_DIR}/memory.max_at_node0
	else
		BENCH="${BENCH_PATH}/XSBench -t 8 -g 70000 -p 25000000"
	fi
elif [[ "$2" == "xindex" ]]; then
        BENCH_PATH="$PWD/XIndex-H"
	if [[ "$3" == "mix2" ]]; then
		BENCH="${BENCH_PATH}/build/ycsb_bench --fg 6 --iteration 25"
	elif [[ "$3" == "config2-bw1" ]]; then
		BENCH="${BENCH_PATH}/build/ycsb_bench --fg 6 --iteration 25"
		echo 80G > ${CGMEM_DIR}/memory.max_at_node0
	elif [[ "$3" == "config2-bw2" ]]; then
		BENCH="${BENCH_PATH}/build/ycsb_bench --fg 6 --iteration 25"
		echo 11605M > ${CGMEM_DIR}/memory.max_at_node0
	elif [[ "$3" == "mix3" ]]; then
		BENCH="${BENCH_PATH}/build/ycsb_bench --fg 6 --iteration 22"
	else
		BENCH="${BENCH_PATH}/build/ycsb_bench --fg 6 --iteration 20"
		echo 23G > ${CGMEM_DIR}/memory.max_at_node0
	fi
elif [[ "$2" == "btree" ]]; then
        BENCH_PATH="${BENCH_DIR}/../../vmitosis-workloads/bin"
        BENCH="${BENCH_PATH}/bench_btree_mt"
elif [[ "$2" == "silo" ]]; then
        BENCH_PATH="${BENCH_DIR}/silo"	
	if [[ "$3" == "mix4" ]]; then
	        BENCH="${BENCH_PATH}/out-perf.masstree/benchmarks/dbtest --verbose --bench ycsb --num-threads 8 --scale-factor 400000 --ops-per-worker=450000000"
	elif [[ "$3" == "config13-basepage" ]]; then
	        BENCH="${BENCH_PATH}/out-perf.masstree/benchmarks/dbtest --verbose --bench ycsb --num-threads 8 --scale-factor 400000 --ops-per-worker=1000000000"
	elif [[ "$3" == "config13-bw1" ]]; then
	        BENCH="${BENCH_PATH}/out-perf.masstree/benchmarks/dbtest --verbose --bench ycsb --num-threads 8 --scale-factor 400000 --ops-per-worker=450000000"
		echo 17G > ${CGMEM_DIR}/memory.max_at_node0
	elif [[ "$3" == "config13-bw2" ]]; then
	        BENCH="${BENCH_PATH}/out-perf.masstree/benchmarks/dbtest --verbose --bench ycsb --num-threads 8 --scale-factor 400000 --ops-per-worker=450000000"
		echo 17G > ${CGMEM_DIR}/memory.max_at_node0
	elif [[ "$3" == "config13-bw3" ]]; then
	        BENCH="${BENCH_PATH}/out-perf.masstree/benchmarks/dbtest --verbose --bench ycsb --num-threads 8 --scale-factor 400000 --ops-per-worker=450000000"
		echo 17G > ${CGMEM_DIR}/memory.max_at_node0
	elif [[ "$3" == "config13-bw4" ]]; then
	        BENCH="${BENCH_PATH}/out-perf.masstree/benchmarks/dbtest --verbose --bench ycsb --num-threads 8 --scale-factor 400000 --ops-per-worker=450000000"
		echo 17G > ${CGMEM_DIR}/memory.max_at_node0
	else
		BENCH="${BENCH_PATH}/out-perf.masstree/benchmarks/dbtest --verbose --bench ycsb --num-threads 8 --scale-factor 240000 --ops-per-worker=450000000"
	fi
elif [[ "$2" == "cpu_dlrm_small_low" ]]; then
        BENCH_PATH="${PWD}"
	if [[ "$3" == "mix3" ]]; then
	        BENCH="bash ${BENCH_PATH}/dp_ht_8c.sh small low mix3"
	else
	        BENCH="bash ${BENCH_PATH}/dp_ht_8c.sh small low"
	fi
elif [[ "$2" == "cpu_dlrm_small_high" ]]; then
        BENCH_PATH="${PWD}"
	if [[ "$3" == "mix2" ]]; then
	        BENCH="bash ${BENCH_PATH}/dp_ht_8c.sh small high mix2"
	elif [[ "$3" == "config2-bw1" ]]; then
	        BENCH="bash ${BENCH_PATH}/dp_ht_8c.sh small high config2"
		echo 80G > ${CGMEM_DIR}/memory.max_at_node0
	elif [[ "$3" == "config2-bw2" ]]; then
	        BENCH="bash ${BENCH_PATH}/dp_ht_8c.sh small high config2"
		echo 11605M > ${CGMEM_DIR}/memory.max_at_node0
	else
	        BENCH="bash ${BENCH_PATH}/dp_ht_8c.sh small high"
	fi
#source shrc at SPECCPU_2017 before run SPECCPU
elif [[ "$2" == "fotonik" ]]; then
	cur_path=$PWD
	cd ${BENCH_DIR}/SPECCPU_2017
	source shrc
	cd ${cur_path}
	if [[ "$3" == "mix2"  || "$3" == "mix4" ]]; then
	        BENCH="runcpu --config=mttm_1 --noreportable --iteration=2 649.fotonik3d_s"
	elif [[ "$3" == "config2-bw1" ]]; then
	        BENCH="runcpu --config=mttm_1 --noreportable --iteration=2 649.fotonik3d_s"
		echo 80G > ${CGMEM_DIR}/memory.max_at_node0
	elif [[ "$3" == "config2-bw2" ]]; then
	        BENCH="runcpu --config=mttm_1 --noreportable --iteration=2 649.fotonik3d_s"
		echo 7G > ${CGMEM_DIR}/memory.max_at_node0
	elif [[ "$3" == "config13-basepage" ]]; then
	        BENCH="runcpu --config=mttm_1 --noreportable --iteration=6 649.fotonik3d_s"
	elif [[ "$3" == "config13-bw1" ]]; then
	        BENCH="runcpu --config=mttm_1 --noreportable --iteration=2 649.fotonik3d_s"
		echo 7G > ${CGMEM_DIR}/memory.max_at_node0
	elif [[ "$3" == "config13-bw2" ]]; then
	        BENCH="runcpu --config=mttm_1 --noreportable --iteration=2 649.fotonik3d_s"
		echo 8G > ${CGMEM_DIR}/memory.max_at_node0
	elif [[ "$3" == "config13-bw3" ]]; then
	        BENCH="runcpu --config=mttm_1 --noreportable --iteration=2 649.fotonik3d_s"
		echo 9G > ${CGMEM_DIR}/memory.max_at_node0
	elif [[ "$3" == "config13-bw4" ]]; then
	        BENCH="runcpu --config=mttm_1 --noreportable --iteration=2 649.fotonik3d_s"
		echo 10G > ${CGMEM_DIR}/memory.max_at_node0
	else
	        BENCH="runcpu --config=mttm_1 --noreportable --iteration=2 649.fotonik3d_s"
	fi
elif [[ "$2" == "gups-1" ]]; then
        BENCH_PATH="${BENCH_DIR}/../microbenchmarks"
	if [[ "$3" == "microbench" || "$3" == "microbench-dynamic" ]]; then
	        BENCH="${BENCH_PATH}/gups-1 8 4000000000 34 8 31 90"
	elif [[ "$3" == "microbench-sensitivity1" ]]; then
	        BENCH="${BENCH_PATH}/gups-1 8 4000000000 34 8 31 90"
	elif [[ "$3" == "microbench-sensitivity2" ]]; then
	        BENCH="${BENCH_PATH}/gups-1 2 4000000000 34 8 33 90"
	fi
elif [[ "$2" == "gups-2" ]]; then
        BENCH_PATH="${BENCH_DIR}/../microbenchmarks"
	if [[ "$3" == "microbench" || "$3" == "microbench-dynamic" ]]; then
	        BENCH="${BENCH_PATH}/gups-2 8 4000000000 34 8 32 90"
	elif [[ "$3" == "microbench-sensitivity1" ]]; then
	        BENCH="${BENCH_PATH}/gups-2 8 4000000000 34 8 32 90"
	elif [[ "$3" == "microbench-sensitivity2" ]]; then
	        BENCH="${BENCH_PATH}/gups-2 4 4000000000 34 8 33 90"
	fi
elif [[ "$2" == "gups-3" ]]; then
        BENCH_PATH="${BENCH_DIR}/../microbenchmarks"
	if [[ "$3" == "microbench" ]]; then
	        BENCH="${BENCH_PATH}/gups-3 8 4000000000 34 8 32 0"
	elif [[ "$3" == "microbench-dynamic" ]]; then
		sleep 50s
	        BENCH="${BENCH_PATH}/gups-3 8 4000000000 34 8 32 0"
	elif [[ "$3" == "microbench-sensitivity1" ]]; then
	        BENCH="${BENCH_PATH}/gups-3 8 4000000000 34 8 32 0"
	elif [[ "$3" == "microbench-sensitivity2" ]]; then
	        BENCH="${BENCH_PATH}/gups-3 8 4000000000 34 8 33 90"
	fi
elif [[ "$2" == "gups-2g" ]]; then
        BENCH_PATH="${BENCH_DIR}/../microbenchmarks"
        BENCH="${BENCH_PATH}/gups1 2 3000000000 34 8 31 90"
elif [[ "$2" == "gups-2g-8t" ]]; then
        BENCH_PATH="${BENCH_DIR}/../microbenchmarks"
        BENCH="${BENCH_PATH}/gups1 8 3000000000 34 8 31 90"
elif [[ "$2" == "gups-4g" ]]; then
        BENCH_PATH="${BENCH_DIR}/../microbenchmarks"
        BENCH="${BENCH_PATH}/gups2 4 3000000000 34 8 32 90"
elif [[ "$2" == "gups-4g-8t" ]]; then
        BENCH_PATH="${BENCH_DIR}/../microbenchmarks"
        BENCH="${BENCH_PATH}/gups2 8 3000000000 34 8 32 90"
elif [[ "$2" == "gups-16g" ]]; then
        BENCH_PATH="${BENCH_DIR}/../microbenchmarks"
        BENCH="${BENCH_PATH}/gups3 8 3000000000 34 8 34 0"
elif [[ "$2" == "gups_small" ]]; then
        BENCH_PATH="${BENCH_DIR}/../microbenchmarks"
        BENCH="${BENCH_PATH}/gups 8 2000000000 34 8 32"
elif [[ "$2" == "gups_large" ]]; then
        BENCH_PATH="${BENCH_DIR}/../microbenchmarks"
        BENCH="${BENCH_PATH}/gups 8 2000000000 35 8 33 90"
elif [[ "$2" == "gups_store" ]]; then
        BENCH_PATH="${BENCH_DIR}/../microbenchmarks"
        BENCH="${BENCH_PATH}/gups-store 8 4000000000 35 8 33"
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

./run_bench ${BENCH} & release_cpuset
wait
