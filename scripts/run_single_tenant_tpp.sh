#!/bin/bash

# Only called by run_multi_tenants_tpp.sh
#1st input : order of tenant
#2nd input : workload
#3rd input : mix number

source ./set_bench_dir.sh
BENCH_CMD=$(./bench_cmd.sh $2 $3)

if [[ "$2" == "fotonik" ]]; then
        cur_path=$PWD
        cd ${SPECCPU_DIR}
        source shrc
        cd ${cur_path}
fi


CPUSETS="0-7"
echo ${CPUSETS} > ${CGCPU_DIR}/cpuset.cpus
echo 0-1 > ${CGCPU_DIR}/cpuset.mems
echo $$ > ${CGCPU_DIR}/cgroup.procs

function release_cpuset
{
	sleep 4s
	echo "0-23" > ${CGCPU_DIR}/cpuset.cpus
}

${BENCH_CMD} & release_cpuset
wait

echo "$2 done"

