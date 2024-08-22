#!/bin/bash
BENCH_DIR=/home/cjlee/CXL-emulation.code/workloads

if [ $# -ne 2 ]
then
        echo "2 input required"
        exit 0
fi

CGMEM_DIR=/sys/fs/cgroup/memory/mttm_1
cgdelete -g memory:mttm_1
cgcreate -g memory:mttm_1
echo enabled > ${CGMEM_DIR}/memory.use_mig
echo enabled > ${CGMEM_DIR}/memory.use_warm
echo $$ > ${CGMEM_DIR}/cgroup.procs


CGCPU_DIR=/sys/fs/cgroup/cpuset/mttm_1
cgdelete -g cpuset:mttm_1
cgcreate -g cpuset:mttm_1

CPUSETS="0-7"
echo ${CPUSETS} > ${CGCPU_DIR}/cpuset.cpus
echo 0-1 > ${CGCPU_DIR}/cpuset.mems
echo $$ > ${CGCPU_DIR}/cgroup.procs
echo $2 > ${CGMEM_DIR}/memory.max_at_node0


if [[ "$1" == "gapbs-bc" ]]; then
        BENCH_PATH="${BENCH_DIR}/gapbs"
	BENCH="${BENCH_PATH}/bc -f ${BENCH_PATH}/pregen_g28.sg -n 8"
elif [[ "$1" == "gapbs-pr" ]]; then
        BENCH_PATH="${BENCH_DIR}/gapbs"
	BENCH="${BENCH_PATH}/pr -f ${BENCH_PATH}/pregen_g28.sg -i 1000 -t 1e-4 -n 6"	
elif [[ "$1" == "gapbs-cc_sv" ]]; then
        BENCH_PATH="${BENCH_DIR}/gapbs"
        BENCH="${BENCH_PATH}/cc_sv -f ${BENCH_PATH}/pregen_g28.sg -n 6"
elif [[ "$1" == "gapbs-tc" ]]; then
        BENCH_PATH="${BENCH_DIR}/gapbs"
	BENCH="${BENCH_PATH}/tc -f ${BENCH_PATH}/pregen_g27.sg -n 1"
elif [[ "$1" == "graph500" ]]; then
        BENCH_PATH="${BENCH_DIR}/graph500/omp-csr"
        BENCH="${BENCH_PATH}/omp-csr -s 26 -e 15 -V" #s27 e 15, options.h NBFS_max is number of BFS
elif [[ "$1" == "xsbench" ]]; then
        BENCH_PATH="${BENCH_DIR}/XSBench/openmp-threading"
	BENCH="${BENCH_PATH}/XSBench -t 8 -g 70000 -p 25000000"
elif [[ "$1" == "xindex" ]]; then
        BENCH_PATH="${BENCH_DIR}/XIndex-H"
	BENCH="${BENCH_PATH}/build/ycsb_bench --fg 6 --iteration 20"
elif [[ "$1" == "btree" ]]; then
        BENCH_PATH="${BENCH_DIR}/../../vmitosis-workloads/bin"
        BENCH="${BENCH_PATH}/bench_btree_mt"
elif [[ "$1" == "silo" ]]; then
        BENCH_PATH="${BENCH_DIR}/silo"
	BENCH="${BENCH_PATH}/out-perf.masstree/benchmarks/dbtest --verbose --bench ycsb --num-threads 8 --scale-factor 200000 --ops-per-worker=200000000"
elif [[ "$1" == "cpu_dlrm_small_low" ]]; then
        BENCH_PATH="${PWD}"
	BENCH="bash ${BENCH_PATH}/dp_ht_24c.sh small low"
elif [[ "$1" == "cpu_dlrm_small_mid" ]]; then
        BENCH_PATH="${PWD}"
        BENCH="bash ${BENCH_PATH}/dp_ht_24c.sh small mid"
elif [[ "$1" == "cpu_dlrm_small_high" ]]; then
        BENCH_PATH="${PWD}"
	BENCH="bash ${BENCH_PATH}/dp_ht_24c.sh small high"
elif [[ "$1" == "cpu_dlrm_med_low" ]]; then
        BENCH_PATH="${PWD}"
	BENCH="bash ${BENCH_PATH}/dp_ht_24c.sh med low"
elif [[ "$1" == "cpu_dlrm_med_mid" ]]; then
        BENCH_PATH="${PWD}"
        BENCH="bash ${BENCH_PATH}/dp_ht_24c.sh med mid"
elif [[ "$1" == "cpu_dlrm_med_high" ]]; then
        BENCH_PATH="${PWD}"
        BENCH="bash ${BENCH_PATH}/dp_ht_24c.sh med high"
elif [[ "$1" == "cpu_dlrm_large_low" ]]; then
        BENCH_PATH="${PWD}"
	BENCH="bash ${BENCH_PATH}/dp_ht_24c.sh large low"
elif [[ "$1" == "cpu_dlrm_large_mid" ]]; then
        BENCH_PATH="${PWD}"
        BENCH="bash ${BENCH_PATH}/dp_ht_24c.sh large mid"
elif [[ "$1" == "cpu_dlrm_large_high" ]]; then
        BENCH_PATH="${PWD}"
        BENCH="bash ${BENCH_PATH}/dp_ht_24c.sh large high"
#source shrc at SPECCPU_2017 before run SPECCPU
elif [[ "$1" == "bwaves" ]]; then
        BENCH="runcpu --config=mttm_1 --noreportable --iteration=1 603.bwaves_s"
elif [[ "$1" == "fotonik" ]]; then
	BENCH="runcpu --config=mttm_1 --noreportable --iteration=1 649.fotonik3d_s"
elif [[ "$1" == "roms" ]]; then
	BENCH="runcpu --config=mttm_1 --noreportable --iteration=1 654.roms_s"
elif [[ "$1" == "nas_cg.d" ]]; then
        BENCH_PATH="${BENCH_DIR}/NPB3.4.2/NPB3.4-OMP"
        BENCH="${BENCH_PATH}/bin/cg.D.x"
elif [[ "$1" == "nas_bt.d" ]]; then
        BENCH_PATH="${BENCH_DIR}/NPB3.4.2/NPB3.4-OMP"
        BENCH="${BENCH_PATH}/bin/bt.D.x"
elif [[ "$1" == "nas_sp.d" ]]; then
        BENCH_PATH="${BENCH_DIR}/NPB3.4.2/NPB3.4-OMP"
        BENCH="${BENCH_PATH}/bin/sp.D.x"
elif [[ "$1" == "nas_mg.d" ]]; then
        BENCH_PATH="${BENCH_DIR}/NPB3.4.2/NPB3.4-OMP"
        BENCH="${BENCH_PATH}/bin/mg.D.x"
elif [[ "$1" == "nas_lu.d" ]]; then
        BENCH_PATH="${BENCH_DIR}/NPB3.4.2/NPB3.4-OMP"
        BENCH="${BENCH_PATH}/bin/lu.D.x"
elif [[ "$1" == "nas_ua.d" ]]; then
        BENCH_PATH="${BENCH_DIR}/NPB3.4.2/NPB3.4-OMP"
        BENCH="${BENCH_PATH}/bin/ua.D.x"
else
        echo "wrong command"
        exit 0
fi

function release_cpuset
{
	sleep 6s
	echo "0-23" > ${CGCPU_DIR}/cpuset.cpus
}

./run_bench ${BENCH} & release_cpuset
wait

