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

HUGEMEM=4096 ./scripts/setup.sh

modprobe ib_cm
modprobe ib_core
# Please note that ib_ucm does not exist in newer versions of the kernel and is not required.
modprobe ib_ucm || true
modprobe ib_umad
modprobe ib_uverbs
modprobe iw_cm
modprobe rdma_cm
modprobe rdma_ucm

# modprobe nvme
# modprobe nvmet
# modprobe nvmet_tcp

modprobe mlx4_core
modprobe mlx4_ib
modprobe mlx4_en

# ifconfig ibp1s0d1 192.168.100.8 netmask 255.255.255.0 up
# ifconfig eth2 192.168.100.9 netmask 255.255.255.0 up

# modprobe nvme-rdma
modprobe nvme-tcp


./build/bin/nvmf_tgt -m '[0,1,2,3]' &
sleep 3

ctrl_nqn="nqn.2024-04.io.zstore:cnode1"

# pci1=$(lspci -mm | perl -lane 'print @F[0] if /NVMe/' | head -1)
pci2=$(lspci -mm | perl -lane 'print @F[0] if /NVMe/' | tail -1)
pci1=05:00.0

scripts/rpc.py bdev_nvme_attach_controller -b nvme0 -t PCIe -a $pci1
scripts/rpc.py bdev_nvme_attach_controller -b nvme1 -t PCIe -a $pci2

scripts/rpc.py nvmf_create_transport -t TCP -u 16384 -m 8 -c 8192
# scripts/rpc.py nvmf_create_transport -t RDMA -u 8192 -i 131072 -c 8192

scripts/rpc.py nvmf_create_subsystem $ctrl_nqn -a -s SPDK00000000000001 -d SPDK_Controller1
sleep 1

scripts/rpc.py nvmf_subsystem_add_ns $ctrl_nqn nvme0n2
scripts/rpc.py nvmf_subsystem_add_ns $ctrl_nqn nvme1n2

# scripts/rpc.py nvmf_subsystem_add_listener $ctrl_nqn -t tcp -a 192.168.1.121 -s 4420
# scripts/rpc.py nvmf_subsystem_add_listener $ctrl_nqn -t tcp -a 12.12.12.2 -s 5520

# scripts/rpc.py nvmf_subsystem_add_listener $ctrl_nqn -t tcp -a 192.168.1.121 -s 4420
# scripts/rpc.py nvmf_subsystem_add_listener $ctrl_nqn -t rdma -a 12.12.12.1 -s 5520
# scripts/rpc.py nvmf_subsystem_add_listener $ctrl_nqn -t rdma -a 192.168.100.9 -s 5520
scripts/rpc.py nvmf_subsystem_add_listener $ctrl_nqn -t tcp -a 192.168.100.9 -s 5520

wait
