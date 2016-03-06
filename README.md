# LSM-SIM

## Description

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
flag. The trace is expected to be in CSV format with each line repre fields appearing as
follows.

  


time (floating point), app id, type (get, put etc.), key size in bytes, value
size, key id, hit/miss bool.  

## Policy Details

### ShadowSlab

### gLRU

## Contributing:

## License
