#include "include/common.h"
#include "include/zns_utils.h"
#include "spdk/env.h"
#include "spdk/log.h"
#include "spdk/nvme.h"
#include "spdk/nvme_intel.h"
#include "spdk/string.h"
#include <bits/stdc++.h>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <fmt/core.h>
#include <stdio.h>
#include <vector>

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

    g_arbitration.tsc_rate = spdk_get_ticks_hz();

    if (register_workers() != 0) {
        rc = 1;
        zstore_cleanup(task_count);
        return rc;
    }

    struct arb_context ctx = {};
    if (register_controllers(&ctx) != 0) {
        rc = 1;
        zstore_cleanup(task_count);
        return rc;
    }

    if (associate_workers_with_ns() != 0) {
        rc = 1;
        zstore_cleanup(task_count);
        return rc;
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
    task_count *= g_arbitration.queue_depth;

    task_pool =
        spdk_mempool_create(task_pool_name, task_count, sizeof(struct arb_task),
                            0, SPDK_ENV_SOCKET_ID_ANY);
    if (task_pool == NULL) {
        fprintf(stderr, "could not initialize task pool\n");
        rc = 1;
        zstore_cleanup(task_count);
        return rc;
    }

    print_configuration(argv[0]);

    printf("Initialization complete. Launching workers.\n");

    /* Launch all of the secondary workers */
    main_core = spdk_env_get_current_core();
    main_worker = NULL;
    // TAILQ_FOREACH(worker, &g_workers, link)
    // {
    //     if (worker->lcore != main_core) {
    //         spdk_env_thread_launch_pinned(worker->lcore, work_fn, worker);
    //     } else {
    // assert(main_worker == NULL);
    // main_worker = g_worker;
    // }
    // }
    assert(main_worker == NULL);
    main_worker = g_worker;
    assert(main_worker != NULL);
    rc = work_fn(main_worker);

    spdk_env_thread_wait_all();

    print_stats();

    zstore_cleanup(task_count);
    return rc;
}
