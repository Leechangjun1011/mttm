#!/bin/bash
BENCH1_RUN=$(${MTTM_DIR}/bench_cmd.sh xindex mix2)
BENCH2_RUN=$(${MTTM_DIR}/bench_cmd.sh fotonik mix2)
BENCH3_RUN=$(${MTTM_DIR}/bench_cmd.sh cpu_dlrm_small_high mix2)
BENCH_DRAM="13G"


export BENCH1_RUN
export BENCH2_RUN
export BENCH3_RUN
export BENCH_DRAM
