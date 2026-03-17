#!/usr/bin/env bash
set -xeuo pipefail

sudo taskset -c 1-12 ~/tools/wrk/wrk -t12 -c60 \
  -H 'Connection: keep-alive' --latency --timeout 60 \
  -s consistency_workload.lua http://12.12.12.1:2000 -- workload_4k.txt false
