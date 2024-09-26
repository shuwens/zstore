#include "include/zns_utils.h"
#include "include/zstore_controller.h"
#include "spdk/env.h"
#include "src/include/global.h"
#include "zstore_controller.cc"
#include <bits/stdc++.h>
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <fmt/core.h>
#include <sys/time.h>

void create_dummy_objects(Zstore zstore)
{
    log_info("Create dummy objects in table: foo, bar, test");

    zstore.putObject("foo", "foo_data");
    zstore.putObject("bar", "bar_data");
    zstore.putObject("baz", "baz_data");
}

int main(int argc, char **argv)
{
    int rc;

    struct spdk_env_opts opts;

    rc = parse_args(argc, argv);
    if (rc != 0) {
        return rc;
    }

    spdk_env_opts_init(&opts);
    opts.name = "arb";
    opts.mem_size = g_dpdk_mem;
    opts.hugepage_single_segments = g_dpdk_mem_single_seg;
    opts.core_mask = g_arbitration.core_mask;
    opts.shm_id = g_arbitration.shm_id;
    if (spdk_env_init(&opts) < 0) {
        return 1;
    }

    rc = spdk_thread_lib_init(nullptr, 0);
    if (rc < 0) {
        log_error("Unable to initialize SPDK thread lib.");
        return 1;
    }

    gZstoreController = new ZstoreController();
    gZstoreController->Init(false);

    print_configuration(argv[0]);

    log_info("Initialization complete. Launching workers.");

    gZstoreController->CheckIoQpair("Starting all the threads");

    gZstoreController->initIoThread();

    gZstoreController->initHttpThread();

    // while (1) {
    // }

    bool use_object = false;
    gZstoreController->initDispatchThread(use_object);

    // ==================================

    // worker->ns_ctx->current_queue_depth = 0;
    gZstoreController->stime = std::chrono::high_resolution_clock::now();
    // while (!worker->ns_ctx->is_draining &&
    //        gZstoreController->mRequestContextPool->availableContexts.size() >
    //            0) {
    //     // while (1) {
    //
    //     // for (int i = 0; i < 100; i++) {
    //     //
    //     &&!gZstoreController->mRequestContextPool->availableContexts.empty())
    //     // {
    //     // if (gZstoreController->verbose)
    //     // if (worker->ns_ctx->io_completed % 1000 == 0)
    //     log_debug(
    //         "queue depth {}, req in flight {}, read q size {},  "
    //         "completed {},  avalable ctx {} ",
    //         gZstoreController->GetQueueDepth(),
    //         (gZstoreController->mRequestContextPool->capacity -
    //          gZstoreController->mRequestContextPool->availableContexts.size()),
    //         gZstoreController->GetReadQueueSize(),
    //         worker->ns_ctx->io_completed,
    //         gZstoreController->mRequestContextPool->availableContexts.size());
    //
    //     RequestContext *slot =
    //         gZstoreController->mRequestContextPool->GetRequestContext(true);
    //     slot->ctrl = gZstoreController;
    //     assert(slot->ctrl == gZstoreController);
    //
    //     auto ioCtx = slot->ioContext;
    //     auto offset_in_ios = rand_r(&seed) % entry->size_in_ios;
    //
    //     ioCtx.ns = entry->nvme.ns;
    //     ioCtx.qpair = worker->ns_ctx->qpair;
    //     ioCtx.data = slot->dataBuffer;
    //     ioCtx.offset = offset_in_ios * entry->io_size_blocks;
    //     // ioCtx.offset = 0;
    //     ioCtx.size = entry->io_size_blocks;
    //     ioCtx.cb = complete;
    //     ioCtx.ctx = slot;
    //     ioCtx.flags = 0;
    //     slot->ioContext = ioCtx;
    //
    //     // if (g_arbitration.is_random) {
    //     // } else {
    //     //     offset_in_ios = worker->ns_ctx->offset_in_ios++;
    //     //     if (worker->ns_ctx->offset_in_ios == entry->size_in_ios) {
    //     //         worker->ns_ctx->offset_in_ios = 0;
    //     //     }
    //     // }
    //
    //     // log_debug("Before READ {}", offset_in_ios *
    //     // entry->io_size_blocks);
    //
    //     // thread_send_msg(zctrlr->GetIoThread(), zoneRead, slot);
    //
    //     if (gZstoreController->verbose)
    //         log_debug("Before READ: read q {}, io completed {}",
    //                   gZstoreController->GetReadQueueSize(),
    //                   gZstoreController->GetWorker()->ns_ctx->io_completed);
    //     assert(slot->ioContext.cb != nullptr);
    //     gZstoreController->EnqueueRead(slot);
    // }
    // std::this_thread::sleep_for(boost_swap_impl::chrono::microseconds(1));
    // }
    // struct worker_thread *worker, *main_worker;
    // unsigned main_core;
    // main_worker = NULL;
    // assert(main_worker == NULL);
    // main_worker = gZstoreController->GetWorker();
    // assert(main_worker != NULL);
    // rc = work_fn(gZstoreController);

    // log_debug("XXXX");
    spdk_env_thread_wait_all();

    // log_debug("XXXX");
    gZstoreController->zstore_cleanup();

    // log_debug("XXXX");
    return rc;
}
