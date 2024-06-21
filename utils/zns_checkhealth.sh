#!/bin/bash
# https://stackoverflow.com/questions/27368996/bash-how-to-check-if-number-of-arguments-exceed-a-certain-number

# if [[ $0 != "sudo" ]]; then
#         echo "Run this script with sudo"
#         exit
# fi

if [[ $# -lt 1 ]]; then
# if (( $# > 2 )); then
        sudo nvme list
        echo "Usage: $0 <ssd>"
        exit
fi
ssd=$1

echo "# Report zones"
sudo nvme zns report-zones /dev/$ssd -d 10
echo "# State code: empty(1), full(e)"

if [[ $2 != "reset" ]]; then
        echo "Usage: we only support reseting zns drives"
        exit
fi
cmd="reset"

echo "# Reset all zones"
sudo nvme zns reset-zone /dev/$ssd -a

echo "# Report zones again"
sudo nvme zns report-zones /dev/$ssd -d 10


