#!/usr/bin/env bash

# ./configure --disable-tests --disable-unit-tests --disable-examples --disable-apps --with-shared --with-rdma --prefix=$INSTALL_PATH --with-dpdk=$INSTALL_PATH &&

debug() {
  echo '===Building SPDK in debug mode...'
  ./configure --enable-debug --with-shared # --with-rdma
}

release() {
  echo '===Building SPDK in release mode...'
  ./configure --with-shared # --with-rdma
}

if [ $# -lt 1 ]; then
  echo "Usage: ./configure-spdk.sh [debug, release]"
  exit
fi

"$@"
