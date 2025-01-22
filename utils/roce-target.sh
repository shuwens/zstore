#!/usr/bin/env bash
set -xeuo pipefail

# Revision 2.3
# 613986-002
# NVM Express over Fabrics with SPDK
# for Intel ® Ethernet Products with
# RDMA
# Configuration Guide
# Ethernet Products Group (EPG)
# May 2021

zstore_dir=$(git rev-parse --show-toplevel)
source $zstore_dir/.env

cd $zstore_dir/subprojects/spdk

if pidof nvmf_tgt; then
	scripts/rpc.py spdk_kill_instance SIGTERM >/dev/null || true
	scripts/rpc.py spdk_kill_instance SIGKILL >/dev/null || true
	pkill -f nvmf_tgt || true
	pkill -f reactor_0 || true
	sleep 3

# 	python3 /opt/spdk/scripts/rpc.py bdev_nvme_detach_controller ${bdev}
# 2. Stop nvmeof_target process.
# 3. Revert drives back to kernel drivers.
# ./scripts/setup.sh reset
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

modprobe mlx4_core
modprobe mlx4_ib
modprobe mlx4_en

modprobe nvme-rdma

# ifconfig enp1s0 12.12.12.2 netmask 255.255.255.0 up

./build/bin/nvmf_tgt -m '[0,1,2,3]' &
sleep 3

if [ "$HOSTNAME" == "zstore2" ]; then
	sudo ifconfig enp1s0 12.12.12.2/24 up
	sudo ifconfig enp1s0 mtu 4200
	pci1=05:00.0
	pci2=07:00.0
	ctrl_nqn="nqn.2024-04.io.zstore2:cnode1"
elif [ "$HOSTNAME" == "zstore3" ]; then
	sudo ifconfig enp1s0 12.12.12.3/24 up
	sudo ifconfig enp1s0 mtu 4200
	pci1=05:00.0
	pci2=07:00.0
	ctrl_nqn="nqn.2024-04.io.zstore3:cnode1"
elif [ "$HOSTNAME" == "zstore4" ]; then
	sudo ifconfig enp1s0 12.12.12.4/24 up
	sudo ifconfig enp1s0 mtu 4200
	pci1=06:00.0
	pci2=0c:00.0
	ctrl_nqn="nqn.2024-04.io.zstore4:cnode1"
elif [ "$HOSTNAME" == "zstore5" ]; then
	sudo ifconfig enp1s0 12.12.12.5/24 up
	sudo ifconfig enp1s0 mtu 4200
	pci1=05:00.0
	pci2=06:00.0
	ctrl_nqn="nqn.2024-04.io.zstore5:cnode1"
fi

scripts/rpc.py bdev_nvme_attach_controller -b nvme0 -t PCIe -a $pci1
scripts/rpc.py bdev_nvme_attach_controller -b nvme1 -t PCIe -a $pci2

# scripts/rpc.py nvmf_create_transport -t TCP -u 16384 -m 8 -c 8192
# scripts/rpc.py nvmf_create_transport -t RDMA -u 8192 -i 131072 -c 8192
# scripts/rpc.py nvmf_create_transport -t RDMA -q 32 -n 1023

scripts/rpc.py nvmf_create_transport -t RDMA -q 128 -m 64 -c 4096 -i 131072 -u 8192 -a 128 -b 32 -n 8192
# {
# trtype: "RDMA"
# max_queue_depth: 128
# max_qpairs_per_ctrlr: 64
# in_capsule_data_size: 4096
# max_io_size: 131072
# io_unit_size: 8192
# max_aq_depth: 128
# num_shared_buffers: 8192
# buf_cache_size: 32
# }

if [ "$HOSTNAME" == "zstore1" ]; then
	scripts/rpc.py nvmf_create_subsystem $ctrl_nqn -a -s SPDK01 -d SPDK_Controller1 -m 8
	sleep 1
	scripts/rpc.py nvmf_subsystem_add_ns $ctrl_nqn nvme0n2
	scripts/rpc.py nvmf_subsystem_add_ns $ctrl_nqn nvme1n2
	# scripts/rpc.py nvmf_subsystem_add_listener $ctrl_nqn -t RDMA -f ipv4 -a 12.12.12.1 -s 5520
	scripts/rpc.py nvmf_subsystem_add_listener $ctrl_nqn -t RDMA -f ipv4 -a 12.12.12.1 -s 5520
elif [ "$HOSTNAME" == "zstore2" ]; then
	scripts/rpc.py nvmf_create_subsystem $ctrl_nqn -a -s SPDK02 -d SPDK_Controller2 -m 8
	sleep 1
	scripts/rpc.py nvmf_subsystem_add_ns $ctrl_nqn nvme0n2
	scripts/rpc.py nvmf_subsystem_add_ns $ctrl_nqn nvme1n2
	scripts/rpc.py nvmf_subsystem_add_listener $ctrl_nqn -t RDMA -f ipv4 -a 12.12.12.2 -s 5520
elif [ "$HOSTNAME" == "zstore3" ]; then
	scripts/rpc.py nvmf_create_subsystem $ctrl_nqn -a -s SPDK03 -d SPDK_Controller3 -m 8
	sleep 1
	scripts/rpc.py nvmf_subsystem_add_ns $ctrl_nqn nvme0n2
	scripts/rpc.py nvmf_subsystem_add_ns $ctrl_nqn nvme1n2
	scripts/rpc.py nvmf_subsystem_add_listener $ctrl_nqn -t RDMA -f ipv4 -a 12.12.12.3 -s 5520
elif [ "$HOSTNAME" == "zstore4" ]; then
	scripts/rpc.py nvmf_create_subsystem $ctrl_nqn -a -s SPDK04 -d SPDK_Controller4 -m 8
	sleep 1
	scripts/rpc.py nvmf_subsystem_add_ns $ctrl_nqn nvme0n2
	scripts/rpc.py nvmf_subsystem_add_ns $ctrl_nqn nvme1n2
	scripts/rpc.py nvmf_subsystem_add_listener $ctrl_nqn -t RDMA -f ipv4 -a 12.12.12.4 -s 5520
elif [ "$HOSTNAME" == "zstore5" ]; then
	scripts/rpc.py nvmf_create_subsystem $ctrl_nqn -a -s SPDK05 -d SPDK_Controller5 -m 8
	sleep 1
	scripts/rpc.py nvmf_subsystem_add_ns $ctrl_nqn nvme0n2
	scripts/rpc.py nvmf_subsystem_add_ns $ctrl_nqn nvme1n2
	scripts/rpc.py nvmf_subsystem_add_listener $ctrl_nqn -t RDMA -f ipv4 -a 12.12.12.5 -s 5520
fi

scripts/rpc.py framework_set_scheduler dynamic

wait
