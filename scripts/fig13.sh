#!/bin/bash

mttm_dat=$(./extract.sh mix3 ./evaluation/fig11/local/mix3.txt)
btree_local=$(echo "$mttm_dat" | grep Took | head -n 1 | cut -d':' -f2 | xargs)
xindex_local=$(echo "$mttm_dat" | grep ycsb | cut -d':' -f2 | xargs)
dlrm_local=$(echo "$mttm_dat" | grep 'Throughput:' | cut -d':' -f2 | cut -d'f' -f1 | xargs)

mttm_dat=$(./extract.sh mix3 ./evaluation/fig11/mttm/mix3/45G_220.txt)
btree_mttm=$(echo "$mttm_dat" | grep Took | head -n 1 | cut -d':' -f2 | xargs)
xindex_mttm=$(echo "$mttm_dat" | grep ycsb | cut -d':' -f2 | xargs)
dlrm_mttm=$(echo "$mttm_dat" | grep 'Throughput:' | cut -d':' -f2 | cut -d'f' -f1 | xargs)

mttm_dat=$(./extract.sh mix3 ./evaluation/fig11/static/mix3/45G_220.txt)
btree_static=$(echo "$mttm_dat" | grep Took | head -n 1 | cut -d':' -f2 | xargs)
xindex_static=$(echo "$mttm_dat" | grep ycsb | cut -d':' -f2 | xargs)
dlrm_static=$(echo "$mttm_dat" | grep 'Throughput:' | cut -d':' -f2 | cut -d'f' -f1 | xargs)

mttm_dat=$(./extract.sh mix3 ./evaluation/fig11/memstrata/mix3/45G_220.txt)
btree_memstrata=$(echo "$mttm_dat" | grep Took | head -n 1 | cut -d':' -f2 | xargs)
xindex_memstrata=$(echo "$mttm_dat" | grep ycsb | cut -d':' -f2 | xargs)
dlrm_memstrata=$(echo "$mttm_dat" | grep 'Throughput:' | cut -d':' -f2 | cut -d'f' -f1 | xargs)




echo "system	btree	xindex	dlrm" > fig13.dat
echo "local	$btree_local	$xindex_local	$dlrm_local" >> fig13.dat
echo "mttm	$btree_mttm	$xindex_mttm	$dlrm_mttm" >> fig13.dat
echo "static	$btree_static	$xindex_static	$dlrm_static" >> fig13.dat
echo "memstrata	$btree_memstrata	$xindex_memstrata	$dlrm_memstrata" >> fig13.dat



