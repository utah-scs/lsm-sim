#!/bin/bash

# Measures partitioning over life of Memcachier trace with varying number of
# partitions. Also includes a run with the original LRU implementation for
# comparison.

partitions="1 8 16 1024 16384"

for partition in $partitions; do
  ~/projects/lsm-sim/lsm-sim -a 19 \
             -p partitioned_LRU \
             -Y $partition \
             -v \
             -f ~/traces/app19 > partitioned_LRU_19_${partition}.data 2>&1 &
done 

~/projects/lsm-sim/lsm-sim -a 19 \
                           -p lru \
                           -v \
                           -f ~/traces/app19 > LRU_19.data 2>&1 &
wait
