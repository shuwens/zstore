#!/usr/bin/env bash

# TODO: rdma
debug() {
  echo '===Building SPDK in debug mode...'
  ./configure --enable-debug --with-shared 
}

release() {
  echo '===Building SPDK in release mode...'
  ./configure --with-shared 
}

if [ $# -lt 1 ]; then
  echo "Usage: ./configure-spdk.sh [debug, release]"
  exit
fi

"$@"
