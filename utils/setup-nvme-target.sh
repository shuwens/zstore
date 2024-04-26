#!/usr/bin/env bash

set -xeuo pipefail


zstore_dir=$(git rev-parse --show-toplevel)
source $zstore_dir/.env
source $zstore_dir/tools/utils.bash

echo "Running gateway on $gw_ip, remote on $host_ip"
# echo "Running with image $pool_name/$imgname"

kill_nvmf

cd $zstore_dir/spdk

./build/bin/nvmf_tgt -m '[0,1,2,3]' &
sleep 3
scripts/rpc.py nvmf_create_transport -t TCP -u 16384 -m 8 -c 8192
scripts/rpc.py bdev_nvme_attach_controller -b Nvme0 -t PCIe -a 0000:05:00.0
scripts/rpc.py nvmf_create_subsystem nqn.2016-06.io.spdk:cnode8 -a -s SPDK00000000000008 -d SPDK_Controller8-wlog
sleep 1
scripts/rpc.py nvmf_subsystem_add_listener nqn.2016-06.io.spdk:cnode8 -t tcp -a $host_ip -s 23789
scripts/rpc.py nvmf_subsystem_add_ns nqn.2016-06.io.spdk:cnode8 Nvme0n1
