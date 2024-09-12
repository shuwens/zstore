#pragma once
#include "device.h"
#include "global.h"
#include "request_handler.h"
#include "utils.hpp"
#include "zstore.h"
#include <cassert>
#include <chrono>
#include <fmt/core.h>
#include <isa-l.h>
#include <rte_errno.h>
#include <rte_mempool.h>
#include <spdk/env.h>
#include <spdk/event.h>
#include <spdk/init.h>
#include <spdk/nvme.h>
#include <spdk/nvmf.h>
#include <spdk/rpc.h>
#include <spdk/string.h>

class ZstoreController
{
  public:
    ~ZstoreController();
    void Init(bool need_env);

    // Add an object to the store
    void putObject(std::string key, void *data);

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

    struct spdk_thread *GetIoThread() { return mIoThread.thread; }

    struct spdk_thread *GetDispatchThread() { return mDispatchThread; }

    struct spdk_thread *GetHttpThread() { return mHttpThread; }

    struct spdk_thread *GetCompletionThread() { return mCompletionThread; }

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

    void CheckIoQpair(std::string msg);

    struct spdk_nvme_qpair *GetIoQpair();

    void CheckTaskPool(std::string msg);

    int GetTaskPoolSize() { return spdk_mempool_count(mTaskPool); }
    void SetTaskCount(int task_count) { mTaskCount = task_count; }

    void SetEventPoller(spdk_poller *p) { mEventsPoller = p; }
    void SetCompletionPoller(spdk_poller *p) { mCompletionPoller = p; }
    void SetDispatchPoller(spdk_poller *p) { mDispatchPoller = p; }
    void SetHttpPoller(spdk_poller *p) { mHttpPoller = p; }

    void SetQueuDepth(int queue_depth) { mQueueDepth = queue_depth; };
    int GetQueueDepth() { return mQueueDepth; };

    ZstoreHandler *mHandler;

    struct spdk_mempool *mTaskPool;
    int mTaskCount;

    struct ctrlr_entry *mController;
    struct ns_entry *mNamespace;
    struct worker_thread *mWorker;

    IoThread mIoThread;
    void initIoThread();
    // void initEcThread();
    void initDispatchThread();
    // void initIndexThread();
    void initCompletionThread();
    void initHttpThread();

    // simple way to terminate the server
    uint64_t tsc_end;

  private:
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

    // ZSTORE

    // object tables, used only by zstore
    // key -> tuple of <zns target, lba>
    // std::unordered_map<std::string, std::tuple<std::pair<std::string,
    // int32_t>>>
    // std::unordered_map<std::string, std::tuple<MapEntry, MapEntry,
    // MapEntry>>
    std::unordered_map<std::string, MapEntry> mZstoreMap;
    std::mutex mObjTableMutex;

    // in memory object tables, used only by kv store
    // std::map<std::string, kvobject> mem_obj_table;
    // std::mutex mem_obj_table_mutex;
    // ZstoreControllerMetadata mMeta;
    std::vector<Zone *> mZones;
    // std::vector<RequestContext> mResetContext;
};
