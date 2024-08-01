[22:11] shwsun@shwsun-MBP.local:~/d/m/zstore (spdk %) | ./run.sh                                                                                                                         (base)
[22:11] shwsun@shwsun-MBP.local:~/d/m/zstore (spdk %) | [INFO ../src/multipath.cc:108 main] Zstore start with current zone: 178                                                          (base)
[2024-08-01 02:11:45.268691] Starting SPDK v24.05 git sha1 40a5c21 / DPDK 24.03.0 initialization...
[2024-08-01 02:11:45.268714] [ DPDK EAL parameters: zns_multipath_opts --no-shconf -c 0x1 --huge-unlink --no-telemetry --log-level=lib.eal:6 --log-level=lib.cryptodev:5 --log-level=user1:6 --iova-mode=pa --base-virtaddr=0x200000000000 --match-allocations --file-prefix=spdk_pid69010 ]
[INFO ../src/multipath.cc:109 main] Zstore start with current zone: 178
[2024-08-01 02:11:45.384614] Starting SPDK v24.05 git sha1 40a5c21 / DPDK 24.03.0 initialization...
[2024-08-01 02:11:45.384637] [ DPDK EAL parameters: zns_multipath_opts --no-shconf -c 0x1 --huge-unlink --no-telemetry --log-level=lib.eal:6 --log-level=lib.cryptodev:5 --log-level=user1:6 --iova-mode=pa --base-virtaddr=0x200000000000 --match-allocations --file-prefix=spdk_pid90232 ]
[2024-08-01 02:11:45.401168] app.c: 909:spdk_app_start: *NOTICE*: Total cores available: 1
[2024-08-01 02:11:45.433728] reactor.c: 937:reactor_run: *NOTICE*: Reactor started on core 0
[2024-08-01 02:11:45.517001] app.c: 909:spdk_app_start: *NOTICE*: Total cores available: 1
[2024-08-01 02:11:45.549020] reactor.c: 937:reactor_run: *NOTICE*: Reactor started on core 0
[INFO ../src/multipath.cc:35 zns_multipath] Fn: zns_multipath for zstore4
[INFO ../src/multipath.cc:47 zns_multipath]
Starting with zone 178, queue depth 64, append times 1000
[INFO ../src/include/zns_device.h:223 zstore_qpair_setup] alloc qpair of queue size 64, request size 64
[INFO ../src/multipath.cc:63 zns_multipath] writing with z_append:
[DBG ../src/multipath.cc:64 zns_multipath] here
[INFO ../src/multipath.cc:65 zns_multipath] zstore4 append start lba 93323264
[INFO ../src/include/zns_device.h:481 __complete] setting current lba value: 93375305
[INFO ../src/multipath.cc:35 zns_multipath] Fn: zns_multipath for zstore1
[INFO ../src/multipath.cc:48 zns_multipath]
Starting with zone 178, queue depth 64, append times 1000
[INFO ../src/include/zns_device.h:223 zstore_qpair_setup] alloc qpair of queue size 64, request size 64
[INFO ../src/multipath.cc:64 zns_multipath] writing with z_append:
[DBG ../src/multipath.cc:65 zns_multipath] here
[INFO ../src/multipath.cc:66 zns_multipath] zstore1 append start lba 93323264
[INFO ../src/include/zns_device.h:481 __complete] setting current lba value: 93376080
[INFO ../src/multipath.cc:76 zns_multipath] zstore4 current lba for read is 93375305
[INFO ../src/multipath.cc:77 zns_multipath] read with z_append:
521-[INFO ../src/multipath.cc:77 zns_multipath] zstore1 current lba for read is 93376080
[INFO ../src/multipath.cc:78 zns_multipath] read with z_append:
th read zstore4:521
1855-th rea[INFO ../src/include/zns_device.h:251 zstore_qpair_teardown] disconnect and free qpair
[INFO ../src/multipath.cc:88 zns_multipath] Test start finish
[INFO ../src/include/zns_device.h:251 zstore_qpair_teardown] disconnect and free qpair
[INFO ../src/multipath.cc:89 zns_multipath] Test start finish
