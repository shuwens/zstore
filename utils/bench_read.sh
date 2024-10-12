#!/usr/bin/env bash
set -xeuo pipefail

~/tools/wrk/wrk -t8 -c800 -d10s -s random-reads.lua http://127.0.0.1:2000 -- 100000 false
