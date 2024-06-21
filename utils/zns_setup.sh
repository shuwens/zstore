#!/bin/bash
# https://github.com/Dantali0n/OpenCSD/blob/master/scripts/setup-zns-wd.sh

if [[ $# -lt 1 ]]; then
        echo "Usage: sudo $0 <ssd>"
        exit
fi
ssd=$1

# new ssd
# ns1: 4294967296
# ns2: 871878361088

# old ssd:
# ns1: 2147483648
# ns2: 871878361088

# Configure ZNS device namespaces

echo "# Total capacity"
nvme id-ctrl /dev/$ssd | grep tnvmcap

echo "# Delete namespaces"
nvme delete-ns /dev/$ssd -n 1
nvme delete-ns /dev/$ssd -n 2

# Create 4GB conventional namespace with either 512 or 4096 sectors
# sudo nvme create-ns /dev/$ssd -s 8388608 -c 8388608 -b 512 --csi=0
echo "# Create first conventional ns"
nvme create-ns /dev/$ssd -s 1048576 -c 1048576 -b 4096 --csi=0
# sudo nvme create-ns /dev/$ssd -s 104857 -c 104857 -b 4096 --csi=0
# sudo nvme create-ns /dev/$ssd -s 2147483648 -c 2147483648 -b 4096 --csi=0

# Create namespace that requires 4GB of conventional space for F2FS
# sudo nvme create-ns /dev/$ssd -s 212693155 -c 212693155 -b 4096 --csi=2
echo "# Create second zns ns"
nvme create-ns /dev/$ssd -s 212860928 -c 212860928 -b 4096 --csi=2

# Determine remaining capacity
nvme id-ctrl /dev/$ssd | grep unvmcap

echo "# Attach namespaces (only ns 2)"
nvme attach-ns /dev/$ssd -n 1 -c 0
nvme attach-ns /dev/$ssd -n 2 -c 0

# Create F2FS filesystem
# sudo mkfs.f2fs -f -m -c /dev/$ssdn2 /dev/nvme1n1

# Ensure queue scheduler is mq-deadline
# echo "mq-deadline" | sudo tee /sys/block/$ssdn2/queue/scheduler
