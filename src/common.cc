#include "include/common.h"
#include "include/configuration.h"
#include "include/request_handler.h"
#include "include/utils.hpp"
#include "include/zns_utils.h"
#include "include/zstore.h"
#include "include/zstore_controller.h"
#include "spdk/thread.h"
#include <cstdlib>
#include <isa-l.h>
#include <queue>
#include <sched.h>
#include <spdk/event.h>
#include <sys/time.h>
#include <vector>

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

int handleSubmit(void *args)
{
    bool busy = false;
    ZstoreController *zctrlr = (ZstoreController *)args;
    // zctrlr->CheckTaskPool("submit IO");
    // int queue_depth = zctrlr->GetQueueDepth();
    int queue_depth = zctrlr->mWorker->ns_ctx->current_queue_depth;
    // log_debug("queue depth {}, task pool size {}", queue_depth,
    //           spdk_mempool_count(zctrlr->mTaskPool));
    while (queue_depth-- > 0) {
        submit_single_io(zctrlr);
        // busy = true;
    }

    // if (spdk_get_ticks() > zctrlr->tsc_end) {
    // if (zctrlr->mWorker->ns_ctx->io_completed > 1000'000) {
    if (zctrlr->mWorker->ns_ctx->io_completed > 1000) {
        log_debug("drain io: {}", spdk_get_ticks());
        drain_io(zctrlr);
        log_debug("clean up ns worker");
        cleanup_ns_worker_ctx(zctrlr);
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
        exit(-1);
    }

    return busy ? SPDK_POLLER_BUSY : SPDK_POLLER_IDLE;
}

int dispatchWorker(void *args)
{
    ZstoreController *zctrl = (ZstoreController *)args;
    struct spdk_thread *thread = zctrl->GetDispatchThread();
    spdk_set_thread(thread);
    spdk_poller *p;
    // p = spdk_poller_register(handleEventsDispatch, zctrl, 1);
    p = spdk_poller_register(handleSubmit, zctrl, 1);
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

// void zoneWrite2(void *arg1, void *arg2)
// {
//     static timeval s, e;
//     RequestContext *slot = reinterpret_cast<RequestContext *>(arg1);
//     auto ioCtx = slot->ioContext;
//
//     gettimeofday(&s, 0);
//     slot->stime = timestamp();
//
//     if (Configuration::GetEnableIOLatencyTest()) {
//         slot->timeA = s;
//     }
//
//     // only one io thread can do this
//     if (slot->zone->GetPos() != slot->offset) {
//         slot->ctrl->EnqueueZoneWrite(slot);
//     } else {
//         realZoneWrite(slot);
//     }
//
//     gettimeofday(&e, 0);
//     if (slot->ctrl == nullptr) {
//         debug_warn("slot nullptr %p\n", slot);
//         assert(0);
//     }
//     slot->ctrl->MarkIoThreadZoneWriteTime(s, e);
// }
//
// void bufferCopy2(void *arg1, void *arg2) { bufferCopy(arg1); }
//
// void bufferCopy(void *args)
// {
//     static timeval s, e;
//     static bool first = true;
//
//     BufferCopyArgs *bcArgs = reinterpret_cast<BufferCopyArgs *>(args);
//     RequestContext *slot = bcArgs->slot;
//     Device *device = bcArgs->dev;
//     ZstoreController *ctrl = bcArgs->slot->ctrl;
//     gettimeofday(&s, 0);
//     if (!first) {
//         ctrl->MarkCompletionThreadIdleTime(e, s);
//     }
//     first = false;
//
//     auto ioCtx = slot->ioContext;
//
//     {
//         uint32_t blockSize = Configuration::GetBlockSize();
//         uint32_t ptr = 0;
//         for (auto &it : slot->associatedRequests) {
//             auto &parent = it.first;
//             uint32_t uCtxOffset = it.second.first;
//             uint32_t uCtxSize = it.second.second;
//
//             if (parent) {
//                 memcpy((uint8_t *)ioCtx.data + ptr,
//                        parent->data + uCtxOffset * blockSize, uCtxSize);
//                 ptr += uCtxSize;
//             } else {
//                 memset((uint8_t *)ioCtx.data + ptr, 0, uCtxSize);
//             }
//         }
//     }
//
//     delete bcArgs;
//
//     if (slot->append) {
//         device->issueIo(zoneAppend, slot);
//     } else {
//         device->issueIo(zoneWrite, slot);
//     }
//     gettimeofday(&e, 0);
//     ctrl->MarkCompletionThreadBufferCopyTime(s, e);
// }
//
// void zoneWrite(void *args) { zoneWrite2(args, nullptr); }
//
// void zoneRead2(void *arg1, void *arg2)
// {
//     RequestContext *slot = reinterpret_cast<RequestContext *>(arg1);
//     auto ioCtx = slot->ioContext;
//     slot->stime = timestamp();
//
//     int rc = 0;
//     if (Configuration::GetEnableIOLatencyTest()) {
//         gettimeofday(&slot->timeA, 0);
//     }
//
//     if (Configuration::GetDeviceSupportMetadata()) {
//         rc = spdk_nvme_ns_cmd_read_with_md(
//             ioCtx.ns, ioCtx.qpair, ioCtx.data, ioCtx.metadata, ioCtx.offset,
//             ioCtx.size, ioCtx.cb, ioCtx.ctx, ioCtx.flags, 0, 0);
//     } else {
//         rc = spdk_nvme_ns_cmd_read(ioCtx.ns, ioCtx.qpair, ioCtx.data,
//                                    ioCtx.offset, ioCtx.size, ioCtx.cb,
//                                    ioCtx.ctx, ioCtx.flags);
//     }
//     if (rc != 0) {
//         fprintf(stderr, "Device read error!\n");
//     }
//     assert(rc == 0);
// }
//
// void realZoneWrite(RequestContext *slot)
// {
//     auto ioCtx = slot->ioContext;
//     int rc = 0;
//     if (Configuration::GetEnableIOLatencyTest()) {
//         gettimeofday(&slot->timeA, 0);
//     }
//
//     if (Configuration::GetDeviceSupportMetadata()) {
//         rc = spdk_nvme_ns_cmd_write_with_md(
//             ioCtx.ns, ioCtx.qpair, ioCtx.data, ioCtx.metadata, ioCtx.offset,
//             ioCtx.size, ioCtx.cb, ioCtx.ctx, ioCtx.flags, 0, 0);
//     } else {
//         rc = spdk_nvme_ns_cmd_write(ioCtx.ns, ioCtx.qpair, ioCtx.data,
//                                     ioCtx.offset, ioCtx.size, ioCtx.cb,
//                                     ioCtx.ctx, ioCtx.flags);
//     }
//     if (rc != 0) {
//         fprintf(stderr, "Device write error!\n");
//     }
//     assert(rc == 0);
// }
//
// void zoneRead(void *args) { zoneRead2(args, nullptr); }
//
// void zoneAppend2(void *arg1, void *arg2)
// {
//     RequestContext *slot = reinterpret_cast<RequestContext *>(arg1);
//     auto ioCtx = slot->ioContext;
//     slot->stime = timestamp();
//
//     //  fprintf(stderr, "Append %lx %lu\n", ioCtx.offset, ioCtx.size);
//
//     int rc = 0;
//     if (Configuration::GetEnableIOLatencyTest()) {
//         gettimeofday(&slot->timeA, 0);
//     }
//
//     //  {
//     //    uint32_t blockSize = Configuration::GetBlockSize();
//     //    uint32_t ptr = 0;
//     //    for (auto& it : slot->associatedRequests) {
//     //      auto& parent = it.first;
//     //      uint32_t uCtxOffset = it.second.first;
//     //      uint32_t uCtxSize = it.second.second;
//     //
//     //      if (parent) {
//     //        memcpy((uint8_t*)ioCtx.data + ptr,
//     //            parent->data + uCtxOffset * blockSize, uCtxSize);
//     //        ptr += uCtxSize;
//     //      } else {
//     //        memset((uint8_t*)ioCtx.data + ptr, 0, uCtxSize);
//     //      }
//     //    }
//     //  }
//
//     if (Configuration::GetDeviceSupportMetadata()) {
//         rc = spdk_nvme_zns_zone_append_with_md(
//             ioCtx.ns, ioCtx.qpair, ioCtx.data, ioCtx.metadata, ioCtx.offset,
//             ioCtx.size, ioCtx.cb, ioCtx.ctx, ioCtx.flags, 0, 0);
//     } else {
//         rc = spdk_nvme_zns_zone_append(ioCtx.ns, ioCtx.qpair, ioCtx.data,
//                                        ioCtx.offset, ioCtx.size, ioCtx.cb,
//                                        ioCtx.ctx, ioCtx.flags);
//     }
//     if (rc != 0) {
//         fprintf(stderr, "Device append error!\n");
//     }
//     assert(rc == 0);
// }
//
// void zoneAppend(void *args) { zoneAppend2(args, nullptr); }
//
// void zoneReset2(void *arg1, void *arg2)
// {
//     RequestContext *slot = reinterpret_cast<RequestContext *>(arg1);
//     auto ioCtx = slot->ioContext;
//     slot->stime = timestamp();
//     int rc = spdk_nvme_zns_reset_zone(ioCtx.ns, ioCtx.qpair, ioCtx.offset, 0,
//                                       ioCtx.cb, ioCtx.ctx);
//     if (rc != 0) {
//         fprintf(stderr, "Device reset error!\n");
//     }
//     assert(rc == 0);
// }
//
// void zoneReset(void *args) { zoneReset2(args, nullptr); }
//
// void zoneFinish2(void *arg1, void *arg2)
// {
//     RequestContext *slot = reinterpret_cast<RequestContext *>(arg1);
//     auto ioCtx = slot->ioContext;
//     slot->stime = timestamp();
//
//     int rc = spdk_nvme_zns_finish_zone(ioCtx.ns, ioCtx.qpair, ioCtx.offset,
//     0,
//                                        ioCtx.cb, ioCtx.ctx);
//     if (rc != 0) {
//         fprintf(stderr, "Device close error!\n");
//     }
//     assert(rc == 0);
// }
//
// void zoneFinish(void *args) { zoneFinish2(args, nullptr); }
//
// void tryDrainController(void *args)
// {
//     DrainArgs *drainArgs = (DrainArgs *)args;
//     drainArgs->ctrl->CheckSegments();
//     drainArgs->ctrl->ReclaimContexts();
//     drainArgs->ctrl->ProceedGc();
//     drainArgs->success = drainArgs->ctrl->GetNumInflightRequests() == 0 &&
//                          !drainArgs->ctrl->ExistsGc();
//
//     drainArgs->ready = true;
// }
//
// void completeOneEvent(void *arg, const struct spdk_nvme_cpl *completion)
// {
//     uint32_t *counter = (uint32_t *)arg;
//     (*counter)--;
//
//     if (spdk_nvme_cpl_is_error(completion)) {
//         fprintf(stderr, "I/O error status: %s\n",
//                 spdk_nvme_cpl_get_status_string(&completion->status));
//         fprintf(stderr, "I/O failed, aborting run\n");
//         assert(0);
//         exit(1);
//     }
// }
//
// void complete(void *arg, const struct spdk_nvme_cpl *completion)
// {
//     // bool *done = (bool *)arg;
//     // *done = true;
//
//     if (spdk_nvme_cpl_is_error(completion)) {
//         fprintf(stderr, "I/O error status: %s\n",
//                 spdk_nvme_cpl_get_status_string(&completion->status));
//         fprintf(stderr, "I/O failed, aborting run\n");
//         assert(0);
//         exit(1);
//     }
// }

void thread_send_msg(spdk_thread *thread, spdk_msg_fn fn, void *args)
{
    if (spdk_thread_send_msg(thread, fn, args) < 0) {
        printf("Thread send message failed: thread_name %s!\n",
               spdk_thread_get_name(thread));
        exit(-1);
    }
}
