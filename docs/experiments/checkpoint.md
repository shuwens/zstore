with ckpt:  149926.05
    Latency    78.33us   10.15us 253.00us   70.04%
    Req/Sec    12.56k   493.51    13.47k    66.67%
  Latency Distribution
     50%   77.00us
     75%   84.00us
     90%   91.00us
     99%  107.00us
  314760 requests in 2.10s, 1.23GB read
Requests/sec: 149926.05
Transfer/sec:    598.24MB

without ckpt: 151275.19

 12 threads and 12 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency    77.68us    9.59us 253.00us   69.76%
    Req/Sec    12.67k   457.44    13.52k    67.86%
  Latency Distribution
     50%   77.00us
     75%   83.00us
     90%   90.00us
     99%  105.00us
  317595 requests in 2.10s, 1.24GB read
Requests/sec: 151275.19
Transfer/sec:    603.62MB

