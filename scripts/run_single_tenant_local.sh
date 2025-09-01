#!/bin/bash

# Only called by run_multi_tenants.sh
#1st input : order of tenant
#2nd input : workload
#3rd input : mix number
if [ $# -lt 2 ]
then
        echo "more than 2 input required"
        exit 0
fi

source ./set_bench_dir.sh
BENCH_CMD=$(./bench_cmd.sh $2 $3)

if [[ "$2" == "fotonik" ]]; then
        cur_path=$PWD
        cd ${SPECCPU_DIR}
        source shrc
        cd ${cur_path}
fi

CGMEM_DIR=/sys/fs/cgroup/memory/mttm_$1
cgdelete -g memory:mttm_$1
cgcreate -g memory:mttm_$1
echo $$ > ${CGMEM_DIR}/cgroup.procs

CGCPU_DIR=/sys/fs/cgroup/cpuset/mttm_$1
cgdelete -g cpuset:mttm_$1
cgcreate -g cpuset:mttm_$1

CPUSETS="0-7"

echo ${CPUSETS} > ${CGCPU_DIR}/cpuset.cpus
echo 0-1 > ${CGCPU_DIR}/cpuset.mems
echo $$ > ${CGCPU_DIR}/cgroup.procs



function release_cpuset
{
	sleep 6s
	echo "0-23" > ${CGCPU_DIR}/cpuset.cpus
}

time ${BENCH_CMD} & release_cpuset
wait

echo "$2 done"

