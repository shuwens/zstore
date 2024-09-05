#include "include/zstore_controller.h"
#include "common.cc"
#include "device.cc"
#include "include/common.h"
#include "include/device.h"
// #include "include/segment.h"
#include "include/utils.hpp"
#include "messages_and_functions.cc"
// #include "segment.cc"
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

void ZstoreController::restart()
{
    uint64_t zoneCapacity = mDevices[0]->GetZoneCapacity();
    std::pair<uint64_t, PhysicalAddr> *indexMap =
        new std::pair<uint64_t,
                      PhysicalAddr>[Configuration::GetStorageSpaceInBytes() /
                                    Configuration::GetBlockSize()];
    std::pair<uint64_t, PhysicalAddr> defaultAddr;
    defaultAddr.first = 0;
    // defaultAddr.second.segment = nullptr;
    std::fill(indexMap,
              indexMap + Configuration::GetStorageSpaceInBytes() /
                             Configuration::GetBlockSize(),
              defaultAddr);

    struct timeval s, e;
    gettimeofday(&s, NULL);
    // Mount from an existing new array
    // Reconstruct existing segments
    uint32_t zoneSize = 2 * 1024 * 1024 / 4;
    // Valid (full and open) zones and their headers
    // std::map<uint64_t, uint8_t *> zonesAndHeaders[mN];
    // Segment ID to (wp, SegmentMetadata)
    // std::map<uint32_t, std::vector<std::pair<uint64_t, uint8_t *>>>
    //     potentialSegments;

    log_info("Reconstructing: storage space {}\n",
             Configuration::GetStorageSpaceInBytes());

    int mN = mDevices.size();
    for (uint32_t i = 0; i < mN; ++i) {
        // Zone *zone = mDevices[i]->OpenZone();
        Zone *zone = mDevices[i]->OpenZoneBySlba(g_current_zone * 0x8000);
        if (zone == nullptr) {
            printf(
                "No available zone in device %d, storage space is exhuasted!\n",
                i);
            assert(0);
        }
        AddZone(zone);
        uint32_t zoneId = zone->GetSlba() / zone->GetSize();
        log_info(
            "Open and add zone {}. Slba {} == {}, Zone id {}, zone size {}",
            g_current_zone, g_current_zone * 0x8000, zone->GetSlba(), zoneId,
            zone->GetSize());
    }

    gettimeofday(&e, NULL);
    double elapsed =
        e.tv_sec - s.tv_sec + e.tv_usec / 1000000. - s.tv_usec / 1000000.;
    printf("Restart time: %.6f\n", elapsed);
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
    // uint32_t maxFooterSize =
    //     round_up(zoneCapacity, (blockSize / 20)) / (blockSize / 20);
    // mHeaderRegionSize = 1;
    // mDataRegionSize =
    //     round_down(zoneCapacity - mHeaderRegionSize - maxFooterSize,
    //                Configuration::GetStripeGroupSize());
    // mFooterRegionSize =
    //     round_up(mDataRegionSize, (blockSize / 20)) / (blockSize / 20);
    // printf("HeaderRegion: %u, DataRegion: %u, FooterRegion: %u\n",
    //        mHeaderRegionSize, mDataRegionSize, mFooterRegionSize);

    // uint32_t totalNumZones = round_up(Configuration::GetStorageSpaceInBytes()
    // /
    //                                       Configuration::GetBlockSize(),
    //                                   mDataRegionSize) /
    //                          mDataRegionSize;
    // uint32_t numDataBlocks =
    //     Configuration::GetStripeDataSize() /
    //     Configuration::GetStripeUnitSize();
    // uint32_t numZonesNeededPerDevice =
    //     round_up(totalNumZones, numDataBlocks) / numDataBlocks;
    // uint32_t numZonesReservedPerDevice =
    //     std::max(3u, (uint32_t)(numZonesNeededPerDevice * 0.25));

    for (uint32_t i = 0; i < mDevices.size(); ++i) {
        mDevices[i]->SetDeviceId(i);
        mDevices[i]->InitZones(2, 2);
    }

    // Preallocate contexts for user requests
    // Sufficient to support multiple I/O queues of NVMe-oF target
    mRequestContextPoolForUserRequests = new RequestContextPool(2048);
    mRequestContextPoolForZstore = new RequestContextPool(4096);

    mReadContextPool = new ReadContextPool(512, mRequestContextPoolForZstore);

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

    restart();
    log_debug("mZone sizes {}", mZones.size());

    log_debug("ZstoreController launching threads");

    // zstore controller meta
    mMeta.numZones = 0;

    initIoThread(); // broken
    initDispatchThread();
    initIndexThread();
    initCompletionThread();
    initHttpThread();

    // init Gc
    // initGc();

    Configuration::PrintConfigurations();
    log_info("ZstoreController Init finish");
}

ZstoreController::~ZstoreController()
{
    // Dump();

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

// void ZstoreController::initGc()
// {
//     mGcTask.numBuffers = 32;
//     mGcTask.dataBuffer = (uint8_t *)spdk_zmalloc(
//         mGcTask.numBuffers * Configuration::GetBlockSize(), 4096, NULL,
//         SPDK_ENV_SOCKET_ID_ANY, SPDK_MALLOC_DMA);
//     mGcTask.metaBuffer = (uint8_t *)spdk_zmalloc(
//         mGcTask.numBuffers * Configuration::GetMetadataSize(), 4096, NULL,
//         SPDK_ENV_SOCKET_ID_ANY, SPDK_MALLOC_DMA);
//     mGcTask.contextPool = new RequestContext[mGcTask.numBuffers];
//     mGcTask.stage = IDLE;
// }

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

// bool ZstoreController::LookupIndex(uint64_t lba, PhysicalAddr *pba)
// {
//
//     if (mAddressMap[lba / Configuration::GetBlockSize()].segment != nullptr)
//     {
//         *pba = mAddressMap[lba / Configuration::GetBlockSize()];
//         return true;
//     } else {
//         pba->segment = nullptr;
//         return false;
//     }
// }

void ZstoreController::WriteInDispatchThread(RequestContext *ctx)
{
    // // if (mAvailableStorageSpaceInZstoreControllers <= 1) {
    // //     return;
    // // }
    //
    // static uint32_t tick = 0;
    // static uint32_t lastTick = 0;
    // static uint32_t last = 0;
    // static uint32_t stucks = 0;
    //
    // uint32_t blockSize = Configuration::GetBlockSize();
    // uint32_t curOffset = ctx->curOffset;
    // uint32_t size = ctx->size;
    // uint32_t numBlocks = size / blockSize;
    //
    // if (curOffset == 0 && ctx->timestamp == ~0ull) {
    //     ctx->timestamp = ++mGlobalTimestamp;
    //     ctx->pbaArray.resize(numBlocks);
    // }
    //
    // uint32_t pos = ctx->curOffset;
    // for (; pos < numBlocks; pos += 1) {
    //     uint32_t openGroupId = 0;
    //     bool success = false;
    //
    //     for (uint32_t trys = 0; trys < mNumOpenZstoreControllers; trys += 1)
    //     {
    //         log_debug("before crash");
    //         success = mOpenZstoreControllers[openGroupId]->Append(ctx, pos);
    //         if (mOpenZstoreControllers[openGroupId]->IsFull()) {
    //             mZstoreControllersToSeal.emplace_back(
    //                 mOpenZstoreControllers[openGroupId]);
    //             mOpenZstoreControllers[openGroupId] = nullptr;
    //             //
    //             createZstoreControllerIfNeeded(&mOpenZstoreControllers[openGroupId],
    //             // openGroupId);
    //         }
    //         if (success) {
    //             break;
    //         }
    //         openGroupId = (openGroupId + 1) % mNumOpenZstoreControllers;
    //     }
    //
    //     if (!success) {
    //         break;
    //     }
    // }
    // ctx->curOffset = pos;
}

void ZstoreController::ReadInDispatchThread(RequestContext *ctx)
{
    log_info("Read: pba arrary {}", ctx->pbaArray.size());
    uint64_t slba = ctx->lba;
    int size = ctx->size;
    void *data = ctx->data;
    uint32_t numBlocks = size / Configuration::GetBlockSize();
    log_info("Read: pba arrary {}", ctx->pbaArray.size());

    PhysicalAddr pba;
    pba.zoneId = 0;
    pba.offset = 0;
    log_info("Read: pba arrary {}", ctx->pbaArray.size());

    // Read(ctx, 0, ctx->pbaArray[0]);
    Read(ctx, 0, pba);

    // if (ctx->status == READ_PREPARE) {
    //     log_info("Read dispatch thread: read prepare ");
    //     // if (mGcTask.stage != INDEX_UPDATE_COMPLETE) {
    //     //     // For any reads that may read the input segment,
    //     //     // track its progress such that the GC will
    //     //     // not delete input segment before they finishes
    //     //     mReadsInCurrentGcEpoch.insert(ctx);
    //     // }
    //
    //     ctx->status = READ_INDEX_QUERYING;
    //     ctx->pbaArray.resize(numBlocks);
    //     // if (!Configuration::GetEventFrameworkEnabled()) {
    //     QueryPbaArgs *args = (QueryPbaArgs *)calloc(1, sizeof(QueryPbaArgs));
    //     args->ctrl = this;
    //     args->ctx = ctx;
    //     log_info("thread send ");
    //     thread_send_msg(mIndexThread, queryPba, args);
    //     // } else {
    //     //     log_info("event send ");
    //     //     event_call(Configuration::GetIndexThreadCoreId(), queryPba2,
    //     //     this,
    //     //                ctx);
    //     // }
    // } else if (ctx->status == READ_REAPING) {
    //     log_info("Read dispatch thread: read reaping");
    //     uint32_t i = ctx->curOffset;
    //     for (; i < numBlocks; ++i) {
    //         ZstoreController *segment = ctx->pbaArray[i].segment;
    //         // if (segment == nullptr) {
    //         uint8_t *block =
    //             (uint8_t *)data + i * Configuration::GetBlockSize();
    //         memset(block, 0, Configuration::GetBlockSize());
    //         ctx->successBytes += Configuration::GetBlockSize();
    //         if (ctx->successBytes == ctx->targetBytes) {
    //             ctx->Queue();
    //         }
    //         log_info("reaping if");
    //         // } else {
    //         //     if (!segment->Read(ctx, i, ctx->pbaArray[i])) {
    //         //         break;
    //         //     }
    //         //     log_info("reaping else ");
    //         // }
    //     }
    //     ctx->curOffset = i;
    // }
}

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

// bool ZstoreController::ExistsGc() { return mGcTask.stage != IDLE; }

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

// GcTask *ZstoreController::GetGcTask() { return &mGcTask; }

uint32_t ZstoreController::GetHeaderRegionSize() { return mHeaderRegionSize; }

uint32_t ZstoreController::GetDataRegionSize() { return mDataRegionSize; }

uint32_t ZstoreController::GetFooterRegionSize() { return mFooterRegionSize; }

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

void ZstoreController::AddZone(Zone *zone)
{
    mMeta.zones[mZones.size()] = zone->GetSlba();
    mMeta.numZones += 1;
    mZones.emplace_back(zone);
}

const std::vector<Zone *> &ZstoreController::GetZones() { return mZones; }

void ZstoreController::PrintStats()
{
    // printf("Zone group position: %d, capacity: %d, num invalid blocks: %d\n",
    //        mPos, mDataRegionSize, mNumInvalidBlocks);
    for (auto zone : mZones) {
        zone->PrintStats();
    }
}

bool ZstoreController::Append(RequestContext *ctx, uint32_t offset)
{
    log_debug("hit segment append");

    // if (mPosInStripe == 0) {
    //     if (!findStripe()) {
    //         return false;
    //     }
    //     mLastStripeCreationTimestamp = GetTimestampInUs();
    // }

    // SystemMode mode = Configuration::GetSystemMode();
    uint32_t blockSize = Configuration::GetBlockSize();

    uint64_t lba = ~0ull;
    uint8_t *blkdata = nullptr;
    // uint32_t whichBlock = mPosInStripe / blockSize;
    uint32_t whichBlock = 0;

    if (ctx) {
        lba = ctx->lba + offset * blockSize;
        blkdata = (uint8_t *)(ctx->data) + offset * blockSize;
    }

    // uint32_t zoneId = Configuration::CalculateDiskId(
    //     mPos, whichBlock, mZstoreControllerMeta.numZones);
    uint32_t zoneId = g_current_zone;
    // Issue data block
    {
        RequestContext *slot =
            mRequestContextPoolForZstore->GetRequestContext(true);
        // mCurStripe->ioContext[whichBlock] = slot;
        slot->lba = lba;
        slot->timestamp = ctx ? ctx->timestamp : ~0ull;

        slot->data = slot->dataBuffer;
        if (blkdata) {
            memcpy(slot->data, blkdata, blockSize);
        } else {
            memset(slot->data, 0, blockSize);
        }
        slot->meta = slot->metadataBuffer;

        // slot->associatedStripe = mCurStripe;
        slot->associatedRequest = ctx;

        slot->zoneId = zoneId;
        // slot->stripeId =
        //     (mPos - mHeaderRegionSize) % Configuration::GetStripeGroupSize();
        slot->ctrl = this;

        slot->type = STRIPE_UNIT;
        slot->status = WRITE_REAPING;
        slot->targetBytes = blockSize;

        // Initialize block (flash page) metadata
        BlockMetadata *blkMeta = (BlockMetadata *)slot->meta;
        blkMeta->fields.coded.lba = slot->lba;
        blkMeta->fields.coded.timestamp = slot->timestamp;
        blkMeta->fields.replicated.stripeId = slot->stripeId;

        // if (mode == ZONEWRITE_ONLY ||
        //     (mode == ZAPRAID &&
        //      mPos % Configuration::GetStripeGroupSize() == 0)) {
        //     slot->append = false;
        // } else {
        slot->append = true;
        // }

        log_debug("1");
        mZones[zoneId]->Write(0, blockSize, (void *)slot);
    }

    // mPosInStripe += blockSize;
    // if (mPosInStripe == mZstoreControllerMeta.stripeDataSize) {
    //     // issue parity block
    //     GenerateParityBlockArgs *args = (GenerateParityBlockArgs *)calloc(
    //         1, sizeof(GenerateParityBlockArgs));
    //     args->segment = this;
    //     args->stripe = mCurStripe;
    //     args->zonePos = mPos;
    //
    //     log_debug("2");
    //     // if (!Configuration::GetEventFrameworkEnabled()) {
    //     thread_send_msg(mZstoreController->GetHttpThread(),
    //     generateParityBlock,
    //                     args);
    //     // } else {
    //     //     event_call(Configuration::GetEcThreadCoreId(),
    //     //     generateParityBlock2,
    //     //                args, nullptr);
    //     // }
    //
    //     mPosInStripe = 0;
    //     mPos += 1;
    //
    //     // if (mode == ZAPRAID) {
    //     //     if (mPos % Configuration::GetStripeGroupSize() == 0) {
    //     //         // writing the stripe metadata at the end of each group
    //     //         mZstoreControllerStatus =
    //     //         SEGMENT_CONCLUDING_APPENDS_IN_GROUP;
    //     //     } else if (mPos % Configuration::GetStripeGroupSize() == 1) {
    //     //         mZstoreControllerStatus =
    //     SEGMENT_CONCLUDING_WRITES_IN_GROUP;
    //     //     }
    //     // }
    //
    //     if (mPos == mHeaderRegionSize + mDataRegionSize) {
    //         // writing the P2L table at the end of the segment
    //         mZstoreControllerStatus = SEGMENT_PREPARE_FOOTER;
    //     }
    // }

    return true;
}

bool ZstoreController::Read(RequestContext *ctx, uint32_t pos,
                            PhysicalAddr phyAddr)
{
    log_info("Read");
    ReadContext *readContext = mReadContextPool->GetContext();
    if (readContext == nullptr) {
        log_info("Try to get ReadContext but NULL ");
        return false;
    }

    log_info("Read");
    uint32_t blockSize = Configuration::GetBlockSize();
    RequestContext *slot =
        mRequestContextPoolForZstore->GetRequestContext(true);
    slot->associatedRead = readContext;
    slot->available = false;
    slot->associatedRequest = ctx;
    slot->lba = ctx->lba + pos * blockSize;
    slot->targetBytes = blockSize;
    slot->zoneId = phyAddr.zoneId;
    slot->offset = phyAddr.offset;
    slot->ctrl = this;
    slot->status = READ_REAPING;
    slot->type = STRIPE_UNIT;
    slot->data = (uint8_t *)ctx->data + pos * blockSize;
    slot->meta = (uint8_t *)(slot->metadataBuffer);

    log_info("Read");
    readContext->data[readContext->ioContext.size()] = (uint8_t *)slot->data;
    readContext->ioContext.emplace_back(slot);

    log_info("Read");
    slot->needDegradedRead = Configuration::GetEnableDegradedRead();
    if (slot->needDegradedRead) {
        log_info("1");
        slot->Queue();
    } else {
        log_info("2");
        uint32_t zoneId = slot->zoneId;
        uint32_t offset = slot->offset;
        log_info("zond id {}, offset {}", zoneId, offset);
        assert(mZones[zoneId] != nullptr);
        mZones[zoneId]->Read(offset, blockSize, slot);
    }

    return true;
}

void ZstoreController::Reset(RequestContext *ctx)
{
    mResetContext.resize(mMeta.n);
    for (int i = 0; i < mMeta.n; ++i) {
        RequestContext *context = &mResetContext[i];
        context->Clear();
        context->associatedRequest = ctx;
        context->ctrl = this;
        context->type = STRIPE_UNIT;
        context->status = RESET_REAPING;
        context->available = false;
        mZones[i]->Reset(context);
    }
}

bool ZstoreController::IsResetDone()
{
    bool done = true;
    for (auto context : mResetContext) {
        if (!context.available) {
            done = false;
            break;
        }
    }
    return done;
}

void ZstoreController::WriteComplete(RequestContext *ctx)
{
    // if (ctx->offset < mHeaderRegionSize ||
    //     ctx->offset >= mHeaderRegionSize + mDataRegionSize) {
    //     return;
    // }
    //
    // uint32_t index =
    //     ctx->zoneId * mDataRegionSize + ctx->offset - mHeaderRegionSize;
    // // Note that here the (lba, timestamp, stripeId) is not the one written
    // to
    // // flash page Thus the lba and timestamp is "INVALID" for parity block,
    // and
    // // the footer (which uses this field to fill) is valid
    // CodedBlockMetadata &pbm = mCodedBlockMetadata[index];
    // pbm.lba = ctx->lba;
    // pbm.timestamp = ctx->timestamp;
    // if (Configuration::GetStripeGroupSize() <= 256) {
    //     mCompactStripeTable[index] = ctx->stripeId;
    // } else if (Configuration::GetStripeGroupSize() <= 65536) {
    //     ((uint16_t *)mCompactStripeTable)[index] = ctx->stripeId;
    // } else {
    //     uint8_t *tmp = mCompactStripeTable + index * 3;
    //     *(tmp + 2) = ctx->stripeId >> 16;
    //     *(uint16_t *)tmp = ctx->stripeId & 0xffff;
    // }
    //
    // FinishBlock(ctx->zoneId, ctx->offset, ctx->lba);
}

void ZstoreController::ReadComplete(RequestContext *ctx)
{
    // uint32_t zoneId = ctx->zoneId;
    // uint32_t offset = ctx->offset;
    // uint32_t blockSize = Configuration::GetBlockSize();
    // uint32_t n = Configuration::GetStripeSize() / blockSize;
    // uint32_t k = Configuration::GetStripeDataSize() / blockSize;
    // RequestContext *parent = ctx->associatedRequest;
    //
    // ReadContext *readContext = ctx->associatedRead;
    // if (ctx->status == READ_REAPING) {
    //     memcpy((uint8_t *)parent->data + ctx->lba - parent->lba, ctx->data,
    //            Configuration::GetStripeUnitSize());
    // } else if (ctx->status == DEGRADED_READ_REAPING) {
    //     bool alive[n];
    //     for (uint32_t i = 0; i < n; ++i) {
    //         alive[i] = false;
    //     }
    //
    //     for (uint32_t i = 1; i < 1 + k; ++i) {
    //         uint32_t zid = readContext->ioContext[i]->zoneId;
    //         alive[zid] = true;
    //     }
    //
    //     readContext->data[ctx->zoneId] = ctx->data;
    //     // if (Configuration::GetSystemMode() == ZONEWRITE_ONLY) {
    //     //     DecodeStripe(offset, readContext->data, alive, n, k, zoneId,
    //     //                  blockSize);
    //     // } else {
    //     uint32_t groupSize = Configuration::GetStripeGroupSize();
    //     uint32_t offsetOfStripe =
    //         mHeaderRegionSize +
    //         (offset - mHeaderRegionSize) / groupSize * groupSize +
    //         ctx->stripeId;
    //     DecodeStripe(offsetOfStripe, readContext->data, alive, n, k, zoneId,
    //                  blockSize);
    //     // }
    // }
    //
    // if (!Configuration::GetDeviceSupportMetadata()) {
    //     BlockMetadata *meta = (BlockMetadata *)ctx->meta;
    //     uint32_t index = zoneId * mDataRegionSize + offset -
    //     mHeaderRegionSize; meta->fields.coded.lba =
    //     mCodedBlockMetadata[index].lba; meta->fields.coded.timestamp =
    //     mCodedBlockMetadata[index].timestamp; if
    //     (Configuration::GetStripeGroupSize() <= 256) {
    //         meta->fields.replicated.stripeId = mCompactStripeTable[index];
    //     } else if (Configuration::GetStripeGroupSize() <= 65536) {
    //         meta->fields.replicated.stripeId =
    //             ((uint16_t *)mCompactStripeTable)[index];
    //     } else {
    //         uint8_t *tmp = mCompactStripeTable + index * 3;
    //         meta->fields.replicated.stripeId =
    //             (*(tmp + 2) << 16) | *(uint16_t *)tmp;
    //     }
    // }
    //
    // if (parent->type == GC && parent->meta) {
    //     uint32_t offsetInBlks =
    //         (ctx->lba - parent->lba) / Configuration::GetBlockSize();
    //     BlockMetadata *result = (BlockMetadata *)(ctx->meta);
    //     BlockMetadata *parentMeta =
    //         &((BlockMetadata *)parent->meta)[offsetInBlks];
    //     parentMeta->fields.coded.lba = result->fields.coded.lba;
    //     parentMeta->fields.coded.timestamp = result->fields.coded.timestamp;
    //     parentMeta->fields.replicated.stripeId =
    //         result->fields.replicated.stripeId;
    // }
}

void ZstoreController::ReclaimReadContext(ReadContext *readContext)
{
    mReadContextPool->ReturnContext(readContext);
}
