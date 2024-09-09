#include "include/zstore_controller.h"
// #include "common.cc"
// #include "device.cc"
#include "include/common.h"
// #include "include/device.h"
// #include "include/segment.h"
#include "include/utils.hpp"
// #include "messages_and_functions.cc"
// #include "segment.cc"
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

static auto quit(void *args) { exit(0); }

// void ZstoreController::initHttpThread()
// {
//     struct spdk_cpuset cpumask;
//     spdk_cpuset_zero(&cpumask);
//     spdk_cpuset_set_cpu(&cpumask, Configuration::GetHttpThreadCoreId(),
//     true); mHttpThread = spdk_thread_create("HttpThread", &cpumask);
//     printf("Create HTTP processing thread %s %lu\n",
//            spdk_thread_get_name(mHttpThread),
//            spdk_thread_get_id(mHttpThread));
//     int rc =
//     spdk_env_thread_launch_pinned(Configuration::GetHttpThreadCoreId(),
//                                            ecWorker, this);
//     if (rc < 0) {
//         printf("Failed to launch ec thread error: %s\n", spdk_strerror(rc));
//     }
// }
//
// void ZstoreController::initIndexThread()
// {
//     struct spdk_cpuset cpumask;
//     spdk_cpuset_zero(&cpumask);
//     spdk_cpuset_set_cpu(&cpumask, Configuration::GetIndexThreadCoreId(),
//     true); mIndexThread = spdk_thread_create("IndexThread", &cpumask);
//     printf("Create index and completion thread %s %lu\n",
//            spdk_thread_get_name(mIndexThread),
//            spdk_thread_get_id(mIndexThread));
//     int rc = spdk_env_thread_launch_pinned(
//         Configuration::GetIndexThreadCoreId(), indexWorker, this);
//     if (rc < 0) {
//         printf("Failed to launch index completion thread, error: %s\n",
//                spdk_strerror(rc));
//     }
// }
//
// void ZstoreController::initCompletionThread()
// {
//     struct spdk_cpuset cpumask;
//     spdk_cpuset_zero(&cpumask);
//     spdk_cpuset_set_cpu(&cpumask, Configuration::GetCompletionThreadCoreId(),
//                         true);
//     mCompletionThread = spdk_thread_create("CompletionThread", &cpumask);
//     printf("Create index and completion thread %s %lu\n",
//            spdk_thread_get_name(mCompletionThread),
//            spdk_thread_get_id(mCompletionThread));
//     int rc = spdk_env_thread_launch_pinned(
//         Configuration::GetCompletionThreadCoreId(), completionWorker, this);
//     if (rc < 0) {
//         printf("Failed to launch completion thread, error: %s\n",
//                spdk_strerror(rc));
//     }
// }
//
// void ZstoreController::initDispatchThread()
// {
//     struct spdk_cpuset cpumask;
//     spdk_cpuset_zero(&cpumask);
//     spdk_cpuset_set_cpu(&cpumask, Configuration::GetDispatchThreadCoreId(),
//                         true);
//     mDispatchThread = spdk_thread_create("DispatchThread", &cpumask);
//     printf("Create dispatch thread %s %lu\n",
//            spdk_thread_get_name(mDispatchThread),
//            spdk_thread_get_id(mDispatchThread));
//     int rc = spdk_env_thread_launch_pinned(
//         Configuration::GetDispatchThreadCoreId(), dispatchWorker, this);
//     if (rc < 0) {
//         printf("Failed to launch dispatch thread error: %s %s\n",
//         strerror(rc),
//                spdk_strerror(rc));
//     }
// }
//
// void ZstoreController::initIoThread()
// {
//     struct spdk_cpuset cpumask;
//     log_debug("init Io thread {}", Configuration::GetNumIoThreads());
//     auto threadId = 0;
//     log_debug("init Io thread {}", threadId);
//     spdk_cpuset_zero(&cpumask);
//     spdk_cpuset_set_cpu(&cpumask, Configuration::GetIoThreadCoreId(threadId),
//                         true);
//     mIoThread[threadId].thread = spdk_thread_create("IoThread", &cpumask);
//     assert(mIoThread[threadId].thread != nullptr);
//     mIoThread[threadId].controller = this;
//     log_debug("here: thread {}", threadId);
//     int rc = spdk_env_thread_launch_pinned(
//         Configuration::GetIoThreadCoreId(threadId), ioWorker, this);
//     // &mIoThread[threadId]);
//     log_info("Zstore io thread {} {}",
//              spdk_thread_get_name(mIoThread[threadId].thread),
//              spdk_thread_get_id(mIoThread[threadId].thread));
//     if (rc < 0) {
//         log_error("Failed to launch IO thread error: {} {}", strerror(rc),
//                   spdk_strerror(rc));
//     }
// }

// void ZstoreController::initIoThread()
// {
//     struct spdk_cpuset cpumask;
//     if (verbose)
//         log_debug("init Io thread {}", Configuration::GetNumIoThreads());
//     for (uint32_t threadId = 0; threadId < Configuration::GetNumIoThreads();
//          ++threadId) {
//         log_debug("init Io thread {}", threadId);
//         spdk_cpuset_zero(&cpumask);
//         spdk_cpuset_set_cpu(&cpumask,
//                             Configuration::GetIoThreadCoreId(threadId),
//                             true);
//         mIoThread[threadId].thread = spdk_thread_create("IoThread",
//         &cpumask); assert(mIoThread[threadId].thread != nullptr);
//         mIoThread[threadId].controller = this;
//         log_debug("here: thread {}", threadId);
//         int rc = spdk_env_thread_launch_pinned(
//             Configuration::GetIoThreadCoreId(threadId), ioWorker,
//             &mIoThread[threadId]);
//         log_info("Zstore io thread {} {}",
//                  spdk_thread_get_name(mIoThread[threadId].thread),
//                  spdk_thread_get_id(mIoThread[threadId].thread));
//         if (rc < 0) {
//             log_error("Failed to launch IO thread error: {} {}",
//             strerror(rc),
//                       spdk_strerror(rc));
//         }
//     }
// }

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

    // initIoThread();
    // initDispatchThread();
    // initIndexThread();
    // initCompletionThread();
    // initHttpThread();

    log_info("ZstoreController Init finish");
}

ZstoreController::~ZstoreController()
{
    // Dump();

    // delete mAddressMap;
    // if (!Configuration::GetEventFrameworkEnabled()) {
    // for (uint32_t i = 0; i < Configuration::GetNumIoThreads(); ++i) {
    //     thread_send_msg(mIoThread[i].thread, quit, nullptr);
    // }
    // thread_send_msg(mDispatchThread, quit, nullptr);
    // thread_send_msg(mHttpThread, quit, nullptr);
    // thread_send_msg(mIndexThread, quit, nullptr);
    // thread_send_msg(mCompletionThread, quit, nullptr);
    // }
}
