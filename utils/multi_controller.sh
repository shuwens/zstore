#!/usr/bin/env bash
set -xeuo pipefail

# This

if [[ $# -lt 3 ]]; then
    echo "Usage: $0 <node> <network> <side>"
    echo "Example: $0 <1> <tcp> <target/initiator>"
    exit
fi
node=$1
transport=$2
side=$3
#ctrl_nqn="nqn.2024-04.io.zstore:cnode${node}"
ctrl1_nqn="nqn.2024-04.io.zstore:cnode1"
# ctrl2_nqn="nqn.2024-04.io.zstore:cnode2"

zstore_dir=$(git rev-parse --show-toplevel)
source $zstore_dir/.env

cd $zstore_dir/subprojects/spdk


HUGEMEM=4096 ./scripts/setup.sh

modprobe nvme
modprobe nvmet
# RDMA over fabric
modprobe ib_cm
modprobe ib_core
# Please note that ib_ucm does not exist in newer versions of the kernel and is not required.
modprobe ib_ucm || true
modprobe ib_umad
modprobe ib_uverbs
modprobe iw_cm

if [[ $transport == 'tcp' ]]; then
    modprobe nvmet_tcp
elif [[ $transport == 'rdma' ]]; then
    modprobe rdma_cm
    modprobe rdma_ucm

    modprobe mlx4_core
    modprobe mlx4_ib
    modprobe mlx4_en
    # ls /sys/class/infiniband/*/device/net
    # enp1s0/  enp1s0d1/
    ifconfig enp1s0 192.168.100.8 netmask 255.255.255.0 up
    ifconfig enp1s0d1 192.168.100.9 netmask 255.255.255.0 up
fi

if [[ $3 == "target" ]]; then
    if pidof nvmf_tgt; then
        scripts/rpc.py spdk_kill_instance SIGTERM >/dev/null || true
        scripts/rpc.py spdk_kill_instance SIGKILL >/dev/null || true
        pkill -f nvmf_tgt || true
        pkill -f reactor_0 || true
        sleep 3
    fi

    # TODO: FC transport support?

    # Configuring the SPDK NVMe over Fabrics Target
    ./build/bin/nvmf_tgt -m '[0,1,2,3]' &
    # ./build/bin/nvmf_tgt -m 0x3 &

    sleep 3

    # TODO: either use lspci to figure it out automatically or move to env
    # pci1=$(lspci -mm | perl -lane 'print @F[0] if /NVMe/' | head -1)
    # pci2=$(lspci -mm | perl -lane 'print @F[0] if /NVMe/' | tail -1)
    if [[ $node == '1' ]]; then
        pci1=05:00.0
        pci2=0b:00.0
        ipaddr=192.168.1.149
        ssd1=nvme0
        ssd2=nvme1
    elif [[ $node == '2' ]]; then
        pci1=05:00.0
        pci2=06:00.0
        ipaddr=192.168.1.121
        ssd1=nvme1
        ssd2=nvme2
    elif [[ $node == '3' ]]; then
        pci1=05:00.0
        pci2=06:00.0
    fi

    echo "## Attach local PCIe controllers"
    scripts/rpc.py bdev_nvme_attach_controller -b $ssd1 -t PCIe -a $pci1
    scripts/rpc.py bdev_nvme_attach_controller -b $ssd2 -t PCIe -a $pci2

    echo "## Create NVMe-oF transport (${transport})"
    if [[ $transport == 'tcp' ]]; then
        scripts/rpc.py nvmf_create_transport -t TCP -u 16384 -m 8 -c 8192
    elif [[ $transport == 'rdma' ]]; then
        scripts/rpc.py nvmf_create_transport -t RDMA -u 8192 -i 131072 -c 8192
    fi

    # -s? -d?
    # scripts/rpc.py nvmf_create_subsystem $ctrl_nqn -a -s SPDK00000000000001 -d SPDK_Controller1 -r
    echo "## Create a NVMe-oF subsystem/controller"
    scripts/rpc.py nvmf_create_subsystem $ctrl1_nqn -a -s SPDK00000000000001 -d SPDK_Controller1
    # scripts/rpc.py nvmf_create_subsystem $ctrl2_nqn -a -s SPDK00000000000002 -d SPDK_Controller2
    sleep 1

    echo "# Export -- add device to SPDK subsystem/controller"
    # scripts/rpc.py nvmf_subsystem_add_ns $ctrl1_nqn nvme0n1
    scripts/rpc.py nvmf_subsystem_add_ns $ctrl1_nqn ${ssd1}n2
    # scripts/rpc.py nvmf_subsystem_add_ns $ctrl1_nqn nvme1n1
    scripts/rpc.py nvmf_subsystem_add_ns $ctrl1_nqn ${ssd2}n2

    echo "## Setup NVMe-oF connection-listener"
    if [[ $transport == 'tcp' ]]; then
        scripts/rpc.py nvmf_subsystem_add_listener $ctrl1_nqn -t tcp -a $ipaddr -s 4420
        scripts/rpc.py nvmf_subsystem_add_listener $ctrl1_nqn -t tcp -a $ipaddr -s 5520
        scripts/rpc.py nvmf_subsystem_add_listener $ctrl1_nqn -t tcp -a $ipaddr -s 6620
        scripts/rpc.py nvmf_subsystem_add_listener $ctrl1_nqn -t tcp -a $ipaddr -s 7720
        scripts/rpc.py nvmf_subsystem_add_listener $ctrl1_nqn -t tcp -a $ipaddr -s 8820
        scripts/rpc.py nvmf_subsystem_add_listener $ctrl1_nqn -t tcp -a $ipaddr -s 9920
        # scripts/rpc.py nvmf_subsystem_add_listener $ctrl_nqn -t tcp -a 192.168.1.149 -s 6620
        # scripts/rpc.py nvmf_subsystem_add_listener $ctrl_nqn -t tcp -a 192.168.1.149 -s 7720
    elif [[ $transport == 'rdma' ]]; then
        scripts/rpc.py nvmf_subsystem_add_listener $ctrl1_nqn -t rdma -a 192.168.100.8 -s 4420
        scripts/rpc.py nvmf_subsystem_add_listener $ctrl1_nqn -t rdma -a 192.168.100.8 -s 5520
    fi

    wait

elif [[ $3 == "initiator" ]]; then
    # TODO: not fully working yet
    if pidof bdevperf; then
        pkill -f bdevperf || true
        sleep 3
    fi
    ./build/examples/bdevperf -m 0x4 -z -r /tmp/bdevperf.sock -q 128 -o 4096 -w verify -t 90 &> bdevperf.log &
    sleep 1
    ./scripts/rpc.py -s /tmp/bdevperf.sock bdev_nvme_set_options -r -1

    if [[ $node == '1' ]]; then
        ./scripts/rpc.py -s /tmp/bdevperf.sock bdev_nvme_attach_controller -b Nvme0 -t tcp -a 192.168.1.121 -s 4420 -f ipv4 -n $ctrl1_nqn -l -1 -o 10
        ./scripts/rpc.py -s /tmp/bdevperf.sock bdev_nvme_attach_controller -b Nvme0 -t tcp -a 192.168.1.121 -s 5520 -f ipv4 -n $ctrl1_nqn -x multipath -l -1 -o 10
   elif [[ $node == '2' ]]; then
        ./scripts/rpc.py -s /tmp/bdevperf.sock bdev_nvme_attach_controller -b Nvme0 -t tcp -a 192.168.1.121 -s 6620 -f ipv4 -n $ctrl1_nqn -l -1 -o 10
        ./scripts/rpc.py -s /tmp/bdevperf.sock bdev_nvme_attach_controller -b Nvme0 -t tcp -a 192.168.1.121 -s 7720 -f ipv4 -n $ctrl1_nqn -x multipath -l -1 -o 10
    elif [[ $node == '4' ]]; then
        ./scripts/rpc.py -s /tmp/bdevperf.sock bdev_nvme_attach_controller -b Nvme0 -t tcp -a 192.168.1.121 -s 8820 -f ipv4 -n $ctrl1_nqn -l -1 -o 10
        ./scripts/rpc.py -s /tmp/bdevperf.sock bdev_nvme_attach_controller -b Nvme0 -t tcp -a 192.168.1.121 -s 9920 -f ipv4 -n $ctrl1_nqn -x multipath -l -1 -o 10
    fi

    fi
