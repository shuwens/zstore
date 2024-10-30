#!/usr/bin/env bash
set -xeuo pipefail

# ~/tools/wrk/wrk -t4 -c10 -d5s -s random-reads.lua http://127.0.0.1:2000 -- 100000 false

# ~/tools/wrk/wrk -t2 -c20 -d10s -s random-reads.lua http://127.0.0.1:2000 -- 1000 false

# sudo nice -n -20 taskset -c 7,8,9,10,11  ~/tools/wrk/wrk -t16 -c800 -d30s -s random-reads.lua http://127.0.0.1:2000 -- 100000 false

# sudo taskset -c 7,8,9,10,11  ~/tools/wrk/wrk -t16 -c1000 -d30s -s random-reads.lua http://127.0.0.1:2000 
# sudo taskset -c 7,8,9,10,11  ~/tools/wrk/wrk -t16 -c1000 -d30s -s random-reads.lua http://127.0.0.1:2000 

# sudo nice -n -20 taskset -c 8-11  ~/tools/wrk/wrk -t16 -c800 -d30s -s random-reads.lua http://127.0.0.1:2000 -- 100000 false
# sudo  taskset -c 0,4,5  ~/tools/wrk/wrk -t16 -c800 -d30s -s random-reads.lua http://127.0.0.1:2000 -- 100000 false
sudo  taskset -c 3,4,5  ~/tools/wrk/wrk -t16 -c800 -d10s -s random-reads.lua http://127.0.0.1:2000 -- 100000 false
