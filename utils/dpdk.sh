#!/usr/bin/env bash

set -xeuo pipefail

zstore_dir=$(git rev-parse --show-toplevel)
source $zstore_dir/.env

cd $zstore_dir/subprojects/spdk

if pidof nvmf_tgt; then
	scripts/rpc.py spdk_kill_instance SIGTERM >/dev/null || true
	scripts/rpc.py spdk_kill_instance SIGKILL >/dev/null || true
	pkill -f nvmf_tgt || true
	pkill -f reactor_0 || true
	sleep 3
fi

echo off | sudo tee /sys/devices/system/cpu/smt/control

HUGEMEM=4096 ./scripts/setup.sh

if [[ $(hostname) == "zstore1" ]]; then
	sudo ifconfig enp1s0 12.12.12.1/24 up
fi
# if [[ $(hostname) == "zstore2" ]]; then
#         # sudo ifconfig enp1s0d1 12.12.12.2/24 up
#         # sudo ifconfig ibp1s0 12.12.12.2/24 up
# fi

