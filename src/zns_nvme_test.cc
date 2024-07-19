#include "include/utils.hpp"
#include "include/zns_device.h"
#include "spdk/env.h"
#include "spdk/event.h"
#include "spdk/log.h"
#include "spdk/nvme.h"
#include "spdk/nvme_zns.h"
#include "spdk/nvmf_spec.h"
#include <atomic>

// static const char *g_bdev_name = "Nvme1n2";
static const char *g_hostnqn = "nqn.2024-04.io.zstore:cnode1";

// struct ZstoreContext {
struct ZstoreContext {
    // spdk things
    struct spdk_nvme_ctrlr *ctrlr = nullptr;
    struct spdk_nvme_ns *ns = nullptr;
    struct spdk_nvme_qpair *qpair = nullptr;
    char *write_buff = nullptr;
    char *read_buff = nullptr;
    uint32_t buff_size;
    // char *bdev_name;

    // device related
    bool device_support_meta = true;

    bool done = false;
    u64 num_queued = 0;
    u64 num_completed = 0;
    u64 num_success = 0;
    u64 num_fail = 0;

    u64 total_us = 0;

    std::atomic<int> count; // atomic count for concurrency
};

inline void spin_complete(struct ZstoreContext *ctx)
{
    while (spdk_nvme_qpair_process_completions(ctx->qpair, 0) == 0) {
        ;
    }
}

static void read_complete(void *arg, const struct spdk_nvme_cpl *completion)
{
    struct ZstoreContext *ctx = static_cast<struct ZstoreContext *>(arg);

    if (spdk_nvme_cpl_is_error(completion)) {
        SPDK_ERRLOG("nvme io read error: %s\n",
                    spdk_nvme_cpl_get_status_string(&completion->status));
        spdk_nvme_ctrlr_free_io_qpair(ctx->qpair);
        spdk_app_stop(-1);
        return;
    }

    // compare read and write
    int cmp_res = memcmp(ctx->write_buff, ctx->read_buff, ctx->buff_size);
    if (cmp_res != 0) {
        SPDK_ERRLOG("read zone data error.\n");
        spdk_nvme_ctrlr_free_io_qpair(ctx->qpair);
        spdk_app_stop(-1);
        return;
    }
    ctx->count.fetch_add(1);
    if (ctx->count.load() == 4 * 0x100) {
        SPDK_NOTICELOG("read zone complete.\n");
        spdk_nvme_ctrlr_free_io_qpair(ctx->qpair);
        spdk_app_stop(0);
        return;
    }

    memset(ctx->read_buff, 0x34, ctx->buff_size);
    // uint64_t lba = ctx->count.load();
    uint64_t lba =
        ctx->count.load() / 0x100 * spdk_nvme_ns_get_num_sectors(ctx->ns) +
        ctx->count.load() % 0x100;

    int rc = spdk_nvme_ns_cmd_read(ctx->ns, ctx->qpair, ctx->read_buff, lba, 1,
                                   read_complete, ctx, 0);
    SPDK_NOTICELOG("read lba:0x%lx\n", lba);
    if (rc != 0) {
        SPDK_ERRLOG("error while reading from bdev: %d\n", rc);
        spdk_nvme_ctrlr_free_io_qpair(ctx->qpair);
        spdk_app_stop(-1);
        return;
    }
}

static void read_zone(void *arg)
{
    struct ZstoreContext *ctx = static_cast<struct ZstoreContext *>(arg);
    ctx->count = 0;
    memset(ctx->read_buff, 0x34, ctx->buff_size);
    int rc = spdk_nvme_ns_cmd_read(ctx->ns, ctx->qpair, ctx->read_buff, 0, 1,
                                   read_complete, ctx, 0);
    SPDK_NOTICELOG("read lba:0x%x\n", 0x0);
    if (rc) {
        SPDK_ERRLOG("error while reading from bdev: %d\n", rc);
        spdk_nvme_ctrlr_free_io_qpair(ctx->qpair);
        spdk_app_stop(-1);
    }
}

static void write_zone_complete(void *arg,
                                const struct spdk_nvme_cpl *completion)
{
    struct ZstoreContext *ctx = static_cast<struct ZstoreContext *>(arg);

    if (spdk_nvme_cpl_is_error(completion)) {
        SPDK_ERRLOG("nvme io write error: %s\n",
                    spdk_nvme_cpl_get_status_string(&completion->status));
        spdk_nvme_ctrlr_free_io_qpair(ctx->qpair);
        spdk_app_stop(-1);
        return;
    }
    ctx->count.fetch_sub(1);
    if (ctx->count.load() == 0) {
        SPDK_NOTICELOG("write zone complete.\n");
        read_zone(ctx);
    }
}

static void write_zone(void *arg)
{
    struct ZstoreContext *ctx = static_cast<struct ZstoreContext *>(arg);
    uint64_t zone_size = spdk_nvme_zns_ns_get_zone_size_sectors(ctx->ns);
    int append_times = 0x100;
    // ctx->count = append_times;
    int zone_num = 4;
    ctx->count = zone_num * append_times;
    memset(ctx->write_buff, 0x12, ctx->buff_size);
    for (uint64_t slba = 0; slba < zone_num * zone_size; slba += zone_size) {
        for (int i = 0; i < append_times; i++) {
            int rc =
                spdk_nvme_ns_cmd_write(ctx->ns, ctx->qpair, ctx->write_buff,
                                       slba, 1, write_zone_complete, ctx, 0);
            if (rc != 0) {
                SPDK_ERRLOG("error while write_zone: %d\n", rc);
                spdk_nvme_ctrlr_free_io_qpair(ctx->qpair);
                spdk_app_stop(-1);
                return;
            }
        }
    }
}

static void reset_zone_complete(void *arg, const struct spdk_nvme_cpl *cpl)
{
    log_info("Entered reset zone complete");
    struct ZstoreContext *ctx = static_cast<struct ZstoreContext *>(arg);

    ctx->num_completed += 1;
    if (spdk_nvme_cpl_is_error(cpl)) {
        spdk_nvme_qpair_print_completion(ctx->qpair,
                                         (struct spdk_nvme_cpl *)cpl);
        fprintf(stderr, "Reset all zone error - status = %s\n",
                spdk_nvme_cpl_get_status_string(&cpl->status));
        ctx->num_fail += 1;
        ctx->num_queued -= 1;
        return;
        // SPDK_ERRLOG("nvme io reset error: %s\n",
        //             spdk_nvme_cpl_get_status_string(&completion->status));
        // spdk_nvme_ctrlr_free_io_qpair(ctx->qpair);
        // spdk_app_stop(-1);
        // return;
    }
    ctx->num_success += 1;
    ctx->num_queued -= 1;

    log_debug("reset zone complete: queued {} completed {} success {} fail {}",
              ctx->num_queued, ctx->num_completed, ctx->num_success,
              ctx->num_fail);
    // when all reset is done, do writes
    ctx->count.fetch_sub(1);
    if (ctx->count.load() == 0) {
        SPDK_NOTICELOG("reset zone complete.\n");
        write_zone(ctx);
    }
}

static void reset_zone(void *arg)
{
    log_info("reset zone");
    struct ZstoreContext *ctx = static_cast<struct ZstoreContext *>(arg);
    int zone_num = 10;
    ctx->count = zone_num;
    uint64_t zone_size = spdk_nvme_zns_ns_get_zone_size_sectors(ctx->ns);
    log_debug("Reset zone: num {}, size {}, loop {}", zone_num, zone_size,
              zone_num * zone_size);
    ctx->num_queued++;
    for (uint64_t slba = 0; slba < zone_num * zone_size; slba += zone_size) {
        // log_debug("Reset zone: slba {}", slba);
        int rc = spdk_nvme_zns_reset_zone(ctx->ns, ctx->qpair, slba, true,
                                          reset_zone_complete, ctx);
        log_debug("Reset zone: slba {}: {}", slba, rc);
        // int rc = spdk_nvme_ns_cmd_zone_management(
        //             ctx->ns, ctx->qpair,
        //             SPDK_NVME_ZONE_MANAGEMENT_SEND, SPDK_NVME_ZONE_RESET, i,
        //          if (rc == -ENOMEM) {
        if (rc == -ENOMEM) {
            log_debug("Queueing io");
        } else if (rc) {
            SPDK_ERRLOG("error while resetting zone: %d\n", rc);
            spdk_nvme_ctrlr_free_io_qpair(ctx->qpair);
            spdk_app_stop(-1);
            return;
        }
    }
    while (ctx->num_queued) {
        // log_debug("reached here: queued {}", ctx->num_queued);
        spdk_nvme_qpair_process_completions(ctx->qpair, 0);
    }
    log_info("reset zone done");
}

// static void test_nvme_event_cb(void *arg, const struct spdk_nvme_cpl *cpl)
// {
//     SPDK_NOTICELOG("Unsupported nvme event: %s\n",
//                    spdk_nvme_cpl_get_status_string(&cpl->status));
// }

// TODO: unused code
static void unused_zns_dev_init(void *arg)
{
    int ret = 0;
    struct spdk_env_opts opts;
    spdk_env_opts_init(&opts);
    opts.core_mask = "0x1fc";
    if (spdk_env_init(&opts) < 0) {
        fprintf(stderr, "Unable to initialize SPDK env.\n");
        exit(-1);
    }

    ret = spdk_thread_lib_init(nullptr, 0);
    if (ret < 0) {
        fprintf(stderr, "Unable to initialize SPDK thread lib.\n");
        exit(-1);
    }
    // ret = spdk_nvme_probe(NULL, NULL, probe_cb, attach_cb, NULL);
    if (ret < 0) {
        fprintf(stderr, "Unable to probe devices\n");
        exit(-1);
    }
}

static void zns_dev_init(void *arg)
{
    struct ZstoreContext *ctx = static_cast<struct ZstoreContext *>(arg);
    int rc = 0;
    ctx->ctrlr = NULL;
    ctx->ns = NULL;

    SPDK_NOTICELOG("Successfully started the application\n");

    // 1. connect nvmf device
    struct spdk_nvme_transport_id trid = {};
    int nsid = 0;

    snprintf(trid.traddr, sizeof(trid.traddr), "%s", "192.168.1.121");
    snprintf(trid.trsvcid, sizeof(trid.trsvcid), "%s", "4420");
    // snprintf(trid.subnqn, sizeof(trid.subnqn), "%s",
    // SPDK_NVMF_DISCOVERY_NQN);
    snprintf(trid.subnqn, sizeof(trid.subnqn), "%s", g_hostnqn);
    trid.adrfam = SPDK_NVMF_ADRFAM_IPV4;
    trid.trtype = SPDK_NVME_TRANSPORT_TCP;

    struct spdk_nvme_ctrlr_opts opts;
    spdk_nvme_ctrlr_get_default_ctrlr_opts(&opts, sizeof(opts));
    memcpy(opts.hostnqn, g_hostnqn, sizeof(opts.hostnqn));
    ctx->ctrlr = spdk_nvme_connect(&trid, &opts, sizeof(opts));
    // ctx->ctrlr = spdk_nvme_connect(&trid, NULL, 0);

    if (ctx->ctrlr == NULL) {
        fprintf(stderr,
                "spdk_nvme_connect() failed for transport address '%s'\n",
                trid.traddr);
        spdk_app_stop(-1);
        // pthread_kill(g_fuzz_td, SIGSEGV);
        // return NULL;
        // return rc;
    }

    SPDK_NOTICELOG("Successfully started the application\n");
    SPDK_NOTICELOG("Initializing NVMe controller\n");

    if (spdk_nvme_zns_ctrlr_get_data(ctx->ctrlr)) {
        printf("ZNS Specific Controller Data\n");
        printf("============================\n");
        printf("Zone Append Size Limit:      %u\n",
               spdk_nvme_zns_ctrlr_get_data(ctx->ctrlr)->zasl);
        printf("\n");
        printf("\n");
    }

    printf("Active Namespaces\n");
    printf("=================\n");
    for (nsid = spdk_nvme_ctrlr_get_first_active_ns(ctx->ctrlr); nsid != 0;
         nsid = spdk_nvme_ctrlr_get_next_active_ns(ctx->ctrlr, nsid)) {
        print_namespace(ctx->ctrlr, spdk_nvme_ctrlr_get_ns(ctx->ctrlr, nsid));
    }

    ctx->ns = spdk_nvme_ctrlr_get_ns(ctx->ctrlr, 1);
    if (ctx->ns == NULL) {
        SPDK_ERRLOG("Could not get NVMe namespace\n");
        spdk_app_stop(-1);
        return;
    }

    // 2. creating qpairs
    struct spdk_nvme_io_qpair_opts qpair_opts;
    spdk_nvme_ctrlr_get_default_io_qpair_opts(ctx->ctrlr, &qpair_opts,
                                              sizeof(qpair_opts));
    qpair_opts.delay_cmd_submit = true;
    qpair_opts.create_only = true;
    ctx->qpair = spdk_nvme_ctrlr_alloc_io_qpair(ctx->ctrlr, &qpair_opts,
                                                sizeof(qpair_opts));
    if (ctx->qpair == NULL) {
        SPDK_ERRLOG("Could not allocate IO queue pair\n");
        spdk_app_stop(-1);
        return;
    }

    // 3. connect qpair
    rc = spdk_nvme_ctrlr_connect_io_qpair(ctx->ctrlr, ctx->qpair);
    if (rc) {
        log_error("Could not connect IO queue pair\n");
        spdk_app_stop(-1);
        return;
    }
}

// TODO: make ZNS device class and put this in Init() or ctor
static void zstore_init(void *arg)
{
    struct ZstoreContext *ctx = static_cast<struct ZstoreContext *>(arg);
    int rc = 0;

    if (spdk_nvme_ns_get_md_size(ctx->ns) == 0) {
        ctx->device_support_meta = false;
    }

    // Adjust the capacity for user data = total capacity - footer size
    // The L2P table information at the end of the segment
    // Each block needs (LBA + timestamp + stripe ID, 20 bytes) for L2P table
    // recovery; we round the number to block size
    auto zone_size = spdk_nvme_zns_ns_get_zone_size_sectors(ctx->ns);
    u32 num_zones = spdk_nvme_zns_ns_get_num_zones(ctx->ns);
    u64 zone_capacity = 0;
    if (zone_size == 2ull * 1024 * 256) {
        zone_capacity = 1077 * 256; // hard-coded here since it is ZN540;
                                    // update this for emulated SSDs
    } else {
        zone_capacity = zone_size;
    }
    log_info("Zone size: {}, zone cap: {}, num of zones: {}\n", zone_size,
             zone_capacity, num_zones);

    //
    // mZoneSize = mDevices[0]->GetZoneSize();
    // uint64_t zoneCapacity = mDevices[0]->GetZoneCapacity();
    uint32_t blockSize = 4096;
    uint64_t storageSpace = 1024 * 1024 * 1024 * 1024ull;
    auto mMappingBlockUnitSize = blockSize * blockSize / 4;
    // uint32_t maxFooterSize =
    //     round_up(zoneCapacity, (blockSize / 20)) / (blockSize / 20);
    // auto mHeaderRegionSize = Configuration::GetMaxStripeUnitSize() /
    // blockSize;
    //  1;
    // mDataRegionSize =
    //     round_down(zoneCapacity - mHeaderRegionSize - maxFooterSize,
    //                Configuration::GetMaxStripeGroupSizeInBlocks());
    // mFooterRegionSize =
    //     round_up(mDataRegionSize, (blockSize / 20)) / (blockSize / 20);
    // printf("HeaderRegion: %u, DataRegion: %u, FooterRegion: %u\n",
    //        mHeaderRegionSize, mDataRegionSize, mFooterRegionSize);

    // uint32_t totalNumZones =
    //     round_up(storageSpace / blockSize, mDataRegionSize) /
    //     mDataRegionSize;
    // uint32_t numDataBlocks =
    //     Configuration::GetStripeDataSize() /
    //     Configuration::GetStripeUnitSize();
    // uint32_t numZonesNeededPerDevice =
    //     round_up(totalNumZones, numDataBlocks) / numDataBlocks;
    // uint32_t numZonesReservedPerDevice =
    //     std::max(3u, (uint32_t)(numZonesNeededPerDevice * 0.25));

    // for (uint32_t i = 0; i < mN; ++i) {
    //     mDevices[i]->SetDeviceId(i);
    //     mDevices[i]->InitZones(numZonesNeededPerDevice,
    //                            numZonesReservedPerDevice);
    // }

    // mStorageSpaceThresholdForGcInSegments = numZonesReservedPerDevice / 2;
    // mAvailableStorageSpaceInSegments =
    //     numZonesNeededPerDevice + numZonesReservedPerDevice;
    // mTotalNumSegments = mAvailableStorageSpaceInSegments;
    // printf("Total available segments: %u, reserved segments: %u\n",
    //        mAvailableStorageSpaceInSegments,
    //        mStorageSpaceThresholdForGcInSegments);
    // mZoneToSegmentMap = new Segment *[mTotalNumSegments * mN];
    // memset(mZoneToSegmentMap, 0, sizeof(Segment *) * mTotalNumSegments * mN);

    // // Preallocate contexts for user requests
    // // Sufficient to support multiple I/O queues of NVMe-oF target
    // mRequestContextPoolForUserRequests = new RequestContextPool(2048);
    // mRequestContextPoolForSegments = new RequestContextPool(4096);
    // mRequestContextPoolForIndex = new RequestContextPool(128);
    //
    // mReadContextPool = new ReadContextPool(512,
    // mRequestContextPoolForSegments);

    // // Initialize address map
    // mAddressMap =
    //     new IndexMap(storageSpace / blockSize,
    //                  Configuration::GetL2PTableSizeInBytes() / blockSize);
    // //  mAddressMap = new IndexMap(storageSpace / blockSize, 0);
    // //  mAddressMapMemory = new uint32_t[storageSpace / blockSize];
    // //  memset(mAddressMapMemory, 0xff, sizeof(uint32_t) * storageSpace /
    // //  blockSize);

    // // Create poll groups for the io threads and perform initialization
    // for (uint32_t threadId = 0; threadId < Configuration::GetNumIoThreads();
    //      ++threadId) {
    //     mIoThread[threadId].group = spdk_nvme_poll_group_create(NULL, NULL);
    //     mIoThread[threadId].controller = this;
    // }
    // for (uint32_t i = 0; i < mN; ++i) {
    //     struct spdk_nvme_qpair **ioQueues = mDevices[i]->GetIoQueues();
    //     for (uint32_t threadId = 0; threadId <
    //     Configuration::GetNumIoThreads();
    //          ++threadId) {
    //         spdk_nvme_ctrlr_disconnect_io_qpair(ioQueues[threadId]);
    //         int rc = spdk_nvme_poll_group_add(mIoThread[threadId].group,
    //                                           ioQueues[threadId]);
    //         assert(rc == 0);
    //     }
    //     mDevices[i]->ConnectIoPairs();
    // }
    //
    // // Preallocate segments
    // mNumOpenSegments = Configuration::GetNumOpenSegments();
    // mOpenGroupForLarge = new bool[mNumOpenSegments];
    // for (int i = 0; i < mNumOpenSegments; ++i) {
    //     mOpenGroupForLarge[i] = Configuration::GetStripeUnitSize(i) >=
    //                             Configuration::GetLargeRequestThreshold();
    // }
    //
    // mStripeWriteContextPools =
    //     new StripeWriteContextPool *[mNumOpenSegments + 2];
    // for (uint32_t i = 0; i < mNumOpenSegments + 2; ++i) {
    //     bool flag = i < mNumOpenSegments
    //                     ? (Configuration::GetStripeGroupSize(i) == 1)
    //                     : (Configuration::GetStripeGroupSize(mNumOpenSegments
    //                     -
    //                                                          1) == 1);
    //     if (Configuration::GetSystemMode() == ZONEWRITE_ONLY ||
    //         (Configuration::GetSystemMode() == ZAPRAID && flag)) {
    //         //      mStripeWriteContextPools[i] = new
    //         StripeWriteContextPool(1,
    //         //      mRequestContextPoolForSegments);
    //         mStripeWriteContextPools[i]
    //         //      = new StripeWriteContextPool(3,
    //         //      mRequestContextPoolForSegments);
    //         mStripeWriteContextPools[i] =
    //             new StripeWriteContextPool(3,
    //             mRequestContextPoolForSegments);
    //         printf("create segment for zone write (i=%d)\n", i);
    //         // a large value causes zone-writes to be slow
    //     } else if (Configuration::GetSystemMode() == RAIZN_SIMPLE) {
    //         // for RAIZN, each segment only allows one ongoing write
    //         mStripeWriteContextPools[i] =
    //             new StripeWriteContextPool(1,
    //             mRequestContextPoolForSegments);
    //         //      mStripeWriteContextPools[i] = new
    //         StripeWriteContextPool(3,
    //         //      mRequestContextPoolForSegments);
    //     } else {
    //         uint32_t index = i;
    //         if (index >= mNumOpenSegments) {
    //             index = mNumOpenSegments - 1;
    //         }
    //         printf("create segment for zone append (i=%d) flag %d unit size
    //         %d "
    //                "mode "
    //                "%d\n",
    //                i, flag, Configuration::GetStripeUnitSize(index),
    //                Configuration::GetSystemMode());
    //         //      mStripeWriteContextPools[i] = new
    //         StripeWriteContextPool(64,
    //         //      mRequestContextPoolForSegments); // try fewer for femu
    //         //      devices
    //         mStripeWriteContextPools[i] = new StripeWriteContextPool(
    //             16, mRequestContextPoolForSegments); // for real devices, can
    //                                                  // have more contexts
    //     }
    // }
    //
    // mOpenSegments.resize(mNumOpenSegments);
    // if (Configuration::GetSystemMode() == RAIZN_SIMPLE) {
    //     mNumOpenSegmentsThres--;
    // }
    //
    // debug_error("reboot mode %d\n", Configuration::GetRebootMode());
    // if (Configuration::GetRebootMode() == 0) {
    //     debug_warn("devices size = %lu\n", mDevices.size());
    //     for (uint32_t i = 0; i < mN; ++i) {
    //         debug_warn("Erase device %u\n", i);
    //         mDevices[i]->EraseWholeDevice();
    //     }
    // } else if (Configuration::GetRebootMode() == 1) {
    //     debug_e("restart()");
    //     restart();
    // } else { // needs rebuild; rebootMode = 2
    //     // Suppose drive 0 is broken
    //     mDevices[0]->EraseWholeDevice();
    //     rebuild(0); // suppose rebuilding drive 0
    //     restart();
    // }
    //
    // if (Configuration::GetEventFrameworkEnabled()) {
    //     event_call(Configuration::GetDispatchThreadCoreId(),
    //                registerDispatchRoutine, this, nullptr);
    //     for (uint32_t threadId = 0; threadId <
    //     Configuration::GetNumIoThreads();
    //          ++threadId) {
    //         event_call(Configuration::GetIoThreadCoreId(threadId),
    //                    registerIoCompletionRoutine, &mIoThread[threadId],
    //                    nullptr);
    //     }
    // } else {
    //     initIoThread();
    //     initDispatchThread();
    //     initIndexThread();
    //     initCompletionThread();
    //     initEcThread();
    // }
    //
    // // create initialized segments
    // for (uint32_t i = 0; i < mNumOpenSegments; ++i) {
    //     createSegmentIfNeeded(&mOpenSegments[i], i);
    // }
    //
    // // init Gc
    // initGc();
    //
    // // debug
    // mStartedWrites = 0;
    // mCompletedWrites = 0;
    // mStartedReads = 0;
    // mCompletedReads = 0;
    // mNumIndexWrites = 0;
    // mNumIndexWritesHandled = 0;
    // mNumIndexReads = 0;
    // mNumIndexReadsHandled = 0;
    //
    // Configuration::PrintConfigurations();
}

static void test_start(void *arg1)
{
    log_info("test start");
    struct ZstoreContext *ctx = static_cast<struct ZstoreContext *>(arg1);
    int rc = 0;
    uint32_t buf_align;

    zns_dev_init(ctx);
    if (rc != 0)
        log_error("zns device init fail");

    // Setting up context done

    ctx->buff_size = spdk_nvme_ns_get_sector_size(ctx->ns);
    // buf_align = spdk_nvme_ns_get_sector_size(ctx->ns);
    buf_align = 1;
    // ctx->buff_size = spdk_nvme_ns_get_sector_size(ctx->ns) *
    //                  spdk_nvme_ns_get_md_size(ctx->ns);
    // buf_align = spdk_nvme_ns_get_optimal_io_boundary(ctx->ns);

    // 4096
    log_info("buffer size: {}", ctx->buff_size);
    // 1
    log_info("buffer align: {}", buf_align);

    ctx->write_buff =
        static_cast<char *>(spdk_dma_zmalloc(ctx->buff_size, buf_align, NULL));
    // ctx->write_buff = static_cast<char *>(
    //     spdk_zmalloc(ctx->buff_size, buf_align, NULL,
    //     SPDK_ENV_SOCKET_ID_ANY,
    //                  SPDK_MALLOC_DMA));

    if (!ctx->write_buff) {
        SPDK_ERRLOG("Failed to allocate buffer\n");
        spdk_nvme_ctrlr_free_io_qpair(ctx->qpair);
        spdk_nvme_detach(ctx->ctrlr);
        spdk_app_stop(-1);
        return;
    }
    // ctx->read_buff = static_cast<char *>(
    //     spdk_zmalloc(ctx->buff_size, buf_align, NULL,
    //     SPDK_ENV_SOCKET_ID_ANY,
    //                  SPDK_MALLOC_DMA));
    ctx->read_buff =
        static_cast<char *>(spdk_dma_zmalloc(ctx->buff_size, buf_align, NULL));

    if (!ctx->read_buff) {
        SPDK_ERRLOG("Failed to allocate buffer\n");
        spdk_nvme_ctrlr_free_io_qpair(ctx->qpair);
        spdk_nvme_detach(ctx->ctrlr);
        spdk_app_stop(-1);
        return;
    }
    // block size:4096 write unit:1 zone size:80000 zone num:406 max append
    // size:503 max open zone:14 max active zone:14

    SPDK_NOTICELOG("block size: %d, write unit: %d, zone size: %lx, zone num: "
                   "%lu, max append size: %d,  max open "
                   "zone: %d,max active zone: %d\n ",
                   spdk_nvme_ns_get_sector_size(ctx->ns),
                   spdk_nvme_ns_get_md_size(ctx->ns),
                   spdk_nvme_zns_ns_get_zone_size_sectors(ctx->ns), // zone size
                   spdk_nvme_zns_ns_get_num_zones(ctx->ns),
                   spdk_nvme_zns_ctrlr_get_max_zone_append_size(ctx->ctrlr),
                   spdk_nvme_zns_ns_get_max_open_zones(ctx->ns),
                   spdk_nvme_zns_ns_get_max_active_zones(ctx->ns));
    SPDK_NOTICELOG("sector size:%d zone size:%lx\n",
                   spdk_nvme_ns_get_sector_size(ctx->ns),
                   spdk_nvme_ns_get_size(ctx->ns));

    reset_zone(ctx);
    // write_zone(ctx);
}

int main(int argc, char **argv)
{
    struct spdk_app_opts opts = {};
    int rc = 0;
    struct ZstoreContext ctx = {};

    spdk_app_opts_init(&opts, sizeof(opts));
    opts.name = "test_nvme";

    if ((rc = spdk_app_parse_args(argc, argv, &opts, NULL, NULL, NULL, NULL)) !=
        SPDK_APP_PARSE_ARGS_SUCCESS) {
        exit(rc);
    }
    // ctx.bdev_name = const_cast<char *>(g_bdev_name);
    log_info("HERE");
    rc = spdk_app_start(&opts, test_start, &ctx);
    if (rc) {
        SPDK_ERRLOG("ERROR starting application\n");
    }

    spdk_dma_free(ctx.write_buff);
    spdk_dma_free(ctx.read_buff);

    spdk_app_fini();

    return rc;
}
