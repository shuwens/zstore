#!/usr/bin/env bash
set -xeuo pipefail

# https://www.rdmamojo.com/2014/11/08/working-rdma-ubuntu/

zstore_dir=$(git rev-parse --show-toplevel)
source $zstore_dir/.env

sudo apt-get -y --force-yes install libibverbs1 ibverbs-utils \
	librdmacm1t64 rdmacm-utils libdapl2 ibsim-utils ibutils \
	libibmad5 libibumad3 ibverbs-providers \
	infiniband-diags mstflint opensm perftest srptools \
	libosmvendor5 libosmcomp5 libopensm9

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
