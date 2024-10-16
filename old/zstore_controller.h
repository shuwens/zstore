#pragma once

#include "global.h"
#include "helper.h"
#include "object.h"
#include "request_handler.h"
#include "utils.hpp"
#include "zns_device.h"
#include <chrono>
#include <fmt/core.h>
#include <fstream>

class ZstoreController
{
  public:
    ~ZstoreController();
    /**
     * @brief Initialzie the Zstore block device
     *
     * @param need_env need to initialize SPDK environment. Example of false,
     *                 app is part of a SPDK bdev and the bdev already
     * initialized the environment.
     */
    void Init(bool need_env);

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
    struct spdk_thread *GetEcThread();
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

    GcTask *GetGcTask();
    void RemoveRequestFromGcEpochIfNecessary(RequestContext *ctx);

  private:
    RequestContext *getContextForUserRequest();
    void doWrite(RequestContext *context);
    void doRead(RequestContext *context);

    void initEcThread();
    void initDispatchThread();
    void initIoThread();
    void initIndexThread();
    void initCompletionThread();
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

    RequestContextPool *mRequestContextPoolForSegments;
    ReadContextPool *mReadContextPool;
    StripeWriteContextPool **mStripeWriteContextPools;

    std::queue<RequestContext *> mRequestQueue;
    std::mutex mRequestQueueMutex;

    struct GcTask mGcTask;

    uint32_t mNumOpenSegments = 1;

    IoThread mIoThread[16];
    struct spdk_thread *mDispatchThread;
    struct spdk_thread *mEcThread;
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
};
