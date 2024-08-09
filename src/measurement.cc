#include "include/utils.hpp"
#include "include/zns_device.h"
#include "spdk/event.h"
#include "spdk/log.h"
#include "spdk/nvme.h"
#include <algorithm>
#include <bits/stdc++.h>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <fmt/core.h>
#include <fstream>
#include <iostream>
#include <iterator>
#include <numeric>
#include <random>
#include <stdio.h>
#include <vector>

#define RUNTIME_RX_BATCH_SIZE 32

static void __m_append_complete(void *arg, const struct spdk_nvme_cpl *cpl)
{
    DeviceManager *dm = static_cast<DeviceManager *>(arg);

    dm->num_completed += 1;
    // dm->num_queued -= 1;
    if (spdk_nvme_cpl_is_error(cpl)) {
        spdk_nvme_qpair_print_completion(dm->qpair,
                                         (struct spdk_nvme_cpl *)cpl);
        log_error("Completion failed {}",
                  spdk_nvme_cpl_get_status_string(&cpl->status));
        dm->num_fail += 1;
        return;
    } else {
        dm->etime = std::chrono::high_resolution_clock::now();
        dm->etimes.push_back(dm->etime);
        dm->num_success += 1;
    }
    if (dm->current_lba == 0) {
        log_info("setting current lba value: {}", cpl->cdw0);
        dm->current_lba = cpl->cdw0;
    }
}

static void __m_complete(void *arg, const struct spdk_nvme_cpl *cpl)
{
    DeviceManager *dm = static_cast<DeviceManager *>(arg);

    dm->num_completed += 1;
    // dm->num_queued -= 1;
    if (spdk_nvme_cpl_is_error(cpl)) {
        spdk_nvme_qpair_print_completion(dm->qpair,
                                         (struct spdk_nvme_cpl *)cpl);
        log_error("Completion failed {}",
                  spdk_nvme_cpl_get_status_string(&cpl->status));
        dm->num_fail += 1;
        return;
    } else {
        dm->etime = std::chrono::high_resolution_clock::now();
        dm->etimes.push_back(dm->etime);
        dm->num_success += 1;
    }
}

int measure_read(void *arg, uint64_t slba, void *buffer, uint64_t size)
{
    DeviceManager *dm = static_cast<DeviceManager *>(arg);

    ERROR_ON_NULL(dm->qpair, 1);
    ERROR_ON_NULL(buffer, 1);
    int rc = 0;
    int ret = 0;

    int lbas = (size + dm->info.lba_size - 1) / dm->info.lba_size;
    int lbas_processed = 0;
    int step_size = (dm->info.mdts / dm->info.lba_size);
    int current_step_size = step_size;
    int slba_start = slba;
    if (dm->verbose)
        log_info("\nmeasure_read: lbas {}, step size {}, slba start {} \n",
                 lbas, current_step_size, slba_start);

    while (lbas_processed < lbas) {
        if ((slba + lbas_processed + step_size) / dm->info.zone_size >
            (slba + lbas_processed) / dm->info.zone_size) {
            current_step_size =
                ((slba + lbas_processed + step_size) / dm->info.zone_size) *
                    dm->info.zone_size -
                lbas_processed - slba;
        } else {
            current_step_size = step_size;
        }
        current_step_size = lbas - lbas_processed > current_step_size
                                ? current_step_size
                                : lbas - lbas_processed;
        if (dm->verbose)
            log_info("{} step {}  \n", slba_start, current_step_size);
        dm->num_queued += 1;
        if (dm->verbose)
            log_info(
                "cmd_read: slba_start {}, current step size {}, queued {} ",
                slba_start, current_step_size, dm->num_queued);
        dm->stime = std::chrono::high_resolution_clock::now();
        dm->stimes.push_back(dm->stime);
        rc = spdk_nvme_ns_cmd_read(dm->ns, dm->qpair,
                                   (char *)buffer +
                                       lbas_processed * dm->info.lba_size,
                                   slba_start,        /* LBA start */
                                   current_step_size, /* number of LBAs */
                                   __m_complete, dm, 0);
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

        while (dm->num_queued) {
            ret = spdk_nvme_qpair_process_completions(dm->qpair, 0);
            dm->num_queued -= ret;
        }
    }

    return rc;
}

int measure_append(void *arg, uint64_t slba, void *buffer, uint64_t size)
{
    DeviceManager *dm = static_cast<DeviceManager *>(arg);
    if (dm->verbose)
        log_info("\n\nmeasure_append start: slba {}, size {}\n", slba, size);
    ERROR_ON_NULL(dm->qpair, 1);
    ERROR_ON_NULL(buffer, 1);

    int rc = 0;
    int ret = 0;

    int lbas = (size + dm->info.lba_size - 1) / dm->info.lba_size;
    int lbas_processed = 0;
    int step_size = (dm->info.zasl / dm->info.lba_size);
    int current_step_size = step_size;
    int slba_start = (slba / dm->info.zone_size) * dm->info.zone_size;
    if (dm->verbose)
        log_info("measure_append: lbas {}, step size {}, slba start {} ", lbas,
                 current_step_size, slba_start);

    while (lbas_processed < lbas) {
        // Completion completion = {.done = false, .err = 0};
        if ((slba + lbas_processed + step_size) / dm->info.zone_size >
            (slba + lbas_processed) / dm->info.zone_size) {
            current_step_size =
                ((slba + lbas_processed + step_size) / dm->info.zone_size) *
                    dm->info.zone_size -
                lbas_processed - slba;
        } else {
            current_step_size = step_size;
        }
        current_step_size = lbas - lbas_processed > current_step_size
                                ? current_step_size
                                : lbas - lbas_processed;
        dm->num_queued += 1;
        if (dm->verbose)
            log_info(
                "zone_append: slba start {}, current step size {}, queued {}",
                slba_start, current_step_size, dm->num_queued);
        dm->stime = std::chrono::high_resolution_clock::now();
        dm->stimes.push_back(dm->stime);
        rc = spdk_nvme_zns_zone_append(dm->ns, dm->qpair,
                                       (char *)buffer +
                                           lbas_processed * dm->info.lba_size,
                                       slba_start,        /* LBA start */
                                       current_step_size, /* number of LBAs */
                                       __m_append_complete, dm, 0);
        if (rc != 0) {
            log_error("zone append error: {}", rc);
            // if (rc == -ENOMEM) {
            //     spdk_nvme_qpair_process_completions(ctx->qpair, 0);
            //     rc = 0;
            // } else
            break;
        }

        lbas_processed += current_step_size;
        slba_start =
            ((slba + lbas_processed) / dm->info.zone_size) * dm->info.zone_size;
    }
    while (dm->num_queued) {
        // log_debug("GOOD: qpair process completion: queued {}, qd {}",
        //           dm->num_queued, dm->qd);
        ret = spdk_nvme_qpair_process_completions(dm->qpair, 0);
        dm->num_queued -= ret;
    }
    return rc;
}

int write_zstore_pattern(char **pattern, void *arg, int32_t size,
                         char *test_str, int value)
{
    DeviceManager *dm = static_cast<DeviceManager *>(arg);
    if (*pattern != NULL) {
        z_free(dm->qpair, *pattern);
    }
    *pattern = (char *)z_calloc(dm, dm->info.lba_size, sizeof(char *));
    if (*pattern == NULL) {
        return 1;
    }
    snprintf(*pattern, dm->info.lba_size, "%s:%d", test_str, value);
    return 0;
}

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

int main(int argc, char **argv)
{
    // NOTE: we switch between zones and keep track of it with a file
    int current_zone = 0;
    std::ifstream inputFile("../current_zone");
    if (inputFile.is_open()) {
        inputFile >> current_zone;
        inputFile.close();
    }
    log_info("Zstore start with current zone: {}", current_zone);

    int rc = 0;

    struct spdk_app_opts opts = {};
    spdk_app_opts_init(&opts, sizeof(opts));
    opts.name = "zns_measurement_opts";
    // opts.shutdown_cb = NULL;
    opts.shutdown_cb = test_cleanup;
    if ((rc = spdk_app_parse_args(argc, argv, &opts, NULL, NULL, NULL, NULL)) !=
        SPDK_APP_PARSE_ARGS_SUCCESS) {
        exit(rc);
    }

    struct ZstoreContext ctx = {};
    ctx.current_zone = current_zone;
    // ctx.verbose = true;

    rc = spdk_app_start(&opts, zns_measure, &ctx);
    if (rc) {
        SPDK_ERRLOG("ERROR starting application\n");
    }

    log_info("zstore exits gracefully");
    spdk_app_fini();

    return rc;
}
