#include "include/object.h"
#include "include/zstore_controller.h"
#include "src/include/utils.hpp"
#include <boost/outcome/success_failure.hpp>
#include <stdlib.h>
#include <string.h>

// static __thread unsigned int seed = 0;

// static void task_complete(struct arb_task *task)
// {
//     ZstoreController *zctrlr = (ZstoreController *)task->zctrlr;
//     auto worker = zctrlr->GetWorker();
//     auto taskpool = zctrlr->GetTaskPool();
//
//     assert(zctrlr != nullptr);
//     assert(worker != nullptr);
//     assert(worker->ns_ctx != nullptr);
//     // zctrlr->mWorker->ns_ctx = task->ns_ctx;
//     worker->ns_ctx->current_queue_depth--;
//     worker->ns_ctx->io_completed++;
//     // worker->ns_ctx->etime = std::chrono::high_resolution_clock::now();
//     // worker->ns_ctx->etimes.push_back(worker->ns_ctx->etime);
//
//     // log_debug("Before returning task {}", zctrlr->GetTaskPoolSize());
//     spdk_dma_free(task->buf);
//     spdk_mempool_put(taskpool, task);
//
//     // log_debug("After returning task {}", zctrlr->GetTaskPoolSize());
//
//     /*
//      * is_draining indicates when time has expired for the test run
//      * and we are just waiting for the previously submitted I/O
//      * to complete.  In this case, do not submit a new I/O to replace
//      * the one just completed.
//      */
//     // if (!worker->ns_ctx->is_draining) {
//     //     // log_info("IO count {}", zctrlr->mWorker->ns_ctx->io_completed);
//     //     submit_single_io(zctrlr);
//     // }
// }

// static void io_complete(void *ctx, const struct spdk_nvme_cpl *completion)
// {
//     task_complete((struct arb_task *)ctx);
//
//     // ZstoreController *zctrlr = (ZstoreController *)args;
//     // task_complete((struct arb_task *)ctx);
// }

// static int _FillObjectBuffer() {}

// Result<struct ZstoreObject *> AppendObject(struct obj_handle *handle,
//                                            uint64_t offset,
//                                            struct obj_object *doc,
//                                            ZstoreController *ctrl)
// {
// }

Result<ZstoreObject> ReadObject( // struct obj_handle *handle,
    uint64_t offset, void *ctx)
{
    ZstoreController *ctrl = (ZstoreController *)ctx;
    auto worker = ctrl->GetWorker();
    auto taskpool = ctrl->GetTaskPool();
    struct ns_entry *entry = worker->ns_ctx->entry;

    int rc;
    uint64_t offset_in_ios;
    int64_t _offset = 0;
    int key_alloc = 0;
    int meta_alloc = 0;
    int body_alloc = 0;

    // fdb_seqnum_t _seqnum;
    // timestamp_t _timestamp;
    void *comp_body = NULL;
    // struct docio_length _length, zero_length;
    struct ZstoreObject object;

    struct arb_task *task = NULL;
    // err_log_callback *log_callback = handle->log_callback;
    // memset(&zero_length, 0x0, sizeof(struct docio_length));

    task = (struct arb_task *)spdk_mempool_get(taskpool);
    if (!task) {
        log_error("Failed to get task from mTaskPool");
        exit(1);
    }

    task->buf = spdk_dma_zmalloc(g_arbitration.io_size_bytes, 0x200, NULL);
    if (!task->buf) {
        spdk_mempool_put(taskpool, task);
        log_error("task->buf spdk_dma_zmalloc failed");
        exit(1);
    }

    // task->ns_ctx = zctrlr->mWorker->ns_ctx;
    task->zctrlr = ctrl;
    assert(task->zctrlr == ctrl);

    if (g_arbitration.is_random) {
        offset_in_ios = rand_r(&seed) % entry->size_in_ios;
    } else {
        offset_in_ios = worker->ns_ctx->offset_in_ios++;
        if (worker->ns_ctx->offset_in_ios == entry->size_in_ios) {
            worker->ns_ctx->offset_in_ios = 0;
        }
    }
    rc = spdk_nvme_ns_cmd_read(entry->nvme.ns, worker->ns_ctx->qpair, task->buf,
                               offset_in_ios * entry->io_size_blocks,
                               entry->io_size_blocks, io_complete, task, 0);
    if (rc != 0) {
        log_error("starting I/O failed");
    } else {
        worker->ns_ctx->current_queue_depth++;
    }

    std::memcpy(task->buf, &object, sizeof(ZstoreObject));

    // assert(object.key != NULL);
    {
        // object.key = (void *)malloc(object.length.keylen);
        // key_alloc = 1;
    }
    // assert(object.body != NULL && object.length.bodylen);
    {
        // object.body = (void *)malloc(object.length.bodylen);
        // body_alloc = 1;
    }

    return outcome::success(object);
}
