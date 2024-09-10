#include "include/common.h"
#include "include/configuration.h"
#include "include/request_handler.h"
#include "include/utils.hpp"
#include "include/zstore.h"
#include "include/zstore_controller.h"
#include "spdk/thread.h"
#include <isa-l.h>
#include <queue>
#include <sched.h>
#include <spdk/event.h>
#include <sys/time.h>
#include <vector>

int handleHttpRequest(void *args)
{
    bool busy = false;
    ZstoreController *zstoreController = (ZstoreController *)args;
    // log_info("XXXX");

    return busy ? SPDK_POLLER_BUSY : SPDK_POLLER_IDLE;
}

int httpWorker(void *args)
{
    ZstoreController *zstoreController = (ZstoreController *)args;
    struct spdk_thread *thread = zstoreController->GetHttpThread();
    spdk_set_thread(thread);
    spdk_poller_register(handleHttpRequest, zstoreController, 0);
    while (true) {
        spdk_thread_poll(thread, 0, 0);
    }
}

int handleIoCompletions(void *args)
{
    int rc;
    ZstoreController *zstoreController = (ZstoreController *)args;
    enum spdk_thread_poller_rc poller_rc = SPDK_POLLER_IDLE;
    rc = spdk_nvme_qpair_process_completions(zstoreController->GetIoQpair(), 0);
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
    ZstoreController *zstoreController = (ZstoreController *)args;
    struct spdk_thread *thread = zstoreController->mIoThread.thread;
    spdk_set_thread(thread);
    spdk_poller_register(handleIoCompletions, zstoreController, 0);
    while (true) {
        spdk_thread_poll(thread, 0, 0);
    }
}

int dispatchWorker(void *args)
{
    // ZstoreController *raidController = (ZstoreController *)args;
    // struct spdk_thread *thread = raidController->GetDispatchThread();
    // spdk_set_thread(thread);
    // spdk_poller *p1, *p2;
    // p1 = spdk_poller_register(handleEventsDispatch, raidController, 1);
    // p2 = spdk_poller_register(handleBackgroundTasks, raidController, 1);
    // raidController->SetEventPoller(p1);
    // raidController->SetBackgroundPoller(p2);
    // while (true) {
    //     spdk_thread_poll(thread, 0, 0);
    // }
}

int completionWorker(void *args)
{
    // ZstoreController *raidController = (ZstoreController *)args;
    // struct spdk_thread *thread = raidController->GetCompletionThread();
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
