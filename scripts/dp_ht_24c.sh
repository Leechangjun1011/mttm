#!/bin/bash -xe
export LD_PRELOAD=$CONDA_PREFIX/lib/libiomp5.so #:$CONDA_PREFIX/lib/libjemalloc.so
export MALLOC_CONF="oversize_threshold:1,background_thread:true,metadata_thp:auto,dirty_decay_ms:9000000000,muzzy_decay_ms:9000000000"
export KMP_AFFINITY=verbose,granularity=fine,compact,1,0
export KMP_BLOCKTIME=1
export OMP_NUM_THREADS=1
PyGenTbl='import sys; rows,tables=sys.argv[1:3]; print("-".join([rows]*int(tables)))'

if [ $# -ne 2 ]
then
	echo "2 input(size, reuse distance) required"
	exit 0
fi

NUM_BATCH=120
BS=64
LOG=print_out.log
INSTANCES=8
EXTRA_FLAGS=
GDB='gdb --args'
DLRM_SYSTEMS=$DLRM_SYSTEM


if [[ "$1" == "small" ]]; then
	BOT_MLP=256-128-128
	TOP_MLP=128-64-1
	EMBS='128,1000000,60,120'
elif [[ "$1" == "med" ]]; then
	BOT_MLP=1024-512-128-128
	TOP_MLP=384-192-1
	EMBS='128,1000000,120,150'
elif [[ "$1" == "large" ]]; then
	BOT_MLP=2048-1024-256-128
	TOP_MLP=512-256-1
	EMBS='128,1000000,170,180'
fi

if [[ "$2" == "low" ]]; then
	reuse="$DLRM_SYSTEMS/datasets/reuse_low/table_1M.txt"
elif [[ "$2" == "mid" ]]; then
	reuse="$DLRM_SYSTEMS/datasets/reuse_medium/table_1M.txt"
elif [[ "$2" == "high" ]]; then
	reuse="$DLRM_SYSTEMS/datasets/reuse_high/table_1M.txt"
fi


for e in $EMBS; do
	IFS=','; set -- $e; EMB_DIM=$1; EMB_ROW=$2; EMB_TBL=$3; EMB_LS=$4; unset IFS;
	EMB_TBL=$(python -c "$PyGenTbl" "$EMB_ROW" "$EMB_TBL")
	DATA_GEN="prod,$reuse,$EMB_ROW"
	$CONDA_PREFIX/bin/python -u $MODELS_PATH/models/recommendation/pytorch/dlrm/product/dlrm_s_pytorch.py --data-generation=$DATA_GEN --round-targets=True --learning-rate=1.0 --arch-mlp-bot=$BOT_MLP --arch-mlp-top=$TOP_MLP --arch-sparse-feature-size=$EMB_DIM --max-ind-range=40000000 --numpy-rand-seed=727 --ipex-interaction --inference-only --num-batches=$NUM_BATCH --data-size 100000000 --num-indices-per-lookup=$EMB_LS --num-indices-per-lookup-fixed=True --arch-embedding-size=$EMB_TBL --print-freq=10 --print-time --mini-batch-size=$BS --share-weight-instance=$INSTANCES $EXTRA_FLAGS | tee -a $LOG
done



