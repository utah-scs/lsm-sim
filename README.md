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

  -Y number of cache partitions

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

# More Trace File Details

## Trace format layout

	In the output file, each row is a single request:
	The first column is the time (in seconds)
	The second column is the application ID
	The third column is the type of request (see legend below)
	The fourth column is the size of the key
	The fifth column is the size of the value. If the size of the value is -1, it means the request does not have a value size (e.g., Delete), or it means that the request was a miss and we could not determine the size of the value.
	The sixth column is the unique key ID. We simply used an incremental counter to identify each request.
	The seventh column indicates if the request hit or missed if it was a GET request (0 is a miss)

	Type of request:
	1 = Get
	2 = Set
	3 = Delete
	4 = Add
	5 = Increment
	6 = Stats
	7 = Other


## Trace format FAQ

Q :   For the paper we estimated the amount of memory memcachier allocated each slab class, by looking at the hit rate that each slab class achieved in the trace, and then by seeing how much memory would be required to achieve this hit rate.So basically, you scan the whole trace to get hit rate. You generate your hit rate curve for each slab class using the whole trace. And then you match the slab class’s hit rate on the y-axis of your hit rate graphs to find a slab size (on the x-axis). Correct me if I’m wrong on that.

A : 	Exactly correct. When calculating the hit rate curves, we ran the trace for one day (letting the cache fill up) and then started computing the hit rate curve. The only rub: in some applications (application trace IDs 1, 3 and 5), estimating the hit rate precisely seems impossible (basically we are estimating the stack distances, and for these applications the stack distances explode and it would take many years to compute it across the entire week of traces. Unfortunately calculating stack distances precisely seems like a sequential operation, so it's hard to parallelize). So instead of calculating the stack distances precisely for these applications, we used an approximation (using the bucket technique described in Mimir).


Q :		Does this mean orig_memory_allocation is application specific? The script runs for app_ID 19. Would we need a new orig_memory_allocation to test it for app_ID 5?

A :		Yes, orig_memory_allocation is application specific.

Q :		The paper reports the LSM number for app_ID 5, not for app_ID 19. We wanted to sanity check against the paper. I assume if we re-run this for app_ID 5 it should match table 2 (96.9%).

A:		The numbers are switched. In the paper we ranked the applications based on the number of requests they had in the trace. In the simulation, we used the original application IDs. So application 20 in the trace is application 5 in the paper. The mapping is:


```
Application in Trace	Application in Cliffhanger Paper
1											1
3											2
19										3
18										4
20										5
6											6
5											7
8											8
59										9
227										10
29										11
10										12
94										13
11										14
23										15
2											16
7											17
53										18
13										19
31										20
```
# Experiments

If you used LSM-SIM to conduct experiments and must store your
scripts/experiments in the LSM-SIM repo, please create a folder for
them within the lsm-sim/experiments folder.
