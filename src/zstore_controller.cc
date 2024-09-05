#include "include/zstore_controller.h"
#include "common.cc"
#include "device.cc"
#include "include/common.h"
#include "include/device.h"
#include "include/segment.h"
#include "include/utils.hpp"
#include "messages_and_functions.cc"
#include "segment.cc"
#include <algorithm>
#include <isa-l.h>
#include <rte_errno.h>
#include <rte_mempool.h>
#include <spdk/env.h>
#include <spdk/event.h>
#include <spdk/init.h>
#include <spdk/nvme.h>
#include <spdk/rpc.h>
#include <spdk/string.h>
#include <sys/time.h>
#include <thread>
#include <tuple>

static void busyWait(bool *ready)
{
    while (!*ready) {
        if (spdk_get_thread() == nullptr) {
            std::this_thread::sleep_for(std::chrono::seconds(0));
        }
    }
}

static auto quit(void *args) { exit(0); }

void ZstoreController::initHttpThread()
{
    struct spdk_cpuset cpumask;
    spdk_cpuset_zero(&cpumask);
    spdk_cpuset_set_cpu(&cpumask, Configuration::GetHttpThreadCoreId(), true);
    mHttpThread = spdk_thread_create("HttpThread", &cpumask);
    printf("Create HTTP processing thread %s %lu\n",
           spdk_thread_get_name(mHttpThread), spdk_thread_get_id(mHttpThread));
    int rc = spdk_env_thread_launch_pinned(Configuration::GetHttpThreadCoreId(),
                                           ecWorker, this);
    if (rc < 0) {
        printf("Failed to launch ec thread error: %s\n", spdk_strerror(rc));
    }
}

void ZstoreController::initIndexThread()
{
    struct spdk_cpuset cpumask;
    spdk_cpuset_zero(&cpumask);
    spdk_cpuset_set_cpu(&cpumask, Configuration::GetIndexThreadCoreId(), true);
    mIndexThread = spdk_thread_create("IndexThread", &cpumask);
    printf("Create index and completion thread %s %lu\n",
           spdk_thread_get_name(mIndexThread),
           spdk_thread_get_id(mIndexThread));
    int rc = spdk_env_thread_launch_pinned(
        Configuration::GetIndexThreadCoreId(), indexWorker, this);
    if (rc < 0) {
        printf("Failed to launch index completion thread, error: %s\n",
               spdk_strerror(rc));
    }
}

void ZstoreController::initCompletionThread()
{
    struct spdk_cpuset cpumask;
    spdk_cpuset_zero(&cpumask);
    spdk_cpuset_set_cpu(&cpumask, Configuration::GetCompletionThreadCoreId(),
                        true);
    mCompletionThread = spdk_thread_create("CompletionThread", &cpumask);
    printf("Create index and completion thread %s %lu\n",
           spdk_thread_get_name(mCompletionThread),
           spdk_thread_get_id(mCompletionThread));
    int rc = spdk_env_thread_launch_pinned(
        Configuration::GetCompletionThreadCoreId(), completionWorker, this);
    if (rc < 0) {
        printf("Failed to launch completion thread, error: %s\n",
               spdk_strerror(rc));
    }
}

void ZstoreController::initDispatchThread()
{
    struct spdk_cpuset cpumask;
    spdk_cpuset_zero(&cpumask);
    spdk_cpuset_set_cpu(&cpumask, Configuration::GetDispatchThreadCoreId(),
                        true);
    mDispatchThread = spdk_thread_create("DispatchThread", &cpumask);
    printf("Create dispatch thread %s %lu\n",
           spdk_thread_get_name(mDispatchThread),
           spdk_thread_get_id(mDispatchThread));
    int rc = spdk_env_thread_launch_pinned(
        Configuration::GetDispatchThreadCoreId(), dispatchWorker, this);
    if (rc < 0) {
        printf("Failed to launch dispatch thread error: %s %s\n", strerror(rc),
               spdk_strerror(rc));
    }
}

void ZstoreController::initIoThread()
{
    struct spdk_cpuset cpumask;
    if (verbose)
        log_debug("init Io thread {}", Configuration::GetNumIoThreads());
    for (uint32_t threadId = 0; threadId < Configuration::GetNumIoThreads();
         ++threadId) {
        spdk_cpuset_zero(&cpumask);
        spdk_cpuset_set_cpu(&cpumask,
                            Configuration::GetIoThreadCoreId(threadId), true);
        mIoThread[threadId].thread = spdk_thread_create("IoThread", &cpumask);
        assert(mIoThread[threadId].thread != nullptr);
        mIoThread[threadId].controller = this;
        int rc = spdk_env_thread_launch_pinned(
            Configuration::GetIoThreadCoreId(threadId), ioWorker,
            &mIoThread[threadId]);
        printf("ZNS_RAID io thread %s %lu\n",
               spdk_thread_get_name(mIoThread[threadId].thread),
               spdk_thread_get_id(mIoThread[threadId].thread));
        if (rc < 0) {
            printf("Failed to launch IO thread error: %s %s\n", strerror(rc),
                   spdk_strerror(rc));
        }
    }
}

void ZstoreController::Init(bool need_env)
{
    int rc = 0;
    if (need_env) {
        struct spdk_env_opts opts;
        spdk_env_opts_init(&opts);

        // NOTE allocate 9 cores
        opts.core_mask = "0x1ff";

        opts.name = "zstore";
        opts.mem_size = g_dpdk_mem;
        opts.hugepage_single_segments = g_dpdk_mem_single_seg;
        opts.core_mask = g_zstore.core_mask;

        if (spdk_env_init(&opts) < 0) {
            fprintf(stderr, "Unable to initialize SPDK env.\n");
            exit(-1);
        }

        rc = spdk_thread_lib_init(nullptr, 0);
        if (rc < 0) {
            fprintf(stderr, "Unable to initialize SPDK thread lib.\n");
            exit(-1);
        }
    }

    // NOTE use zns_dev_init
    // RDMA
    // zns_dev_init("192.168.100.9", "5520");
    // TCP
    zns_dev_init("12.12.12.2", "5520");

    // init devices
    mDevices = g_devices;
    std::sort(mDevices.begin(), mDevices.end(),
              [](const Device *o1, const Device *o2) -> bool {
                  // there will be no tie between two PCIe address, so no issue
                  // without equality test
                  return strcmp(o1->GetDeviceTransportAddress(),
                                o2->GetDeviceTransportAddress()) < 0;
              });

    // Adjust the capacity for user data = total capacity - footer size
    // The L2P table information at the end of the segment
    // Each block needs (LBA + timestamp + stripe ID, 20 bytes) for L2P table
    // recovery; we round the number to block size
    uint64_t zoneCapacity = mDevices[0]->GetZoneCapacity();
    uint32_t blockSize = Configuration::GetBlockSize();
    uint32_t maxFooterSize =
        round_up(zoneCapacity, (blockSize / 20)) / (blockSize / 20);
    mHeaderRegionSize = 1;
    mDataRegionSize =
        round_down(zoneCapacity - mHeaderRegionSize - maxFooterSize,
                   Configuration::GetStripeGroupSize());
    mFooterRegionSize =
        round_up(mDataRegionSize, (blockSize / 20)) / (blockSize / 20);
    printf("HeaderRegion: %u, DataRegion: %u, FooterRegion: %u\n",
           mHeaderRegionSize, mDataRegionSize, mFooterRegionSize);

    uint32_t totalNumZones = round_up(Configuration::GetStorageSpaceInBytes() /
                                          Configuration::GetBlockSize(),
                                      mDataRegionSize) /
                             mDataRegionSize;
    uint32_t numDataBlocks =
        Configuration::GetStripeDataSize() / Configuration::GetStripeUnitSize();
    uint32_t numZonesNeededPerDevice =
        round_up(totalNumZones, numDataBlocks) / numDataBlocks;
    uint32_t numZonesReservedPerDevice =
        std::max(3u, (uint32_t)(numZonesNeededPerDevice * 0.25));

    for (uint32_t i = 0; i < mDevices.size(); ++i) {
        mDevices[i]->SetDeviceId(i);
        mDevices[i]->InitZones(numZonesNeededPerDevice,
                               numZonesReservedPerDevice);
    }

    mStorageSpaceThresholdForGcInSegments = numZonesReservedPerDevice / 2;
    mAvailableStorageSpaceInSegments =
        numZonesNeededPerDevice + numZonesReservedPerDevice;
    printf("Total available segments: %u, reserved segments: %u\n",
           mAvailableStorageSpaceInSegments,
           mStorageSpaceThresholdForGcInSegments);

    // Preallocate contexts for user requests
    // Sufficient to support multiple I/O queues of NVMe-oF target
    mRequestContextPoolForUserRequests = new RequestContextPool(2048);
    mRequestContextPoolForSegments = new RequestContextPool(4096);

    mReadContextPool = new ReadContextPool(512, mRequestContextPoolForSegments);

    // Initialize address map
    mAddressMap = new PhysicalAddr[Configuration::GetStorageSpaceInBytes() /
                                   Configuration::GetBlockSize()];
    PhysicalAddr defaultAddr;
    defaultAddr.segment = nullptr;
    std::fill(mAddressMap,
              mAddressMap + Configuration::GetStorageSpaceInBytes() /
                                Configuration::GetBlockSize(),
              defaultAddr);

    // Create poll groups for the io threads and perform initialization
    for (uint32_t threadId = 0; threadId < Configuration::GetNumIoThreads();
         ++threadId) {
        mIoThread[threadId].group = spdk_nvme_poll_group_create(NULL, NULL);
    }
    for (uint32_t i = 0; i < mDevices.size(); ++i) {
        struct spdk_nvme_qpair **ioQueues = mDevices[i]->GetIoQueues();
        for (uint32_t threadId = 0; threadId < Configuration::GetNumIoThreads();
             ++threadId) {
            spdk_nvme_ctrlr_disconnect_io_qpair(ioQueues[threadId]);
            int rc = spdk_nvme_poll_group_add(mIoThread[threadId].group,
                                              ioQueues[threadId]);
            assert(rc == 0);
        }
        mDevices[i]->ConnectIoPairs();
    }

    // Preallocate segments
    mNumOpenSegments = Configuration::GetNumOpenSegments();

    mStripeWriteContextPools =
        new StripeWriteContextPool *[mNumOpenSegments + 2];
    for (uint32_t i = 0; i < mNumOpenSegments + 2; ++i) {
        // if (Configuration::GetSystemMode() == ZONEWRITE_ONLY) {
        //     mStripeWriteContextPools[i] =
        //         new StripeWriteContextPool(1,
        //         mRequestContextPoolForSegments);
        // } else {
        mStripeWriteContextPools[i] =
            new StripeWriteContextPool(64, mRequestContextPoolForSegments);
        // }
    }

    mOpenSegments.resize(mNumOpenSegments);

    initIoThread(); // broken
    initDispatchThread();
    initIndexThread();
    initCompletionThread();
    initHttpThread();

    // init Gc
    initGc();

    Configuration::PrintConfigurations();
    log_info("ZstoreController Init finish");
}

ZstoreController::~ZstoreController()
{
    Dump();

    delete mAddressMap;
    // if (!Configuration::GetEventFrameworkEnabled()) {
    for (uint32_t i = 0; i < Configuration::GetNumIoThreads(); ++i) {
        thread_send_msg(mIoThread[i].thread, quit, nullptr);
    }
    thread_send_msg(mDispatchThread, quit, nullptr);
    thread_send_msg(mHttpThread, quit, nullptr);
    thread_send_msg(mIndexThread, quit, nullptr);
    thread_send_msg(mCompletionThread, quit, nullptr);
    // }
}

void ZstoreController::initGc()
{
    mGcTask.numBuffers = 32;
    mGcTask.dataBuffer = (uint8_t *)spdk_zmalloc(
        mGcTask.numBuffers * Configuration::GetBlockSize(), 4096, NULL,
        SPDK_ENV_SOCKET_ID_ANY, SPDK_MALLOC_DMA);
    mGcTask.metaBuffer = (uint8_t *)spdk_zmalloc(
        mGcTask.numBuffers * Configuration::GetMetadataSize(), 4096, NULL,
        SPDK_ENV_SOCKET_ID_ANY, SPDK_MALLOC_DMA);
    mGcTask.contextPool = new RequestContext[mGcTask.numBuffers];
    mGcTask.stage = IDLE;
}

uint32_t ZstoreController::GcBatchUpdateIndex(
    const std::vector<uint64_t> &lbas,
    const std::vector<std::pair<PhysicalAddr, PhysicalAddr>> &pbas)
{

    uint32_t numSuccessUpdates = 0;
    assert(lbas.size() == pbas.size());
    for (int i = 0; i < lbas.size(); ++i) {
        uint64_t lba = lbas[i];
        PhysicalAddr oldPba = pbas[i].first;
        PhysicalAddr newPba = pbas[i].second;

        if (mAddressMap[lba / Configuration::GetBlockSize()].segment ==
            nullptr) {
            printf("Missing old lba %lu\n", lba);
        }
        if (mAddressMap[lba / Configuration::GetBlockSize()] == oldPba) {
            numSuccessUpdates += 1;
            UpdateIndex(lba, newPba);
        } else {
            newPba.segment->InvalidateBlock(newPba.zoneId, newPba.offset);
        }
    }
    return numSuccessUpdates;
}

void ZstoreController::UpdateIndex(uint64_t lba, PhysicalAddr pba)
{
    // Invalidate the old block
    if (lba >= Configuration::GetStorageSpaceInBytes()) {
        printf("Error\n");
        assert(0);
    }
    if (mAddressMap[lba / Configuration::GetBlockSize()].segment != nullptr) {
        PhysicalAddr oldPba = mAddressMap[lba / Configuration::GetBlockSize()];
        oldPba.segment->InvalidateBlock(oldPba.zoneId, oldPba.offset);
        mNumInvalidBlocks += 1;
    }
    assert(pba.segment != nullptr);
    mAddressMap[lba / Configuration::GetBlockSize()] = pba;
    mNumBlocks += 1;
}

void ZstoreController::Append(uint64_t zslba, uint32_t size, void *data,
                              zns_raid_request_complete cb_fn, void *cb_args)
{
    if (Configuration::GetEventFrameworkEnabled()) {
        Request *req = (Request *)calloc(1, sizeof(Request));
        req->controller = this;
        req->offset = zslba;
        req->size = size;
        req->data = data;
        req->type = 'W';
        req->cb_fn = cb_fn;
        req->cb_args = cb_args;
        event_call(Configuration::GetReceiverThreadCoreId(), executeRequest,
                   req, nullptr);
    } else {
        if (verbose)
            log_debug("Controller write");
        Execute(zslba, size, data, true, cb_fn, cb_args);
    }
}

void ZstoreController::Write(uint64_t offset, uint32_t size, void *data,
                             zns_raid_request_complete cb_fn, void *cb_args)
{
    if (Configuration::GetEventFrameworkEnabled()) {
        Request *req = (Request *)calloc(1, sizeof(Request));
        req->controller = this;
        req->offset = offset;
        req->size = size;
        req->data = data;
        req->type = 'W';
        req->cb_fn = cb_fn;
        req->cb_args = cb_args;
        event_call(Configuration::GetReceiverThreadCoreId(), executeRequest,
                   req, nullptr);
    } else {
        if (verbose)
            log_debug("Controller write");
        Execute(offset, size, data, true, cb_fn, cb_args);
    }
}

void ZstoreController::Read(uint64_t offset, uint32_t size, void *data,
                            zns_raid_request_complete cb_fn, void *cb_args)
{
    if (Configuration::GetEventFrameworkEnabled()) {
        Request *req = (Request *)calloc(1, sizeof(Request));
        req->controller = this;
        req->offset = offset;
        req->size = size;
        req->data = data;
        req->type = 'R';
        req->cb_fn = cb_fn;
        req->cb_args = cb_args;
        event_call(Configuration::GetReceiverThreadCoreId(), executeRequest,
                   req, nullptr);
    } else {
        Execute(offset, size, data, false, cb_fn, cb_args);
    }
}

void ZstoreController::ReclaimContexts()
{
    int numSuccessfulReclaims = 0;
    for (auto it = mInflightRequestContext.begin();
         it != mInflightRequestContext.end();) {
        if ((*it)->available) {
            (*it)->Clear();
            mRequestContextPoolForUserRequests->ReturnRequestContext(*it);
            it = mInflightRequestContext.erase(it);

            numSuccessfulReclaims++;
            if (numSuccessfulReclaims >= 128) {
                break;
            }
        } else {
            ++it;
        }
    }
}

void ZstoreController::Flush()
{
    bool remainWrites;
    do {
        remainWrites = false;
        for (auto it = mInflightRequestContext.begin();
             it != mInflightRequestContext.end();) {
            if ((*it)->available) {
                (*it)->Clear();
                mRequestContextPoolForUserRequests->ReturnRequestContext(*it);
                it = mInflightRequestContext.erase(it);
            } else {
                if ((*it)->req_type == 'W') {
                    remainWrites = true;
                }
                ++it;
            }
        }
    } while (remainWrites);
}

RequestContext *ZstoreController::getContextForUserRequest()
{
    RequestContext *ctx =
        mRequestContextPoolForUserRequests->GetRequestContext(false);
    while (ctx == nullptr) {
        ReclaimContexts();
        ctx = mRequestContextPoolForUserRequests->GetRequestContext(false);
        if (ctx == nullptr) {
            //      printf("NO AVAILABLE CONTEXT FOR USER.\n");
        }
    }

    mInflightRequestContext.insert(ctx);
    ctx->Clear();
    ctx->available = false;
    ctx->meta = nullptr;
    ctx->ctrl = this;
    return ctx;
}

void ZstoreController::Execute(uint64_t offset, uint32_t size, void *data,
                               bool is_write, zns_raid_request_complete cb_fn,
                               void *cb_args)
{
    RequestContext *ctx = getContextForUserRequest();
    ctx->type = USER;
    ctx->data = (uint8_t *)data;
    ctx->lba = offset;
    ctx->size = size;
    ctx->targetBytes = size;
    ctx->cb_fn = cb_fn;
    ctx->cb_args = cb_args;
    if (is_write) {
        ctx->req_type = 'W';
        // ctx->status = WRITE_REAPING;
        ctx->status = APPEND;
    } else {
        ctx->req_type = 'R';
        ctx->status = READ_PREPARE;
    }

    if (!Configuration::GetEventFrameworkEnabled()) {
        if (verbose)
            log_debug("thread send enqueue requst");
        thread_send_msg(mDispatchThread, enqueueRequest, ctx);
    } else {
        event_call(Configuration::GetDispatchThreadCoreId(), enqueueRequest2,
                   ctx, nullptr);
    }

    return;
}

void ZstoreController::EnqueueWrite(RequestContext *ctx)
{
    if (verbose)
        log_debug("controller EnqueueWrite: {}", mWriteQueue.size());
    mWriteQueue.push(ctx);
}

void ZstoreController::EnqueueReadPrepare(RequestContext *ctx)
{
    mReadPrepareQueue.push(ctx);
}

void ZstoreController::EnqueueReadReaping(RequestContext *ctx)
{
    mReadReapingQueue.push(ctx);
}

std::queue<RequestContext *> &ZstoreController::GetWriteQueue()
{
    return mWriteQueue;
}

std::queue<RequestContext *> &ZstoreController::GetReadPrepareQueue()
{
    return mReadPrepareQueue;
}

std::queue<RequestContext *> &ZstoreController::GetReadReapingQueue()
{
    return mReadReapingQueue;
}

int ZstoreController::GetWriteQueueSize() { return mWriteQueue.size(); }

int ZstoreController::GetReadPrepareQueueSize()
{
    return mReadPrepareQueue.size();
}

int ZstoreController::GetReadReapingQueueSize()
{
    return mReadReapingQueue.size();
}

bool ZstoreController::LookupIndex(uint64_t lba, PhysicalAddr *pba)
{

    if (mAddressMap[lba / Configuration::GetBlockSize()].segment != nullptr) {
        *pba = mAddressMap[lba / Configuration::GetBlockSize()];
        return true;
    } else {
        pba->segment = nullptr;
        return false;
    }
}

void ZstoreController::WriteInDispatchThread(RequestContext *ctx)
{
    if (mAvailableStorageSpaceInSegments <= 1) {
        return;
    }

    static uint32_t tick = 0;
    static uint32_t lastTick = 0;
    static uint32_t last = 0;
    static uint32_t stucks = 0;

    uint32_t blockSize = Configuration::GetBlockSize();
    uint32_t curOffset = ctx->curOffset;
    uint32_t size = ctx->size;
    uint32_t numBlocks = size / blockSize;

    if (curOffset == 0 && ctx->timestamp == ~0ull) {
        ctx->timestamp = ++mGlobalTimestamp;
        ctx->pbaArray.resize(numBlocks);
    }

    uint32_t pos = ctx->curOffset;
    for (; pos < numBlocks; pos += 1) {
        uint32_t openGroupId = 0;
        bool success = false;

        for (uint32_t trys = 0; trys < mNumOpenSegments; trys += 1) {
            log_debug("before crash");
            success = mOpenSegments[openGroupId]->Append(ctx, pos);
            if (mOpenSegments[openGroupId]->IsFull()) {
                mSegmentsToSeal.emplace_back(mOpenSegments[openGroupId]);
                mOpenSegments[openGroupId] = nullptr;
                createSegmentIfNeeded(&mOpenSegments[openGroupId], openGroupId);
            }
            if (success) {
                break;
            }
            openGroupId = (openGroupId + 1) % mNumOpenSegments;
        }

        if (!success) {
            break;
        }
    }
    ctx->curOffset = pos;
}

void ZstoreController::ReadInDispatchThread(RequestContext *ctx)
{
    log_info("Read");
    uint64_t slba = ctx->lba;
    int size = ctx->size;
    void *data = ctx->data;
    uint32_t numBlocks = size / Configuration::GetBlockSize();

    if (ctx->status == READ_PREPARE) {
        log_info("Read dispatch thread: read prepare ");
        if (mGcTask.stage != INDEX_UPDATE_COMPLETE) {
            // For any reads that may read the input segment,
            // track its progress such that the GC will
            // not delete input segment before they finishes
            mReadsInCurrentGcEpoch.insert(ctx);
        }

        ctx->status = READ_INDEX_QUERYING;
        ctx->pbaArray.resize(numBlocks);
        if (!Configuration::GetEventFrameworkEnabled()) {
            QueryPbaArgs *args =
                (QueryPbaArgs *)calloc(1, sizeof(QueryPbaArgs));
            args->ctrl = this;
            args->ctx = ctx;
            log_info("thread send ");
            thread_send_msg(mIndexThread, queryPba, args);
        } else {
            log_info("event send ");
            event_call(Configuration::GetIndexThreadCoreId(), queryPba2, this,
                       ctx);
        }
    } else if (ctx->status == READ_REAPING) {
        log_info("Read dispatch thread: read reaping");
        uint32_t i = ctx->curOffset;
        for (; i < numBlocks; ++i) {
            Segment *segment = ctx->pbaArray[i].segment;
            if (segment == nullptr) {
                uint8_t *block =
                    (uint8_t *)data + i * Configuration::GetBlockSize();
                memset(block, 0, Configuration::GetBlockSize());
                ctx->successBytes += Configuration::GetBlockSize();
                if (ctx->successBytes == ctx->targetBytes) {
                    ctx->Queue();
                }
                log_info("reaping if");
            } else {
                if (!segment->Read(ctx, i, ctx->pbaArray[i])) {
                    break;
                }
                log_info("reaping else ");
            }
        }
        ctx->curOffset = i;
    }
}

bool ZstoreController::scheduleGc()
{
    if (mAvailableStorageSpaceInSegments >
        mStorageSpaceThresholdForGcInSegments) {
        return false;
    }

    // Use Greedy algorithm to pick segments
    std::vector<Segment *> groups;
    for (Segment *segment : mSealedSegments) {
        groups.emplace_back(segment);
    }
    if (groups.size() == 0) {
        return false;
    }
    std::sort(groups.begin(), groups.end(),
              [](const Segment *lhs, const Segment *rhs) {
                  double score1 =
                      (double)lhs->GetNumInvalidBlocks() / lhs->GetNumBlocks();
                  double score2 =
                      (double)rhs->GetNumInvalidBlocks() / rhs->GetNumBlocks();
                  return score1 > score2;
              });

    mGcTask.inputSegment = groups[0];
    printf("Select: %p, Score: %f, Invalid: %u, Valid: %u\n",
           mGcTask.inputSegment,
           (double)groups[0]->GetNumInvalidBlocks() / groups[0]->GetNumBlocks(),
           groups[0]->GetNumInvalidBlocks(), groups[0]->GetNumBlocks());

    mGcTask.maxZoneId = mDevices.size();

    printf("Schedule GC. Available storage: %u %u\n",
           mAvailableStorageSpaceInSegments,
           mStorageSpaceThresholdForGcInSegments);

    return true;
}

// bool ZstoreController::ProceedGc()
// {
//     bool hasProgress = false;
//     if (!Configuration::GetEnableGc()) {
//         return hasProgress;
//     }
//
//     if (mGcTask.stage == IDLE) { // IDLE
//         if (scheduleGc()) {
//             hasProgress = true;
//             mGcTask.stage = INIT;
//         }
//     }
//
//     if (mGcTask.stage == INIT) {
//         initializeGcTask();
//     }
//
//     if (mGcTask.stage == REWRITING) {
//         hasProgress |= progressGcWriter();
//         hasProgress |= progressGcReader();
//
//         if (mGcTask.curZoneId == mGcTask.maxZoneId) {
//             if (mGcTask.numWriteSubmitted == mGcTask.numReads &&
//                 mGcTask.numWriteFinish == mGcTask.numWriteSubmitted) {
//                 assert(mGcTask.mappings.size() == mGcTask.numWriteFinish);
//                 mGcTask.stage = REWRITE_COMPLETE;
//             }
//         }
//     }
//
//     if (mGcTask.stage == REWRITE_COMPLETE) {
//         hasProgress = true;
//         mGcTask.stage = INDEX_UPDATING;
//
//         // if (!Configuration::GetEventFrameworkEnabled()) {
//         thread_send_msg(mIndexThread, progressGcIndexUpdate, this);
//         // } else {
//         //     event_call(Configuration::GetIndexThreadCoreId(),
//         //                progressGcIndexUpdate2, this, nullptr);
//         // }
//     }
//
//     if (mGcTask.stage == INDEX_UPDATING_BATCH) {
//         if (mGcTask.mappings.size() != 0) {
//             hasProgress = true;
//             mGcTask.stage = INDEX_UPDATING;
//             // if (!Configuration::GetEventFrameworkEnabled()) {
//             thread_send_msg(mIndexThread, progressGcIndexUpdate, this);
//             // } else {
//             //     event_call(Configuration::GetIndexThreadCoreId(),
//             //                progressGcIndexUpdate2, this, nullptr);
//             // }
//         } else { // Finish updating all mappings
//             hasProgress = true;
//             mGcTask.stage = INDEX_UPDATE_COMPLETE;
//         }
//     }
//
//     if (mGcTask.stage == INDEX_UPDATE_COMPLETE) {
//         if (mReadsInCurrentGcEpoch.empty()) {
//             hasProgress = true;
//             mGcTask.inputSegment->Reset(nullptr);
//             mGcTask.stage = RESETTING_INPUT_SEGMENT;
//         }
//     }
//
//     if (mGcTask.stage == RESETTING_INPUT_SEGMENT) {
//         if (mGcTask.inputSegment->IsResetDone()) {
//             auto zones = mGcTask.inputSegment->GetZones();
//             for (uint32_t i = 0; i < zones.size(); ++i) {
//                 mDevices[i]->ReturnZone(zones[i]);
//             }
//             mSealedSegments.erase(mGcTask.inputSegment);
//             delete mGcTask.inputSegment;
//             mAvailableStorageSpaceInSegments += 1;
//             mGcTask.stage = IDLE;
//         }
//     }
//
//     return hasProgress;
// }

void ZstoreController::Drain()
{
    if (verbose)
        printf("Perform draining on the system.\n");
    DrainArgs args;
    args.ctrl = this;
    args.success = false;
    while (!args.success) {
        args.ready = false;
        thread_send_msg(mDispatchThread, tryDrainController, &args);
        busyWait(&args.ready);
    }
}

std::queue<RequestContext *> &ZstoreController::GetEventsToDispatch()
{
    return mEventsToDispatch;
}

int ZstoreController::GetEventsToDispatchSize()
{
    return mEventsToDispatch.size();
}

void ZstoreController::EnqueueEvent(RequestContext *ctx)
{
    mEventsToDispatch.push(ctx);
}

int ZstoreController::GetNumInflightRequests()
{
    return mInflightRequestContext.size();
}

bool ZstoreController::ExistsGc() { return mGcTask.stage != IDLE; }

void ZstoreController::createSegmentIfNeeded(Segment **segment, uint32_t spId)
{
    if (*segment != nullptr)
        return;
    // Check there are available zones
    if (mAvailableStorageSpaceInSegments == 0) {
        assert(0);
        printf("No available storage; this should never happen!\n");
        return;
    }

    mAvailableStorageSpaceInSegments -= 1;
    Segment *seg = new Segment(this, mNextAssignedSegmentId++,
                               mRequestContextPoolForSegments, mReadContextPool,
                               mStripeWriteContextPools[spId]);
    for (uint32_t i = 0; i < mDevices.size(); ++i) {
        Zone *zone = mDevices[i]->OpenZone();
        if (zone == nullptr) {
            printf(
                "No available zone in device %d, storage space is exhuasted!\n",
                i);
        }
        seg->AddZone(zone);
        log_debug("Add zone pos {}, size {}, slba {}", zone->GetPos(),
                  zone->GetSize(), zone->GetSlba());
    }

    if (spId == mNumOpenSegments + 1) {
        printf("Create spare segment %p\n", seg);
    } else {
        printf("Create normal segment %p\n", seg);
    }
    seg->FinalizeCreation();
    *segment = seg;
}

std::queue<RequestContext *> &ZstoreController::GetRequestQueue()
{
    return mRequestQueue;
}

int ZstoreController::GetRequestQueueSize() { return mRequestQueue.size(); }

std::mutex &ZstoreController::GetRequestQueueMutex()
{
    return mRequestQueueMutex;
}

struct spdk_thread *ZstoreController::GetIoThread(int id)
{
    return mIoThread[id].thread;
}

struct spdk_thread *ZstoreController::GetDispatchThread()
{
    return mDispatchThread;
}

struct spdk_thread *ZstoreController::GetHttpThread() { return mHttpThread; }

struct spdk_thread *ZstoreController::GetIndexThread() { return mIndexThread; }

struct spdk_thread *ZstoreController::GetCompletionThread()
{
    return mCompletionThread;
}

void ZstoreController::initializeGcTask()
{
    mGcTask.curZoneId = 0;
    mGcTask.nextOffset = 0;
    mGcTask.stage = REWRITING;

    mGcTask.writerPos = 0;
    mGcTask.readerPos = 0;

    mGcTask.numWriteSubmitted = 0;
    mGcTask.numWriteFinish = 0;
    mGcTask.numReads = 0;

    mGcTask.mappings.clear();

    // Initialize the status of the context pool
    for (uint32_t i = 0; i < mGcTask.numBuffers; ++i) {
        mGcTask.contextPool[i].Clear();
        mGcTask.contextPool[i].available = true;
        mGcTask.contextPool[i].ctrl = this;
        mGcTask.contextPool[i].pbaArray.resize(1);
        mGcTask.contextPool[i].gcTask = &mGcTask;
        mGcTask.contextPool[i].type = GC;
        mGcTask.contextPool[i].lba = ~0ull;
        mGcTask.contextPool[i].data = (uint8_t *)mGcTask.dataBuffer +
                                      i * Configuration::GetStripeUnitSize();
        mGcTask.contextPool[i].meta = (uint8_t *)mGcTask.metaBuffer +
                                      i * Configuration::GetMetadataSize();
        mGcTask.contextPool[i].targetBytes = Configuration::GetBlockSize();
        mGcTask.contextPool[i].status = WRITE_COMPLETE;
    }
}

// bool ZstoreController::progressGcReader()
// {
//     bool hasProgress = false;
//     // Find contexts that are available, schedule read for valid blocks
//     RequestContext *nextReader = &mGcTask.contextPool[mGcTask.readerPos];
//     while (nextReader->available && (nextReader->status == WRITE_COMPLETE)) {
//         if (nextReader->lba != ~0ull) {
//             // The sign of valid lba means a successful rewrite a valid block
//             // So we update the information here
//             mGcTask.numWriteFinish += 1;
//             mGcTask.mappings[nextReader->lba].second =
//             nextReader->pbaArray[0];
//         }
//
//         nextReader->available = false;
//         nextReader->lba = 0;
//
//         bool valid = false;
//         bool success = true;
//         if (mGcTask.curZoneId != mGcTask.maxZoneId) {
//             nextReader->req_type = 'R';
//             nextReader->status = READ_REAPING;
//             nextReader->successBytes = 0;
//
//             do {
//                 nextReader->segment = mGcTask.inputSegment;
//                 nextReader->zoneId = mGcTask.curZoneId;
//                 nextReader->offset = mGcTask.nextOffset;
//
//                 success = mGcTask.inputSegment->ReadValid(
//                     nextReader, 0, nextReader->GetPba(), &valid);
//                 if (!success)
//                     break;
//
//                 mGcTask.nextOffset += 1;
//                 if (mGcTask.nextOffset == mHeaderRegionSize +
//                 mDataRegionSize) {
//                     mGcTask.nextOffset = mHeaderRegionSize;
//                     mGcTask.curZoneId += 1;
//                 }
//             } while (!valid && mGcTask.curZoneId != mGcTask.maxZoneId);
//             if (valid) {
//                 mGcTask.numReads += 1;
//             }
//         }
//         if (!success) {
//             // will retry later
//             nextReader->status = WRITE_COMPLETE;
//             nextReader->available = true;
//             nextReader->lba = ~0ull;
//             break;
//         }
//         hasProgress = true;
//         mGcTask.readerPos = (mGcTask.readerPos + 1) % mGcTask.numBuffers;
//         nextReader = &mGcTask.contextPool[mGcTask.readerPos];
//     }
//
//     return hasProgress;
// }

// bool ZstoreController::progressGcWriter()
// {
//     bool hasProgress = false;
//     // Process blocks that are read and valid, and rewrite them
//     RequestContext *nextWriter = &mGcTask.contextPool[mGcTask.writerPos];
//     while (nextWriter->available && nextWriter->status == READ_COMPLETE) {
//         uint64_t lba = ((BlockMetadata *)nextWriter->meta)->fields.coded.lba;
//         if (lba == ~0ull) {
//             fprintf(stderr,
//                     "GC write does not expect block with invalid lba!\n");
//             exit(-1);
//         }
//         assert(lba != ~0ull);
//
//         PhysicalAddr oldPba = nextWriter->GetPba();
//         RequestContext backup; // Backup prevents from context lost due to
//         retry backup.CopyFrom(*nextWriter); nextWriter->lba = lba;
//         nextWriter->req_type = 'W';
//         nextWriter->status = WRITE_REAPING;
//         nextWriter->successBytes = 0;
//         nextWriter->available = false;
//         nextWriter->timestamp =
//             ((BlockMetadata *)nextWriter->meta)->fields.coded.timestamp;
//
//         bool success = false;
//         for (uint32_t i = 0; i < mNumOpenSegments; i += 1) {
//             success = mOpenSegments[i]->Append(nextWriter, 0);
//             if (mOpenSegments[i]->IsFull()) {
//                 mSegmentsToSeal.emplace_back(mOpenSegments[i]);
//                 mOpenSegments[i] = nullptr;
//                 createSegmentIfNeeded(&mOpenSegments[i], i);
//             }
//             if (success) {
//                 break;
//             }
//         }
//         if (!success) {
//             nextWriter->CopyFrom(backup);
//             break;
//         }
//
//         mGcTask.mappings[lba] = std::make_pair(oldPba, PhysicalAddr());
//         mGcTask.numWriteSubmitted += 1;
//
//         mGcTask.writerPos = (mGcTask.writerPos + 1) % mGcTask.numBuffers;
//         nextWriter = &mGcTask.contextPool[mGcTask.writerPos];
//
//         hasProgress = true;
//     }
//     return hasProgress;
// }

// GcTask *ZstoreController::GetGcTask() { return &mGcTask; }

uint32_t ZstoreController::GetHeaderRegionSize() { return mHeaderRegionSize; }

uint32_t ZstoreController::GetDataRegionSize() { return mDataRegionSize; }

uint32_t ZstoreController::GetFooterRegionSize() { return mFooterRegionSize; }

// void ZstoreController::RemoveRequestFromGcEpochIfNecessary(RequestContext
// *ctx)
// {
//     if (mReadsInCurrentGcEpoch.empty()) {
//         return;
//     }
//
//     if (mReadsInCurrentGcEpoch.find(ctx) != mReadsInCurrentGcEpoch.end()) {
//         mReadsInCurrentGcEpoch.erase(ctx);
//     }
// }

void ZstoreController::Dump()
{
    // Dump address map
    //  for (uint32_t i = 0; i < Configuration::GetStorageSpaceInBytes() /
    //  Configuration::GetBlockSize(); ++i) {
    //    if (mAddressMap[i].segment != nullptr) {
    //      printf("%llu %u %u %u\n",
    //          i * Configuration::GetBlockSize() * 1ull,
    //          mAddressMap[i].segment->GetSegmentId(),
    //          mAddressMap[i].zoneId,
    //          mAddressMap[i].offset);
    //    }
    //  }

    std::map<uint64_t, Segment *> orderedSegments;
    // Dump the information of each segment
    for (auto segment : mOpenSegments) {
        orderedSegments[segment->GetSegmentId()] = segment;
    }
    for (auto segment : mSegmentsToSeal) {
        orderedSegments[segment->GetSegmentId()] = segment;
    }
    for (auto segment : mSealedSegments) {
        orderedSegments[segment->GetSegmentId()] = segment;
    }
    for (auto pr : orderedSegments) {
        pr.second->Dump();
    }
}

// Add an object to the store
//
// FIXME this version does not have object header, so the data is just a 4kb
// block
void ZstoreController::putObject(std::string key, void *data)
{
    auto it = mZstoreMap.find(key);
    if (it != mZstoreMap.end()) {
        log_info("PutObject is updating an existing object: key {}", key);
        MapEntry *entry = &(it->second);

        // const auto first = std::get<0>(tuple);
        // const auto second = std::get<1>(tuple);
        // const auto third = std::get<2>(tuple);

        // log_info("\tfirst: device {}, lba {}", std::get<0>(first),
        //          std::get<1>(first));
        // log_info("\tsecond: device {}, lba {}", std::get<0>(second),
        //          std::get<1>(second));
        // log_info("\tthird: device {}, lba {}", std::get<0>(third),
        //          std::get<1>(third));

        updateMapEntry(*entry, dummy_device, 0);
        // updateMapEntry(second, dummy_device, 0);
        // updateMapEntry(third, dummy_device, 0);

        // return first;
        // return &(it->second);
    } else {
        log_info("PutObject is creating a new object: key {}", key);

        MapEntry first;
        // MapEntry second;
        // MapEntry third;
        // std::pair<std::string, int32_t> first;
        // std::pair<std::string, int32_t> second;
        // std::pair<std::string, int32_t> third;

        updateMapEntry(first, dummy_device, 0);
        // updateMapEntry(second, dummy_device, 0);
        // updateMapEntry(third, dummy_device, 0);

        // mZstoreMap[key] = std::make_tuple(first, second, third);
        mZstoreMap[key] = first;
    }
    uint64_t zslba = g_current_zone * 0x8000;
    log_info("PutObject appending key {}", key);
    ZstoreController::Append(zslba, 4096, &data, nullptr, nullptr);
}

// Retrieve an object from the store by ID
// Object *ZstoreController::getObject(std::string key)
int ZstoreController::getObject(std::string key, uint8_t *readValidateBuffer)
{
    auto it = mZstoreMap.find(key);
    if (it != mZstoreMap.end()) {
        log_info("GetObject found object: key {}", key);
        // const std::tuple<MapEntry, MapEntry, MapEntry> tuple = &(it->second);
        MapEntry *entry = &(it->second);

        // const auto first = std::get<0>(tuple);
        // const auto second = std::get<1>(tuple);
        // const auto third = std::get<2>(tuple);

        // log_info("\tfirst: device {}, lba {}", std::get<0>(first),
        //          std::get<1>(first));
        // log_info("\tsecond: device {}, lba {}", std::get<0>(second),
        //          std::get<1>(second));
        // log_info("\tthird: device {}, lba {}", std::get<0>(third),
        //          std::get<1>(third));

        log_info("FIXME: skipping to use the default device ");
        ZstoreController::Read(entry->second, 4096, &readValidateBuffer,
                               nullptr, nullptr);

        return 0;
        // return &(it->second);
    }
    log_info("GetObject cannot find object: key {}", key);
    return -1; // Object not found
}

// Delete an object from the store by ID
bool ZstoreController::deleteObject(std::string key)
{
    log_warn("delete object is unimplemented!!!");
    return 0;
    // return store.erase(id) > 0;
}
