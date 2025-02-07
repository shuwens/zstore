#!/usr/bin/env bash
set -xeuo pipefail

# ~/tools/wrk/wrk -t4 -c10 -d5s -s random-reads.lua http://127.0.0.1:2000 -- 100000 false

# ~/tools/wrk/wrk -t2 -c20 -d10s -s random-reads.lua http://127.0.0.1:2000 -- 1000 false

# sudo nice -n -20 taskset -c 7,8,9,10,11  ~/tools/wrk/wrk -t16 -c800 -d30s -s random-reads.lua http://127.0.0.1:2000 -- 100000 false

# sudo taskset -c 7,8,9,10,11  ~/tools/wrk/wrk -t16 -c1000 -d30s -s random-reads.lua http://127.0.0.1:2000 
# sudo taskset -c 7,8,9,10,11  ~/tools/wrk/wrk -t16 -c1000 -d30s -s random-reads.lua http://127.0.0.1:2000 

# sudo nice -n -20 taskset -c 8-11  ~/tools/wrk/wrk -t16 -c800 -d30s -s random-reads.lua http://127.0.0.1:2000 -- 100000 false
# sudo  taskset -c 0,4,5  ~/tools/wrk/wrk -t16 -c800 -d30s -s random-reads.lua http://127.0.0.1:2000 -- 100000 false
# sudo  taskset -c 3,4,5  ~/tools/wrk/wrk -t16 -c800 -d10s -s random-reads.lua http://127.0.0.1:2000 -- 100000 false



# optimal: 10s to 30s
# 300k
# sudo taskset -c 12-15 ~/tools/wrk/wrk -t8 -c400 -d30s -s random-reads.lua http://12.12.12.1:2000 -- 100000 false

# 400k
# sudo taskset -c 10-15 ~/tools/wrk/wrk -t12 -c600 -d10s -s random-reads.lua http://12.12.12.1:2000 -- 100000 false
# sudo taskset -c 8-15 ~/tools/wrk/wrk -t16 -c800 -d10s -s random-reads.lua http://12.12.12.1:2000 -- 100000 false


# sudo taskset -c 10-15 ~/tools/wrk/wrk -t12 -c600 -d10s -s random-reads.lua http://12.12.12.1:2000 -- 100000 false

# 250k with 250us
# sudo taskset -c 10-15 ~/tools/wrk/wrk -t6 -c60 -d10s -s random-reads.lua http://12.12.12.1:2000 -- 100000 false

# 12 threads and 180 connections
# Thread Stats   Avg      Stdev     Max   +/- Stdev
# Latency   544.51us  177.09us  12.59ms   95.09%
# Req/Sec    27.63k     1.53k   31.19k    98.02%
# 3331936 requests in 10.10s, 12.98GB read
# Requests/sec: 329905.63
# Transfer/sec:      1.29GB
# sudo taskset -c 1-12 ~/tools/wrk/wrk -t12 -c120 -d5s \

sudo taskset -c 1-12 ~/tools/wrk/wrk -t12 -c120 -d5s \
	-H 'Connection: keep-alive' --latency --timeout 60 \
	-s random-reads.lua http://12.12.12.1:2000 -- 100000 false

# 380k
# sudo taskset -c 1-12 ~/tools/wrk/wrk -t12 -c60 -d5s \
# 	-H 'Connection: keep-alive' --latency --timeout 60 \
# 	-s random-reads.lua http://12.12.12.1:2000 -- 100000 false



# sudo taskset -c 10-15 ~/tools/wrk/wrk -t12 -c180 -d10s -s random-reads.lua http://12.12.12.1:2000 -- 100000 false
# sudo taskset -c 10-15 ~/tools/wrk/wrk -t18 -c256 -d10s -s random-reads.lua http://12.12.12.1:2000 -- 100000 false

sudo taskset -c 1-12 ~/tools/wrk/wrk -t12 -c60 -d2s \
	-H 'Connection: keep-alive' --latency --timeout 60 \
	-s random-reads.lua http://12.12.12.1:2000 -- 100000 false

# Ceph
# sudo taskset -c 1-12 ~/tools/wrk/wrk -t12 -c120 -d5s \
# 	-H 'Connection: keep-alive' --latency --timeout 60 \
# 	-s random-reads.lua http://12.12.12.2:80 -- 100000 false


