2: 

[INFO ../src/measurement.cc:317 zns_measure] WRITES-1 qd 2, append: mean 183.546875 us, std 15.301071947232161
[INFO ../src/measurement.cc:319 zns_measure] WRITES-2 qd 2, append: mean 253.71875 us, std 11.90255848284309
[INFO ../src/measurement.cc:330 zns_measure] current lba for read: device 1 93866682, device 2 93860672
[INFO ../src/measurement.cc:331 zns_measure] read with z_append:
[DBG ../src/measurement.cc:363 zns_measure] m1: 64, 64
[DBG ../src/measurement.cc:364 zns_measure] m2: 64, 64
[DBG ../src/measurement.cc:376 zns_measure] deltas: m1 64, m2 64
[INFO ../src/measurement.cc:388 zns_measure] READ-1 qd 2, append: mean 169.5625 us, std 27.841557854222167
[INFO ../src/measurement.cc:390 zns_measure] READ-2 qd 2, append: mean 171.84375 us, std 29.37804173081487

4

[INFO ../src/measurement.cc:317 zns_measure] WRITES-1 qd 4, append: mean 275.015625 us, std 35.59384259755295
[INFO ../src/measurement.cc:319 zns_measure] WRITES-2 qd 4, append: mean 401.40625 us, std 18.291663700645167
[INFO ../src/measurement.cc:330 zns_measure] current lba for read: device 1 93866745, device 2 93860736
[INFO ../src/measurement.cc:331 zns_measure] read with z_append:
[DBG ../src/measurement.cc:363 zns_measure] m1: 64, 64
[DBG ../src/measurement.cc:364 zns_measure] m2: 64, 64
[DBG ../src/measurement.cc:376 zns_measure] deltas: m1 64, m2 64
[INFO ../src/measurement.cc:388 zns_measure] READ-1 qd 4, append: mean 169.46875 us, std 32.959240334654254
[INFO ../src/measurement.cc:390 zns_measure] READ-2 qd 4, append: mean 173.078125 us, std 31.747197065006777


8

[DBG ../src/measurement.cc:292 zns_measure] write is all done
[INFO ../src/measurement.cc:317 zns_measure] WRITES-1 qd 8, append: mean 417.609375 us, std 75.40880609789134
[INFO ../src/measurement.cc:319 zns_measure] WRITES-2 qd 8, append: mean 691.328125 us, std 81.12241033761494
[INFO ../src/measurement.cc:330 zns_measure] current lba for read: device 1 93866809, device 2 93860800
[INFO ../src/measurement.cc:331 zns_measure] read with z_append:
[DBG ../src/measurement.cc:363 zns_measure] m1: 64, 64
[DBG ../src/measurement.cc:364 zns_measure] m2: 64, 64
[DBG ../src/measurement.cc:376 zns_measure] deltas: m1 64, m2 64
[INFO ../src/measurement.cc:388 zns_measure] READ-1 qd 8, append: mean 176.40625 us, std 24.976062758919788
[INFO ../src/measurement.cc:390 zns_measure] READ-2 qd 8, append: mean 180.578125 us, std 28.239934427763373
[INFO ../src/include/zns_device.h:263 zstore_qpair_teardown] disconnect and free qpair
[INFO ../src/measurement.cc:399 zns_measure] Test start finish


16

[INFO ../src/measurement.cc:317 zns_measure] WRITES-1 qd 16, append: mean 734.609375 us, std 267.5586744755426
[INFO ../src/measurement.cc:319 zns_measure] WRITES-2 qd 16, append: mean 1028.59375 us, std 241.9719585219277
[INFO ../src/measurement.cc:330 zns_measure] current lba for read: device 1 93866873, device 2 93860864
[INFO ../src/measurement.cc:331 zns_measure] read with z_append:
[DBG ../src/measurement.cc:363 zns_measure] m1: 64, 64
[DBG ../src/measurement.cc:364 zns_measure] m2: 64, 64
[DBG ../src/measurement.cc:376 zns_measure] deltas: m1 64, m2 64
[INFO ../src/measurement.cc:388 zns_measure] READ-1 qd 16, append: mean 178.234375 us, std 28.696004832718003
[INFO ../src/measurement.cc:390 zns_measure] READ-2 qd 16, append: mean 166.9375 us, std 36.50208684102869
[INFO ../src/include/zns_device.h:263 zstore_qpair_teardown] disconnect and free qpair
[INFO ../src/measurement.cc:399 zns_measure] Test start finish


32


[INFO ../src/measurement.cc:38 __m_append_complete] setting current lba value: 93860928
[DBG ../src/measurement.cc:292 zns_measure] write is all done
[INFO ../src/measurement.cc:317 zns_measure] WRITES-1 qd 32, append: mean 1230.046875 us, std 648.8304726026163
[INFO ../src/measurement.cc:319 zns_measure] WRITES-2 qd 32, append: mean 1638.6875 us, std 573.052895546083
[INFO ../src/measurement.cc:330 zns_measure] current lba for read: device 1 93866937, device 2 93860928
[INFO ../src/measurement.cc:331 zns_measure] read with z_append:
[DBG ../src/measurement.cc:363 zns_measure] m1: 64, 64
[DBG ../src/measurement.cc:364 zns_measure] m2: 64, 64
[DBG ../src/measurement.cc:376 zns_measure] deltas: m1 64, m2 64
[INFO ../src/measurement.cc:388 zns_measure] READ-1 qd 32, append: mean 171.4375 us, std 31.30439016735512
[INFO ../src/measurement.cc:390 zns_measure] READ-2 qd 32, append: mean 176.703125 us, std 29.489235158518014


64


[DBG ../src/measurement.cc:292 zns_measure] write is all done
[INFO ../src/measurement.cc:317 zns_measure] WRITES-1 qd 64, append: mean 2297.90625 us, std 1254.9870731250332
[INFO ../src/measurement.cc:319 zns_measure] WRITES-2 qd 64, append: mean 2766.046875 us, std 1327.918441933741
[INFO ../src/measurement.cc:330 zns_measure] current lba for read: device 1 93867001, device 2 93860992
[INFO ../src/measurement.cc:331 zns_measure] read with z_append:
[DBG ../src/measurement.cc:363 zns_measure] m1: 64, 64
[DBG ../src/measurement.cc:364 zns_measure] m2: 64, 64
[DBG ../src/measurement.cc:376 zns_measure] deltas: m1 64, m2 64
[INFO ../src/measurement.cc:388 zns_measure] READ-1 qd 64, append: mean 172.890625 us, std 29.946889022223576
[INFO ../src/measurement.cc:390 zns_measure] READ-2 qd 64, append: mean 182.265625 us, std 25.17330070450387
[INFO ../src/include/zns_device.h:263 zstore_qpair_teardown] disconnect and free qpair
[INFO ../src/measurement.cc:399 zns_measure] Test start finish

