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
#echo 10000 > ${CGMEM_DIR}/memory.cooling_period
#echo 1000 > ${CGMEM_DIR}/memory.adjust_period
echo enabled > ${CGMEM_DIR}/memory.use_mig
echo enabled > ${CGMEM_DIR}/memory.use_warm
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
        BENCH="${BENCH_PATH}/bc -f ${BENCH_PATH}/pregen_g28.sg -n 8"
        #BENCH="${BENCH_PATH}/bc -g 28 -n 30"
	echo 20G > ${CGMEM_DIR}/memory.max_at_node0
elif [[ "$2" == "gapbs-pr" ]]; then
        BENCH_PATH="${BENCH_DIR}/gapbs"
        BENCH="${BENCH_PATH}/pr -f ${BENCH_PATH}/pregen_g28.sg -i 1000 -t 1e-4 -n 8"
       	echo 20G > ${CGMEM_DIR}/memory.max_at_node0
elif [[ "$2" == "gapbs-cc_sv" ]]; then
        BENCH_PATH="${BENCH_DIR}/gapbs"
        BENCH="${BENCH_PATH}/cc_sv -f ${BENCH_PATH}/pregen_g28.sg -n 8"
	echo 20G > ${CGMEM_DIR}/memory.max_at_node0
elif [[ "$2" == "gapbs-tc" ]]; then
        BENCH_PATH="${BENCH_DIR}/gapbs"
        BENCH="${BENCH_PATH}/tc -f ${BENCH_PATH}/pregen_g27.sg -n 1"
	echo 20G > ${CGMEM_DIR}/memory.max_at_node0
elif [[ "$2" == "graph500" ]]; then
        BENCH_PATH="${BENCH_DIR}/graph500/omp-csr"
        BENCH="${BENCH_PATH}/omp-csr -s 26 -e 15 -V" #s27 e 15
	echo 20G > ${CGMEM_DIR}/memory.max_at_node0
elif [[ "$2" == "xsbench" ]]; then
        BENCH_PATH="${BENCH_DIR}/XSBench/openmp-threading"
        BENCH="${BENCH_PATH}/XSBench -t 24 -g 70000 -p 30000000" #g 130000
	echo 20G > ${CGMEM_DIR}/memory.max_at_node0
elif [[ "$2" == "xindex" ]]; then
        BENCH_PATH="${BENCH_DIR}/XIndex-H"
        BENCH="${BENCH_PATH}/build/ycsb_bench --fg 16 --iteration 70"
	echo 20G > ${CGMEM_DIR}/memory.max_at_node0
elif [[ "$2" == "btree" ]]; then
        BENCH_PATH="${BENCH_DIR}/../../vmitosis-workloads/bin"
        BENCH="${BENCH_PATH}/bench_btree_mt"
	echo 20G > ${CGMEM_DIR}/memory.max_at_node0
elif [[ "$2" == "silo" ]]; then
        BENCH_PATH="${BENCH_DIR}/silo"
        BENCH="${BENCH_PATH}/out-perf.masstree/benchmarks/dbtest --verbose --bench ycsb --num-threads 20 --scale-factor 200000 --ops-per-worker=500000000 --slow-exit"
	echo 20G > ${CGMEM_DIR}/memory.max_at_node0
elif [[ "$2" == "cpu_dlrm_small_low" ]]; then
        BENCH_PATH="${PWD}"
        BENCH="bash ${BENCH_PATH}/dp_ht_24c.sh small low"
        echo 20G > ${CGMEM_DIR}/memory.max_at_node0
elif [[ "$2" == "cpu_dlrm_small_mid" ]]; then
        BENCH_PATH="${PWD}"
        BENCH="bash ${BENCH_PATH}/dp_ht_24c.sh small mid"
        echo 20G > ${CGMEM_DIR}/memory.max_at_node0
elif [[ "$2" == "cpu_dlrm_small_high" ]]; then
        BENCH_PATH="${PWD}"
        BENCH="bash ${BENCH_PATH}/dp_ht_24c.sh small high"
        echo 20G > ${CGMEM_DIR}/memory.max_at_node0
elif [[ "$2" == "cpu_dlrm_med_low" ]]; then
        BENCH_PATH="${PWD}"
        BENCH="bash ${BENCH_PATH}/dp_ht_24c.sh med low"
        echo 20G > ${CGMEM_DIR}/memory.max_at_node0
elif [[ "$2" == "cpu_dlrm_med_mid" ]]; then
        BENCH_PATH="${PWD}"
        BENCH="bash ${BENCH_PATH}/dp_ht_24c.sh med mid"
        echo 20G > ${CGMEM_DIR}/memory.max_at_node0
elif [[ "$2" == "cpu_dlrm_med_high" ]]; then
        BENCH_PATH="${PWD}"
        BENCH="bash ${BENCH_PATH}/dp_ht_24c.sh med high"
        echo 20G > ${CGMEM_DIR}/memory.max_at_node0
elif [[ "$2" == "cpu_dlrm_large_low" ]]; then
        BENCH_PATH="${PWD}"
        BENCH="bash ${BENCH_PATH}/dp_ht_24c.sh large low"
        echo 20G > ${CGMEM_DIR}/memory.max_at_node0
elif [[ "$2" == "cpu_dlrm_large_mid" ]]; then
        BENCH_PATH="${PWD}"
        BENCH="bash ${BENCH_PATH}/dp_ht_24c.sh large mid"
        echo 20G > ${CGMEM_DIR}/memory.max_at_node0
elif [[ "$2" == "cpu_dlrm_large_high" ]]; then
        BENCH_PATH="${PWD}"
        BENCH="bash ${BENCH_PATH}/dp_ht_24c.sh large high"
        echo 20G > ${CGMEM_DIR}/memory.max_at_node0
elif [[ "$2" == "bwaves" ]]; then
        BENCH="runcpu --config=mttm_1 --noreportable --iteration=1 603.bwaves_s"
        echo 20G > ${CGMEM_DIR}/memory.max_at_node0
elif [[ "$2" == "fotonik" ]]; then
        BENCH="runcpu --config=mttm_1 --noreportable --iteration=1 649.fotonik3d_s"
        echo 20G > ${CGMEM_DIR}/memory.max_at_node0
elif [[ "$2" == "roms" ]]; then
        BENCH="runcpu --config=mttm_1 --noreportable --iteration=1 654.roms_s"
        echo 20G > ${CGMEM_DIR}/memory.max_at_node0
elif [[ "$2" == "nas_cg.d" ]]; then
        BENCH_PATH="${BENCH_DIR}/NPB3.4.2/NPB3.4-OMP"
        BENCH="${BENCH_PATH}/bin/cg.D.x"
	echo 20G > ${CGMEM_DIR}/memory.max_at_node0
elif [[ "$2" == "nas_bt.d" ]]; then
        BENCH_PATH="${BENCH_DIR}/NPB3.4.2/NPB3.4-OMP"
        BENCH="${BENCH_PATH}/bin/bt.D.x"
	echo 20G > ${CGMEM_DIR}/memory.max_at_node0
elif [[ "$2" == "nas_sp.d" ]]; then
        BENCH_PATH="${BENCH_DIR}/NPB3.4.2/NPB3.4-OMP"
        BENCH="${BENCH_PATH}/bin/sp.D.x"
	echo 20G > ${CGMEM_DIR}/memory.max_at_node0
elif [[ "$2" == "nas_mg.d" ]]; then
        BENCH_PATH="${BENCH_DIR}/NPB3.4.2/NPB3.4-OMP"
        BENCH="${BENCH_PATH}/bin/mg.D.x"
	echo 20G > ${CGMEM_DIR}/memory.max_at_node0
elif [[ "$2" == "nas_lu.d" ]]; then
        BENCH_PATH="${BENCH_DIR}/NPB3.4.2/NPB3.4-OMP"
        BENCH="${BENCH_PATH}/bin/lu.D.x"
	echo 20G > ${CGMEM_DIR}/memory.max_at_node0
elif [[ "$2" == "nas_ua.d" ]]; then
        BENCH_PATH="${BENCH_DIR}/NPB3.4.2/NPB3.4-OMP"
        BENCH="${BENCH_PATH}/bin/ua.D.x"
	echo 20G > ${CGMEM_DIR}/memory.max_at_node0
elif [[ "$2" == "gups_small" ]]; then
        BENCH_PATH="${BENCH_DIR}/../microbenchmarks"
        BENCH="${BENCH_PATH}/gups 8 2000000000 34 8 32"
	echo 10G > ${CGMEM_DIR}/memory.max_at_node0
elif [[ "$2" == "gups_large" ]]; then
        BENCH_PATH="${BENCH_DIR}/../microbenchmarks"
        BENCH="${BENCH_PATH}/gups 8 2000000000 35 8 33"
	echo 20G > ${CGMEM_DIR}/memory.max_at_node0
else
        echo "$2 benchmark is not supported"
        exit 0
fi

./run_bench ${BENCH}

