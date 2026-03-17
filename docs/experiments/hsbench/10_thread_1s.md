
+ hsbench -a test -s test -u http://12.12.12.1:2000 -z 4K -d 1 -t 10 -b 1
2025/01/11 22:02:29 Hotsauce S3 Benchmark Version 0.1
2025/01/11 22:02:29 Parameters:
2025/01/11 22:02:29 url=http://12.12.12.1:2000
2025/01/11 22:02:29 object_prefix=
2025/01/11 22:02:29 bucket_prefix=hotsauce-bench
2025/01/11 22:02:29 region=us-east-1
2025/01/11 22:02:29 modes=cxiplgdcx
2025/01/11 22:02:29 output=
2025/01/11 22:02:29 json_output=
2025/01/11 22:02:29 max_keys=1000
2025/01/11 22:02:29 object_count=-1
2025/01/11 22:02:29 bucket_count=1
2025/01/11 22:02:29 duration=1
2025/01/11 22:02:29 threads=10
2025/01/11 22:02:29 loops=1
2025/01/11 22:02:29 size=4K
2025/01/11 22:02:29 interval=1.000000
2025/01/11 22:02:29 Running Loop 0 BUCKET CLEAR TEST
2025/01/11 22:02:29 Loop: 0, Int: TOTAL, Dur(s): 0.0, Mode: BCLR, Ops: 0, MB/s: 0.00, IO/s: 0, Lat(ms): [ min: 0.0, avg: 0.0, 99%: 0.0, max: 0.0 ], Slowdowns: 0
2025/01/11 22:02:29 Running Loop 0 BUCKET DELETE TEST
2025/01/11 22:02:29 Loop: 0, Int: TOTAL, Dur(s): 0.0, Mode: BDEL, Ops: 1, MB/s: 0.00, IO/s: 3561, Lat(ms): [ min: 0.2, avg: 0.2, 99%: 0.2, max: 0.2 ], Slowdowns: 0
2025/01/11 22:02:29 Running Loop 0 BUCKET INIT TEST
2025/01/11 22:02:29 Loop: 0, Int: TOTAL, Dur(s): 0.0, Mode: BINIT, Ops: 1, MB/s: 0.00, IO/s: 3937, Lat(ms): [ min: 0.2, avg: 0.2, 99%: 0.2, max: 0.2 ], Slowdowns: 0
2025/01/11 22:02:29 Running Loop 0 OBJECT PUT TEST
2025/01/11 22:02:30 Loop: 0, Int: 0, Dur(s): 1.0, Mode: PUT, Ops: 37299, MB/s: 145.70, IO/s: 37299, Lat(ms): [ min: 0.1, avg: 0.3, 99%: 0.5, max: 3.1 ], Slowdowns: 0
2025/01/11 22:02:30 Loop: 0, Int: TOTAL, Dur(s): 1.0, Mode: PUT, Ops: 37309, MB/s: 145.24, IO/s: 37183, Lat(ms): [ min: 0.1, avg: 0.3, 99%: 0.6, max: 3.1 ], Slowdowns: 0
2025/01/11 22:02:30 Running Loop 0 BUCKET LIST TEST
2025/01/11 22:02:30 Loop: 0, Int: TOTAL, Dur(s): 0.0, Mode: LIST, Ops: 0, MB/s: 0.00, IO/s: 0, Lat(ms): [ min: 0.0, avg: 0.0, 99%: 0.0, max: 0.0 ], Slowdowns: 0
2025/01/11 22:02:30 Running Loop 0 OBJECT GET TEST
2025/01/11 22:02:31 Loop: 0, Int: 0, Dur(s): 1.0, Mode: GET, Ops: 15850, MB/s: 61.91, IO/s: 15850, Lat(ms): [ min: 0.1, avg: 0.6, 99%: 2.5, max: 15.0 ], Slowdowns: 0
2025/01/11 22:02:31 Loop: 0, Int: TOTAL, Dur(s): 1.0, Mode: GET, Ops: 15860, MB/s: 61.84, IO/s: 15830, Lat(ms): [ min: 0.1, avg: 0.6, 99%: 2.5, max: 15.0 ], Slowdowns: 0
2025/01/11 22:02:31 Running Loop 0 OBJECT DELETE TEST
2025/01/11 22:02:32 Loop: 0, Int: TOTAL, Dur(s): 1.0, Mode: DEL, Ops: 17855, MB/s: 69.74, IO/s: 17853, Lat(ms): [ min: 0.1, avg: 0.6, 99%: 3.2, max: 26.6 ], Slowdowns: 0
2025/01/11 22:02:32 Running Loop 0 BUCKET CLEAR TEST
2025/01/11 22:02:32 Loop: 0, Int: TOTAL, Dur(s): 0.0, Mode: BCLR, Ops: 0, MB/s: 0.00, IO/s: 0, Lat(ms): [ min: 0.0, avg: 0.0, 99%: 0.0, max: 0.0 ], Slowdowns: 0
2025/01/11 22:02:32 Running Loop 0 BUCKET DELETE TEST
2025/01/11 22:02:32 Loop: 0, Int: TOTAL, Dur(s): 0.0, Mode: BDEL, Ops: 1, MB/s: 0.00, IO/s: 8643, Lat(ms): [ min: 0.1, avg: 0.1, 99%: 0.1, max: 0.1 ], Slowdowns: 0

