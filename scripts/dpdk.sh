#!/usr/bin/env bash

set -xeuo pipefail

zstore_dir=$(git rev-parse --show-toplevel)
source $zstore_dir/.env
source $zstore_dir/scripts/network_env.sh

cd $zstore_dir/subprojects/spdk

if pidof nvmf_tgt; then
	scripts/rpc.py spdk_kill_instance SIGTERM >/dev/null || true
	scripts/rpc.py spdk_kill_instance SIGKILL >/dev/null || true
	pkill -f nvmf_tgt || true
	pkill -f reactor_0 || true
	sleep 3
fi

# echo off | sudo tee /sys/devices/system/cpu/smt/control

# HUGEMEM=4096 ./scripts/setup.sh
# HUGEMEM=8192 ./scripts/setup.sh
./scripts/setup.sh
