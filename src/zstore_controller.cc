#include "include/zstore_controller.h"
#include "common.cc"
#include "include/common.h"
#include "include/configuration.h"
#include "include/request_handler.h"
#include "include/utils.hpp"
#include <algorithm>
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
#include <tuple>

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
    log_info("Create HTTP thread {} {}", spdk_thread_get_name(mHttpThread),
             spdk_thread_get_id(mHttpThread));
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
    log_info("Create completion thread {} {}",
             spdk_thread_get_name(mCompletionThread),
             spdk_thread_get_id(mCompletionThread));
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
    log_info("Create dispatch thread {} {}",
             spdk_thread_get_name(mDispatchThread),
             spdk_thread_get_id(mDispatchThread));
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
    log_debug("init Io thread {}", Configuration::GetNumIoThreads());
    // auto threadId = 0;
    // log_debug("init Io thread {}", threadId);
    spdk_cpuset_zero(&cpumask);
    spdk_cpuset_set_cpu(&cpumask, Configuration::GetIoThreadCoreId(), true);
    mIoThread.thread = spdk_thread_create("IoThread", &cpumask);
    assert(mIoThread.thread != nullptr);
    mIoThread.controller = this;
    int rc = spdk_env_thread_launch_pinned(Configuration::GetIoThreadCoreId(),
                                           ioWorker, this);
    if (rc < 0) {
        log_error("Failed to launch IO thread error: {} {}", strerror(rc),
                  spdk_strerror(rc));
    }
}

// struct spdk_nvme_qpair *GetIoQpair() { return g_devices[0]->GetIoQueue(0); }

void ZstoreController::Init(bool need_env)
{
    int rc = 0;

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

    SetQueuDepth(64);
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

    log_info("ZstoreController Init finish");
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
    // thread_send_msg(mIoThread.thread, quit, nullptr);
    // thread_send_msg(mDispatchThread, quit, nullptr);
    // thread_send_msg(mHttpThread, quit, nullptr);
    // thread_send_msg(mIndexThread, quit, nullptr);
    // thread_send_msg(mCompletionThread, quit, nullptr);
}
