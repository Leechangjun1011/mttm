#!/bin/bash
BENCH_DIR=/home/cjlee/CXL-emulation.code/workloads

#1st input : order of tenant
#2nd input : workload

if [ $# -ne 2 ]
then
        echo "2 input required"
        exit 0
fi

CGROUP_DIR=/sys/fs/cgroup/memory/mttm_$1
cgdelete -g memory:mttm_$1
cgcreate -g memory:mttm_$1
echo $$ > ${CGROUP_DIR}/cgroup.procs
echo 2000000 > ${CGROUP_DIR}/memory.cooling_period
echo 20000 > ${CGROUP_DIR}/memory.adjust_period
echo enabled > ${CGROUP_DIR}/memory.use_mig
echo disabled > ${CGROUP_DIR}/memory.use_warm

if [[ "$2" == "gapbs" ]]; then
        BENCH_PATH="${BENCH_DIR}/gapbs"
        #BENCH="${BENCH_PATH}/bc -f ${BENCH_PATH}/pregen_g29.sg -n 16"
        BENCH="${BENCH_PATH}/bc -g 28 -n 24"
	echo 13G > ${CGROUP_DIR}/memory.max_at_node0
elif [[ "$2" == "xindex" ]]; then
        BENCH_PATH="${BENCH_DIR}/XIndex-H"
        BENCH="${BENCH_PATH}/build/ycsb_bench --fg 16 --iteration 20"
	echo 24G > ${CGROUP_DIR}/memory.max_at_node0
elif [[ "$2" == "nas_cg.d" ]]; then
        BENCH_PATH="${BENCH_DIR}/NPB3.4.2/NPB3.4-OMP"
        BENCH="${BENCH_PATH}/bin/cg.D.x"
	echo 6G > ${CGROUP_DIR}/memory.max_at_node0
else
        echo "$2 benchmark is not supported"
        exit 0
fi

./run_bench ${BENCH}

