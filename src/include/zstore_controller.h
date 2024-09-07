#pragma once

#include "device.h"
#include "global.h"
#include "helper.h"
#include "messages_and_functions.h"
#include "object.h"
#include "request_handler.h"
#include "segment.h"
#include "utils.hpp"
#include "zns_device.h"
#include "zone.h"
#include <chrono>
#include <fmt/core.h>
#include <fstream>

struct ZstoreControllerMetadata {
    // uint64_t segmentId;             // 8
    uint64_t zones[16]; // 128
    // uint32_t stripeSize;            // 16384
    // uint32_t stripeDataSize;        // 12288
    // uint32_t stripeParitySize;      // 4096
    // uint32_t stripeGroupSize = 256; // 256 or 1
    // uint32_t stripeUnitSize;
    uint32_t n;        // 4
    uint32_t k;        // 3
    uint32_t numZones; // 4
    // RAIDLevel raidScheme; // 1
    // uint8_t useAppend;    // 0 for Zone Write and 1 for Zone Append
};

class ZstoreController
{
  public:
    ~ZstoreController();
    /**
     * @brief Initialzie the Zstore block device
     *
     * Threading model is the following:
     * - we need one thread per ns/ns worker/SSD
     * - we need one thread which handles civetweb tasks
     *
     * @param need_env need to initialize SPDK environment. Example of false,
     * app is part of a SPDK bdev and the bdev already initialized the
     * environment.
     */
    void Init(bool need_env);

    // Add an object to the store
    void putObject(std::string key, void *data);

    // Retrieve an object from the store by ID
    // Object *getObject(std::string key);
    int getObject(std::string key, uint8_t *readValidateBuffer);

    // Delete an object from the store by ID
    bool deleteObject(std::string key);

    /**
     * @brief Append a block from the Zstore system
     *
     * @param zslba the logical block address of the Zstore block device, in
     * bytes, aligned with 4KiB
     * @param size the number of bytes written, in bytes, aligned with 4KiB
     * @param data the data buffer
     * @param cb_fn call back function provided by the client (SPDK bdev)
     * @param cb_args arguments provided to the call back function
     */
    void Append(uint64_t zslba, uint32_t size, void *data,
                zns_raid_request_complete cb_fn, void *cb_args);

    /**
     * @brief Write a block from the Zstore system
     *
     * @param offset the logical block address of the Zstore block device, in
     * bytes, aligned with 4KiB
     * @param size the number of bytes written, in bytes, aligned with 4KiB
     * @param data the data buffer
     * @param cb_fn call back function provided by the client (SPDK bdev)
     * @param cb_args arguments provided to the call back function
     */
    void Write(uint64_t offset, uint32_t size, void *data,
               zns_raid_request_complete cb_fn, void *cb_args);

    /**
     * @brief Read a block from the Zstore system
     *
     * @param offset the logical block address of the Zstore block device, in
     * bytes, aligned with 4KiB
     * @param size the number of bytes read, in bytes, aligned with 4KiB
     * @param data the data buffer
     * @param cb_fn call back function provided by the client (SPDK bdev)
     * @param cb_args arguments provided to the call back function
     */
    void Read(uint64_t offset, uint32_t size, void *data,
              zns_raid_request_complete cb_fn, void *cb_args);

    void Execute(uint64_t offset, uint32_t size, void *data, bool is_write,
                 zns_raid_request_complete cb_fn, void *cb_args);

    void EnqueueWrite(RequestContext *ctx);
    void EnqueueReadPrepare(RequestContext *ctx);
    void EnqueueReadReaping(RequestContext *ctx);
    std::queue<RequestContext *> &GetWriteQueue();
    std::queue<RequestContext *> &GetReadPrepareQueue();
    std::queue<RequestContext *> &GetReadReapingQueue();

    std::queue<RequestContext *> &GetEventsToDispatch();

    int GetWriteQueueSize();
    int GetReadPrepareQueueSize();
    int GetReadReapingQueueSize();

    int GetEventsToDispatchSize();

    /**
     * @brief Drain the Zstore system to finish all the in-flight requests
     */
    void Drain();

    /**
     * @brief Get the Request Queue object
     *
     * @return std::queue<RequestContext*>&
     */
    std::queue<RequestContext *> &GetRequestQueue();
    std::mutex &GetRequestQueueMutex();
    int GetRequestQueueSize();

    void UpdateIndexNeedLock(uint64_t lba, PhysicalAddr phyAddr);
    void UpdateIndex(uint64_t lba, PhysicalAddr phyAddr);
    int GetNumInflightRequests();
    bool ProceedGc();
    bool ExistsGc();
    bool CheckSegments();

    void WriteInDispatchThread(RequestContext *ctx);
    void ReadInDispatchThread(RequestContext *ctx);
    void EnqueueEvent(RequestContext *ctx);

    uint32_t GcBatchUpdateIndex(
        const std::vector<uint64_t> &lbas,
        const std::vector<std::pair<PhysicalAddr, PhysicalAddr>> &pbas);

    /**
     * @brief Get the Io Thread object
     *
     * @param id
     * @return struct spdk_thread*
     */
    struct spdk_thread *GetIoThread(int id);

    /**
     * @brief Get the Dispatch Thread object
     *
     * @return struct spdk_thread*
     */
    struct spdk_thread *GetDispatchThread();
    /**
     * @brief Get the Ec Thread object
     *
     * @return struct spdk_thread*
     */
    // struct spdk_thread *GetEcThread();

    /**
     * @brief Get the Index And Completion Thread object
     *
     * @return struct spdk_thread*
     */
    struct spdk_thread *GetIndexThread();

    /**
     * @brief Get the Completion Thread object
     *
     * @return struct spdk_thread*
     */
    struct spdk_thread *GetCompletionThread();

    /**
     * @brief Get the HTTP Thread object
     *
     * @return struct spdk_thread*
     */
    struct spdk_thread *GetHttpThread();

    /**
     * @brief Find a PhysicalAddress given a LogicalAddress of a block
     *
     * @param lba the logial address of the queried block
     * @param phyAddr the pointer to store the physical address of the queried
     * block
     * @return true the queried block was written before and not trimmed
     * @return false the queried block was not written before or was trimmed
     */
    bool LookupIndex(uint64_t lba, PhysicalAddr *phyAddr);

    void ReclaimContexts();
    void Flush();
    void Dump();

    uint32_t GetHeaderRegionSize();
    uint32_t GetDataRegionSize();
    uint32_t GetFooterRegionSize();

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

    struct spdk_nvme_qpair *GetIoQpair() { return g_devices[0]->GetIoQueue(); }

  private:
    RequestContext *getContextForUserRequest();
    void doWrite(RequestContext *context);
    void doRead(RequestContext *context);

    // void initEcThread();
    void initDispatchThread();
    void initIoThread();
    void initIndexThread();
    void initCompletionThread();
    void initHttpThread();
    void initGc();

    void createSegmentIfNeeded(Segment **segment, uint32_t spId);
    bool scheduleGc();

    void initializeGcTask();
    bool progressGcWriter();
    bool progressGcReader();

    void restart();
    void rebuild(uint32_t failedDriveId);

    std::vector<Device *> mDevices;
    std::unordered_set<Segment *> mSealedSegments;
    std::vector<Segment *> mSegmentsToSeal;
    std::vector<Segment *> mOpenSegments;
    Segment *mSpareSegment;
    PhysicalAddr *mAddressMap;

    RequestContextPool *mRequestContextPoolForUserRequests;
    std::unordered_set<RequestContext *> mInflightRequestContext;

    RequestContextPool *mRequestContextPoolForZstore;
    ReadContextPool *mReadContextPool;
    StripeWriteContextPool **mStripeWriteContextPools;

    std::queue<RequestContext *> mRequestQueue;
    std::mutex mRequestQueueMutex;

    // struct GcTask mGcTask;

    uint32_t mNumOpenSegments = 1;

    IoThread mIoThread[16];
    struct spdk_thread *mDispatchThread;
    struct spdk_thread *mHttpThread;
    struct spdk_thread *mIndexThread;
    struct spdk_thread *mCompletionThread;

    int64_t mNumInvalidBlocks = 0;
    int64_t mNumBlocks = 0;

    std::queue<RequestContext *> mEventsToDispatch;
    std::queue<RequestContext *> mWriteQueue;
    std::queue<RequestContext *> mReadPrepareQueue;
    std::queue<RequestContext *> mReadReapingQueue;

    uint32_t mAvailableStorageSpaceInSegments = 0;
    uint32_t mStorageSpaceThresholdForGcInSegments = 0;
    uint32_t mNumTotalZones = 0;

    uint32_t mNextAppendOpenSegment = 0;
    uint32_t mNextAssignedSegmentId = 0;
    uint32_t mGlobalTimestamp = 0;

    uint32_t mHeaderRegionSize = 0;
    uint32_t mDataRegionSize = 0;
    uint32_t mFooterRegionSize = 0;

    std::unordered_set<RequestContext *> mReadsInCurrentGcEpoch;

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
    ZstoreControllerMetadata mMeta;
    std::vector<Zone *> mZones;
    std::vector<RequestContext> mResetContext;
};
