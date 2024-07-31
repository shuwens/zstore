[INFO ../src/measurement.cc:163 main] Zstore start with current zone: 176
[2024-07-31 17:01:05.520286] Starting SPDK v24.05 git sha1 40a5c21 / DPDK 24.03.0 initialization...
[2024-07-31 17:01:05.520309] [ DPDK EAL parameters: zns_measurement_opts --no-shconf -c 0x1 --huge-unlink --no-telemetry --log-level=lib.eal:6 --log-level=lib.cryptodev:5 --log-level=user1:6 --iova-mode=pa --base-virtaddr=0x200000000000 --match-allocations --file-prefix=spdk_pid55756 ]
[2024-07-31 17:01:05.652585] app.c: 909:spdk_app_start: *NOTICE*: Total cores available: 1
[2024-07-31 17:01:05.684037] reactor.c: 937:reactor_run: *NOTICE*: Reactor started on core 0
[INFO ../src/measurement.cc:42 zns_measure] Fn: zns_measure

[INFO ../src/measurement.cc:50 zns_measure] Starting measurment with queue depth 2
[INFO ../src/include/zns_device.h:221 zstore_qpair_setup] alloc qpair of queue size 2, request size 2
[INFO ../src/include/zns_device.h:1038 z_get_zone_head] Zone Capacity (in number of LBAs) 275712, Zone Start LBA 0, Write Pointer (LBA) 0
[INFO ../src/include/zns_device.h:1041 z_get_zone_head] zone head: 0
[INFO ../src/measurement.cc:88 zns_measure] current zone: 176, current lba 0, head 0
[INFO ../src/measurement.cc:97 zns_measure] writing with z_append:
[DBG ../src/measurement.cc:98 zns_measure] here
[INFO ../src/measurement.cc:120 zns_measure] qd 2, averge append 146.1719375 us
[INFO ../src/measurement.cc:124 zns_measure] read with z_append:
[INFO ../src/measurement.cc:138 zns_measure] qd 2, averge read 140.9858125 us
[INFO ../src/include/zns_device.h:250 zstore_qpair_teardown] disconnect and free qpair
[INFO ../src/measurement.cc:50 zns_measure] Starting measurment with queue depth 4
[INFO ../src/include/zns_device.h:221 zstore_qpair_setup] alloc qpair of queue size 4, request size 4
[INFO ../src/include/zns_device.h:1038 z_get_zone_head] Zone Capacity (in number of LBAs) 275712, Zone Start LBA 0, Write Pointer (LBA) 0
[INFO ../src/include/zns_device.h:1041 z_get_zone_head] zone head: 0
[INFO ../src/measurement.cc:88 zns_measure] current zone: 176, current lba 91758036, head 0
[INFO ../src/measurement.cc:97 zns_measure] writing with z_append:
[DBG ../src/measurement.cc:98 zns_measure] here
[INFO ../src/measurement.cc:120 zns_measure] qd 4, averge append 142.2265625 us
[INFO ../src/measurement.cc:124 zns_measure] read with z_append:
[INFO ../src/measurement.cc:138 zns_measure] qd 4, averge read 142.7948125 us
[INFO ../src/include/zns_device.h:250 zstore_qpair_teardown] disconnect and free qpair
[INFO ../src/measurement.cc:50 zns_measure] Starting measurment with queue depth 8
[INFO ../src/include/zns_device.h:221 zstore_qpair_setup] alloc qpair of queue size 8, request size 8
[INFO ../src/include/zns_device.h:1038 z_get_zone_head] Zone Capacity (in number of LBAs) 275712, Zone Start LBA 0, Write Pointer (LBA) 0
[INFO ../src/include/zns_device.h:1041 z_get_zone_head] zone head: 0
[INFO ../src/measurement.cc:88 zns_measure] current zone: 176, current lba 91758036, head 0
[INFO ../src/measurement.cc:97 zns_measure] writing with z_append:
[DBG ../src/measurement.cc:98 zns_measure] here
[2024-07-31 17:01:15.776256] nvme_qpair.c: 255:nvme_io_qpair_print_command: *NOTICE*: IO COMMAND (7d) sqid:1 cid:0 nsid:1
[2024-07-31 17:01:15.776278] nvme_qpair.c: 474:spdk_nvme_print_completion: *NOTICE*: ABORTED - SQ DELETION (00/08) qid:1 cid:0 cdw0:0 sqhd:0000 p:0 m:0 dnr:0
[2024-07-31 17:01:15.776288] nvme_qpair.c: 474:spdk_nvme_print_completion: *NOTICE*: ABORTED - SQ DELETION (00/08) qid:1 cid:0 cdw0:0 sqhd:0000 p:0 m:0 dnr:0
Reset all zone error - status = ABORTED - SQ DELETION
[2024-07-31 17:01:15.776305] app.c:1053:spdk_app_stop: *WARNING*: spdk_app_stop'd on non-zero
[2024-07-31 17:01:15.776314] nvme_qpair.c: 804:spdk_nvme_qpair_process_completions: *ERROR*: CQ transport error -6 (No such device or address) on qpair id 1
^C

fish: Job 1, 'sudo ./measure -c ../zstore2.js…' terminated by signal SIGKILL (Forced quit)
[17:01] zstore1:build (spdk *) | nvim ../src/measurement.cc
[17:02] zstore1:build (spdk *) | meson compile && sudo ./measure -c ../zstore2.json
INFO: autodetecting backend as ninja
INFO: calculating backend command to run: /usr/local/bin/ninja
[1/2] Compiling C++ object measure.p/src_measurement.cc.o
In file included from ../src/measurement.cc:2:
../src/include/zns_device.h:33:15: warning: anonymous non-C-compatible type given name for linkage purposes by typedef declaration; add a tag name here [-Wnon-c-typedef-for-linkage]
   33 | typedef struct {
      |               ^
      |                DeviceManager
../src/include/zns_device.h:34:44: note: type is not C-compatible due to this default member initializer
   34 |     struct spdk_nvme_transport_id g_trid = {};
      |                                            ^~
../src/include/zns_device.h:38:3: note: type is given name 'DeviceManager' for linkage purposes by this typedef declaration
   38 | } DeviceManager;
      |   ^
../src/include/zns_device.h:89:15: warning: anonymous non-C-compatible type given name for linkage purposes by typedef declaration; add a tag name here [-Wnon-c-typedef-for-linkage]
   89 | typedef struct {
      |               ^
      |                Completion
../src/include/zns_device.h:90:17: note: type is not C-compatible due to this default member initializer
   90 |     bool done = false;
      |                 ^~~~~
../src/include/zns_device.h:92:3: note: type is given name 'Completion' for linkage purposes by this typedef declaration
   92 | } Completion;
      |   ^
../src/include/zns_device.h:122:9: warning: unused variable 'rc' [-Wunused-variable]
  122 |     int rc = 0;
      |         ^~
../src/include/zns_device.h:131:9: warning: unused variable 'nsid' [-Wunused-variable]
  131 |     int nsid = 0;
      |         ^~~~
../src/include/zns_device.h:213:9: warning: unused variable 'rc' [-Wunused-variable]
  213 |     int rc = 0;
      |         ^~
../src/include/zns_device.h:797:28: warning: format specifies type 'unsigned int' but the argument has type 'u64' (aka 'unsigned long') [-Wformat]
  796 |             SPDK_NOTICELOG("read lba:0x%x to read buffer\n",
      |                                        ~~
      |                                        %lx
  797 |                            ctx->current_lba + i);
      |                            ^~~~~~~~~~~~~~~~~~~~
../subprojects/spdk/build/include/spdk/log.h:109:58: note: expanded from macro 'SPDK_NOTICELOG'
  109 |         spdk_log(SPDK_LOG_NOTICE, __FILE__, __LINE__, __func__, __VA_ARGS__)
      |                                                                 ^~~~~~~~~~~
In file included from ../src/measurement.cc:2:
../src/include/zns_device.h:936:24: warning: format specifies type 'unsigned int' but the argument has type 'uint64_t' (aka 'unsigned long') [-Wformat]
  935 |         SPDK_NOTICELOG("reset zone with lba:0x%x\n",
      |                                               ~~
      |                                               %lx
  936 |                        ctx->zslba + slba + ctx->info.zone_size);
      |                        ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
../subprojects/spdk/build/include/spdk/log.h:109:58: note: expanded from macro 'SPDK_NOTICELOG'
  109 |         spdk_log(SPDK_LOG_NOTICE, __FILE__, __LINE__, __func__, __VA_ARGS__)
      |                                                                 ^~~~~~~~~~~
../src/measurement.cc:103:39: warning: ISO C++11 does not allow conversion from string literal to 'char *' [-Wwritable-strings]
  103 |                                       "test_zstore1:", value + i);
      |                                       ^
In file included from ../src/measurement.cc:2:
In file included from ../src/include/zns_device.h:8:
../src/include/zns_utils.h:256:12: warning: unused function 'get_error_log_page' [-Wunused-function]
  256 | static int get_error_log_page(struct spdk_nvme_ctrlr *ctrlr)
      |            ^~~~~~~~~~~~~~~~~~
../src/include/zns_utils.h:273:12: warning: unused function 'get_health_log_page' [-Wunused-function]
  273 | static int get_health_log_page(struct spdk_nvme_ctrlr *ctrlr)
      |            ^~~~~~~~~~~~~~~~~~~
../src/include/zns_utils.h:286:12: warning: unused function 'get_firmware_log_page' [-Wunused-function]
  286 | static int get_firmware_log_page(struct spdk_nvme_ctrlr *ctrlr)
      |            ^~~~~~~~~~~~~~~~~~~~~
../src/include/zns_utils.h:299:12: warning: unused function 'get_ana_log_page' [-Wunused-function]
  299 | static int get_ana_log_page(struct spdk_nvme_ctrlr *ctrlr)
      |            ^~~~~~~~~~~~~~~~
../src/include/zns_utils.h:312:12: warning: unused function 'get_cmd_effects_log_page' [-Wunused-function]
  312 | static int get_cmd_effects_log_page(struct spdk_nvme_ctrlr *ctrlr)
      |            ^~~~~~~~~~~~~~~~~~~~~~~~
../src/include/zns_utils.h:325:12: warning: unused function 'get_intel_smart_log_page' [-Wunused-function]
  325 | static int get_intel_smart_log_page(struct spdk_nvme_ctrlr *ctrlr)
      |            ^~~~~~~~~~~~~~~~~~~~~~~~
../src/include/zns_utils.h:338:12: warning: unused function 'get_intel_temperature_log_page' [-Wunused-function]
  338 | static int get_intel_temperature_log_page(struct spdk_nvme_ctrlr *ctrlr)
      |            ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
../src/include/zns_utils.h:350:12: warning: unused function 'get_intel_md_log_page' [-Wunused-function]
  350 | static int get_intel_md_log_page(struct spdk_nvme_ctrlr *ctrlr)
      |            ^~~~~~~~~~~~~~~~~~~~~
../src/include/zns_utils.h:663:13: warning: unused function 'print_uint128_dec' [-Wunused-function]
  663 | static void print_uint128_dec(uint64_t *v)
      |             ^~~~~~~~~~~~~~~~~
../src/include/zns_utils.h:676:13: warning: unused function 'print_uint_var_dec' [-Wunused-function]
  676 | static void print_uint_var_dec(uint8_t *array, unsigned int len)
      |             ^~~~~~~~~~~~~~~~~~
../src/include/zns_utils.h:727:13: warning: unused function 'print_zns_current_zone_report' [-Wunused-function]
  727 | static void print_zns_current_zone_report(uint64_t current_zone)
      |             ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~
In file included from ../src/measurement.cc:2:
../src/include/zns_device.h:336:13: warning: unused function '__operation_complete2' [-Wunused-function]
  336 | static void __operation_complete2(void *arg,
      |             ^~~~~~~~~~~~~~~~~~~~~
../src/include/zns_device.h:348:13: warning: unused function '__append_complete2' [-Wunused-function]
  348 | static void __append_complete2(void *arg,
      |             ^~~~~~~~~~~~~~~~~~
../src/include/zns_device.h:360:13: warning: unused function '__read_complete2' [-Wunused-function]
  360 | static void __read_complete2(void *arg, const struct spdk_nvme_cpl *completion)
      |             ^~~~~~~~~~~~~~~~
../src/include/zns_device.h:381:13: warning: unused function '__append_complete' [-Wunused-function]
  381 | static void __append_complete(void *arg, const struct spdk_nvme_cpl *completion)
      |             ^~~~~~~~~~~~~~~~~
../src/include/zns_device.h:386:13: warning: unused function '__read_complete' [-Wunused-function]
  386 | static void __read_complete(void *arg, const struct spdk_nvme_cpl *completion)
      |             ^~~~~~~~~~~~~~~
../src/include/zns_device.h:684:13: warning: unused function 'close_zone' [-Wunused-function]
  684 | static void close_zone(void *arg)
      |             ^~~~~~~~~~
../src/include/zns_device.h:768:13: warning: unused function 'read_zone' [-Wunused-function]
  768 | static void read_zone(void *arg)
      |             ^~~~~~~~~
../src/include/zns_device.h:850:13: warning: unused function 'write_zone' [-Wunused-function]
  850 | static void write_zone(void *arg)
      |             ^~~~~~~~~~
../src/include/zns_device.h:920:13: warning: unused function 'reset_zone' [-Wunused-function]
  920 | static void reset_zone(void *arg)
      |             ^~~~~~~~~~
28 warnings generated.
[2/2] Linking target measure
[INFO ../src/measurement.cc:164 main] Zstore start with current zone: 176
[2024-07-31 17:02:09.134171] Starting SPDK v24.05 git sha1 40a5c21 / DPDK 24.03.0 initialization...
[2024-07-31 17:02:09.134194] [ DPDK EAL parameters: zns_measurement_opts --no-shconf -c 0x1 --huge-unlink --no-telemetry --log-level=lib.eal:6 --log-level=lib.cryptodev:5 --log-level=user1:6 -
-iova-mode=pa --base-virtaddr=0x200000000000 --match-allocations --file-prefix=spdk_pid56144 ]
[2024-07-31 17:02:09.266477] app.c: 909:spdk_app_start: *NOTICE*: Total cores available: 1
[2024-07-31 17:02:09.297974] reactor.c: 937:reactor_run: *NOTICE*: Reactor started on core 0
[INFO ../src/measurement.cc:42 zns_measure] Fn: zns_measure

[INFO ../src/measurement.cc:51 zns_measure] Starting measurment with queue depth 8
[INFO ../src/include/zns_device.h:221 zstore_qpair_setup] alloc qpair of queue size 8, request size 8
[INFO ../src/include/zns_device.h:1038 z_get_zone_head] Zone Capacity (in number of LBAs) 275712, Zone Start LBA 0, Write Pointer (LBA) 0
[INFO ../src/include/zns_device.h:1041 z_get_zone_head] zone head: 0
[INFO ../src/measurement.cc:89 zns_measure] current zone: 176, current lba 0, head 0
[INFO ../src/measurement.cc:98 zns_measure] writing with z_append:
[DBG ../src/measurement.cc:99 zns_measure] here
[INFO ../src/measurement.cc:121 zns_measure] qd 8, averge append 144.1708125 us
[INFO ../src/measurement.cc:125 zns_measure] read with z_append:
[INFO ../src/measurement.cc:139 zns_measure] qd 8, averge read 136.039 us
[INFO ../src/include/zns_device.h:250 zstore_qpair_teardown] disconnect and free qpair
[INFO ../src/measurement.cc:51 zns_measure] Starting measurment with queue depth 16
[INFO ../src/include/zns_device.h:221 zstore_qpair_setup] alloc qpair of queue size 16, request size 16
[INFO ../src/include/zns_device.h:1038 z_get_zone_head] Zone Capacity (in number of LBAs) 275712, Zone Start LBA 0, Write Pointer (LBA) 0
[INFO ../src/include/zns_device.h:1041 z_get_zone_head] zone head: 0
[INFO ../src/measurement.cc:89 zns_measure] current zone: 176, current lba 91758036, head 0
[INFO ../src/measurement.cc:98 zns_measure] writing with z_append:
[DBG ../src/measurement.cc:99 zns_measure] here
[INFO ../src/measurement.cc:121 zns_measure] qd 16, averge append 148.7203125 us
[INFO ../src/measurement.cc:125 zns_measure] read with z_append:
[INFO ../src/measurement.cc:139 zns_measure] qd 16, averge read 143.1915625 us
[INFO ../src/include/zns_device.h:250 zstore_qpair_teardown] disconnect and free qpair
[INFO ../src/measurement.cc:51 zns_measure] Starting measurment with queue depth 32
[INFO ../src/include/zns_device.h:221 zstore_qpair_setup] alloc qpair of queue size 32, request size 32
[INFO ../src/include/zns_device.h:1038 z_get_zone_head] Zone Capacity (in number of LBAs) 275712, Zone Start LBA 0, Write Pointer (LBA) 0
[INFO ../src/include/zns_device.h:1041 z_get_zone_head] zone head: 0
[INFO ../src/measurement.cc:89 zns_measure] current zone: 176, current lba 91758036, head 0
[INFO ../src/measurement.cc:98 zns_measure] writing with z_append:
[DBG ../src/measurement.cc:99 zns_measure] here
[2024-07-31 17:02:19.388256] nvme_qpair.c: 255:nvme_io_qpair_print_command: *NOTICE*: IO COMMAND (7d) sqid:1 cid:0 nsid:1
[2024-07-31 17:02:19.388278] nvme_qpair.c: 474:spdk_nvme_print_completion: *NOTICE*: ABORTED - SQ DELETION (00/08) qid:1 cid:0 cdw0:0 sqhd:0000 p:0 m:0 dnr:0
[2024-07-31 17:02:19.388287] nvme_qpair.c: 474:spdk_nvme_print_completion: *NOTICE*: ABORTED - SQ DELETION (00/08) qid:1 cid:0 cdw0:0 sqhd:0000 p:0 m:0 dnr:0
Reset all zone error - status = ABORTED - SQ DELETION
[2024-07-31 17:02:19.388305] app.c:1053:spdk_app_stop: *WARNING*: spdk_app_stop'd on non-zero
[2024-07-31 17:02:19.388315] nvme_qpair.c: 804:spdk_nvme_qpair_process_completions: *ERROR*: CQ transport error -6 (No such device or address) on qpair id 1
^Cfish: Job 1, 'sudo ./measure -c ../zstore2.js…' terminated by signal SIGKILL (Forced quit)
[17:02] zstore1:build (spdk *) | nvim ../src/measurement.cc
[17:02] zstore1:build (spdk *) | meson compile && sudo ./measure -c ../zstore2.json
INFO: autodetecting backend as ninja
INFO: calculating backend command to run: /usr/local/bin/ninja
[1/2] Compiling C++ object measure.p/src_measurement.cc.o
In file included from ../src/measurement.cc:2:
../src/include/zns_device.h:33:15: warning: anonymous non-C-compatible type given name for linkage purposes by typedef declaration; add a tag name here [-Wnon-c-typedef-for-linkage]
   33 | typedef struct {
      |               ^
      |                DeviceManager
../src/include/zns_device.h:34:44: note: type is not C-compatible due to this default member initializer
   34 |     struct spdk_nvme_transport_id g_trid = {};
      |                                            ^~
../src/include/zns_device.h:38:3: note: type is given name 'DeviceManager' for linkage purposes by this typedef declaration
   38 | } DeviceManager;
      |   ^
../src/include/zns_device.h:89:15: warning: anonymous non-C-compatible type given name for linkage purposes by typedef declaration; add a tag name here [-Wnon-c-typedef-for-linkage]
   89 | typedef struct {
      |               ^
      |                Completion
../src/include/zns_device.h:90:17: note: type is not C-compatible due to this default member initializer
   90 |     bool done = false;
      |                 ^~~~~
../src/include/zns_device.h:92:3: note: type is given name 'Completion' for linkage purposes by this typedef declaration
   92 | } Completion;
      |   ^
../src/include/zns_device.h:122:9: warning: unused variable 'rc' [-Wunused-variable]
  122 |     int rc = 0;
      |         ^~
../src/include/zns_device.h:131:9: warning: unused variable 'nsid' [-Wunused-variable]
  131 |     int nsid = 0;
      |         ^~~~
../src/include/zns_device.h:213:9: warning: unused variable 'rc' [-Wunused-variable]
  213 |     int rc = 0;
      |         ^~
../src/include/zns_device.h:797:28: warning: format specifies type 'unsigned int' but the argument has type 'u64' (aka 'unsigned long') [-Wformat]
  796 |             SPDK_NOTICELOG("read lba:0x%x to read buffer\n",
      |                                        ~~
      |                                        %lx
  797 |                            ctx->current_lba + i);
      |                            ^~~~~~~~~~~~~~~~~~~~
../subprojects/spdk/build/include/spdk/log.h:109:58: note: expanded from macro 'SPDK_NOTICELOG'
  109 |         spdk_log(SPDK_LOG_NOTICE, __FILE__, __LINE__, __func__, __VA_ARGS__)
      |                                                                 ^~~~~~~~~~~
In file included from ../src/measurement.cc:2:
../src/include/zns_device.h:936:24: warning: format specifies type 'unsigned int' but the argument has type 'uint64_t' (aka 'unsigned long') [-Wformat]
  935 |         SPDK_NOTICELOG("reset zone with lba:0x%x\n",
      |                                               ~~
      |                                               %lx
  936 |                        ctx->zslba + slba + ctx->info.zone_size);
      |                        ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
../subprojects/spdk/build/include/spdk/log.h:109:58: note: expanded from macro 'SPDK_NOTICELOG'
  109 |         spdk_log(SPDK_LOG_NOTICE, __FILE__, __LINE__, __func__, __VA_ARGS__)
      |                                                                 ^~~~~~~~~~~
../src/measurement.cc:103:39: warning: ISO C++11 does not allow conversion from string literal to 'char *' [-Wwritable-strings]
  103 |                                       "test_zstore1:", value + i);
      |                                       ^
In file included from ../src/measurement.cc:2:
In file included from ../src/include/zns_device.h:8:
../src/include/zns_utils.h:256:12: warning: unused function 'get_error_log_page' [-Wunused-function]
  256 | static int get_error_log_page(struct spdk_nvme_ctrlr *ctrlr)
      |            ^~~~~~~~~~~~~~~~~~
../src/include/zns_utils.h:273:12: warning: unused function 'get_health_log_page' [-Wunused-function]
  273 | static int get_health_log_page(struct spdk_nvme_ctrlr *ctrlr)
      |            ^~~~~~~~~~~~~~~~~~~
../src/include/zns_utils.h:286:12: warning: unused function 'get_firmware_log_page' [-Wunused-function]
  286 | static int get_firmware_log_page(struct spdk_nvme_ctrlr *ctrlr)
      |            ^~~~~~~~~~~~~~~~~~~~~
../src/include/zns_utils.h:299:12: warning: unused function 'get_ana_log_page' [-Wunused-function]
  299 | static int get_ana_log_page(struct spdk_nvme_ctrlr *ctrlr)
      |            ^~~~~~~~~~~~~~~~
../src/include/zns_utils.h:312:12: warning: unused function 'get_cmd_effects_log_page' [-Wunused-function]
  312 | static int get_cmd_effects_log_page(struct spdk_nvme_ctrlr *ctrlr)
      |            ^~~~~~~~~~~~~~~~~~~~~~~~
../src/include/zns_utils.h:325:12: warning: unused function 'get_intel_smart_log_page' [-Wunused-function]
  325 | static int get_intel_smart_log_page(struct spdk_nvme_ctrlr *ctrlr)
      |            ^~~~~~~~~~~~~~~~~~~~~~~~
../src/include/zns_utils.h:338:12: warning: unused function 'get_intel_temperature_log_page' [-Wunused-function]
  338 | static int get_intel_temperature_log_page(struct spdk_nvme_ctrlr *ctrlr)
      |            ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
../src/include/zns_utils.h:350:12: warning: unused function 'get_intel_md_log_page' [-Wunused-function]
  350 | static int get_intel_md_log_page(struct spdk_nvme_ctrlr *ctrlr)
      |            ^~~~~~~~~~~~~~~~~~~~~
../src/include/zns_utils.h:663:13: warning: unused function 'print_uint128_dec' [-Wunused-function]
  663 | static void print_uint128_dec(uint64_t *v)
      |             ^~~~~~~~~~~~~~~~~
../src/include/zns_utils.h:676:13: warning: unused function 'print_uint_var_dec' [-Wunused-function]
  676 | static void print_uint_var_dec(uint8_t *array, unsigned int len)
      |             ^~~~~~~~~~~~~~~~~~
../src/include/zns_utils.h:727:13: warning: unused function 'print_zns_current_zone_report' [-Wunused-function]
  727 | static void print_zns_current_zone_report(uint64_t current_zone)
      |             ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~
In file included from ../src/measurement.cc:2:
../src/include/zns_device.h:336:13: warning: unused function '__operation_complete2' [-Wunused-function]
  336 | static void __operation_complete2(void *arg,
      |             ^~~~~~~~~~~~~~~~~~~~~
../src/include/zns_device.h:348:13: warning: unused function '__append_complete2' [-Wunused-function]
  348 | static void __append_complete2(void *arg,
      |             ^~~~~~~~~~~~~~~~~~
../src/include/zns_device.h:360:13: warning: unused function '__read_complete2' [-Wunused-function]
  360 | static void __read_complete2(void *arg, const struct spdk_nvme_cpl *completion)
      |             ^~~~~~~~~~~~~~~~
../src/include/zns_device.h:381:13: warning: unused function '__append_complete' [-Wunused-function]
  381 | static void __append_complete(void *arg, const struct spdk_nvme_cpl *completion)
      |             ^~~~~~~~~~~~~~~~~
../src/include/zns_device.h:386:13: warning: unused function '__read_complete' [-Wunused-function]
  386 | static void __read_complete(void *arg, const struct spdk_nvme_cpl *completion)
      |             ^~~~~~~~~~~~~~~
../src/include/zns_device.h:684:13: warning: unused function 'close_zone' [-Wunused-function]
  684 | static void close_zone(void *arg)
      |             ^~~~~~~~~~
../src/include/zns_device.h:768:13: warning: unused function 'read_zone' [-Wunused-function]
  768 | static void read_zone(void *arg)
      |             ^~~~~~~~~
../src/include/zns_device.h:850:13: warning: unused function 'write_zone' [-Wunused-function]
  850 | static void write_zone(void *arg)
      |             ^~~~~~~~~~
../src/include/zns_device.h:920:13: warning: unused function 'reset_zone' [-Wunused-function]
  920 | static void reset_zone(void *arg)
      |             ^~~~~~~~~~
28 warnings generated.
[2/2] Linking target measure
[INFO ../src/measurement.cc:164 main] Zstore start with current zone: 176
[2024-07-31 17:02:46.808222] Starting SPDK v24.05 git sha1 40a5c21 / DPDK 24.03.0 initialization...
[2024-07-31 17:02:46.808245] [ DPDK EAL parameters: zns_measurement_opts --no-shconf -c 0x1 --huge-unlink --no-telemetry --log-level=lib.eal:6 --log-level=lib.cryptodev:5 --log-level=user1:6 --iova-mode=pa --base-virtaddr=0x200000000000 --match-allocations --file-prefix=spdk_pid56534 ]
[2024-07-31 17:02:46.940774] app.c: 909:spdk_app_start: *NOTICE*: Total cores available: 1
[2024-07-31 17:02:46.975949] reactor.c: 937:reactor_run: *NOTICE*: Reactor started on core 0
[INFO ../src/measurement.cc:42 zns_measure] Fn: zns_measure

[INFO ../src/measurement.cc:51 zns_measure] Starting measurment with queue depth 32
[INFO ../src/include/zns_device.h:221 zstore_qpair_setup] alloc qpair of queue size 32, request size 32
[INFO ../src/include/zns_device.h:1038 z_get_zone_head] Zone Capacity (in number of LBAs) 275712, Zone Start LBA 0, Write Pointer (LBA) 0
[INFO ../src/include/zns_device.h:1041 z_get_zone_head] zone head: 0
[INFO ../src/measurement.cc:89 zns_measure] current zone: 176, current lba 0, head 0
[INFO ../src/measurement.cc:98 zns_measure] writing with z_append:
[DBG ../src/measurement.cc:99 zns_measure] here
[INFO ../src/measurement.cc:121 zns_measure] qd 32, averge append 143.33325 us
[INFO ../src/measurement.cc:125 zns_measure] read with z_append:
[INFO ../src/measurement.cc:139 zns_measure] qd 32, averge read 141.599875 us
[INFO ../src/include/zns_device.h:250 zstore_qpair_teardown] disconnect and free qpair
[INFO ../src/measurement.cc:51 zns_measure] Starting measurment with queue depth 64
[INFO ../src/include/zns_device.h:221 zstore_qpair_setup] alloc qpair of queue size 64, request size 64
[INFO ../src/include/zns_device.h:1038 z_get_zone_head] Zone Capacity (in number of LBAs) 275712, Zone Start LBA 0, Write Pointer (LBA) 0
[INFO ../src/include/zns_device.h:1041 z_get_zone_head] zone head: 0
[INFO ../src/measurement.cc:89 zns_measure] current zone: 176, current lba 91758036, head 0
[INFO ../src/measurement.cc:98 zns_measure] writing with z_append:
[DBG ../src/measurement.cc:99 zns_measure] here
[INFO ../src/measurement.cc:121 zns_measure] qd 64, averge append 144.9316875 us
[INFO ../src/measurement.cc:125 zns_measure] read with z_append:
[INFO ../src/measurement.cc:139 zns_measure] qd 64, averge read 144.2354375 us
[INFO ../src/include/zns_device.h:250 zstore_qpair_teardown] disconnect and free qpair
[INFO ../src/measurement.cc:144 zns_measure] Test start finish
^Ctest_abort
[INFO ../src/measurement.cc:187 main] zstore exits gracefully
[17:02] zstore1:build (spdk *) |

