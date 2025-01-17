# 500'000 with full capacity

## ckpt

    Latency   151.82us   37.39us   1.43ms   75.58%
    Req/Sec    32.23k     1.30k   35.64k    89.68%
  Latency Distribution
     50%  148.00us
     75%  170.00us
     90%  196.00us
     99%  268.00us
  808043 requests in 2.10s, 3.15GB read
Requests/sec: 384859.72
Transfer/sec:      1.50GB


## without ckpt
   Latency   151.67us   33.66us 493.00us   72.73%
    Req/Sec    32.25k     1.15k   37.12k    89.29%
  Latency Distribution
     50%  149.00us
     75%  170.00us
     90%  194.00us
     99%  254.00us
  808272 requests in 2.10s, 3.15GB read
Requests/sec: 384903.34


# 1'000'000 with full capacity

## ckpt

Latency   153.34us   47.99us   1.87ms   82.29%
    Req/Sec    32.02k     1.64k   36.26k    84.52%
  Latency Distribution
     50%  148.00us
     75%  171.00us
     90%  200.00us
     99%  284.00us
  802490 requests in 2.10s, 3.13GB read
Requests/sec: 382136.09
Transfer/sec:      1.49GB


## without ckpt

 Latency   152.10us   40.07us 413.00us   74.23%
    Req/Sec    32.16k     1.41k   36.79k    84.52%
  Latency Distribution
     50%  148.00us
     75%  172.00us
     90%  202.00us
     99%  275.00us
  806421 requests in 2.10s, 3.14GB read
Requests/sec: 384004.51
Transfer/sec:      1.50GB

# 2'000'000 with full capacity

## ckpt
   Latency   155.96us   88.15us   3.89ms   96.13%
    Req/Sec    31.91k     1.60k   38.01k    82.54%
  Latency Distribution
     50%  149.00us
     75%  173.00us
     90%  202.00us
     99%  281.00us
  799900 requests in 2.10s, 3.12GB read
Requests/sec: 380926.71
Transfer/sec:      1.48GB

## without ckpt

   Latency   151.61us   36.30us 419.00us   73.74%
    Req/Sec    32.25k     1.25k   36.43k    89.29%
  Latency Distribution
     50%  148.00us
     75%  171.00us
     90%  196.00us
     99%  268.00us
  808554 requests in 2.10s, 3.15GB read
Requests/sec: 385005.73
Transfer/sec:      1.50GB

