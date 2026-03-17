#!/usr/bin/env bash
set -xeuo pipefail
# https://support.hpe.com/hpesc/public/docDisplay?docId=a00071081en_us&page=GUID-617F4C95-AA58-43F7-B524-78C6535747AC.html

# https://www.rdmamojo.com/2014/11/08/working-rdma-ubuntu/
# https://gist.github.com/hokiegeek2/96c07d4b0de71a641c45fcf105a2fce0
# https://kb.linbit.com/enabling-rdma-support-in-linux
# https://gist.github.com/hookenz/61af2a46aa0004d856e48c7afbb79e5c

zstore_dir=$(git rev-parse --show-toplevel)
source $zstore_dir/.env

sudo apt-get -y --force-yes install libibverbs1 ibverbs-utils \
	librdmacm1t64 rdmacm-utils libdapl2 ibsim-utils ibutils \
	libibmad5 libibumad3 ibverbs-providers \
	infiniband-diags mstflint opensm perftest srptools \
	libosmvendor5 libosmcomp5 libopensm9

sudo apt-get install -y libmlx4-1 infiniband-diags ibutils ibverbs-utils rdmacm-utils perftest
sudo apt-get install -y tgt
# sudo apt-get install -y targetcli
# sudo apt-get install -y open-iscsi-utils open-iscsi


# RDMA stack modules
sudo modprobe rdma_cm
sudo modprobe ib_uverbs
sudo modprobe rdma_ucm
sudo modprobe ib_ucm || true
sudo modprobe ib_umad
sudo modprobe ib_ipoib
# RDMA devices low-level drivers
sudo modprobe mlx4_ib
sudo modprobe mlx4_en
sudo modprobe iw_cxgb3 || true
sudo modprobe iw_cxgb4
sudo modprobe iw_nes || true
sudo modprobe iw_c2 || true

sudo modprobe mlx4_core
sudo modprobe mlx4_ib
sudo modprobe mlx4_en

sudo opensm
