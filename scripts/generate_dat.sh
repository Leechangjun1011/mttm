#!/bin/bash
MTTM_SCRIPT=path/to/mttm/script
./bin/ycsb load basic -P workloads/workloada -P ${MTTM_SCRIPT}/mttm_ycsb_conf.dat > ${MTTM_SCRIPT}/xindex_load.dat
./bin/ycsb run basic -P workloads/workloada -P ${MTTM_SCRIPT}/mttm_ycsb_conf.dat > ${MTTM_SCRIPT}/xindex_transaction.dat
