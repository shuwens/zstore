#!/usr/bin/env bash

# TODO: rdma
debug() {
  echo '===Building SPDK in debug mode...'
  ./configure --enable-debug --with-shared --without-xnvme
}

release() {
  echo '===Building SPDK in release mode...'
  ./configure --enable-debug --with-shared --without-xnvme
}

if [ $# -lt 1 ]; then
  echo "Usage: ./configure-spdk.sh [debug, release]"
  exit
fi

"$@"
