#!/bin/bash
BENCH_DIR=/home/cjlee/CXL-emulation.code/workloads

# Only called by run_multi_tenants.sh
#1st input : order of tenant
#2nd input : workload

if [ $# -ne 2 ]
then
        echo "2 input required"
        exit 0
fi

CGMEM_DIR=/sys/fs/cgroup/memory/mttm_$1
cgdelete -g memory:mttm_$1
cgcreate -g memory:mttm_$1
echo $$ > ${CGMEM_DIR}/cgroup.procs

: << END
CGCPU_DIR=/sys/fs/cgroup/cpuset/mttm_$1
cgdelete -g cpuset:mttm_$1
cgcreate -g cpuset:mttm_$1
if [[ $1 -eq 1 ]]
then
	CPUSETS="0-7"
elif [[ $1 -eq 2 ]]
then
	CPUSETS="8-15"
else
	CPUSETS="16-23"
fi
echo ${CPUSETS} > ${CGCPU_DIR}/cpuset.cpus
echo 0-1 > ${CGCPU_DIR}/cpuset.mems
echo $$ > ${CGCPU_DIR}/cgroup.procs
END

if [[ "$2" == "gapbs-bc" ]]; then
        BENCH_PATH="${BENCH_DIR}/gapbs"
        BENCH="${BENCH_PATH}/bc -f ${BENCH_PATH}/pregen_g28.sg -n 30"
        #BENCH="${BENCH_PATH}/bc -g 28 -n 30"
	echo 13G > ${CGMEM_DIR}/memory.max_at_node0
elif [[ "$2" == "gapbs-pr" ]]; then
        BENCH_PATH="${BENCH_DIR}/gapbs"
        BENCH="${BENCH_PATH}/pr -f ${BENCH_PATH}/pregen_g29.sg -i 1000 -t 1e-4 -n 8"
        #BENCH="${BENCH_PATH}/bc -g 28 -n 30"
	echo 28G > ${CGMEM_DIR}/memory.max_at_node0
elif [[ "$2" == "graph500" ]]; then
        BENCH_PATH="${BENCH_DIR}/graph500/omp-csr"
        BENCH="${BENCH_PATH}/omp-csr -s 27 -e 15 -V"
        echo 10G > ${CGMEM_DIR}/memory.max_at_node0
elif [[ "$2" == "xsbench" ]]; then
        BENCH_PATH="${BENCH_DIR}/XSBench/openmp-threading"
        BENCH="${BENCH_PATH}/XSBench -t 24 -g 130000 -p 30000000"
        echo 4G > ${CGMEM_DIR}/memory.max_at_node0
elif [[ "$2" == "xindex" ]]; then
        BENCH_PATH="${BENCH_DIR}/XIndex-H"
        BENCH="${BENCH_PATH}/build/ycsb_bench --fg 16 --iteration 70"
	echo 10G > ${CGMEM_DIR}/memory.max_at_node0
elif [[ "$2" == "silo" ]]; then
        BENCH_PATH="${BENCH_DIR}/silo"
        BENCH="${BENCH_PATH}/out-perf.masstree/benchmarks/dbtest --verbose --bench ycsb --num-threads 20 --scale-factor 200000 --ops-per-worker=500000000 --slow-exit"
	echo 10G > ${CGMEM_DIR}/memory.max_at_node0
elif [[ "$2" == "nas_cg.d" ]]; then
        BENCH_PATH="${BENCH_DIR}/NPB3.4.2/NPB3.4-OMP"
        BENCH="${BENCH_PATH}/bin/cg.D.x"
	echo 10G > ${CGMEM_DIR}/memory.max_at_node0
elif [[ "$2" == "nas_bt.d" ]]; then
        BENCH_PATH="${BENCH_DIR}/NPB3.4.2/NPB3.4-OMP"
        BENCH="${BENCH_PATH}/bin/bt.D.x"
	echo 4382M > ${CGMEM_DIR}/memory.max_at_node0
else
        echo "$2 benchmark is not supported"
        exit 0
fi

${BENCH}

