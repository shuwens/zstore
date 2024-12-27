
[16:20] zstore1:build-rel (dev) | meson compile && sudo ./zstore 6 1
INFO: autodetecting backend as ninja
INFO: calculating backend command to run: /usr/local/bin/ninja
ninja: no work to do.
[INFO ../src/zstore_controller.cc:59 Init] Init Checkpointing Gateway on Zstore1
[DBG ../src/zstore_controller.cc:102 Init] Configuration: sector size 4096, context pool size 4096, targets 4, devices 2
[INFO ../src/device.cc:25 Init] ns: 1, Zone size: 524288, zone cap: 275712, num of zones: 904, md size 0
[INFO ../src/device.cc:25 Init] ns: 2, Zone size: 524288, zone cap: 275712, num of zones: 904, md size 0
[INFO ../src/device.cc:25 Init] ns: 1, Zone size: 524288, zone cap: 275712, num of zones: 3688, md size 0
[INFO ../src/device.cc:25 Init] ns: 2, Zone size: 524288, zone cap: 275712, num of zones: 3688, md size 0
[INFO ../src/device.cc:25 Init] ns: 1, Zone size: 524288, zone cap: 275712, num of zones: 887, md size 0
[INFO ../src/device.cc:25 Init] ns: 2, Zone size: 524288, zone cap: 275712, num of zones: 887, md size 0
[INFO ../src/device.cc:25 Init] ns: 1, Zone size: 524288, zone cap: 275712, num of zones: 3688, md size 0
[INFO ../src/device.cc:25 Init] ns: 2, Zone size: 524288, zone cap: 275712, num of zones: 3688, md size 0
[DBG ../src/zstore_controller.cc:195 Init] ZstoreController launching threads
[INFO ../src/zstore_controller.cc:475 initIoThread] IO thread name IoThread id 1 on core 1
[INFO ../src/zstore_controller.cc:248 Init] Initialization complete. Launching workers.
[INFO ../src/zstore_controller.cc:242 operator()] HTTP server: Thread 0 on core 2
[INFO ../src/zstore_controller.cc:242 operator()] HTTP server: Thread 2 on core 4
[INFO ../src/zstore_controller.cc:242 operator()] HTTP server: Thread 3 on core 5
[INFO ../src/zstore_controller.cc:242 operator()] HTTP server: Thread 4 on core 6
[INFO ../src/zstore_controller.cc:242 operator()] HTTP server: Thread 5 on core 7
[INFO ../src/zstore_controller.cc:242 operator()] HTTP server: Thread 1 on core 3
[INFO ../src/zstore_controller.cc:1019 startZooKeeper] Connecting to ZooKeeper server...
[INFO ../src/zstore_controller.cc:858 createZnodes] success to create /election/n_
[INFO ../src/zstore_controller.cc:964 checkChildrenChange] leader: gateway_1
[INFO ../src/zstore_controller.cc:964 checkChildrenChange] leader: gateway_1
[INFO ../src/zstore_controller.cc:964 checkChildrenChange] leader: gateway_1
[INFO ../src/zstore_controller.cc:964 checkChildrenChange] leader: gateway_1
[INFO ../src/zstore_controller.cc:964 checkChildrenChange] leader: gateway_1
[INFO ../src/zstore_controller.cc:964 checkChildrenChange] leader: gateway_1
[INFO ../src/zstore_controller.cc:1056 Checkpoint] create children under /tx: gateway_1, tx_path /tx/gateway_1
[INFO ../src/zstore_controller.cc:1059 Checkpoint] /tx/gateway_1 does not exist
[INFO ../src/zstore_controller.cc:1066 Checkpoint] Success creating znode /tx/gateway_1
[INFO ../src/zstore_controller.cc:1073 Checkpoint] Success setting watcher on /tx/gateway_1
[INFO ../src/zstore_controller.cc:1056 Checkpoint] create children under /tx: gateway_4, tx_path /tx/gateway_4
[INFO ../src/zstore_controller.cc:1059 Checkpoint] /tx/gateway_4 does not exist
[INFO ../src/zstore_controller.cc:1066 Checkpoint] Success creating znode /tx/gateway_4
[INFO ../src/zstore_controller.cc:1073 Checkpoint] Success setting watcher on /tx/gateway_4
[INFO ../src/zstore_controller.cc:1056 Checkpoint] create children under /tx: gateway_2, tx_path /tx/gateway_2
[INFO ../src/zstore_controller.cc:1059 Checkpoint] /tx/gateway_2 does not exist
[INFO ../src/zstore_controller.cc:1066 Checkpoint] Success creating znode /tx/gateway_2
[INFO ../src/zstore_controller.cc:1073 Checkpoint] Success setting watcher on /tx/gateway_2
[INFO ../src/zstore_controller.cc:1056 Checkpoint] create children under /tx: gateway_5, tx_path /tx/gateway_5
[INFO ../src/zstore_controller.cc:1059 Checkpoint] /tx/gateway_5 does not exist
[INFO ../src/zstore_controller.cc:1066 Checkpoint] Success creating znode /tx/gateway_5
[INFO ../src/zstore_controller.cc:1073 Checkpoint] Success setting watcher on /tx/gateway_5
[INFO ../src/zstore_controller.cc:1056 Checkpoint] create children under /tx: gateway_3, tx_path /tx/gateway_3
[INFO ../src/zstore_controller.cc:1059 Checkpoint] /tx/gateway_3 does not exist
[INFO ../src/zstore_controller.cc:1066 Checkpoint] Success creating znode /tx/gateway_3
[INFO ../src/zstore_controller.cc:1073 Checkpoint] Success setting watcher on /tx/gateway_3
[INFO ../src/zstore_controller.cc:1056 Checkpoint] create children under /tx: gateway_6, tx_path /tx/gateway_6
[INFO ../src/zstore_controller.cc:1059 Checkpoint] /tx/gateway_6 does not exist
[INFO ../src/zstore_controller.cc:1066 Checkpoint] Success creating znode /tx/gateway_6
[INFO ../src/zstore_controller.cc:1073 Checkpoint] Success setting watcher on /tx/gateway_6
[INFO ../src/zstore_controller.cc:895 checkTxChange] checkTxChange
[INFO ../src/zstore_controller.cc:913 checkTxChange] data from /tx/gateway_4: empty
[INFO ../src/zstore_controller.cc:915 checkTxChange] node /tx/gateway_4 has not changed
[INFO ../src/zstore_controller.cc:913 checkTxChange] data from /tx/gateway_3: empty
[INFO ../src/zstore_controller.cc:915 checkTxChange] node /tx/gateway_3 has not changed
[INFO ../src/zstore_controller.cc:913 checkTxChange] data from /tx/gateway_6: empty
[INFO ../src/zstore_controller.cc:915 checkTxChange] node /tx/gateway_6 has not changed
[INFO ../src/zstore_controller.cc:913 checkTxChange] data from /tx/gateway_5: empty
[INFO ../src/zstore_controller.cc:915 checkTxChange] node /tx/gateway_5 has not changed
[INFO ../src/zstore_controller.cc:913 checkTxChange] data from /tx/gateway_2: commit
[INFO ../src/zstore_controller.cc:923 checkTxChange] Success deleting data from /tx/gateway_2
[INFO ../src/zstore_controller.cc:913 checkTxChange] data from /tx/gateway_1: empty
[INFO ../src/zstore_controller.cc:915 checkTxChange] node /tx/gateway_1 has not changed
[INFO ../src/zstore_controller.cc:964 checkChildrenChange] leader: gateway_1
[INFO ../src/zstore_controller.cc:895 checkTxChange] checkTxChange
[INFO ../src/zstore_controller.cc:913 checkTxChange] data from /tx/gateway_4: empty
[INFO ../src/zstore_controller.cc:915 checkTxChange] node /tx/gateway_4 has not changed
[INFO ../src/zstore_controller.cc:913 checkTxChange] data from /tx/gateway_3: commit
[INFO ../src/zstore_controller.cc:923 checkTxChange] Success deleting data from /tx/gateway_3
[INFO ../src/zstore_controller.cc:913 checkTxChange] data from /tx/gateway_6: empty
[INFO ../src/zstore_controller.cc:915 checkTxChange] node /tx/gateway_6 has not changed
[INFO ../src/zstore_controller.cc:913 checkTxChange] data from /tx/gateway_5: empty
[INFO ../src/zstore_controller.cc:915 checkTxChange] node /tx/gateway_5 has not changed
[INFO ../src/zstore_controller.cc:913 checkTxChange] data from /tx/gateway_1: empty
[INFO ../src/zstore_controller.cc:915 checkTxChange] node /tx/gateway_1 has not changed
[INFO ../src/zstore_controller.cc:964 checkChildrenChange] leader: gateway_1
[INFO ../src/zstore_controller.cc:895 checkTxChange] checkTxChange
[INFO ../src/zstore_controller.cc:913 checkTxChange] data from /tx/gateway_4: commit
[INFO ../src/zstore_controller.cc:923 checkTxChange] Success deleting data from /tx/gateway_4
[INFO ../src/zstore_controller.cc:913 checkTxChange] data from /tx/gateway_6: empty
[INFO ../src/zstore_controller.cc:915 checkTxChange] node /tx/gateway_6 has not changed
[INFO ../src/zstore_controller.cc:913 checkTxChange] data from /tx/gateway_5: empty
[INFO ../src/zstore_controller.cc:915 checkTxChange] node /tx/gateway_5 has not changed
[INFO ../src/zstore_controller.cc:913 checkTxChange] data from /tx/gateway_1: empty
[INFO ../src/zstore_controller.cc:915 checkTxChange] node /tx/gateway_1 has not changed
[INFO ../src/zstore_controller.cc:964 checkChildrenChange] leader: gateway_1
[INFO ../src/zstore_controller.cc:895 checkTxChange] checkTxChange
[INFO ../src/zstore_controller.cc:913 checkTxChange] data from /tx/gateway_6: empty
[INFO ../src/zstore_controller.cc:915 checkTxChange] node /tx/gateway_6 has not changed
[INFO ../src/zstore_controller.cc:913 checkTxChange] data from /tx/gateway_5: commit
[INFO ../src/zstore_controller.cc:923 checkTxChange] Success deleting data from /tx/gateway_5
[INFO ../src/zstore_controller.cc:913 checkTxChange] data from /tx/gateway_1: empty
[INFO ../src/zstore_controller.cc:915 checkTxChange] node /tx/gateway_1 has not changed
[INFO ../src/zstore_controller.cc:964 checkChildrenChange] leader: gateway_1
[INFO ../src/zstore_controller.cc:895 checkTxChange] checkTxChange
[INFO ../src/zstore_controller.cc:913 checkTxChange] data from /tx/gateway_6: commit
[INFO ../src/zstore_controller.cc:923 checkTxChange] Success deleting data from /tx/gateway_6
[INFO ../src/zstore_controller.cc:913 checkTxChange] data from /tx/gateway_1: empty
[INFO ../src/zstore_controller.cc:915 checkTxChange] node /tx/gateway_1 has not changed
[INFO ../src/zstore_controller.cc:964 checkChildrenChange] leader: gateway_1
[INFO ../src/zstore_controller.cc:895 checkTxChange] checkTxChange
[INFO ../src/zstore_controller.cc:1086 Checkpoint] Success setting data to /tx/gateway_1
[INFO ../src/zstore_controller.cc:284 Init] ZstoreController Init finish
[INFO ../src/zstore_controller.cc:913 checkTxChange] data from /tx/gateway_1: commit
[INFO ../src/zstore_controller.cc:923 checkTxChange] Success deleting data from /tx/gateway_1
[INFO ../src/zstore_controller.cc:964 checkChildrenChange] leader: gateway_1


