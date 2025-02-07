#!/usr/bin/env bash
set -xeuo pipefail

~/tools/wrk/wrk -t1 -c1 -d1s \
	-s seq-writes.lua \
	http://127.0.0.1:2000 -- 10 false

