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

if [[ "$3" == "6tenants" ]]; then
        CPUSETS="0-3"
elif [[ "$3" == "12tenants" ]]; then
        CPUSETS="0-1"
	if [ $1 -lt 5 ]; then
		CPUSETS="0-1"
	elif [ $1 -lt 9 ]; then
		CPUSETS="2-3"
	elif [ $1 -lt 13 ]; then
		CPUSETS="4-5"
	fi

        if [[ "$2" == "cpu_dlrm_small_low_1" ]]; then
                CPUSETS="0-23"
        elif [[ "$2" == "cpu_dlrm_small_low_2" ]]; then
                CPUSETS="0-23"
	fi
else
        CPUSETS="0-7"
fi

echo ${CPUSETS} > ${CGCPU_DIR}/cpuset.cpus
echo 0-1 > ${CGCPU_DIR}/cpuset.mems
echo $$ > ${CGCPU_DIR}/cgroup.procs


if [[ "$2" == "gapbs-bc" ]]; then
        BENCH_PATH="${BENCH_DIR}/gapbs"
	if [[ "$3" == "config1" ]]; then
	        BENCH="${BENCH_PATH}/bc -f ${BENCH_PATH}/pregen_g28.sg -n 12"
	elif [[ "$3" == "config5" ]]; then
	        BENCH="${BENCH_PATH}/bc -f ${BENCH_PATH}/pregen_g28.sg -n 20"
	elif [[ "$3" == "config7" ]]; then
	        BENCH="${BENCH_PATH}/bc -f ${BENCH_PATH}/pregen_g28.sg -n 38"
	elif [[ "$3" == "config8" ]]; then
	        BENCH="${BENCH_PATH}/bc -f ${BENCH_PATH}/pregen_g28.sg -n 22"
	elif [[ "$3" == "6tenants" ]]; then
	        BENCH="${BENCH_PATH}/bc -f ${BENCH_PATH}/pregen_g26.sg -n 28"
	elif [[ "$3" == "12tenants" ]]; then
	        BENCH="${BENCH_PATH}/bc -f ${BENCH_PATH}/pregen_g26.sg -n 26"
	else
	        BENCH="${BENCH_PATH}/bc -f ${BENCH_PATH}/pregen_g28.sg -n 8"
	fi
	echo 2G > ${CGMEM_DIR}/memory.max_at_node0
elif [[ "$2" == "gapbs-pr" ]]; then
        BENCH_PATH="${BENCH_DIR}/gapbs"
	if [[ "$3" == "config1" ]]; then
	        BENCH="${BENCH_PATH}/pr -f ${BENCH_PATH}/pregen_g28.sg -i 1000 -t 1e-4 -n 8"
	elif [[ "$3" == "config5" ]]; then
	        BENCH="${BENCH_PATH}/pr -f ${BENCH_PATH}/pregen_g28.sg -i 1000 -t 1e-4 -n 14"
	elif [[ "$3" == "config6" ]]; then
	        BENCH="${BENCH_PATH}/pr -f ${BENCH_PATH}/pregen_g28.sg -i 1000 -t 1e-4 -n 8"
	elif [[ "$3" == "6tenants" ]]; then
	        BENCH="${BENCH_PATH}/pr -f ${BENCH_PATH}/pregen_g26.sg -i 1000 -t 1e-4 -n 20"
	elif [[ "$3" == "12tenants" ]]; then
	        BENCH="${BENCH_PATH}/pr -f ${BENCH_PATH}/pregen_g26.sg -i 1000 -t 1e-4 -n 18"
	elif [[ "$3" == "motiv" ]]; then
	        BENCH="${BENCH_PATH}/pr -f ${BENCH_PATH}/pregen_g28.sg -i 1000 -t 1e-4 -n 8"
	elif [[ "$3" == "motiv-pr" ]]; then
	        BENCH="${BENCH_PATH}/pr -f ${BENCH_PATH}/pregen_g28.sg -i 1000 -t 1e-4 -n 8"
	else
	        BENCH="${BENCH_PATH}/pr -f ${BENCH_PATH}/pregen_g28.sg -i 1000 -t 1e-4 -n 8"
	fi
       	echo 3G > ${CGMEM_DIR}/memory.max_at_node0
elif [[ "$2" == "gapbs-cc_sv" ]]; then
        BENCH_PATH="${BENCH_DIR}/gapbs"
        BENCH="${BENCH_PATH}/cc_sv -f ${BENCH_PATH}/pregen_g28.sg -n 10"
	echo 20G > ${CGMEM_DIR}/memory.max_at_node0
elif [[ "$2" == "gapbs-tc" ]]; then
        BENCH_PATH="${BENCH_DIR}/gapbs"
	if [[ "$3" == "config7" ]]; then
	        BENCH="${BENCH_PATH}/tc -f ${BENCH_PATH}/pregen_g27.sg -n 1"
	else
	        BENCH="${BENCH_PATH}/tc -f ${BENCH_PATH}/pregen_g27.sg -n 1"
	fi
	echo 80G > ${CGMEM_DIR}/memory.max_at_node0
elif [[ "$2" == "graph500" ]]; then
        BENCH_PATH="${BENCH_DIR}/graph500/omp-csr"
        BENCH="${BENCH_PATH}/omp-csr -s 26 -e 15 -V" #s27 e 15, options.h NBFS_max is number of BFS
	echo 2G > ${CGMEM_DIR}/memory.max_at_node0
elif [[ "$2" == "xsbench" ]]; then
        BENCH_PATH="${BENCH_DIR}/XSBench/openmp-threading"
	if [[ "$3" == "config3" ]]; then
	        BENCH="${BENCH_PATH}/XSBench -t 8 -g 70000 -p 35000000"
	elif [[ "$3" == "config4" ]]; then
	        BENCH="${BENCH_PATH}/XSBench -t 8 -g 70000 -p 60000000"
	elif [[ "$3" == "6tenants" ]]; then
	        BENCH="${BENCH_PATH}/XSBench -t 4 -g 25000 -p 12000000"
	elif [[ "$3" == "12tenants" ]]; then
	        BENCH="${BENCH_PATH}/XSBench -t 2 -g 25000 -p 11000000"
	elif [[ "$3" == "motiv" ]]; then
	        BENCH="${BENCH_PATH}/XSBench -t 8 -g 70000 -p 25000000"
	elif [[ "$3" == "motiv-xsbench" ]]; then
	        BENCH="${BENCH_PATH}/XSBench -t 8 -g 70000 -p 20000000"
	else
		BENCH="${BENCH_PATH}/XSBench -t 8 -g 70000 -p 25000000"
	fi
	echo 1100M > ${CGMEM_DIR}/memory.max_at_node0
elif [[ "$2" == "xindex" ]]; then
        BENCH_PATH="${BENCH_DIR}/XIndex-H"
	if [[ "$3" == "config3" ]]; then
		BENCH="${BENCH_PATH}/build/ycsb_bench --fg 6 --iteration 30"
	elif [[ "$3" == "config7" ]]; then
		BENCH="${BENCH_PATH}/build/ycsb_bench --fg 6 --iteration 50"
	elif [[ "$3" == "config8" ]]; then
		BENCH="${BENCH_PATH}/build/ycsb_bench --fg 6 --iteration 30"
	elif [[ "$3" == "motiv-xindex" ]]; then
		BENCH="${BENCH_PATH}/build/ycsb_bench --fg 6 --iteration 20"
	else
		BENCH="${BENCH_PATH}/build/ycsb_bench --fg 6 --iteration 35"
	fi
	echo 13G > ${CGMEM_DIR}/memory.max_at_node0
elif [[ "$2" == "btree" ]]; then
        BENCH_PATH="${BENCH_DIR}/../../vmitosis-workloads/bin"
        BENCH="${BENCH_PATH}/bench_btree_mt"
	echo 20G > ${CGMEM_DIR}/memory.max_at_node0
elif [[ "$2" == "silo" ]]; then
        BENCH_PATH="${BENCH_DIR}/silo"
	if [[ "$3" == "config5" ]]; then
	        BENCH="${BENCH_PATH}/out-perf.masstree/benchmarks/dbtest --verbose --bench ycsb --num-threads 8 --scale-factor 100000 --ops-per-worker=650000000"
	elif [[ "$3" == "config6" ]]; then
	        BENCH="${BENCH_PATH}/out-perf.masstree/benchmarks/dbtest --verbose --bench ycsb --num-threads 8 --scale-factor 400000 --ops-per-worker=450000000"
	elif [[ "$3" == "config9" ]]; then
	        BENCH="${BENCH_PATH}/out-perf.masstree/benchmarks/dbtest --verbose --bench ycsb --num-threads 8 --scale-factor 400000 --ops-per-worker=1000000000"
	elif [[ "$3" == "config10" ]]; then
	        BENCH="${BENCH_PATH}/out-perf.masstree/benchmarks/dbtest --verbose --bench ycsb --num-threads 8 --scale-factor 400000 --ops-per-worker=450000000"
	elif [[ "$3" == "6tenants" ]]; then
	        BENCH="${BENCH_PATH}/out-perf.masstree/benchmarks/dbtest --verbose --bench ycsb --num-threads 4 --scale-factor 80000 --ops-per-worker=450000000"
	elif [[ "$3" == "12tenants" ]]; then
		BENCH="${BENCH_PATH}/out-perf.masstree/benchmarks/dbtest --verbose --bench ycsb --num-threads 2 --scale-factor 80000 --ops-per-worker=900000000"
	else
	       BENCH="${BENCH_PATH}/out-perf.masstree/benchmarks/dbtest --verbose --bench ycsb --num-threads 8 --scale-factor 200000 --ops-per-worker=1100000000 --slow-exit"
	fi
	echo 40221M > ${CGMEM_DIR}/memory.max_at_node0
elif [[ "$2" == "cpu_dlrm_small_low" ]]; then
        BENCH_PATH="${PWD}"
	if [[ "$3" == "config1" ]]; then
	        BENCH="bash ${BENCH_PATH}/dp_ht_24c.sh small low config1"
	elif [[ "$3" == "config9" ]]; then
	        BENCH="bash ${BENCH_PATH}/dp_ht_24c.sh small low config9"
	elif [[ "$3" == "config10" ]]; then
	        BENCH="bash ${BENCH_PATH}/dp_ht_24c.sh small low config10"
	elif [[ "$3" == "6tenants" ]]; then
	        BENCH="bash ${BENCH_PATH}/dp_ht_24c.sh small low 6tenants"
	elif [[ "$3" == "motiv" ]]; then
	        BENCH="bash ${BENCH_PATH}/dp_ht_24c.sh small low motiv"
	else
	        BENCH="bash ${BENCH_PATH}/dp_ht_24c.sh small low"
	fi
        echo 5111M > ${CGMEM_DIR}/memory.max_at_node0
elif [[ "$2" == "cpu_dlrm_small_low_1" ]]; then
        BENCH_PATH="${PWD}"
	BENCH="bash ${BENCH_PATH}/dp_ht_2c_1.sh small low 12tenants"
elif [[ "$2" == "cpu_dlrm_small_low_2" ]]; then
        BENCH_PATH="${PWD}"
	BENCH="bash ${BENCH_PATH}/dp_ht_2c_2.sh small low 12tenants"
elif [[ "$2" == "cpu_dlrm_small_mid" ]]; then
        BENCH_PATH="${PWD}"
        BENCH="bash ${BENCH_PATH}/dp_ht_24c.sh small mid"
        echo 20G > ${CGMEM_DIR}/memory.max_at_node0
elif [[ "$2" == "cpu_dlrm_small_high" ]]; then
        BENCH_PATH="${PWD}"
	if [[ "$3" == "config8" ]]; then
	        BENCH="bash ${BENCH_PATH}/dp_ht_24c.sh small high config8"
	elif [[ "$3" == "motiv-cpu_dlrm_small_high" ]]; then
	        BENCH="bash ${BENCH_PATH}/dp_ht_24c.sh small high motiv-cpu_dlrm_small_high"
	else
	        BENCH="bash ${BENCH_PATH}/dp_ht_24c.sh small high"
	fi
        echo 4G > ${CGMEM_DIR}/memory.max_at_node0
elif [[ "$2" == "cpu_dlrm_med_low" ]]; then
        BENCH_PATH="${PWD}"
	if [[ "$3" == "config5" ]]; then
	        BENCH="bash ${BENCH_PATH}/dp_ht_24c.sh med low config5"
	else
	        BENCH="bash ${BENCH_PATH}/dp_ht_24c.sh med low"
	fi
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
	if [[ "$3" == "config3" ]]; then
	        BENCH="bash ${BENCH_PATH}/dp_ht_24c.sh large low config3"
	elif [[ "$3" == "config4" ]]; then
	        BENCH="bash ${BENCH_PATH}/dp_ht_24c.sh large low config4"
	else
	        BENCH="bash ${BENCH_PATH}/dp_ht_24c.sh large low"
	fi
        echo 20G > ${CGMEM_DIR}/memory.max_at_node0
elif [[ "$2" == "cpu_dlrm_large_mid" ]]; then
        BENCH_PATH="${PWD}"
        BENCH="bash ${BENCH_PATH}/dp_ht_24c.sh large mid"
        echo 20G > ${CGMEM_DIR}/memory.max_at_node0
elif [[ "$2" == "cpu_dlrm_large_high" ]]; then
        BENCH_PATH="${PWD}"
        BENCH="bash ${BENCH_PATH}/dp_ht_24c.sh large high"
        echo 20G > ${CGMEM_DIR}/memory.max_at_node0
#source shrc at SPECCPU_2017 before run SPECCPU
elif [[ "$2" == "bwaves" ]]; then
        BENCH="runcpu --config=mttm_1 --noreportable --iteration=1 603.bwaves_s"
        echo 20G > ${CGMEM_DIR}/memory.max_at_node0
elif [[ "$2" == "fotonik" ]]; then
	if [[ "$3" == "config6" ]]; then
	        BENCH="runcpu --config=mttm_1 --noreportable --iteration=2 649.fotonik3d_s"
	elif [[ "$3" == "config10" ]]; then
	        BENCH="runcpu --config=mttm_1 --noreportable --iteration=2 649.fotonik3d_s"
	elif [[ "$3" == "6tenants" ]]; then
	        BENCH="runcpu --config=mttm_2 --noreportable --iteration=1 649.fotonik3d_s"
	elif [[ "$3" == "12tenants" ]]; then
	        BENCH="runcpu --config=mttm_3 --noreportable --iteration=1 649.fotonik3d_s"
	else
	        BENCH="runcpu --config=mttm_1 --noreportable --iteration=1 649.fotonik3d_s"
	fi
        echo 8373M > ${CGMEM_DIR}/memory.max_at_node0
elif [[ "$2" == "roms" ]]; then
	if [[ "$3" == "config4" ]]; then
	        BENCH="runcpu --config=mttm_1 --noreportable --iteration=2 654.roms_s"
	elif [[ "$3" == "6tenants" ]]; then
	        BENCH="runcpu --config=mttm_1 --noreportable --iteration=1 654.roms_s"
	else
	        BENCH="runcpu --config=mttm_1 --noreportable --iteration=1 654.roms_s"
	fi
        echo 2G > ${CGMEM_DIR}/memory.max_at_node0
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
        BENCH="${BENCH_PATH}/gups 8 2000000000 35 8 33 90"
	echo 80G > ${CGMEM_DIR}/memory.max_at_node0
elif [[ "$2" == "gups_store" ]]; then
        BENCH_PATH="${BENCH_DIR}/../microbenchmarks"
        BENCH="${BENCH_PATH}/gups-store 8 4000000000 35 8 33"
	echo 16G > ${CGMEM_DIR}/memory.max_at_node0
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
