#!/usr/bin/env bash

debug() {
  echo '===Building SPDK in debug mode...'
  ./configure --enable-debug --with-shared --with-rdma
}

release() {
  echo '===Building SPDK in release mode...'
  ./configure --with-shared --with-rdma
}

if [ $# -lt 1 ]; then
  echo "Usage: ./configure-spdk.sh [debug, release]"
  exit
fi

"$@"
