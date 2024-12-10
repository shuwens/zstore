## read 4K
12 threads and 120 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency   364.96us   35.44us   1.04ms   77.59%
    Req/Sec    27.19k   782.45    33.00k    96.40%
  Latency Distribution
     50%  362.00us
     75%  381.00us
     90%  407.00us
     99%  468.00us
  1652536 requests in 5.10s, 6.44GB read
Requests/sec: 324081.06
Transfer/sec:      1.26GB

## read 4K *4
12 threads and 120 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency   631.50us  653.13us  19.56ms   88.68%
    Req/Sec    17.96k     0.92k   22.84k    92.31%
  Latency Distribution
     50%  415.00us
     75%    0.92ms
     90%    1.33ms
     99%    2.12ms
  1092171 requests in 5.10s, 16.76GB read
Requests/sec: 214184.98
Transfer/sec:      3.29GB

## read 4K *8
12 threads and 120 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     1.29ms    1.26ms  21.02ms   85.70%
    Req/Sec     9.05k   473.45    11.61k    83.31%
  Latency Distribution
     50%    1.05ms
     75%    1.99ms
     90%    2.87ms
     99%    4.59ms
  550298 requests in 5.10s, 16.84GB read
Requests/sec: 107903.85
Transfer/sec:      3.30GB


## Zstore4 read 4K *8
12 threads and 120 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     1.27ms    1.10ms  14.07ms   82.11%
    Req/Sec     9.09k   446.87    12.47k    81.80%
  Latency Distribution
     50%    0.95ms
     75%    1.99ms
     90%    2.90ms
     99%    4.38ms
  551690 requests in 5.10s, 16.88GB read
Requests/sec: 108185.69
Transfer/sec:      3.31GB


## read 4K *16
12 threads and 120 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     2.64ms    2.46ms  26.68ms   82.32%
    Req/Sec     4.53k   292.94     6.10k    75.08%
  Latency Distribution
     50%    2.26ms
     75%    4.30ms
     90%    6.18ms
     99%    9.31ms
  275130 requests in 5.10s, 16.82GB read
Requests/sec:  53954.34
Transfer/sec:      3.30GB

## read 4K *32
12 threads and 120 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency   541.89ms  865.67ms   3.95s    84.22%
    Req/Sec     1.31k     1.09k    2.63k    45.73%
  Latency Distribution
     50%   11.18ms
     75%  835.06ms
     90%    1.84s
     99%    3.55s
  22072 requests in 5.09s, 2.70GB read
Requests/sec:   4335.99
Transfer/sec:    542.37MB
