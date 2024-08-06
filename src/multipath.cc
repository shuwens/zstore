#include "include/utils.hpp"
#include "include/zns_device.h"
#include "spdk/event.h"
#include "spdk/log.h"
#include "spdk/nvme.h"
#include <bits/stdc++.h>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <fmt/core.h>
#include <fstream>
#include <iostream>
#include <numeric>
#include <stdio.h>
#include <vector>

// using chrono_tp = std::chrono::high_resolution_clock::time_point;

int write_zstore_pattern(char **pattern, void *arg, int32_t size,
                         char *test_str, int value)
{
    DeviceManager *dm = static_cast<DeviceManager *>(arg);
    if (*pattern != NULL) {
        z_free(dm->qpair, *pattern);
    }
    *pattern = (char *)z_calloc(dm, size, sizeof(char *));
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

    std::vector<int> qds{64};
    // std::vector<int> qds{2, 4, 8, 16, 32, 64};

    for (auto qd : qds) {
        log_info("\nStarting measurment with queue depth {}, append times {}\n",
                 qd, append_times);
        ctx->qd = qd;
        qpair_opts.io_queue_size = ctx->qd;
        qpair_opts.io_queue_requests = ctx->qd;
        zns_dev_init(ctx, "192.168.1.121", "4420", "192.168.1.121", "5520");

        zstore_qpair_setup(ctx, qpair_opts);
        zstore_init(ctx);

        z_get_device_info(&ctx->m1, ctx->verbose);
        z_get_device_info(&ctx->m2, ctx->verbose);

        ctx->m1.zstore_open = true;
        ctx->m2.zstore_open = true;

        // zone cap * lba_bytes ()
        // log_info("zone cap: {}, lba bytes {}", ctx->info.zone_cap,
        //          ctx->info.lba_size);
        // uint32_t buf_align = ctx->info.lba_size;
        // log_info("buffer size: {}, align {}", ctx->buff_size, buf_align);
        //
        // log_info("block size: {}, write unit: {}, zone size: {}, zone num: "
        //          "{}, max append size: {},  max open "
        //          "zone: {}, max active zone: {}\n ",
        //          spdk_nvme_ns_get_sector_size(ctx->ns),
        //          spdk_nvme_ns_get_md_size(ctx->ns),
        //          spdk_nvme_zns_ns_get_zone_size_sectors(ctx->ns), // zone
        //          size spdk_nvme_zns_ns_get_num_zones(ctx->ns),
        //          spdk_nvme_zns_ctrlr_get_max_zone_append_size(ctx->ctrlr) /
        //              spdk_nvme_ns_get_sector_size(ctx->ns),
        //          spdk_nvme_zns_ns_get_max_open_zones(ctx->ns),
        //          spdk_nvme_zns_ns_get_max_active_zones(ctx->ns));

        // working
        int rc = 0;

        // uint64_t write_head = 0;
        // rc = z_get_zone_head(ctx, ctx->current_zone, &write_head);
        // assert(rc == 0);
        // log_info("current zone: {}, current lba {}, head {}",
        // ctx->current_zone,
        //          ctx->current_lba, write_head);
        // // FIXME:
        // ctx->current_lba = write_head;

        // measurment time points
        chrono_tp stime;
        chrono_tp etime;
        std::vector<u64> deltas;

        log_info("writing with z_append:");
        char **wbuf = (char **)calloc(1, sizeof(char **));
        for (int i = 0; i < append_times; i++) {
            rc = write_zstore_pattern(wbuf, &ctx->m1, ctx->m1.info.lba_size, "",
                                      value + i);
            assert(rc == 0);
            rc = write_zstore_pattern(wbuf, &ctx->m2, ctx->m2.info.lba_size, "",
                                      value + i);
            assert(rc == 0);

            stime = std::chrono::high_resolution_clock::now();

            // APPEND
            rc =
                z_append(&ctx->m1, ctx->m1.zslba, *wbuf, ctx->m1.info.lba_size);
            assert(rc == 0);
            rc =
                z_append(&ctx->m2, ctx->m2.zslba, *wbuf, ctx->m2.info.lba_size);
            assert(rc == 0);

            etime = std::chrono::high_resolution_clock::now();
            auto dur = std::chrono::duration_cast<std::chrono::microseconds>(
                etime - stime);
            deltas.push_back(dur.count());

            // log_info("write {}", *wbuf);
        }
        auto sum = std::accumulate(deltas.begin(), deltas.end(), 0.0);
        auto mean = sum / deltas.size();
        auto sq_sum = std::inner_product(deltas.begin(), deltas.end(),
                                         deltas.begin(), 0.0);
        auto stdev = std::sqrt(sq_sum / deltas.size() - mean * mean);
        log_info("qd {}, append: mean {} us, std {}", ctx->qd, mean, stdev);
        deltas.clear();

        log_info("current lba for read: device 1 {}, device 2 {}",
                 ctx->m1.current_lba, ctx->m2.current_lba);
        log_info("read with z_append:");
        char *rbuf1 =
            (char *)z_calloc(&ctx->m1, ctx->m1.info.lba_size, sizeof(char *));
        char *rbuf2 =
            (char *)z_calloc(&ctx->m2, ctx->m2.info.lba_size, sizeof(char *));

        for (int i = 0; i < append_times; i++) {
            stime = std::chrono::high_resolution_clock::now();
            rc = z_read(&ctx->m1, ctx->m1.current_lba + i, rbuf1, 4096);
            assert(rc == 0);
            rc = z_read(&ctx->m2, ctx->m2.current_lba + i, rbuf2, 4096);
            assert(rc == 0);

            etime = std::chrono::high_resolution_clock::now();
            auto dur = std::chrono::duration_cast<std::chrono::microseconds>(
                etime - stime);
            deltas.push_back(dur.count());
        }

        sum = std::accumulate(deltas.begin(), deltas.end(), 0.0);
        mean = sum / deltas.size();
        sq_sum = std::inner_product(deltas.begin(), deltas.end(),
                                    deltas.begin(), 0.0);
        stdev = std::sqrt(sq_sum / deltas.size() - mean * mean);
        log_info("qd {}, read: mean {} us, std {}", ctx->qd, mean, stdev);
        deltas.clear();

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
