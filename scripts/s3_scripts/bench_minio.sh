#!/usr/bin/env bash
set -xeuo pipefail

# RUN 4k writes
sudo taskset -c 1-12 ~/tools/wrk/wrk -t12 -c120 -d5s \
	-H 'Connection: keep-alive' --latency --timeout 60 \
	-s put_4kb_minio.lua http://12.12.12.2:9000 -- 100000

