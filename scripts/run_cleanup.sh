#!/usr/bin/env bash
set -euo pipefail

RED='\033[1;35m'
GREEN='\033[1;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

for node in zstore1 zstore3 zstore4 zstore5 zstore6
do
	ssh ${node} "sudo killall -r 'zstore' || true"
	ssh ${node} "cd ~/dev/zstore/ && git pull && cd build && meson compile"
done

echo "Done"
