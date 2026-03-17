#!/bin/bash
# InfiniBand/RDMA Verbs
modprobe ib_cm
modprobe ib_core
# Please note that ib_ucm does not exist in newer versions of the kernel and is not required.
# modprobe ib_ucm || true
modprobe ib_umad
modprobe ib_uverbs
modprobe iw_cm
modprobe rdma_cm
modprobe rdma_ucm

# connect x-3 pro
# modprobe mlx4_core
# modprobe mlx4_ib
# modprobe mlx4_en
# connect x-4
modprobe mlx5_core
modprobe mlx5_ib

modprobe nvme-rdma

if [ $(hostname) == "zstore1" ]; then
	sudo ifconfig enp1s0f0np0 12.12.12.1/24 up
	sudo ifconfig enp1s0f0np0 mtu 4200
elif [ "$HOSTNAME" == "zstore2" ]; then
	sudo ifconfig enp1s0f0np0  12.12.12.2/24 up
	sudo ifconfig enp1s0f0np0 mtu 4200
elif [ "$HOSTNAME" == "zstore3" ]; then
	sudo ifconfig enp1s0f0np0 12.12.12.3/24 up
	sudo ifconfig enp1s0f0np0 mtu 4200
elif [ "$HOSTNAME" == "zstore4" ]; then
	sudo ifconfig enp1s0 12.12.12.4/24 up
	sudo ifconfig enp1s0 mtu 4200
elif [ "$HOSTNAME" == "zstore5" ]; then
	sudo ifconfig enp1s0f0np0 12.12.12.5/24 up
	sudo ifconfig enp1s0f0np0 mtu 4200
elif [[ $(hostname) == "zstore6" ]]; then
	sudo ifconfig enp1s0 12.12.12.6/24 up
	sudo ifconfig enp1s0 mtu 4200
fi

