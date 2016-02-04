#!/usr/bin/env python

import sys
import os

CLIFF = 0
FIFO = 1
LRU = 2

def compute(size=None, apps=[], warmup=None, policy=FIFO, infile=None):
    cmd = ('./compute %s %s %s %s %s' %
           ('-p %d' % policy,
            ('', '-s %s' % str(size))[size != None],
            ('', '-a %s' % ','.join([str(i) for i in apps]))[len(apps) > 0],
            ('', '-w %s' % str(warmup))[warmup != None],
            ('', '-f %s' % infile)[infile != None]
           )
          )
    print(cmd)
    os.system(cmd)

def main():
    compute(size=1000, apps=[20], warmup=0)

if __name__ == '__main__': main()
