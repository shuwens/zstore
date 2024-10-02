[INFO ../src/measurement.cc:163 main] Zstore start with current zone: 176
[2024-07-31 15:44:31.322950] Starting SPDK v24.05 git sha1 40a5c21 / DPDK 24.03.0 initialization...
[2024-07-31 15:44:31.322973] [ DPDK EAL parameters: zns_measurement_opts --no-shconf -c 0x1 --huge-unlink --no-telemetry --log-level=lib.eal:6 --log-level=lib.cryptodev:5 --log-level=user1:6 --iova-mode=pa --base-virtaddr=0x200000000000 --match-allocations --file-prefix=spdk_pid45940 ]
[2024-07-31 15:44:31.455562] app.c: 909:spdk_app_start: *NOTICE*: Total cores available: 1
[2024-07-31 15:44:31.487948] reactor.c: 937:reactor_run: *NOTICE*: Reactor started on core 0
[INFO ../src/measurement.cc:42 zns_measure] Fn: zns_measure

[INFO ../src/measurement.cc:50 zns_measure] Starting measurment with queue depth 2
[INFO ../src/include/zns_device.h:220 zstore_qpair_setup] alloc qpair of queue size 2, request size 2
[INFO ../src/include/zns_device.h:974 z_get_zone_head] Zone Capacity (in number of LBAs) 275712, Zone Start LBA 0, Write Pointer (LBA) 0
[INFO ../src/include/zns_device.h:977 z_get_zone_head] zone head: 0
[INFO ../src/measurement.cc:88 zns_measure] current zone: 176, current lba 0, head 0
[INFO ../src/measurement.cc:97 zns_measure] writing with z_append:
[DBG ../src/measurement.cc:98 zns_measure] here
[INFO ../src/measurement.cc:120 zns_measure] qd 2, averge append 144.90875 us
[INFO ../src/measurement.cc:124 zns_measure] read with z_append:
[INFO ../src/measurement.cc:138 zns_measure] qd 2, averge read 137.847 us
[INFO ../src/include/zns_device.h:243 zstore_qpair_teardown] disconnect and free qpair
[INFO ../src/measurement.cc:50 zns_measure] Starting measurment with queue depth 4
[INFO ../src/include/zns_device.h:220 zstore_qpair_setup] alloc qpair of queue size 4, request size 4
[INFO ../src/include/zns_device.h:974 z_get_zone_head] Zone Capacity (in number of LBAs) 275712, Zone Start LBA 0, Write Pointer (LBA) 0
[INFO ../src/include/zns_device.h:977 z_get_zone_head] zone head: 0
[INFO ../src/measurement.cc:88 zns_measure] current zone: 176, current lba 91758036, head 0
[INFO ../src/measurement.cc:97 zns_measure] writing with z_append:
[DBG ../src/measurement.cc:98 zns_measure] here
[INFO ../src/measurement.cc:120 zns_measure] qd 4, averge append 149.4399375 us
[INFO ../src/measurement.cc:124 zns_measure] read with z_append:
[INFO ../src/measurement.cc:138 zns_measure] qd 4, averge read 143.228125 us
[INFO ../src/include/zns_device.h:243 zstore_qpair_teardown] disconnect and free qpair
[INFO ../src/measurement.cc:50 zns_measure] Starting measurment with queue depth 8
[INFO ../src/include/zns_device.h:220 zstore_qpair_setup] alloc qpair of queue size 8, request size 8
[INFO ../src/include/zns_device.h:974 z_get_zone_head] Zone Capacity (in number of LBAs) 275712, Zone Start LBA 0, Write Pointer (LBA) 0
[INFO ../src/include/zns_device.h:977 z_get_zone_head] zone head: 0
[INFO ../src/measurement.cc:88 zns_measure] current zone: 176, current lba 91758036, head 0
[INFO ../src/measurement.cc:97 zns_measure] writing with z_append:
[DBG ../src/measurement.cc:98 zns_measure] here
[INFO ../src/measurement.cc:120 zns_measure] qd 8, averge append 138.597625 us
[INFO ../src/measurement.cc:124 zns_measure] read with z_append:
[INFO ../src/measurement.cc:138 zns_measure] qd 8, averge read 144.0721875 us
[INFO ../src/include/zns_device.h:243 zstore_qpair_teardown] disconnect and free qpair
[INFO ../src/measurement.cc:50 zns_measure] Starting measurment with queue depth 16
[INFO ../src/include/zns_device.h:220 zstore_qpair_setup] alloc qpair of queue size 16, request size 16
[INFO ../src/include/zns_device.h:974 z_get_zone_head] Zone Capacity (in number of LBAs) 275712, Zone Start LBA 0, Write Pointer (LBA) 0
[INFO ../src/include/zns_device.h:977 z_get_zone_head] zone head: 0
[INFO ../src/measurement.cc:88 zns_measure] current zone: 176, current lba 91758036, head 0
[INFO ../src/measurement.cc:97 zns_measure] writing with z_append:
[DBG ../src/measurement.cc:98 zns_measure] here
[INFO ../src/measurement.cc:120 zns_measure] qd 16, averge append 149.466 us
[INFO ../src/measurement.cc:124 zns_measure] read with z_append:
[INFO ../src/measurement.cc:138 zns_measure] qd 16, averge read 142.7880625 us
[INFO ../src/include/zns_device.h:243 zstore_qpair_teardown] disconnect and free qpair
[INFO ../src/measurement.cc:50 zns_measure] Starting measurment with queue depth 32
[INFO ../src/include/zns_device.h:220 zstore_qpair_setup] alloc qpair of queue size 32, request size 32
[INFO ../src/include/zns_device.h:974 z_get_zone_head] Zone Capacity (in number of LBAs) 275712, Zone Start LBA 0, Write Pointer (LBA) 0
[INFO ../src/include/zns_device.h:977 z_get_zone_head] zone head: 0
[INFO ../src/measurement.cc:88 zns_measure] current zone: 176, current lba 91758036, head 0
[INFO ../src/measurement.cc:97 zns_measure] writing with z_append:
[DBG ../src/measurement.cc:98 zns_measure] here
[2024-07-31 15:44:51.569779] nvme_qpair.c: 255:nvme_io_qpair_print_command: *NOTICE*: IO COMMAND (7d) sqid:1 cid:0 nsid:1
[2024-07-31 15:44:51.569803] nvme_qpair.c: 474:spdk_nvme_print_completion: *NOTICE*: ABORTED - SQ DELETION (00/08) qid:1 cid:0 cdw0:0 sqhd:0000 p:0 m:0 dnr:0
[2024-07-31 15:44:51.569812] nvme_qpair.c: 804:spdk_nvme_qpair_process_completions: *ERROR*: CQ transport error -6 (No such device or address) on qpair id 1
measure: ../src/measurement.cc:109: void zns_measure(void *): Assertion `rc == 0' failed.
fish: Job 1, 'sudo ./measure -c ../zstore2.js…' terminated by signal SIGABRT (Abort)
[2/2] Linking target measure
[INFO ../src/measurement.cc:164 main] Zstore start with current zone: 176
[2024-07-31 15:46:19.744002] Starting SPDK v24.05 git sha1 40a5c21 / DPDK 24.03.0 initialization...
[2024-07-31 15:46:19.744026] [ DPDK EAL parameters: zns_measurement_opts --no-shconf -c 0x1 --huge-unlink --no-telemetry --log-level=lib.eal:6 --log-level=lib.cryptodev:5 --log-level=user1:6 --iova-mode=pa --base-virtaddr=0x200000000000 --match-allocations --file-prefix=spdk_pid46728 ]
[2024-07-31 15:46:19.876390] app.c: 909:spdk_app_start: *NOTICE*: Total cores available: 1
[2024-07-31 15:46:19.910985] reactor.c: 937:reactor_run: *NOTICE*: Reactor started on core 0
[INFO ../src/measurement.cc:42 zns_measure] Fn: zns_measure

[INFO ../src/measurement.cc:51 zns_measure] Starting measurment with queue depth 16
[INFO ../src/include/zns_device.h:220 zstore_qpair_setup] alloc qpair of queue size 16, request size 16
[INFO ../src/include/zns_device.h:974 z_get_zone_head] Zone Capacity (in number of LBAs) 275712, Zone Start LBA 0, Write Pointer (LBA) 0
[INFO ../src/include/zns_device.h:977 z_get_zone_head] zone head: 0
[INFO ../src/measurement.cc:89 zns_measure] current zone: 176, current lba 0, head 0
[INFO ../src/measurement.cc:98 zns_measure] writing with z_append:
[DBG ../src/measurement.cc:99 zns_measure] here
[INFO ../src/measurement.cc:121 zns_measure] qd 16, averge append 145.0999375 us
[INFO ../src/measurement.cc:125 zns_measure] read with z_append:
[INFO ../src/measurement.cc:139 zns_measure] qd 16, averge read 143.150375 us
[INFO ../src/include/zns_device.h:243 zstore_qpair_teardown] disconnect and free qpair
[INFO ../src/measurement.cc:51 zns_measure] Starting measurment with queue depth 32
[INFO ../src/include/zns_device.h:220 zstore_qpair_setup] alloc qpair of queue size 32, request size 32
[INFO ../src/include/zns_device.h:974 z_get_zone_head] Zone Capacity (in number of LBAs) 275712, Zone Start LBA 0, Write Pointer (LBA) 0
[INFO ../src/include/zns_device.h:977 z_get_zone_head] zone head: 0
[INFO ../src/measurement.cc:89 zns_measure] current zone: 176, current lba 91758036, head 0
[INFO ../src/measurement.cc:98 zns_measure] writing with z_append:
[DBG ../src/measurement.cc:99 zns_measure] here
[INFO ../src/measurement.cc:121 zns_measure] qd 32, averge append 147.99675 us
[INFO ../src/measurement.cc:125 zns_measure] read with z_append:
[INFO ../src/measurement.cc:139 zns_measure] qd 32, averge read 141.4315625 us
[INFO ../src/include/zns_device.h:243 zstore_qpair_teardown] disconnect and free qpair
[INFO ../src/measurement.cc:51 zns_measure] Starting measurment with queue depth 64
[INFO ../src/include/zns_device.h:220 zstore_qpair_setup] alloc qpair of queue size 64, request size 64
[INFO ../src/include/zns_device.h:974 z_get_zone_head] Zone Capacity (in number of LBAs) 275712, Zone Start LBA 0, Write Pointer (LBA) 0
[INFO ../src/include/zns_device.h:977 z_get_zone_head] zone head: 0
[INFO ../src/measurement.cc:89 zns_measure] current zone: 176, current lba 91758036, head 0
[INFO ../src/measurement.cc:98 zns_measure] writing with z_append:
[DBG ../src/measurement.cc:99 zns_measure] here
[2024-07-31 15:46:29.997269] nvme_qpair.c: 255:nvme_io_qpair_print_command: *NOTICE*: IO COMMAND (7d) sqid:1 cid:0 nsid:1
[2024-07-31 15:46:29.997291] nvme_qpair.c: 474:spdk_nvme_print_completion: *NOTICE*: ABORTED - SQ DELETION (00/08) qid:1 cid:0 cdw0:0 sqhd:0000 p:0 m:0 dnr:0
[2024-07-31 15:46:29.997300] nvme_qpair.c: 804:spdk_nvme_qpair_process_completions: *ERROR*: CQ transport error -6 (No such device or address) on qpair id 1
measure: ../src/measurement.cc:110: void zns_measure(void *): Assertion `rc == 0' failed.
fish: Job 1, 'sudo ./measure -c ../zstore2.js…' terminated by signal SIGABRT (Abort)

2/2] Linking target measure
[INFO ../src/measurement.cc:164 main] Zstore start with current zone: 176
[2024-07-31 15:46:54.473782] Starting SPDK v24.05 git sha1 40a5c21 / DPDK 24.03.0 initialization...
[2024-07-31 15:46:54.473804] [ DPDK EAL parameters: zns_measurement_opts --no-shconf -c 0x1 --huge-unlink --no-telemetry --log-level=lib.eal:6 --log-level=lib.cryptodev:5 --log-level=user1:6 --iova-mode=pa --base-virtaddr=0x200000000000 --match-allocations --file-prefix=spdk_pid46957 ]
[2024-07-31 15:46:54.606315] app.c: 909:spdk_app_start: *NOTICE*: Total cores available: 1
[2024-07-31 15:46:54.640916] reactor.c: 937:reactor_run: *NOTICE*: Reactor started on core 0
[INFO ../src/measurement.cc:42 zns_measure] Fn: zns_measure

[INFO ../src/measurement.cc:51 zns_measure] Starting measurment with queue depth 64
[INFO ../src/include/zns_device.h:220 zstore_qpair_setup] alloc qpair of queue size 64, request size 64
[INFO ../src/include/zns_device.h:974 z_get_zone_head] Zone Capacity (in number of LBAs) 275712, Zone Start LBA 0, Write Pointer (LBA) 0
[INFO ../src/include/zns_device.h:977 z_get_zone_head] zone head: 0
[INFO ../src/measurement.cc:89 zns_measure] current zone: 176, current lba 0, head 0
[INFO ../src/measurement.cc:98 zns_measure] writing with z_append:
[DBG ../src/measurement.cc:99 zns_measure] here
[INFO ../src/measurement.cc:121 zns_measure] qd 64, averge append 143.355 us
[INFO ../src/measurement.cc:125 zns_measure] read with z_append:
[INFO ../src/measurement.cc:139 zns_measure] qd 64, averge read 143.1879375 us
[INFO ../src/include/zns_device.h:243 zstore_qpair_teardown] disconnect and free qpair
[INFO ../src/measurement.cc:144 zns_measure] Test start finish
^Ctest_abort
[INFO ../src/measurement.cc:186 main] zstore exits gracefully
[15:47] zstore1:build (spdk *) |

