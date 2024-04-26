#!/usr/bin/env bash

set -xeuo pipefail

# This script will build the nvme target on remote server (gundyr). 
#
# https://access.redhat.com/documentation/en-us/red_hat_enterprise_linux/9/html/managing_storage_devices/configuring-nvme-over-fabrics-using-nvme-tcp_managing-storage-devices

# if [[ $# -lt 2 ]]; then
#   echo "Usage: $0 <pool_name> <imgname>"
#   exit
# fi
#
# # pool must already exist
# pool_name=$1
# imgname=$2

zstore_dir=$(git rev-parse --show-toplevel)
source $zstore_dir/.env
source $zstore_dir/tools/utils.bash

echo "Running gateway on $gw_ip, remote on $remote_ip"
# echo "Running with image $pool_name/$imgname"

cd $zstore_dir/build-rel
which meson
meson compile
cd ..

# make sure image exists
rados -p $pool_name stat $imgname

kill_nvmf
launch_zstore_gw_background $rcache $wlog $((20 * 1024 * 1024 * 1024))
configure_nvmf_common $gw_ip
add_rbd_img $pool_name $imgname

wait
