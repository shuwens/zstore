#include "include/utils.hpp"
#include "include/zns_device.h"
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

// static const char *print_qprio(enum spdk_nvme_qprio);

// static int g_dpdk_mem = 0;
// static bool g_dpdk_mem_single_seg = false;

static void register_ns(struct spdk_nvme_ctrlr *ctrlr, struct spdk_nvme_ns *ns)
{
    log_debug("resiger ns");
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
    log_debug("resiger ns ends");
}

static void register_ctrlr(struct spdk_nvme_ctrlr *ctrlr)
{
    log_debug("resiger ctrlr");
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
        register_ns(ctrlr, ns);
    }
    log_debug("resiger ctrlr ends");
}

static int register_workers(void)
{
    log_debug("resiger workers");
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
    }

    log_debug("resiger workers ends ");
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
        SPDK_NOTICELOG("Successfully started the application\n");

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
    register_ctrlr(spdk_nvme_connect(&trid2, &opts, sizeof(opts)));

    /*
    if (ctx->m2.ctrlr == NULL && ctx->verbose) {
        fprintf(stderr,
                "spdk_nvme_connect() failed for transport address '%s'\n",
                ctx->m2.g_trid.traddr);
        spdk_app_stop(-1);
        // pthread_kill(g_fuzz_td, SIGSEGV);
        // return NULL;
        // return rc;
    }

    // SPDK_NOTICELOG("Successfully started the application\n");
    // SPDK_NOTICELOG("Initializing NVMe controller\n");

    if (spdk_nvme_zns_ctrlr_get_data(ctx->m2.ctrlr) && ctx->verbose) {
        printf("ZNS Specific Controller Data\n");
        printf("============================\n");
        printf("Zone Append Size Limit:      %u\n",
               spdk_nvme_zns_ctrlr_get_data(ctx->m2.ctrlr)->zasl);
        printf("\n");
        printf("\n");

        printf("Active Namespaces\n");
        printf("=================\n");
        // for (nsid = spdk_nvme_ctrlr_get_first_active_ns(ctx->ctrlr); nsid !=
        // 0;
        //      nsid = spdk_nvme_ctrlr_get_next_active_ns(ctx->ctrlr, nsid)) {
        //     print_namespace(ctx->ctrlr,
        //                     spdk_nvme_ctrlr_get_ns(ctx->ctrlr, nsid));
        // }
    }
    // ctx->ns = spdk_nvme_ctrlr_get_ns(ctx->ctrlr, 1);

    // NOTE: must find zns ns
    // take any ZNS namespace, we do not care which.
    for (int nsid = spdk_nvme_ctrlr_get_first_active_ns(ctx->m1.ctrlr);
         nsid != 0;
         nsid = spdk_nvme_ctrlr_get_next_active_ns(ctx->m1.ctrlr, nsid)) {

        struct spdk_nvme_ns *ns = spdk_nvme_ctrlr_get_ns(ctx->m1.ctrlr, nsid);
        if (ns == NULL) {
            continue;
        }
        if (spdk_nvme_ns_get_csi(ns) != SPDK_NVME_CSI_ZNS) {
            continue;
        }

        if (ctx->m1.ns == NULL) {
            log_info("Found namespace {}, connect to device manger m1", nsid);
            ctx->m1.ns = ns;
        } else if (ctx->m2.ns == NULL) {
            log_info("Found namespace {}, connect to device manger m2", nsid);
            ctx->m2.ns = ns;

        } else
            break;

        if (ctx->verbose)
            print_namespace(ctx->m1.ctrlr,
                            spdk_nvme_ctrlr_get_ns(ctx->m1.ctrlr, nsid),
                            ctx->current_zone);
    }

    if (ctx->m1.ns == NULL) {
        SPDK_ERRLOG("Could not get NVMe namespace\n");
        spdk_app_stop(-1);
        return;
    }
    */
}

static int register_controllers(struct zstore_context *ctx)
{
    printf("Initializing NVMe Controllers\n");

    zns_dev_init(ctx, "192.168.1.121", "4420", "192.168.1.121", "5520");
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

static void __m_append_complete(void *ctx, const struct spdk_nvme_cpl *cpl)
{
    struct ns_worker_ctx *ns_ctx = static_cast<struct ns_worker_ctx *>(ctx);

    if (spdk_nvme_cpl_is_error(cpl)) {
        spdk_nvme_qpair_print_completion(ns_ctx->qpair,
                                         (struct spdk_nvme_cpl *)cpl);
        log_error("Completion failed {}",
                  spdk_nvme_cpl_get_status_string(&cpl->status));
        return;
    } else {
        ns_ctx->etime = std::chrono::high_resolution_clock::now();
        ns_ctx->etimes.push_back(ns_ctx->etime);
    }
    if (ns_ctx->current_lba == 0) {
        log_info("setting current lba value: {}", cpl->cdw0);
        ns_ctx->current_lba = cpl->cdw0;
    }
}

static void __m_complete(void *ctx, const struct spdk_nvme_cpl *cpl)
{
    struct ns_worker_ctx *ns_ctx = static_cast<struct ns_worker_ctx *>(ctx);

    if (spdk_nvme_cpl_is_error(cpl)) {
        spdk_nvme_qpair_print_completion(ns_ctx->qpair,
                                         (struct spdk_nvme_cpl *)cpl);
        log_error("Completion failed {}",
                  spdk_nvme_cpl_get_status_string(&cpl->status));
        return;
    } else {
        ns_ctx->etime = std::chrono::high_resolution_clock::now();
        ns_ctx->etimes.push_back(ns_ctx->etime);
    }
}

int measure_read(struct ns_worker_ctx *ns_ctx, uint64_t slba, void *buffer,
                 uint64_t size)
{
    ERROR_ON_NULL(ns_ctx->qpair, 1);
    ERROR_ON_NULL(buffer, 1);
    int rc = 0;

    int lbas = (size + ns_ctx->info.lba_size - 1) / ns_ctx->info.lba_size;
    int lbas_processed = 0;
    int step_size = (ns_ctx->info.mdts / ns_ctx->info.lba_size);
    int current_step_size = step_size;
    int slba_start = slba;
    if (ns_ctx->verbose)
        log_info("\nmeasure_read: lbas {}, step size {}, slba start {} \n",
                 lbas, current_step_size, slba_start);

    while (lbas_processed < lbas) {
        if ((slba + lbas_processed + step_size) / ns_ctx->info.zone_size >
            (slba + lbas_processed) / ns_ctx->info.zone_size) {
            current_step_size =
                ((slba + lbas_processed + step_size) / ns_ctx->info.zone_size) *
                    ns_ctx->info.zone_size -
                lbas_processed - slba;
        } else {
            current_step_size = step_size;
        }
        current_step_size = lbas - lbas_processed > current_step_size
                                ? current_step_size
                                : lbas - lbas_processed;
        if (ns_ctx->verbose)
            log_info("{} step {}  \n", slba_start, current_step_size);
        if (ns_ctx->verbose)
            log_info(
                "cmd_read: slba_start {}, current step size {}, queued {} ",
                slba_start, current_step_size, ns_ctx->current_queue_depth);
        ns_ctx->stime = std::chrono::high_resolution_clock::now();
        ns_ctx->stimes.push_back(ns_ctx->stime);
        rc = spdk_nvme_ns_cmd_read(ns_ctx->entry->nvme.ns, ns_ctx->qpair,
                                   (char *)buffer +
                                       lbas_processed * ns_ctx->info.lba_size,
                                   slba_start,        /* LBA start */
                                   current_step_size, /* number of LBAs */
                                   __m_complete, ns_ctx, 0);
        if (rc != 0) {
            // log_error("cmd read error: {}", rc);
            // if (rc == -ENOMEM) {
            //     spdk_nvme_qpair_process_completions(ctx->qpair, 0);
            //     rc = 0;
            // } else
            return 1;
        }

        lbas_processed += current_step_size;
        slba_start = slba + lbas_processed;

        // while (ns_ctx->current_queue_depth) {
        //     ret = spdk_nvme_qpair_process_completions(ns_ctx->qpair, 0);
        //     ns_ctx->num_queued -= ret;
        // }
    }

    return rc;
}

int measure_append(struct ns_worker_ctx *ns_ctx, uint64_t slba, void *buffer,
                   uint64_t size)
{
    if (ns_ctx->verbose)
        log_info("\n\nmeasure_append start: slba {}, size {}\n", slba, size);
    ERROR_ON_NULL(ns_ctx->qpair, 1);
    ERROR_ON_NULL(buffer, 1);

    int rc = 0;

    int lbas = (size + ns_ctx->info.lba_size - 1) / ns_ctx->info.lba_size;
    int lbas_processed = 0;
    int step_size = (ns_ctx->info.zasl / ns_ctx->info.lba_size);
    int current_step_size = step_size;
    int slba_start = (slba / ns_ctx->info.zone_size) * ns_ctx->info.zone_size;
    if (ns_ctx->verbose)
        log_info("measure_append: lbas {}, step size {}, slba start {} ", lbas,
                 current_step_size, slba_start);

    while (lbas_processed < lbas) {
        // Completion completion = {.done = false, .err = 0};
        if ((slba + lbas_processed + step_size) / ns_ctx->info.zone_size >
            (slba + lbas_processed) / ns_ctx->info.zone_size) {
            current_step_size =
                ((slba + lbas_processed + step_size) / ns_ctx->info.zone_size) *
                    ns_ctx->info.zone_size -
                lbas_processed - slba;
        } else {
            current_step_size = step_size;
        }
        current_step_size = lbas - lbas_processed > current_step_size
                                ? current_step_size
                                : lbas - lbas_processed;
        if (ns_ctx->verbose)
            log_info(
                "zone_append: slba start {}, current step size {}, queued {}",
                slba_start, current_step_size, ns_ctx->current_queue_depth);
        ns_ctx->stime = std::chrono::high_resolution_clock::now();
        ns_ctx->stimes.push_back(ns_ctx->stime);
        rc = spdk_nvme_zns_zone_append(
            ns_ctx->entry->nvme.ns, ns_ctx->qpair,
            (char *)buffer + lbas_processed * ns_ctx->info.lba_size,
            slba_start,        /* LBA start */
            current_step_size, /* number of LBAs */
            __m_append_complete, ns_ctx, 0);
        if (rc != 0) {
            log_error("zone append error: {}", rc);
            // if (rc == -ENOMEM) {
            //     spdk_nvme_qpair_process_completions(ctx->qpair, 0);
            //     rc = 0;
            // } else
            break;
        }

        lbas_processed += current_step_size;
        slba_start = ((slba + lbas_processed) / ns_ctx->info.zone_size) *
                     ns_ctx->info.zone_size;
    }
    // while (ns_ctx->num_queued) {
    //     // log_debug("GOOD: qpair process completion: queued {}, qd {}",
    //     //           ns_ctx->num_queued, ns_ctx->qd);
    //     ret = spdk_nvme_qpair_process_completions(ns_ctx->qpair, 0);
    //     ns_ctx->num_queued -= ret;
    // }
    return rc;
}

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
/*
// FIXME:
// DONE: add queue size as qpair options
// DONE: detect current lbas to use for reads
// TODO:
// something smart about open/close zones etc
static void zns_measure(void *arg)
{
    log_info("Fn: zns_measure \n");
    struct ZstoreContext *ctx = static_cast<struct ZstoreContext *>(arg);
    struct spdk_nvme_io_qpair_opts qpair_opts = {};

    // std::vector<int> qds{2, 4, 8, 16, 32, 64};

    // std::vector<int> qds{2}; // 143, 137
    // std::vector<int> qds{4}; //
    // std::vector<int> qds{8}; // 495
    // std::vector<int> qds{16}; //
    std::vector<int> qds{32}; //
    // std::vector<int> qds{64}; // 2345, 4340

    for (auto qd : qds) {
        log_info("\nStarting measurment with queue depth {}, append times {}\n",
                 qd, append_times);
        ctx->qd = qd;
        zns_dev_init(ctx, "192.168.1.121", "4420", "192.168.1.121", "5520");

        // default
        // qpair_opts.io_queue_size = 128;
        // qpair_opts.io_queue_requests = 512;
        qpair_opts.io_queue_size = 64;
        qpair_opts.io_queue_requests = 64 * 4;

        zstore_qpair_setup(ctx, qpair_opts);
        zstore_init(ctx);

        z_get_device_info(&ctx->m1, ctx->verbose);
        z_get_device_info(&ctx->m2, ctx->verbose);
        ctx->m1.zstore_open = true;
        ctx->m2.zstore_open = true;

        ctx->m1.qd = qd;
        ctx->m2.qd = qd;

        // working
        int rc = 0;
        int rc1 = 0;
        int rc2 = 0;

        log_info("writing with z_append:");
        std::vector<void *> wbufs;
        for (int i = 0; i < append_times; i++) {
            char **wbuf = (char **)calloc(1, sizeof(char **));
            rc = write_zstore_pattern(wbuf, &ctx->m1, ctx->m2.info.lba_size,
                                      "testing", value + i);
            assert(rc == 0);
            wbufs.push_back(*wbuf);
        }

        for (int i = 0; i < append_times; i++) {
            // do multple append qd times
            // APPEND
            rc1 = measure_append(&ctx->m1, ctx->m1.zslba, wbufs[i],
                                 ctx->m1.info.lba_size);
            rc2 = measure_append(&ctx->m2, ctx->m2.zslba, wbufs[i],
                                 ctx->m2.info.lba_size);
            assert(rc1 == 0 && rc2 == 0);
            // printf("%d-th round: %d-th is %s\n", i, j, wbufs[i * qd +
            // j]);
        }
        while (ctx->m1.num_queued || ctx->m2.num_queued) {
            log_debug("Reached here to process outstanding requests");
            // log_debug("qpair queued: m1 {}, m2 {}", ctx->m1.num_queued,
            //           ctx->m2.num_queued);
            spdk_nvme_qpair_process_completions(ctx->m1.qpair, 0);
            spdk_nvme_qpair_process_completions(ctx->m2.qpair, 0);
        }
        log_debug("write is all done ");

        std::vector<u64> deltas1;
        std::vector<u64> deltas2;
        for (int i = 0; i < append_times; i++) {
            deltas1.push_back(
                std::chrono::duration_cast<std::chrono::microseconds>(
                    ctx->m1.etimes[i] - ctx->m1.stimes[i])
                    .count());
            deltas2.push_back(
                std::chrono::duration_cast<std::chrono::microseconds>(
                    ctx->m2.etimes[i] - ctx->m2.stimes[i])
                    .count());
        }
        auto sum1 = std::accumulate(deltas1.begin(), deltas1.end(), 0.0);
        auto sum2 = std::accumulate(deltas2.begin(), deltas2.end(), 0.0);
        auto mean1 = sum1 / deltas1.size();
        auto mean2 = sum2 / deltas2.size();
        auto sq_sum1 = std::inner_product(deltas1.begin(), deltas1.end(),
                                          deltas1.begin(), 0.0);
        auto sq_sum2 = std::inner_product(deltas2.begin(), deltas2.end(),
                                          deltas2.begin(), 0.0);
        auto stdev1 = std::sqrt(sq_sum1 / deltas1.size() - mean1 * mean1);
        auto stdev2 = std::sqrt(sq_sum2 / deltas2.size() - mean2 * mean2);
        log_info("WRITES-1 qd {}, append: mean {} us, std {}", ctx->qd, mean1,
                 stdev1);
        log_info("WRITES-2 qd {}, append: mean {} us, std {}", ctx->qd, mean2,
                 stdev2);

        // clearnup
        deltas1.clear();
        deltas2.clear();
        ctx->m1.stimes.clear();
        ctx->m1.etimes.clear();
        ctx->m2.stimes.clear();
        ctx->m2.etimes.clear();

        log_info("current lba for read: device 1 {}, device 2 {}",
                 ctx->m1.current_lba, ctx->m2.current_lba);
        log_info("\nread with z_append:");

        std::vector<int> vec;
        vec.reserve(append_times);
        for (int i = 0; i < append_times; i++) {
            vec.push_back(i);
        }

        auto rng = std::default_random_engine{};
        std::shuffle(std::begin(vec), std::end(vec), rng);

        char *rbuf1 =
            (char *)z_calloc(&ctx->m1, ctx->m1.info.lba_size, sizeof(char *));
        char *rbuf2 =
            (char *)z_calloc(&ctx->m2, ctx->m2.info.lba_size, sizeof(char *));
        for (const auto &i : vec) {
            rc = measure_read(&ctx->m1, ctx->m1.current_lba + i, rbuf1,
                              ctx->m1.info.lba_size);
            assert(rc == 0);
            rc = measure_read(&ctx->m2, ctx->m2.current_lba + i, rbuf2,
                              ctx->m2.info.lba_size);
            assert(rc == 0);
            // printf("%d-th round: %d-th is %s, %s\n", i, j, rbuf1, rbuf2);

            // rc = measure_read(&ctx->m1, ctx->m1.current_lba + i, rbuf1,
            // 4096); assert(rc == 0); rc = measure_read(&ctx->m2,
            // ctx->m2.current_lba + i, rbuf2, 4096); assert(rc == 0);

            // printf("m1: %s, m2: %s\n", rbuf1, rbuf2);
        }
        while (ctx->m1.num_queued || ctx->m2.num_queued) {
            // log_debug("qpair queued: m1 {}, m2 {}",
            // ctx->m1.num_queued,
            //           ctx->m2.num_queued);

            spdk_nvme_qpair_process_completions(ctx->m1.qpair, 0);
            spdk_nvme_qpair_process_completions(ctx->m2.qpair, 0);
        }
        log_debug("m1: {}, {}", ctx->m1.stimes.size(), ctx->m1.etimes.size());
        log_debug("m2: {}, {}", ctx->m2.stimes.size(), ctx->m2.etimes.size());
        for (int i = 0; i < append_times; i++) {
            deltas1.push_back(
                std::chrono::duration_cast<std::chrono::microseconds>(
                    ctx->m1.etimes[i] - ctx->m1.stimes[i])
                    .count());
            deltas2.push_back(
                std::chrono::duration_cast<std::chrono::microseconds>(
                    ctx->m2.etimes[i] - ctx->m2.stimes[i])
                    .count());
        }

        log_debug("deltas: m1 {}, m2 {}", deltas1.size(), deltas2.size());
        sum1 = std::accumulate(deltas1.begin(), deltas1.end(), 0.0);
        sum2 = std::accumulate(deltas2.begin(), deltas2.end(), 0.0);
        mean1 = sum1 / deltas1.size();
        mean2 = sum2 / deltas2.size();
        sq_sum1 = std::inner_product(deltas1.begin(), deltas1.end(),
                                     deltas1.begin(), 0.0);
        sq_sum2 = std::inner_product(deltas2.begin(), deltas2.end(),
                                     deltas2.begin(), 0.0);
        stdev1 = std::sqrt(sq_sum1 / deltas1.size() - mean1 * mean1);
        stdev2 = std::sqrt(sq_sum2 / deltas2.size() - mean2 * mean2);
        log_info("READ-1 qd {}, append: mean {} us, std {}", ctx->qd, mean1,
                 stdev1);
        log_info("READ-2 qd {}, append: mean {} us, std {}", ctx->qd, mean2,
                 stdev2);

        deltas1.clear();
        deltas2.clear();
        // log_info("qd {}, read: mean {} us, std {}", ctx->qd, mean,
        // stdev);

        zstore_qpair_teardown(ctx);
    }

    log_info("Test start finish");
}

static void test_cleanup(void)
{
    printf("test_abort\n");
    // g_app_stopped = true;
    // spdk_poller_unregister(&test_end_poller);
    spdk_app_stop(0);
}

*/

// static void task_complete(struct zstore_task *task)
// {
//     struct ns_worker_ctx *ns_ctx;
//
//     ns_ctx = task->ns_ctx;
//     ns_ctx->current_queue_depth--;
//     ns_ctx->io_completed++;
//
//     spdk_dma_free(task->buf);
//     spdk_mempool_put(task_pool, task);
//
//     /*
//      * is_draining indicates when time has expired for the test run
//      * and we are just waiting for the previously submitted I/O
//      * to complete.  In this case, do not submit a new I/O to replace
//      * the one just completed.
//      */
//     if (!ns_ctx->is_draining) {
//         submit_single_io(ns_ctx);
//     }
// }
//

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

static int init_ns_worker_ctx(struct ns_worker_ctx *ns_ctx,
                              enum spdk_nvme_qprio qprio)
{
    struct spdk_nvme_ctrlr *ctrlr = ns_ctx->entry->nvme.ctrlr;
    struct spdk_nvme_io_qpair_opts opts;

    spdk_nvme_ctrlr_get_default_io_qpair_opts(ctrlr, &opts, sizeof(opts));
    opts.qprio = qprio;

    ns_ctx->qpair = spdk_nvme_ctrlr_alloc_io_qpair(ctrlr, &opts, sizeof(opts));
    if (!ns_ctx->qpair) {
        printf("ERROR: spdk_nvme_ctrlr_alloc_io_qpair failed\n");
        return 1;
    }

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

    // printf("Starting thread on core %u with %s\n", worker->lcore,
    //        print_qprio(worker->qprio));

    /* Allocate a queue pair for each namespace. */
    TAILQ_FOREACH(ns_ctx, &worker->ns_ctx, link)
    {
        if (init_ns_worker_ctx(ns_ctx, worker->qprio) != 0) {
            printf("ERROR: init_ns_worker_ctx() failed\n");
            return 1;
        }
    }

    // tsc_end = spdk_get_ticks() + g_zstore.time_in_sec * g_zstore.tsc_rate;

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

        if (ns_ctx->total_ops > append_times) {
            break;
        }
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
    struct worker_thread *worker, *main_worker;
    unsigned main_core;
    char task_pool_name[30];
    uint32_t task_count = 0;
    struct spdk_env_opts opts;

    if (register_workers() != 0) {
        rc = 1;
        log_info("zstore exits gracefully");
        unregister_controllers();
        cleanup(task_count);

        spdk_env_fini();

        if (rc != 0) {
            fprintf(stderr, "%s: errors occurred\n", argv[0]);
        }

        return rc;
    }

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
        log_info("zstore exits gracefully");
        unregister_controllers();
        cleanup(task_count);

        spdk_env_fini();

        if (rc != 0) {
            fprintf(stderr, "%s: errors occurred\n", argv[0]);
        }

        return rc;
    }
    if (associate_workers_with_ns(current_zone) != 0) {
        rc = 1;
        log_info("zstore exits gracefully");
        unregister_controllers();
        cleanup(task_count);

        spdk_env_fini();

        if (rc != 0) {
            fprintf(stderr, "%s: errors occurred\n", argv[0]);
        }

        return rc;
    }

    spdk_env_opts_init(&opts);
    // struct spdk_app_opts opts = {};
    // spdk_app_opts_init(&opts, sizeof(opts));
    opts.name = "zns_measurement_opts";
    opts.mem_size = g_dpdk_mem;
    opts.hugepage_single_segments = g_dpdk_mem_single_seg;
    opts.core_mask = g_zstore.core_mask;
    // opts.shm_id = g_zstore.shm_id;
    if (spdk_env_init(&opts) < 0) {
        return 1;
    }

    // if ((rc = spdk_app_parse_args(argc, argv, &opts, NULL, NULL, NULL, NULL))
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
        log_info("zstore exits gracefully");
        unregister_controllers();
        cleanup(task_count);

        spdk_env_fini();

        if (rc != 0) {
            fprintf(stderr, "%s: errors occurred\n", argv[0]);
        }

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
    TAILQ_FOREACH(worker, &g_workers, link)
    {
        if (worker->lcore != main_core) {
            spdk_env_thread_launch_pinned(worker->lcore, work_fn, worker);
        } else {
            assert(main_worker == NULL);
            main_worker = worker;
        }
    }

    assert(main_worker != NULL);
    rc = work_fn(main_worker);

    spdk_env_thread_wait_all();

    // print_stats();

    log_info("zstore exits gracefully");
    unregister_controllers();
    cleanup(task_count);

    spdk_env_fini();

    if (rc != 0) {
        fprintf(stderr, "%s: errors occurred\n", argv[0]);
    }

    return rc;
}
