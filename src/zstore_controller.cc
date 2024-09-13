#include "include/zstore_controller.h"
#include "common.cc"
#include "include/common.h"
#include "include/configuration.h"
#include "include/request_handler.h"
#include "include/utils.hpp"
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
#include <sys/time.h>
#include <thread>

static void busyWait(bool *ready)
{
    while (!*ready) {
        if (spdk_get_thread() == nullptr) {
            std::this_thread::sleep_for(std::chrono::seconds(0));
        }
    }
}

void ZstoreController::initHttpThread()
{
    struct spdk_cpuset cpumask;
    spdk_cpuset_zero(&cpumask);
    spdk_cpuset_set_cpu(&cpumask, Configuration::GetHttpThreadCoreId(), true);
    mHttpThread = spdk_thread_create("HttpThread", &cpumask);
    log_info("Create {} (id {}) on Core {}", spdk_thread_get_name(mHttpThread),
             spdk_thread_get_id(mHttpThread),
             Configuration::GetHttpThreadCoreId());
    int rc = spdk_env_thread_launch_pinned(Configuration::GetHttpThreadCoreId(),
                                           httpWorker, this);
    if (rc < 0) {
        log_error("Failed to launch ec thread error: {}", spdk_strerror(rc));
    }
}

void ZstoreController::initCompletionThread()
{
    struct spdk_cpuset cpumask;
    spdk_cpuset_zero(&cpumask);
    spdk_cpuset_set_cpu(&cpumask, Configuration::GetCompletionThreadCoreId(),
                        true);
    // mCompletionThread = spdk_thread_create("CompletionThread", &cpumask);
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
    int rc = spdk_env_thread_launch_pinned(
        Configuration::GetDispatchThreadCoreId(), dispatchWorker, this);
    if (rc < 0) {
        log_error("Failed to launch dispatch thread error: {} {}", strerror(rc),
                  spdk_strerror(rc));
    }
}

void ZstoreController::initIoThread()
{
    struct spdk_cpuset cpumask;
    // auto threadId = 0;
    // log_debug("init Io thread {}", threadId);
    spdk_cpuset_zero(&cpumask);
    spdk_cpuset_set_cpu(&cpumask, Configuration::GetIoThreadCoreId(), true);
    mIoThread.thread = spdk_thread_create("IoThread", &cpumask);
    assert(mIoThread.thread != nullptr);
    mIoThread.controller = this;
    log_debug("Create {} (id {}) on Core {}",
              spdk_thread_get_name(mIoThread.thread),
              spdk_thread_get_id(mIoThread.thread),
              Configuration::GetIoThreadCoreId());
    int rc = spdk_env_thread_launch_pinned(Configuration::GetIoThreadCoreId(),
                                           ioWorker, this);
    if (rc < 0) {
        log_error("Failed to launch IO thread error: {} {}", strerror(rc),
                  spdk_strerror(rc));
    }
}

int ZstoreController::Init(bool need_env)
{
    int rc = 0;
    uint32_t task_count = 0;
    char task_pool_name[30];

    mController = g_controller;
    mNamespace = g_namespace;
    mWorker = g_worker;
    mTaskPool = task_pool;

    // if (need_env) {
    //     struct spdk_env_opts opts;
    //     spdk_env_opts_init(&opts);
    //
    //     // NOTE allocate 9 cores
    //     opts.core_mask = "0x1ff";
    //
    //     opts.name = "zstore";
    //     opts.mem_size = g_dpdk_mem;
    //     opts.hugepage_single_segments = g_dpdk_mem_single_seg;
    //     opts.core_mask = g_zstore.core_mask;
    //
    //     if (spdk_env_init(&opts) < 0) {
    //         fprintf(stderr, "Unable to initialize SPDK env.\n");
    //         exit(-1);
    //     }
    //
    //     rc = spdk_thread_lib_init(nullptr, 0);
    //     if (rc < 0) {
    //         fprintf(stderr, "Unable to initialize SPDK thread lib.\n");
    //         exit(-1);
    //     }
    // }

    // log_debug("qpair: connected? {}, enabled? ",
    //       spdk_nvme_qpair_is_connected(mDevices[0]->GetIoQueue()));
    log_debug("mZone sizes {}", mZones.size());

    log_debug("ZstoreController launching threads");

    // log_debug("qpair: connected? {}, enabled? ",
    //           spdk_nvme_qpair_is_connected(mDevices[0]->GetIoQueue()));
    assert(rc == 0);

    SetQueuDepth(Configuration::GetQueueDepth());
    // initCompletionThread();
    // initHttpThread();

    tsc_end =
        spdk_get_ticks() - g_arbitration.time_in_sec * g_arbitration.tsc_rate;
    log_debug("TSC Now: {}, End: {}", spdk_get_ticks(), tsc_end);

    // Create and configure Zstore instance
    // std::string zstore_name, bucket_name;
    // zstore_name = "test";
    // mZstore = new Zstore(zstore_name);
    //
    // zstore.SetVerbosity(1);

    // create a bucket: this process is now manual, not via create/get bucket
    // zstore.buckets.push_back(AWS_S3_Bucket(bucket_name, "db"));

    // create_dummy_objects(zstore);
    // Start the web server controllers.

    // mHandler = new ZstoreHandler;
    // CivetServer web_server = startWebServer(*mHandler);
    // log_info("Launching CivetWeb HTTP server in HTTP thread");

    g_arbitration.tsc_rate = spdk_get_ticks_hz();

    if (register_workers() != 0) {
        rc = 1;
        zstore_cleanup();
        return rc;
    }

    struct arb_context ctx = {};
    if (register_controllers(&ctx) != 0) {
        rc = 1;
        zstore_cleanup();
        return rc;
    }

    if (associate_workers_with_ns() != 0) {
        rc = 1;
        zstore_cleanup();
        return rc;
    }

    if (init_ns_worker_ctx(mWorker->ns_ctx, mWorker->qprio) != 0) {
        log_error("init_ns_worker_ctx() failed");
        return 1;
        // }
    }

    snprintf(task_pool_name, sizeof(task_pool_name), "task_pool_%d", getpid());

    /*
     * The task_count will be dynamically calculated based on the
     * number of attached active namespaces, queue depth and number
     * of cores (workers) involved in the IO perations.
     */
    task_count = g_arbitration.num_namespaces > g_arbitration.num_workers
                     ? g_arbitration.num_namespaces
                     : g_arbitration.num_workers;
    // task_count *= g_arbitration.queue_depth;
    task_count *= mQueueDepth;
    SetTaskCount(task_count);

    log_info("Creating task pool: name {}, count {}", task_pool_name,
             task_count);
    mTaskPool =
        spdk_mempool_create(task_pool_name, task_count, sizeof(struct arb_task),
                            0, SPDK_ENV_SOCKET_ID_ANY);
    if (mTaskPool == NULL) {
        log_error("could not initialize task pool");
        rc = 1;
        zstore_cleanup();
        return rc;
    }

    stime = std::chrono::high_resolution_clock::now();
    log_info("ZstoreController Init finish");
    return rc;
}

void ZstoreController::ReadInDispatchThread(RequestContext *ctx) {}

void ZstoreController::WriteInDispatchThread(RequestContext *ctx) {}

void ZstoreController::CheckIoQpair(std::string msg)
{
    assert(mWorker != nullptr);
    assert(mWorker->ns_ctx != nullptr);
    assert(mWorker->ns_ctx->qpair != nullptr);
    assert(spdk_nvme_qpair_is_connected(mWorker->ns_ctx->qpair));
    log_debug("{}, qpair connected: {}", msg,
              spdk_nvme_qpair_is_connected(mWorker->ns_ctx->qpair));
}

struct spdk_nvme_qpair *ZstoreController::GetIoQpair()
{
    assert(mWorker != nullptr);
    assert(mWorker->ns_ctx != nullptr);
    assert(mWorker->ns_ctx->qpair != nullptr);
    // assert(spdk_nvme_qpair_is_connected(mWorker->ns_ctx->qpair));

    return mWorker->ns_ctx->qpair;
}

void ZstoreController::CheckTaskPool(std::string msg)
{
    assert(mTaskPool != nullptr);
    auto task = (struct arb_task *)spdk_mempool_get(mTaskPool);
    if (!task) {
        log_error("Failed to get task from mTaskPool: {}", msg);
        exit(1);
    }
    spdk_mempool_put(mTaskPool, task);

    log_info("{}: TaskPool ok: {}", msg, spdk_mempool_count(mTaskPool));
}

ZstoreController::~ZstoreController()
{
    thread_send_msg(mIoThread.thread, quit, nullptr);
    thread_send_msg(mDispatchThread, quit, nullptr);
    // thread_send_msg(mHttpThread, quit, nullptr);
    // thread_send_msg(mIndexThread, quit, nullptr);
    // thread_send_msg(mCompletionThread, quit, nullptr);
    log_debug("drain io: {}", spdk_get_ticks());
    drain_io(this);
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
    print_stats(this);
}

void ZstoreController::register_ns(struct spdk_nvme_ctrlr *ctrlr,
                                   struct spdk_nvme_ns *ns)
{
    const struct spdk_nvme_ctrlr_data *cdata;

    cdata = spdk_nvme_ctrlr_get_data(ctrlr);

    if (spdk_nvme_ns_get_size(ns) < g_arbitration.io_size_bytes ||
        spdk_nvme_ns_get_extended_sector_size(ns) >
            g_arbitration.io_size_bytes ||
        g_arbitration.io_size_bytes %
            spdk_nvme_ns_get_extended_sector_size(ns)) {
        printf("WARNING: controller %-20.20s (%-20.20s) ns %u has invalid "
               "ns size %" PRIu64 " / block size %u for I/O size %u\n",
               cdata->mn, cdata->sn, spdk_nvme_ns_get_id(ns),
               spdk_nvme_ns_get_size(ns),
               spdk_nvme_ns_get_extended_sector_size(ns),
               g_arbitration.io_size_bytes);
        return;
    }

    mNamespace = (struct ns_entry *)malloc(sizeof(struct ns_entry));
    if (mNamespace == NULL) {
        perror("ns_entry malloc");
        exit(1);
    }

    mNamespace->nvme.ctrlr = ctrlr;
    mNamespace->nvme.ns = ns;

    mNamespace->size_in_ios =
        spdk_nvme_ns_get_size(ns) / g_arbitration.io_size_bytes;
    mNamespace->io_size_blocks =
        g_arbitration.io_size_bytes / spdk_nvme_ns_get_sector_size(ns);

    snprintf(mNamespace->name, 44, "%-20.20s (%-20.20s)", cdata->mn, cdata->sn);

    g_arbitration.num_namespaces++;
    // mNamespace = ;
}

void ZstoreController::register_ctrlr(struct spdk_nvme_ctrlr *ctrlr)
{
    uint32_t nsid;
    struct spdk_nvme_ns *ns;
    // struct ctrlr_entry *entry =
    mController = (struct ctrlr_entry *)calloc(1, sizeof(struct ctrlr_entry));
    union spdk_nvme_cap_register cap = spdk_nvme_ctrlr_get_regs_cap(ctrlr);
    const struct spdk_nvme_ctrlr_data *cdata = spdk_nvme_ctrlr_get_data(ctrlr);

    if (mController == NULL) {
        perror("ctrlr_entry malloc");
        exit(1);
    }

    snprintf(mController->name, sizeof(mController->name),
             "%-20.20s (%-20.20s)", cdata->mn, cdata->sn);

    // entry->ctrlr = ctrlr;
    // mController = entry;

    // if ((g_arbitration.latency_tracking_enable != 0) &&
    //     spdk_nvme_ctrlr_is_feature_supported(
    //         ctrlr, SPDK_NVME_INTEL_FEAT_LATENCY_TRACKING)) {
    //     set_latency_tracking_feature(ctrlr, true);
    // }

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
            log_info("ns {} is zns ns", nsid);
        }
        register_ns(ctrlr, ns);
    }

    // TODO log and store stats

    auto zone_size_sectors = spdk_nvme_zns_ns_get_zone_size_sectors(ns);
    auto zone_size_bytes = spdk_nvme_zns_ns_get_zone_size(ns);
    auto num_zones = spdk_nvme_zns_ns_get_num_zones(ns);
    uint32_t max_open_zones = spdk_nvme_zns_ns_get_max_open_zones(ns);
    uint32_t active_zones = spdk_nvme_zns_ns_get_max_active_zones(ns);
    uint32_t max_zone_append_size =
        spdk_nvme_zns_ctrlr_get_max_zone_append_size(ctrlr);

    log_info("Zone size: sectors {}, bytes {}", zone_size_sectors,
             zone_size_bytes);
    log_info("Zones: num {}, max open {}, active {}", num_zones, max_open_zones,
             active_zones);
    log_info("Max zones append size: {}", max_zone_append_size);
}

int ZstoreController::register_workers()
{
    uint32_t i = 1;
    // struct worker_thread *worker;
    enum spdk_nvme_qprio qprio = SPDK_NVME_QPRIO_URGENT;

    mWorker = (struct worker_thread *)calloc(1, sizeof(*mWorker));
    if (mWorker == NULL) {
        log_error("Unable to allocate worker");
        return -1;
    }

    // TAILQ_INIT(&worker->ns_ctx);
    mWorker->lcore = i;
    // TAILQ_INSERT_TAIL(&mWorkers, worker, link);
    g_arbitration.num_workers++;

    if (g_arbitration.arbitration_mechanism == SPDK_NVME_CAP_AMS_WRR) {
        qprio = static_cast<enum spdk_nvme_qprio>(static_cast<int>(qprio) + 1);
    }

    mWorker->qprio = static_cast<enum spdk_nvme_qprio>(
        qprio & SPDK_NVME_CREATE_IO_SQ_QPRIO_MASK);
    // }

    return 0;
}

void ZstoreController::zns_dev_init(struct arb_context *ctx, std::string ip1,
                                    std::string port1)
{
    int rc = 0;

    log_debug("zns dev");
    // 1. connect nvmf device
    struct spdk_nvme_transport_id trid1 = {};
    snprintf(trid1.traddr, sizeof(trid1.traddr), "%s", ip1.c_str());
    snprintf(trid1.trsvcid, sizeof(trid1.trsvcid), "%s", port1.c_str());
    snprintf(trid1.subnqn, sizeof(trid1.subnqn), "%s", g_hostnqn);
    trid1.adrfam = SPDK_NVMF_ADRFAM_IPV4;

    trid1.trtype = SPDK_NVME_TRANSPORT_TCP;
    // trid1.trtype = SPDK_NVME_TRANSPORT_RDMA;

    // struct spdk_nvme_transport_id trid2 = {};
    // snprintf(trid2.traddr, sizeof(trid2.traddr), "%s", ip2.c_str());
    // snprintf(trid2.trsvcid, sizeof(trid2.trsvcid), "%s", port2.c_str());
    // snprintf(trid2.subnqn, sizeof(trid2.subnqn), "%s", g_hostnqn);
    // trid2.adrfam = SPDK_NVMF_ADRFAM_IPV4;
    // trid2.trtype = SPDK_NVME_TRANSPORT_TCP;

    struct spdk_nvme_ctrlr_opts opts;
    spdk_nvme_ctrlr_get_default_ctrlr_opts(&opts, sizeof(opts));
    memcpy(opts.hostnqn, g_hostnqn, sizeof(opts.hostnqn));

    register_ctrlr(spdk_nvme_connect(&trid1, &opts, sizeof(opts)));
    // register_ctrlr(spdk_nvme_connect(&trid2, &opts, sizeof(opts)));

    log_info("Found {} namspaces", g_arbitration.num_namespaces);
}

int ZstoreController::register_controllers(struct arb_context *ctx)
{
    log_info("Initializing NVMe Controllers");

    // RDMA
    // zns_dev_init(ctx, "192.168.100.9", "5520");
    // TCP
    zns_dev_init(ctx, "12.12.12.2", "5520");

    if (g_arbitration.num_namespaces == 0) {
        log_error("No valid namespaces to continue IO testing");
        return 1;
    }

    return 0;
}

void ZstoreController::unregister_controllers()
{
    // struct ctrlr_entry *entry, *tmp;
    struct spdk_nvme_detach_ctx *detach_ctx = NULL;

    // TAILQ_FOREACH_SAFE(entry, &mControllers, link, tmp)
    // {
    //     TAILQ_REMOVE(&mControllers, entry, link);

    spdk_nvme_detach_async(mController->ctrlr, &detach_ctx);
    free(mController);
    // }

    while (detach_ctx && spdk_nvme_detach_poll_async(detach_ctx) == -EAGAIN) {
        ;
    }
}

int ZstoreController::associate_workers_with_ns()
{
    // struct ns_entry *entry = mNamespace;
    // struct worker_thread *worker = mWorker;
    // struct ns_worker_ctx *ns_ctx;
    int count;

    count = g_arbitration.num_namespaces > g_arbitration.num_workers
                ? g_arbitration.num_namespaces
                : g_arbitration.num_workers;
    count = 1;
    log_debug("DEBUG ns {}, workers {}, count {}", g_arbitration.num_namespaces,
              g_arbitration.num_workers, count);
    // for (i = 0; i < count; i++) {
    if (mNamespace == NULL) {
        return -1;
    }

    assert(mWorker != nullptr);
    mWorker->ns_ctx =
        (struct ns_worker_ctx *)malloc(sizeof(struct ns_worker_ctx));
    if (!mWorker->ns_ctx) {
        log_error("Alloc ns worker failed ");
        return 1;
    }
    memset(mWorker->ns_ctx, 0, sizeof(*mWorker->ns_ctx));

    log_info("Associating {} with lcore {}", mNamespace->name, mWorker->lcore);
    // FIXME redundent?
    mWorker->ns_ctx->entry = mNamespace;
    //
    // TAILQ_INSERT_TAIL(&worker->ns_ctx, ns_ctx, link);

    // worker = TAILQ_NEXT(worker, link);
    // if (worker == NULL) {
    //     worker = TAILQ_FIRST(&mWorkers);
    // }

    // entry = TAILQ_NEXT(entry, link);
    // if (entry == NULL) {
    //     entry = TAILQ_FIRST(&mNamespaces);
    // }
    // }

    // zctrlr->mWorker->ns_ctx = ns_ctx;
    return 0;
}

void ZstoreController::zstore_cleanup()
{
    log_info("unreg controllers");
    unregister_controllers();
    log_info("cleanup ");
    cleanup(mTaskCount);

    spdk_env_fini();

    // if (rc != 0) {
    //     fprintf(stderr, "%s: errors occurred\n", argv[0]);
    // }
}

void ZstoreController::cleanup_ns_worker_ctx()
{
    log_info("here");
    thread_send_msg(mIoThread.thread, quit, nullptr);
    spdk_nvme_ctrlr_free_io_qpair(mWorker->ns_ctx->qpair);
}

void ZstoreController::cleanup(uint32_t task_count)
{
    free(mNamespace);

    free(mWorker->ns_ctx);
    free(mWorker);

    if (spdk_mempool_count(mTaskPool) != (size_t)task_count) {
        log_error("mTaskPool count is {} but should be {}",
                  spdk_mempool_count(mTaskPool), task_count);
    }
    spdk_mempool_free(mTaskPool);
}

static const char *print_qprio(enum spdk_nvme_qprio qprio)
{
    switch (qprio) {
    case SPDK_NVME_QPRIO_URGENT:
        return "urgent priority queue";
    case SPDK_NVME_QPRIO_HIGH:
        return "high priority queue";
    case SPDK_NVME_QPRIO_MEDIUM:
        return "medium priority queue";
    case SPDK_NVME_QPRIO_LOW:
        return "low priority queue";
    default:
        return "invalid priority queue";
    }
}

int ZstoreController::init_ns_worker_ctx(struct ns_worker_ctx *ns_ctx,
                                         enum spdk_nvme_qprio qprio)
{
    log_info("Starting thread with {}", print_qprio(qprio));

    // TODO dont need ns_ctx anymore
    assert(mWorker != nullptr);
    assert(mWorker->ns_ctx != nullptr);
    assert(mWorker->ns_ctx->entry != nullptr);
    assert(mWorker->ns_ctx->entry->nvme.ctrlr != nullptr);
    struct spdk_nvme_ctrlr *ctrlr = mWorker->ns_ctx->entry->nvme.ctrlr;

    struct spdk_nvme_io_qpair_opts opts;
    spdk_nvme_ctrlr_get_default_io_qpair_opts(ctrlr, &opts, sizeof(opts));
    opts.qprio = qprio;

    mWorker->ns_ctx->qpair =
        spdk_nvme_ctrlr_alloc_io_qpair(ctrlr, &opts, sizeof(opts));
    if (!ns_ctx->qpair) {
        log_error("ERROR: spdk_nvme_ctrlr_alloc_io_qpair failed");
        return 1;
    }

    // allocate space for times
    // mWorker->ns_ctx->stimes.reserve(1000000);
    // mWorker->ns_ctx->etimes.reserve(1000000);

    // zctrlr->mWorker->ns_ctx->zctrlr = zctrlr;

    return 0;
}
