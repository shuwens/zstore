#include "include/object.h"
#include "include/zstore_controller.h"
#include "spdk/string.h"
#include "src/include/configuration.h"
#include "src/include/utils.hpp"
#include <boost/outcome/success_failure.hpp>
#include <cassert>
#include <stdlib.h>
#include <string.h>

// Read a ZstoreObject from a 4096-byte buffer
ZstoreObject *read_from_buffer(const char *buffer, size_t buffer_size)
{
    // Allocate memory for the ZstoreObject
    LogEntry *entry = reinterpret_cast<LogEntry *>(const_cast<char *>(buffer));

    // Calculate the size of the key and body
    uint16_t key_size = entry->key_size;
    uint64_t datalen;

    // Copy the datalen from the buffer
    std::memcpy(&datalen, buffer + sizeof(LogEntry) + key_size,
                sizeof(uint64_t));

    // Allocate memory for the entire ZstoreObject
    ZstoreObject *obj =
        (ZstoreObject *)malloc(ZstoreObject::size(key_size, datalen));

    // Copy the LogEntry
    std::memcpy(&obj->entry, entry, sizeof(LogEntry));

    // Copy the key
    std::memcpy(obj->key, buffer + sizeof(LogEntry), key_size);

    // Copy the datalen
    obj->datalen = datalen;

    // Allocate and copy the body
    obj->body = malloc(datalen);
    std::memcpy(obj->body,
                buffer + sizeof(LogEntry) + key_size + sizeof(uint64_t),
                datalen);

    return obj;
}

void _zoneRead(void *arg1)
{
    RequestContext *ctx = reinterpret_cast<RequestContext *>(arg1);
    auto ioCtx = ctx->ioContext;
    int rc = 0;
    ZstoreController *ctrl = (ZstoreController *)ctx->ctrl;
    if (ctrl->verbose)
        log_debug("ding ding: we are running spdk read");
    rc = spdk_nvme_ns_cmd_read(ioCtx.ns, ioCtx.qpair, ioCtx.data, ioCtx.offset,
                               ioCtx.size, ioCtx.cb, ioCtx.ctx, 0);
    assert(rc == 0);
}

// struct obj_handle *handle,
ZstoreObject *ReadObject(uint64_t offset, void *ctx)
{
    // ZstoreController *ctrl = (ZstoreController *)ctx;
    // if (ctrl->verbose)
    //     log_debug("XXX: Reading object ");
    //
    // int rc;
    // uint64_t offset_in_ios;
    // int64_t _offset = 0;
    // int key_alloc = 0;
    // int meta_alloc = 0;
    // int body_alloc = 0;
    //
    // // fdb_seqnum_t _seqnum;
    // // timestamp_t _timestamp;
    // // struct docio_length _length, zero_length;
    struct ZstoreObject *object = nullptr;

    // {
    //
    //     if (ctrl->verbose)
    //         log_debug("Offset: {}, pool capacity {}, pool available {}",
    //         offset,
    //                   ctrl->mRequestContextPool->capacity,
    //                   ctrl->mRequestContextPool->availableContexts.size());
    //     RequestContext *slot;
    //     {
    //         std::unique_lock lock(ctrl->context_pool_mutex_);
    //         slot = ctrl->mRequestContextPool->GetRequestContext(true);
    //     }
    //     assert(slot != nullptr);
    //     // task->ns_ctx = zctrlr->mWorker->ns_ctx;
    //     slot->ctrl = ctrl;
    //     assert(slot->ctrl == ctrl);
    //
    //     auto ioCtx = slot->ioContext;
    //
    //     ioCtx.ns = entry->nvme.ns;
    //     ioCtx.qpair = worker->ns_ctx->qpair;
    //     // ioCtx.offset = offset_in_ios * entry->io_size_blocks;
    //     ioCtx.offset = 0;
    //     ioCtx.size = entry->io_size_blocks;
    //     // ioCtx.cb = io_complete;
    //     // ioCtx.ctx = task;
    //     ioCtx.cb = complete;
    //     ioCtx.ctx = slot;
    //     ioCtx.flags = 0;
    //
    //     if (ctrl->verbose)
    //         log_debug("buffer size {}, Offset: {}", slot->bufferSize,
    //         offset);
    //
    //     // ioCtx.data = slot->dataBuffer;
    //     ioCtx.data = (uint8_t *)spdk_zmalloc(
    //         4096, 4096, NULL, SPDK_ENV_SOCKET_ID_ANY, SPDK_MALLOC_DMA);
    //
    //     if (ctrl->verbose)
    //         log_debug("IO completed {}, Offset: {}",
    //                   worker->ns_ctx->io_completed, offset);
    //
    //     thread_send_msg(ctrl->GetIoThread(), _zoneRead, slot);
    //
    //     // rc = spdk_nvme_ns_cmd_read(ioCtx.ns, ioCtx.qpair, ioCtx.data,
    //     //                            ioCtx.offset, ioCtx.size, ioCtx.cb,
    //     //                            ioCtx.ctx, ioCtx.flags);
    //
    //     if (rc != 0) {
    //         log_error("NVME Read failed: {}", spdk_strerror(-rc));
    //         exit(1);
    //     } else {
    //         log_error("unimplemented");
    //         exit(1);
    //         worker->ns_ctx->current_queue_depth++;
    //     }
    //
    //     // object = read_from_buffer((const char *)ioCtx.data,
    //     // sizeof(ioCtx.data));
    // }
    // // std::memcpy(task->buf, &object, sizeof(ZstoreObject));
    //
    // // assert(object.key != NULL);
    // {
    //     // object.key = (void *)malloc(object.length.keylen);
    //     // key_alloc = 1;
    // }
    // // assert(object.body != NULL && object.length.bodylen);
    // {
    //     // object.body = (void *)malloc(object.length.bodylen);
    //     // body_alloc = 1;
    // }

    // return outcome::success(read_obj);
    return object;
}

void _create_dummy_object(ZstoreObject *obj, int offset)
{
    char key[] = "example_key";
    snprintf(key + strlen(key), sizeof(key) - strlen(key), "%d", offset);

    char body[] = "This is the body data.";
    uint16_t key_size = sizeof(key) - 1; // Exclude null terminator
    uint64_t datalen = sizeof(body) - 1; // Exclude null terminator

    // Allocate ZstoreObject and fill data
    obj = (ZstoreObject *)malloc(ZstoreObject::size(key_size, datalen));
    obj->entry.next_offset = 0;
    obj->entry.seqnum = 1;
    obj->entry.chunk_seqnum = 1;
    obj->entry.next = 0;
    obj->entry.key_size = key_size;
    obj->datalen = datalen;
    obj->body = malloc(datalen);
    std::memcpy(obj->body, body, datalen);
    std::memcpy(obj->key, key, key_size);
}

// Write a ZstoreObject to a 4096-byte buffer
bool write_to_buffer(ZstoreObject *obj, char *buffer, size_t buffer_size)
{
    // Calculate the total size of the object
    size_t total_size = sizeof(LogEntry) + obj->entry.key_size +
                        sizeof(uint64_t) + obj->datalen;

    if (total_size > buffer_size) {
        std::cerr << "Object is too large for the buffer!" << std::endl;
        return false;
    }

    // Copy LogEntry (without key) to the buffer
    std::memcpy(buffer, &obj->entry, sizeof(LogEntry));

    // Copy the key after the LogEntry
    std::memcpy(buffer + sizeof(LogEntry), obj->entry.key, obj->entry.key_size);

    // Copy the datalen
    std::memcpy(buffer + sizeof(LogEntry) + obj->entry.key_size, &obj->datalen,
                sizeof(uint64_t));

    // Copy the body data after the datalen
    std::memcpy(buffer + sizeof(LogEntry) + obj->entry.key_size +
                    sizeof(uint64_t),
                obj->body, obj->datalen);

    return true;
}

// bool AppendObject(uint64_t offset, void *ctx)
// {
//     ZstoreController *ctrl = (ZstoreController *)ctx;
//     auto worker = ctrl->GetWorker();
//     // auto taskpool = ctrl->GetTaskPool();
//     struct ns_entry *entry = worker->ns_ctx->entry;
//
//     int rc;
//     uint64_t offset_in_ios;
//     int64_t _offset = 0;
//     int key_alloc = 0;
//     int meta_alloc = 0;
//     int body_alloc = 0;
//
//     // fdb_seqnum_t _seqnum;
//     // timestamp_t _timestamp;
//     void *comp_body = NULL;
//     // struct docio_length _length, zero_length;
//     struct ZstoreObject *object;
//
//     struct arb_task *task = NULL;
//     // err_log_callback *log_callback = handle->log_callback;
//     // memset(&zero_length, 0x0, sizeof(struct docio_length));
//
//     task = (struct arb_task *)spdk_mempool_get(taskpool);
//     if (!task) {
//         log_error("Failed to get task from mTaskPool");
//         exit(1);
//     }
//
//     task->buf = spdk_dma_zmalloc(g_arbitration.io_size_bytes, 0x200, NULL);
//     if (!task->buf) {
//         spdk_mempool_put(taskpool, task);
//         log_error("task->buf spdk_dma_zmalloc failed");
//         exit(1);
//     }
//     _create_dummy_object(object, offset);
//     if (write_to_buffer(object, (char *)task->buf, sizeof(task->buf))) {
//         std::cout << "ZstoreObject written to buffer successfully."
//                   << std::endl;
//     }
//
//     // task->ns_ctx = zctrlr->mWorker->ns_ctx;
//     task->zctrlr = ctrl;
//     assert(task->zctrlr == ctrl);
//
//     auto zslba = Configuration().GetZslba();
//
//     rc = spdk_nvme_zns_zone_append(entry->nvme.ns, worker->ns_ctx->qpair,
//                                    task->buf, zslba, entry->io_size_blocks,
//                                    io_complete, task, 0);
//
//     if (rc != 0) {
//         log_error("NVME Read failed: {}", spdk_strerror(-rc));
//     } else {
//         worker->ns_ctx->current_queue_depth++;
//     }
//
//     // ZstoreObject *read_obj =
//     //     read_from_buffer((const char *)task->buf, sizeof(task->buf));
//
//     // std::memcpy(task->buf, &object, sizeof(ZstoreObject));
//
//     // assert(object.key != NULL);
//     {
//         // object.key = (void *)malloc(object.length.keylen);
//         // key_alloc = 1;
//     }
//     // assert(object.body != NULL && object.length.bodylen);
//     {
//         // object.body = (void *)malloc(object.length.bodylen);
//         // body_alloc = 1;
//     }
//
//     // return outcome::success(read_obj);
//     return true;
// }
