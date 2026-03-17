#!/usr/bin/env bash
set -xeuo pipefail

sudo taskset -c 1-12 ~/tools/wrk/wrk -t10 -c10 \
	-H 'Connection: keep-alive' --latency --timeout 60 \
	-s ibm_workload.lua http://12.12.12.1:2000 -- workload042.txt false

# workload035.txt  workload042.txt


# # Ceph
# sudo taskset -c 1-12 ~/tools/wrk/wrk -t12 -c120 -d5s \
# 	-H 'Connection: keep-alive' --latency --timeout 60 \
# 	-s random-reads.lua http://12.12.12.2:80 -- 100000 false
