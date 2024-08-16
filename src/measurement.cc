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
    // log_debug("\tSUBMIT IO: io completed {}, current qd {}, offset {}",
    //           ns_ctx->io_completed, ns_ctx->current_queue_depth,
    //           ns_ctx->offset_in_ios);

    ns_ctx->stime = std::chrono::high_resolution_clock::now();
    ns_ctx->stimes.push_back(ns_ctx->stime);
    rc = spdk_nvme_ns_cmd_read(entry->nvme.ns, ns_ctx->qpair, task->buf,
                               offset_in_ios * entry->io_size_blocks,
                               entry->io_size_blocks, io_complete, task, 0);
    // log_debug("read {}, stime {}", ns_ctx->io_completed, ns_ctx->stime);

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
    ns_ctx->etime = std::chrono::high_resolution_clock::now();
    ns_ctx->etimes.push_back(ns_ctx->etime);
    // log_debug("read {}, etime {}", ns_ctx->io_completed, ns_ctx->etime);

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
    // log_debug("\tCHECK IO: io completed {}, current qd {}, offset {}",
    //           ns_ctx->io_completed, ns_ctx->current_queue_depth,
    //           ns_ctx->offset_in_ios);
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
    log_debug("ticks {}, tsc end {}, delta {}", spdk_get_ticks(), tsc_end,
              g_zstore.time_in_sec * g_zstore.tsc_rate);

    /* Submit initial I/O for each namespace. */
    TAILQ_FOREACH(ns_ctx, &worker->ns_ctx, link)
    {
        submit_io(ns_ctx, g_zstore.queue_depth);
    }

    while (1) {
        /*
         * Check for completed I/O for each controller. A new
         * I/O will be submitted in the io_complete callback
         * to replace each I/O that is completed.
         */
        TAILQ_FOREACH(ns_ctx, &worker->ns_ctx, link)
        {
            // log_debug("\tfor each : {}", ns_ctx->io_completed);
            check_io(ns_ctx);

            if (ns_ctx->io_completed > append_times) {
                break;
            }
        }

        if (spdk_get_ticks() > tsc_end) {
            log_debug("ticks {}, tsc end {}", spdk_get_ticks(), tsc_end);
            break;
        }
    }

    TAILQ_FOREACH(ns_ctx, &worker->ns_ctx, link)
    {
        drain_io(ns_ctx);
        cleanup_ns_worker_ctx(ns_ctx);
        log_debug("\t io completed {}, current qd {}, offset {}",
                  ns_ctx->io_completed, ns_ctx->current_queue_depth,
                  ns_ctx->offset_in_ios);

        log_debug("\t stimes size {}, etimes size {}", ns_ctx->stimes.size(),
                  ns_ctx->etimes.size());

        std::vector<u64> deltas1;
        // std::vector<u64> deltas2;
        for (int i = 0; i < ns_ctx->stimes.size(); i++) {
            deltas1.push_back(
                std::chrono::duration_cast<std::chrono::microseconds>(
                    ns_ctx->etimes[i] - ns_ctx->stimes[i])
                    .count());
            // deltas2.push_back(std::chrono::duration_cast<std::chrono::microseconds>(
            //                       ctx->m2.etimes[i] - ctx->m2.stimes[i])
            //                       .count());
        }
        auto sum1 = std::accumulate(deltas1.begin(), deltas1.end(), 0.0);
        // auto sum2 = std::accumulate(deltas2.begin(), deltas2.end(), 0.0);
        auto mean1 = sum1 / deltas1.size();
        // auto mean2 = sum2 / deltas2.size();
        auto sq_sum1 = std::inner_product(deltas1.begin(), deltas1.end(),
                                          deltas1.begin(), 0.0);
        // auto sq_sum2 = std::inner_product(deltas2.begin(), deltas2.end(),
        //                                   deltas2.begin(), 0.0);
        auto stdev1 = std::sqrt(sq_sum1 / deltas1.size() - mean1 * mean1);
        // auto stdev2 = std::sqrt(sq_sum2 / deltas2.size() - mean2 * mean2);
        log_info("WRITES-1 qd {}, append: mean {} us, std {}",
                 ns_ctx->current_queue_depth, mean1, stdev1);
        // log_info("WRITES-2 qd {}, append: mean {} us, std {}", ctx->qd,
        // mean2,
        //          stdev2);

        // clearnup
        deltas1.clear();
        // deltas2.clear();
        ns_ctx->stimes.clear();
        // ctx->m1.etimes.clear();
        ns_ctx->stimes.clear();
        // ctx->m2.etimes.clear();
    }

    return 0;
}

static void print_performance(void)
{
    float io_per_second, sent_all_io_in_secs;
    struct worker_thread *worker;
    struct ns_worker_ctx *ns_ctx;

    TAILQ_FOREACH(worker, &g_workers, link)
    {
        TAILQ_FOREACH(ns_ctx, &worker->ns_ctx, link)
        {
            io_per_second = (float)ns_ctx->io_completed / g_zstore.time_in_sec;
            sent_all_io_in_secs = g_zstore.io_count / io_per_second;
            printf("%-43.43s core %u: %8.2f IO/s %8.2f secs/%d ios\n",
                   ns_ctx->entry->name, worker->lcore, io_per_second,
                   sent_all_io_in_secs, g_zstore.io_count);
        }
    }
    printf("========================================================\n");

    printf("\n");
}

static void print_latency_statistics(const char *op_name,
                                     enum spdk_nvme_intel_log_page log_page)
{
    struct ctrlr_entry *ctrlr;

    printf("%s Latency Statistics:\n", op_name);
    printf("========================================================\n");
    TAILQ_FOREACH(ctrlr, &g_controllers, link)
    {
        if (spdk_nvme_ctrlr_is_log_page_supported(ctrlr->ctrlr, log_page)) {
            // if (spdk_nvme_ctrlr_cmd_get_log_page(
            //         ctrlr->ctrlr, log_page, SPDK_NVME_GLOBAL_NS_TAG,
            //         &ctrlr->latency_page,
            //         sizeof(struct spdk_nvme_intel_rw_latency_page), 0,
            //         enable_latency_tracking_complete, NULL)) {
            //     printf("nvme_ctrlr_cmd_get_log_page() failed\n");
            //     exit(1);
            // }

            g_zstore.outstanding_commands++;
        } else {
            printf("Controller %s: %s latency statistics not supported\n",
                   ctrlr->name, op_name);
        }
    }

    while (g_zstore.outstanding_commands) {
        TAILQ_FOREACH(ctrlr, &g_controllers, link)
        {
            spdk_nvme_ctrlr_process_admin_completions(ctrlr->ctrlr);
        }
    }

    TAILQ_FOREACH(ctrlr, &g_controllers, link)
    {
        // if (spdk_nvme_ctrlr_is_log_page_supported(ctrlr->ctrlr, log_page)) {
        //     print_latency_page(ctrlr);
        // }
    }
    printf("\n");
}

static void print_stats(void)
{
    print_performance();
    // if (g_zstore.latency_tracking_enable) {
    //     if (g_zstore.rw_percentage != 0) {
    //         print_latency_statistics("Read",
    //                                  SPDK_NVME_INTEL_LOG_READ_CMD_LATENCY);
    //     }
    //     if (g_zstore.rw_percentage != 100) {
    //         print_latency_statistics("Write",
    //                                  SPDK_NVME_INTEL_LOG_WRITE_CMD_LATENCY);
    //     }
    // }
}

static void print_configuration(char *program_name)
{
    printf("%s run with configuration:\n", program_name);
    printf("%s -q %d -s %d -w %s -t %d -c %s -m %d -n %d\n", program_name,
           g_zstore.queue_depth, g_zstore.io_size_bytes, g_zstore.workload_type,
           g_zstore.time_in_sec, g_zstore.core_mask, g_zstore.max_completions,
           g_zstore.io_count);
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

    g_zstore.tsc_rate = spdk_get_ticks_hz();

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

    print_configuration(argv[0]);
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

    // log_debug("1\n");
    assert(main_worker != NULL);
    rc = work_fn(main_worker);

    // log_debug("1\n");
    spdk_env_thread_wait_all();

    print_stats();
    log_info("zstore exits gracefully");
    zstore_cleanup(task_count);

    return rc;
}
