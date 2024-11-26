## Code notes
 * The object cannot be destroyed .

## ownership notes
 * The two less interesting cases:

## DPDK notes
 * SPDK Ubuntu's DPDK package 

## Immediate TODOs
 * This list should be empty :)
 * read object workflow: handle request, read into object etc
 * write object worklfow: handle request, write into object etc
 * format the broken drives

## Short-term TODOs
 * key experiment tput/latency: read
 * key experiment tput/latency: append
 * key experiment tput/latency: target failure
 * key experiment tput/latency: gw failure
 * key experiment tput/latency: GC
 * key experiment tput/latency: checkpoint
 * Need to have a test for object
 * crafting map for recovery: two devices 
 * crafting map for read and writes
- [x] different object sizes
- [ ] some functional or correctness tests
- [ ] failure recover things 
- [ ] RoCE2
- [ ] RDMA hash test

## OSDI TODOs
- [ ] read the paper
- [ ] compare to other systems
- [ ] perf is not stuck with 100k, try different IO threads?

## Long-term TODOs
 * read zone header broken, we should be able to read this and store the zones
   we need to write to in the future
 * Need to have a test for  overall system

## Longer-term TODOs
 * Need to have a test for 

## S3 APIs
[DBG ../src/include/http_server.h:156 awaitable_on_request] req GET target /hotsauce-bench000000000000, body
[DBG ../src/include/http_server.h:163 awaitable_on_request] req DELETE target /hotsauce-bench000000000000, body
[DBG ../src/include/http_server.h:160 awaitable_on_request] req PUT target /hotsauce-bench000000000000, body
[DBG ../src/include/http_server.h:160 awaitable_on_request] req PUT target /hotsauce-bench000000000000/000000000000, body
[DBG ../src/include/http_server.h:160 awaitable_on_request] req PUT target /hotsauce-bench000000000000/000000000401, body
[DBG ../src/include/http_server.h:160 awaitable_on_request] req PUT target /hotsauce-bench000000000000/000000012452, body
[DBG ../src/include/http_server.h:160 awaitable_on_request] req PUT target /hotsauce-bench000000000000/000000012453, body
[DBG ../src/include/http_server.h:160 awaitable_on_request] req PUT target /hotsauce-bench000000000000/000000012454, body
[DBG ../src/include/http_server.h:160 awaitable_on_request] req PUT target /hotsauce-bench000000000000/000000012455, body
[DBG ../src/include/http_server.h:160 awaitable_on_request] req PUT target /hotsauce-bench000000000000/000000012456, body
[DBG ../src/include/http_server.h:160 awaitable_on_request] req PUT target /hotsauce-bench000000000000/000000012457, body
[DBG ../src/include/http_server.h:156 awaitable_on_request] req GET target /hotsauce-bench000000000000?max-keys=1000, body
[DBG ../src/include/http_server.h:156 awaitable_on_request] req GET target /hotsauce-bench000000000000/000000000000, body
[DBG ../src/include/http_server.h:156 awaitable_on_request] req GET target /hotsauce-bench000000000000/000000000001, body
[DBG ../src/include/http_server.h:156 awaitable_on_request] req GET target /hotsauce-bench000000000000/000000000002, body
[DBG ../src/include/http_server.h:156 awaitable_on_request] req GET target /hotsauce-bench000000000000/000000012453, body
[DBG ../src/include/http_server.h:156 awaitable_on_request] req GET target /hotsauce-bench000000000000/000000012454, body
[DBG ../src/include/http_server.h:156 awaitable_on_request] req GET target /hotsauce-bench000000000000/000000012455, body
[DBG ../src/include/http_server.h:156 awaitable_on_request] req GET target /hotsauce-bench000000000000/000000012456, body
[DBG ../src/include/http_server.h:156 awaitable_on_request] req GET target /hotsauce-bench000000000000/000000012457, body
[DBG ../src/include/http_server.h:163 awaitable_on_request] req DELETE target /hotsauce-bench000000000000/000000000000, body
[DBG ../src/include/http_server.h:163 awaitable_on_request] req DELETE target /hotsauce-bench000000000000/000000000001, body
[DBG ../src/include/http_server.h:163 awaitable_on_request] req DELETE target /hotsauce-bench000000000000/000000012452, body
[DBG ../src/include/http_server.h:163 awaitable_on_request] req DELETE target /hotsauce-bench000000000000/000000012453, body
[DBG ../src/include/http_server.h:163 awaitable_on_request] req DELETE target /hotsauce-bench000000000000/000000012454, body
[DBG ../src/include/http_server.h:163 awaitable_on_request] req DELETE target /hotsauce-bench000000000000/000000012455, body
[DBG ../src/include/http_server.h:163 awaitable_on_request] req DELETE target /hotsauce-bench000000000000/000000012456, body
[DBG ../src/include/http_server.h:163 awaitable_on_request] req DELETE target /hotsauce-bench000000000000/000000012457, body
[DBG ../src/include/http_server.h:156 awaitable_on_request] req GET target /hotsauce-bench000000000000, body
[DBG ../src/include/http_server.h:163 awaitable_on_request] req DELETE target /hotsauce-bench000000000000, body

