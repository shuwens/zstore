#include "include/common.h"
#include "include/configuration.h"
#include "include/device.h"
#include "include/global.h"
#include "include/zstore_controller.h"
#include "spdk/thread.h"
#include <boost/asio/use_awaitable.hpp>
#include <cassert>
#include <cstdlib>
#include <isa-l.h>
#include <sched.h>
#include <shared_mutex>
#include <spdk/event.h>
#include <spdk/nvme.h>
#include <spdk/nvme_zns.h>
#include <sys/time.h>
#include <tuple>
#include <utility>

std::shared_mutex g_shared_mutex_;

// void complete(void *arg, const struct spdk_nvme_cpl *completion)
// {
//     RequestContext *slot = (RequestContext *)arg;
//     ZstoreController *ctrl = (ZstoreController *)slot->ctrl;
//
//     if (spdk_nvme_cpl_is_error(completion)) {
//         log_error("I/O error status: {}",
//                   spdk_nvme_cpl_get_status_string(&completion->status));
//         // fprintf(stderr, "I/O failed, aborting run\n");
//         // assert(0);
//         // exit(1);
//         log_debug("Unimplemented: put context back in pool");
//     }
//
//     auto ioCtx = slot->ioContext;
//     if (slot->is_write) {
//         // TODO: 1. update entry with LBA
//         // TODO: 2. what do we return in response
//         slot->write_complete = true;
//         slot->lba = completion->cdw0;
//         // slot->write_fn(slot->request, slot->entry);
//     } else {
//         // For read, we swap the read date into the request body
//         // std::string body(static_cast<char *>(ioCtx.data), ioCtx.size);
//         // slot->request.body() = body;
//         //
//         // slot->read_fn(slot->request);
//     }
//
//     ctrl->mTotalCounts++;
//     assert(ctrl != nullptr);
//
//     // TODO: this should move to reclaim context and in controller
//     slot->available = true;
//     slot->Clear();
//     ctrl->mRequestContextPool->ReturnRequestContext(slot);
// }

void thread_send_msg(spdk_thread *thread, spdk_msg_fn fn, void *args)
{
    if (spdk_thread_send_msg(thread, fn, args) < 0) {
        log_error("Thread send message failed: thread_name {}.",
                  spdk_thread_get_name(thread));
        exit(-1);
    }
}

int handleDummyRequest(void *args)
{
    bool busy = false;
    // ZstoreController *ctrl = (ZstoreController *)args;
    //
    // if (!ctrl->isDraining &&
    //     ctrl->mRequestContextPool->availableContexts.size() > 0) {
    //     if (!ctrl->start) {
    //         ctrl->start = true;
    //         ctrl->stime = std::chrono::high_resolution_clock::now();
    //     }
    //
    //     RequestContext *slot =
    //         ctrl->mRequestContextPool->GetRequestContext(true);
    //     slot->ctrl = ctrl;
    //     assert(slot->ctrl == ctrl);
    //
    //     auto ioCtx = slot->ioContext;
    //     ioCtx.ns = ctrl->GetDevice(slot->target_dev)->GetNamespace();
    //     ioCtx.qpair = ctrl->GetIoQpair();
    //     ioCtx.data = slot->dataBuffer;
    //     ioCtx.offset = Configuration::GetZslba() + ctrl->mTotalCounts;
    //     ioCtx.size = Configuration::GetDataBufferSizeInSector();
    //     ioCtx.cb = complete;
    //     ioCtx.ctx = slot;
    //     ioCtx.flags = 0;
    //     slot->ioContext = ioCtx;
    //
    //     assert(slot->ioContext.cb != nullptr);
    //     assert(slot->ctrl != nullptr);
    //     ctrl->EnqueueRead(slot);
    //     busy = true;
    // }

    return busy ? SPDK_POLLER_BUSY : SPDK_POLLER_IDLE;
}

int handleHttpRequest(void *args)
{
    bool busy = false;
    ZstoreController *ctrl = (ZstoreController *)args;
    while (ctrl->mIoc_.poll()) {
        busy = true;
    }

    return busy ? SPDK_POLLER_BUSY : SPDK_POLLER_IDLE;
}

int httpWorker(void *args)
{
    IoThread *httpThread = (IoThread *)args;
    struct spdk_thread *thread = httpThread->thread;
    ZstoreController *ctrl = httpThread->controller;
    spdk_set_thread(thread);
    spdk_poller *p;
    p = spdk_poller_register(handleHttpRequest, ctrl, 0);
    ctrl->SetHttpPoller(p);
    while (true) {
        spdk_thread_poll(thread, 0, 0);
    }
}

int dummyWorker(void *args)
{
    IoThread *httpThread = (IoThread *)args;
    struct spdk_thread *thread = httpThread->thread;
    ZstoreController *ctrl = httpThread->controller;
    spdk_set_thread(thread);
    spdk_poller *p;
    p = spdk_poller_register(handleDummyRequest, ctrl, 0);
    ctrl->SetHttpPoller(p);
    while (true) {
        spdk_thread_poll(thread, 0, 0);
    }
}

static void dummy_disconnect_handler(struct spdk_nvme_qpair *qpair,
                                     void *poll_group_ctx)
{
}

int handleIoCompletions(void *args)
{
    struct spdk_nvme_poll_group *pollGroup =
        (struct spdk_nvme_poll_group *)args;
    int r = 0;
    r = spdk_nvme_poll_group_process_completions(pollGroup, 0,
                                                 dummy_disconnect_handler);
    return r > 0 ? SPDK_POLLER_BUSY : SPDK_POLLER_IDLE;
}

int ioWorker(void *args)
{
    IoThread *ioThread = (IoThread *)args;
    struct spdk_thread *thread = ioThread->thread;
    spdk_set_thread(thread);
    spdk_poller_register(handleIoCompletions, ioThread->group, 0);
    while (true) {
        spdk_thread_poll(thread, 0, 0);
    }
}

void spdk_nvme_zone_read_wrapper(
    Timer &timer, struct spdk_thread *thread, struct spdk_nvme_ns *ns,
    struct spdk_nvme_qpair *qpair, void *data, uint64_t offset, uint32_t size,
    uint32_t flags,
    std::move_only_function<void(const spdk_nvme_cpl *completion)> cb)
{
    // auto cb_heap = new decltype(cb)(std::move(cb));

    auto cb_heap = new std::pair<
        std::move_only_function<void(const spdk_nvme_cpl *completion)>,
        Timer &>(decltype(cb)(std::move(cb)), timer);

    auto fn = new std::move_only_function<void(void)>([=, &timer]() {
        timer.t4 = std::chrono::high_resolution_clock::now();

        // log_debug("before spdk cmd read");
        int rc = spdk_nvme_ns_cmd_read(
            ns, qpair, data, offset, size,
            [](void *arg, const spdk_nvme_cpl *completion) mutable {
                // auto tuple_ = static_cast<
                //     std::tuple<void(const spdk_nvme_cpl *completion), Timer
                //     &>>( arg);
                auto tuple_ = reinterpret_cast<decltype(cb_heap)>(arg);
                auto &cb3 = tuple_->first;
                // auto &timer = std::get<1>(*tuple_);
                tuple_->second.t5 = std::chrono::high_resolution_clock::now();
                (cb3)(completion);
                // timer.t5 = std::chrono::high_resolution_clock::now();
                delete tuple_;
            },
            (void *)(cb_heap), flags);
    });
    thread_send_msg(
        thread,
        [](void *fn2) {
            auto rc = reinterpret_cast<decltype(fn)>(fn2);
            (*rc)();
            delete rc;
        },
        fn);
}

auto spdk_nvme_zone_read_async(
    Timer &timer, struct spdk_thread *thread, struct spdk_nvme_ns *ns,
    struct spdk_nvme_qpair *qpair, void *data, uint64_t offset, uint32_t size,
    uint32_t flags) -> net::awaitable<const spdk_nvme_cpl *>
{
    timer.t2 = std::chrono::high_resolution_clock::now();
    auto init = [](auto completion_handler, Timer *timer, spdk_thread *thread,
                   spdk_nvme_ns *ns, spdk_nvme_qpair *qpair, void *data,
                   uint64_t offset, uint32_t size, uint32_t flags) {
        // log_debug("2222");
        timer->t3 = std::chrono::high_resolution_clock::now();
        spdk_nvme_zone_read_wrapper(*timer, thread, ns, qpair, data, offset,
                                    size, flags, std::move(completion_handler));
    };

    return net::async_initiate<decltype(net::use_awaitable),
                               void(const spdk_nvme_cpl *)>(
        init, net::use_awaitable, &timer, thread, ns, qpair, data, offset, size,
        flags);
}

auto zoneRead(void *arg1) -> net::awaitable<void>
{
    RequestContext *ctx = reinterpret_cast<RequestContext *>(arg1);
    auto ioCtx = ctx->ioContext;
    int rc = 0;
    assert(ctx->ctrl != nullptr);
    // log_debug("11111");

    Timer timer;
    timer.t1 = std::chrono::high_resolution_clock::now();
    // ctx->ctrl->CheckIoQpair("Zone read");
    auto cpl = co_await spdk_nvme_zone_read_async(
        timer, ctx->io_thread, ioCtx.ns, ioCtx.qpair, ioCtx.data, ioCtx.offset,
        ioCtx.size, ioCtx.flags);
    timer.t6 = std::chrono::high_resolution_clock::now();

    if (ctx->ctrl->mTotalCounts % 1000 == 0)
        log_debug("t2-t1 {}us, t3-t2 {}us, t4-t3 {}us, t5-t4 {}us, t6-t5 {}us",
                  tdiff_us(timer.t2, timer.t1), tdiff_us(timer.t3, timer.t2),
                  tdiff_us(timer.t4, timer.t3), tdiff_us(timer.t5, timer.t4),
                  tdiff_us(timer.t6, timer.t5));

    if (spdk_nvme_cpl_is_error(cpl)) {
        log_error("I/O error status: {}",
                  spdk_nvme_cpl_get_status_string(&cpl->status));
        // fprintf(stderr, "I/O failed, aborting run\n");
        // assert(0);
        // exit(1);
        log_debug("Unimplemented: put context back in pool");
    }
    // log_debug("11111");

    if (ctx->is_write) {
        // TODO: 1. update entry with LBA
        // TODO: 2. what do we return in response
        ctx->write_complete = true;
        ctx->lba = cpl->cdw0;
        // slot->write_fn(slot->request, slot->entry);
    } else {
        // For read, we swap the read date into the request body
        // std::string body(static_cast<char *>(ioCtx.data), ioCtx.size);
        // slot->request.body() = body;
        //
        // slot->read_fn(slot->request);
    }

    ctx->ctrl->mTotalCounts++;
    assert(ctx->ctrl != nullptr);
}

static void issueIo(void *args)
{
    // ZstoreController *zc = (ZstoreController *)args;
    // RequestContext *slot = zc->mRequestContextPool->GetRequestContext(true);
    //
    // auto ioCtx = slot->ioContext;
    // ioCtx.ns = zc->GetDevice()->GetNamespace();
    // ioCtx.qpair = zc->GetIoQpair();
    // ioCtx.data = slot->dataBuffer;
    // ioCtx.offset = 0;
    // ioCtx.size = Configuration::GetDataBufferSize;
    // ioCtx.cb = complete;
    // ioCtx.ctx = slot;
    // ioCtx.flags = 0;
    //
    // slot->ctrl = zc;
    // assert(slot->ctrl == zc);
    // assert(slot->ctrl != nullptr);
    //
    // thread_send_msg(zc->GetIoThread(0), zoneRead, slot);
}

int handleSubmit(void *args)
{
    bool busy = false;
    ZstoreController *ctrl = (ZstoreController *)args;
    std::queue<RequestContext *> &readQ = ctrl->GetReadQueue();

    while (!readQ.empty()) {
        RequestContext *ctx = readQ.front();
        assert(ctx->ctrl != nullptr);
        // ctrl->ReadInDispatchThread(ctx);

        // if (ctx->curOffset == ctx->size / Configuration::GetBlockSize()) {
        busy = true;
        readQ.pop();
        // } else {
        //     break;
        // }
    }

    // FIXME
    // if (worker->ns_ctx->io_completed > Configuration::GetTotalIo()) {
    //     auto etime = std::chrono::high_resolution_clock::now();
    //     auto delta = std::chrono::duration_cast<std::chrono::microseconds>(
    //                      etime - ctrl->stime)
    //                      .count();
    //     auto tput = worker->ns_ctx->io_completed * g_micro_to_second / delta;
    //
    //     // if (g->verbose)
    //     log_info("Total IO {}, total time {}ms, throughput {} IOPS",
    //              worker->ns_ctx->io_completed, delta, tput);
    //
    //     // log_debug("drain io: {}", spdk_get_ticks());
    //     drain_io(ctrl);
    //     log_debug("clean up ns worker");
    //     ctrl->cleanup_ns_worker_ctx();
    //     print_stats(ctrl);
    //     exit(0);
    // }

    // log_debug("queue depth {}, req in flight {}, completed {}, current queue
    // "
    //           "depth {}",
    //           queue_depth, req_inflight, worker->ns_ctx->io_completed,
    //           worker->ns_ctx->current_queue_depth);
    //
    // while (req_inflight < queue_depth &&
    //        worker->ns_ctx->io_completed < Configuration::GetTotalIo()) {
    //     issueIo(zctrlr);
    //     busy = true;
    // }
    // if (worker->ns_ctx->io_completed > Configuration::GetTotalIo()) {
    //     auto etime = std::chrono::high_resolution_clock::now();
    //     auto delta = std::chrono::duration_cast<std::chrono::microseconds>(
    //                      etime - zctrlr->stime)
    //                      .count();
    //     auto tput = worker->ns_ctx->io_completed * g_micro_to_second / delta;
    //     log_info("Total IO {}, total time {}ms, throughput {} IOPS",
    //              worker->ns_ctx->io_completed, delta, tput);
    //
    //     log_debug("drain io: {}", spdk_get_ticks());
    //     drain_io(zctrlr);
    //     log_debug("clean up ns worker");
    //     zctrlr->cleanup_ns_worker_ctx();
    //     log_debug("end work fn");
    //     print_stats(zctrlr);
    //     exit(0);
    // }
    return busy ? SPDK_POLLER_BUSY : SPDK_POLLER_IDLE;
}

int dispatchWorker(void *args)
{
    ZstoreController *zctrl = (ZstoreController *)args;
    struct spdk_thread *thread = zctrl->GetDispatchThread();
    spdk_set_thread(thread);
    spdk_poller *p;
    p = spdk_poller_register(handleSubmit, zctrl, 0);
    zctrl->SetDispatchPoller(p);
    while (true) {
        spdk_thread_poll(thread, 0, 0);
    }
}

int handleObjectSubmit(void *args)
{
    bool busy = false;
    // ZstoreController *zctrlr = (ZstoreController *)args;
    // int queue_depth = zctrlr->GetQueueDepth();
    // // Multiple threads/readers can read the counter's value at the same
    // time. auto req_inflight = zctrlr->mRequestContextPool->capacity -
    //                     zctrlr->mRequestContextPool->availableContexts.size();
    // while (req_inflight < queue_depth) {
    //     std::string current_key = "key" + std::to_string(zctrlr->pivot);
    //     // MapIter got = zctrlr->mMap.find(curent_key);
    //     // if (got == zctrlr->mMap.end())
    //     //     log_debug("key {} is not in the map", current_key);
    //     // else
    //     //     // log_debug("key {} is not in the map", current_key);
    //     //     // std::cout << got->first << " is " << got->second;
    //     //     MapEntry entry = got->second;
    //     MapEntry entry;
    //     auto res = zctrlr->GetObject(current_key, entry);
    //     // log_debug("Found {}, value {}", current_key, res.value());
    //     // int offset = res.value().second;
    //
    //     // int offset = 0;
    //     // struct ZstoreObject *obj = ReadObject(zctrlr->pivot, zctrlr);
    //
    //     // struct ZstoreObject obj = ReadObject(offset, zctrlr).value();
    //     // inspect(zctrlr);
    //     // struct ZstoreObject *obj = ReadObject(0, zctrlr);
    //     issueIo(zctrlr);
    //     // log_debug("Receive object at LBA {}: key {}, seqnum {}, vernum
    //     {}",
    //     //           offset, obj.key, obj.seqnum, obj.vernum);
    //     zctrlr->pivot++;
    //     busy = true;
    // }
    // // if (worker->ns_ctx->io_completed > Configuration::GetTotalIo()) {
    // //     auto etime = std::chrono::high_resolution_clock::now();
    // //     auto delta =
    // std::chrono::duration_cast<std::chrono::microseconds>(
    // //                      etime - zctrlr->stime)
    // //                      .count();
    // //     auto tput = worker->ns_ctx->io_completed * g_micro_to_second /
    // delta;
    // //
    // //     if (zctrlr->verbose)
    // //         log_info("Total IO {}, total time {}ms, throughput {} IOPS",
    // //                  worker->ns_ctx->io_completed, delta, tput);
    // //
    // //     log_debug("drain io: {}", spdk_get_ticks());
    // //     drain_io(zctrlr);
    // //     log_debug("clean up ns worker");
    // //     zctrlr->cleanup_ns_worker_ctx();
    // //     //
    // //     //     std::vector<uint64_t> deltas1;
    // //     //     for (int i = 0; i < zctrlr->mWorker->ns_ctx->stimes.size();
    // //     i++)
    // //     //     {
    // //     //         deltas1.push_back(
    // //     // std::chrono::duration_cast<std::chrono::microseconds>(
    // //     //                 zctrlr->mWorker->ns_ctx->etimes[i] -
    // //     //                 zctrlr->mWorker->ns_ctx->stimes[i])
    // //     //                 .count());
    // //     //     }
    // //     //     auto sum1 = std::accumulate(deltas1.begin(), deltas1.end(),
    // //     0.0);
    // //     //     auto mean1 = sum1 / deltas1.size();
    // //     //     auto sq_sum1 = std::inner_product(deltas1.begin(),
    // //     deltas1.end(),
    // //     //                                       deltas1.begin(), 0.0);
    // //     //     auto stdev1 = std::sqrt(sq_sum1 / deltas1.size() - mean1 *
    // //     //     mean1); log_info("qd: {}, mean {}, std {}",
    // //     //              zctrlr->mWorker->ns_ctx->io_completed, mean1,
    // //     stdev1);
    // //     //
    // //     //     // clearnup
    // //     //     deltas1.clear();
    // //     //     zctrlr->mWorker->ns_ctx->etimes.clear();
    // //     //     zctrlr->mWorker->ns_ctx->stimes.clear();
    // //     //     // }
    // //     //
    // //     log_debug("end work fn");
    // //     print_stats(zctrlr);
    // //     exit(0);
    // // }
    return busy ? SPDK_POLLER_BUSY : SPDK_POLLER_IDLE;
}

int dispatchObjectWorker(void *args)
{
    ZstoreController *zctrl = (ZstoreController *)args;
    struct spdk_thread *thread = zctrl->GetDispatchThread();
    spdk_set_thread(thread);
    spdk_poller *p;
    // p = spdk_poller_register(handleEventsDispatch, zctrl, 1);
    p = spdk_poller_register(handleObjectSubmit, zctrl, 0);
    zctrl->SetDispatchPoller(p);
    while (true) {
        spdk_thread_poll(thread, 0, 0);
    }
}

int completionWorker(void *args)
{
    // ZstoreController *zctrl = (ZstoreController *)args;
    // struct spdk_thread *thread = zctrl->GetCompletionThread();
    // spdk_set_thread(thread);
    // while (true) {
    //     spdk_thread_poll(thread, 0, 0);
    // }
}

void RequestContext::Clear()
{
    // TODO: double check clear is fully done
    available = true;
    successBytes = 0;
    targetBytes = 0;
    lba = 0;
    // cb_fn = nullptr;
    cb_args = nullptr;
    curOffset = 0;
    ioOffset = 0;
    // status = WRITE_COMPLETE; // added for clean operation

    timestamp = ~0ull;

    // append = false;

    // associatedRequests.clear(); // = nullptr;
    // associatedStripe = nullptr;
    // associatedRead = nullptr;
    // ctrl = nullptr;

    successBytes = 0;
    targetBytes = 0;
}

double RequestContext::GetElapsedTime() { return ctime - stime; }

void RequestContext::Queue()
{
    // struct spdk_thread *th = ctrl->GetDispatchThread();
    // thread_send_msg(ctrl->GetDispatchThread(), handleEventCompletion, this);
    // } else {
    //     event_call(Configuration::GetDispatchThreadCoreId(),
    //                handleEventCompletion2, this, nullptr);
    // }
}

void RequestContext::PrintStats()
{
    // printf("RequestStats: %d %d %lu %d, iocontext: %p %p %lu %d\n", type,
    //        status, lba, size, ioContext.data, ioContext.metadata,
    //        ioContext.offset, ioContext.size);
}

void RequestContext::CopyFrom(const RequestContext &o)
{
    // type = o.type;
    // status = o.status;

    lba = o.lba;
    size = o.size;
    req_type = o.req_type;
    data = o.data;
    // meta = o.meta;
    // pbaArray = o.pbaArray;
    successBytes = o.successBytes;
    targetBytes = o.targetBytes;
    curOffset = o.curOffset;
    // ioOffset = o.ioOffset;
    // cb_fn = o.cb_fn;
    cb_args = o.cb_args;

    available = o.available;

    ctrl = o.ctrl;
    zoneId = o.zoneId;
    offset = o.offset;
    // append = o.append;

    stime = o.stime;
    ctime = o.ctime;

    // associatedRequests = o.associatedRequests;
    // associatedStripe = o.associatedStripe;
    // associatedRead = o.associatedRead;

    // needDegradedRead = o.needDegradedRead;
    // needDegradedRead = o.needDecodeMeta;

    ioContext = o.ioContext;
    // gcTask = o.gcTask;
}

RequestContextPool::RequestContextPool(uint32_t cap)
{
    capacity = cap;
    contexts = new RequestContext[capacity];
    for (uint32_t i = 0; i < capacity; ++i) {
        contexts[i].Clear();
        contexts[i].dataBuffer =
            (uint8_t *)spdk_zmalloc(Configuration::GetDataBufferSizeInSector() *
                                        Configuration::GetBlockSize(),
                                    Configuration::GetBlockSize(), NULL,
                                    SPDK_ENV_SOCKET_ID_ANY, SPDK_MALLOC_DMA);
        // contexts[i].metadataBuffer =
        //     (uint8_t *)spdk_zmalloc(Configuration::GetMaxStripeUnitSize() /
        //                                 Configuration::GetBlockSize() *
        //                                 Configuration::GetMetadataSize(),
        //                             Configuration::GetBlockSize(), NULL,
        //                             SPDK_ENV_SOCKET_ID_ANY, SPDK_MALLOC_DMA);
        contexts[i].bufferSize = Configuration::GetBlockSize();
        availableContexts.emplace_back(&contexts[i]);
    }
}

RequestContext *RequestContextPool::GetRequestContext(bool force)
{
    std::unique_lock lock(g_shared_mutex_);

    RequestContext *ctx = nullptr;
    if (availableContexts.empty() && force == false) {
        ctx = nullptr;
    } else {
        if (!availableContexts.empty()) {
            ctx = availableContexts.back();
            availableContexts.pop_back();
            ctx->Clear();
            ctx->available = false;
        } else {
            ctx = new RequestContext();
            ctx->dataBuffer = (uint8_t *)spdk_zmalloc(
                Configuration::GetDataBufferSizeInSector() *
                    Configuration::GetBlockSize(),
                Configuration::GetBlockSize(), NULL, SPDK_ENV_SOCKET_ID_ANY,
                SPDK_MALLOC_DMA);
            // ctx->metadataBuffer = (uint8_t *)spdk_zmalloc(
            //     Configuration::GetMaxStripeUnitSize() /
            //         Configuration::GetBlockSize() *
            //         Configuration::GetMetadataSize(),
            //     Configuration::GetBlockSize(), NULL, SPDK_ENV_SOCKET_ID_ANY,
            //     SPDK_MALLOC_DMA);
            ctx->bufferSize = Configuration::GetBlockSize();
            ctx->Clear();
            ctx->available = false;
            exit(1);
        }
    }
    return ctx;
}

void RequestContextPool::ReturnRequestContext(RequestContext *slot)
{
    std::unique_lock lock(g_shared_mutex_);

    assert(slot->available);
    if (slot < contexts || slot >= contexts + capacity) {
        // test whether the returned slot is pre-allocated
        spdk_free(slot->dataBuffer);
        delete slot;
    } else {
        assert(availableContexts.size() <= capacity);
        availableContexts.emplace_back(slot);
    }
}

Result<MapEntry> createMapEntry(DevTuple tuple, int32_t lba1, int32_t lba2,
                                int32_t lba3)
{

    auto data = std::make_tuple(
        std::make_pair(std::get<0>(tuple).first + std::get<0>(tuple).second,
                       lba1),
        std::make_pair(std::get<1>(tuple).first + std::get<1>(tuple).second,
                       lba2),
        std::make_pair(std::get<2>(tuple).first + std::get<2>(tuple).second,
                       lba3));
    MapEntry entry = {.data = data};
    return entry;
}

Result<DevTuple> GetDevTuple(ObjectKey object_key)
{
    return std::make_tuple(std::make_pair("Zstore2", "Dev1"),
                           std::make_pair("Zstore3", "Dev1"),
                           std::make_pair("Zstore4", "Dev1"));
}

Result<RequestContext *> MakeReadRequest(ZstoreController *zctrl_, Device *dev,
                                         uint64_t offset, HttpRequest request)
{
    RequestContext *slot = zctrl_->mRequestContextPool->GetRequestContext(true);
    slot->ctrl = zctrl_;
    assert(slot->ctrl == zctrl_);
    // log_debug("11111");

    auto ioCtx = slot->ioContext;
    ioCtx.ns = dev->GetNamespace();
    ioCtx.qpair = dev->GetIoQueue(0);
    ioCtx.data = slot->dataBuffer;
    ioCtx.offset = Configuration::GetZslba() + offset;
    ioCtx.size = Configuration::GetDataBufferSizeInSector();
    // ioCtx.cb = complete;
    // ioCtx.ctx = slot;
    ioCtx.flags = 0;
    slot->ioContext = ioCtx;

    slot->io_thread = zctrl_->GetIoThread(0);
    // Request is read
    slot->is_write = false;
    // slot->target_dev = false;
    slot->request = std::move(request);
    // slot->read_fn = closure;
    // assert(slot->ioContext.cb != nullptr);
    assert(slot->ctrl != nullptr);
    // log_debug("11111");

    return slot;
}

Result<RequestContext *> MakeWriteRequest(ZstoreController *zctrl_, Device *dev,
                                          HttpRequest request, MapEntry entry)
{
    RequestContext *slot = zctrl_->mRequestContextPool->GetRequestContext(true);
    slot->ctrl = zctrl_;
    assert(slot->ctrl == zctrl_);

    auto ioCtx = slot->ioContext;
    ioCtx.ns = dev->GetNamespace();
    ioCtx.qpair = dev->GetIoQueue(0);
    ioCtx.data = slot->dataBuffer;
    ioCtx.offset = Configuration::GetZslba();
    ioCtx.size = Configuration::GetDataBufferSizeInSector();
    // ioCtx.cb = complete;
    // ioCtx.ctx = slot;
    ioCtx.flags = 0;
    slot->ioContext = ioCtx;

    // Write request
    slot->is_write = true;
    slot->request = std::move(request);
    // slot->write_fn = closure;
    slot->entry = std::move(entry);
    // assert(slot->ioContext.cb != nullptr);
    assert(slot->ctrl != nullptr);

    return slot;
}
