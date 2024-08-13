#include "include/utils.hpp"
#include "include/zns_device.h"
#include "spdk/env.h"
#include "spdk/nvme.h"
#include <bits/stdc++.h>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <fmt/core.h>
#include <fstream>
#include <iostream>
#include <stdio.h>
#include <vector>

static void task_complete(struct zstore_task *task);

static void io_complete(void *ctx, const struct spdk_nvme_cpl *completion);

int write_zstore_pattern(char **pattern, struct ns_worker_ctx *ns_ctx,
                         int32_t size, char *test_str, int value)
{
    if (*pattern != NULL) {
        z_free(ns_ctx, *pattern);
    }
    *pattern = (char *)z_calloc(ns_ctx, ns_ctx->info.lba_size, sizeof(char *));
    if (*pattern == NULL) {
        return 1;
    }
    snprintf(*pattern, ns_ctx->info.lba_size, "%s:%d", test_str, value);
    return 0;
}

static __thread unsigned int seed = 0;

static void submit_single_io(struct ns_worker_ctx *ns_ctx)
{
    struct zstore_task *task = NULL;
    uint64_t offset_in_ios;
    int rc;
    struct ns_entry *entry = ns_ctx->entry;

    task = static_cast<struct zstore_task *>(spdk_mempool_get(task_pool));
    if (!task) {
        fprintf(stderr, "Failed to get task from task_pool\n");
        exit(1);
    }

    task->buf = spdk_dma_zmalloc(g_zstore.io_size_bytes, 0x200, NULL);
    if (!task->buf) {
        spdk_mempool_put(task_pool, task);
        fprintf(stderr, "task->buf spdk_dma_zmalloc failed\n");
        exit(1);
    }

    task->ns_ctx = ns_ctx;

    offset_in_ios = ns_ctx->offset_in_ios++;
    if (ns_ctx->offset_in_ios == entry->size_in_ios) {
        ns_ctx->offset_in_ios = 0;
    }

    // if ((g_zstore.rw_percentage == 100) ||
    //     (g_zstore.rw_percentage != 0 &&
    //      ((rand_r(&seed) % 100) < g_zstore.rw_percentage))) {
    //
    log_debug("read ");
    rc = spdk_nvme_ns_cmd_read(entry->nvme.ns, ns_ctx->qpair, task->buf,
                               offset_in_ios * entry->io_size_blocks,
                               entry->io_size_blocks, io_complete, task, 0);
    // } else {
    //     log_debug("write");
    //     rc =
    //         spdk_nvme_ns_cmd_write(entry->nvme.ns, ns_ctx->qpair, task->buf,
    //                                offset_in_ios * entry->io_size_blocks,
    //                                entry->io_size_blocks, io_complete, task,
    //                                0);
    // }

    if (rc != 0) {
        fprintf(stderr, "starting I/O failed\n");
    } else {
        ns_ctx->current_queue_depth++;
    }
}

static void task_complete(struct zstore_task *task)
{
    struct ns_worker_ctx *ns_ctx;

    ns_ctx = task->ns_ctx;
    ns_ctx->current_queue_depth--;
    ns_ctx->io_completed++;

    spdk_dma_free(task->buf);
    spdk_mempool_put(task_pool, task);

    /*
     * is_draining indicates when time has expired for the test run
     * and we are just waiting for the previously submitted I/O
     * to complete.  In this case, do not submit a new I/O to replace
     * the one just completed.
     */
    if (!ns_ctx->is_draining) {
        submit_single_io(ns_ctx);
    }
}

static void io_complete(void *ctx, const struct spdk_nvme_cpl *completion)
{
    task_complete((struct zstore_task *)ctx);
}

static void check_io(struct ns_worker_ctx *ns_ctx)
{
    spdk_nvme_qpair_process_completions(ns_ctx->qpair,
                                        g_zstore.max_completions);
}

static void submit_io(struct ns_worker_ctx *ns_ctx, int queue_depth)
{
    while (queue_depth-- > 0) {
        submit_single_io(ns_ctx);
    }
}

static void drain_io(struct ns_worker_ctx *ns_ctx)
{
    ns_ctx->is_draining = true;
    while (ns_ctx->current_queue_depth > 0) {
        check_io(ns_ctx);
    }
}

static int work_fn(void *arg)
{
    log_debug("workfn");
    uint64_t tsc_end;
    struct worker_thread *worker = (struct worker_thread *)arg;
    struct ns_worker_ctx *ns_ctx;

    log_info("Starting thread on core {}", worker->lcore);
    // printf("Starting thread on core %u with %s\n", worker->lcore,
    //        print_qprio(worker->qprio));

    /* Allocate a queue pair for each namespace. */
    TAILQ_FOREACH(ns_ctx, &worker->ns_ctx, link)
    {
        if (init_ns_worker_ctx(ns_ctx) != 0) {
            printf("ERROR: init_ns_worker_ctx() failed\n");
            return 1;
        }
    }

    tsc_end = spdk_get_ticks() + g_zstore.time_in_sec * g_zstore.tsc_rate;

    /* Submit initial I/O for each namespace. */
    TAILQ_FOREACH(ns_ctx, &worker->ns_ctx, link)
    {
        submit_io(ns_ctx, g_zstore.queue_depth);
    }

    while (1) {
        log_debug("while");
        // crashes
        /*
         * Check for completed I/O for each controller. A new
         * I/O will be submitted in the io_complete callback
         * to replace each I/O that is completed.
         */
        TAILQ_FOREACH(ns_ctx, &worker->ns_ctx, link)
        {
            log_debug("\tfor each : {}", ns_ctx->io_completed);
            check_io(ns_ctx);
        }

        // if (ns_ctx->io_completed > append_times) {
        //     break;
        // }
        if (spdk_get_ticks() > tsc_end) {
            break;
        }
        log_debug("one loop: {}", ns_ctx->io_completed);
    }

    TAILQ_FOREACH(ns_ctx, &worker->ns_ctx, link)
    {
        drain_io(ns_ctx);
        cleanup_ns_worker_ctx(ns_ctx);
    }

    return 0;
}

int main(int argc, char **argv)
{
    int rc;
    //
    struct worker_thread *worker, *main_worker;
    unsigned main_core;
    char task_pool_name[30];
    uint32_t task_count = 0;

    // spdk bootstrap
    struct spdk_env_opts opts;
    spdk_env_opts_init(&opts);
    opts.name = "zns_measurement_opts";
    opts.mem_size = g_dpdk_mem;
    opts.hugepage_single_segments = g_dpdk_mem_single_seg;
    opts.core_mask = g_zstore.core_mask;
    // opts.shm_id = g_zstore.shm_id;
    if (spdk_env_init(&opts) < 0) {
        return 1;
    }

    if (register_workers() != 0) {
        rc = 1;
        log_error("zstore cannot register workers ");
        zstore_cleanup(task_count);
        return rc;
    }

    // Bootstrap zstore
    // NOTE: we switch between zones and keep track of it with a file
    int current_zone = 0;
    std::ifstream inputFile("../current_zone");
    if (inputFile.is_open()) {
        inputFile >> current_zone;
        inputFile.close();
    }
    log_info("Zstore start with current zone: {}", current_zone);

    struct zstore_context ctx = {};

    if (register_controllers(&ctx) != 0) {
        rc = 1;
        log_error("zstore cannot register controllers");
        zstore_cleanup(task_count);
        return rc;
    }
    if (associate_workers_with_ns(current_zone) != 0) {
        rc = 1;
        log_error("zstore cannot associate workers with ns");
        return rc;
    }

    // if ((rc = spdk_app_parse_args(argc, argv, &opts, NULL, NULL, NULL,
    // NULL))
    // !=
    //     SPDK_APP_PARSE_ARGS_SUCCESS) {
    //     exit(rc);
    // }

    // task pool
    snprintf(task_pool_name, sizeof(task_pool_name), "task_pool_%d", getpid());

    /*
     * The task_count will be dynamically calculated based on the
     * number of attached active namespaces, queue depth and number
     * of cores (workers) involved in the IO perations.
     */
    task_count = g_zstore.num_namespaces > g_zstore.num_workers
                     ? g_zstore.num_namespaces
                     : g_zstore.num_workers;
    task_count *= g_zstore.queue_depth;

    task_pool = spdk_mempool_create(task_pool_name, task_count,
                                    sizeof(struct zstore_task), 0,
                                    SPDK_ENV_SOCKET_ID_ANY);
    if (task_pool == NULL) {
        fprintf(stderr, "could not initialize task pool\n");
        rc = 1;
        zstore_cleanup(task_count);
        return rc;
    }

    // rc = spdk_app_start(&opts, zns_measure, &ctx);
    // if (rc) {
    //     SPDK_ERRLOG("ERROR starting application\n");
    // }

    log_info("Initialization complete. Launching workers.\n");

    /* Launch all of the secondary workers */
    main_core = spdk_env_get_current_core();
    main_worker = NULL;
    log_debug("main core {}", main_core);
    TAILQ_FOREACH(worker, &g_workers, link)
    {
        log_debug("TAIL for core {}", worker->lcore);
        if (worker->lcore != main_core) {
            spdk_env_thread_launch_pinned(worker->lcore, work_fn, worker);
        } else {
            assert(main_worker == NULL);
            main_worker = worker;
        }
    }

    log_debug("1\n");
    assert(main_worker != NULL);
    rc = work_fn(main_worker);

    log_debug("1\n");
    spdk_env_thread_wait_all();

    // print_stats();

    log_info("zstore exits gracefully");
    zstore_cleanup(task_count);

    return rc;
}
