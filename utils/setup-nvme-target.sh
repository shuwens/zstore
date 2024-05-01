#!/usr/bin/env bash

set -xeuo pipefail

zstore_dir=$(git rev-parse --show-toplevel)
source $zstore_dir/.env

echo "Running gateway on $gw_ip, remote on $host_ip"
# echo "Running with image $pool_name/$imgname"

if pidof nvmf_tgt; then
	scripts/rpc.py spdk_kill_instance SIGTERM >/dev/null || true
	scripts/rpc.py spdk_kill_instance SIGKILL >/dev/null || true
	pkill -f nvmf_tgt || true
	pkill -f reactor_0 || true
	sleep 3
fi

cd $zstore_dir/subprojects/spdk

modprobe nvme
modprobe nvmet
modprobe nvmet_tcp

./build/bin/nvmf_tgt -m '[0,1,2,3]' &
sleep 3

scripts/rpc.py bdev_nvme_attach_controller -b nvme0 -t PCIe -a 0000:01:00.0
scripts/rpc.py bdev_nvme_attach_controller -b nvme1 -t PCIe -a 0000:06:00.0

scripts/rpc.py nvmf_create_transport -t TCP -u 16384 -m 8 -c 8192
scripts/rpc.py nvmf_create_subsystem nqn.2024-04.io.zstore:cnode1 -a -s SPDK00000000000001 -d SPDK_Controller1
sleep 1

scripts/rpc.py nvmf_subsystem_add_ns nqn.2016-06.io.spdk:cnode1 nvme0n2
scripts/rpc.py nvmf_subsystem_add_ns nqn.2016-06.io.spdk:cnode1 nvme1n2
scripts/rpc.py nvmf_subsystem_add_listener nqn.2024-04.io.zstore:cnode1 -t tcp -a $host_ip -s 23789
