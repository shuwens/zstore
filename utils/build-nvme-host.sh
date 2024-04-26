#!/usr/bin/env bash

set -xeuo pipefail

# This script will build the nvme host on remote server (gundyr).
#
# https://access.redhat.com/documentation/en-us/red_hat_enterprise_linux/9/html/managing_storage_devices/configuring-nvme-over-fabrics-using-nvme-tcp_managing-storage-devices



if [[ $# -lt 1 ]]; then
    echo "Usage: $0 <target_name>"
    exit
fi


if [ $1 == "default" ]; then
    echo -e "\tusing default nvme"
    # [16:22] gundyr | cat /etc/nvme/hostnqn
    # nqn.2014-08.org.nvmexpress:uuid:07baccdd-85ca-458e-b4cc-3a3af588fd8f
    # [16:28] gundyr | cat /etc/nvme/hostid
    # bad3d55d-8518-4d82-9443-66e44ae31f31

    # host nic: enp4s0

    # [16:34] gundyr | nmcli device show enp4s0
    # GENERAL.DEVICE:                         enp4s0
    # GENERAL.TYPE:                           ethernet
    # GENERAL.HWADDR:                         A8:A1:59:E7:C6:25
    # GENERAL.MTU:                            1500
    # GENERAL.STATE:                          10 (unmanaged)
    # GENERAL.CONNECTION:                     --
    # GENERAL.CON-PATH:                       --
    # WIRED-PROPERTIES.CARRIER:               on
    # IP4.ADDRESS[1]:                         192.168.1.156/24
    # IP4.GATEWAY:                            192.168.1.1
    # IP4.ROUTE[1]:                           dst = 0.0.0.0/0, nh = 192.168.1.1, mt = 0
    # IP4.ROUTE[2]:                           dst = 192.168.1.0/24, nh = 0.0.0.0, mt = 0
    # IP6.ADDRESS[1]:                         fe80::aaa1:59ff:fee7:c625/64
    # IP6.GATEWAY:                            --
    # IP6.ROUTE[1]:                           dst = fe80::/64, nh = ::, mt = 256
else
    echo "target is not configured:" $1
    exit
fi

zstore_dir=$(git rev-parse --show-toplevel)
source $zstore_dir/.env
# source $zstore_dir/tools/utils.bash

echo "Running gateway on $gw_ip, remote on $host_ip"
# echo "Running with image $pool_name/$imgname"

modprobe nvme_tcp

nvme discover --transport=tcp --traddr=$gw_ip --host-traddr=$host_ip --trsvcid=8009

echo "--transport=tcp --traddr=$gw_ip --host-traddr=$host_ip --trsvcid=8009" >> /etc/nvme/discovery.conf

nvme connect-all

nvme list-subsys

# cd $zstore_dir/build-rel
# which meson
# meson compile
# cd ..
#
# # make sure image exists
# rados -p $pool_name stat $imgname
#
# kill_nvmf
# launch_zstore_gw_background $rcache $wlog $((20 * 1024 * 1024 * 1024))
# configure_nvmf_common $gw_ip
# add_rbd_img $pool_name $imgname
#
# wait
