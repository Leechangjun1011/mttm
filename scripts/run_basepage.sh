#!/bin/bash
: << END
echo 10007 > /proc/sys/vm/pebs_sample_period
./run_multi_tenants.sh 1 $1

echo 4999 > /proc/sys/vm/pebs_sample_period
./run_multi_tenants.sh 1 $1

echo 1999 > /proc/sys/vm/pebs_sample_period
./run_multi_tenants.sh 1 $1
END

echo 997 > /proc/sys/vm/pebs_sample_period
./run_multi_tenants.sh 1 $1

: << END
echo 499 > /proc/sys/vm/pebs_sample_period
./run_multi_tenants.sh 1 $1

echo 199 > /proc/sys/vm/pebs_sample_period
./run_multi_tenants.sh 1 $1
END

