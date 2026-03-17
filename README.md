# ZStore: A fast, efficient, and strongly-consistent object store 

This is a object store prototype using ZNS SSDs.

## Building ZStore

To build ZStore, follow the instructions below (skip them if you are
already on one of the ZStore machines):
- dependencies: `make install-deps`
- boost C++ library: `make install-boost`

This project builds the following parts after compilation (they are done
automatically):
- `spdk`: path is `subproject/spdk` and it is currently pinned to the `v24.09`
  release.

In addition, the following parts need to be installed manually (skip
them if you are on ZStore machines):
- `wrk`: this is used for benchmarking the system, currently points to path `~/tools/wrk/wrk`.
- `zookeeper`: this is used for coordination when running multiple
  gateways, currently points to path `~/tools/apache-zookeeper-3.9.3-bin`.


## Running ZStore

To run ZStore, cd to the build-rel (for release build) or build-dbg (for
debug build) directory and run the following command:
- `meson compile`
- `sudo ./zstore <experiment> <option>`

If you see something like the following, then ZStore is running:
```
INFO ../src/zstore_controller.cc:121 SetParameters] Init Zstore for random read, starting from zone 1
[DBG ../src/zstore_controller.cc:189 SetParameters] Configuration: sector size 4096, context pool size 4096, targets 3, devices 2
[INFO ../src/zstore_controller.cc:228 SetParameters] RDMA server: listen ip 12.12.12.1 port 8982
[INFO ../src/device.cc:25 Init] ns: 1, Zone size: 524288, zone cap: 275712, num of zones: 904, md size 0
[INFO ../src/device.cc:25 Init] ns: 2, Zone size: 524288, zone cap: 275712, num of zones: 904, md size 0
[INFO ../src/device.cc:25 Init] ns: 1, Zone size: 524288, zone cap: 275712, num of zones: 3688, md size 0
[INFO ../src/device.cc:25 Init] ns: 2, Zone size: 524288, zone cap: 275712, num of zones: 3688, md size 0
[INFO ../src/device.cc:25 Init] ns: 1, Zone size: 524288, zone cap: 275712, num of zones: 904, md size 0
[INFO ../src/device.cc:25 Init] ns: 2, Zone size: 524288, zone cap: 275712, num of zones: 904, md size 0
[INFO ../src/zstore_controller.cc:547 initIoThread] IO thread name IoThread id 1 on core 1
[INFO ../src/zstore_controller.cc:795 PopulateMap] Populate Map(1,1): random read, starting from zone 2
[DBG ../src/zstore_controller.cc:823 PopulateMap] Populate Map for index 275457, current lba 1572864: zone 3 is full, moving to next zone
[DBG ../src/zstore_controller.cc:823 PopulateMap] Populate Map for index 550914, current lba 2097152: zone 4 is full, moving to next zone
[DBG ../src/zstore_controller.cc:823 PopulateMap] Populate Map for index 826371, current lba 2621440: zone 5 is full, moving to next zone
[INFO ../src/zstore_controller.cc:103 Init] ZstoreController Init finish
```
