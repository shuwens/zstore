#!/usr/bin/env bash
set -xeuo pipefail
# https://support.hpe.com/hpesc/public/docDisplay?docId=a00071081en_us&page=GUID-617F4C95-AA58-43F7-B524-78C6535747AC.html

# https://www.rdmamojo.com/2014/11/08/working-rdma-ubuntu/
# https://gist.github.com/hokiegeek2/96c07d4b0de71a641c45fcf105a2fce0
# https://kb.linbit.com/enabling-rdma-support-in-linux
# https://gist.github.com/hookenz/61af2a46aa0004d856e48c7afbb79e5c

zstore_dir=$(git rev-parse --show-toplevel)
source $zstore_dir/.env

apt-get -y install -f build-essential pkg-config vlan automake autoconf dkms git
apt-get -y install \
	libibverbs* librdma* libibmad.* libibumad*
apt-get -y install \
	libtool ibutils ibverbs-utils \
	rdmacm-utils infiniband-diags perftest librdmacm-dev \
	libibverbs-dev numactl libnuma-dev libnl-3-200 \
	libnl-route-3-200 libnl-route-3-dev libnl-utils 
# libibcm.* 


cat > /etc/udev/rules.d/40-rdma.rules <<EOF

KERNEL=="umad*", NAME="infiniband/%k"
KERNEL=="issm*", NAME="infiniband/%k"
KERNEL=="ucm*", NAME="infiniband/%k", MODE="0666"
KERNEL=="uverbs*", NAME="infiniband/%k", MODE="0666"
KERNEL=="ucma", NAME="infiniband/%k", MODE="0666"
KERNEL=="rdma_cm", NAME="infiniband/%k", MODE="0666"

EOF


cat > /etc/security/limits.conf <<EOF

* soft memlock unlimited
* hard memlock unlimited
root soft memlock unlimited
root hard memlock unlimited

EOF

modprobe rdma_cm
modprobe ib_uverbs
modprobe rdma_ucm
modprobe ib_ucm
modprobe ib_umad


