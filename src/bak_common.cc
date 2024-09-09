#include "include/common.h"
#include "spdk/thread.h"
#include <isa-l.h>
#include <queue>
#include <sched.h>
#include <spdk/event.h>
#include <sys/time.h>
#include <vector>

void handleContext(RequestContext *context);

static void dummy_disconnect_handler(struct spdk_nvme_qpair *qpair,
                                     void *poll_group_ctx)
{
}

int handleIoCompletions(void *args)
{
    static double mTime = 0;
    static int timeCnt = 0;
    {
        double timeNow = GetTimestampInUs();
        if (mTime == 0) {
            printf("[DEBUG] %s %d\n", __func__, timeCnt);
            mTime = timeNow;
        } else {
            if (timeNow - mTime > 10.0) {
                mTime += 10.0;
                timeCnt += 10;
                printf("[DEBUG] %s %d\n", __func__, timeCnt);
            }
        }
    }

    struct spdk_nvme_poll_group *pollGroup =
        (struct spdk_nvme_poll_group *)args;
    int r = 0;
    r = spdk_nvme_poll_group_process_completions(pollGroup, 0,
                                                 dummy_disconnect_handler);
    return r > 0 ? SPDK_POLLER_BUSY : SPDK_POLLER_IDLE;
}

// bool checkPendingWrites(ZstoreController *ctrl)
// {
//     static timeval s, e;
//     static int starve_time = 5;
//     bool busy = false;
//     gettimeofday(&s, 0);
//     std::map<Zone *, std::map<uint32_t, RequestContext *>> &zoneWriteQ =
//         ctrl->GetPendingZoneWriteQueue();
//     for (auto &it : zoneWriteQ) {
//         Zone *zone = it.first;
//         uint64_t pos = zone->GetPos();
//         if (it.second.count(pos) == 0) {
//             continue;
//         }
//         realZoneWrite(it.second[pos]);
//         it.second.erase(pos);
//         busy = true;
//
//         if (it.second.empty()) {
//             zoneWriteQ.erase(zone);
//         }
//         break;
//     }
//
//     gettimeofday(&e, 0);
//     ctrl->MarkIoThreadCheckPendingWritesTime(s, e);
//     return busy;
// }

// int handleIoPendingZoneWrites(void *args)
// {
//     ZstoreController *ctrl = (ZstoreController *)args;
//     int r = 0;
//     bool busy = checkPendingWrites(ctrl);
//
//     return busy ? SPDK_POLLER_BUSY : SPDK_POLLER_IDLE;
// }

int ioWorker(void *args)
{
    IoThread *ioThread = (IoThread *)args;
    struct spdk_thread *thread = ioThread->thread;
    ZstoreController *ctrl = ioThread->controller;
    spdk_set_thread(thread);
    spdk_poller_register(handleIoCompletions, ioThread->group, 0);
    // spdk_poller_register(handleIoPendingZoneWrites, ctrl, 0);
    while (true) {
        spdk_thread_poll(thread, 0, 0);
    }
}

// static bool contextReady(RequestContext *ctx)
// {
//     if (ctx->successBytes > ctx->targetBytes) {
//         log_error("ctx->successBytes %u ctx->targetBytes %u\n",
//                   ctx->successBytes, ctx->targetBytes);
//         printf("line %d ctx->successBytes %u ctx->targetBytes %u\n",
//         __LINE__,
//                ctx->successBytes, ctx->targetBytes);
//         assert(0);
//     }
//     return ctx->successBytes == ctx->targetBytes;
// }

// the handling of write request finishes at updating PBA;
// the handling of read request finishes here (reaping)
static void handleUserContext(RequestContext *usrCtx)
{
    // ContextStatus &status = usrCtx->status;
    ZstoreController *ctrl = usrCtx->ctrl;
    // assert(contextReady(usrCtx));
    // if (status == WRITE_REAPING) {
    //     status = WRITE_INDEX_UPDATING;
    //     // reuse for index updating, avoid repeated PBA update
    //     // repeated PBA updates may cause repeated block invalidation.
    //     UpdatePbaArgs *args = (UpdatePbaArgs *)calloc(1,
    //     sizeof(UpdatePbaArgs)); args->ctrl = ctrl; args->ctx = usrCtx;
    //     thread_send_msg(ctrl->GetIndexThread(), updatePba, args);
    //
    // } else if (status == READ_REAPING) {
    //     ctrl->RemoveRequestFromGcEpochIfNecessary(usrCtx);
    //     status = READ_COMPLETE;
    //     ctrl->CompleteRead();
    //
    //     if (usrCtx->cb_fn != nullptr) {
    //         usrCtx->cb_fn(usrCtx->cb_args);
    //     }
    //     usrCtx->available = true;
    // }
}

void handleContext(RequestContext *context)
{
    // ContextType type = context->type;
    // if (type == USER) {
    handleUserContext(context);
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

// int handleEventsDispatch(void *args)
// {
//     // debug
//     static double mTime = 0;
//     static int timeCnt = 0;
//     bool printFlag = false;
//     double timeNow = GetTimestampInUs();
//     {
//         if (mTime == 0) {
//             printf("[DEBUG] %s %d\n", __func__, timeCnt);
//             mTime = timeNow;
//             printFlag = true;
//         } else {
//             if (timeNow - mTime > 10.0) {
//                 mTime += 10.0;
//                 timeCnt += 10;
//                 printf("[DEBUG] %s %d\n", __func__, timeCnt);
//                 printFlag = true;
//             }
//         }
//     }
//     static timeval s, e;
//
//     bool busy = false;
//     ZstoreController *ctrl = (ZstoreController *)args;
//     gettimeofday(&s, 0);
//
//     std::queue<RequestContext *> &writeQ = ctrl->GetWriteQueue();
//     if (Configuration::StuckDebugMode()) {
//         Configuration::IncreaseStuckCounter();
//         //    if (Configuration::GetStuckCounter() > 2)
//         if (Configuration::StuckDebugModeFinished(timeNow)) {
//             debug_e("Stucked. Exit\n");
//             assert(0);
//         }
//     }
//     while (!writeQ.empty()) {
//         RequestContext *ctx = writeQ.front();
//         ctrl->WriteInDispatchThread(ctx);
//
//         if (ctx->curOffset == ctx->size / Configuration::GetBlockSize()) {
//             busy = true;
//             writeQ.pop();
//         } else {
//             break;
//         }
//     }
//
//     std::queue<RequestContext *> &readPrepareQ = ctrl->GetReadPrepareQueue();
//     while (!readPrepareQ.empty()) {
//         RequestContext *ctx = readPrepareQ.front();
//         ctrl->ReadInDispatchThread(ctx);
//         readPrepareQ.pop();
//         busy = true;
//     }
//
//     std::queue<RequestContext *> &readReapingQ = ctrl->GetReadReapingQueue();
//     while (!readReapingQ.empty()) {
//         RequestContext *ctx = readReapingQ.front();
//         ctrl->ReadInDispatchThread(ctx);
//
//         if (ctx->curOffset == ctx->size / Configuration::GetBlockSize()) {
//             busy = true;
//             readReapingQ.pop();
//         } else {
//             break;
//         }
//     }
//
//     // new implementation: write index
//     std::queue<RequestContext *> &writeIndexQ = ctrl->GetWriteIndexQueue();
//     while (!writeIndexQ.empty()) {
//         RequestContext *ctx = writeIndexQ.front();
//         //    ctrl->WriteIndexInDispatchThread(ctx);
//         ctrl->WriteInDispatchThread(ctx);
//
//         if (ctx->curOffset == ctx->size / Configuration::GetBlockSize()) {
//             busy = true;
//             writeIndexQ.pop();
//         } else {
//             break;
//         }
//     }
//
//     // new implementation: read index
//     std::queue<RequestContext *> &readIndexQ = ctrl->GetReadIndexQueue();
//     while (!readIndexQ.empty()) {
//         RequestContext *ctx = readIndexQ.front();
//         ctrl->ReadInDispatchThread(ctx);
//
//         if (ctx->curOffset == ctx->size / Configuration::GetBlockSize()) {
//             busy = true;
//             readIndexQ.pop();
//         } else {
//             break;
//         }
//     }
//
//     gettimeofday(&e, 0);
//     ctrl->MarkDispatchThreadEventTime(s, e);
//
//     return busy ? SPDK_POLLER_BUSY : SPDK_POLLER_IDLE;
// }

// int handleBackgroundTasks(void *args)
// {
//     // debug
//     static double mTime = 0;
//     static int timeCnt = 0;
//     static bool stuckFlag = 0;
//     static uint64_t lastIOBytes = 0;
//     static uint64_t savedTimeCnt = 0;
//     struct timeval s, e;
//     gettimeofday(&s, 0);
//
//     ZstoreController *raidController = (ZstoreController *)args;
//     {
//         double timeNow = GetTimestampInUs();
//         uint64_t sw, sr, cw, cr;
//         uint64_t isw, isr, icw, icr;
//         if (mTime == 0) {
//             raidController->RetrieveWRCounts(sw, sr, cw, cr);
//             printf("[DEBUG] %s %d (%lu %lu %lu %lu)\n", __func__, timeCnt,
//             sw,
//                    sr, cw, cr);
//             mTime = timeNow;
//         } else {
//             if (timeNow - mTime > 10.0) {
//                 raidController->RetrieveWRCounts(sw, sr, cw, cr);
//                 raidController->RetrieveIndexWRCounts(isw, isr, icw, icr);
//                 mTime += 10.0;
//                 timeCnt += 10;
//                 uint64_t IOBytes, writeBytes;
//                 IOBytes = StatsRecorder::getInstance()->getTotalIOBytes();
//                 writeBytes =
//                 StatsRecorder::getInstance()->getTotalWriteBytes(); printf(
//                     "[DEBUG] %s %d (%lu %lu %lu %lu) index (%lu %lu %lu %lu)
//                     " "read/write %.3lf/%.3lf GiB\n",
//                     __func__, timeCnt, sw, sr, cw, cr, isw, isr, icw, icr,
//                     (double)(IOBytes - writeBytes) / 1024 / 1024 / 1024,
//                     (double)(writeBytes) / 1024 / 1024 / 1024);
//                 raidController->printCompletionThreadTime();
//                 raidController->printIoThreadTime(timeCnt * 1000000.0);
//                 raidController->printDispatchThreadTime(timeCnt * 1000000.0);
//                 raidController->printIndexThreadTime(timeCnt * 1000000.0);
//                 if (timeCnt > 20 && timeCnt > savedTimeCnt + 5 &&
//                     lastIOBytes == IOBytes && cw + cr < sw + sr) {
//                     // I/O not processing, but some requests needs to be
//                     // processed
//                     printf("[DEBUG] totally stuck\n");
//                     Configuration::SetStuckDebugMode(timeNow);
//                     assert(0);
//                 } else {
//                 }
//
//                 if (timeCnt > savedTimeCnt + 5) {
//                     lastIOBytes =
//                         StatsRecorder::getInstance()->getTotalIOBytes();
//                     savedTimeCnt = timeCnt;
//                 }
//                 if (timeCnt % 30 == 0) {
//                     StatsRecorder::getInstance()->Dump();
//                 }
//                 //        raidController->PrintSegmentPos();
//             }
//         }
//     }
//
//     bool hasProgress = false;
//     hasProgress |= raidController->ProceedGc();
//     hasProgress |= raidController->CheckSegments(); // including meta zones
//     gettimeofday(&e, 0);
//     raidController->MarkDispatchThreadBackgroundTime(s, e);
//
//     return hasProgress ? SPDK_POLLER_BUSY : SPDK_POLLER_IDLE;
// }

int dispatchWorker(void *args)
{
    ZstoreController *raidController = (ZstoreController *)args;
    struct spdk_thread *thread = raidController->GetDispatchThread();
    spdk_set_thread(thread);
    spdk_poller *p1, *p2;
    p1 = spdk_poller_register(handleEventsDispatch, raidController, 1);
    p2 = spdk_poller_register(handleBackgroundTasks, raidController, 1);
    raidController->SetEventPoller(p1);
    raidController->SetBackgroundPoller(p2);
    while (true) {
        spdk_thread_poll(thread, 0, 0);
    }
}

int ecWorker(void *args)
{
    ZstoreController *raidController = (ZstoreController *)args;
    struct spdk_thread *thread = raidController->GetEcThread();
    spdk_set_thread(thread);
    while (true) {
        spdk_thread_poll(thread, 0, 0);
    }
}

// int indexWorker(void *args)
// {
//     ZstoreController *raidController = (ZstoreController *)args;
//     struct spdk_thread *thread = raidController->GetIndexThread();
//     spdk_set_thread(thread);
//     spdk_poller *p1, *p2;
//     p1 = spdk_poller_register(handleIndexTasks, raidController, 1);
//     while (true) {
//         spdk_thread_poll(thread, 0, 0);
//     }
// }

int completionWorker(void *args)
{
    ZstoreController *raidController = (ZstoreController *)args;
    struct spdk_thread *thread = raidController->GetCompletionThread();
    spdk_set_thread(thread);
    while (true) {
        spdk_thread_poll(thread, 0, 0);
    }
}

void executeRequest(void *arg1, void *arg2)
{
    Request *req = reinterpret_cast<Request *>(arg1);
    req->controller->Execute(req->offset, req->size, req->data,
                             req->type == 'W', req->cb_fn, req->cb_args);
    free(req);
}

void registerIoCompletionRoutine(void *arg1, void *arg2)
{
    IoThread *ioThread = (IoThread *)arg1;
    spdk_poller_register(handleIoCompletions, ioThread->group, 0);
    spdk_poller_register(handleIoPendingZoneWrites, ioThread->controller, 0);
}

void registerDispatchRoutine(void *arg1, void *arg2)
{
    ZstoreController *raidController =
        reinterpret_cast<ZstoreController *>(arg1);
    spdk_poller *p1, *p2;
    p1 = spdk_poller_register(handleEventsDispatch, raidController, 1);
    p2 = spdk_poller_register(handleBackgroundTasks, raidController, 1);
    raidController->SetEventPoller(p1);
    raidController->SetBackgroundPoller(p2);
}

void enqueueRequest2(void *arg1, void *arg2)
{
    struct timeval s, e;
    gettimeofday(&s, 0);
    RequestContext *ctx = reinterpret_cast<RequestContext *>(arg1);
    if (ctx->req_type == 'W') {
        ctx->ctrl->EnqueueWrite(ctx);
    } else {
        if (ctx->status == READ_PREPARE) {
            ctx->ctrl->EnqueueReadPrepare(ctx);
        } else if (ctx->status == READ_REAPING) {
            ctx->ctrl->EnqueueReadReaping(ctx);
        }
    }
    gettimeofday(&e, 0);
    ctx->ctrl->MarkDispatchThreadEnqueueTime(s, e);
}

void enqueueRequest(void *args) { enqueueRequest2(args, nullptr); }

void zoneWrite2(void *arg1, void *arg2)
{
    static timeval s, e;
    RequestContext *slot = reinterpret_cast<RequestContext *>(arg1);
    auto ioCtx = slot->ioContext;

    gettimeofday(&s, 0);
    slot->stime = timestamp();

    if (Configuration::GetEnableIOLatencyTest()) {
        slot->timeA = s;
    }

    // only one io thread can do this
    if (slot->zone->GetPos() != slot->offset) {
        slot->ctrl->EnqueueZoneWrite(slot);
    } else {
        realZoneWrite(slot);
    }

    gettimeofday(&e, 0);
    if (slot->ctrl == nullptr) {
        debug_warn("slot nullptr %p\n", slot);
        assert(0);
    }
    slot->ctrl->MarkIoThreadZoneWriteTime(s, e);
}

void bufferCopy2(void *arg1, void *arg2) { bufferCopy(arg1); }

void bufferCopy(void *args)
{
    static timeval s, e;
    static bool first = true;

    BufferCopyArgs *bcArgs = reinterpret_cast<BufferCopyArgs *>(args);
    RequestContext *slot = bcArgs->slot;
    Device *device = bcArgs->dev;
    ZstoreController *ctrl = bcArgs->slot->ctrl;
    gettimeofday(&s, 0);
    if (!first) {
        ctrl->MarkCompletionThreadIdleTime(e, s);
    }
    first = false;

    auto ioCtx = slot->ioContext;

    {
        uint32_t blockSize = Configuration::GetBlockSize();
        uint32_t ptr = 0;
        for (auto &it : slot->associatedRequests) {
            auto &parent = it.first;
            uint32_t uCtxOffset = it.second.first;
            uint32_t uCtxSize = it.second.second;

            if (parent) {
                memcpy((uint8_t *)ioCtx.data + ptr,
                       parent->data + uCtxOffset * blockSize, uCtxSize);
                ptr += uCtxSize;
            } else {
                memset((uint8_t *)ioCtx.data + ptr, 0, uCtxSize);
            }
        }
    }

    delete bcArgs;

    if (slot->append) {
        device->issueIo(zoneAppend, slot);
    } else {
        device->issueIo(zoneWrite, slot);
    }
    gettimeofday(&e, 0);
    ctrl->MarkCompletionThreadBufferCopyTime(s, e);
}

void zoneWrite(void *args) { zoneWrite2(args, nullptr); }

void zoneRead2(void *arg1, void *arg2)
{
    RequestContext *slot = reinterpret_cast<RequestContext *>(arg1);
    auto ioCtx = slot->ioContext;
    slot->stime = timestamp();

    int rc = 0;
    if (Configuration::GetEnableIOLatencyTest()) {
        gettimeofday(&slot->timeA, 0);
    }

    if (Configuration::GetDeviceSupportMetadata()) {
        rc = spdk_nvme_ns_cmd_read_with_md(
            ioCtx.ns, ioCtx.qpair, ioCtx.data, ioCtx.metadata, ioCtx.offset,
            ioCtx.size, ioCtx.cb, ioCtx.ctx, ioCtx.flags, 0, 0);
    } else {
        rc = spdk_nvme_ns_cmd_read(ioCtx.ns, ioCtx.qpair, ioCtx.data,
                                   ioCtx.offset, ioCtx.size, ioCtx.cb,
                                   ioCtx.ctx, ioCtx.flags);
    }
    if (rc != 0) {
        fprintf(stderr, "Device read error!\n");
    }
    assert(rc == 0);
}

void realZoneWrite(RequestContext *slot)
{
    auto ioCtx = slot->ioContext;
    int rc = 0;
    if (Configuration::GetEnableIOLatencyTest()) {
        gettimeofday(&slot->timeA, 0);
    }

    if (Configuration::GetDeviceSupportMetadata()) {
        rc = spdk_nvme_ns_cmd_write_with_md(
            ioCtx.ns, ioCtx.qpair, ioCtx.data, ioCtx.metadata, ioCtx.offset,
            ioCtx.size, ioCtx.cb, ioCtx.ctx, ioCtx.flags, 0, 0);
    } else {
        rc = spdk_nvme_ns_cmd_write(ioCtx.ns, ioCtx.qpair, ioCtx.data,
                                    ioCtx.offset, ioCtx.size, ioCtx.cb,
                                    ioCtx.ctx, ioCtx.flags);
    }
    if (rc != 0) {
        fprintf(stderr, "Device write error!\n");
    }
    assert(rc == 0);
}

void zoneRead(void *args) { zoneRead2(args, nullptr); }

void zoneAppend2(void *arg1, void *arg2)
{
    RequestContext *slot = reinterpret_cast<RequestContext *>(arg1);
    auto ioCtx = slot->ioContext;
    slot->stime = timestamp();

    //  fprintf(stderr, "Append %lx %lu\n", ioCtx.offset, ioCtx.size);

    int rc = 0;
    if (Configuration::GetEnableIOLatencyTest()) {
        gettimeofday(&slot->timeA, 0);
    }

    //  {
    //    uint32_t blockSize = Configuration::GetBlockSize();
    //    uint32_t ptr = 0;
    //    for (auto& it : slot->associatedRequests) {
    //      auto& parent = it.first;
    //      uint32_t uCtxOffset = it.second.first;
    //      uint32_t uCtxSize = it.second.second;
    //
    //      if (parent) {
    //        memcpy((uint8_t*)ioCtx.data + ptr,
    //            parent->data + uCtxOffset * blockSize, uCtxSize);
    //        ptr += uCtxSize;
    //      } else {
    //        memset((uint8_t*)ioCtx.data + ptr, 0, uCtxSize);
    //      }
    //    }
    //  }

    if (Configuration::GetDeviceSupportMetadata()) {
        rc = spdk_nvme_zns_zone_append_with_md(
            ioCtx.ns, ioCtx.qpair, ioCtx.data, ioCtx.metadata, ioCtx.offset,
            ioCtx.size, ioCtx.cb, ioCtx.ctx, ioCtx.flags, 0, 0);
    } else {
        rc = spdk_nvme_zns_zone_append(ioCtx.ns, ioCtx.qpair, ioCtx.data,
                                       ioCtx.offset, ioCtx.size, ioCtx.cb,
                                       ioCtx.ctx, ioCtx.flags);
    }
    if (rc != 0) {
        fprintf(stderr, "Device append error!\n");
    }
    assert(rc == 0);
}

void zoneAppend(void *args) { zoneAppend2(args, nullptr); }

void zoneReset2(void *arg1, void *arg2)
{
    RequestContext *slot = reinterpret_cast<RequestContext *>(arg1);
    auto ioCtx = slot->ioContext;
    slot->stime = timestamp();
    int rc = spdk_nvme_zns_reset_zone(ioCtx.ns, ioCtx.qpair, ioCtx.offset, 0,
                                      ioCtx.cb, ioCtx.ctx);
    if (rc != 0) {
        fprintf(stderr, "Device reset error!\n");
    }
    assert(rc == 0);
}

void zoneReset(void *args) { zoneReset2(args, nullptr); }

void zoneFinish2(void *arg1, void *arg2)
{
    RequestContext *slot = reinterpret_cast<RequestContext *>(arg1);
    auto ioCtx = slot->ioContext;
    slot->stime = timestamp();

    int rc = spdk_nvme_zns_finish_zone(ioCtx.ns, ioCtx.qpair, ioCtx.offset, 0,
                                       ioCtx.cb, ioCtx.ctx);
    if (rc != 0) {
        fprintf(stderr, "Device close error!\n");
    }
    assert(rc == 0);
}

void zoneFinish(void *args) { zoneFinish2(args, nullptr); }

void tryDrainController(void *args)
{
    DrainArgs *drainArgs = (DrainArgs *)args;
    drainArgs->ctrl->CheckSegments();
    drainArgs->ctrl->ReclaimContexts();
    drainArgs->ctrl->ProceedGc();
    drainArgs->success = drainArgs->ctrl->GetNumInflightRequests() == 0 &&
                         !drainArgs->ctrl->ExistsGc();

    drainArgs->ready = true;
}

void completeOneEvent(void *arg, const struct spdk_nvme_cpl *completion)
{
    uint32_t *counter = (uint32_t *)arg;
    (*counter)--;

    if (spdk_nvme_cpl_is_error(completion)) {
        fprintf(stderr, "I/O error status: %s\n",
                spdk_nvme_cpl_get_status_string(&completion->status));
        fprintf(stderr, "I/O failed, aborting run\n");
        assert(0);
        exit(1);
    }
}

void complete(void *arg, const struct spdk_nvme_cpl *completion)
{
    // bool *done = (bool *)arg;
    // *done = true;

    if (spdk_nvme_cpl_is_error(completion)) {
        fprintf(stderr, "I/O error status: %s\n",
                spdk_nvme_cpl_get_status_string(&completion->status));
        fprintf(stderr, "I/O failed, aborting run\n");
        assert(0);
        exit(1);
    }
}

void thread_send_msg(spdk_thread *thread, spdk_msg_fn fn, void *args)
{
    if (spdk_thread_send_msg(thread, fn, args) < 0) {
        printf("Thread send message failed: thread_name %s!\n",
               spdk_thread_get_name(thread));
        exit(-1);
    }
}
