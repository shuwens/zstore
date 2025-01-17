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

  12 threads and 12 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency    84.57us   10.07us 255.00us   69.71%
    Req/Sec    11.66k   293.41    12.19k    60.71%
  Latency Distribution
     50%   84.00us
     75%   91.00us
     90%   98.00us
     99%  113.00us
  292305 requests in 2.10s, 1.14GB read
Requests/sec: 139215.79

 Latency    81.22us   11.27us 253.00us   72.67%
    Req/Sec    12.11k   457.88    12.82k    64.68%
  Latency Distribution
     50%   80.00us
     75%   88.00us
     90%   96.00us
     99%  114.00us
  303723 requests in 2.10s, 1.18GB read
Requests/sec: 144637.03
Transfer/sec:    577.13MB

# 10'000'000 with full capacity

## ckpt

  Latency    33.23ms   59.18ms 250.70ms   82.39%
    Req/Sec    14.77k     5.80k   30.04k    66.94%
  Latency Distribution
     50%  171.00us
     75%   44.04ms
     90%  142.52ms
     99%  201.79ms
  365442 requests in 2.10s, 1.42GB read
Requests/sec: 174048.43
Transfer/sec:    694.53MB


Latency    33.20ms   59.91ms 418.39ms   82.62%
    Req/Sec    14.60k     5.63k   29.25k    68.00%
  Latency Distribution
     50%  172.00us
     75%   42.30ms
     90%  142.74ms
     99%  203.36ms
  364258 requests in 2.10s, 1.42GB read
Requests/sec: 173497.17
Transfer/sec:    692.33MB


## without ckpt

Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency    36.50ms   76.91ms 819.87ms   85.42%
    Req/Sec    14.41k     4.73k   28.91k    76.19%
  Latency Distribution
     50%  177.00us
     75%   39.90ms
     90%  145.88ms
     99%  300.26ms
  361330 requests in 2.10s, 1.41GB read
Requests/sec: 172088.62
Transfer/sec:    686.71MB



# 4'000'000

## ckpt

## without ckpt
    Latency   153.87us   38.19us 531.00us   74.65%
    Req/Sec    31.81k     1.29k   34.83k    84.52%
  Latency Distribution
     50%  150.00us
     75%  173.00us
     90%  200.00us
     99%  275.00us
  797512 requests in 2.10s, 3.11GB read
Requests/sec: 379769.25
Transfer/sec:      1.48GB



# 40'000'000


