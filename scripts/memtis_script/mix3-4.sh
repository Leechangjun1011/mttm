#!/bin/bash

BENCH1_RUN=$(${MTTM_DIR}/bench_cmd.sh xindex mix3)
BENCH2_RUN=$(${MTTM_DIR}/bench_cmd.sh btree mix3)
BENCH3_RUN=$(${MTTM_DIR}/bench_cmd.sh cpu_dlrm_small_low mix3)
BENCH_DRAM="18G"


export BENCH1_RUN
export BENCH2_RUN
export BENCH3_RUN
export BENCH_DRAM
