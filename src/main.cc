#include "include/utils.hpp"
#include "include/zns_device.h"
#include "spdk/env.h"
#include "spdk/event.h"
#include "spdk/log.h"
#include "spdk/nvme.h"
#include "spdk/nvme_zns.h"
#include <cstdint>
#include <fmt/core.h>
#include <fstream>
#include <stdio.h>
// #include "spdk/nvmf_spec.h"
#include <cstdlib>
#include <fstream>
#include <iostream>

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

    struct spdk_nvme_io_qpair_opts qpair_opts = {};
    zns_dev_init(ctx);
    zstore_qpair_setup(ctx, qpair_opts);

    zstore_init(ctx);

    z_get_device_info(ctx);

    ctx->zstore_open = true;

    // zone cap * lba_bytes ()
    log_info("zone cap: {}, lba bytes {}", ctx->info.zone_cap,
             ctx->info.lba_size);
    // ctx->buff_size = ctx->info.zone_cap * ctx->info.lba_size;
    ctx->buff_size = ctx->info.lba_size * append_times;
    // ctx->buff_size = 4096;
    uint32_t buf_align = ctx->info.lba_size;
    log_info("buffer size: {}, align {}", ctx->buff_size, buf_align);

    // static_cast<char *>(spdk_zmalloc(ctx->buff_size, buf_align, NULL));
    ctx->write_buff = (char *)spdk_zmalloc(
        ctx->buff_size, 0, NULL, SPDK_ENV_SOCKET_ID_ANY, SPDK_MALLOC_DMA);
    if (!ctx->write_buff) {
        SPDK_ERRLOG("Failed to allocate buffer\n");
        // spdk_nvme_ctrlr_free_io_qpair(ctx->qpair);
        spdk_nvme_detach(ctx->ctrlr);
        spdk_app_stop(-1);
        return;
    }
    ctx->read_buff = (char *)spdk_zmalloc(
        ctx->buff_size, 0, NULL, SPDK_ENV_SOCKET_ID_ANY, SPDK_MALLOC_DMA);
    if (!ctx->read_buff) {
        SPDK_ERRLOG("Failed to allocate buffer\n");
        // spdk_nvme_ctrlr_free_io_qpair(ctx->qpair);
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

    // memset(ctx->write_buff, 0, ctx->buff_size);
    // memset(ctx->read_buff, 0, ctx->buff_size);
    // for (int i = 0; i < append_times; i++) {
    //     log_info("memset buffer in before write:");
    //     // std::memcpy(ctx->write_buff + 4096 * i, &value + i, 4096);
    //     memset64((char *)ctx->write_buff + 4096 * i, i + value, 4096);
    //     // memset64((char *)ctx->write_buff + 4096 * i, i + value, 4096);
    //
    //     u64 dw = *(u64 *)(ctx->write_buff + 4096 * i);
    //     u64 dr = *(u64 *)(ctx->read_buff + 4096 * i);
    //     printf("write: %d\n", dw);
    //     printf("read: %d\n", dr);
    // }

    // working
    int rc = 0;
    log_info("writing with z_append:");
    log_debug("here");
    for (int i = 0; i < append_times; i++) {
        log_debug("1");
        char **wbuf = (char **)calloc(1, sizeof(char **));
        rc = write_zstore_pattern(wbuf, ctx, ctx->info.lba_size,
                                  "test_zstore1:", value + i);
        assert(rc == 0);
        // snprintf(*wbuf, 4096, "zstore1:%d", value + i);
        log_debug("2");

        // printf("write: %d\n", value + i);
        rc = z_append(ctx, ctx->zslba, *wbuf, ctx->info.lba_size);
        assert(rc == 0);

        for (int i = 0; i < 30; i++) {
            // log_debug("{}", i);
            // assert((char *)(pattern_read_zstore)[i] ==
            //        (char *)(*pattern_zstore)[i]);
            printf("%d-th write %c\n", i, (char *)(*wbuf)[i]);
        }
    }

    // log_info("append lbs for loop");
    // for (auto &i : ctx->append_lbas) {
    //     log_info("append lbs: {}", i);
    // }

    ctx->current_lba = 0x5780267;
    log_info("read with z_append:");
    for (int i = 0; i < append_times; i++) {
        log_info("z_append: {}", i);
        char *rbuf = (char *)z_calloc(ctx, ctx->info.lba_size, sizeof(char *));

        rc = z_read(ctx, ctx->current_lba + i, rbuf, 4096);
        assert(rc == 0);

        // for (int i = 0; i < ctx->info.lba_size; i++) {
        for (int i = 0; i < 30; i++) {
            // log_debug("{}", i);
            // assert((char *)(pattern_read_zstore)[i] ==
            //        (char *)(*pattern_zstore)[i]);
            printf("%d-th read %c\n", i, (char *)(rbuf)[i]);
        }
    }

    // read_zone(ctx);

    // close_zone(ctx);

    // for (const uint32_t &i : ctx.append_lbas)
    //     std::cout << "append lbs: " << i << std::endl;

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
