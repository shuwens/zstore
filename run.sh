#!/bin/bash
set -xeuo pipefail

if [[ $(hostname) != "zstore2" ]]; then
	echo "run from zstore2 "
	exit
fi

if [[ $# -lt 2 ]]; then
	echo "Usage: $0 <zone number> <queue depth>"
	exit
fi
znum=$1
qd=$2

zstore_dir=$(git rev-parse --show-toplevel)
source $zstore_dir/.env

# ssh -f zstore1 /home/shwsun/dev/zstore/build/mc_zstore 1 $znum $qd
# ssh -f zstore2 /home/shwsun/dev/zstore/build/mc_zstore 2 $znum $qd
ssh -f shwsun@$initiator1_ip /home/shwsun/dev/zstore/build/mc_zstore 1 $znum $qd
ssh -f shwsun@$initiator2_ip /home/shwsun/dev/zstore/build/mc_zstore 2 $znum $qd



