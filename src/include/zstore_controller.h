#pragma once
#include "device.h"
#include "global.h"
#include "utils.hpp"
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
    // std::queue<RequestContext *> &GetWriteQueue();
    // std::queue<RequestContext *> &GetReadPrepareQueue();
    // std::queue<RequestContext *> &GetReadReapingQueue();

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
    // bool ProceedGc();
    // bool ExistsGc();
    // bool CheckSegments();

    void WriteInDispatchThread(RequestContext *ctx);
    void ReadInDispatchThread(RequestContext *ctx);
    // void EnqueueEvent(RequestContext *ctx);

    // uint32_t GcBatchUpdateIndex(
    //     const std::vector<uint64_t> &lbas,
    //     const std::vector<std::pair<PhysicalAddr, PhysicalAddr>> &pbas);

    struct spdk_thread *GetIoThread(int id);

    struct spdk_thread *GetDispatchThread();

    // struct spdk_thread *GetIndexThread();

    struct spdk_thread *GetCompletionThread();

    struct spdk_thread *GetHttpThread();

    void ReclaimContexts();
    void Flush();
    void Dump();

    // uint32_t GetHeaderRegionSize();
    // uint32_t GetDataRegionSize();
    // uint32_t GetFooterRegionSize();

    // GcTask *GetGcTask();
    // void RemoveRequestFromGcEpochIfNecessary(RequestContext *ctx);

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

    struct spdk_nvme_qpair *GetIoQpair() { return mWorker->ns_ctx->qpair; }

    struct spdk_mempool *mTaskPool;

    struct ctrlr_entry *mController;
    struct ns_entry *mNamespace;
    struct worker_thread *mWorker;

    IoThread mIoThread;
    void initIoThread();

  private:
    RequestContext *getContextForUserRequest();
    void doWrite(RequestContext *context);
    void doRead(RequestContext *context);

    // void initEcThread();
    void initDispatchThread();
    // void initIndexThread();
    void initCompletionThread();
    void initHttpThread();

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

    // struct GcTask mGcTask;

    // uint32_t mNumOpenSegments = 1;

    // IoThread mIoThread[16];
    // struct spdk_thread *mIoThread;
    struct spdk_thread *mDispatchThread;
    struct spdk_thread *mHttpThread;
    struct spdk_thread *mCompletionThread;

    // int64_t mNumInvalidBlocks = 0;
    // int64_t mNumBlocks = 0;

    std::queue<RequestContext *> mEventsToDispatch;
    std::queue<RequestContext *> mWriteQueue;
    std::queue<RequestContext *> mReadPrepareQueue;
    std::queue<RequestContext *> mReadReapingQueue;

    // uint32_t mAvailableStorageSpaceInSegments = 0;
    // uint32_t mStorageSpaceThresholdForGcInSegments = 0;
    // uint32_t mNumTotalZones = 0;

    // uint32_t mNextAppendOpenSegment = 0;
    // uint32_t mNextAssignedSegmentId = 0;
    // uint32_t mGlobalTimestamp = 0;

    // uint32_t mHeaderRegionSize = 0;
    // uint32_t mDataRegionSize = 0;
    // uint32_t mFooterRegionSize = 0;

    // std::unordered_set<RequestContext *> mReadsInCurrentGcEpoch;

    // ZSTORE

    // object tables, used only by zstore
    // key -> tuple of <zns target, lba>
    // std::unordered_map<std::string, std::tuple<std::pair<std::string,
    // int32_t>>>
    // std::unordered_map<std::string, std::tuple<MapEntry, MapEntry, MapEntry>>
    std::unordered_map<std::string, MapEntry> mZstoreMap;
    std::mutex mObjTableMutex;

    // in memory object tables, used only by kv store
    // std::map<std::string, kvobject> mem_obj_table;
    // std::mutex mem_obj_table_mutex;
    // ZstoreControllerMetadata mMeta;
    std::vector<Zone *> mZones;
    // std::vector<RequestContext> mResetContext;
};
