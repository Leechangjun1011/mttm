#!/bin/bash
BENCH_DIR=/home/cjlee/CXL-emulation.code/workloads

# Only called by run_multi_tenants.sh
#1st input : order of tenant
#2nd input : workload


CGCPU_DIR=/sys/fs/cgroup/cpuset/mttm_$1
cgdelete -g cpuset:mttm_$1
cgcreate -g cpuset:mttm_$1
: << END
if [[ $1 -eq 1 ]]
then
	CPUSETS="0-7"
elif [[ $1 -eq 2 ]]
then
	CPUSETS="8-15"
else
	CPUSETS="16-23"
fi
END
CPUSETS="0-7"
echo ${CPUSETS} > ${CGCPU_DIR}/cpuset.cpus
echo 0-1 > ${CGCPU_DIR}/cpuset.mems
echo $$ > ${CGCPU_DIR}/cgroup.procs


if [[ "$2" == "gapbs-bc" ]]; then
        BENCH_PATH="${BENCH_DIR}/gapbs"
        BENCH="${BENCH_PATH}/bc -f ${BENCH_PATH}/pregen_g28.sg -n 16"
elif [[ "$2" == "gapbs-pr" ]]; then
        BENCH_PATH="${BENCH_DIR}/gapbs"
	if [[ "$3" == "config1" ]]; then
	        BENCH="${BENCH_PATH}/pr -f ${BENCH_PATH}/pregen_g28.sg -i 1000 -t 1e-4 -n 11"
	elif [[ "$3" == "config13" ]]; then
	        BENCH="${BENCH_PATH}/pr -f ${BENCH_PATH}/pregen_g28.sg -i 1000 -t 1e-4 -n 11"
	else
	        BENCH="${BENCH_PATH}/pr -f ${BENCH_PATH}/pregen_g28.sg -i 1000 -t 1e-4 -n 8"
	fi
elif [[ "$2" == "xsbench" ]]; then
        BENCH_PATH="${BENCH_DIR}/XSBench/openmp-threading"
        BENCH="${BENCH_PATH}/XSBench -t 8 -g 70000 -p 30000000" #g 130000
elif [[ "$2" == "xindex" ]]; then
        BENCH_PATH="${BENCH_DIR}/XIndex-H"
        BENCH="${BENCH_PATH}/build/ycsb_bench --fg 6 --iteration 25"
elif [[ "$2" == "xindex_large" ]]; then
        BENCH_PATH="${BENCH_DIR}/XIndex-H"
        BENCH="${BENCH_PATH}/build/ycsb_bench_large --fg 6 --iteration 20"
elif [[ "$2" == "silo" ]]; then
        BENCH_PATH="${BENCH_DIR}/silo"
        BENCH="${BENCH_PATH}/out-perf.masstree/benchmarks/dbtest --verbose --bench ycsb --num-threads 8 --scale-factor 400000 --ops-per-worker=450000000"
elif [[ "$2" == "cpu_dlrm_small_low" ]]; then
        BENCH_PATH="${PWD}"
        BENCH="bash ${BENCH_PATH}/dp_ht_24c.sh small low config12"
elif [[ "$2" == "cpu_dlrm_small_high" ]]; then
        BENCH_PATH="${PWD}"
        BENCH="bash ${BENCH_PATH}/dp_ht_24c.sh small high config2"
elif [[ "$2" == "cpu_dlrm_med_low" ]]; then
        BENCH_PATH="${PWD}"
        BENCH="bash ${BENCH_PATH}/dp_ht_24c.sh med low"
elif [[ "$2" == "cpu_dlrm_med_mid" ]]; then
        BENCH_PATH="${PWD}"
        BENCH="bash ${BENCH_PATH}/dp_ht_24c.sh med mid"
elif [[ "$2" == "cpu_dlrm_med_high" ]]; then
        BENCH_PATH="${PWD}"
        BENCH="bash ${BENCH_PATH}/dp_ht_24c.sh med high"
elif [[ "$2" == "cpu_dlrm_large_low" ]]; then
        BENCH_PATH="${PWD}"
        BENCH="bash ${BENCH_PATH}/dp_ht_24c.sh large low"
elif [[ "$2" == "cpu_dlrm_large_mid" ]]; then
        BENCH_PATH="${PWD}"
        BENCH="bash ${BENCH_PATH}/dp_ht_24c.sh large mid"
elif [[ "$2" == "cpu_dlrm_large_high" ]]; then
        BENCH_PATH="${PWD}"
        BENCH="bash ${BENCH_PATH}/dp_ht_24c.sh large high"
elif [[ "$2" == "nas_cg.d" ]]; then
        BENCH_PATH="${BENCH_DIR}/NPB3.4.2/NPB3.4-OMP"
        BENCH="${BENCH_PATH}/bin/cg.D.x"
elif [[ "$2" == "nas_bt.d" ]]; then
        BENCH_PATH="${BENCH_DIR}/NPB3.4.2/NPB3.4-OMP"
        BENCH="${BENCH_PATH}/bin/bt.D.x"
elif [[ "$2" == "nas_sp.d" ]]; then
        BENCH_PATH="${BENCH_DIR}/NPB3.4.2/NPB3.4-OMP"
        BENCH="${BENCH_PATH}/bin/sp.D.x"
elif [[ "$2" == "nas_mg.d" ]]; then
        BENCH_PATH="${BENCH_DIR}/NPB3.4.2/NPB3.4-OMP"
        BENCH="${BENCH_PATH}/bin/mg.D.x"
elif [[ "$2" == "nas_lu.d" ]]; then
        BENCH_PATH="${BENCH_DIR}/NPB3.4.2/NPB3.4-OMP"
        BENCH="${BENCH_PATH}/bin/lu.D.x"
elif [[ "$2" == "nas_ua.d" ]]; then
        BENCH_PATH="${BENCH_DIR}/NPB3.4.2/NPB3.4-OMP"
        BENCH="${BENCH_PATH}/bin/ua.D.x"
elif [[ "$2" == "btree" ]]; then
        BENCH_PATH="${BENCH_DIR}/../../vmitosis-workloads/bin"
        BENCH="${BENCH_PATH}/bench_btree_mt"
elif [[ "$2" == "gups_small" ]]; then
        BENCH_PATH="${BENCH_DIR}/../microbenchmarks"
        BENCH="${BENCH_PATH}/gups 8 2000000000 34 8 32"
elif [[ "$2" == "gups_large" ]]; then
        BENCH_PATH="${BENCH_DIR}/../microbenchmarks"
        BENCH="${BENCH_PATH}/gups 8 4000000000 35 8 33"
elif [[ "$2" == "gups_store" ]]; then
        BENCH_PATH="${BENCH_DIR}/../microbenchmarks"
        BENCH="${BENCH_PATH}/gups-store 8 4000000000 35 8 33"
elif [[ "$2" == "fotonik" ]]; then
        if [[ "$3" == "config2" ]]; then
                BENCH="runcpu --config=mttm_1 --noreportable --iteration=2 649.fotonik3d_s"
        elif [[ "$3" == "config13" ]]; then
                BENCH="runcpu --config=mttm_1 --noreportable --iteration=2 649.fotonik3d_s"
        else
                BENCH="runcpu --config=mttm_1 --noreportable --iteration=1 649.fotonik3d_s"
        fi
elif [[ "$2" == "roms" ]]; then
        if [[ "$3" == "config4" ]]; then
                BENCH="runcpu --config=mttm_1 --noreportable --iteration=2 654.roms_s"
        else
                BENCH="runcpu --config=mttm_1 --noreportable --iteration=1 654.roms_s"
        fi
else
        echo "$2 benchmark is not supported"
        exit 0
fi

function release_cpuset
{
	sleep 4s
	echo "0-23" > ${CGCPU_DIR}/cpuset.cpus
}

${BENCH} & release_cpuset
wait

echo "$2 done"

