#pragma once
#include "CivetServer.h"
#include "common.h"
#include "configuration.h"
#include "device.h"
#include "global.h"
#include "utils.hpp"
#include "zstore.h"
#include "zstore_controller.h"
#include <cstring>
#include <iostream>
#include <mutex>
#include <shared_mutex>
#include <spdk/env.h> // Include SPDK's environment header
#include <thread>
#include <unistd.h>

#define PORT "8081"
#define EXAMPLE_URI "/example"
#define EXIT_URI "/exit"

// volatile bool exitNow = false;

const uint64_t zone_dist = 0x80000; // zone size
const int current_zone = 0;
// const int current_zone = 30;

auto zslba = zone_dist * current_zone;

class ZstoreHandler;
struct RequestContext;

class ZstoreController
{
  public:
    ~ZstoreController();
    int Init(bool need_env);
    int PopulateMap(bool bogus);
    int pivot;

    // threads
    void initIoThread();
    // void initEcThread();
    void initDispatchThread(bool use_object);
    // void initIndexThread();
    void initCompletionThread();
    void initHttpThread(bool dummy);

    struct spdk_thread *GetIoThread(int id) { return mIoThread[id].thread; };
    struct spdk_thread *GetDispatchThread() { return mDispatchThread; }
    struct spdk_thread *GetHttpThread() { return mHttpThread; }
    struct spdk_thread *GetCompletionThread() { return mCompletionThread; }

    struct spdk_nvme_qpair *GetIoQpair();
    bool CheckIoQpair(std::string msg);

    void SetEventPoller(spdk_poller *p) { mEventsPoller = p; }
    void SetCompletionPoller(spdk_poller *p) { mCompletionPoller = p; }
    void SetDispatchPoller(spdk_poller *p) { mDispatchPoller = p; }
    void SetHttpPoller(spdk_poller *p) { mHttpPoller = p; }

    void SetQueuDepth(int queue_depth) { mQueueDepth = queue_depth; };
    int GetQueueDepth() { return mQueueDepth; };

    Device *GetDevice() { return mDevices[0]; };

    void register_ctrlr(Device *device, struct spdk_nvme_ctrlr *ctrlr);
    void register_ns(struct spdk_nvme_ctrlr *ctrlr, struct spdk_nvme_ns *ns);

    int register_workers();
    int register_controllers(Device *device);
    void unregister_controllers(Device *device);
    int associate_workers_with_ns(Device *device);
    void zstore_cleanup();
    void zns_dev_init(Device *device, std::string ip1, std::string port1);
    void cleanup_ns_worker_ctx();
    void cleanup(uint32_t task_count);

    int init_ns_worker_ctx(struct ns_worker_ctx *ns_ctx,
                           enum spdk_nvme_qprio qprio);

    // Object APIs

    Result<MapEntry> find_object(std::string key);
    Result<void> release_object(std::string key);

    // int64_t alloc_object_entry();
    // void dealloc_object_entry(int64_t object_index);
    Result<void> putObject(std::string key, void *data, size_t size);
    // Result<Object> getObject(std::string key, sm_offset *ptr, size_t *size);
    // int seal_object(uint64_t object_id);
    Result<void> delete_object(std::string key);

    // Add an object to the store
    // void(std::string key, void *data);

    // Retrieve an object from the store by ID
    // Object *getObject(std::string key);
    int getObject(std::string key, uint8_t *readValidateBuffer);

    // Delete an object from the store by ID
    bool deleteObject(std::string key);

    void Append(uint64_t zslba, uint32_t size, void *data,
                zns_raid_request_complete cb_fn, void *cb_args);

    void Write(uint64_t offset, uint32_t size, void *data,
               zns_raid_request_complete cb_fn, void *cb_args);

    void Read(uint64_t offset, uint32_t size, void *data,
              zns_raid_request_complete cb_fn, void *cb_args);

    void Execute(uint64_t offset, uint32_t size, void *data, bool is_write,
                 zns_raid_request_complete cb_fn, void *cb_args);

    void EnqueueWrite(RequestContext *ctx);
    void EnqueueRead(RequestContext *ctx);
    // void EnqueueReadReaping(RequestContext *ctx);
    std::queue<RequestContext *> &GetWriteQueue() { return mWriteQueue; }
    std::queue<RequestContext *> &GetReadQueue() { return mReadQueue; }

    // std::queue<RequestContext *> &GetEventsToDispatch();

    int GetWriteQueueSize() { return mWriteQueue.size(); };
    int GetReadQueueSize() { return mReadQueue.size(); };

    // int GetEventsToDispatchSize();

    void Drain();

    std::queue<RequestContext *> &GetRequestQueue();
    std::mutex &GetRequestQueueMutex();
    int GetRequestQueueSize();

    // void UpdateIndexNeedLock(uint64_t lba, PhysicalAddr phyAddr);
    // void UpdateIndex(uint64_t lba, PhysicalAddr phyAddr);
    int GetNumInflightRequests();

    void WriteInDispatchThread(RequestContext *ctx);
    void ReadInDispatchThread(RequestContext *ctx);
    // void EnqueueEvent(RequestContext *ctx);

    void ReclaimContexts();
    void Flush();
    void Dump();

    // uint32_t GetHeaderRegionSize();
    // uint32_t GetDataRegionSize();
    // uint32_t GetFooterRegionSize();

    bool Append(RequestContext *ctx, uint32_t offset);
    // bool Read(RequestContext *ctx, uint32_t pos, PhysicalAddr phyAddr);
    void Reset(RequestContext *ctx);
    bool IsResetDone();
    void WriteComplete(RequestContext *ctx);
    void ReadComplete(RequestContext *ctx);
    // void ReclaimReadContext(ReadContext *readContext);

    void AddZone(Zone *zone);
    const std::vector<Zone *> &GetZones();
    void PrintStats();

    bool start = false;
    chrono_tp stime;

    // ZStore Map: this maps key to tuple of ZNS target and lba
    // TODO
    // at some point we need to discuss the usage of flat hash map or unordered
    // map key -> tuple of <zns target, lba> std::unordered_map<std::string,
    // std::tuple<std::pair<std::string, int32_t>>>
    // std::unordered_map<std::string, std::tuple<MapEntry, MapEntry,
    // MapEntry>>
    // std::unordered_map<std::string, MapEntry, std::less<>> mMap;
    std::unordered_map<std::string, MapEntry> mMap;
    std::mutex mMapMutex;

    // ZStore Bloom Filter: this maintains a bloom filter of hashes of
    // object name (key).
    //
    // For simplicity, right now we are just using a set to keep track of
    // the hashes
    std::unordered_set<std::string> mBF;
    std::mutex mBFMutex;

    RequestContextPool *mRequestContextPool;
    std::unordered_set<RequestContext *> mInflightRequestContext;

    mutable std::shared_mutex g_mutex_;
    // mutable std::shared_mutex context_pool_mutex_;
    std::mutex context_pool_mutex_;

    // std::mutex mTaskPoolMutex;
    bool verbose;

    bool isDraining;

  private:
    // number of devices
    int mN;
    ZstoreHandler *mHandler;

    // simple way to terminate the server
    uint64_t tsc_end;

    RequestContext *getContextForUserRequest();
    void doWrite(RequestContext *context);
    void doRead(RequestContext *context);

    std::vector<Device *> mDevices;
    std::queue<RequestContext *> mRequestQueue;
    std::mutex mRequestQueueMutex;

    spdk_poller *mEventsPoller = nullptr;
    spdk_poller *mDispatchPoller = nullptr;
    spdk_poller *mHttpPoller = nullptr;
    spdk_poller *mCompletionPoller = nullptr;

    int mQueueDepth = 1;

    IoThread mIoThread[16];
    // struct spdk_thread *mIoThread;
    struct spdk_thread *mDispatchThread;
    struct spdk_thread *mHttpThread;
    struct spdk_thread *mCompletionThread;

    // int64_t mNumInvalidBlocks = 0;
    // int64_t mNumBlocks = 0;

    std::queue<RequestContext *> mEventsToDispatch;
    std::queue<RequestContext *> mWriteQueue;
    std::queue<RequestContext *> mReadQueue;

    // in memory object tables, used only by kv store
    // std::map<std::string, kvobject> mem_obj_table;
    // std::mutex mem_obj_table_mutex;
    // ZstoreControllerMetadata mMeta;
    std::vector<Zone *> mZones;
    // std::vector<RequestContext> mResetContext;
};

class ZstoreHandler : public CivetHandler
{
  public:
    bool handleGet(CivetServer *server, struct mg_connection *conn)
    {
        const struct mg_request_info *req = mg_get_request_info(conn);

        char bucket[128], key[128];
        const char *query = req->query_string;
        parse_uri(req->local_uri, bucket, key);

        // log_info("Recv GET: bucket {}, key {}", bucket, key);

        // log_info("Recv GET with no key: bucket {}, key {}", bucket, key);

        auto ctrl = gZstoreController;

        // FIXME we assume the object is located, and turn into a read

        if (!ctrl->isDraining &&
            ctrl->mRequestContextPool->availableContexts.size() > 0) {
            if (!ctrl->start) {
                ctrl->start = true;
                ctrl->stime = std::chrono::high_resolution_clock::now();
            }

            RequestContext *slot =
                ctrl->mRequestContextPool->GetRequestContext(true);
            slot->ctrl = ctrl;
            assert(slot->ctrl == ctrl);

            auto ioCtx = slot->ioContext;
            // FIXME hardcode
            int size_in_ios = 212860928;
            int io_size_blocks = 1;
            // auto offset_in_ios = rand_r(&seed) % size_in_ios;
            auto offset_in_ios = 1;

            ioCtx.ns = ctrl->GetDevice()->GetNamespace();
            ioCtx.qpair = ctrl->GetIoQpair();
            ioCtx.data = slot->dataBuffer;
            ioCtx.offset = zslba + ctrl->GetDevice()->mTotalCounts;
            ioCtx.size = io_size_blocks;
            ioCtx.cb = complete;
            ioCtx.ctx = slot;
            ioCtx.flags = 0;
            slot->ioContext = ioCtx;

            assert(slot->ioContext.cb != nullptr);
            assert(slot->ctrl != nullptr);
            ctrl->EnqueueRead(slot);
            // busy = true;
        }

        const char *msg = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                          "<LocationConstraint "
                          "xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\">"
                          "here</LocationConstraint>";
        size_t len = strlen(msg);
        mg_send_http_ok(conn, "application/xml", len);
        mg_write(conn, msg, len);
        return 200;

        // return true;
    }

    bool handlePut(CivetServer *server, struct mg_connection *conn)
    {
        const struct mg_request_info *req = mg_get_request_info(conn);

        char bucket[128], key[128];
        const char *query = req->query_string;
        parse_uri(req->local_uri, bucket, key);

        // log_info("Recv PUT : bucket {}, key {}", bucket, key);

        // log_info("Recv GET with no key: bucket {}, key {}", bucket, key);

        auto ctrl = gZstoreController;

        // FIXME we assume the object is located, and turn into a read

        if (!ctrl->isDraining &&
            ctrl->mRequestContextPool->availableContexts.size() > 0) {
            if (!ctrl->start) {
                ctrl->start = true;
                ctrl->stime = std::chrono::high_resolution_clock::now();
            }

            RequestContext *slot =
                ctrl->mRequestContextPool->GetRequestContext(true);
            slot->ctrl = ctrl;
            assert(slot->ctrl == ctrl);

            auto ioCtx = slot->ioContext;
            // FIXME hardcode
            int size_in_ios = 212860928;
            int io_size_blocks = 1;
            // auto offset_in_ios = rand_r(&seed) % size_in_ios;
            auto offset_in_ios = 1;

            ioCtx.ns = ctrl->GetDevice()->GetNamespace();
            ioCtx.qpair = ctrl->GetIoQpair();
            ioCtx.data = slot->dataBuffer;
            ioCtx.offset = zslba + ctrl->GetDevice()->mTotalCounts;
            ioCtx.size = io_size_blocks;
            ioCtx.cb = complete;
            ioCtx.ctx = slot;
            ioCtx.flags = 0;
            slot->ioContext = ioCtx;

            assert(slot->ioContext.cb != nullptr);
            assert(slot->ctrl != nullptr);
            ctrl->EnqueueRead(slot);
            // busy = true;
        }

        const char *msg = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                          "<LocationConstraint "
                          "xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\">"
                          "here</LocationConstraint>";
        size_t len = strlen(msg);
        mg_send_http_ok(conn, "application/xml", len);
        mg_write(conn, msg, len);
        return 200;

        return true;
    }

    bool handleDelete(CivetServer *server, struct mg_connection *conn)
    {
        const struct mg_request_info *req = mg_get_request_info(conn);

        char bucket[128], key[128];
        const char *query = req->query_string;
        parse_uri(req->local_uri, bucket, key);

        log_info("Recv DELETE : bucket {}, key {}", bucket, key);

        // log_info("Recv GET with no key: bucket {}, key {}", bucket, key);

        auto ctrl = gZstoreController;

        // FIXME we assume the object is located, and turn into a read

        if (!ctrl->isDraining &&
            ctrl->mRequestContextPool->availableContexts.size() > 0) {
            if (!ctrl->start) {
                ctrl->start = true;
                ctrl->stime = std::chrono::high_resolution_clock::now();
            }

            RequestContext *slot =
                ctrl->mRequestContextPool->GetRequestContext(true);
            slot->ctrl = ctrl;
            assert(slot->ctrl == ctrl);

            auto ioCtx = slot->ioContext;
            // FIXME hardcode
            int size_in_ios = 212860928;
            int io_size_blocks = 1;
            // auto offset_in_ios = rand_r(&seed) % size_in_ios;
            auto offset_in_ios = 1;

            ioCtx.ns = ctrl->GetDevice()->GetNamespace();
            ioCtx.qpair = ctrl->GetIoQpair();
            ioCtx.data = slot->dataBuffer;
            ioCtx.offset = zslba + ctrl->GetDevice()->mTotalCounts;
            ioCtx.size = io_size_blocks;
            ioCtx.cb = complete;
            ioCtx.ctx = slot;
            ioCtx.flags = 0;
            slot->ioContext = ioCtx;

            assert(slot->ioContext.cb != nullptr);
            assert(slot->ctrl != nullptr);
            ctrl->EnqueueRead(slot);
            // busy = true;
        }

        const char *msg = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                          "<LocationConstraint "
                          "xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\">"
                          "here</LocationConstraint>";
        size_t len = strlen(msg);
        mg_send_http_ok(conn, "application/xml", len);
        mg_write(conn, msg, len);
        return 200;
    }

    bool handlePost(CivetServer *server, struct mg_connection *conn)
    {
        /* Handler may access the request info using mg_get_request_info */
        const struct mg_request_info *req_info = mg_get_request_info(conn);
        long long rlen, wlen;
        long long nlen = 0;
        long long tlen = req_info->content_length;
        char buf[1024];

        mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: "
                        "text/html\r\nConnection: close\r\n\r\n");

        mg_printf(conn, "<html><body>\n");
        mg_printf(conn, "<h2>This is the Foo POST handler!!!</h2>\n");
        mg_printf(conn, "<p>The request was:<br><pre>%s %s HTTP/%s</pre></p>\n",
                  req_info->request_method, req_info->request_uri,
                  req_info->http_version);
        mg_printf(conn, "<p>Content Length: %li</p>\n", (long)tlen);
        mg_printf(conn, "<pre>\n");

        while (nlen < tlen) {
            rlen = tlen - nlen;
            if (rlen > sizeof(buf)) {
                rlen = sizeof(buf);
            }
            rlen = mg_read(conn, buf, (size_t)rlen);
            if (rlen <= 0) {
                break;
            }
            wlen = mg_write(conn, buf, (size_t)rlen);
            if (wlen != rlen) {
                break;
            }
            nlen += wlen;
        }

        mg_printf(conn, "\n</pre>\n");
        mg_printf(conn, "</body></html>\n");

        return true;
    }
};
