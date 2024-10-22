#!/usr/bin/env bash
set -xeuo pipefail

# ~/tools/wrk/wrk -t4 -c10 -d5s -s random-reads.lua http://127.0.0.1:2000 -- 100000 false

# ~/tools/wrk/wrk -t2 -c20 -d10s -s random-reads.lua http://127.0.0.1:2000 -- 1000 false

sudo taskset -c 7-11  ~/tools/wrk/wrk -t16 -c800 -d30s -s random-reads.lua http://127.0.0.1:2000 -- 100000 false
