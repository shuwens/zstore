#!/usr/bin/env bash
# set -xeuo pipefail
set -euo pipefail

source $(dirname $0)/utils.sh
source $(dirname $0)/autorun_parse.sh

if [ ${autorun_gen} == "wrk" ]; then
  echo "Using wrk"
  if [ ${autorun_workload} == "read" ]; then
    sudo taskset -c 1-${autorun_num_processes} ~/tools/wrk/wrk -t${autorun_num_processes} -c${autorun_num_connections} -d5s \
      -H 'Connection: keep-alive' --latency --timeout 60 \
      -s random-reads.lua ${autorun_ip_addr} -- 100000 false
  elif [ ${autorun_workload} == "write" ]; then
    sudo taskset -c 1-${autorun_num_processes} ~/tools/wrk/wrk -t${autorun_num_processes} -c${autorun_num_connections} -d1s \
      -H 'Connection: keep-alive' --latency --timeout 60 \
      -s random-reads.lua ${autorun_ip_addr} -- 100000 false
  fi
elif [ $autorun_gen == "s3bench" ]; then
  echo "Using s3bench"
else
  echo "Invalid autorun_gen"
  exit 1
fi
