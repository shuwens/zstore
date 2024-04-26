#!/usr/bin/env bash

set -xeuo pipefail

# https://github.com/CCI-MOC/lsvd-rbd/blob/23ee44eb1ce5dc170ebd17d6ce038f8f5ece1535/qemu/qemu-client.bash#L8
# https://github.com/CCI-MOC/lsvd-rbd/blob/23ee44eb1ce5dc170ebd17d6ce038f8f5ece1535/tools/setup-wlog.bash

zstore_dir=$(git rev-parse --show-toplevel)
source $zstore_dir/.env
# source $zstore_dir/tools/utils.bash

echo "Running gateway on $gw_ip, remote on $host_ip"
# echo "Running with image $pool_name/$imgname"

modprobe nvme-fabrics
# nvme disconnect -n nqn.2016-06.io.spdk:cnode1
# gw_ip=${gw_ip:-10.1.0.5}
# nvme connect -t tcp --traddr $gw_ip -s 9922 -n nqn.2024-04.io.spdk:cnode8
# nvme connect -t tcp --traddr $gw_ip -s 9922 -n nqn.2016-06.io.spdk:cnode8
nvme connect -t tcp --traddr $host_ip -s 23789 -n nqn.2016-06.io.spdk:cnode8
sleep 2
nvme list
dev_name=$(nvme list | perl -lane 'print @F[0] if /SPDK/')
printf "Using device $dev_name\n"

