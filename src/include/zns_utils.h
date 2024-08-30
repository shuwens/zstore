#pragma once
#include "spdk/endian.h"
#include "spdk/env.h"
#include "spdk/log.h"
#include "spdk/nvme.h"
#include "spdk/nvme_intel.h"
#include "spdk/nvme_ocssd.h"
#include "spdk/nvme_spec.h"
#include "spdk/nvme_zns.h"
#include "spdk/nvmf_spec.h"
#include "spdk/pci_ids.h"
#include "spdk/stdinc.h"
#include "spdk/string.h"
#include "spdk/util.h"
#include "spdk/uuid.h"
#include "spdk/vmd.h"
#include "zns_device.h"

static void task_complete(struct arb_task *task);

static void io_complete(void *ctx, const struct spdk_nvme_cpl *completion);

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
}

static __thread unsigned int seed = 0;

static void submit_single_io(struct ns_worker_ctx *ns_ctx)
{
    struct arb_task *task = NULL;
    uint64_t offset_in_ios;
    int rc;
    struct ns_entry *entry = ns_ctx->entry;

    task = (struct arb_task *)spdk_mempool_get(task_pool);
    if (!task) {
        log_error("Failed to get task from task_pool");
        exit(1);
    }

    task->buf = spdk_dma_zmalloc(g_zstore.io_size_bytes, 0x200, NULL);
    if (!task->buf) {
        spdk_mempool_put(task_pool, task);
        log_error("task->buf spdk_dma_zmalloc failed");
        exit(1);
    }

    task->ns_ctx = ns_ctx;

    if (g_zstore.is_random) {
        offset_in_ios = rand_r(&seed) % entry->size_in_ios;
    } else {
        offset_in_ios = ns_ctx->offset_in_ios++;
        if (ns_ctx->offset_in_ios == entry->size_in_ios) {
            ns_ctx->offset_in_ios = 0;
        }
    }

    // if ((g_zstore.rw_percentage == 100) ||
    //     (g_zstore.rw_percentage != 0 &&
    //      ((rand_r(&seed) % 100) < g_zstore.rw_percentage))) {
    ns_ctx->stime = std::chrono::high_resolution_clock::now();
    ns_ctx->stimes.push_back(ns_ctx->stime);

    rc = spdk_nvme_ns_cmd_read(entry->nvme.ns, ns_ctx->qpair, task->buf,
                               offset_in_ios * entry->io_size_blocks,
                               entry->io_size_blocks, io_complete, task, 0);
    // } else {
    //     ns_ctx->stime = std::chrono::high_resolution_clock::now();
    //     ns_ctx->stimes.push_back(ns_ctx->stime);
    //     rc =
    //         spdk_nvme_ns_cmd_write(entry->nvme.ns, ns_ctx->qpair, task->buf,
    //                                offset_in_ios * entry->io_size_blocks,
    //                                entry->io_size_blocks, io_complete, task,
    //                                0);
    // }

    if (rc != 0) {
        log_error("starting I/O failed");
    } else {
        ns_ctx->current_queue_depth++;
    }
}

static void task_complete(struct arb_task *task)
{
    struct ns_worker_ctx *ns_ctx;

    ns_ctx = task->ns_ctx;
    ns_ctx->current_queue_depth--;
    ns_ctx->io_completed++;
    ns_ctx->etime = std::chrono::high_resolution_clock::now();
    ns_ctx->etimes.push_back(ns_ctx->etime);

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
    task_complete((struct arb_task *)ctx);
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

static int init_ns_worker_ctx(struct ns_worker_ctx *ns_ctx,
                              enum spdk_nvme_qprio qprio)
{
    struct spdk_nvme_ctrlr *ctrlr = ns_ctx->entry->nvme.ctrlr;
    struct spdk_nvme_io_qpair_opts opts;

    spdk_nvme_ctrlr_get_default_io_qpair_opts(ctrlr, &opts, sizeof(opts));
    opts.qprio = qprio;

    ns_ctx->qpair = spdk_nvme_ctrlr_alloc_io_qpair(ctrlr, &opts, sizeof(opts));
    if (!ns_ctx->qpair) {
        log_error("ERROR: spdk_nvme_ctrlr_alloc_io_qpair failed");
        return 1;
    }

    // allocate space for times
    ns_ctx->stimes.reserve(1000000);
    ns_ctx->etimes.reserve(1000000);

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

static int work_fn(void *arg)
{
    uint64_t tsc_end;
    struct worker_thread *worker = (struct worker_thread *)arg;
    struct ns_worker_ctx *ns_ctx;

    log_info("Starting thread on core {} with {}", worker->lcore, "what");

    /* Allocate a queue pair for each namespace. */
    TAILQ_FOREACH(ns_ctx, &worker->ns_ctx, link)
    {
        if (init_ns_worker_ctx(ns_ctx, worker->qprio) != 0) {
            log_error("ERROR: init_ns_worker_ctx() failed");
            return 1;
        }
    }

    tsc_end = spdk_get_ticks() + g_zstore.time_in_sec * g_zstore.tsc_rate;
    // printf("tick %s, time in sec %s, tsc rate %s", spdk_get_ticks(),
    //        g_zstore.time_in_sec, g_zstore.tsc_rate);

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
        TAILQ_FOREACH(ns_ctx, &worker->ns_ctx, link) { check_io(ns_ctx); }

        if (spdk_get_ticks() > tsc_end) {
            break;
        }
    }

    TAILQ_FOREACH(ns_ctx, &worker->ns_ctx, link)
    {
        drain_io(ns_ctx);
        cleanup_ns_worker_ctx(ns_ctx);

        std::vector<uint64_t> deltas1;
        for (int i = 0; i < ns_ctx->stimes.size(); i++) {
            deltas1.push_back(
                std::chrono::duration_cast<std::chrono::microseconds>(
                    ns_ctx->etimes[i] - ns_ctx->stimes[i])
                    .count());
        }
        auto sum1 = std::accumulate(deltas1.begin(), deltas1.end(), 0.0);
        auto mean1 = sum1 / deltas1.size();
        auto sq_sum1 = std::inner_product(deltas1.begin(), deltas1.end(),
                                          deltas1.begin(), 0.0);
        auto stdev1 = std::sqrt(sq_sum1 / deltas1.size() - mean1 * mean1);
        printf("qd: %d, mean %f, std %f\n", ns_ctx->current_queue_depth, mean1,
               stdev1);

        // clearnup
        deltas1.clear();
        ns_ctx->etimes.clear();
        ns_ctx->stimes.clear();
    }

    return 0;
}

static void print_configuration(char *program_name)
{
    printf("%s run with configuration:\n", program_name);
    printf("%s -q %d -s %d -w %s -M %d -t %d -c %s  -b %d -n "
           "%d \n",
           program_name, g_zstore.queue_depth, g_zstore.io_size_bytes,
           g_zstore.workload_type, g_zstore.rw_percentage, g_zstore.time_in_sec,
           g_zstore.core_mask, g_zstore.max_completions, g_zstore.io_count);
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

static void print_latency_page(struct ctrlr_entry *entry)
{
    int i;

    printf("\n");
    printf("%s\n", entry->name);
    printf("--------------------------------------------------------\n");

    for (i = 0; i < 32; i++) {
        if (entry->latency_page.buckets_32us[i])
            printf("Bucket %dus - %dus: %d\n", i * 32, (i + 1) * 32,
                   entry->latency_page.buckets_32us[i]);
    }
    for (i = 0; i < 31; i++) {
        if (entry->latency_page.buckets_1ms[i])
            printf("Bucket %dms - %dms: %d\n", i + 1, i + 2,
                   entry->latency_page.buckets_1ms[i]);
    }
    for (i = 0; i < 31; i++) {
        if (entry->latency_page.buckets_32ms[i])
            printf("Bucket %dms - %dms: %d\n", (i + 1) * 32, (i + 2) * 32,
                   entry->latency_page.buckets_32ms[i]);
    }
}

static void print_stats(void) { print_performance(); }

static int parse_args(int argc, char **argv)
{
    const char *workload_type = NULL;
    int op = 0;
    bool mix_specified = false;
    int rc;
    long int val;

    spdk_nvme_trid_populate_transport(&g_trid, SPDK_NVME_TRANSPORT_PCIE);
    snprintf(g_trid.subnqn, sizeof(g_trid.subnqn), "%s",
             SPDK_NVMF_DISCOVERY_NQN);

    while ((op = getopt(argc, argv, "a:b:c:d:ghi:l:m:n:o:q:r:t:w:M:L:")) !=
           -1) {
        switch (op) {
        case 'c':
            g_zstore.core_mask = optarg;
            break;
        case 'd':
            g_dpdk_mem = spdk_strtol(optarg, 10);
            if (g_dpdk_mem < 0) {
                fprintf(stderr, "Invalid DPDK memory size\n");
                return g_dpdk_mem;
            }
            break;
        case 'w':
            g_zstore.workload_type = optarg;
            break;
        case 'r':
            if (spdk_nvme_transport_id_parse(&g_trid, optarg) != 0) {
                fprintf(stderr, "Error parsing transport address\n");
                return 1;
            }
            break;
        case 'g':
            g_dpdk_mem_single_seg = true;
            break;
        case 'h':
            // case '?':
            //     usage(argv[0]);
            //     return 1;
            // case 'L':
            //     rc = spdk_log_set_flag(optarg);
            //     if (rc < 0) {
            //         fprintf(stderr, "unknown flag\n");
            //         usage(argv[0]);
            //         exit(EXIT_FAILURE);
            //     }
#ifdef DEBUG
            spdk_log_set_print_level(SPDK_LOG_DEBUG);
#endif
            break;
        default:
            val = spdk_strtol(optarg, 10);
            if (val < 0) {
                fprintf(stderr, "Converting a string to integer failed\n");
                return val;
            }
            switch (op) {
            case 'm':
                g_zstore.max_completions = val;
                break;
            case 'q':
                g_zstore.queue_depth = val;
                break;
            case 'o':
                g_zstore.io_size_bytes = val;
                break;
            case 't':
                g_zstore.time_in_sec = val;
                break;
            case 'M':
                g_zstore.rw_percentage = val;
                mix_specified = true;
                break;
                break;
            default:
                // usage(argv[0]);
                return -EINVAL;
            }
        }
    }

    workload_type = g_zstore.workload_type;

    if (strcmp(workload_type, "read") && strcmp(workload_type, "write") &&
        strcmp(workload_type, "randread") &&
        strcmp(workload_type, "randwrite") && strcmp(workload_type, "rw") &&
        strcmp(workload_type, "randrw")) {
        fprintf(stderr, "io pattern type must be one of\n"
                        "(read, write, randread, randwrite, rw, randrw)\n");
        return 1;
    }

    if (!strcmp(workload_type, "read") || !strcmp(workload_type, "randread")) {
        g_zstore.rw_percentage = 100;
    }

    if (!strcmp(workload_type, "write") ||
        !strcmp(workload_type, "randwrite")) {
        g_zstore.rw_percentage = 0;
    }

    if (!strcmp(workload_type, "read") || !strcmp(workload_type, "randread") ||
        !strcmp(workload_type, "write") ||
        !strcmp(workload_type, "randwrite")) {
        if (mix_specified) {
            fprintf(stderr, "Ignoring -M option... Please use -M option"
                            " only when using rw or randrw.\n");
        }
    }

    if (!strcmp(workload_type, "rw") || !strcmp(workload_type, "randrw")) {
        if (g_zstore.rw_percentage < 0 || g_zstore.rw_percentage > 100) {
            fprintf(stderr, "-M must be specified to value from 0 to 100 "
                            "for rw or randrw.\n");
            return 1;
        }
    }

    if (!strcmp(workload_type, "read") || !strcmp(workload_type, "write") ||
        !strcmp(workload_type, "rw")) {
        g_zstore.is_random = 0;
    } else {
        g_zstore.is_random = 1;
    }

    return 0;
}

static int register_workers(void)
{
    uint32_t i;
    struct worker_thread *worker;
    enum spdk_nvme_qprio qprio = SPDK_NVME_QPRIO_URGENT;

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

        worker->qprio = static_cast<enum spdk_nvme_qprio>(
            qprio & SPDK_NVME_CREATE_IO_SQ_QPRIO_MASK);
    }

    return 0;
}

static bool probe_cb(void *cb_ctx, const struct spdk_nvme_transport_id *trid,
                     struct spdk_nvme_ctrlr_opts *opts)
{
    /* Update with user specified zstore configuration */
    // opts->arb_mechanism =
    //     static_cast<enum
    //     spdk_nvme_cc_ams>(g_zstore.zstore_mechanism);

    printf("Attaching to %s\n", trid->traddr);

    return true;
}

static void attach_cb(void *cb_ctx, const struct spdk_nvme_transport_id *trid,
                      struct spdk_nvme_ctrlr *ctrlr,
                      const struct spdk_nvme_ctrlr_opts *opts)
{
    printf("Attached to %s\n", trid->traddr);

    /* Update with actual zstore configuration in use */
    // g_zstore.zstore_mechanism = opts->arb_mechanism;

    register_ctrlr(ctrlr);
}

static void zns_dev_init(struct arb_context *ctx, std::string ip1,
                         std::string port1)
{
    int rc = 0;

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

    printf("Found %d namspaces\n", g_zstore.num_namespaces);
}

static int register_controllers(struct arb_context *ctx)
{
    printf("Initializing NVMe Controllers\n");

    // RDMA
    // zns_dev_init(ctx, "192.168.100.9", "5520");
    // TCP
    zns_dev_init(ctx, "12.12.12.2", "5520");

    // if (spdk_nvme_probe(&g_trid, NULL, probe_cb, attach_cb, NULL) != 0) {
    //     fprintf(stderr, "spdk_nvme_probe() failed\n");
    //     return 1;
    // }

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

        spdk_nvme_detach_async(entry->ctrlr, &detach_ctx);
        free(entry);
    }

    while (detach_ctx && spdk_nvme_detach_poll_async(detach_ctx) == -EAGAIN) {
        ;
    }
}

static int associate_workers_with_ns(void)
{
    struct ns_entry *entry = TAILQ_FIRST(&g_namespaces);
    struct worker_thread *worker = TAILQ_FIRST(&g_workers);
    struct ns_worker_ctx *ns_ctx;
    int i, count;

    count = g_zstore.num_namespaces > g_zstore.num_workers
                ? g_zstore.num_namespaces
                : g_zstore.num_workers;
    log_info("DEBUG ns {}, workers {}, count {}", g_zstore.num_namespaces,
             g_zstore.num_workers, count);

    count = 1;
    log_info("Hard code worker count to {} so we only use {} worker", count,
             count);

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

static void get_feature_completion(void *cb_arg,
                                   const struct spdk_nvme_cpl *cpl)
{
    struct feature *feature = (struct feature *)cb_arg;
    int fid = feature - features;

    if (spdk_nvme_cpl_is_error(cpl)) {
        printf("get_feature(0x%02X) failed\n", fid);
    } else {
        feature->result = cpl->cdw0;
        feature->valid = true;
    }

    g_zstore.outstanding_commands--;
}

static int get_feature(struct spdk_nvme_ctrlr *ctrlr, uint8_t fid)
{
    struct spdk_nvme_cmd cmd = {};
    struct feature *feature = &features[fid];

    feature->valid = false;

    cmd.opc = SPDK_NVME_OPC_GET_FEATURES;
    cmd.cdw10_bits.get_features.fid = fid;

    return spdk_nvme_ctrlr_cmd_admin_raw(ctrlr, &cmd, NULL, 0,
                                         get_feature_completion, feature);
}

static void get_arb_feature(struct spdk_nvme_ctrlr *ctrlr)
{
    get_feature(ctrlr, SPDK_NVME_FEAT_ARBITRATION);

    g_zstore.outstanding_commands++;

    while (g_zstore.outstanding_commands) {
        spdk_nvme_ctrlr_process_admin_completions(ctrlr);
    }

    if (features[SPDK_NVME_FEAT_ARBITRATION].valid) {
        union spdk_nvme_cmd_cdw11 arb;
        arb.feat_arbitration.raw = features[SPDK_NVME_FEAT_ARBITRATION].result;

        printf("Current zstore Configuration\n");
        printf("===========\n");
        printf("zstore Burst:           ");
        if (arb.feat_arbitration.bits.ab ==
            SPDK_NVME_ARBITRATION_BURST_UNLIMITED) {
            printf("no limit\n");
        } else {
            printf("%u\n", 1u << arb.feat_arbitration.bits.ab);
        }

        printf("Low Priority Weight:         %u\n",
               arb.feat_arbitration.bits.lpw + 1);
        printf("Medium Priority Weight:      %u\n",
               arb.feat_arbitration.bits.mpw + 1);
        printf("High Priority Weight:        %u\n",
               arb.feat_arbitration.bits.hpw + 1);
        printf("\n");
    }
}

static void set_feature_completion(void *cb_arg,
                                   const struct spdk_nvme_cpl *cpl)
{
    struct feature *feature = (struct feature *)cb_arg;
    int fid = feature - features;

    if (spdk_nvme_cpl_is_error(cpl)) {
        printf("set_feature(0x%02X) failed\n", fid);
        feature->valid = false;
    } else {
        printf("Set zstore Feature Successfully\n");
    }

    g_zstore.outstanding_commands--;
}

static int set_arb_feature(struct spdk_nvme_ctrlr *ctrlr)
{
    int ret;
    struct spdk_nvme_cmd cmd = {};

    cmd.opc = SPDK_NVME_OPC_SET_FEATURES;
    cmd.cdw10_bits.set_features.fid = SPDK_NVME_FEAT_ARBITRATION;

    g_zstore.outstanding_commands = 0;

    if (features[SPDK_NVME_FEAT_ARBITRATION].valid) {
        cmd.cdw11_bits.feat_arbitration.bits.ab =
            SPDK_NVME_ARBITRATION_BURST_UNLIMITED;
        cmd.cdw11_bits.feat_arbitration.bits.lpw =
            USER_SPECIFIED_LOW_PRIORITY_WEIGHT;
        cmd.cdw11_bits.feat_arbitration.bits.mpw =
            USER_SPECIFIED_MEDIUM_PRIORITY_WEIGHT;
        cmd.cdw11_bits.feat_arbitration.bits.hpw =
            USER_SPECIFIED_HIGH_PRIORITY_WEIGHT;
    }

    ret = spdk_nvme_ctrlr_cmd_admin_raw(ctrlr, &cmd, NULL, 0,
                                        set_feature_completion,
                                        &features[SPDK_NVME_FEAT_ARBITRATION]);
    if (ret) {
        printf("Set zstore Feature: Failed 0x%x\n", ret);
        return 1;
    }

    g_zstore.outstanding_commands++;

    while (g_zstore.outstanding_commands) {
        spdk_nvme_ctrlr_process_admin_completions(ctrlr);
    }

    if (!features[SPDK_NVME_FEAT_ARBITRATION].valid) {
        printf("Set zstore Feature failed and use default configuration\n");
    }

    return 0;
}

static void zstore_cleanup(uint32_t task_count)
{

    unregister_controllers();
    cleanup(task_count);

    spdk_env_fini();

    // if (rc != 0) {
    //     fprintf(stderr, "%s: errors occurred\n", argv[0]);
    // }
}
