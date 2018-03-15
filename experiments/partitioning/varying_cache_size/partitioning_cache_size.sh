#!/bin/bash

# Runs the cross product of cache sizes and number of partitions.

app="19"
policy="partitioned_LRU"
cache_sizes="50 100 500 1000"           # in MiB
num_partitions="1 8 16 1024 8192 16384"
max_overall_request_size="3200"         # 50*1024*1024 / 16384 

for cache_size in $cache_sizes
do
  for partitions in $num_partitions
  do
    ~/projects/lsm-sim/lsm-sim -a $app \
    -p $policy \
    -Y $partitions \
    -R $max_overall_request_size \
    -G $cache_size \
    -f ~/traces/app19 > partitioned_LRU_app${app}_${cache_size}_${partitions}.txt 2>&1 &
  done
done

wait
