#!/bin/sh

weights_size="14540253,14540253,14540253,14540253,14540253,14540253,14540253,14540253,14540253,14540253,14540253,14540253,14540253,14540253,14540253,14540253,14540253,14540253,14540253,14540253,14540253,14540253,14540253,14540253,14540253,14540253,14540253,14540253,14540253,14540253,14540253,14540253,14540253,14540253,14540253,14540253" # 36x6-tuple
alpha="0.00000025"
# ./threes --total=0 --slide="init=$weights_size save=weights.bin" # generate a clean network
for i in {1..200}; do
        ./threes --total=100000 --block=10000 --limit=1000 --slide="load=weights.bin save=weights.bin alpha=${alpha}" | tee -a train.log
        ./threes --total=1000 --slide="load=weights.bin alpha=0" --save="stats.txt"
        tar zcvf ./log/weights.$(date +%Y%m%d-%H%M%S).tar.gz train.log weights.bin stats.txt
done
