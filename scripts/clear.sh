#!/usr/bin/env bash
set -xeuo pipefail

zstore_dir=$(git rev-parse --show-toplevel)
source $zstore_dir/.env

cd $zstore_dir/subprojects/spdk

if [ "$HOSTNAME" == "zstore1" ]; then
        pci1=05:00.0
        pci2=0b:00.0
        ctrl_nqn="nqn.2024-04.io.zstore1:cnode1"
elif [ "$HOSTNAME" == "zstore2" ]; then
        pci1=05:00.0
        pci2=06:00.0
        ctrl_nqn="nqn.2024-04.io.zstore2:cnode1"
elif [ "$HOSTNAME" == "zstore3" ]; then
        pci1=04:00.0
        pci2=06:00.0
        ctrl_nqn="nqn.2024-04.io.zstore3:cnode1"
elif [ "$HOSTNAME" == "zstore4" ]; then
        pci1=05:00.0
        pci2=0b:00.0
        ctrl_nqn="nqn.2024-04.io.zstore4:cnode1"
fi

scripts/setup.sh reset

