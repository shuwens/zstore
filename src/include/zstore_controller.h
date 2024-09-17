#pragma once
#include "common.h"
#include "configuration.h"
#include "device.h"
#include "request_handler.h"
#include "utils.hpp"
#include "zstore.h"
#include <iostream>
#include <mutex>
#include <spdk/env.h> // Include SPDK's environment header
#include <thread>

// #include "log_disk.h"
// #include "object_log.h"
// #include "store.h"

typedef std::unordered_map<std::string, MapEntry>::const_iterator MapIter;

class ZstoreController
{
  public:
    ~ZstoreController();
    int Init(bool need_env);
    int PopulateMap(bool bogus);
    int pivot;

    // threads
    IoThread mIoThread;
    void initIoThread();
    // void initEcThread();
    void initDispatchThread(bool use_object);
    // void initIndexThread();
    void initCompletionThread();
    void initHttpThread();
    struct spdk_thread *GetIoThread() { return mIoThread.thread; }
    struct spdk_thread *GetDispatchThread() { return mDispatchThread; }
    struct spdk_thread *GetHttpThread() { return mHttpThread; }
    struct spdk_thread *GetCompletionThread() { return mCompletionThread; }

    struct spdk_nvme_qpair *GetIoQpair();
    void CheckIoQpair(std::string msg);

    struct spdk_mempool *GetTaskPool() { return mTaskPool; };
    void CheckTaskPool(std::string msg);
    int GetTaskPoolSize() { return spdk_mempool_count(mTaskPool); }
    int GetTaskCount() { return mTaskCount; };
    void SetTaskCount(int task_count) { mTaskCount = task_count; }

    void SetEventPoller(spdk_poller *p) { mEventsPoller = p; }
    void SetCompletionPoller(spdk_poller *p) { mCompletionPoller = p; }
    void SetDispatchPoller(spdk_poller *p) { mDispatchPoller = p; }
    void SetHttpPoller(spdk_poller *p) { mHttpPoller = p; }

    void SetQueuDepth(int queue_depth) { mQueueDepth = queue_depth; };
    int GetQueueDepth() { return mQueueDepth; };

    void register_ctrlr(struct spdk_nvme_ctrlr *ctrlr);
    void register_ns(struct spdk_nvme_ctrlr *ctrlr, struct spdk_nvme_ns *ns);

    struct worker_thread *GetWorker() { return mWorker; };
    struct ns_entry *GetNamespace() { return mNamespace; };

    int register_workers();
    int register_controllers(struct arb_context *ctx);
    void unregister_controllers();
    int associate_workers_with_ns();
    void zstore_cleanup();
    void zns_dev_init(struct arb_context *ctx, std::string ip1,
                      std::string port1);
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

    // void EnqueueWrite(RequestContext *ctx);
    // void EnqueueReadPrepare(RequestContext *ctx);
    // void EnqueueReadReaping(RequestContext *ctx);

    std::queue<RequestContext *> &GetWriteQueue() { return mWriteQueue; }

    std::queue<RequestContext *> &GetReadQueue() { return mReadQueue; }

    // std::queue<RequestContext *> &GetEventsToDispatch();

    // int GetWriteQueueSize();
    // int GetReadPrepareQueueSize();
    // int GetReadReapingQueueSize();

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
    bool Read(RequestContext *ctx, uint32_t pos, PhysicalAddr phyAddr);
    void Reset(RequestContext *ctx);
    bool IsResetDone();
    void WriteComplete(RequestContext *ctx);
    void ReadComplete(RequestContext *ctx);
    void ReclaimReadContext(ReadContext *readContext);

    void AddZone(Zone *zone);
    const std::vector<Zone *> &GetZones();
    void PrintStats();
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

    std::mutex mTaskPoolMutex;

  private:
    ZstoreHandler *mHandler;

    struct spdk_mempool *mTaskPool;
    int mTaskCount;
    // Create a global mutex to protect access to the mempool

    struct ctrlr_entry *mController;
    struct ns_entry *mNamespace;
    struct worker_thread *mWorker;

    // simple way to terminate the server
    uint64_t tsc_end;

    RequestContext *getContextForUserRequest();
    void doWrite(RequestContext *context);
    void doRead(RequestContext *context);

    std::vector<Device *> mDevices;
    // std::unordered_set<Segment *> mSealedSegments;
    // std::vector<Segment *> mSegmentsToSeal;
    // std::vector<Segment *> mOpenSegments;
    // Segment *mSpareSegment;
    // PhysicalAddr *mAddressMap;

    RequestContextPool *mRequestContextPool;
    std::unordered_set<RequestContext *> mInflightRequestContext;

    // RequestContextPool *mRequestContextPoolForZstore;
    // ReadContextPool *mReadContextPool;
    // StripeWriteContextPool **mStripeWriteContextPools;

    std::queue<RequestContext *> mRequestQueue;
    std::mutex mRequestQueueMutex;

    spdk_poller *mEventsPoller = nullptr;
    spdk_poller *mDispatchPoller = nullptr;
    spdk_poller *mHttpPoller = nullptr;
    spdk_poller *mCompletionPoller = nullptr;

    int mQueueDepth = 1;

    // IoThread mIoThread[16];
    // struct spdk_thread *mIoThread;
    struct spdk_thread *mDispatchThread;
    struct spdk_thread *mHttpThread;
    struct spdk_thread *mCompletionThread;

    // int64_t mNumInvalidBlocks = 0;
    // int64_t mNumBlocks = 0;

    std::queue<RequestContext *> mEventsToDispatch;
    std::queue<RequestContext *> mWriteQueue;
    std::queue<RequestContext *> mReadQueue;

    // uint32_t mAvailableStorageSpaceInSegments = 0;
    // uint32_t mNumTotalZones = 0;

    // uint32_t mNextAppendOpenSegment = 0;
    // uint32_t mNextAssignedSegmentId = 0;
    // uint32_t mGlobalTimestamp = 0;

    // uint32_t mHeaderRegionSize = 0;
    // uint32_t mDataRegionSize = 0;
    // uint32_t mFooterRegionSize = 0;

    // in memory object tables, used only by kv store
    // std::map<std::string, kvobject> mem_obj_table;
    // std::mutex mem_obj_table_mutex;
    // ZstoreControllerMetadata mMeta;
    std::vector<Zone *> mZones;
    // std::vector<RequestContext> mResetContext;
};
