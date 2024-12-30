#!/bin/bash
set -euo pipefail

# This script checks all non empty zones and finishes them

if [[ "$EUID" -ne 0 ]]; then
    echo "Error: This script must be run with sudo or as root."
    exit 1
fi

if [ "$HOSTNAME" == "zstore2" ]; then
  device1="nvme1n2"
  device2="nvme2n2"
elif [ "$HOSTNAME" == "zstore3" ]; then
  device1="nvme0n2"
  device2="nvme1n2"
elif [ "$HOSTNAME" == "zstore4" ]; then
  device1="nvme0n2"
  device2="nvme1n2"
elif [ "$HOSTNAME" == "zstore4" ]; then
  device1="nvme1n2"
  device2="nvme2n2"
fi

sudo nvme zns report-zone /dev/$device1 > device1.txt
sudo nvme zns report-zone /dev/$device2 > device2.txt

dev1_lbas=()
dev2_lbas=()

while IFS= read -r line; do
  word_count=$(echo "$line" | wc -w)
  # Skip lines with fewer than two words
  if [[ $word_count -lt 3 ]]; then
    continue
  fi

  # Extract the SLBA and State values using awk
  SLBA=$(echo "$line" | awk '{print $2}')
  STATE=$(echo "$line" | awk '{print $8}')

  # Check if State is not 0xe0 or 0x10
  if [[ "$STATE" != "0xe0" && "$STATE" != "0x10" ]]; then
    # Append the SLBA to the output file
    dev1_lbas+=("$SLBA")
  fi
done  < "device1.txt"

echo "Parsing complete. To finish SLBA values " "${dev1_lbas[@]}"

while IFS= read -r line; do
  word_count=$(echo "$line" | wc -w)
  # Skip lines with fewer than two words
  if [[ $word_count -lt 3 ]]; then
    continue
  fi

  # Extract the SLBA and State values using awk
  SLBA=$(echo "$line" | awk '{print $2}')
  STATE=$(echo "$line" | awk '{print $8}')

  # Check if State is not 0xe0 or 0x10
  if [[ "$STATE" != "0xe0" && "$STATE" != "0x10" ]]; then
    # Append the SLBA to the output file
    dev2_lbas+=("$SLBA")
  fi
done  < "device2.txt"

echo "Parsing complete. To finish SLBA values " "${dev2_lbas[@]}"

for lba in "${dev1_lbas[@]}"
do
  # echo "$lba"
  sudo nvme zns finish-zone /dev/$device1 -s $lba
done

for lba in "${dev2_lbas[@]}"
do
  # echo "$lba"
  sudo nvme zns finish-zone /dev/$device2 -s $lba
done
