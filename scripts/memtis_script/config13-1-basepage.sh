#!/bin/bash

######## changes the below path
BIN=/home/cjlee/CXL-emulation.code/workloads

BENCH1_RUN="${BIN}/gapbs/pr -f ${BIN}/gapbs/pregen_g28.sg -i 1000 -t 1e-4 -n 20"
BENCH2_RUN="runcpu --config=mttm_1 --noreportable --iteration=6 649.fotonik3d_s"
BENCH3_RUN="${BIN}/silo/out-perf.masstree/benchmarks/dbtest --verbose --bench ycsb --num-threads 8 --scale-factor 400000 --ops-per-worker=1000000000"

BENCH_DRAM="51G"

cd ${BIN}/SPECCPU_2017
source shrc

export BENCH1_RUN
export BENCH2_RUN
export BENCH3_RUN
export BENCH_DRAM
