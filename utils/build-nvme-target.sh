#!/usr/bin/env bash

set -xeuo pipefail

zstore_dir=$(git rev-parse --show-toplevel)
source $zstore_dir/.env

cd $zstore_dir/subprojects/spdk

# sudo scripts/pkgdep.sh --rdma
#
# ./configure --with-rdma <other config parameters>
# make

modprobe ib_cm
modprobe ib_core
# Please note that ib_ucm does not exist in newer versions of the kernel and is not required.
modprobe ib_ucm || true
modprobe ib_umad
modprobe ib_uverbs
modprobe iw_cm
modprobe rdma_cm
modprobe rdma_ucm



