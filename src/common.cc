#include "include/common.h"
#include "include/configuration.h"
#include "include/object.h"
#include "include/utils.hpp"
#include "include/zns_utils.h"
#include "include/zstore_controller.h"
#include "object.cc"
#include "spdk/thread.h"
#include <cstdlib>
#include <isa-l.h>
#include <queue>
#include <sched.h>
#include <spdk/event.h>
#include <sys/time.h>

void complete(void *arg, const struct spdk_nvme_cpl *completion)
{
    RequestContext *slot = (RequestContext *)arg;

    if (spdk_nvme_cpl_is_error(completion)) {
        fprintf(stderr, "I/O error status: %s\n",
                spdk_nvme_cpl_get_status_string(&completion->status));
        fprintf(stderr, "I/O failed, aborting run\n");
        assert(0);
        exit(1);
    }

    ZstoreController *ctrl = (ZstoreController *)slot->ctrl;

    // FIXME
    // assert(ctrl != nullptr);
    if (ctrl == nullptr)
        ctrl = gZstoreController;

    auto worker = ctrl->GetWorker();
    assert(worker != nullptr);
    assert(worker->ns_ctx != nullptr);
    //
    // worker->ns_ctx->current_queue_depth--;
    worker->ns_ctx->io_completed++;

    // this should move to reclaim context and in controller
    {
        std::unique_lock lock(gZstoreController->context_pool_mutex_);
        slot->available = true;
        slot->Clear();
        // if (ctrl->verbose)
        //     log_error("Before return: pool capacity {}, pool available {}",
        //               ctrl->mRequestContextPool->capacity,
        //               ctrl->mRequestContextPool->availableContexts.size());
        gZstoreController->mRequestContextPool->ReturnRequestContext(slot);
    }
    // if (ctrl->verbose)
    //     log_error("After return: pool capacity {}, pool available {}\n",
    //               ctrl->mRequestContextPool->capacity,
    //               ctrl->mRequestContextPool->availableContexts.size());
}

void thread_send_msg(spdk_thread *thread, spdk_msg_fn fn, void *args)
{
    if (spdk_thread_send_msg(thread, fn, args) < 0) {
        printf("Thread send message failed: thread_name %s!\n",
               spdk_thread_get_name(thread));
        exit(-1);
    }
}

int handleHttpRequest(void *args)
{
    bool busy = false;
    ZstoreController *zctrlr = (ZstoreController *)args;
    // log_info("http fn: once");

    return busy ? SPDK_POLLER_BUSY : SPDK_POLLER_IDLE;
}

int httpWorker(void *args)
{
    ZstoreController *zctrlr = (ZstoreController *)args;
    struct spdk_thread *thread = zctrlr->GetHttpThread();
    spdk_set_thread(thread);
    spdk_poller *p;
    // called once x microseconds
    p = spdk_poller_register(handleHttpRequest, zctrlr, 0);
    zctrlr->SetHttpPoller(p);
    while (true) {
        spdk_thread_poll(thread, 0, 0);
    }
}

int handleIoCompletions(void *args)
{
    int rc;
    ZstoreController *zctrlr = (ZstoreController *)args;
    // log_debug("XXX");
    enum spdk_thread_poller_rc poller_rc = SPDK_POLLER_IDLE;

    rc = spdk_nvme_qpair_process_completions(zctrlr->GetIoQpair(), 0);
    if (rc < 0) {
        // NXIO is expected when the connection is down.
        if (rc != -ENXIO) {
            SPDK_ERRLOG("NVMf request failed for conn %d\n", rc);
        }
    } else if (rc > 0) {
        poller_rc = SPDK_POLLER_BUSY;
    }
    return poller_rc;
}

int ioWorker(void *args)
{
    ZstoreController *zctrlr = (ZstoreController *)args;
    struct spdk_thread *thread = zctrlr->mIoThread.thread;
    spdk_set_thread(thread);
    spdk_poller *p;
    p = spdk_poller_register(handleIoCompletions, zctrlr, 0);
    zctrlr->SetCompletionPoller(p);
    while (true) {
        spdk_thread_poll(thread, 0, 0);
    }
}

void handleContext(RequestContext *context)
{
    // ContextType type = context->type;
    // if (type == USER) {
    //     handleUserContext(context);
    // } else if (type == STRIPE_UNIT) {
    //     handleStripeUnitContext(context);
    // } else if (type == GC) {
    //     handleGcContext(context);
    // } else if (type == INDEX) {
    //     handleIndexContext(context);
    // }
}

void handleEventCompletion2(void *arg1, void *arg2)
{
    RequestContext *slot = (RequestContext *)arg1;
    struct timeval s, e;
    gettimeofday(&s, 0);
    handleContext(slot);
    gettimeofday(&e, 0);
    // slot->ctrl->MarkDispatchThreadHandleContextTime(s, e);
}

void handleEventCompletion(void *args)
{
    handleEventCompletion2(args, nullptr);
}

int handleEventsDispatch(void *args)
{
    bool busy = false;
    ZstoreController *ctrl = (ZstoreController *)args;

    std::queue<RequestContext *> &writeQ = ctrl->GetWriteQueue();
    while (!writeQ.empty()) {
        RequestContext *ctx = writeQ.front();
        ctrl->WriteInDispatchThread(ctx);

        // if (ctx->curOffset == ctx->size / Configuration::GetBlockSize()) {
        busy = true;
        writeQ.pop();
        // } else {
        //     break;
        // }
    }

    std::queue<RequestContext *> &readQ = ctrl->GetReadQueue();
    while (!readQ.empty()) {
        RequestContext *ctx = readQ.front();
        ctrl->ReadInDispatchThread(ctx);
        readQ.pop();
        busy = true;
    }

    return busy ? SPDK_POLLER_BUSY : SPDK_POLLER_IDLE;
}

void zoneRead(void *arg1)
{
    RequestContext *ctx = reinterpret_cast<RequestContext *>(arg1);
    auto ioCtx = ctx->ioContext;
    int rc = 0;
    // auto ctrl = ctx->ctrl;
    // if (ctrl->verbose)
    //     log_debug("ding ding: we are running spdk read: offset {}, size {}, "
    //               "flags {}\n",
    //               ioCtx.offset, ioCtx.size, ioCtx.flags);

    rc = spdk_nvme_ns_cmd_read(ioCtx.ns, ioCtx.qpair, ioCtx.data, ioCtx.offset,
                               ioCtx.size, ioCtx.cb, ioCtx.ctx, ioCtx.flags);
    // assert(rc == 0);
}

static void issueIo(void *args)
{
    ZstoreController *zctrlr = (ZstoreController *)args;
    int rc;
    if (zctrlr->verbose)
        log_debug("Issue IO");

    auto worker = zctrlr->GetWorker();
    struct ns_entry *entry = worker->ns_ctx->entry;

    RequestContext *slot =
        zctrlr->mRequestContextPool->GetRequestContext(false);
    auto ioCtx = slot->ioContext;

    ioCtx.ns = entry->nvme.ns;
    ioCtx.qpair = worker->ns_ctx->qpair;
    ioCtx.data = slot->dataBuffer;
    // ioCtx.offset = offset_in_ios * entry->io_size_blocks;
    ioCtx.offset = 0;
    ioCtx.size = entry->io_size_blocks;
    ioCtx.cb = complete;
    ioCtx.ctx = slot;
    ioCtx.flags = 0;

    slot->ctrl = zctrlr;
    assert(slot->ctrl == zctrlr);

    // if (g_arbitration.is_random) {
    //     offset_in_ios = rand_r(&seed) % entry->size_in_ios;
    // } else {
    //     offset_in_ios = worker->ns_ctx->offset_in_ios++;
    //     if (worker->ns_ctx->offset_in_ios == entry->size_in_ios) {
    //         worker->ns_ctx->offset_in_ios = 0;
    //     }
    // }

    // log_debug("Before READ {}", zctrlr->GetTaskPoolSize());
    // log_debug("Before READ {}", offset_in_ios * entry->io_size_blocks);

    thread_send_msg(zctrlr->GetIoThread(), zoneRead, slot);

    if (rc != 0) {
        log_error("starting I/O failed");
    } else {
        // log_error("starting I/O failed");
        // worker->ns_ctx->current_queue_depth++;
    }
}

int handleSubmit(void *args)
{
    bool busy = false;
    ZstoreController *ctrl = (ZstoreController *)args;

    std::queue<RequestContext *> &readQ = ctrl->GetReadQueue();

    int queue_depth = ctrl->GetQueueDepth();
    auto req_inflight = ctrl->mRequestContextPool->capacity -
                        ctrl->mRequestContextPool->availableContexts.size();
    auto worker = ctrl->GetWorker();
    // worker->ns_ctx->current_queue_depth

    while (!readQ.empty()) {

        // if (ctrl->verbose)
        //     log_debug(
        //         "queue depth {}, req in flight {}, completed {}, current
        //         queue " "depth {}", queue_depth, req_inflight,
        //         worker->ns_ctx->io_completed, readQ.size());

        RequestContext *ctx = readQ.front();
        ctrl->ReadInDispatchThread(ctx);

        // if (ctx->curOffset == ctx->size / Configuration::GetBlockSize()) {
        busy = true;
        readQ.pop();
        // } else {
        //     break;
        // }
    }

    if (worker->ns_ctx->io_completed > Configuration::GetTotalIo()) {
        auto etime = std::chrono::high_resolution_clock::now();
        auto delta = std::chrono::duration_cast<std::chrono::microseconds>(
                         etime - gZstoreController->stime)
                         .count();
        auto tput = worker->ns_ctx->io_completed * g_micro_to_second / delta;

        // if (g->verbose)
        log_info("Total IO {}, total time {}ms, throughput {} IOPS",
                 worker->ns_ctx->io_completed, delta, tput);

        // log_debug("drain io: {}", spdk_get_ticks());
        drain_io(gZstoreController);
        log_debug("clean up ns worker");
        gZstoreController->cleanup_ns_worker_ctx();
        print_stats(gZstoreController);
        exit(0);
    }

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

// void inspect(void *args)
// {
//     ZstoreController *zctrlr = (ZstoreController *)args;
//     auto worker = zctrlr->GetWorker();
//     log_debug("IO completed {}, task pool {}, task count {}, ns ctx qd {}",
//               worker->ns_ctx->io_completed, zctrlr->GetTaskPoolSize(),
//               zctrlr->GetTaskCount(), worker->ns_ctx->current_queue_depth);
//     assert(zctrlr->GetTaskCount() ==
//            zctrlr->GetTaskPoolSize() + worker->ns_ctx->current_queue_depth
//
//     );
// }

int handleObjectSubmit(void *args)
{
    bool busy = false;
    ZstoreController *zctrlr = (ZstoreController *)args;
    // zctrlr->CheckTaskPool("submit IO");
    int queue_depth = zctrlr->GetQueueDepth();
    // int queue_depth = zctrlr->mWorker->ns_ctx->current_queue_depth;

    auto worker = zctrlr->GetWorker();
    // auto task_count = zctrlr->GetTaskCount();

    // Multiple threads/readers can read the counter's value at the same time.
    // std::shared_lock lock(zctrlr->context_pool_mutex_);
    auto req_inflight = zctrlr->mRequestContextPool->capacity -
                        zctrlr->mRequestContextPool->availableContexts.size();
    while (req_inflight < queue_depth && !worker->ns_ctx->is_draining) {

        // log_debug("queue depth {}, task count {} - task pool size {} ",
        //           queue_depth, task_count, zctrlr->GetTaskPoolSize());

        std::string current_key = "key" + std::to_string(zctrlr->pivot);
        // MapIter got = zctrlr->mMap.find(curent_key);
        // if (got == zctrlr->mMap.end())
        //     log_debug("key {} is not in the map", current_key);
        // else
        //     // log_debug("key {} is not in the map", current_key);
        //     // std::cout << got->first << " is " << got->second;
        //     MapEntry entry = got->second;
        auto res = zctrlr->find_object(current_key);
        // log_debug("Found {}, value {}", current_key, res.value());
        // int offset = res.value().second;

        int offset = 0;
        // struct ZstoreObject *obj = ReadObject(zctrlr->pivot, zctrlr);

        // struct ZstoreObject obj = ReadObject(offset, zctrlr).value();
        // inspect(zctrlr);
        // struct ZstoreObject *obj = ReadObject(0, zctrlr);
        issueIo(zctrlr);
        // log_debug("Receive object at LBA {}: key {}, seqnum {}, vernum {}",
        //           offset, obj.key, obj.seqnum, obj.vernum);
        zctrlr->pivot++;
        busy = true;
    }
    if (worker->ns_ctx->io_completed > Configuration::GetTotalIo()) {
        auto etime = std::chrono::high_resolution_clock::now();
        auto delta = std::chrono::duration_cast<std::chrono::microseconds>(
                         etime - zctrlr->stime)
                         .count();
        auto tput = worker->ns_ctx->io_completed * g_micro_to_second / delta;

        if (zctrlr->verbose)
            log_info("Total IO {}, total time {}ms, throughput {} IOPS",
                     worker->ns_ctx->io_completed, delta, tput);

        log_debug("drain io: {}", spdk_get_ticks());
        drain_io(zctrlr);
        log_debug("clean up ns worker");
        zctrlr->cleanup_ns_worker_ctx();
        //
        //     std::vector<uint64_t> deltas1;
        //     for (int i = 0; i < zctrlr->mWorker->ns_ctx->stimes.size(); i++)
        //     {
        //         deltas1.push_back(
        //             std::chrono::duration_cast<std::chrono::microseconds>(
        //                 zctrlr->mWorker->ns_ctx->etimes[i] -
        //                 zctrlr->mWorker->ns_ctx->stimes[i])
        //                 .count());
        //     }
        //     auto sum1 = std::accumulate(deltas1.begin(), deltas1.end(), 0.0);
        //     auto mean1 = sum1 / deltas1.size();
        //     auto sq_sum1 = std::inner_product(deltas1.begin(), deltas1.end(),
        //                                       deltas1.begin(), 0.0);
        //     auto stdev1 = std::sqrt(sq_sum1 / deltas1.size() - mean1 *
        //     mean1); log_info("qd: {}, mean {}, std {}",
        //              zctrlr->mWorker->ns_ctx->io_completed, mean1, stdev1);
        //
        //     // clearnup
        //     deltas1.clear();
        //     zctrlr->mWorker->ns_ctx->etimes.clear();
        //     zctrlr->mWorker->ns_ctx->stimes.clear();
        //     // }
        //
        log_debug("end work fn");
        print_stats(zctrlr);
        exit(0);
    }
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

Result<MapEntry> createMapEntry(std::string device, int32_t lba)
{
    MapEntry entry;
    entry.first = device;
    entry.second = lba;
    return entry;
}

void updateMapEntry(MapEntry *entry, std::string device, int32_t lba)
{
    entry->first = device;
    entry->second = lba;
}

void RequestContext::Clear()
{
    available = true;
    successBytes = 0;
    targetBytes = 0;
    lba = 0;
    cb_fn = nullptr;
    cb_args = nullptr;
    curOffset = 0;
    ioOffset = 0;
    // status = WRITE_COMPLETE; // added for clean operation

    timestamp = ~0ull;

    append = false;

    // associatedRequests.clear(); // = nullptr;
    // associatedStripe = nullptr;
    // associatedRead = nullptr;
    ctrl = nullptr;
    // segment = nullptr;

    successBytes = 0;
    targetBytes = 0;

    // needDegradedRead = false;
    pbaArray.clear();
}

double RequestContext::GetElapsedTime() { return ctime - stime; }

PhysicalAddr RequestContext::GetPba()
{
    PhysicalAddr addr;
    // addr.segment = segment;
    addr.zoneId = zoneId;
    addr.offset = offset;
    return addr;
}

void RequestContext::Queue()
{
    struct spdk_thread *th = ctrl->GetDispatchThread();
    thread_send_msg(ctrl->GetDispatchThread(), handleEventCompletion, this);
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
    meta = o.meta;
    pbaArray = o.pbaArray;
    successBytes = o.successBytes;
    targetBytes = o.targetBytes;
    curOffset = o.curOffset;
    // ioOffset = o.ioOffset;
    cb_fn = o.cb_fn;
    cb_args = o.cb_args;

    available = o.available;

    ctrl = o.ctrl;
    // segment = o.segment;
    zoneId = o.zoneId;
    // stripeId = o.stripeId;
    offset = o.offset;
    append = o.append;

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
        // contexts[i].lbaArray.resize(Configuration::GetMaxStripeUnitSize() /
        //                             Configuration::GetBlockSize());
        contexts[i].dataBuffer = (uint8_t *)spdk_zmalloc(
            4096, 4096, NULL, SPDK_ENV_SOCKET_ID_ANY, SPDK_MALLOC_DMA);
        // contexts[i].metadataBuffer =
        //     (uint8_t *)spdk_zmalloc(Configuration::GetMaxStripeUnitSize() /
        //                                 Configuration::GetBlockSize() *
        //                                 Configuration::GetMetadataSize(),
        //                             Configuration::GetBlockSize(), NULL,
        //                             SPDK_ENV_SOCKET_ID_ANY, SPDK_MALLOC_DMA);
        contexts[i].bufferSize = 4096;
        availableContexts.emplace_back(&contexts[i]);
    }
}

RequestContext *RequestContextPool::GetRequestContext(bool force)
{
    RequestContext *ctx = nullptr;
    if (availableContexts.empty() && force == false) {
        ctx = nullptr;
    } else {
        if (!availableContexts.empty()) {
            // log_debug(
            //     "available Context is not empty pop one from the back.");
            ctx = availableContexts.back();
            availableContexts.pop_back();
            ctx->Clear();
            ctx->available = false;
        } else {
            // log_debug("available Context is empty, BAD.");
            ctx = new RequestContext();
            ctx->dataBuffer = (uint8_t *)spdk_zmalloc(
                4096, 4096, NULL, SPDK_ENV_SOCKET_ID_ANY, SPDK_MALLOC_DMA);
            // ctx->metadataBuffer = (uint8_t *)spdk_zmalloc(
            //     Configuration::GetMaxStripeUnitSize() /
            //         Configuration::GetBlockSize() *
            //         Configuration::GetMetadataSize(),
            //     Configuration::GetBlockSize(), NULL, SPDK_ENV_SOCKET_ID_ANY,
            //     SPDK_MALLOC_DMA);
            ctx->bufferSize = 4096;
            ctx->Clear();
            ctx->available = false;
            // log_debug("Request Context runs out.");
            // exit(1);
        }
    }
    return ctx;
}

void RequestContextPool::ReturnRequestContext(RequestContext *slot)
{
    // std::unique_lock lock(gZstoreController->context_pool_mutex_);
    // assert(slot->available);
    // ZstoreController *ctrl = (ZstoreController *)slot->ctrl;
    // if (ctrl->verbose)
    //     log_error("slot < contexts {}, slot >= contexts+cap {}",
    //               slot < contexts, slot >= contexts + capacity);
    assert(availableContexts.size() <= capacity);
    availableContexts.emplace_back(slot);

    // if (slot < contexts || slot >= contexts + capacity) {
    //     // if (ctrl->verbose)
    //     log_debug("freeing buffers, not sure why");
    //     // test whether the returned slot is pre-allocated
    //     spdk_free(slot->dataBuffer);
    //     spdk_free(slot->metadataBuffer);
    //     delete slot;
    // } else {
    //     // if (ctrl->verbose)
    //     log_debug("Puting slot back to avaiable context");
    //     assert(availableContexts.size() <= capacity);
    //     availableContexts.emplace_back(slot);
    // }
}
