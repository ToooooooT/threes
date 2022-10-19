#!/bin/sh

weights_size="14641,14641,14641,14641,14641,14641,14641,14641,14641,1771561,1771561,1771561,1771561,1771561,1771561,1771561,1771561,1771561,1771561,1771561,1771561,1771561,1771561,1771561,1771561,1771561,1771561,1771561,1771561,1771561,1771561,1771561,1771561,1771561,1771561,1771561,1771561,1771561,1771561,1771561,1771561,1771561,1771561,1771561,1771561" # 36x6 + 9x4-tuple
alpha="0.0025"
# ./threes --total=0 --slide="init=$weights_size save=weights.bin" # generate a clean network
for i in {1..671}; do
        if [ ${i} -gt 71 ]; then
            alpha="0.000025";
        elif [ ${i} -gt 371 ]; then
            alpha="0.00000025";
        fi
        ./threes --total=100000 --block=10000 --limit=1000 --slide="load=weights.bin save=weights.bin alpha=${alpha}" | tee -a train.log
        ./threes --total=1000 --slide="load=weights.bin alpha=0"
        tar zcvf ./log/weights.$(date +%Y%m%d-%H%M%S).tar.gz train.log
done
