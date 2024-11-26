[INFO ../src/measurement.cc:175 main] Zstore start with current zone: 178
[2024-07-31 21:05:20.188078] Starting SPDK v24.05 git sha1 40a5c21 / DPDK 24.03.0 initialization...
[2024-07-31 21:05:20.188104] [ DPDK EAL parameters: zns_measurement_opts --no-shconf -c 0x1 --huge-unlink --no-telemetry --log-level=lib.eal:6 --log-level=lib.cryptodev:5 --log-level=user1:6 --iova-mode=pa --base-virtaddr=0x200000000000 --match-allocations --file-prefix=spdk_pid76187 ]
[2024-07-31 21:05:20.320424] app.c: 909:spdk_app_start: *NOTICE*: Total cores available: 1
[2024-07-31 21:05:20.353128] reactor.c: 937:reactor_run: *NOTICE*: Reactor started on core 0
[INFO ../src/measurement.cc:43 zns_measure] Fn: zns_measure

[INFO ../src/measurement.cc:52 zns_measure]
Starting measurment with queue depth 2, append times 1000

[INFO ../src/include/zns_device.h:223 zstore_qpair_setup] alloc qpair of queue size 2, request size 2
[INFO ../src/measurement.cc:101 zns_measure] writing with z_append:
[DBG ../src/measurement.cc:102 zns_measure] here
[INFO ../src/include/zns_device.h:481 __complete] setting current lba value: 93361296
[INFO ../src/measurement.cc:127 zns_measure] qd 2, append: mean 136.528 us, std 12.351729271644626
[INFO ../src/measurement.cc:130 zns_measure] current lba for read is 93361296
[INFO ../src/measurement.cc:131 zns_measure] read with z_append:
[INFO ../src/measurement.cc:149 zns_measure] qd 2, read: mean 158.877 us, std 25.666200945991154
[INFO ../src/include/zns_device.h:251 zstore_qpair_teardown] disconnect and free qpair
[INFO ../src/measurement.cc:52 zns_measure]
Starting measurment with queue depth 4, append times 1000

[INFO ../src/include/zns_device.h:223 zstore_qpair_setup] alloc qpair of queue size 4, request size 4
[INFO ../src/measurement.cc:101 zns_measure] writing with z_append:
[DBG ../src/measurement.cc:102 zns_measure] here
[INFO ../src/include/zns_device.h:481 __complete] setting current lba value: 93362296
[INFO ../src/measurement.cc:127 zns_measure] qd 4, append: mean 130.498 us, std 7.838494498307783
[INFO ../src/measurement.cc:130 zns_measure] current lba for read is 93362296
[INFO ../src/measurement.cc:131 zns_measure] read with z_append:
[INFO ../src/measurement.cc:149 zns_measure] qd 4, read: mean 159.204 us, std 28.180461032424542
[INFO ../src/include/zns_device.h:251 zstore_qpair_teardown] disconnect and free qpair
[INFO ../src/measurement.cc:52 zns_measure]
Starting measurment with queue depth 8, append times 1000

[INFO ../src/include/zns_device.h:223 zstore_qpair_setup] alloc qpair of queue size 8, request size 8
[INFO ../src/measurement.cc:101 zns_measure] writing with z_append:
[DBG ../src/measurement.cc:102 zns_measure] here
[INFO ../src/include/zns_device.h:481 __complete] setting current lba value: 93363296
[INFO ../src/measurement.cc:127 zns_measure] qd 8, append: mean 131.875 us, std 8.066806989137644
[INFO ../src/measurement.cc:130 zns_measure] current lba for read is 93363296
[INFO ../src/measurement.cc:131 zns_measure] read with z_append:
[INFO ../src/measurement.cc:149 zns_measure] qd 8, read: mean 160.011 us, std 25.496801348404514
[INFO ../src/include/zns_device.h:251 zstore_qpair_teardown] disconnect and free qpair
[INFO ../src/measurement.cc:52 zns_measure]
Starting measurment with queue depth 16, append times 1000

[INFO ../src/include/zns_device.h:223 zstore_qpair_setup] alloc qpair of queue size 16, request size 16
[INFO ../src/measurement.cc:101 zns_measure] writing with z_append:
[DBG ../src/measurement.cc:102 zns_measure] here
[INFO ../src/include/zns_device.h:481 __complete] setting current lba value: 93364296
[INFO ../src/measurement.cc:127 zns_measure] qd 16, append: mean 150.216 us, std 12.125235832757935
[INFO ../src/measurement.cc:130 zns_measure] current lba for read is 93364296
[INFO ../src/measurement.cc:131 zns_measure] read with z_append:
[INFO ../src/measurement.cc:149 zns_measure] qd 16, read: mean 159.77 us, std 24.567846873505122
[INFO ../src/include/zns_device.h:251 zstore_qpair_teardown] disconnect and free qpair
[INFO ../src/measurement.cc:52 zns_measure]
Starting measurment with queue depth 32, append times 1000

[INFO ../src/include/zns_device.h:223 zstore_qpair_setup] alloc qpair of queue size 32, request size 32
[INFO ../src/measurement.cc:101 zns_measure] writing with z_append:
[DBG ../src/measurement.cc:102 zns_measure] here
[INFO ../src/include/zns_device.h:481 __complete] setting current lba value: 93365296
[INFO ../src/measurement.cc:127 zns_measure] qd 32, append: mean 131.512 us, std 7.983098145456944
[INFO ../src/measurement.cc:130 zns_measure] current lba for read is 93365296
[INFO ../src/measurement.cc:131 zns_measure] read with z_append:
[INFO ../src/measurement.cc:149 zns_measure] qd 32, read: mean 160.397 us, std 28.151756446090573
[INFO ../src/include/zns_device.h:251 zstore_qpair_teardown] disconnect and free qpair
[INFO ../src/measurement.cc:52 zns_measure]
Starting measurment with queue depth 64, append times 1000

[INFO ../src/include/zns_device.h:223 zstore_qpair_setup] alloc qpair of queue size 64, request size 64
[INFO ../src/measurement.cc:101 zns_measure] writing with z_append:
[DBG ../src/measurement.cc:102 zns_measure] here
[INFO ../src/include/zns_device.h:481 __complete] setting current lba value: 93366296
[INFO ../src/measurement.cc:127 zns_measure] qd 64, append: mean 129.936 us, std 8.21157134779943
[INFO ../src/measurement.cc:130 zns_measure] current lba for read is 93366296
[INFO ../src/measurement.cc:131 zns_measure] read with z_append:
[INFO ../src/measurement.cc:149 zns_measure] qd 64, read: mean 152.553 us, std 29.575381502188616
[INFO ../src/include/zns_device.h:251 zstore_qpair_teardown] disconnect and free qpair
[INFO ../src/measurement.cc:155 zns_measure] Test start finish
^Ctest_abort
[INFO ../src/measurement.cc:198 main] zstore exits gracefully
[21:05] zstore1:build (spdk *%) |
[21:06] zstore1:build (spdk *%) | nvim ../experiments/spdk_measure2.md
[21:06] zstore1:build (spdk *%) |



