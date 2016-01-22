#!/usr/bin/env python

import csv
import traceback
import sys
import math
import random
from collections import deque

if len(sys.argv) < 3:
  print "usage: <in-file> <app-id>"
  sys.exit(-1)

input_file = sys.argv[1]
application_ID = int(sys.argv[2])

lsm_utilization = 0.7

# List of hits (1 is a hit, 0 is a miss)
global_hits = []
# LRU queue of request tuples. Each request is a tuple of: <key ID, request size>
global_lru = deque()

slab_classes = []
for i in range(0, 15):
    slab_classes.append(64 * pow(2, i))

# Amount of bytes allocated to each slab class (from slab class 0 to 14)
# From Asaf: For the paper we estimated the amount of memory memcachier
# allocated each slab class, by looking at the hit rate that each slab class
# achieved in the trace, and then by seeing how much memory would be required
# to achieve this hit rate.
orig_memory_allocation = [ 1664         #  64 B
                         , 2304         # 128 B
                         , 512          # 256 B
                         , 17408
                         , 266240
                         , 16384
                         , 73728
                         , 188416
                         , 442368
                         , 3932160
                         , 11665408
                         , 34340864
                         , 262144
                         , 0
                         , 0
                         ]

# Amount of effective memory in the LSM queue
global_memory = float(sum(orig_memory_allocation)) * lsm_utilization

orig_queue_size = []
for i in range(0, len(orig_memory_allocation)):
  orig_queue_size.append(math.ceil(float(orig_memory_allocation[i]) / float((pow(2, i) * 64))))

print "simulator: python"
print "performing trace analysis on app %d" % application_ID
print "using trace file: %s" % input_file
print "rounding: on"
print "utilization rate: %f" % lsm_utilization

# go through each row of the trace
with open(input_file, 'rb') as trace:
  tracereader = csv.reader(trace, delimiter=',')
  j = 0
  for row in tracereader:
    # Only look at GETs
    if (int(row[2]) == 1 and int(row[4]) > 0 and int(row[1]) == application_ID):
      if (j == 100):
        pos = 0;

      # Check the slab class of the request
      for i in range(0, 15):
        if (int(row[4]) + int(row[3]) <= slab_classes[i]):
          break
      try:
        global_position = -1
        # Computes the point in which the request was inserted into the queue
        # if it's larger than global_memory, the request will be a miss, if
        # it's smaller, it will be a hit.
        global_queue_size = 0
        k = 0
        # Compute the request size (round it up to the closest slab class)
        request_size = pow(2, i) * 64
        for key in global_lru:
          k = k + 1
          global_queue_size = global_queue_size + key[1]
          # Check if key exists in LRU queue
          if (key[0] == int(row[5])):
            global_position = k
            global_queue_size = global_queue_size + request_size - key[1]
            global_lru.remove(key)
            break
        global_lru.appendleft([int(row[5]), request_size])
        j = j + 1

        if (float(row[0]) > 0):
          if (global_position != -1):
            if (global_queue_size > global_memory):
              global_hits.append(0)
            else:
              global_hits.append(1)
          else:
            global_hits.append(0)
            
        if (j % 1000000 == 0):
          if (len(global_hits) > 0):
            print 'hit rate: %f' % (float(sum(global_hits)) / float(len(global_hits)))
      except Exception, err:
        print traceback.format_exc()
        sys.exit()
 

final_global_queue_size = 0
for _, size in global_lru:
  final_global_queue_size += size

print 'final global queue size: %d' % final_global_queue_size
print 'final hit rate: %f' % (float(sum(global_hits)) / float(len(global_hits)))
