#include "include/utils.hpp"
#include "include/zns_device.h"
#include "spdk/env.h"
#include "spdk/event.h"
#include "spdk/log.h"
#include "spdk/nvme.h"
#include "spdk/nvme_zns.h"
#include <chrono>
#include <cstdint>
#include <fmt/core.h>
#include <fstream>
#include <stdio.h>
// #include "spdk/nvmf_spec.h"
#include <atomic>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

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
    // reset_zone(ctx);

    // write_zone(ctx);
    log_info("writing with z_append:");
    for (int i = 0; i < append_times; i++) {
        char *wbuf = (char *)z_calloc(ctx, 4096, sizeof(char));

        // const std::string data = "zstore:test:42";
        // memcpy(valpt, data.c_str(), data.size());
        snprintf(wbuf, 4096, "zstore1:%d", value + i);

        // printf("write: %d\n", value + i);
        int rc = z_append(ctx, ctx->zslba, wbuf, 4096);
    }

    log_info("append lbs for loop");
    for (auto &i : ctx->append_lbas) {
        log_info("append lbs: {}", i);
    }

    // ctx->current_lba = 0x5780342;
    ctx->current_lba = 0x57a14c0;

    // char **rbuf = new char *[append_times];
    // auto rbuf = new char *[4096 * append_times];
    std::vector<u64> data1;
    int rc = 0;
    log_info("read with z_append:");
    for (int i = 0; i < append_times; i++) {
        log_info("z_append: {}", i);
        char *rbuf = (char *)z_calloc(ctx, 4096, sizeof(char *));

        rc = z_read(ctx, ctx->current_lba + i * 4096, rbuf, 4096);
        assert(rc == 0);
        // printf("%s\n", rbuf);
        // fprintf("%s\n", rbuf);
        // fprintf(stderr, "read [%lx] [%lx] [%lx]", *((uint64_t *)(rbuf1[i])),
        //         *((uint64_t *)(rbuf1[i] + 512)),
        //         *((uint64_t *)(rbuf1[i] + 1024)));

        for (int j = 0; j < 4096; j++) {
            fprintf(stdout, "%c", rbuf[j]);
        }

        // log_info("log rbuf {}", rbuf);
        // u64 data;
        // data = *(u64 *)(rbuf + i * 4096);
        // log_info("log data {}", data);
        //
        // char cdata;
        // cdata = *(char *)(rbuf + i * 4096);
        // log_info("log cdata {}", cdata);

        // std::string myString = std::string((char *)rbuf + i * 4096);
        // log_info("test1 {}", myString);
    }

    // for (int i = 0; i < append_times; i++) {
    //     data1.push_back(*(u64 *)rbuf + i * 4096);
    //     printf("%s", rbuf + i * 4096);
    // }
    //
    // delete[] rbuf;
    std::ofstream of1("data1.txt");
    for (auto d : data1)
        of1 << d << " ";

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
