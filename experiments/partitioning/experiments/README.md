### Vanilla LRU comparison with partitioned\_LRU
Comparison between typical LRU policy and LRU partitions.

# LSM-SIM Commit
b6e6ede222b1a7695149469059222ad041384980

# Parameters
app : 19
trace : ~/traces/app19
partitions : {1, 8, 16, 1024, 16384, 1048577} for partitioned\_LRU

# Description

This experiment will run a single iteration of the app19 trace on traditional
LRU, and 5 traces of increasing partitioning i.e. 1, 4, 16, 16384, 1048577}.
The traditional LRU run is purely a control case to verify that traditional\_LRU
is correct and displays the same numbers as partitioned\_LRU when run with a
single partition.

