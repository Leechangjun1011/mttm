#!/bin/bash


BENCH1_RUN=$(${MTTM_DIR}/bench_cmd.sh gapbs-bc mix1)
BENCH2_RUN=$(${MTTM_DIR}/bench_cmd.sh gapbs-pr mix1)
BENCH3_RUN=$(${MTTM_DIR}/bench_cmd.sh xsbench mix1)

BENCH_DRAM="21G"


export BENCH1_RUN
export BENCH2_RUN
export BENCH3_RUN
export BENCH_DRAM
