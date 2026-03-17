
+ hsbench -a test -s test -u http://12.12.12.1:2000 -z 4K -d 1 -t 1 -b 1
2025/01/11 21:48:36 Hotsauce S3 Benchmark Version 0.1
2025/01/11 21:48:36 Parameters:
2025/01/11 21:48:36 url=http://12.12.12.1:2000
2025/01/11 21:48:36 object_prefix=
2025/01/11 21:48:36 bucket_prefix=hotsauce-bench
2025/01/11 21:48:36 region=us-east-1
2025/01/11 21:48:36 modes=cxiplgdcx
2025/01/11 21:48:36 output=
2025/01/11 21:48:36 json_output=
2025/01/11 21:48:36 max_keys=1000
2025/01/11 21:48:36 object_count=-1
2025/01/11 21:48:36 bucket_count=1
2025/01/11 21:48:36 duration=1
2025/01/11 21:48:36 threads=1
2025/01/11 21:48:36 loops=1
2025/01/11 21:48:36 size=4K
2025/01/11 21:48:36 interval=1.000000
2025/01/11 21:48:36 Running Loop 0 BUCKET CLEAR TEST
2025/01/11 21:48:36 Loop: 0, Int: TOTAL, Dur(s): 0.0, Mode: BCLR, Ops: 0, MB/s: 0.00, IO/s: 0, Lat(ms): [ min: 0.0, avg: 0.0, 99%: 0.0, max: 0.0 ], Slowdowns: 0
2025/01/11 21:48:36 Running Loop 0 BUCKET DELETE TEST
2025/01/11 21:48:36 Loop: 0, Int: TOTAL, Dur(s): 0.0, Mode: BDEL, Ops: 1, MB/s: 0.00, IO/s: 3279, Lat(ms): [ min: 0.2, avg: 0.2, 99%: 0.2, max: 0.2 ], Slowdowns: 0
2025/01/11 21:48:36 Running Loop 0 BUCKET INIT TEST
2025/01/11 21:48:36 Loop: 0, Int: TOTAL, Dur(s): 0.0, Mode: BINIT, Ops: 1, MB/s: 0.00, IO/s: 5199, Lat(ms): [ min: 0.2, avg: 0.2, 99%: 0.2, max: 0.2 ], Slowdowns: 0
2025/01/11 21:48:36 Running Loop 0 OBJECT PUT TEST
2025/01/11 21:48:37 Loop: 0, Int: 0, Dur(s): 1.0, Mode: PUT, Ops: 4964, MB/s: 19.39, IO/s: 4964, Lat(ms): [ min: 0.2, avg: 0.2, 99%: 0.3, max: 7.7 ], Slowdowns: 0
2025/01/11 21:48:37 Loop: 0, Int: TOTAL, Dur(s): 1.0, Mode: PUT, Ops: 4965, MB/s: 19.39, IO/s: 4964, Lat(ms): [ min: 0.2, avg: 0.2, 99%: 0.3, max: 7.7 ], Slowdowns: 0
2025/01/11 21:48:37 Running Loop 0 BUCKET LIST TEST
2025/01/11 21:48:37 Loop: 0, Int: TOTAL, Dur(s): 0.0, Mode: LIST, Ops: 0, MB/s: 0.00, IO/s: 0, Lat(ms): [ min: 0.0, avg: 0.0, 99%: 0.0, max: 0.0 ], Slowdowns: 0
2025/01/11 21:48:37 Running Loop 0 OBJECT GET TEST
2025/01/11 21:48:37 Loop: 0, Int: TOTAL, Dur(s): 0.7, Mode: GET, Ops: 4965, MB/s: 26.61, IO/s: 6813, Lat(ms): [ min: 0.1, avg: 0.1, 99%: 0.2, max: 0.4 ], Slowdowns: 0
2025/01/11 21:48:37 Running Loop 0 OBJECT DELETE TEST
2025/01/11 21:48:38 Loop: 0, Int: TOTAL, Dur(s): 0.4, Mode: DEL, Ops: 4965, MB/s: 48.89, IO/s: 12517, Lat(ms): [ min: 0.1, avg: 0.1, 99%: 0.2, max: 0.4 ], Slowdowns: 0
2025/01/11 21:48:38 Running Loop 0 BUCKET CLEAR TEST
2025/01/11 21:48:38 Loop: 0, Int: TOTAL, Dur(s): 0.0, Mode: BCLR, Ops: 0, MB/s: 0.00, IO/s: 0, Lat(ms): [ min: 0.0, avg: 0.0, 99%: 0.0, max: 0.0 ], Slowdowns: 0
2025/01/11 21:48:38 Running Loop 0 BUCKET DELETE TEST
2025/01/11 21:48:38 Loop: 0, Int: TOTAL, Dur(s): 0.0, Mode: BDEL, Ops: 1, MB/s: 0.00, IO/s: 9582, Lat(ms): [ min: 0.1, avg: 0.1, 99%: 0.1, max: 0.1 ], Slowdowns: 0

