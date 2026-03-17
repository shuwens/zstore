#!/usr/bin/env bash
set -xeuo pipefail

zstore_dir=$(git rev-parse --show-toplevel)
source $zstore_dir/.env

cd $zstore_dir/subprojects/spdk

# fio --filename=/dev/nvme0n1 --direct=1 --rw=write --bs=128K --numjobs=4 --iodepth=32 \
# --size=100% --loops=2 --runtime=1200 --ramp_time=60 --time_based --ioengine=libaio  \
# -output-format=normal

fio --ioengine=psync --direct=1 --filename=/dev/$1 --rw=write \
	--group_reporting --zonemode=zbd --name=seqwrite \
	--offset_increment=1z --size=1z --numjobs=1 --job_max_open_zones=1
