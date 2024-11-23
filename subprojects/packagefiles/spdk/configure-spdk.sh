#!/usr/bin/env bash

# ./configure  --prefix=$INSTALL_PATH --with-dpdk=$INSTALL_PATH &&

debug() {
  echo '===Building SPDK in debug mode...'
  ./configure --enable-debug --disable-tests --disable-unit-tests --disable-examples --with-shared --with-rdma —enable-lto
}

release() {
  echo '===Building SPDK in release mode...'
  ./configure --disable-tests --disable-unit-tests --disable-examples --with-shared --with-rdma —enable-lto
}

if [ $# -lt 1 ]; then
  echo "Usage: ./configure-spdk.sh [debug, release]"
  exit
fi

"$@"
