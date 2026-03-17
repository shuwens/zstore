#!/usr/bin/env bash
set -xeuo pipefail

# RUN 4k writes
# sudo taskset -c 1-12 ~/tools/wrk/wrk -t12 -c120 -d5s \
# 	-H 'Connection: keep-alive' --latency --timeout 60 \
# 	-s ceph-writes.lua http://12.12.12.2:80 -- 10000 false

# RUN 4k writes
sudo taskset -c 1-12 ~/tools/wrk/wrk -t120 -c120 -d10s \
	-s ceph-writes.lua http://12.12.12.2:8080
	# -s put_4kb_minio.lua http://12.12.12.2:8080
