#!/bin/bash

# Runs the cross product of cache sizes and number of partitions.

app="19"
policy="partitioned_LRU"
cache_sizes="50 100"           # in MiB
num_partitions="1 16 64 256 1024 4096 8192 16384"
max_sizes="0 1024 4096 8192"          

for cache_size in $cache_sizes
do
  for max_size in $max_sizes
  do
    for partitions in $num_partitions
    do
      ~/projects/lsm-sim/lsm-sim -a $app \
      -p $policy \
      -Y $partitions \
      -R $max_size \
      -G $cache_size \
      -f ~/traces/app19 > pLRU_app${app}_csz${cache_size}_mrsz${max_size}_p${partitions}.txt 2>&1 &
    done
  done
  wait
done
wait
