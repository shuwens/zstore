+ hsbench -a test -s test -u http://12.12.12.1:2000 -z 4K -d 1 -t 20 -b 1
2025/01/11 22:18:25 Hotsauce S3 Benchmark Version 0.1
2025/01/11 22:18:25 Parameters:
2025/01/11 22:18:25 url=http://12.12.12.1:2000
2025/01/11 22:18:25 object_prefix=
2025/01/11 22:18:25 bucket_prefix=hotsauce-bench
2025/01/11 22:18:25 region=us-east-1
2025/01/11 22:18:25 modes=cxiplgdcx
2025/01/11 22:18:25 output=
2025/01/11 22:18:25 json_output=
2025/01/11 22:18:25 max_keys=1000
2025/01/11 22:18:25 object_count=-1
2025/01/11 22:18:25 bucket_count=1
2025/01/11 22:18:25 duration=1
2025/01/11 22:18:25 threads=20
2025/01/11 22:18:25 loops=1
2025/01/11 22:18:25 size=4K
2025/01/11 22:18:25 interval=1.000000
2025/01/11 22:18:25 Running Loop 0 BUCKET CLEAR TEST
2025/01/11 22:18:25 Loop: 0, Int: TOTAL, Dur(s): 0.0, Mode: BCLR, Ops: 0, MB/s: 0.00, IO/s: 0, Lat(ms): [ min: 0.0, avg: 0.0, 99%: 0.0, max: 0.0 ], Slowdowns: 0
2025/01/11 22:18:25 Running Loop 0 BUCKET DELETE TEST
2025/01/11 22:18:25 Loop: 0, Int: TOTAL, Dur(s): 0.0, Mode: BDEL, Ops: 1, MB/s: 0.00, IO/s: 3945, Lat(ms): [ min: 0.2, avg: 0.2, 99%: 0.2, max: 0.2 ], Slowdowns: 0
2025/01/11 22:18:25 Running Loop 0 BUCKET INIT TEST
2025/01/11 22:18:25 Loop: 0, Int: TOTAL, Dur(s): 0.0, Mode: BINIT, Ops: 1, MB/s: 0.00, IO/s: 2114, Lat(ms): [ min: 0.4, avg: 0.4, 99%: 0.4, max: 0.4 ], Slowdowns: 0
2025/01/11 22:18:25 Running Loop 0 OBJECT PUT TEST
2025/01/11 22:18:26 Loop: 0, Int: TOTAL, Dur(s): 1.0, Mode: PUT, Ops: 37789, MB/s: 147.44, IO/s: 37745, Lat(ms): [ min: 0.2, avg: 0.5, 99%: 3.5, max: 52.1 ], Slowdowns: 0
2025/01/11 22:18:26 Running Loop 0 BUCKET LIST TEST
2025/01/11 22:18:26 Loop: 0, Int: TOTAL, Dur(s): 0.0, Mode: LIST, Ops: 0, MB/s: 0.00, IO/s: 0, Lat(ms): [ min: 0.0, avg: 0.0, 99%: 0.0, max: 0.0 ], Slowdowns: 0
2025/01/11 22:18:26 Running Loop 0 OBJECT GET TEST
2025/01/11 22:18:27 Loop: 0, Int: 0, Dur(s): 1.0, Mode: GET, Ops: 15391, MB/s: 60.12, IO/s: 15391, Lat(ms): [ min: 0.1, avg: 1.2, 99%: 14.6, max: 37.6 ], Slowdowns: 0
2025/01/11 22:18:27 Loop: 0, Int: TOTAL, Dur(s): 1.0, Mode: GET, Ops: 15411, MB/s: 59.91, IO/s: 15336, Lat(ms): [ min: 0.1, avg: 1.2, 99%: 15.1, max: 37.6 ], Slowdowns: 0
2025/01/11 22:18:27 Running Loop 0 OBJECT DELETE TEST
2025/01/11 22:18:28 Loop: 0, Int: 0, Dur(s): 1.0, Mode: DEL, Ops: 15803, MB/s: 61.73, IO/s: 15803, Lat(ms): [ min: 0.1, avg: 1.3, 99%: 17.6, max: 109.8 ], Slowdowns: 0
2025/01/11 22:18:28 Loop: 0, Int: TOTAL, Dur(s): 1.0, Mode: DEL, Ops: 15823, MB/s: 61.24, IO/s: 15677, Lat(ms): [ min: 0.1, avg: 1.3, 99%: 18.4, max: 109.8 ], Slowdowns: 0
2025/01/11 22:18:28 Running Loop 0 BUCKET CLEAR TEST
2025/01/11 22:18:28 Loop: 0, Int: TOTAL, Dur(s): 0.0, Mode: BCLR, Ops: 0, MB/s: 0.00, IO/s: 0, Lat(ms): [ min: 0.0, avg: 0.0, 99%: 0.0, max: 0.0 ], Slowdowns: 0
2025/01/11 22:18:28 Running Loop 0 BUCKET DELETE TEST
2025/01/11 22:18:28 Loop: 0, Int: TOTAL, Dur(s): 0.0, Mode: BDEL, Ops: 1, MB/s: 0.00, IO/s: 7271, Lat(ms): [ min: 0.1, avg: 0.1, 99%: 0.1, max: 0.1 ], Slowdowns: 0

