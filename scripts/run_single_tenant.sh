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
#echo 1 > ${CGMEM_DIR}/memory.qos_wss

CGCPU_DIR=/sys/fs/cgroup/cpuset/mttm_$1
cgdelete -g cpuset:mttm_$1
cgcreate -g cpuset:mttm_$1

if [[ "$3" == "6tenants" ]]; then
	if [ $1 -lt 3 ]; then
		CPUSETS="0-3"
	elif [ $1 -lt 5 ]; then
		CPUSETS="4-7"
	elif [ $1 -lt 7 ]; then
		CPUSETS="8-11"
	else
	        CPUSETS="12-15"
	fi
elif [[ "$3" == "12tenants" ]]; then
	if [ $1 -lt 3 ]; then
		CPUSETS="0-1"
	elif [ $1 -lt 5 ]; then
		CPUSETS="2-3"
	elif [ $1 -lt 7 ]; then
		CPUSETS="4-5"
	elif [ $1 -lt 9 ]; then
		CPUSETS="6-7"
	elif [ $1 -lt 11 ]; then
		CPUSETS="8-9"
	elif [ $1 -lt 13 ]; then
		CPUSETS="10-11"
	else
		CPUSETS="12-13"
	fi
        if [[ "$2" == "cpu_dlrm_small_low_1" ]]; then
                CPUSETS="0-23"
        elif [[ "$2" == "cpu_dlrm_small_low_2" ]]; then
                CPUSETS="0-23"
	fi
else
        CPUSETS="0-7"
        if [[ "$2" == "cpu_dlrm_small_low_1" ]]; then
                CPUSETS="0-23"
        elif [[ "$2" == "cpu_dlrm_large_low_2" ]]; then
                CPUSETS="0-23"
	fi
fi

echo ${CPUSETS} > ${CGCPU_DIR}/cpuset.cpus
echo 0-1 > ${CGCPU_DIR}/cpuset.mems
echo $$ > ${CGCPU_DIR}/cgroup.procs

if [[ "$2" == "gapbs-bc" ]]; then
        BENCH_PATH="${BENCH_DIR}/gapbs"
	if [[ "$3" == "config1" ]]; then
	        BENCH="${BENCH_PATH}/bc -f ${BENCH_PATH}/pregen_g28.sg -n 16"
	elif [[ "$3" == "config1-static1" ]]; then
	        BENCH="${BENCH_PATH}/bc -f ${BENCH_PATH}/pregen_g28.sg -n 16"
		echo 18G > ${CGMEM_DIR}/memory.max_at_node0
	elif [[ "$3" == "config1-static4" ]]; then
	        BENCH="${BENCH_PATH}/bc -f ${BENCH_PATH}/pregen_g28.sg -n 16"
		echo 7G > ${CGMEM_DIR}/memory.max_at_node0
	elif [[ "$3" == "config1-bw1" ]]; then
	        BENCH="${BENCH_PATH}/bc -f ${BENCH_PATH}/pregen_g28.sg -n 16"
		echo 200G > ${CGMEM_DIR}/memory.max_at_node0
	elif [[ "$3" == "config1-bw2" ]]; then
	        BENCH="${BENCH_PATH}/bc -f ${BENCH_PATH}/pregen_g28.sg -n 16"
		echo 6G > ${CGMEM_DIR}/memory.max_at_node0
	elif [[ "$3" == "6tenants" ]]; then
	        BENCH="${BENCH_PATH}/bc -f ${BENCH_PATH}/pregen_g26.sg -n 28"
	elif [[ "$3" == "12tenants" ]]; then
	        BENCH="${BENCH_PATH}/bc -f ${BENCH_PATH}/pregen_g26.sg -n 26"
	else
	        BENCH="${BENCH_PATH}/bc -f ${BENCH_PATH}/pregen_g28.sg -n 8"
	fi
elif [[ "$2" == "gapbs-pr" ]]; then
        BENCH_PATH="${BENCH_DIR}/gapbs"
	if [[ "$3" == "config1" ]]; then
	        BENCH="${BENCH_PATH}/pr -f ${BENCH_PATH}/pregen_g28.sg -i 1000 -t 1e-4 -n 11"
	elif [[ "$3" == "config1-static1" ]]; then
	        BENCH="${BENCH_PATH}/pr -f ${BENCH_PATH}/pregen_g28.sg -i 1000 -t 1e-4 -n 11"
		echo 18G > ${CGMEM_DIR}/memory.max_at_node0
	elif [[ "$3" == "config1-static4" ]]; then
	        BENCH="${BENCH_PATH}/pr -f ${BENCH_PATH}/pregen_g28.sg -i 1000 -t 1e-4 -n 11"
		echo 7G > ${CGMEM_DIR}/memory.max_at_node0
	elif [[ "$3" == "config1-bw1" ]]; then
	        BENCH="${BENCH_PATH}/pr -f ${BENCH_PATH}/pregen_g28.sg -i 1000 -t 1e-4 -n 11"
		echo 200G > ${CGMEM_DIR}/memory.max_at_node0
	elif [[ "$3" == "config1-bw2" ]]; then
	        BENCH="${BENCH_PATH}/pr -f ${BENCH_PATH}/pregen_g28.sg -i 1000 -t 1e-4 -n 11"
		echo 4G > ${CGMEM_DIR}/memory.max_at_node0
	elif [[ "$3" == "config13" ]]; then
	        BENCH="${BENCH_PATH}/pr -f ${BENCH_PATH}/pregen_g28.sg -i 1000 -t 1e-4 -n 11"
	elif [[ "$3" == "config13-static1" ]]; then
	        BENCH="${BENCH_PATH}/pr -f ${BENCH_PATH}/pregen_g28.sg -i 1000 -t 1e-4 -n 11"
		echo 17G > ${CGMEM_DIR}/memory.max_at_node0
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
	elif [[ "$3" == "config13-static4" ]]; then
	        BENCH="${BENCH_PATH}/pr -f ${BENCH_PATH}/pregen_g28.sg -i 1000 -t 1e-4 -n 11"
		echo 6826M > ${CGMEM_DIR}/memory.max_at_node0
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
elif [[ "$2" == "gapbs-cc_sv" ]]; then
        BENCH_PATH="${BENCH_DIR}/gapbs"
        BENCH="${BENCH_PATH}/cc_sv -f ${BENCH_PATH}/pregen_g28.sg -n 10"
elif [[ "$2" == "gapbs-tc" ]]; then
        BENCH_PATH="${BENCH_DIR}/gapbs"
	BENCH="${BENCH_PATH}/tc -f ${BENCH_PATH}/pregen_g27.sg -n 1"
elif [[ "$2" == "graph500" ]]; then
        BENCH_PATH="${BENCH_DIR}/graph500/omp-csr"
        BENCH="${BENCH_PATH}/omp-csr -s 26 -e 15 -V" #s27 e 15, options.h NBFS_max is number of BFS
elif [[ "$2" == "xsbench" ]]; then
        BENCH_PATH="${BENCH_DIR}/XSBench/openmp-threading"
	if [[ "$3" == "config1" ]]; then
	        BENCH="${BENCH_PATH}/XSBench -t 8 -g 70000 -p 30000000"
	elif [[ "$3" == "config1-static1" ]]; then
	        BENCH="${BENCH_PATH}/XSBench -t 8 -g 70000 -p 30000000"
		echo 18G > ${CGMEM_DIR}/memory.max_at_node0
	elif [[ "$3" == "config1-static4" ]]; then
	        BENCH="${BENCH_PATH}/XSBench -t 8 -g 70000 -p 30000000"
		echo 7G > ${CGMEM_DIR}/memory.max_at_node0
	elif [[ "$3" == "config1-bw1" ]]; then
	        BENCH="${BENCH_PATH}/XSBench -t 8 -g 70000 -p 30000000"
		echo 200G > ${CGMEM_DIR}/memory.max_at_node0
	elif [[ "$3" == "config1-bw2" ]]; then
	        BENCH="${BENCH_PATH}/XSBench -t 8 -g 70000 -p 30000000"
		echo 3G > ${CGMEM_DIR}/memory.max_at_node0
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
elif [[ "$2" == "xindex" ]]; then
        BENCH_PATH="${BENCH_DIR}/XIndex-H"
	if [[ "$3" == "config2" ]]; then
		BENCH="${BENCH_PATH}/build/ycsb_bench --fg 6 --iteration 25"
	elif [[ "$3" == "config2-static1" ]]; then
		BENCH="${BENCH_PATH}/build/ycsb_bench --fg 6 --iteration 25"
		echo 11605M > ${CGMEM_DIR}/memory.max_at_node0
	elif [[ "$3" == "config2-static4" ]]; then
		BENCH="${BENCH_PATH}/build/ycsb_bench --fg 6 --iteration 25"
		echo 4437M > ${CGMEM_DIR}/memory.max_at_node0
	elif [[ "$3" == "config2-bw1" ]]; then
		BENCH="${BENCH_PATH}/build/ycsb_bench --fg 6 --iteration 25"
		echo 80G > ${CGMEM_DIR}/memory.max_at_node0
	elif [[ "$3" == "config2-bw2" ]]; then
		BENCH="${BENCH_PATH}/build/ycsb_bench --fg 6 --iteration 25"
		echo 11605M > ${CGMEM_DIR}/memory.max_at_node0
	elif [[ "$3" == "config12" ]]; then
		BENCH="${BENCH_PATH}/build/ycsb_bench --fg 6 --iteration 22"
	elif [[ "$3" == "config12-static1" ]]; then
		BENCH="${BENCH_PATH}/build/ycsb_bench --fg 6 --iteration 22"
		echo 15G > ${CGMEM_DIR}/memory.max_at_node0
	elif [[ "$3" == "config12-static4" ]]; then
		BENCH="${BENCH_PATH}/build/ycsb_bench --fg 6 --iteration 22"
		echo 6G > ${CGMEM_DIR}/memory.max_at_node0
	elif [[ "$3" == "motiv-xindex" ]]; then
		BENCH="${BENCH_PATH}/build/ycsb_bench --fg 6 --iteration 20"
	else
		BENCH="${BENCH_PATH}/build/ycsb_bench --fg 6 --iteration 30"
	fi
elif [[ "$2" == "xindex_tiny" ]]; then
        BENCH_PATH="${BENCH_DIR}/XIndex-H"
	if [[ "$3" == "6tenants" ]]; then
		BENCH="${BENCH_PATH}/build/ycsb_bench_tiny --fg 2 --iteration 15"
	elif [[ "$3" == "12tenants" ]]; then
		BENCH="${BENCH_PATH}/build/ycsb_bench_tiny --fg 1 --iteration 15"
	else
		BENCH="${BENCH_PATH}/build/ycsb_bench_tiny --fg 2 --iteration 10"
	fi
elif [[ "$2" == "xindex_large" ]]; then
        BENCH_PATH="${BENCH_DIR}/XIndex-H"
	if [[ "$3" == "config12" ]]; then
		BENCH="${BENCH_PATH}/build/ycsb_bench_large --fg 6 --iteration 20"
	else
		BENCH="${BENCH_PATH}/build/ycsb_bench_large --fg 6 --iteration 20"
	fi
elif [[ "$2" == "btree" ]]; then
        BENCH_PATH="${BENCH_DIR}/../../vmitosis-workloads/bin"
        BENCH="${BENCH_PATH}/bench_btree_mt"
	if [[ "$3" == "config12-static1" ]]; then
		echo 15G > ${CGMEM_DIR}/memory.max_at_node0
	elif [[ "$3" == "config12-static4" ]]; then
		echo 6G > ${CGMEM_DIR}/memory.max_at_node0
	fi
elif [[ "$2" == "silo" ]]; then
        BENCH_PATH="${BENCH_DIR}/silo"	
	if [[ "$3" == "config3" ]]; then
	        BENCH="${BENCH_PATH}/out-perf.masstree/benchmarks/dbtest --verbose --bench ycsb --num-threads 8 --scale-factor 400000 --ops-per-worker=480000000"
	elif [[ "$3" == "config13" ]]; then
	        BENCH="${BENCH_PATH}/out-perf.masstree/benchmarks/dbtest --verbose --bench ycsb --num-threads 8 --scale-factor 400000 --ops-per-worker=450000000"
	elif [[ "$3" == "config13-static1" ]]; then
	        BENCH="${BENCH_PATH}/out-perf.masstree/benchmarks/dbtest --verbose --bench ycsb --num-threads 8 --scale-factor 400000 --ops-per-worker=450000000"
		echo 17G > ${CGMEM_DIR}/memory.max_at_node0
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
	elif [[ "$3" == "config13-static4" ]]; then
	        BENCH="${BENCH_PATH}/out-perf.masstree/benchmarks/dbtest --verbose --bench ycsb --num-threads 8 --scale-factor 400000 --ops-per-worker=450000000"
		echo 6826M > ${CGMEM_DIR}/memory.max_at_node0
	elif [[ "$3" == "6tenants" ]]; then
	        BENCH="${BENCH_PATH}/out-perf.masstree/benchmarks/dbtest --verbose --bench ycsb --num-threads 4 --scale-factor 80000 --ops-per-worker=450000000"
	elif [[ "$3" == "12tenants" ]]; then
		BENCH="${BENCH_PATH}/out-perf.masstree/benchmarks/dbtest --verbose --bench ycsb --num-threads 2 --scale-factor 80000 --ops-per-worker=900000000"
	else
		BENCH="${BENCH_PATH}/out-perf.masstree/benchmarks/dbtest --verbose --bench ycsb --num-threads 8 --scale-factor 240000 --ops-per-worker=450000000"
	fi
elif [[ "$2" == "cpu_dlrm_small_low" ]]; then
        BENCH_PATH="${PWD}"
	if [[ "$3" == "config12" ]]; then
	        BENCH="bash ${BENCH_PATH}/dp_ht_24c.sh small low config12"
	elif [[ "$3" == "config12-static1" ]]; then
	        BENCH="bash ${BENCH_PATH}/dp_ht_24c.sh small low config12"
		echo 15G > ${CGMEM_DIR}/memory.max_at_node0
	elif [[ "$3" == "config12-static4" ]]; then
	        BENCH="bash ${BENCH_PATH}/dp_ht_24c.sh small low config12"
		echo 6G > ${CGMEM_DIR}/memory.max_at_node0
	elif [[ "$3" == "6tenants" ]]; then
	        BENCH="bash ${BENCH_PATH}/dp_ht_24c.sh small low 6tenants"
	elif [[ "$3" == "motiv" ]]; then
	        BENCH="bash ${BENCH_PATH}/dp_ht_24c.sh small low motiv"
	else
	        BENCH="bash ${BENCH_PATH}/dp_ht_24c.sh small low"
	fi
elif [[ "$2" == "cpu_dlrm_small_low_1" ]]; then
        BENCH_PATH="${PWD}"
	if [[ "$3" == "config3" ]]; then
		BENCH="bash ${BENCH_PATH}/dp_ht_2c_1.sh small low config3"
	else
		BENCH="bash ${BENCH_PATH}/dp_ht_2c_1.sh small low 12tenants"
	fi
elif [[ "$2" == "cpu_dlrm_small_low_2" ]]; then
        BENCH_PATH="${PWD}"
	BENCH="bash ${BENCH_PATH}/dp_ht_2c_2.sh small low 12tenants"
elif [[ "$2" == "cpu_dlrm_small_mid" ]]; then
        BENCH_PATH="${PWD}"
        BENCH="bash ${BENCH_PATH}/dp_ht_24c.sh small mid"
elif [[ "$2" == "cpu_dlrm_small_high" ]]; then
        BENCH_PATH="${PWD}"
	if [[ "$3" == "config2" ]]; then
	        BENCH="bash ${BENCH_PATH}/dp_ht_24c.sh small high config2"
	elif [[ "$3" == "config2-static1" ]]; then
	        BENCH="bash ${BENCH_PATH}/dp_ht_24c.sh small high config2"
		echo 11605M > ${CGMEM_DIR}/memory.max_at_node0
	elif [[ "$3" == "config2-static4" ]]; then
	        BENCH="bash ${BENCH_PATH}/dp_ht_24c.sh small high config2"
		echo 4437M > ${CGMEM_DIR}/memory.max_at_node0
	elif [[ "$3" == "config2-bw1" ]]; then
	        BENCH="bash ${BENCH_PATH}/dp_ht_24c.sh small high config2"
		echo 80G > ${CGMEM_DIR}/memory.max_at_node0
	elif [[ "$3" == "config2-bw2" ]]; then
	        BENCH="bash ${BENCH_PATH}/dp_ht_24c.sh small high config2"
		echo 11605M > ${CGMEM_DIR}/memory.max_at_node0
	elif [[ "$3" == "config8" ]]; then
	        BENCH="bash ${BENCH_PATH}/dp_ht_24c.sh small high config8"
	elif [[ "$3" == "motiv-cpu_dlrm_small_high" ]]; then
	        BENCH="bash ${BENCH_PATH}/dp_ht_24c.sh small high motiv-cpu_dlrm_small_high"
	else
	        BENCH="bash ${BENCH_PATH}/dp_ht_24c.sh small high"
	fi
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
	if [[ "$3" == "config4" ]]; then
	        BENCH="bash ${BENCH_PATH}/dp_ht_24c.sh large low config4"
	elif [[ "$3" == "config5" ]]; then
	        BENCH="bash ${BENCH_PATH}/dp_ht_24c.sh large low config5"
	else
	        BENCH="bash ${BENCH_PATH}/dp_ht_24c.sh large low"
	fi
elif [[ "$2" == "cpu_dlrm_large_low_2" ]]; then
        BENCH_PATH="${PWD}"
	BENCH="bash ${BENCH_PATH}/dp_ht_2c_2.sh large low config3"
elif [[ "$2" == "cpu_dlrm_large_mid" ]]; then
        BENCH_PATH="${PWD}"
        BENCH="bash ${BENCH_PATH}/dp_ht_24c.sh large mid"
elif [[ "$2" == "cpu_dlrm_large_high" ]]; then
        BENCH_PATH="${PWD}"
        BENCH="bash ${BENCH_PATH}/dp_ht_24c.sh large high"
#source shrc at SPECCPU_2017 before run SPECCPU
elif [[ "$2" == "bwaves" ]]; then
        BENCH="runcpu --config=mttm_1 --noreportable --iteration=1 603.bwaves_s"
elif [[ "$2" == "fotonik" ]]; then
	cur_path=$PWD
	cd ${BENCH_DIR}/SPECCPU_2017
	source shrc
	cd ${cur_path}
	if [[ "$3" == "config2" ]]; then
	        BENCH="runcpu --config=mttm_1 --noreportable --iteration=2 649.fotonik3d_s"
	elif [[ "$3" == "config2-static1" ]]; then
	        BENCH="runcpu --config=mttm_1 --noreportable --iteration=2 649.fotonik3d_s"
		echo 11605M > ${CGMEM_DIR}/memory.max_at_node0
	elif [[ "$3" == "config2-static4" ]]; then
	        BENCH="runcpu --config=mttm_1 --noreportable --iteration=2 649.fotonik3d_s"
		echo 4437M > ${CGMEM_DIR}/memory.max_at_node0
	elif [[ "$3" == "config2-bw1" ]]; then
	        BENCH="runcpu --config=mttm_1 --noreportable --iteration=2 649.fotonik3d_s"
		echo 80G > ${CGMEM_DIR}/memory.max_at_node0
	elif [[ "$3" == "config2-bw2" ]]; then
	        BENCH="runcpu --config=mttm_1 --noreportable --iteration=2 649.fotonik3d_s"
		echo 7G > ${CGMEM_DIR}/memory.max_at_node0
	elif [[ "$3" == "config13" ]]; then
	        BENCH="runcpu --config=mttm_1 --noreportable --iteration=2 649.fotonik3d_s"
	elif [[ "$3" == "config13-static1" ]]; then
	        BENCH="runcpu --config=mttm_1 --noreportable --iteration=2 649.fotonik3d_s"
		echo 17G > ${CGMEM_DIR}/memory.max_at_node0
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
	elif [[ "$3" == "config13-static4" ]]; then
	        BENCH="runcpu --config=mttm_1 --noreportable --iteration=2 649.fotonik3d_s"
		echo 6826M > ${CGMEM_DIR}/memory.max_at_node0
	elif [[ "$3" == "6tenants" ]]; then
	        BENCH="runcpu --config=mttm_2 --noreportable --iteration=1 649.fotonik3d_s"
	elif [[ "$3" == "12tenants" ]]; then
	        BENCH="runcpu --config=mttm_3 --noreportable --iteration=1 649.fotonik3d_s"
	else
	        BENCH="runcpu --config=mttm_1 --noreportable --iteration=2 649.fotonik3d_s"
	fi
elif [[ "$2" == "roms" ]]; then
	if [[ "$3" == "config4" ]]; then
	        BENCH="runcpu --config=mttm_1 --noreportable --iteration=2 654.roms_s"
	elif [[ "$3" == "config5" ]]; then
	        BENCH="runcpu --config=mttm_1 --noreportable --iteration=1 654.roms_s"
	elif [[ "$3" == "6tenants" ]]; then
	        BENCH="runcpu --config=mttm_1 --noreportable --iteration=1 654.roms_s"
	else
	        BENCH="runcpu --config=mttm_1 --noreportable --iteration=1 654.roms_s"
	fi
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
