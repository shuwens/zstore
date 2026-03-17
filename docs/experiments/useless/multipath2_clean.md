[21:56] shwsun@shwsun-MBP.local:~/d/m/zstore (spdk %) | ./run.sh                                                                                                                         (base)
[INFO ../src/multipath.cc:107 main] Zstore start with current zone: 178
[2024-08-01 01:57:03.772646] Starting SPDK v24.05 git sha1 40a5c21 / DPDK 24.03.0 initialization...
[2024-08-01 01:57:03.772667] [ DPDK EAL parameters: zns_multipath_opts --no-shconf -c 0x1 --huge-unlink --no-telemetry --log-level=lib.eal:6 --log-level=lib.cryptodev:5 --log-level=user1:6 --iova-mode=pa --base-virtaddr=0x200000000000 --match-allocations --file-prefix=spdk_pid88039 ]
[2024-08-01 01:57:03.904859] app.c: 909:spdk_app_start: *NOTICE*: Total cores available: 1
[2024-08-01 01:57:03.938960] reactor.c: 937:reactor_run: *NOTICE*: Reactor started on core 0
[INFO ../src/multipath.cc:35 zns_multipath] Fn: zns_multipath for zstore1
[INFO ../src/multipath.cc:47 zns_multipath]
Starting with zone 178, queue depth 2, append times 1000
[INFO ../src/include/zns_device.h:223 zstore_qpair_setup] alloc qpair of queue size 2, request size 2
[INFO ../src/multipath.cc:63 zns_multipath] writing with z_append:
[DBG ../src/multipath.cc:64 zns_multipath] here
[INFO ../src/include/zns_device.h:481 __complete] setting current lba value: 93369305
[INFO ../src/multipath.cc:75 zns_multipath] zstore1 current lba for read is 93369305
[INFO ../src/multipath.cc:76 zns_multipath] read with z_append:
1934-th [INFO ../src/include/zns_device.h:251 zstore_qpair_teardown] disconnect and free qpair
[INFO ../src/multipath.cc:47 zns_multipath]
Starting with zone 179, queue depth 64, append times 1000
[INFO ../src/include/zns_device.h:223 zstore_qpair_setup] alloc qpair of queue size 64, request size 64
[INFO ../src/multipath.cc:63 zns_multipath] writing with z_append:
[DBG ../src/multipath.cc:64 zns_multipath] here
[INFO ../src/include/zns_device.h:481 __complete] setting current lba value: 93849561
[INFO ../src/multipath.cc:75 zns_multipath] zstore1 current lba for read is 93849561
[INFO ../src/multipath.cc:76 zns_multipath] read with z_append:
read
18[INFO ../src/include/zns_device.h:251 zstore_qpair_teardown] disconnect and free qpair
[INFO ../src/multipath.cc:87 zns_multipath] Test start finish
^C[INFO ../src/multipath.cc:107 main] Zstore start with current zone: 178
[2024-08-01 01:57:11.249369] Starting SPDK v24.05 git sha1 40a5c21 / DPDK 24.03.0 initialization...
[2024-08-01 01:57:11.249394] [ DPDK EAL parameters: zns_multipath_opts --no-shconf -c 0x1 --huge-unlink --no-telemetry --log-level=lib.eal:6 --log-level=lib.cryptodev:5 --log-level=user1:6 --iova-mode=pa --base-virtaddr=0x200000000000 --match-allocations --file-prefix=spdk_pid67325 ]
[2024-08-01 01:57:11.381620] app.c: 909:spdk_app_start: *NOTICE*: Total cores available: 1
[2024-08-01 01:57:11.414597] reactor.c: 937:reactor_run: *NOTICE*: Reactor started on core 0
[INFO ../src/multipath.cc:35 zns_multipath] Fn: zns_multipath for zstore4
[INFO ../src/multipath.cc:47 zns_multipath]
Starting with zone 178, queue depth 2, append times 1000
[INFO ../src/include/zns_device.h:223 zstore_qpair_setup] alloc qpair of queue size 2, request size 2
[INFO ../src/multipath.cc:63 zns_multipath] writing with z_append:
[DBG ../src/multipath.cc:64 zns_multipath] here
[INFO ../src/include/zns_device.h:481 __complete] setting current lba value: 93370305
[INFO ../src/multipath.cc:75 zns_multipath] zstore4 current lba for read is 93370305
[INFO ../src/multipath.cc:76 zns_multipath] read with z_append:
1934-th [INFO ../src/include/zns_device.h:251 zstore_qpair_teardown] disconnect and free qpair
[INFO ../src/multipath.cc:47 zns_multipath]
Starting with zone 179, queue depth 64, append times 1000
[INFO ../src/include/zns_device.h:223 zstore_qpair_setup] alloc qpair of queue size 64, request size 64
[INFO ../src/multipath.cc:63 zns_multipath] writing with z_append:
[DBG ../src/multipath.cc:64 zns_multipath] here
[INFO ../src/include/zns_device.h:481 __complete] setting current lba value: 93850561
[INFO ../src/multipath.cc:75 zns_multipath] zstore4 current lba for read is 93850561
[INFO ../src/multipath.cc:76 zns_multipath] read with z_append:
read
18[INFO ../src/include/zns_device.h:251 zstore_qpair_teardown] disconnect and free qpair
[INFO ../src/multipath.cc:87 zns_multipath] Test start finish
^C⏎
[21:57] shwsun@shwsun-MBP.local:~/d/m/zstore (spdk %) |                                                                                                                                  (base)
