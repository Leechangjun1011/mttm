#!/bin/bash

# 1st input: workload
# 2nd input: mix number

GUPS_DIR="$PWD/gups"

if [[ "$1" == "gapbs-bc" ]]; then
        if [[ "$2" == "mix1" ]]; then
                BENCH="${GAPBS_DIR}/bc -f ${GAPBS_DIR}/pregen_g28.sg -n 16"
        fi
elif [[ "$1" == "gapbs-pr" ]]; then
        if [[ "$2" == "mix1" || "$2" == "mix4" ]]; then
                BENCH="${GAPBS_DIR}/pr -f ${GAPBS_DIR}/pregen_g28.sg -i 1000 -t 1e-4 -n 11"
        elif [[ "$2" == "mix4-basepage" ]]; then
                BENCH="${GAPBS_DIR}/pr -f ${GAPBS_DIR}/pregen_g28.sg -i 1000 -t 1e-4 -n 20"
        fi
elif [[ "$1" == "xsbench" ]]; then
        if [[ "$2" == "mix1" ]]; then
                BENCH="${XSBENCH_DIR}/XSBench -t 8 -g 70000 -p 30000000"
        fi
elif [[ "$1" == "xindex" ]]; then
        if [[ "$2" == "mix2" ]]; then
                BENCH="${XINDEX_DIR}/build/ycsb_bench --fg 6 --iteration 25"
        elif [[ "$2" == "mix3" ]]; then
                BENCH="${XINDEX_DIR}/build/ycsb_bench --fg 6 --iteration 22"
        fi
elif [[ "$1" == "btree" ]]; then
        BENCH="${BTREE_DIR}/bench_btree_mt"
elif [[ "$1" == "silo" ]]; then
        if [[ "$2" == "mix4" ]]; then
                BENCH="${SILO_DIR}/out-perf.masstree/benchmarks/dbtest --verbose --bench ycsb --num-threads 8 --scale-factor 400000 --ops-per-worker=450000000"
        elif [[ "$2" == "mix4-basepage" ]]; then
                BENCH="${SILO_DIR}/out-perf.masstree/benchmarks/dbtest --verbose --bench ycsb --num-threads 8 --scale-factor 400000 --ops-per-worker=1000000000"
        fi
elif [[ "$1" == "cpu_dlrm_small_low" ]]; then
        if [[ "$2" == "mix3" ]]; then
                BENCH="bash ${CPU_DLRM_DIR}/dp_ht_8c.sh small low mix3"
        fi
elif [[ "$1" == "cpu_dlrm_small_high" ]]; then
        if [[ "$2" == "mix2" ]]; then
                BENCH="bash ${CPU_DLRM_DIR}/dp_ht_8c.sh small high mix2"
        fi
elif [[ "$1" == "fotonik" ]]; then
        if [[ "$2" == "mix2"  || "$2" == "mix4" ]]; then
                BENCH="runcpu --config=mttm_1 --noreportable --iteration=2 649.fotonik3d_s"
        elif [[ "$2" == "mix4-basepage" ]]; then
                BENCH="runcpu --config=mttm_1 --noreportable --iteration=6 649.fotonik3d_s"
        fi
elif [[ "$1" == "gups-1" ]]; then
        if [[ "$2" == "microbench" || "$2" == "microbench-dynamic" ]]; then
                BENCH="${GUPS_DIR}/gups-1 8 4000000000 34 8 31 90"
        elif [[ "$2" == "microbench-sensitivity1" ]]; then
                BENCH="${GUPS_DIR}/gups-1 8 4000000000 34 8 31 90"
        elif [[ "$2" == "microbench-sensitivity2" ]]; then
                BENCH="${GUPS_DIR}/gups-1 2 4000000000 34 8 33 90"
        fi
elif [[ "$1" == "gups-2" ]]; then
        if [[ "$2" == "microbench" || "$2" == "microbench-dynamic" ]]; then
                BENCH="${GUPS_DIR}/gups-2 8 4000000000 34 8 32 90"
        elif [[ "$2" == "microbench-sensitivity1" ]]; then
                BENCH="${GUPS_DIR}/gups-2 8 4000000000 34 8 32 90"
        elif [[ "$2" == "microbench-sensitivity2" ]]; then
                BENCH="${GUPS_DIR}/gups-2 4 4000000000 34 8 33 90"
        fi
elif [[ "$1" == "gups-3" ]]; then
        if [[ "$2" == "microbench" ]]; then
                BENCH="${GUPS_DIR}/gups-3 8 4000000000 34 8 32 0"
        elif [[ "$2" == "microbench-dynamic" ]]; then
                sleep 50s
                BENCH="${GUPS_DIR}/gups-3 8 4000000000 34 8 32 0"
        elif [[ "$2" == "microbench-sensitivity1" ]]; then
                BENCH="${GUPS_DIR}/gups-3 8 4000000000 34 8 32 0"
        elif [[ "$2" == "microbench-sensitivity2" ]]; then
                BENCH="${GUPS_DIR}/gups-3 8 4000000000 34 8 33 90"
        fi
elif [[ "$1" == "gups-2g" ]]; then
        BENCH_PATH="${BENCH_DIR}/../microbenchmarks"
        BENCH="${BENCH_PATH}/gups1 2 3000000000 34 8 31 90"
elif [[ "$1" == "gups-2g-8t" ]]; then
        BENCH_PATH="${BENCH_DIR}/../microbenchmarks"
        BENCH="${BENCH_PATH}/gups1 8 3000000000 34 8 31 90"
elif [[ "$1" == "gups-4g" ]]; then
        BENCH_PATH="${BENCH_DIR}/../microbenchmarks"
        BENCH="${BENCH_PATH}/gups2 4 3000000000 34 8 32 90"
elif [[ "$1" == "gups-4g-8t" ]]; then
        BENCH_PATH="${BENCH_DIR}/../microbenchmarks"
        BENCH="${BENCH_PATH}/gups2 8 3000000000 34 8 32 90"
elif [[ "$1" == "gups-16g" ]]; then
        BENCH_PATH="${BENCH_DIR}/../microbenchmarks"
        BENCH="${BENCH_PATH}/gups3 8 3000000000 34 8 34 0"
elif [[ "$1" == "gups_small" ]]; then
        BENCH_PATH="${BENCH_DIR}/../microbenchmarks"
        BENCH="${BENCH_PATH}/gups 8 2000000000 34 8 32"
elif [[ "$1" == "gups_large" ]]; then
        BENCH_PATH="${BENCH_DIR}/../microbenchmarks"
        BENCH="${BENCH_PATH}/gups 8 2000000000 35 8 33 90"
elif [[ "$1" == "gups_store" ]]; then
        BENCH_PATH="${BENCH_DIR}/../microbenchmarks"
        BENCH="${BENCH_PATH}/gups-store 8 4000000000 35 8 33"
else
        echo "$1 benchmark is not supported"
        exit 0
fi


echo ${BENCH}

