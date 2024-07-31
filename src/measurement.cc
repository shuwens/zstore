#include "include/utils.hpp"
#include "include/zns_device.h"
#include "spdk/env.h"
#include "spdk/event.h"
#include "spdk/log.h"
#include "spdk/nvme.h"
#include "spdk/nvme_zns.h"
#include <bits/stdc++.h>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <fmt/core.h>
#include <fstream>
#include <iostream>
#include <stdio.h>

// using chrono_tp = std::chrono::high_resolution_clock::time_point;

int write_zstore_pattern(char **pattern, void *arg, int32_t size,
                         char *test_str, int value)
{
    struct ZstoreContext *ctx = static_cast<struct ZstoreContext *>(arg);
    if (*pattern != NULL) {
        z_free(ctx->qpair, *pattern);
    }
    *pattern = (char *)z_calloc(ctx, size, sizeof(char *));
    if (*pattern == NULL) {
        return 1;
    }
    snprintf(*pattern, ctx->info.lba_size, "%s:%d", test_str, value);
    return 0;
}

static void test_start(void *arg1)
{
    log_info("test start\n");
    struct ZstoreContext *ctx = static_cast<struct ZstoreContext *>(arg1);

    zns_dev_init(ctx);

    zstore_init(ctx);

    z_get_device_info(ctx);

    ctx->zstore_open = true;

    // zone cap * lba_bytes ()
    log_info("zone cap: {}, lba bytes {}", ctx->info.zone_cap,
             ctx->info.lba_size);
    ctx->buff_size = ctx->info.lba_size * append_times;
    uint32_t buf_align = ctx->info.lba_size;
    log_info("buffer size: {}, align {}", ctx->buff_size, buf_align);

    ctx->write_buff = (char *)spdk_zmalloc(
        ctx->buff_size, 0, NULL, SPDK_ENV_SOCKET_ID_ANY, SPDK_MALLOC_DMA);
    if (!ctx->write_buff) {
        SPDK_ERRLOG("Failed to allocate buffer\n");
        spdk_nvme_detach(ctx->ctrlr);
        spdk_app_stop(-1);
        return;
    }
    ctx->read_buff = (char *)spdk_zmalloc(
        ctx->buff_size, 0, NULL, SPDK_ENV_SOCKET_ID_ANY, SPDK_MALLOC_DMA);
    if (!ctx->read_buff) {
        SPDK_ERRLOG("Failed to allocate buffer\n");
        spdk_nvme_detach(ctx->ctrlr);
        spdk_app_stop(-1);
        return;
    }
    log_info("block size: {}, write unit: {}, zone size: {}, zone num: "
             "{}, max append size: {},  max open "
             "zone: {}, max active zone: {}\n ",
             spdk_nvme_ns_get_sector_size(ctx->ns),
             spdk_nvme_ns_get_md_size(ctx->ns),
             spdk_nvme_zns_ns_get_zone_size_sectors(ctx->ns), // zone size
             spdk_nvme_zns_ns_get_num_zones(ctx->ns),
             spdk_nvme_zns_ctrlr_get_max_zone_append_size(ctx->ctrlr) /
                 spdk_nvme_ns_get_sector_size(ctx->ns),
             spdk_nvme_zns_ns_get_max_open_zones(ctx->ns),
             spdk_nvme_zns_ns_get_max_active_zones(ctx->ns));

    // working
    int rc = 0;

    uint64_t write_head = 0;
    rc = z_get_zone_head(ctx, ctx->current_zone, &write_head);
    assert(rc == 0);
    log_info("current zone: {}, current lba {}, head {}", ctx->current_zone,
             ctx->current_lba, write_head);
    // FIXME:
    ctx->current_lba = write_head;

    // measurment time points
    chrono_tp stime;
    chrono_tp etime;
    std::vector<u64> deltas;

    log_info("writing with z_append:");
    log_debug("here");
    char **wbuf = (char **)calloc(1, sizeof(char **));
    for (int i = 0; i < append_times; i++) {
        rc = write_zstore_pattern(wbuf, ctx, ctx->info.lba_size,
                                  "test_zstore1:", value + i);
        assert(rc == 0);

        stime = std::chrono::high_resolution_clock::now();

        // APPEND
        rc = z_append(ctx, ctx->zslba, *wbuf, ctx->info.lba_size);
        assert(rc == 0);

        etime = std::chrono::high_resolution_clock::now();
        auto dur = std::chrono::duration_cast<std::chrono::microseconds>(etime -
                                                                         stime);
        deltas.push_back(dur.count());

        // log_info("write {}", *wbuf);
    }
    auto sum = std::accumulate(deltas.begin(), deltas.end(), 0.0);
    auto avg = sum / append_times;
    log_info("Averge append {} us", avg);
    deltas.clear();

    ctx->current_lba = 0x5781dd4;
    log_info("read with z_append:");
    char *rbuf = (char *)z_calloc(ctx, ctx->info.lba_size, sizeof(char *));
    for (int i = 0; i < append_times; i++) {
        stime = std::chrono::high_resolution_clock::now();
        rc = z_read(ctx, ctx->current_lba + i, rbuf, 4096);
        assert(rc == 0);

        etime = std::chrono::high_resolution_clock::now();
        auto dur = std::chrono::duration_cast<std::chrono::microseconds>(etime -
                                                                         stime);
        deltas.push_back(dur.count());

        // log_info("z_read: {}, {}", i, rbuf);
        // printf("%s\n", rbuf);

        // for (int i = 0; i < 30; i++) {
        //     printf("%d-th read %c\n", i, (char *)(rbuf)[i]);
        // }
    }
    sum = std::accumulate(deltas.begin(), deltas.end(), 0.0);
    avg = sum / append_times;
    log_info("Averge read {} us", avg);

    log_info("Test start finish");
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
    opts.name = "test_nvme";
    if ((rc = spdk_app_parse_args(argc, argv, &opts, NULL, NULL, NULL, NULL)) !=
        SPDK_APP_PARSE_ARGS_SUCCESS) {
        exit(rc);
    }

    struct ZstoreContext ctx = {};
    ctx.current_zone = current_zone;
    rc = spdk_app_start(&opts, test_start, &ctx);
    if (rc) {
        SPDK_ERRLOG("ERROR starting application\n");
    }

    log_info("freee dma");
    // spdk_nvme_ctrlr_free_io_qpair(ctx.qpair);
    spdk_dma_free(ctx.write_buff);
    spdk_dma_free(ctx.read_buff);

    spdk_app_fini();

    log_info("zstore exits gracefully");
    return rc;
}
