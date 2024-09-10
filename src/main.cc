#include "include/zns_utils.h"
#include "include/zstore_controller.h"
#include "spdk/env.h"
#include "zstore_controller.cc"
#include <bits/stdc++.h>
#include <cstdint>
#include <cstdlib>
#include <fmt/core.h>
#include <stdio.h>
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

    struct worker_thread *worker, *main_worker;
    unsigned main_core;
    char task_pool_name[30];
    uint32_t task_count = 0;
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

    log_info("1111");
    gZstoreController = new ZstoreController();
    gZstoreController->Init(false);

    g_arbitration.tsc_rate = spdk_get_ticks_hz();

    log_info("1");
    if (register_workers(gZstoreController) != 0) {
        rc = 1;
        zstore_cleanup(task_count, gZstoreController);
        return rc;
    }

    log_info("222");
    struct arb_context ctx = {};
    if (register_controllers(&ctx, gZstoreController) != 0) {
        rc = 1;
        zstore_cleanup(task_count, gZstoreController);
        return rc;
    }

    log_info("333");
    if (associate_workers_with_ns(gZstoreController) != 0) {
        rc = 1;
        zstore_cleanup(task_count, gZstoreController);
        return rc;
    }

    if (init_ns_worker_ctx(gZstoreController->mWorker->ns_ctx,
                           gZstoreController->mWorker->qprio,
                           gZstoreController) != 0) {
        log_error("init_ns_worker_ctx() failed");
        return 1;
        // }
    }

    gZstoreController->CheckIoQpair("");

    gZstoreController->initIoThread();
    gZstoreController->initHttpThread();

    snprintf(task_pool_name, sizeof(task_pool_name), "task_pool_%d", getpid());

    /*
     * The task_count will be dynamically calculated based on the
     * number of attached active namespaces, queue depth and number
     * of cores (workers) involved in the IO perations.
     */
    task_count = g_arbitration.num_namespaces > g_arbitration.num_workers
                     ? g_arbitration.num_namespaces
                     : g_arbitration.num_workers;
    task_count *= g_arbitration.queue_depth;

    log_info("Creating task pool: name {}, count {}", task_pool_name,
             task_count);
    gZstoreController->mTaskPool =
        spdk_mempool_create(task_pool_name, task_count, sizeof(struct arb_task),
                            0, SPDK_ENV_SOCKET_ID_ANY);
    if (gZstoreController->mTaskPool == NULL) {
        log_error("could not initialize task pool");
        rc = 1;
        zstore_cleanup(task_count, gZstoreController);
        return rc;
    }
    gZstoreController->CheckTaskPool("Task pool created");

    // Create poll groups for the io threads and perform initialization
    // gZstoreController->mIoThread.group =
    //     spdk_nvme_poll_group_create(NULL, NULL);
    // gZstoreController->mIoThread.controller = gZstoreController;

    // struct spdk_nvme_qpair *ioQueues =
    // gZstoreController->mWorker->ns_ctx->qpair;
    // spdk_nvme_ctrlr_disconnect_io_qpair(*ioQueues);
    // rc = spdk_nvme_poll_group_add(gZstoreController->mIoThread.group,
    // ioQueues); assert(rc == 0); if
    // (spdk_nvme_ctrlr_connect_io_qpair(gZstoreController->mController->ctrlr,
    //                                      *ioQueues) < 0) {
    //     printf("Connect ctrl failed!\n");
    // }

    // gZstoreController->initIoThread();

    print_configuration(argv[0]);

    log_info("Initialization complete. Launching workers.");

    // while (1) {
    //     sleep(1);
    // }

    // ==================================

    main_worker = NULL;
    assert(main_worker == NULL);
    main_worker = gZstoreController->mWorker;
    assert(main_worker != NULL);
    rc = work_fn(gZstoreController);

    spdk_env_thread_wait_all();

    log_info("xxxx");

    log_info("xxxx");
    zstore_cleanup(task_count, gZstoreController);
    return rc;
}
