#!/usr/bin/env bash
set -xeuo pipefail

# ~/tools/wrk/wrk -t1 -c1 -d1s \
# 	-s seq-writes.lua \
# 	http://127.0.0.1:2000 -- 10 false


~/tools/wrk/wrk -t8 -c100 -d5s --timeout 10s \
	-s seq-writes.lua \
	http://127.0.0.1:2000 -- 100000 false

