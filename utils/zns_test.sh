#!/usr/bin/env bash
set -xeuo pipefail

if [[ $# -lt 1 ]]; then
	echo "Usage: $0 <node>"
	# echo "Example: $0 zstore1 tcp"
	exit
fi
node=$1

zstore_dir=$(git rev-parse --show-toplevel)
source $zstore_dir/.env

if [[ $node == 'zstore1' ]]; then
	nvme1=
	nvme2=0b:00.0
elif [[ $node == 'zstore2' ]]; then
	pci1=05:00.0
	pci1=06:00.0
fi



nvme zns id-ctrl /dev/nvme1n1
