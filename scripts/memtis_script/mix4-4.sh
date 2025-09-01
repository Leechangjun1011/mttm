#!/bin/bash

BENCH1_RUN=$(${MTTM_DIR}/bench_cmd.sh gapbs-pr mix4)
BENCH2_RUN=$(${MTTM_DIR}/bench_cmd.sh fotonik mix4)
BENCH3_RUN=$(${MTTM_DIR}/bench_cmd.sh silo mix4)

BENCH_DRAM="20G"


export BENCH1_RUN
export BENCH2_RUN
export BENCH3_RUN
export BENCH_DRAM
