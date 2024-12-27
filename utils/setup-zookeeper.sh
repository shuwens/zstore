#!/usr/bin/env bash
set -xeuo pipefail

# https://github.com/cocalele/PureFlash/blob/2b01e07c26d0193f404924052fc3e4e939027e83/build_and_run.txt#L2

zstore_dir=$(git rev-parse --show-toplevel)
source $zstore_dir/.env
cd 

sudo snap install ant --classic -y
sudo apt install openjdk-8-jdk-headless maven -7
sudo apt-get install libcppunit-dev autoconf automake libtool -y

mkdir -p tools
cd tools 
wget https://dlcdn.apache.org/zookeeper/zookeeper-3.9.3/apache-zookeeper-3.9.3.tar.gz
tar -xvf apache-zookeeper-3.9.3.tar.gz

# jute
cd apache-zookeeper-3.9.3/zookeeper-jute/; mvn compile

# c client
cd /home/shwsun/tools/apache-zookeeper-3.9.3/zookeeper-client/zookeeper-client-c
autoreconf -if
./configure; make; sudo make install
