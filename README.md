# zstore

An distributed, eventually-consistent KV store built on ZNS SSDs

This project builds the following parts after compilation:
- `measurement`: we measure the append time, write time, read time, and zone
  full detection time.
- `multipath`: we use the multipath support from spdk to understand the
  reordering caused by running zone append in parallel
- `zstore`: the distributed, eventually-consistent object store built on ZNS SSDs


https://codecapsule.com/2014/10/18/implementing-a-key-value-store-part-7-optimizing-data-structures-for-ssds/


### RDMA

root@zstore1 /h/s/t/MLNX_OFED_LINUX-24.07-0.6.1.0-ubuntu24.04-x86_64 [1]# ./mlnxofedinstall --skip-unsupported-devices-check --force



### wrk workload

https://github.com/mikegreen/vault-benchmarking

### monitoring

sudo taskset -c --all-tasks -p (pgrep zstore)
htop -p (pgrep zstore) -H

