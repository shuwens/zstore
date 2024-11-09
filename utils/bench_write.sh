#!/usr/bin/env bash
set -xeuo pipefail

# 4 threads and 40 connections
#   Thread Stats   Avg      Stdev     Max   +/- Stdev
#     Latency   384.69us  560.18us  16.26ms   99.46%
#     Req/Sec    28.02k   746.35    29.76k    81.82%
#   122632 requests in 1.10s, 12.28MB read
# Requests/sec: 111557.57
# Transfer/sec:     11.17MB
# sudo taskset -c 10-15 ~/tools/wrk/wrk -t4 -c40 -d1s \
# 	-s seq-writes.lua \
# 	http://12.12.12.1:2000 -- 100000 false



sudo taskset -c 10-15 ~/tools/wrk/wrk -t6 -c60 -d1s \
	-s seq-writes.lua \
	http://12.12.12.1:2000 -- 100000 false

