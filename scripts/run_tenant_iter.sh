#!/bin/bash



./run_multi_tenants_iter.sh nas_bt.d 2G > bt_2g.txt
./run_multi_tenants_iter.sh nas_bt.d 4G > bt_4g.txt
./run_multi_tenants_iter.sh nas_bt.d 8G > bt_8g.txt
./run_multi_tenants_native.sh 1 nas_bt.d > bt_local.txt
./run_multi_tenants_native_remote.sh 1 nas_bt.d > bt_remote.txt

./run_multi_tenants_iter.sh nas_cg.d 2G > cg_2g.txt
./run_multi_tenants_iter.sh nas_cg.d 4G > cg_4g.txt
./run_multi_tenants_iter.sh nas_cg.d 8G > cg_8g.txt
./run_multi_tenants_iter.sh nas_cg.d 16G > cg_16g.txt
./run_multi_tenants_native.sh 1 nas_cg.d > cg_local.txt
./run_multi_tenants_native_remote.sh 1 nas_cg.d > cg_remote.txt

./run_multi_tenants_iter.sh nas_lu.d 2G > lu_2g.txt
./run_multi_tenants_iter.sh nas_lu.d 4G > lu_4g.txt
./run_multi_tenants_iter.sh nas_lu.d 8G > lu_8g.txt
./run_multi_tenants_native.sh 1 nas_lu.d > lu_local.txt
./run_multi_tenants_native_remote.sh 1 nas_lu.d > lu_remote.txt

./run_multi_tenants_iter.sh nas_mg.d 2G > mg_2g.txt
./run_multi_tenants_iter.sh nas_mg.d 4G > mg_4g.txt
./run_multi_tenants_iter.sh nas_mg.d 8G > mg_8g.txt
./run_multi_tenants_iter.sh nas_mg.d 16G > mg_16g.txt
./run_multi_tenants_native.sh 1 nas_mg.d > mg_local.txt
./run_multi_tenants_native_remote.sh 1 nas_mg.d > mg_remote.txt

./run_multi_tenants_iter.sh nas_sp.d 2G > sp_2g.txt
./run_multi_tenants_iter.sh nas_sp.d 4G > sp_4g.txt
./run_multi_tenants_iter.sh nas_sp.d 8G > sp_8g.txt
./run_multi_tenants_native.sh 1 nas_sp.d > sp_local.txt
./run_multi_tenants_native_remote.sh 1 nas_sp.d > sp_remote.txt

./run_multi_tenants_iter.sh cpu_dlrm_med_low 2G > dlrm_ml_2g.txt
./run_multi_tenants_iter.sh cpu_dlrm_med_low 4G > dlrm_ml_4g.txt
./run_multi_tenants_iter.sh cpu_dlrm_med_low 8G > dlrm_ml_8g.txt
./run_multi_tenants_iter.sh cpu_dlrm_med_low 16G > dlrm_ml_16g.txt
./run_multi_tenants_iter.sh cpu_dlrm_med_low 32G > dlrm_ml_32g.txt
./run_multi_tenants_native.sh 1 cpu_dlrm_med_low > dlrm_ml_local.txt
./run_multi_tenants_native_remote.sh 1 cpu_dlrm_med_low > dlrm_ml_remote.txt

./run_multi_tenants_iter.sh cpu_dlrm_med_high 2G > dlrm_mh_2g.txt
./run_multi_tenants_iter.sh cpu_dlrm_med_high 4G > dlrm_mh_4g.txt
./run_multi_tenants_iter.sh cpu_dlrm_med_high 8G > dlrm_mh_8g.txt
./run_multi_tenants_iter.sh cpu_dlrm_med_high 16G > dlrm_mh_16g.txt
./run_multi_tenants_iter.sh cpu_dlrm_med_high 32G > dlrm_mh_32g.txt
./run_multi_tenants_native.sh 1 cpu_dlrm_med_high > dlrm_mh_local.txt
./run_multi_tenants_native_remote.sh 1 cpu_dlrm_med_high > dlrm_mh_remote.txt



: << END

./run_multi_tenants_iter.sh gapbs-pr 2G > pr_2g.txt
./run_multi_tenants_native.sh 1 gapbs-pr > pr_remote.txt

./run_multi_tenants_iter.sh gapbs-bc 2G > bc_2g.txt
./run_multi_tenants_native.sh 1 gapbs-bc > bc_remote.txt

./run_multi_tenants_iter.sh gapbs-cc_sv 2G > cc_sv_2g.txt
./run_multi_tenants_native.sh 1 gapbs-cc_sv > cc_sv_remote.txt

./run_multi_tenants_iter.sh graph500 2G > graph500_2g.txt
./run_multi_tenants_native.sh 1 graph500 > graph500_remote.txt

./run_multi_tenants_iter.sh xsbench 2G > xsbench_2g.txt
./run_multi_tenants_native.sh 1 xsbench > xsbench_remote.txt

./run_multi_tenants_iter.sh xindex 2G > xindex_2g.txt
./run_multi_tenants_native.sh 1 xindex > xindex_remote.txt

./run_multi_tenants_iter.sh silo 2G
./run_multi_tenants_native.sh 1 silo

./run_multi_tenants_iter.sh cpu_dlrm_small_low 2G > dlrm_sl_2g.txt
./run_multi_tenants_native.sh 1 cpu_dlrm_small_low > dlrm_sl_remote.txt

./run_multi_tenants_iter.sh cpu_dlrm_small_high 2G > dlrm_sh_2g.txt
./run_multi_tenants_native.sh 1 cpu_dlrm_small_high > dlrm_sh_remote.txt

./run_multi_tenants_iter.sh cpu_dlrm_large_low 2G > dlrm_ll_2g.txt
./run_multi_tenants_native.sh 1 cpu_dlrm_large_low > dlrm_ll_remote.txt

./run_multi_tenants_iter.sh cpu_dlrm_large_high 2G > dlrm_lh_2g.txt
./run_multi_tenants_native.sh 1 cpu_dlrm_large_high > dlrm_lh_remote.txt

./run_multi_tenants_iter.sh fotonik 2G > fotonik_2g.txt
./run_multi_tenants_native.sh 1 fotonik > fotonik_remote.txt

./run_multi_tenants_iter.sh roms 2G > roms_2g.txt
./run_multi_tenants_native.sh 1 roms > roms_remote.txt


./run_multi_tenants_iter.sh gapbs-bc 8G > bc_8g.txt
./run_multi_tenants_iter.sh gapbs-bc 16G > bc_16g.txt
./run_multi_tenants_iter.sh gapbs-bc 32G > bc_32g.txt
./run_multi_tenants_native.sh 1 gapbs-bc > bc_local.txt

#./run_multi_tenants_iter.sh gapbs-cc_sv 4G > cc_sv_4g.txt
./run_multi_tenants_iter.sh gapbs-cc_sv 8G > cc_sv_8g.txt
./run_multi_tenants_iter.sh gapbs-cc_sv 16G > cc_sv_16g.txt
./run_multi_tenants_iter.sh gapbs-cc_sv 32G > cc_sv_32g.txt
./run_multi_tenants_native.sh 1 gapbs-cc_sv > cc_sv_local.txt

./run_multi_tenants_iter.sh graph500 4G > graph500_4g.txt
./run_multi_tenants_iter.sh graph500 8G > graph500_8g.txt
./run_multi_tenants_iter.sh graph500 16G > graph500_16g.txt
./run_multi_tenants_iter.sh graph500 32G > graph500_32g.txt
./run_multi_tenants_native.sh 1 graph500 > graph500_local.txt

./run_multi_tenants_iter.sh xsbench 4G > xsbench_4g.txt
./run_multi_tenants_iter.sh xsbench 8G > xsbench_8g.txt
./run_multi_tenants_iter.sh xsbench 16G > xsbench_16g.txt
./run_multi_tenants_iter.sh xsbench 32G > xsbench_32g.txt
./run_multi_tenants_native.sh 1 xsbench > xsbench_local.txt

./run_multi_tenants_iter.sh xindex 4G > xindex_4g.txt
./run_multi_tenants_iter.sh xindex 8G > xindex_8g.txt
./run_multi_tenants_iter.sh xindex 16G > xindex_16g.txt
./run_multi_tenants_iter.sh xindex 32G > xindex_32g.txt
./run_multi_tenants_native.sh 1 xindex > xindex_local.txt

./run_multi_tenants_iter.sh silo 4G
./run_multi_tenants_iter.sh silo 8G
./run_multi_tenants_iter.sh silo 16G
./run_multi_tenants_iter.sh silo 32G
./run_multi_tenants_native.sh 1 silo

#./run_multi_tenants_iter.sh cpu_dlrm_small_low 4G > dlrm_sl_4g.txt
./run_multi_tenants_iter.sh cpu_dlrm_small_low 8G > dlrm_sl_8g.txt
./run_multi_tenants_iter.sh cpu_dlrm_small_low 16G > dlrm_sl_16g.txt
./run_multi_tenants_iter.sh cpu_dlrm_small_low 32G > dlrm_sl_32g.txt
./run_multi_tenants_native.sh 1 cpu_dlrm_small_low > dlrm_sl_local.txt

./run_multi_tenants_iter.sh cpu_dlrm_small_high 4G > dlrm_sh_4g.txt
./run_multi_tenants_iter.sh cpu_dlrm_small_high 8G > dlrm_sh_8g.txt
./run_multi_tenants_iter.sh cpu_dlrm_small_high 16G > dlrm_sh_16g.txt
./run_multi_tenants_iter.sh cpu_dlrm_small_high 32G > dlrm_sh_32g.txt
./run_multi_tenants_native.sh 1 cpu_dlrm_small_high > dlrm_sh_local.txt

./run_multi_tenants_iter.sh cpu_dlrm_large_low 4G > dlrm_ll_4g.txt
./run_multi_tenants_iter.sh cpu_dlrm_large_low 8G > dlrm_ll_8g.txt
./run_multi_tenants_iter.sh cpu_dlrm_large_low 16G > dlrm_ll_16g.txt
./run_multi_tenants_iter.sh cpu_dlrm_large_low 32G > dlrm_ll_32g.txt
./run_multi_tenants_iter.sh cpu_dlrm_large_low 64G > dlrm_ll_64g.txt
./run_multi_tenants_native.sh 1 cpu_dlrm_large_low > dlrm_ll_local.txt

./run_multi_tenants_iter.sh cpu_dlrm_large_high 4G > dlrm_lh_4g.txt
./run_multi_tenants_iter.sh cpu_dlrm_large_high 8G > dlrm_lh_8g.txt
./run_multi_tenants_iter.sh cpu_dlrm_large_high 16G > dlrm_lh_16g.txt
./run_multi_tenants_iter.sh cpu_dlrm_large_high 32G > dlrm_lh_32g.txt
./run_multi_tenants_iter.sh cpu_dlrm_large_high 64G > dlrm_lh_64g.txt
./run_multi_tenants_native.sh 1 cpu_dlrm_large_high > dlrm_lh_local.txt

./run_multi_tenants_iter.sh fotonik 4G > fotonik_4g.txt
./run_multi_tenants_iter.sh fotonik 8G > fotonik_8g.txt
./run_multi_tenants_native.sh 1 fotonik > fotonik_local.txt

./run_multi_tenants_iter.sh roms 4G > roms_4g.txt
./run_multi_tenants_iter.sh roms 8G > roms_8g.txt
./run_multi_tenants_native.sh 1 roms > roms_local.txt
END
