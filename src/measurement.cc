#include "include/utils.hpp"
#include "include/zns_device.h"
#include "spdk/env.h"
#include "spdk/event.h"
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

// static int g_dpdk_mem = 0;
// static bool g_dpdk_mem_single_seg = false;

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

static void register_ns(struct spdk_nvme_ctrlr *ctrlr, struct spdk_nvme_ns *ns)
{
    struct ns_entry *entry;
    const struct spdk_nvme_ctrlr_data *cdata;

    cdata = spdk_nvme_ctrlr_get_data(ctrlr);

    if (spdk_nvme_ns_get_size(ns) < g_zstore.io_size_bytes ||
        spdk_nvme_ns_get_extended_sector_size(ns) > g_zstore.io_size_bytes ||
        g_zstore.io_size_bytes % spdk_nvme_ns_get_extended_sector_size(ns)) {
        printf("WARNING: controller %-20.20s (%-20.20s) ns %u has invalid "
               "ns size %" PRIu64 " / block size %u for I/O size %u\n",
               cdata->mn, cdata->sn, spdk_nvme_ns_get_id(ns),
               spdk_nvme_ns_get_size(ns),
               spdk_nvme_ns_get_extended_sector_size(ns),
               g_zstore.io_size_bytes);
        return;
    }

    entry = (struct ns_entry *)malloc(sizeof(struct ns_entry));
    if (entry == NULL) {
        perror("ns_entry malloc");
        exit(1);
    }

    entry->nvme.ctrlr = ctrlr;
    entry->nvme.ns = ns;

    entry->size_in_ios = spdk_nvme_ns_get_size(ns) / g_zstore.io_size_bytes;
    entry->io_size_blocks =
        g_zstore.io_size_bytes / spdk_nvme_ns_get_sector_size(ns);

    snprintf(entry->name, 44, "%-20.20s (%-20.20s)", cdata->mn, cdata->sn);

    g_zstore.num_namespaces++;
    TAILQ_INSERT_TAIL(&g_namespaces, entry, link);
}

static void register_ctrlr(struct spdk_nvme_ctrlr *ctrlr)
{
    uint32_t nsid;
    struct spdk_nvme_ns *ns;

    struct ctrlr_entry *entry =
        (struct ctrlr_entry *)calloc(1, sizeof(struct ctrlr_entry));
    union spdk_nvme_cap_register cap = spdk_nvme_ctrlr_get_regs_cap(ctrlr);
    const struct spdk_nvme_ctrlr_data *cdata = spdk_nvme_ctrlr_get_data(ctrlr);

    if (entry == NULL) {
        perror("ctrlr_entry malloc");
        exit(1);
    }

    snprintf(entry->name, sizeof(entry->name), "%-20.20s (%-20.20s)", cdata->mn,
             cdata->sn);

    entry->ctrlr = ctrlr;
    TAILQ_INSERT_TAIL(&g_controllers, entry, link);

    for (nsid = spdk_nvme_ctrlr_get_first_active_ns(ctrlr); nsid != 0;
         nsid = spdk_nvme_ctrlr_get_next_active_ns(ctrlr, nsid)) {
        log_info("getting ns {}", nsid);
        ns = spdk_nvme_ctrlr_get_ns(ctrlr, nsid);
        if (ns == NULL) {
            continue;
        }
        if (spdk_nvme_ns_get_csi(ns) != SPDK_NVME_CSI_ZNS) {
            log_info("ns {} is not zns ns", nsid);
            continue;
        }
        register_ns(ctrlr, ns);
        // FIXME: why is it getting namespace 1 2 again?
        // force exit when we have two namespace
        break;
    }
}

static int register_workers(void)
{
    log_info("reg worksers");
    uint32_t i;
    struct worker_thread *worker;
    // enum spdk_nvme_qprio qprio = SPDK_NVME_QPRIO_URGENT;

    SPDK_ENV_FOREACH_CORE(i)
    {
        worker = (struct worker_thread *)calloc(1, sizeof(*worker));
        if (worker == NULL) {
            fprintf(stderr, "Unable to allocate worker\n");
            return -1;
        }

        TAILQ_INIT(&worker->ns_ctx);
        worker->lcore = i;
        TAILQ_INSERT_TAIL(&g_workers, worker, link);
        g_zstore.num_workers++;
        // FIXME: we have 4 cores, but we only want 1 worker for now
        if (g_zstore.num_workers == 1)
            break;
    }
    return 0;
}

// static bool probe_cb(void *cb_ctx, const struct spdk_nvme_transport_id *trid,
//                      struct spdk_nvme_ctrlr_opts *opts)
// {
//     /* Update with user specified arbitration configuration */
//     // opts->arb_mechanism = g_zstore.arbitration_mechanism;
//
//     printf("Attaching to %s\n", trid->traddr);
//
//     return true;
// }

// static void attach_cb(void *cb_ctx, const struct spdk_nvme_transport_id
// *trid,
//                       struct spdk_nvme_ctrlr *ctrlr,
//                       const struct spdk_nvme_ctrlr_opts *opts)
// {
//     printf("Attached to %s\n", trid->traddr);
//
//     /* Update with actual arbitration configuration in use */
//     // g_zstore.arbitration_mechanism = opts->arb_mechanism;
//
//     register_ctrlr(ctrlr);
// }

static void zns_dev_init(struct zstore_context *ctx, std::string ip1,
                         std::string port1, std::string ip2, std::string port2)
{
    int rc = 0;
    // FIXME
    // allocate space for times
    // ctx->stimes.reserve(append_times);
    // ctx->m1.etimes.reserve(append_times);
    // ctx->m2.stimes.reserve(append_times);
    // ctx->m2.etimes.reserve(append_times);

    if (ctx->verbose)
        log_info("Successfully started the application\n");

    // 1. connect nvmf device
    struct spdk_nvme_transport_id trid1 = {};
    snprintf(trid1.traddr, sizeof(trid1.traddr), "%s", ip1.c_str());
    snprintf(trid1.trsvcid, sizeof(trid1.trsvcid), "%s", port1.c_str());
    snprintf(trid1.subnqn, sizeof(trid1.subnqn), "%s", g_hostnqn);
    trid1.adrfam = SPDK_NVMF_ADRFAM_IPV4;
    trid1.trtype = SPDK_NVME_TRANSPORT_TCP;

    struct spdk_nvme_transport_id trid2 = {};
    snprintf(trid2.traddr, sizeof(trid2.traddr), "%s", ip2.c_str());
    snprintf(trid2.trsvcid, sizeof(trid2.trsvcid), "%s", port2.c_str());
    snprintf(trid2.subnqn, sizeof(trid2.subnqn), "%s", g_hostnqn);
    trid2.adrfam = SPDK_NVMF_ADRFAM_IPV4;
    trid2.trtype = SPDK_NVME_TRANSPORT_TCP;

    struct spdk_nvme_ctrlr_opts opts;
    spdk_nvme_ctrlr_get_default_ctrlr_opts(&opts, sizeof(opts));
    memcpy(opts.hostnqn, g_hostnqn, sizeof(opts.hostnqn));

    register_ctrlr(spdk_nvme_connect(&trid1, &opts, sizeof(opts)));
    // register_ctrlr(spdk_nvme_connect(&trid2, &opts, sizeof(opts)));

    log_info("Found {} namspaces", g_zstore.num_namespaces);
}

static int register_controllers(struct zstore_context *ctx)
{
    log_info("Initializing NVMe Controllers");

    zns_dev_init(ctx, "192.168.1.121", "4420", "192.168.1.121", "5520");

    if (g_zstore.num_namespaces == 0) {
        fprintf(stderr, "No valid namespaces to continue IO testing\n");
        return 1;
    }

    return 0;
}

static void unregister_controllers(void)
{
    struct ctrlr_entry *entry, *tmp;
    struct spdk_nvme_detach_ctx *detach_ctx = NULL;

    TAILQ_FOREACH_SAFE(entry, &g_controllers, link, tmp)
    {
        TAILQ_REMOVE(&g_controllers, entry, link);
        // if (g_zstore.latency_tracking_enable &&
        //     spdk_nvme_ctrlr_is_feature_supported(
        //         entry->ctrlr, SPDK_NVME_INTEL_FEAT_LATENCY_TRACKING)) {
        //     set_latency_tracking_feature(entry->ctrlr, false);
        // }
        spdk_nvme_detach_async(entry->ctrlr, &detach_ctx);
        free(entry);
    }

    while (detach_ctx && spdk_nvme_detach_poll_async(detach_ctx) == -EAGAIN) {
        ;
    }
}

static int associate_workers_with_ns(int current_zone)
{
    struct ns_entry *entry = TAILQ_FIRST(&g_namespaces);
    struct worker_thread *worker = TAILQ_FIRST(&g_workers);
    struct ns_worker_ctx *ns_ctx;
    int i, count;

    count = g_zstore.num_namespaces > g_zstore.num_workers
                ? g_zstore.num_namespaces
                : g_zstore.num_workers;
    log_info("worker {}, ns {}, count {}", g_zstore.num_workers,
             g_zstore.num_namespaces, count);
    // FIXME:
    count = 1;

    for (i = 0; i < count; i++) {
        if (entry == NULL) {
            break;
        }

        ns_ctx = (struct ns_worker_ctx *)malloc(sizeof(struct ns_worker_ctx));
        if (!ns_ctx) {
            return 1;
        }
        memset(ns_ctx, 0, sizeof(*ns_ctx));

        printf("Associating %s with lcore %d\n", entry->name, worker->lcore);
        ns_ctx->entry = entry;
        TAILQ_INSERT_TAIL(&worker->ns_ctx, ns_ctx, link);

        worker = TAILQ_NEXT(worker, link);
        if (worker == NULL) {
            worker = TAILQ_FIRST(&g_workers);
        }

        entry = TAILQ_NEXT(entry, link);
        if (entry == NULL) {
            entry = TAILQ_FIRST(&g_namespaces);
        }
    }

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

static int init_ns_worker_ctx(struct ns_worker_ctx *ns_ctx)
{
    log_debug("init ns worker ctx");
    struct spdk_nvme_ctrlr *ctrlr = ns_ctx->entry->nvme.ctrlr;
    struct spdk_nvme_io_qpair_opts opts;

    log_debug("init ns worker ctx2");
    spdk_nvme_ctrlr_get_default_io_qpair_opts(ctrlr, &opts, sizeof(opts));
    // opts.qprio = qprio;

    log_debug("init ns worker ctx3");
    ns_ctx->qpair = spdk_nvme_ctrlr_alloc_io_qpair(ctrlr, &opts, sizeof(opts));
    if (!ns_ctx->qpair) {
        printf("ERROR: spdk_nvme_ctrlr_alloc_io_qpair failed\n");
        return 1;
    }

    log_debug("init ns worker ctx4");
    return 0;
}

static void cleanup_ns_worker_ctx(struct ns_worker_ctx *ns_ctx)
{
    spdk_nvme_ctrlr_free_io_qpair(ns_ctx->qpair);
}

static void cleanup(uint32_t task_count)
{
    struct ns_entry *entry, *tmp_entry;
    struct worker_thread *worker, *tmp_worker;
    struct ns_worker_ctx *ns_ctx, *tmp_ns_ctx;

    TAILQ_FOREACH_SAFE(entry, &g_namespaces, link, tmp_entry)
    {
        TAILQ_REMOVE(&g_namespaces, entry, link);
        free(entry);
    };

    TAILQ_FOREACH_SAFE(worker, &g_workers, link, tmp_worker)
    {
        TAILQ_REMOVE(&g_workers, worker, link);

        /* ns_worker_ctx is a list in the worker */
        TAILQ_FOREACH_SAFE(ns_ctx, &worker->ns_ctx, link, tmp_ns_ctx)
        {
            TAILQ_REMOVE(&worker->ns_ctx, ns_ctx, link);
            free(ns_ctx);
        }

        free(worker);
    };

    if (spdk_mempool_count(task_pool) != (size_t)task_count) {
        fprintf(stderr, "task_pool count is %zu but should be %u\n",
                spdk_mempool_count(task_pool), task_count);
    }
    spdk_mempool_free(task_pool);
}

static void zstore_cleanup(u32 task_count)
{

    unregister_controllers();
    cleanup(task_count);

    spdk_env_fini();

    // if (rc != 0) {
    //     fprintf(stderr, "%s: errors occurred\n", argv[0]);
    // }
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
