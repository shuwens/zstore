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

# 1'000'000

## ckpt
Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency    79.94us   27.05us   1.86ms   98.55%
    Req/Sec    12.40k   428.20    13.38k    72.62%
  Latency Distribution
     50%   78.00us
     75%   85.00us
     90%   92.00us
     99%  109.00us
  310892 requests in 2.10s, 1.21GB read
Requests/sec: 148070.18
Transfer/sec:    590.83MB

## without ckpt

Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency    79.69us   10.55us 216.00us   71.06%
    Req/Sec    12.35k   583.54    13.40k    67.06%
  Latency Distribution
     50%   79.00us
     75%   86.00us
     90%   93.00us
     99%  109.00us
  309547 requests in 2.10s, 1.21GB read
Requests/sec: 147418.50
Transfer/sec:    588.23MB

