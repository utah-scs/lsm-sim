#!/usr/bin/env python

import sys
import os
import subprocess
import datetime

CLIFF = 0
FIFO = 1
LRU = 2

def compute(outfile,
            policy=FIFO,
            size=None,
            apps=[],
            warmup=None,
            request_limit=None,
            infile=None):
    cmd = ('./compute %s %s %s %s %s %s' %
           ('-p %d' % policy,
            ('', '-s %s' % str(size))[size != None],
            ('', '-l %s' % str(request_limit))[request_limit != None],
            ('', '-a %s' % ','.join([str(i) for i in apps]))[len(apps) > 0],
            ('', '-w %s' % str(warmup))[warmup != None],
            ('', '-f %s' % infile)[infile != None]
           )
          )
    print(cmd, 'into file', outfile)
    subprocess.call(cmd, shell=True, stdout=outfile)

def main():
    try:
        os.mkdir('logs')
    except:
        pass
    ts = datetime.datetime.now().strftime('%Y%m%d%H%m%S')
    log_dir = os.path.join('logs', ts)
    os.mkdir(log_dir)
    os.unlink(os.path.join('logs', 'latest'))
    os.symlink(ts, os.path.join('logs', 'latest'))

    app = 20
    request_limit = 1000000
    warmup = 0

    cache_size = 2**20
    for policy in range(0, 3):
        while cache_size < 64 * 2**20:
            out_filename = 'output-policy%d-app%d-size%08d.log' % (policy,
                                                                   app,
                                                                   cache_size)
            out_path = os.path.join(log_dir, out_filename)
            with file(out_path, 'w') as f:
                compute(f, size=cache_size, apps=[app],
                        warmup=warmup, request_limit=request_limit)
            cache_size *=2

if __name__ == '__main__': main()
