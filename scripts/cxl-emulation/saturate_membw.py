#!/usr/bin/env python3

"""
   saturate_membw.py

    Created on: Feb. 7, 2019
        Author: Taekyung Heo <tkheo@casys.kaist.ac.kr>
"""

import argparse
import numa
import os
import sys

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("-membw", default="./membw/membw")
    parser.add_argument("-bandwidth", type=int, default=8000)
    parser.add_argument("-operation", default="nt-write")
    parser.add_argument("-cores", type=int, default=16)
    parser.add_argument("-last_core_bw", type=int, default=0)
    args = parser.parse_args()

    max_nid = numa.get_max_node()
    if max_nid != 1:
        print("This tool requires two sockets at least")
        sys.exit()
    cpus = numa.node_to_cpus(max_nid)
    last_cpu = list(cpus)[-1]
    threshold = last_cpu - args.cores
    for cpuid in cpus:
        if cpuid <= threshold:
            continue
        cmd = "%s -c %d -b %d --%s &" % (args.membw, cpuid, args.bandwidth, args.operation)
        print(cmd)
        os.system(cmd)
    if args.last_core_bw > 0:
        cmd = "%s -c %d -b %d --%s &" % (args.membw, threshold, args.last_core_bw, args.operation)
        print(cmd)
        os.system(cmd)

if __name__ == '__main__':
    main()
