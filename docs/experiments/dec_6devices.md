## read 4K 
12 threads and 120 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency   372.66us  231.37us  12.00ms   99.77%
    Req/Sec    27.09k   724.81    32.13k    86.25%
  Latency Distribution
     50%  371.00us
     75%  391.00us
     90%  413.00us
     99%  476.00us
  1647059 requests in 5.10s, 6.42GB read
Requests/sec: 323006.45
Transfer/sec:      1.26GB

## read 4K *2
12 threads and 120 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency   400.33us  249.39us  13.33ms   99.80%
    Req/Sec    25.18k   836.02    33.30k    97.54%
  Latency Distribution
     50%  397.00us
     75%  420.00us
     90%  449.00us
     99%  528.00us
  1527911 requests in 5.10s, 11.78GB read
Requests/sec: 299623.12
Transfer/sec:      2.31GB

## read 4K *4
12 threads and 120 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency   510.03us  660.01us  20.17ms   99.66%
    Req/Sec    20.79k   597.01    24.04k    95.74%
  Latency Distribution
     50%  471.00us
     75%  515.00us
     90%  564.00us
     99%  711.00us
  1263390 requests in 5.10s, 19.38GB read
Requests/sec: 247765.75
Transfer/sec:      3.80GB

## read 4K *8
broken


