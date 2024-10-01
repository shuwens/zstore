#include "include/zstore_controller.h"
#include "common.cc"
#include "device.cc"
#include "include/common.h"
#include "include/configuration.h"
#include "include/device.h"
#include "include/utils.hpp"
#include "src/include/global.h"

static std::vector<Device *> g_devices;

void ZstoreController::initHttpThread()
{
    struct spdk_cpuset cpumask;
    for (uint32_t threadId = 0; threadId < Configuration::GetNumHttpThreads();
         ++threadId) {
        spdk_cpuset_zero(&cpumask);
        spdk_cpuset_set_cpu(&cpumask,
                            Configuration::GetHttpThreadCoreId(threadId), true);
        mHttpThread[threadId].thread =
            spdk_thread_create("HttpThread", &cpumask);
        assert(mHttpThread[threadId].thread != nullptr);
        mHttpThread[threadId].controller = this;
        int rc;
        if (Configuration::UseDummyWorkload())
            rc = spdk_env_thread_launch_pinned(
                Configuration::GetHttpThreadCoreId(threadId), dummyWorker,
                &mHttpThread[threadId]);
        else
            rc = spdk_env_thread_launch_pinned(
                Configuration::GetHttpThreadCoreId(threadId), httpWorker,
                &mHttpThread[threadId]);
        log_info("Http thread name {} id {} on core {}",
                 spdk_thread_get_name(mHttpThread[threadId].thread),
                 spdk_thread_get_id(mHttpThread[threadId].thread),
                 Configuration::GetHttpThreadCoreId(threadId));
        if (rc < 0) {
            log_error("Failed to launch Http thread error: {} {}", strerror(rc),
                      spdk_strerror(rc));
        }
    }
}

void ZstoreController::initCompletionThread()
{
    struct spdk_cpuset cpumask;
    spdk_cpuset_zero(&cpumask);
    spdk_cpuset_set_cpu(&cpumask, Configuration::GetCompletionThreadCoreId(),
                        true);
    log_info("Create {} (id {}) on Core {}",
             spdk_thread_get_name(mCompletionThread),
             spdk_thread_get_id(mCompletionThread),
             Configuration::GetCompletionThreadCoreId());
    int rc = spdk_env_thread_launch_pinned(
        Configuration::GetCompletionThreadCoreId(), completionWorker, this);
    if (rc < 0) {
        log_error("Failed to launch completion thread, error: {}",
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
    log_info("Create {} (id {}) on Core {}",
             spdk_thread_get_name(mDispatchThread),
             spdk_thread_get_id(mDispatchThread),
             Configuration::GetDispatchThreadCoreId());

    int rc;
    if (Configuration::UseObject()) {
        log_info("Dispatch object worker");
        rc = spdk_env_thread_launch_pinned(
            Configuration::GetDispatchThreadCoreId(), dispatchObjectWorker,
            this);
    } else {
        log_info("Not using object");
        rc = spdk_env_thread_launch_pinned(
            Configuration::GetDispatchThreadCoreId(), dispatchWorker, this);
    }
    if (rc < 0) {
        log_error("Failed to launch dispatch thread error: {} {}", strerror(rc),
                  spdk_strerror(rc));
    }
}

void ZstoreController::initIoThread()
{
    struct spdk_cpuset cpumask;
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
        log_info("IO thread name {} id {} on core {}",
                 spdk_thread_get_name(mIoThread[threadId].thread),
                 spdk_thread_get_id(mIoThread[threadId].thread),
                 Configuration::GetIoThreadCoreId(threadId));
        if (rc < 0) {
            log_error("Failed to launch IO thread error: {} {}", strerror(rc),
                      spdk_strerror(rc));
        }
    }
}

int ZstoreController::PopulateMap(bool bogus)
{
    mMap.insert({"apples", createMapEntry("device", 0).value()});
    mMap.insert({"carrots", createMapEntry("device", 7).value()});
    mMap.insert({"tomatoes", createMapEntry("device", 13).value()});

    // if (bogus) {
    for (int i = 0; i < 2'000'000; i++) {
        mMap.insert(
            {"key" + std::to_string(i), createMapEntry("device", i).value()});
    }
    // }

    return 0;
}

int ZstoreController::Init(bool object)
{
    int rc = 0;
    verbose = true;

    // TODO: set all parameters too
    log_debug("mZone sizes {}", mZones.size());

    log_debug("Configure Zstore with configuration");
    setQueuDepth(Configuration::GetQueueDepth());
    setContextPoolSize(Configuration::GetContextPoolSize());
    setNumOfDevices(Configuration::GetNumOfDevices());

    // we add one device for now
    {
        Device *device = new Device();

        if (register_controllers(device) != 0) {
            rc = 1;
            zstore_cleanup();
            return rc;
        }

        g_devices.emplace_back(device);
    }
    mDevices = g_devices;

    // Preallocate contexts for user requests
    // Sufficient to support multiple I/O queues of NVMe-oF target
    mRequestContextPool = new RequestContextPool(mContextPoolSize);

    if (mRequestContextPool == NULL) {
        log_error("could not initialize task pool");
        rc = 1;
        zstore_cleanup();
        return rc;
    }
    isDraining = false;
    // bogus setup for Map and BF

    PopulateMap(true);
    pivot = 0;

    // Create poll groups for the io threads and perform initialization
    for (uint32_t threadId = 0; threadId < Configuration::GetNumIoThreads();
         ++threadId) {
        mIoThread[threadId].group = spdk_nvme_poll_group_create(NULL, NULL);
        mIoThread[threadId].controller = this;
    }
    for (uint32_t i = 0; i < mN; ++i) {
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

    log_info("Initialization complete. Launching workers.");

    CheckIoQpair("Starting all the threads");

    log_debug("ZstoreController launching threads");

    initIoThread();
    initDispatchThread();

    auto const address = net::ip::make_address("127.0.0.1");
    auto const port = 2000;
    auto const doc_root = std::make_shared<std::string>(".");
    std::make_shared<listener>(mIoc_, tcp::endpoint{address, port}, doc_root,
                               *this)
        ->run();

    for (uint32_t threadId = 0; threadId < Configuration::GetNumHttpThreads();
         ++threadId) {
        // mHttpThread[threadId].group = spdk_nvme_poll_group_create(NULL,
        // NULL);
        mHttpThread[threadId].controller = this;
    }
    initHttpThread();
    log_info("ZstoreController Init finish");

    return rc;
}

void ZstoreController::ReadInDispatchThread(RequestContext *ctx)
{
    thread_send_msg(GetIoThread(0), zoneRead, ctx);
}

void ZstoreController::WriteInDispatchThread(RequestContext *ctx)
{
    thread_send_msg(GetIoThread(0), zoneRead, ctx);
}

// TODO: assume we only have one device, we should check all device in the end
bool ZstoreController::CheckIoQpair(std::string msg)
{
    assert(mDevices[0] != nullptr);
    assert(mDevices[0]->GetIoQueue(0) != nullptr);
    if (!spdk_nvme_qpair_is_connected(mDevices[0]->GetIoQueue(0)))
        exit(0);
    return spdk_nvme_qpair_is_connected(mDevices[0]->GetIoQueue(0));
}

struct spdk_nvme_qpair *ZstoreController::GetIoQpair()
{
    assert(mDevices[0] != nullptr);
    assert(mDevices[0]->GetIoQueue(0) != nullptr);

    return mDevices[0]->GetIoQueue(0);
}

ZstoreController::~ZstoreController()
{
    for (uint32_t i = 0; i < Configuration::GetNumIoThreads(); ++i) {
        thread_send_msg(mIoThread[i].thread, quit, nullptr);
    }
    thread_send_msg(mDispatchThread, quit, nullptr);
    for (uint32_t i = 0; i < Configuration::GetNumHttpThreads(); ++i) {
        thread_send_msg(mHttpThread[i].thread, quit, nullptr);
    }
    log_debug("drain io: {}", spdk_get_ticks());
    log_debug("clean up ns worker");
    cleanup_ns_worker_ctx();
    //
    //     std::vector<uint64_t> deltas1;
    //     for (int i = 0; i < zctrlr->mWorker->ns_ctx->stimes.size(); i++)
    //     {
    //         deltas1.push_back(
    //             std::chrono::duration_cast<std::chrono::microseconds>(
    //                 zctrlr->mWorker->ns_ctx->etimes[i] -
    //                 zctrlr->mWorker->ns_ctx->stimes[i])
    //                 .count());
    //     }
    //     auto sum1 = std::accumulate(deltas1.begin(), deltas1.end(), 0.0);
    //     auto mean1 = sum1 / deltas1.size();
    //     auto sq_sum1 = std::inner_product(deltas1.begin(), deltas1.end(),
    //                                       deltas1.begin(), 0.0);
    //     auto stdev1 = std::sqrt(sq_sum1 / deltas1.size() - mean1 *
    //     mean1); log_info("qd: {}, mean {}, std {}",
    //              zctrlr->mWorker->ns_ctx->io_completed, mean1, stdev1);
    //
    //     // clearnup
    //     deltas1.clear();
    //     zctrlr->mWorker->ns_ctx->etimes.clear();
    //     zctrlr->mWorker->ns_ctx->stimes.clear();
    //     // }
    //
    log_debug("end work fn");
    // print_stats(this);
}

void ZstoreController::register_ctrlr(Device *device,
                                      struct spdk_nvme_ctrlr *ctrlr)
{
    uint32_t nsid;
    struct spdk_nvme_ns *ns;
    union spdk_nvme_cap_register cap = spdk_nvme_ctrlr_get_regs_cap(ctrlr);
    const struct spdk_nvme_ctrlr_data *cdata = spdk_nvme_ctrlr_get_data(ctrlr);

    for (nsid = spdk_nvme_ctrlr_get_first_active_ns(ctrlr); nsid != 0;
         nsid = spdk_nvme_ctrlr_get_next_active_ns(ctrlr, nsid)) {
        ns = spdk_nvme_ctrlr_get_ns(ctrlr, nsid);
        if (ns == NULL) {
            continue;
        }

        if (spdk_nvme_ns_get_csi(ns) != SPDK_NVME_CSI_ZNS) {
            log_info("ns {} is not zns ns", nsid);
            // continue;
        } else {
            // log_info("ns {} is zns ns", nsid);
        }

        // register_ns(ctrlr, ns);
        device->Init(ctrlr, nsid);
    }

    // TODO log and store stats

    auto zone_size_sectors = spdk_nvme_zns_ns_get_zone_size_sectors(ns);
    auto zone_size_bytes = spdk_nvme_zns_ns_get_zone_size(ns);
    auto num_zones = spdk_nvme_zns_ns_get_num_zones(ns);
    uint32_t max_open_zones = spdk_nvme_zns_ns_get_max_open_zones(ns);
    uint32_t active_zones = spdk_nvme_zns_ns_get_max_active_zones(ns);
    uint32_t max_zone_append_size =
        spdk_nvme_zns_ctrlr_get_max_zone_append_size(ctrlr);
    auto metadata_size = spdk_nvme_ns_get_md_size(ns);

    log_info("Zone size: sectors {}, bytes {}", zone_size_sectors,
             zone_size_bytes);
    log_info("Zones: num {}, max open {}, active {}", num_zones, max_open_zones,
             active_zones);
    log_info("Max zones append size: {}, metadata size {}",
             max_zone_append_size, metadata_size);
}

void ZstoreController::zns_dev_init(Device *device, std::string ip1,
                                    std::string port1)
{
    int rc = 0;

    char *g_hostnqn = "nqn.2024-04.io.zstore:cnode1";

    log_debug("zns dev");
    // 1. connect nvmf device
    struct spdk_nvme_transport_id trid1 = {};
    snprintf(trid1.traddr, sizeof(trid1.traddr), "%s", ip1.c_str());
    snprintf(trid1.trsvcid, sizeof(trid1.trsvcid), "%s", port1.c_str());
    snprintf(trid1.subnqn, sizeof(trid1.subnqn), "%s", g_hostnqn);
    trid1.adrfam = SPDK_NVMF_ADRFAM_IPV4;
    trid1.trtype = SPDK_NVME_TRANSPORT_TCP;

    struct spdk_nvme_ctrlr_opts opts;
    spdk_nvme_ctrlr_get_default_ctrlr_opts(&opts, sizeof(opts));
    snprintf(opts.hostnqn, sizeof(opts.hostnqn), "%s", g_hostnqn);
    // NOTE: disable keep alive timeout
    opts.keep_alive_timeout_ms = 0;
    register_ctrlr(device, spdk_nvme_connect(&trid1, &opts, sizeof(opts)));
    device->SetDeviceTransportAddress(trid1.traddr);
}

int ZstoreController::register_controllers(Device *device)
{
    log_info("Initializing NVMe Controllers");

    // RDMA
    // zns_dev_init(ctx, "192.168.100.9", "5520");
    // TCP
    zns_dev_init(device, "12.12.12.2", "5520");

    return 0;
}

void ZstoreController::unregister_controllers(Device *device)
{
    struct spdk_nvme_detach_ctx *detach_ctx = NULL;
}

void ZstoreController::zstore_cleanup()
{
    log_info("unreg controllers");
    unregister_controllers(mDevices[0]);
    log_info("cleanup ");
    cleanup(0);

    spdk_env_fini();
}

void ZstoreController::cleanup_ns_worker_ctx()
{
    log_info("here");
    // FIXME
    // thread_send_msg(mIoThread.thread, quit, nullptr);
    // spdk_nvme_ctrlr_free_io_qpair(mDevices[0]->GetIoQueue());
}

void ZstoreController::cleanup(uint32_t task_count)
{
    // free(mNamespace);
    //
    // free(mWorker->ns_ctx);
    // free(mWorker);

    // if (spdk_mempool_count(mTaskPool) != (size_t)task_count) {
    //     log_error("mTaskPool count is {} but should be {}",
    //               spdk_mempool_count(mTaskPool), task_count);
    // }
    // spdk_mempool_free(mTaskPool);
}

void ZstoreController::EnqueueRead(RequestContext *ctx)
{
    mReadQueue.push(ctx);
}

void ZstoreController::EnqueueWrite(RequestContext *ctx)
{
    mWriteQueue.push(ctx);
    // log_debug("after push: read q {}", GetWriteQueueSize());
}

Result<MapEntry> ZstoreController::find_object(std::string key)
{
    // thanks to std::less<> this no longer creates a std::string
    auto it = mMap.find(key);
    if (it != mMap.end()) {
        // std::cout << "I have " << it->second << " apples!\n";
        return Result<MapEntry>(it->second);
    }

    // int64_t head_index =
    //     store_header_->hashmap.hash_entries[object_id % 65536].object_list;
    // int64_t current_index = head_index;
    //
    // while (current_index >= 0) {
    //     ObjectEntry *current = &store_header_->object_entries[current_index];
    //     if (current->object_id == object_id) {
    //         return current_index;
    //     }
    //     current_index = current->next;
    // }
}

void tryDrainController(void *args)
{
    DrainArgs *drainArgs = (DrainArgs *)args;
    // drainArgs->ctrl->CheckSegments();
    // drainArgs->ctrl->ReclaimContexts();
    // drainArgs->ctrl->ProceedGc();
    drainArgs->success =
        drainArgs->ctrl->mRequestContextPool->availableContexts.size() ==
        drainArgs->ctrl->GetContextPoolSize();

    drainArgs->ready = true;
}

void ZstoreController::Drain()
{
    log_info("Perform draining on the system.");
    isDraining = true;
    DrainArgs args;
    args.ctrl = this;
    args.success = false;
    while (!args.success) {
        args.ready = false;
        thread_send_msg(mDispatchThread, tryDrainController, &args);
        busyWait(&args.ready);
    }

    auto etime = std::chrono::high_resolution_clock::now();
    auto delta =
        std::chrono::duration_cast<std::chrono::microseconds>(etime - stime)
            .count();
    auto tput = GetDevice()->mTotalCounts * g_micro_to_second / delta;

    // if (g->verbose)
    log_info("Total IO {}, total time {}ms, throughput {} IOPS",
             GetDevice()->mTotalCounts, delta, tput);

    // log_debug("drain io: {}", spdk_get_ticks());
    log_debug("clean up ns worker");
    // ctrl->cleanup_ns_worker_ctx();
    // print_stats(ctrl);
    // exit(0);

    log_info("done .");
}
