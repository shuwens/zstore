#pragma once
#include "device.h"
#include "global.h"
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
#include "src/include/zstore_controller.h"
// #include "zns_device.h"

static void task_complete(struct arb_task *task);

static void io_complete(void *ctx, const struct spdk_nvme_cpl *completion);

static void register_ns(struct spdk_nvme_ctrlr *ctrlr, struct spdk_nvme_ns *ns,
                        void *args)
{
    ZstoreController *zctrlr = (ZstoreController *)args;
    // struct ns_entry *entry;
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

    zctrlr->mNamespace = (struct ns_entry *)malloc(sizeof(struct ns_entry));
    if (zctrlr->mNamespace == NULL) {
        perror("ns_entry malloc");
        exit(1);
    }

    zctrlr->mNamespace->nvme.ctrlr = ctrlr;
    zctrlr->mNamespace->nvme.ns = ns;

    zctrlr->mNamespace->size_in_ios =
        spdk_nvme_ns_get_size(ns) / g_arbitration.io_size_bytes;
    zctrlr->mNamespace->io_size_blocks =
        g_arbitration.io_size_bytes / spdk_nvme_ns_get_sector_size(ns);

    snprintf(zctrlr->mNamespace->name, 44, "%-20.20s (%-20.20s)", cdata->mn,
             cdata->sn);

    g_arbitration.num_namespaces++;
    // mNamespace = ;
}

static void register_ctrlr(struct spdk_nvme_ctrlr *ctrlr, void *args)
{
    ZstoreController *zctrlr = (ZstoreController *)args;
    uint32_t nsid;
    struct spdk_nvme_ns *ns;
    // struct ctrlr_entry *entry =
    zctrlr->mController =
        (struct ctrlr_entry *)calloc(1, sizeof(struct ctrlr_entry));
    union spdk_nvme_cap_register cap = spdk_nvme_ctrlr_get_regs_cap(ctrlr);
    const struct spdk_nvme_ctrlr_data *cdata = spdk_nvme_ctrlr_get_data(ctrlr);

    if (zctrlr->mController == NULL) {
        perror("ctrlr_entry malloc");
        exit(1);
    }

    snprintf(zctrlr->mController->name, sizeof(zctrlr->mController->name),
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
            printf("ns %d is not zns ns\n", nsid);
            // continue;
        } else {
            printf("ns %d is zns ns\n", nsid);
        }
        register_ns(ctrlr, ns, zctrlr);
    }
}

static __thread unsigned int seed = 0;

static void submit_single_io(struct ns_worker_ctx *ns_ctx)
{
    ZstoreController *zctrlr = (ZstoreController *)ns_ctx->zctrlr;
    struct arb_task *task = NULL;
    uint64_t offset_in_ios;
    int rc;
    struct ns_entry *entry = ns_ctx->entry;

    task = (struct arb_task *)spdk_mempool_get(zctrlr->mTaskPool);
    if (!task) {
        fprintf(stderr, "Failed to get task from mTaskPool\n");
        exit(1);
    }

    task->buf = spdk_dma_zmalloc(g_arbitration.io_size_bytes, 0x200, NULL);
    if (!task->buf) {
        spdk_mempool_put(zctrlr->mTaskPool, task);
        fprintf(stderr, "task->buf spdk_dma_zmalloc failed\n");
        exit(1);
    }

    task->ns_ctx = ns_ctx;

    if (g_arbitration.is_random) {
        offset_in_ios = rand_r(&seed) % entry->size_in_ios;
    } else {
        offset_in_ios = ns_ctx->offset_in_ios++;
        if (ns_ctx->offset_in_ios == entry->size_in_ios) {
            ns_ctx->offset_in_ios = 0;
        }
    }

    // if ((g_arbitration.rw_percentage == 100) ||
    //     (g_arbitration.rw_percentage != 0 &&
    //      ((rand_r(&seed) % 100) < g_arbitration.rw_percentage))) {
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
        fprintf(stderr, "starting I/O failed\n");
    } else {
        ns_ctx->current_queue_depth++;
    }
}

static void task_complete(struct arb_task *task)
{
    ZstoreController *zctrlr = (ZstoreController *)task->ns_ctx->zctrlr;
    struct ns_worker_ctx *ns_ctx;

    ns_ctx = task->ns_ctx;
    ns_ctx->current_queue_depth--;
    ns_ctx->io_completed++;
    ns_ctx->etime = std::chrono::high_resolution_clock::now();
    ns_ctx->etimes.push_back(ns_ctx->etime);

    spdk_dma_free(task->buf);
    spdk_mempool_put(zctrlr->mTaskPool, task);

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
    // ZstoreController *zctrlr = (ZstoreController *)args;
    task_complete((struct arb_task *)ctx);
}

static void check_io(struct ns_worker_ctx *ns_ctx)
{

    // ZstoreController *zctrlr = (ZstoreController *)args;
    spdk_nvme_qpair_process_completions(ns_ctx->qpair,
                                        g_arbitration.max_completions);
}

static void submit_io(struct ns_worker_ctx *ns_ctx, int queue_depth)
{
    // ZstoreController *zctrlr = (ZstoreController *)args;
    while (queue_depth-- > 0) {
        submit_single_io(ns_ctx);
    }
}

static void drain_io(struct ns_worker_ctx *ns_ctx)
{
    // ZstoreController *zctrlr = (ZstoreController *)args;
    ns_ctx->is_draining = true;
    while (ns_ctx->current_queue_depth > 0) {
        check_io(ns_ctx);
    }
}

static int init_ns_worker_ctx(struct ns_worker_ctx *ns_ctx,
                              enum spdk_nvme_qprio qprio, void *args)
{
    ZstoreController *zctrlr = (ZstoreController *)args;
    // struct spdk_nvme_ctrlr *ctrlr = ns_ctx->entry->nvme.ctrlr;
    struct spdk_nvme_ctrlr *ctrlr = zctrlr->mWorker->ns_ctx->entry->nvme.ctrlr;
    struct spdk_nvme_io_qpair_opts opts;

    spdk_nvme_ctrlr_get_default_io_qpair_opts(ctrlr, &opts, sizeof(opts));
    opts.qprio = qprio;

    ns_ctx->qpair = spdk_nvme_ctrlr_alloc_io_qpair(ctrlr, &opts, sizeof(opts));
    if (!ns_ctx->qpair) {
        printf("ERROR: spdk_nvme_ctrlr_alloc_io_qpair failed\n");
        return 1;
    }

    // allocate space for times
    ns_ctx->stimes.reserve(1000000);
    ns_ctx->etimes.reserve(1000000);

    ns_ctx->zctrlr = zctrlr;

    return 0;
}

static void cleanup_ns_worker_ctx(struct ns_worker_ctx *ns_ctx)
{
    spdk_nvme_ctrlr_free_io_qpair(ns_ctx->qpair);
}

static void cleanup(uint32_t task_count, void *args)
{
    ZstoreController *zctrlr = (ZstoreController *)args;
    // struct ns_entry *entry, *tmp_entry;
    // struct worker_thread *worker, *tmp_worker;
    // struct ns_worker_ctx *ns_ctx, *tmp_ns_ctx;

    free(zctrlr->mNamespace);
    // TAILQ_FOREACH_SAFE(entry, &mNamespaces, link, tmp_entry)
    // {
    //     TAILQ_REMOVE(&mNamespaces, entry, link);
    //     free(entry);
    // };

    free(zctrlr->mWorker->ns_ctx);
    free(zctrlr->mWorker);
    // TAILQ_FOREACH_SAFE(worker, &mWorkers, link, tmp_worker)
    // {
    //     TAILQ_REMOVE(&mWorkers, worker, link);
    //
    //     /* ns_worker_ctx is a list in the worker */
    //     TAILQ_FOREACH_SAFE(ns_ctx, &worker->ns_ctx, link, tmp_ns_ctx)
    //     {
    //         TAILQ_REMOVE(&worker->ns_ctx, ns_ctx, link);
    //         free(ns_ctx);
    //     }
    //
    //     free(worker);
    // };

    if (spdk_mempool_count(zctrlr->mTaskPool) != (size_t)task_count) {
        fprintf(stderr, "mTaskPool count is %zu but should be %u\n",
                spdk_mempool_count(zctrlr->mTaskPool), task_count);
    }
    spdk_mempool_free(zctrlr->mTaskPool);
}

static int work_fn(void *args)
{
    ZstoreController *zctrlr = (ZstoreController *)args;
    uint64_t tsc_end;
    // struct worker_thread *worker = (struct worker_thread *)arg;
    // struct ns_worker_ctx *ns_ctx;

    printf("Starting thread on core %u \n", zctrlr->mWorker->lcore);

    /* Allocate a queue pair for each namespace. */
    // TAILQ_FOREACH(ns_ctx, &worker->ns_ctx, link)
    // {
    if (init_ns_worker_ctx(zctrlr->mWorker->ns_ctx, zctrlr->mWorker->qprio,
                           zctrlr) != 0) {
        printf("ERROR: init_ns_worker_ctx() failed\n");
        return 1;
        // }
    }

    tsc_end =
        spdk_get_ticks() + g_arbitration.time_in_sec * g_arbitration.tsc_rate;
    // printf("tick %s, time in sec %s, tsc rate %s", spdk_get_ticks(),
    //        g_arbitration.time_in_sec, g_arbitration.tsc_rate);

    /* Submit initial I/O for each namespace. */
    // TAILQ_FOREACH(ns_ctx, &worker->ns_ctx, link)
    // {
    submit_io(zctrlr->mWorker->ns_ctx, g_arbitration.queue_depth);
    // }

    while (1) {
        /*
         * Check for completed I/O for each controller. A new
         * I/O will be submitted in the io_complete callback
         * to replace each I/O that is completed.
         */
        // TAILQ_FOREACH(ns_ctx, &worker->ns_ctx, link) {
        check_io(zctrlr->mWorker->ns_ctx);
        // }

        if (spdk_get_ticks() > tsc_end) {
            break;
        }
    }

    // TAILQ_FOREACH(ns_ctx, &worker->ns_ctx, link)
    // {
    drain_io(zctrlr->mWorker->ns_ctx);
    cleanup_ns_worker_ctx(zctrlr->mWorker->ns_ctx);

    std::vector<uint64_t> deltas1;
    for (int i = 0; i < zctrlr->mWorker->ns_ctx->stimes.size(); i++) {
        deltas1.push_back(std::chrono::duration_cast<std::chrono::microseconds>(
                              zctrlr->mWorker->ns_ctx->etimes[i] -
                              zctrlr->mWorker->ns_ctx->stimes[i])
                              .count());
    }
    auto sum1 = std::accumulate(deltas1.begin(), deltas1.end(), 0.0);
    auto mean1 = sum1 / deltas1.size();
    auto sq_sum1 = std::inner_product(deltas1.begin(), deltas1.end(),
                                      deltas1.begin(), 0.0);
    auto stdev1 = std::sqrt(sq_sum1 / deltas1.size() - mean1 * mean1);
    printf("qd: %d, mean %f, std %f\n", zctrlr->mWorker->ns_ctx->io_completed,
           mean1, stdev1);

    // clearnup
    deltas1.clear();
    zctrlr->mWorker->ns_ctx->etimes.clear();
    zctrlr->mWorker->ns_ctx->stimes.clear();
    // }

    return 0;
}

static void usage(char *program_name)
{
    printf("%s options", program_name);
    printf("\t\n");
    printf("\t[-d DPDK huge memory size in MB]\n");
    printf("\t[-q io depth]\n");
    printf("\t[-o io size in bytes]\n");
    printf("\t[-w io pattern type, must be one of\n");
    printf("\t\t(read, write, randread, randwrite, rw, randrw)]\n");
    printf("\t[-M rwmixread (100 for reads, 0 for writes)]\n");
#ifdef DEBUG
    printf("\t[-L enable debug logging]\n");
#else
    printf("\t[-L enable debug logging (flag disabled, must reconfigure with "
           "--enable-debug)]\n");
#endif
    spdk_log_usage(stdout, "\t\t-L");
    printf("\t[-l enable latency tracking, default: disabled]\n");
    printf("\t\t(0 - disabled; 1 - enabled)\n");
    printf("\t[-t time in seconds]\n");
    printf("\t[-c core mask for I/O submission/completion.]\n");
    printf("\t\t(default: 0xf - 4 cores)]\n");
    printf("\t[-m max completions per poll]\n");
    printf("\t\t(default: 0 - unlimited)\n");
    printf("\t[-a arbitration mechanism, must be one of below]\n");
    printf("\t\t(0, 1, 2)]\n");
    printf("\t\t(0: default round robin mechanism)]\n");
    printf("\t\t(1: weighted round robin mechanism)]\n");
    printf("\t\t(2: vendor specific mechanism)]\n");
    printf("\t[-b enable arbitration user configuration, default: disabled]\n");
    printf("\t\t(0 - disabled; 1 - enabled)\n");
    printf("\t[-n subjected IOs for performance comparison]\n");
    printf("\t[-i shared memory group ID]\n");
    printf("\t[-r remote NVMe over Fabrics target address]\n");
    printf("\t[-g use single file descriptor for DPDK memory segments]\n");
}

static void print_configuration(char *program_name)
{
    printf("%s run with configuration:\n", program_name);
    printf("%s -q %d -s %d -w %s -M %d -l %d -t %d -c %s -m %d -a %d -b %d -n "
           "%d -i %d\n",
           program_name, g_arbitration.queue_depth, g_arbitration.io_size_bytes,
           g_arbitration.workload_type, g_arbitration.rw_percentage,
           g_arbitration.latency_tracking_enable, g_arbitration.time_in_sec,
           g_arbitration.core_mask, g_arbitration.max_completions,
           g_arbitration.arbitration_mechanism,
           g_arbitration.arbitration_config, g_arbitration.io_count,
           g_arbitration.shm_id);
}

static void print_performance(void *args)
{
    ZstoreController *zctrlr = (ZstoreController *)args;
    float io_per_second, sent_all_io_in_secs;
    // struct worker_thread *worker;
    // struct ns_worker_ctx *ns_ctx;

    // TAILQ_FOREACH(worker, &mWorkers, link)
    // {
    //     TAILQ_FOREACH(ns_ctx, &worker->ns_ctx, link)
    //     {
    io_per_second = (float)zctrlr->mWorker->ns_ctx->io_completed /
                    g_arbitration.time_in_sec;
    sent_all_io_in_secs = g_arbitration.io_count / io_per_second;
    // printf("%-43.43s core %u: %8.2f IO/s %8.2f secs/%d ios\n",
    //        ns_ctx->entry->name, worker->lcore, io_per_second,
    //        sent_all_io_in_secs, g_arbitration.io_count);
    printf("%8.2f IO/s %8.2f secs/%d ios\n", io_per_second, sent_all_io_in_secs,
           g_arbitration.io_count);
    //     }
    // }
    printf("========================================================\n");

    printf("\n");
}

static void print_stats(void *args)
{
    ZstoreController *zctrlr = (ZstoreController *)args;
    print_performance(zctrlr);
}

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
            g_arbitration.core_mask = optarg;
            break;
        case 'd':
            g_dpdk_mem = spdk_strtol(optarg, 10);
            if (g_dpdk_mem < 0) {
                fprintf(stderr, "Invalid DPDK memory size\n");
                return g_dpdk_mem;
            }
            break;
        case 'w':
            g_arbitration.workload_type = optarg;
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
        case '?':
            usage(argv[0]);
            return 1;
        case 'L':
            rc = spdk_log_set_flag(optarg);
            if (rc < 0) {
                fprintf(stderr, "unknown flag\n");
                usage(argv[0]);
                exit(EXIT_FAILURE);
            }
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
            case 'i':
                g_arbitration.shm_id = val;
                break;
            case 'l':
                g_arbitration.latency_tracking_enable = val;
                break;
            case 'm':
                g_arbitration.max_completions = val;
                break;
            case 'q':
                g_arbitration.queue_depth = val;
                break;
            case 'o':
                g_arbitration.io_size_bytes = val;
                break;
            case 't':
                g_arbitration.time_in_sec = val;
                break;
            case 'M':
                g_arbitration.rw_percentage = val;
                mix_specified = true;
                break;
            case 'a':
                g_arbitration.arbitration_mechanism = val;
                break;
            case 'b':
                g_arbitration.arbitration_config = val;
                break;
            case 'n':
                g_arbitration.io_count = val;
                break;
            default:
                usage(argv[0]);
                return -EINVAL;
            }
        }
    }

    workload_type = g_arbitration.workload_type;

    if (strcmp(workload_type, "read") && strcmp(workload_type, "write") &&
        strcmp(workload_type, "randread") &&
        strcmp(workload_type, "randwrite") && strcmp(workload_type, "rw") &&
        strcmp(workload_type, "randrw")) {
        fprintf(stderr, "io pattern type must be one of\n"
                        "(read, write, randread, randwrite, rw, randrw)\n");
        return 1;
    }

    if (!strcmp(workload_type, "read") || !strcmp(workload_type, "randread")) {
        g_arbitration.rw_percentage = 100;
    }

    if (!strcmp(workload_type, "write") ||
        !strcmp(workload_type, "randwrite")) {
        g_arbitration.rw_percentage = 0;
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
        if (g_arbitration.rw_percentage < 0 ||
            g_arbitration.rw_percentage > 100) {
            fprintf(stderr, "-M must be specified to value from 0 to 100 "
                            "for rw or randrw.\n");
            return 1;
        }
    }

    if (!strcmp(workload_type, "read") || !strcmp(workload_type, "write") ||
        !strcmp(workload_type, "rw")) {
        g_arbitration.is_random = 0;
    } else {
        printf("workload is random\n");
        g_arbitration.is_random = 1;
    }

    if (g_arbitration.latency_tracking_enable != 0 &&
        g_arbitration.latency_tracking_enable != 1) {
        fprintf(stderr, "-l must be specified to value 0 or 1.\n");
        return 1;
    }

    switch (g_arbitration.arbitration_mechanism) {
    case SPDK_NVME_CC_AMS_RR:
    case SPDK_NVME_CC_AMS_WRR:
    case SPDK_NVME_CC_AMS_VS:
        break;
    default:
        fprintf(stderr, "-a must be specified to value 0, 1, or 7.\n");
        return 1;
    }

    if (g_arbitration.arbitration_config != 0 &&
        g_arbitration.arbitration_config != 1) {
        fprintf(stderr, "-b must be specified to value 0 or 1.\n");
        return 1;
    } else if (g_arbitration.arbitration_config == 1 &&
               g_arbitration.arbitration_mechanism != SPDK_NVME_CC_AMS_WRR) {
        fprintf(stderr, "-a must be specified to 1 (WRR) together.\n");
        return 1;
    }

    return 0;
}

static int register_workers(void *args)
{
    ZstoreController *zctrlr = (ZstoreController *)args;
    uint32_t i = 1;
    // struct worker_thread *worker;
    enum spdk_nvme_qprio qprio = SPDK_NVME_QPRIO_URGENT;

    // SPDK_ENV_FOREACH_CORE(i)
    // {
    zctrlr->mWorker =
        (struct worker_thread *)calloc(1, sizeof(*zctrlr->mWorker));
    if (zctrlr->mWorker == NULL) {
        fprintf(stderr, "Unable to allocate worker\n");
        return -1;
    }

    // TAILQ_INIT(&worker->ns_ctx);
    zctrlr->mWorker->lcore = i;
    // TAILQ_INSERT_TAIL(&mWorkers, worker, link);
    g_arbitration.num_workers++;

    if (g_arbitration.arbitration_mechanism == SPDK_NVME_CAP_AMS_WRR) {
        qprio = static_cast<enum spdk_nvme_qprio>(static_cast<int>(qprio) + 1);
    }

    zctrlr->mWorker->qprio = static_cast<enum spdk_nvme_qprio>(
        qprio & SPDK_NVME_CREATE_IO_SQ_QPRIO_MASK);
    // }

    return 0;
}

static void zns_dev_init(struct arb_context *ctx, std::string ip1,
                         std::string port1, void *args)
{
    int rc = 0;

    ZstoreController *zctrlr = (ZstoreController *)args;
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

    register_ctrlr(spdk_nvme_connect(&trid1, &opts, sizeof(opts)), zctrlr);
    // register_ctrlr(spdk_nvme_connect(&trid2, &opts, sizeof(opts)));

    printf("Found %d namspaces\n", g_arbitration.num_namespaces);
}

static int register_controllers(struct arb_context *ctx, void *args)
{
    ZstoreController *zctrlr = (ZstoreController *)args;
    printf("Initializing NVMe Controllers\n");

    // RDMA
    // zns_dev_init(ctx, "192.168.100.9", "5520");
    // TCP
    zns_dev_init(ctx, "12.12.12.2", "5520", zctrlr);

    if (g_arbitration.num_namespaces == 0) {
        fprintf(stderr, "No valid namespaces to continue IO testing\n");
        return 1;
    }

    return 0;
}

static void unregister_controllers(void *args)
{
    ZstoreController *zctrlr = (ZstoreController *)args;
    // struct ctrlr_entry *entry, *tmp;
    struct spdk_nvme_detach_ctx *detach_ctx = NULL;

    // TAILQ_FOREACH_SAFE(entry, &mControllers, link, tmp)
    // {
    //     TAILQ_REMOVE(&mControllers, entry, link);

    spdk_nvme_detach_async(zctrlr->mController->ctrlr, &detach_ctx);
    free(zctrlr->mController);
    // }

    while (detach_ctx && spdk_nvme_detach_poll_async(detach_ctx) == -EAGAIN) {
        ;
    }
}

static int associate_workers_with_ns(void *args)
{
    ZstoreController *zctrlr = (ZstoreController *)args;
    // struct ns_entry *entry = mNamespace;
    // struct worker_thread *worker = mWorker;
    struct ns_worker_ctx *ns_ctx;
    int i, count;

    count = g_arbitration.num_namespaces > g_arbitration.num_workers
                ? g_arbitration.num_namespaces
                : g_arbitration.num_workers;
    count = 1;
    printf("DEBUG ns %d, workers %d, count %d\n", g_arbitration.num_namespaces,
           g_arbitration.num_workers, count);
    // for (i = 0; i < count; i++) {
    if (zctrlr->mNamespace == NULL) {
        return -1;
    }

    ns_ctx = (struct ns_worker_ctx *)malloc(sizeof(struct ns_worker_ctx));
    if (!ns_ctx) {
        return 1;
    }
    memset(ns_ctx, 0, sizeof(*ns_ctx));

    printf("Associating %s with lcore %d\n", zctrlr->mNamespace->name,
           zctrlr->mWorker->lcore);
    ns_ctx->entry = zctrlr->mNamespace;
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

    zctrlr->mWorker->ns_ctx = ns_ctx;
    return 0;
}

static void zstore_cleanup(uint32_t task_count, void *args)
{

    ZstoreController *zctrlr = (ZstoreController *)args;
    unregister_controllers(zctrlr);
    cleanup(task_count, zctrlr);

    spdk_env_fini();

    // if (rc != 0) {
    //     fprintf(stderr, "%s: errors occurred\n", argv[0]);
    // }
}
