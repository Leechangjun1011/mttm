#!/bin/bash

mttm_dat=$(./extract.sh mix3 ./evaluation/fig11/local/mix3.txt)
btree_local=$(echo "$mttm_dat" | grep Took | head -n 1 | cut -d':' -f2 | xargs)
xindex_local=$(echo "$mttm_dat" | grep ycsb | cut -d':' -f2 | xargs)
dlrm_local=$(echo "$mttm_dat" | grep 'Throughput:' | cut -d':' -f2 | cut -d'f' -f1 | xargs)

mttm_dat=$(./extract.sh mix3 ./evaluation/fig11/mttm/mix3/18G_220.txt)
btree_mttm=$(echo "$mttm_dat" | grep Took | head -n 1 | cut -d':' -f2 | xargs)
xindex_mttm=$(echo "$mttm_dat" | grep ycsb | cut -d':' -f2 | xargs)
dlrm_mttm=$(echo "$mttm_dat" | grep 'Throughput:' | cut -d':' -f2 | cut -d'f' -f1 | xargs)

mttm_dat=$(./extract.sh mix3 ./evaluation/fig11/tpp/mix3/18G_220.txt)
btree_tpp=$(echo "$mttm_dat" | grep Took | head -n 1 | cut -d':' -f2 | xargs)
xindex_tpp=$(echo "$mttm_dat" | grep ycsb | cut -d':' -f2 | xargs)
dlrm_tpp=$(echo "$mttm_dat" | grep 'Throughput:' | cut -d':' -f2 | cut -d'f' -f1 | xargs)

mttm_dat=$(./extract.sh mix3 ./evaluation/fig11/colloid/mix3/18G_220.txt)
btree_colloid=$(echo "$mttm_dat" | grep Took | head -n 1 | cut -d':' -f2 | xargs)
xindex_colloid=$(echo "$mttm_dat" | grep ycsb | cut -d':' -f2 | xargs)
dlrm_colloid=$(echo "$mttm_dat" | grep 'Throughput:' | cut -d':' -f2 | cut -d'f' -f1 | xargs)




echo "system	btree	xindex	dlrm" > fig14.dat
echo "local	$btree_local	$xindex_local	$dlrm_local" >> fig14.dat
echo "mttm	$btree_mttm	$xindex_mttm	$dlrm_mttm" >> fig14.dat
echo "tpp	$btree_tpp	$xindex_tpp	$dlrm_tpp" >> fig14.dat
echo "colloid	$btree_colloid	$xindex_colloid	$dlrm_colloid" >> fig14.dat



