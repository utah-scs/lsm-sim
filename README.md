# LSM-SIM

## Description

LSM-SIM is a simulation suite for DRAM-based web caches such as memcached the
intent of which is to expose potential performance gains between different
implementations for such caches. Specifically, LSM-SIM compares existing
slab-based allocators with proposed log-structured allocators such as LSC.  

## Usage

### Command Line Arguments

  -f specify trace file path

  -a specify whitespace separated list of apps to evaluate

  -r enable rounding

  -u specify utilization factor

  -w specify a warmup period

  -l specify number of requests to evaluate after warmup period

  -s specify simulated cache size in bytes

  -p specify which policy to simulate

  -v verbose incremental output

  -g specify slab growth factor

  -M force memcachier slab class configuration

### Output

## Trace Format Protocol

LSM-SIM operates on a trace file specified at the command line via the -f
flag. The trace is expected to be in CSV format with each line repre fields 
appearing as follows.

time (floating point), app id, type (get, put etc.), key size in bytes, value
size, key id, hit/miss bool.  

## Policy Details

### ShadowSlab

ShadowSlab simulates slab-based allocators such as memcached and Memcachier.
Slabs classes are established based on a object size. Classes are determined by
a growth factor (taken as command line arg -f). Each class keeps a dedicated
LRU eviction queue.

### gLRU

gLRU simulates a log-structured cache with a single LRU eviction queue. This
simulator is intended to provide analysis of log-structured allocation and
specifically LSC as described in ... 

## License

Copyright (c) 2016, Asaf Cidon, Ryan Stutsman, Daniel Rushton, The University of
Utah SCS

Permission to use, copy modify, and/or distribute this software for any purpose
with or without fee is hereby granted, provided that the above copyright notice
and this permission notice appear in all copies.

THIS SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR/S DISCALIM ALL WARRANTIES WITH
REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
FITNESS. IN NO EVENT SHALL THE AUTHOR/S BE LIABLE FOR ANY SPECIAL, DIRECT,
INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
OF USE, DATA OR PROFITS, WHETHER AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF
THIS SOFTWARE.
