#!/bin/bash
# https://github.com/stonet-research/systor-confznsplusplus-artifact/blob/f6e8151c187d20ae51f1ffa5141de4c592af5289/benchmarks/scheduler-benchmarks/macrobenchmarks/rocksdb-with-interference/namespaces/create_three_namespaces#L7

echo "!!!!!!!!!!!!!!!!!!!"
echo "Harcoded for one drive, please check the script"

DEV=$1

randwritable_size=2147483648
nr_zones=904
bs=4096

tnvmcap=$(sudo nvme id-ctrl /dev/$DEV | grep tnvmcap | awk '{print $3}')
randwriteable_pages=$((${randwritable_size} / ${bs}))
zonesize=$(($tnvmcap / $nr_zones / $bs))

# Count=sudo nvme list-ns -a /dev/nvme0
sudo nvme list-ns -a /dev/nvme0 | awk -F: '{print $2}' | while read line; do echo $(($line)) | xargs sudo nvme delete-ns /dev/$DEV -n ; done 

# Random writeable namespaces
sudo nvme create-ns /dev/$DEV -s ${randwriteable_pages} -c ${randwriteable_pages} -b $bs --csi=0
sudo nvme attach-ns /dev/$DEV -c 0 -n 1

# ZNS namespaces
sudo nvme create-ns /dev/$DEV -s $((${zonesize}*100)) -c $((${zonesize}*100)) -b $bs --csi=2
sudo nvme attach-ns /dev/$DEV -c 0 -n 2
# sudo nvme create-ns /dev/$DEV -s $((${zonesize}*100)) -c $((${zonesize}*100)) -b $bs --csi=2
# sudo nvme attach-ns /dev/$DEV -c 0 -n 3
# sudo nvme create-ns /dev/$DEV -s $((${zonesize}*($nr_zones - 200 - 4))) -c $((${zonesize}*($nr_zones - 200 - 4))) -b $bs --csi=2
# sudo nvme attach-ns /dev/$DEV -c 0 -n 4
