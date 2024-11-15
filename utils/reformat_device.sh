#!/bin/bash

echo "Harcoded for one drive, please check the script"

DEV=$1
#1TB
nvme delete-ns /dev/$DEV --namespace-id 0xffffffff
nvme create-ns /dev/$DEV --nsze 0x80000 --ncap 0x80000 --flbas 2 --nmic 1 --csi 0
nvme attach-ns /dev/$DEV --namespace-id 1 --controllers 0
nvme create-ns /dev/$DEV --nsze 0x1C400000 --ncap 0x1C400000 --flbas 2 --nmic 1 --csi 2
nvme attach-ns /dev/$DEV --namespace-id 2 --controllers 0


#4TB
# nvme delete-ns /dev/$DEV --namespace-id 0xffffffff
# nvme create-ns /dev/$DEV --nsze 0x100000 --ncap 0x100000 --flbas 0 --nmic 1 --csi 0
# nvme attach-ns /dev/$DEV --namespace-id 1 --controllers 0
# nvme create-ns /dev/$DEV --nsze 0x73400000 --ncap 0x73400000 --flbas 0 --nmic 1 --csi 2
# nvme attach-ns /dev/$DEV --namespace-id 2 --controllers 0
