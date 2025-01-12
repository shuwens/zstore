
+ hsbench -a test -s test -u http://12.12.12.1:2000 -z 4K -d 5 -t 10 -b 1
2025/01/11 22:05:01 Hotsauce S3 Benchmark Version 0.1
2025/01/11 22:05:01 Parameters:
2025/01/11 22:05:01 url=http://12.12.12.1:2000
2025/01/11 22:05:01 object_prefix=
2025/01/11 22:05:01 bucket_prefix=hotsauce-bench
2025/01/11 22:05:01 region=us-east-1
2025/01/11 22:05:01 modes=cxiplgdcx
2025/01/11 22:05:01 output=
2025/01/11 22:05:01 json_output=
2025/01/11 22:05:01 max_keys=1000
2025/01/11 22:05:01 object_count=-1
2025/01/11 22:05:01 bucket_count=1
2025/01/11 22:05:01 duration=5
2025/01/11 22:05:01 threads=10
2025/01/11 22:05:01 loops=1
2025/01/11 22:05:01 size=4K
2025/01/11 22:05:01 interval=1.000000
2025/01/11 22:05:01 Running Loop 0 BUCKET CLEAR TEST
2025/01/11 22:05:01 Loop: 0, Int: TOTAL, Dur(s): 0.0, Mode: BCLR, Ops: 0, MB/s: 0.00, IO/s: 0, Lat(ms): [ min: 0.0, avg: 0.0, 99%: 0.0, max: 0.0 ], Slowdowns: 0
2025/01/11 22:05:01 Running Loop 0 BUCKET DELETE TEST
2025/01/11 22:05:01 Loop: 0, Int: TOTAL, Dur(s): 0.0, Mode: BDEL, Ops: 1, MB/s: 0.00, IO/s: 3801, Lat(ms): [ min: 0.2, avg: 0.2, 99%: 0.2, max: 0.2 ], Slowdowns: 0
2025/01/11 22:05:01 Running Loop 0 BUCKET INIT TEST
2025/01/11 22:05:01 Loop: 0, Int: TOTAL, Dur(s): 0.0, Mode: BINIT, Ops: 1, MB/s: 0.00, IO/s: 4001, Lat(ms): [ min: 0.2, avg: 0.2, 99%: 0.2, max: 0.2 ], Slowdowns: 0
2025/01/11 22:05:01 Running Loop 0 OBJECT PUT TEST
2025/01/11 22:05:02 Loop: 0, Int: 0, Dur(s): 1.0, Mode: PUT, Ops: 37065, MB/s: 144.79, IO/s: 37065, Lat(ms): [ min: 0.1, avg: 0.3, 99%: 0.7, max: 9.5 ], Slowdowns: 0
2025/01/11 22:05:03 Loop: 0, Int: 1, Dur(s): 1.0, Mode: PUT, Ops: 15088, MB/s: 58.94, IO/s: 15088, Lat(ms): [ min: 0.2, avg: 0.7, 99%: 2.1, max: 5.4 ], Slowdowns: 0
2025/01/11 22:05:04 Loop: 0, Int: 2, Dur(s): 1.0, Mode: PUT, Ops: 15389, MB/s: 60.11, IO/s: 15389, Lat(ms): [ min: 0.2, avg: 0.6, 99%: 2.0, max: 6.7 ], Slowdowns: 0
2025/01/11 22:05:05 Loop: 0, Int: 3, Dur(s): 1.0, Mode: PUT, Ops: 5779, MB/s: 22.57, IO/s: 5779, Lat(ms): [ min: 0.2, avg: 1.7, 99%: 13.0, max: 107.0 ], Slowdowns: 0
2025/01/11 22:05:06 Loop: 0, Int: 4, Dur(s): 1.0, Mode: PUT, Ops: 1254, MB/s: 4.90, IO/s: 1254, Lat(ms): [ min: 0.9, avg: 8.0, 99%: 15.8, max: 78.5 ], Slowdowns: 0
2025/01/11 22:05:06 Loop: 0, Int: TOTAL, Dur(s): 5.0, Mode: PUT, Ops: 74585, MB/s: 58.25, IO/s: 14911, Lat(ms): [ min: 0.1, avg: 0.7, 99%: 8.7, max: 107.0 ], Slowdowns: 0
2025/01/11 22:05:06 Running Loop 0 BUCKET LIST TEST
2025/01/11 22:05:06 Loop: 0, Int: TOTAL, Dur(s): 0.0, Mode: LIST, Ops: 0, MB/s: 0.00, IO/s: 0, Lat(ms): [ min: 0.0, avg: 0.0, 99%: 0.0, max: 0.0 ], Slowdowns: 0
2025/01/11 22:05:06 Running Loop 0 OBJECT GET TEST
2025/01/11 22:05:07 Loop: 0, Int: 0, Dur(s): 1.0, Mode: GET, Ops: 1232, MB/s: 4.81, IO/s: 1232, Lat(ms): [ min: 0.2, avg: 8.0, 99%: 49.3, max: 79.7 ], Slowdowns: 0
2025/01/11 22:05:08 Loop: 0, Int: 1, Dur(s): 1.0, Mode: GET, Ops: 1230, MB/s: 4.80, IO/s: 1230, Lat(ms): [ min: 1.1, avg: 8.2, 99%: 58.8, max: 158.1 ], Slowdowns: 0
2025/01/11 22:05:09 Loop: 0, Int: 2, Dur(s): 1.0, Mode: GET, Ops: 1226, MB/s: 4.79, IO/s: 1226, Lat(ms): [ min: 1.2, avg: 8.1, 99%: 62.9, max: 80.5 ], Slowdowns: 0
2025/01/11 22:05:10 Loop: 0, Int: 3, Dur(s): 1.0, Mode: GET, Ops: 1224, MB/s: 4.78, IO/s: 1224, Lat(ms): [ min: 1.0, avg: 8.2, 99%: 59.1, max: 73.2 ], Slowdowns: 0
2025/01/11 22:05:11 Loop: 0, Int: 4, Dur(s): 1.0, Mode: GET, Ops: 1220, MB/s: 4.77, IO/s: 1220, Lat(ms): [ min: 0.8, avg: 8.2, 99%: 58.5, max: 75.8 ], Slowdowns: 0
2025/01/11 22:05:11 Loop: 0, Int: TOTAL, Dur(s): 5.0, Mode: GET, Ops: 6142, MB/s: 4.80, IO/s: 1228, Lat(ms): [ min: 0.2, avg: 8.1, 99%: 59.1, max: 158.1 ], Slowdowns: 0
2025/01/11 22:05:11 Running Loop 0 OBJECT DELETE TEST
2025/01/11 22:05:12 Loop: 0, Int: 0, Dur(s): 1.0, Mode: DEL, Ops: 1231, MB/s: 4.81, IO/s: 1231, Lat(ms): [ min: 0.3, avg: 8.1, 99%: 58.9, max: 85.1 ], Slowdowns: 0
2025/01/11 22:05:13 Loop: 0, Int: 1, Dur(s): 1.0, Mode: DEL, Ops: 1240, MB/s: 4.84, IO/s: 1240, Lat(ms): [ min: 1.1, avg: 8.1, 99%: 15.3, max: 85.1 ], Slowdowns: 0
2025/01/11 22:05:14 Loop: 0, Int: 2, Dur(s): 1.0, Mode: DEL, Ops: 1230, MB/s: 4.80, IO/s: 1230, Lat(ms): [ min: 1.1, avg: 8.1, 99%: 60.1, max: 144.0 ], Slowdowns: 0
2025/01/11 22:05:15 Loop: 0, Int: 3, Dur(s): 1.0, Mode: DEL, Ops: 1221, MB/s: 4.77, IO/s: 1221, Lat(ms): [ min: 1.2, avg: 8.2, 99%: 58.8, max: 74.6 ], Slowdowns: 0
2025/01/11 22:05:16 Loop: 0, Int: 4, Dur(s): 1.0, Mode: DEL, Ops: 1227, MB/s: 4.79, IO/s: 1227, Lat(ms): [ min: 1.4, avg: 8.1, 99%: 57.8, max: 77.8 ], Slowdowns: 0
2025/01/11 22:05:16 Loop: 0, Int: TOTAL, Dur(s): 5.0, Mode: DEL, Ops: 6159, MB/s: 4.77, IO/s: 1222, Lat(ms): [ min: 0.3, avg: 8.1, 99%: 57.8, max: 144.0 ], Slowdowns: 0
2025/01/11 22:05:16 Running Loop 0 BUCKET CLEAR TEST
2025/01/11 22:05:16 Loop: 0, Int: TOTAL, Dur(s): 0.0, Mode: BCLR, Ops: 0, MB/s: 0.00, IO/s: 0, Lat(ms): [ min: 0.0, avg: 0.0, 99%: 0.0, max: 0.0 ], Slowdowns: 0
2025/01/11 22:05:16 Running Loop 0 BUCKET DELETE TEST
2025/01/11 22:05:16 Loop: 0, Int: TOTAL, Dur(s): 0.0, Mode: BDEL, Ops: 1, MB/s: 0.00, IO/s: 5535, Lat(ms): [ min: 0.2, avg: 0.2, 99%: 0.2, max: 0.2 ], Slowdowns: 0

