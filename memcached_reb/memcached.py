#!python

import sys
import csv
from collections import deque

## Globals.
trace_file  = sys.argv[1]
appid       = int(sys.argv[2])
gfactor     = float(sys.argv[3])
init_chunk  = int(sys.argv[4])
hits = 0
accesses = 0
appID = 19
num_classes = 0
MB = 1024 * 1024
global_mem = 1024 * 1024 * 10
mem_used  = 0

print "Simulating memcached on app %d with growth factor %f" %(appid,gfactor) 

##
class RTyp:
  GET, SET, DEL, ADD, INC, STA, OTH = range(7)

##
class Request:
  def __init__ (self, time, appid, typ, ksz, vsz, kid, hit):
    self.time = time
    self.appid = appid
    self.typ = typ
    self.ksz = ksz
    self.vsz = vsz
    self.kid = kid
    self.hit = hit
  def printReq (self) :
    print "%f %d %d %d %d %d %d" %(self.time, self.appid, self.typ, self.ksz, 
    self.vsz, self.kid, self.hit) 
##
class DataObj:
  def __init__(self, ksz, vsz, objid):
    self.ksz = ksz
    self.vsz = vsz
    self.objid = objid
    self.slabid = 0
  def __eq__(self, other):
    return self.objid == other.objid
  def size (self):
    return self.ksz + self.vsz

## 
class SlabClass:
  def __init__(self, chk):
    self.chunk_size = chk 
  eviction_queue = deque()
  
  def put (self, obj):
    global global_mem
    global mem_used
 
    # Evict if needed.
    while global_mem - mem_used < obj.size():
      victim = self.eviction_queue.popleft()
      del lookup[victim.objid]
      mem_used -= victim.size()   
    
    # Append new obj.
    self.eviction_queue.append(obj)
    mem_used += obj.size()
    lookup[obj.objid] = obj
        

# Gen slab classes.
slab_classes = [] 
 
MAX_CLASSES = 64
POWER_SMALLEST = 1
POWER_LARGEST = 256
CHUNK_ALIGN = 8

i = POWER_SMALLEST - 1
size = init_chunk
num_classes = 1
while( i < MAX_CLASSES -1 and size <= MB):
  print "Created class %d %d" %(i, size) 
  num_classes += 1
  if(size % CHUNK_ALIGN != 0):
    size += CHUNK_ALIGN - (size % CHUNK_ALIGN)
  slab_classes.append(SlabClass(size))
  size *= gfactor
  i+=1
 
# Simple map to simplify/speed up lookup
lookup = { }

## Returns class for size 's'.
def find_class (s):
  for i in range(0, num_classes - 1):
    if slab_classes[i].chunk_size >= s:
      return i

## Parse file and process request.
with open(trace_file, 'rb') as tf:
  reader = csv.reader(tf, delimiter=',')
  for rw in reader:
    req = Request(float(rw[0]),int(rw[1]),int(rw[2]),int(rw[3]),int(rw[4]),
    int(rw[5]),int(rw[6]))   
    if req.hit and req.vsz > 0 and req.appid == appID:
      accesses += 1
      rec = lookup.get(req.kid)
      if (rec != None):
        hits +=1
      else:
        c = find_class (req.ksz + req.vsz) 
        obj = DataObj(req.ksz, req.vsz, req.kid)
        slab_classes[c].put(obj)  
      if accesses % 100000 == 0:
        print "%f %f" %(req.time,  hits/float(accesses))
    



