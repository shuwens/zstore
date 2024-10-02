[21:33] shwsun@shwsun-MBP.local:~/d/m/zstore (spdk) | ./run.sh                                                                                                                           (base)
[INFO ../src/multipath.cc:107 main] Zstore start with current zone: 178
[2024-08-01 01:33:06.628616] Starting SPDK v24.05 git sha1 40a5c21 / DPDK 24.03.0 initialization...
[2024-08-01 01:33:06.628637] [ DPDK EAL parameters: zns_multipath_opts --no-shconf -c 0x1 --huge-unlink --no-telemetry --log-level=lib.eal:6 --log-level=lib.cryptodev:5 --log-level=user1:6 --iova-mode=pa --base-virtaddr=0x200000000000 --match-allocations --file-prefix=spdk_pid85102 ]
[2024-08-01 01:33:06.760865] app.c: 909:spdk_app_start: *NOTICE*: Total cores available: 1
[2024-08-01 01:33:06.793893] reactor.c: 937:reactor_run: *NOTICE*: Reactor started on core 0
[INFO ../src/multipath.cc:34 zns_multipath] Fn: zns_multipath
[INFO ../src/multipath.cc:46 zns_multipath]
Starting with zone 178, queue depth 2, append times 1000
[INFO ../src/include/zns_device.h:223 zstore_qpair_setup] alloc qpair of queue size 2, request size 2
[INFO ../src/multipath.cc:62 zns_multipath] writing with z_append:
[DBG ../src/multipath.cc:63 zns_multipath] here
[INFO ../src/include/zns_device.h:481 __complete] setting current lba value: 93367305
[INFO ../src/multipath.cc:75 zns_multipath] current lba for read is 93367305
[INFO ../src/multipath.cc:76 zns_multipath] read with z_append:
1934-th [INFO ../src/include/zns_device.h:251 zstore_qpair_teardown] disconnect and free qpair
[INFO ../src/multipath.cc:46 zns_multipath]
Starting with zone 179, queue depth 64, append times 1000
[INFO ../src/include/zns_device.h:223 zstore_qpair_setup] alloc qpair of queue size 64, request size 64
[INFO ../src/multipath.cc:62 zns_multipath] writing with z_append:
[DBG ../src/multipath.cc:63 zns_multipath] here
[INFO ../src/include/zns_device.h:481 __complete] setting current lba value: 93847561
[INFO ../src/multipath.cc:75 zns_multipath] current lba for read is 93847561
[INFO ../src/multipath.cc:76 zns_multipath] read with z_append:
read
18[INFO ../src/include/zns_device.h:251 zstore_qpair_teardown] disconnect and free qpair
[INFO ../src/multipath.cc:87 zns_multipath] Test start finish
^C[INFO ../src/multipath.cc:107 main] Zstore start with current zone: 178
[2024-08-01 01:33:12.242632] Starting SPDK v24.05 git sha1 40a5c21 / DPDK 24.03.0 initialization...
[2024-08-01 01:33:12.242656] [ DPDK EAL parameters: zns_multipath_opts --no-shconf -c 0x1 --huge-unlink --no-telemetry --log-level=lib.eal:6 --log-level=lib.cryptodev:5 --log-level=user1:6 --iova-mode=pa --base-virtaddr=0x200000000000 --match-allocations --file-prefix=spdk_pid66225 ]
[2024-08-01 01:33:12.375003] app.c: 909:spdk_app_start: *NOTICE*: Total cores available: 1
[2024-08-01 01:33:12.410672] reactor.c: 937:reactor_run: *NOTICE*: Reactor started on core 0
[INFO ../src/multipath.cc:34 zns_multipath] Fn: zns_multipath
[INFO ../src/multipath.cc:46 zns_multipath]
Starting with zone 178, queue depth 2, append times 1000
[INFO ../src/include/zns_device.h:223 zstore_qpair_setup] alloc qpair of queue size 2, request size 2
[INFO ../src/multipath.cc:62 zns_multipath] writing with z_append:
[DBG ../src/multipath.cc:63 zns_multipath] here
[INFO ../src/include/zns_device.h:481 __complete] setting current lba value: 93368305
[INFO ../src/multipath.cc:75 zns_multipath] current lba for read is 93368305
[INFO ../src/multipath.cc:76 zns_multipath] read with z_append:
1934-th [INFO ../src/include/zns_device.h:251 zstore_qpair_teardown] disconnect and free qpair
[INFO ../src/multipath.cc:46 zns_multipath]
Starting with zone 179, queue depth 64, append times 1000
[INFO ../src/include/zns_device.h:223 zstore_qpair_setup] alloc qpair of queue size 64, request size 64
[INFO ../src/multipath.cc:62 zns_multipath] writing with z_append:
[DBG ../src/multipath.cc:63 zns_multipath] here
[INFO ../src/include/zns_device.h:481 __complete] setting current lba value: 93848561
[INFO ../src/multipath.cc:75 zns_multipath] current lba for read is 93848561
[INFO ../src/multipath.cc:76 zns_multipath] read with z_append:
read
18[INFO ../src/include/zns_device.h:251 zstore_qpair_teardown] disconnect and free qpair
[INFO ../src/multipath.cc:87 zns_multipath] Test start finish
^C⏎
[21:33] shwsun@shwsun-MBP.local:~/d/m/zstore (spdk) |                                                                                                                                    (base)
