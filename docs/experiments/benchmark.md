## s3bench


+ s3bench -accessKey=KEY -accessSecret=SECRET -bucket=db -endpoint=http://localhost:2000 -numClients=100 -numSamples=1000000 -objectSize=131 -operations=read -skipBucketCreate -skipCleanup -cpuprofile
Version 1.0
Test parameters
endpoint(s):      [http://localhost:2000]
bucket:           db
objectNamePrefix: zstore1_loadgen_test/
objectSize:       0.0001 MB
numClients:       100
numSamples:       1000000
batchSize:       1000
Total size of data set : 0.1220 GB
verbose:       false


2024/09/27 20:48:02 skipping bucket creation..it better exist!
Running Read test...
Test parameters
endpoint(s):      [http://localhost:2000]
bucket:           db
objectNamePrefix: zstore1_loadgen_test/
objectSize:       0.0001 MB
numClients:       100
numSamples:       1000000
batchSize:       1000
Total size of data set : 0.1220 GB
verbose:       false

Results Summary for Read Operation(s)
Total Transferred: 124.931 MB
Total Throughput:  2.00 MB/s
Ops/sec:  16034.50 ops/s
Total Duration:    62.366 s
Number of Errors:  0
------------------------------------
Read times Max:       62.0607 s
Read times 99th %ile: 0.0039 s
Read times 90th %ile: 0.0005 s
Read times 75th %ile: 0.0005 s
Read times 50th %ile: 0.0004 s
Read times 25th %ile: 0.0004 s
Read times Min:       0.0001 s


## wrk

[21:27] zstore1:wrk (master) | ./wrk -t12 -c400 -d30s http://localhost:2000/db/bar
Running 30s test @ http://localhost:2000/db/bar
  12 threads and 400 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency   472.16us  409.37us   4.64ms   98.59%
    Req/Sec     9.55k     7.44k   18.36k    58.00%
  570674 requests in 30.04s, 198.65MB read
Requests/sec:  18994.65
Transfer/sec:      6.61MB



## warp

./warp get --duration=3m --host=localhost:2000 --access-key=minio --secret-key=minio123 --autoterm

