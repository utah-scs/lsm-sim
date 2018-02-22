#!/usr/bin/env python

import sys
import os
import subprocess
import datetime
import argparse

SHADOWLRU = 0
FIFO = 1
LRU = 2

def compute(outfile,
            errfile,
            policy=FIFO,
            size=None,
            apps=[],
            warmup=None,
            request_limit=None,
            infile=None):
    cmd = ('../lsm-sim %s %s %s %s %s %s' %
           ('-p %d' % policy,
            ('', '-s %s' % str(size))[size != None],
            ('', '-l %s' % str(request_limit))[request_limit != None],
            ('', '-a %s' % ','.join([str(i) for i in apps]))[len(apps) > 0],
            ('', '-w %s' % str(warmup))[warmup != None],
            ('', '-f %s' % infile)[infile != None]
           )
          )
    print(cmd)
    p = subprocess.Popen(cmd, shell=True, stdout=outfile, stderr=errfile)
    return p

def main():
    parser = argparse.ArgumentParser(description='Process some integers.')
    parser.add_argument('--limit', dest='request_limit',
                         default=None,
                         help='Simulate only this many requests after warmup.')
    args = parser.parse_args()

    # Setup output dirs.
    try:
        os.mkdir('logs')
    except:
        pass
    ts = datetime.datetime.now().strftime('%Y%m%d%H%m%S')
    log_dir = os.path.join('logs', ts)
    os.mkdir(log_dir)
    os.unlink(os.path.join('logs', 'latest'))
    os.symlink(ts, os.path.join('logs', 'latest'))

    # Set parameters and launch parallel processes.
    app = 20
    warmup = 86400

    cache_size = 2**20
    procs = []
    max_procs = 4
    for policy in range(1, 3):
        while cache_size < 128 * 2**20:
            prefix = 'output-policy%d-app%d-size%08d' % (policy,
                                                         app,
                                                         cache_size)
            out_path = os.path.join(log_dir, prefix + '.out')
            err_path = os.path.join(log_dir, prefix + '.err')
            with file(out_path, 'w') as outfile:
                with file(err_path, 'w') as errfile:
                    procs.append(
                        compute(outfile, errfile,
                            policy=policy,
                            size=cache_size, apps=[app],
                            warmup=warmup, request_limit=args.request_limit))
            cache_size *=2
            if len(procs) == max_procs:
                proc = procs.pop(0)
                proc.wait()
    for proc in procs:
        proc.wait()

if __name__ == '__main__': main()
