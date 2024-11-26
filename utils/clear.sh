#!/usr/bin/env bash
set -xeuo pipefail

zstore_dir=$(git rev-parse --show-toplevel)
source $zstore_dir/.env

cd $zstore_dir/subprojects/spdk

if [ "$HOSTNAME" == "zstore1" ]; then
	pci1=05:00.0
	pci2=0b:00.0
	ctrl_nqn="nqn.2024-04.io.zstore1:cnode1"
	ip=12.12.12.1
elif [ "$HOSTNAME" == "zstore2" ]; then
	pci1=05:00.0
	pci2=06:00.0
	ctrl_nqn="nqn.2024-04.io.zstore2:cnode1"
	ip=12.12.12.2
elif [ "$HOSTNAME" == "zstore3" ]; then
	pci1=04:00.0
	pci2=06:00.0
	ctrl_nqn="nqn.2024-04.io.zstore3:cnode1"
	ip=12.12.12.3
elif [ "$HOSTNAME" == "zstore4" ]; then
	pci1=05:00.0
	pci2=0b:00.0
	ctrl_nqn="nqn.2024-04.io.zstore4:cnode1"
	ip=12.12.12.4
fi

scripts/rpc_py nvmf_subsystem_remove_listener $ctrl_nqn -t RDMA -a $ip -s 5520
scripts/rpc_py nvmf_delete_subsystem $ctrl_nqn
	# scripts/rpc.py nvmf_subsystem_add_ns $ctrl_nqn nvme0n2
	# scripts/rpc.py nvmf_subsystem_add_ns $ctrl_nqn nvme1n2
scripts/rpc.py bdev_nvme_detach_controller nvme0
scripts/rpc.py bdev_nvme_detach_controller nvme1

if pidof nvmf_tgt; then
	scripts/rpc.py spdk_kill_instance SIGTERM >/dev/null || true
	scripts/rpc.py spdk_kill_instance SIGKILL >/dev/null || true
	pkill -f nvmf_tgt || true
	pkill -f reactor_0 || true
	sleep 3
fi

scripts/setup.sh reset
