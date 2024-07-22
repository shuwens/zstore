#include "include/utils.hpp"
#include "include/zns_device.h"
#include "spdk/env.h"
#include "spdk/event.h"
#include "spdk/log.h"
#include "spdk/nvme.h"
#include "spdk/nvme_zns.h"
#include "spdk/nvmf_spec.h"
#include <atomic>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

static void zstore_exit(void *arg)
{
    struct ZstoreContext *ctx = static_cast<struct ZstoreContext *>(arg);
    // Increment the parameter
    // current_zone++;
    // Write the new value back to the file
    std::ofstream outputFile("../current_zone");
    if (outputFile.is_open()) {
        outputFile << ctx->current_zone;
        outputFile.close();
    }
}

static void read_complete(void *arg, const struct spdk_nvme_cpl *completion)
{
    struct ZstoreContext *ctx = static_cast<struct ZstoreContext *>(arg);
    log_info("read_complete: load {}", ctx->count.load());

    ctx->num_completed += 1;
    if (spdk_nvme_cpl_is_error(completion)) {
        SPDK_ERRLOG("nvme io read error: %s\n",
                    spdk_nvme_cpl_get_status_string(&completion->status));
        ctx->num_fail += 1;
        ctx->num_queued -= 1;

        spdk_nvme_ctrlr_free_io_qpair(ctx->qpair);
        zstore_exit(ctx);
        spdk_app_stop(-1);
        return;
    } else {
        ctx->num_success += 1;
        ctx->num_queued -= 1;
    }

    // compare read and write buffer
    int cmp_res = memcmp(ctx->write_buff, ctx->read_buff, ctx->buff_size);
    if (cmp_res != 0) {
        log_error("read and write buffer are not the same!");
        // std::string myString(data, size);
        std::string w_str(ctx->write_buff, ctx->buff_size);
        std::string r_str(ctx->read_buff, ctx->buff_size);
        printf("write buf %s, read buf %s", w_str.c_str(), r_str.c_str());

        spdk_nvme_ctrlr_free_io_qpair(ctx->qpair);
        zstore_exit(ctx);
        spdk_app_stop(-1);
        return;
    } else {
        log_info("read and write buffer are the same. load {}",
                 ctx->count.load());
    }

    ctx->count.fetch_add(1);
    if (ctx->count.load() == 2 * 0x1) {
        log_info("read zone complete. load {}\n", ctx->count.load());
        spdk_nvme_ctrlr_free_io_qpair(ctx->qpair);
        zstore_exit(ctx);
        spdk_app_stop(0);
        return;
    }

    // memset(ctx->read_buff, 0x34, ctx->buff_size);
    // uint64_t lba = ctx->count.load() / 0x100 *
    //                    spdk_nvme_zns_ns_get_zone_size_sectors(ctx->ns) +
    //                ctx->count.load() % 0x100;
    //
    // int rc = spdk_nvme_ns_cmd_read(ctx->ns, ctx->qpair, ctx->read_buff, lba,
    // 1,
    //                                read_complete, ctx, 0);
    // SPDK_NOTICELOG("read lba:0x%lx\n", lba);
    // if (rc != 0) {
    //     SPDK_ERRLOG("%s error while reading from nvme: %d\n",
    //                 spdk_strerror(-rc), rc);
    //     spdk_nvme_ctrlr_free_io_qpair(ctx->qpair);
    //     spdk_app_stop(-1);
    //     return;
    // }
}

static void read_zone(void *arg)
{
    struct ZstoreContext *ctx = static_cast<struct ZstoreContext *>(arg);

    log_info("read_zone");
    // int append_times = 0x100;
    int append_times = 0x1;
    int zone_num = 2;

    ctx->count = 0;

    memset(ctx->read_buff, 0x34, ctx->buff_size);
    for (uint64_t slba = 0; slba < zone_num * ctx->info.zone_size;
         slba += ctx->info.zone_size) {
        for (int i = 0; i < append_times; i++) {
            ctx->num_queued++;

            int rc = spdk_nvme_ns_cmd_read(ctx->ns, ctx->qpair, ctx->read_buff,
                                           ctx->zslba + slba + i, 1,
                                           read_complete, ctx, 0);
            SPDK_NOTICELOG("read lba:0x%x to read buffer\n",
                           ctx->zslba + slba + i);
            if (rc) {
                log_error("{} error while reading from nvme: {} \n",
                          spdk_strerror(-rc), rc);
                spdk_nvme_ctrlr_free_io_qpair(ctx->qpair);
                spdk_app_stop(-1);
                return;
            }
        }
    }
    while (ctx->num_queued) {
        spdk_nvme_qpair_process_completions(ctx->qpair, 0);
    }
}

static void write_zone_complete(void *arg,
                                const struct spdk_nvme_cpl *completion)
{
    struct ZstoreContext *ctx = static_cast<struct ZstoreContext *>(arg);
    log_info("write_zone_complete: load {}", ctx->count.load());
    ctx->num_completed += 1;

    if (spdk_nvme_cpl_is_error(completion)) {
        log_error("nvme io write error: {}\n",
                  spdk_nvme_cpl_get_status_string(&completion->status));
        ctx->num_fail += 1;
        ctx->num_queued -= 1;

        spdk_nvme_ctrlr_free_io_qpair(ctx->qpair);
        spdk_app_stop(-1);
        return;
    } else {
        ctx->num_success += 1;
        ctx->num_queued -= 1;
    }

    SPDK_NOTICELOG("append lba:0x%lx\n", completion->cdw0);

    ctx->count.fetch_sub(1);
    if (ctx->count.load() == 0) {
        log_info("write zone complete. load {}\n", ctx->count.load());
        read_zone(ctx);
    }
}

static void write_zone(void *arg)
{
    log_info("write_zone");
    struct ZstoreContext *ctx = static_cast<struct ZstoreContext *>(arg);

    int append_times = 0x1;
    int zone_num = 2;
    ctx->count = zone_num * append_times;
    // log_info("memset?");
    memset(ctx->write_buff, 0x12, ctx->buff_size);
    for (uint64_t slba = 0; slba < zone_num * ctx->info.zone_size;
         slba += ctx->info.zone_size) {
        for (int i = 0; i < append_times; i++) {
            // log_info("slba {}, i {}", slba, i);
            ctx->num_queued++;
            int rc = spdk_nvme_zns_zone_append(
                ctx->ns, ctx->qpair, ctx->write_buff, ctx->zslba + slba, 1,
                write_zone_complete, ctx, 0);
            if (rc != 0) {
                log_error("{} error while write_zone: {}\n", spdk_strerror(-rc),
                          rc);
                spdk_nvme_ctrlr_free_io_qpair(ctx->qpair);
                spdk_app_stop(-1);
                return;
            }
        }
    }

    while (ctx->num_queued) {
        spdk_nvme_qpair_process_completions(ctx->qpair, 0);
    }
}

static void reset_zone_complete(void *arg, const struct spdk_nvme_cpl *cpl)
{
    struct ZstoreContext *ctx = static_cast<struct ZstoreContext *>(arg);
    log_info("reset_zone_complete: load {}", ctx->count.load());

    ctx->num_completed += 1;
    if (spdk_nvme_cpl_is_error(cpl)) {
        spdk_nvme_qpair_print_completion(ctx->qpair,
                                         (struct spdk_nvme_cpl *)cpl);
        fprintf(stderr, "Reset all zone error - status = %s\n",
                spdk_nvme_cpl_get_status_string(&cpl->status));
        ctx->num_fail += 1;
        ctx->num_queued -= 1;
        log_debug(
            "reset zone complete: queued {} completed {} success {} fail {}",
            ctx->num_queued, ctx->num_completed, ctx->num_success,
            ctx->num_fail);
        SPDK_ERRLOG("nvme io reset error: %s\n",
                    spdk_nvme_cpl_get_status_string(&cpl->status));
        spdk_nvme_ctrlr_free_io_qpair(ctx->qpair);
        spdk_app_stop(-1);
        return;
    } else {
        ctx->num_success += 1;
        ctx->num_queued -= 1;
    }

    log_debug("reset zone complete: queued {} completed {} success {} fail {}, "
              "load {}",
              ctx->num_queued, ctx->num_completed, ctx->num_success,
              ctx->num_fail, ctx->count.load());

    // when all reset is done, do writes
    ctx->count.fetch_sub(1);
    if (ctx->count.load() == 0) {
        log_info("reset zone complete. load {}\n", ctx->count.load());
        write_zone(ctx);
    }
}

static void reset_zone(void *arg)
{
    log_info("reset_zone \n");
    struct ZstoreContext *ctx = static_cast<struct ZstoreContext *>(arg);
    int zone_num = 2;
    ctx->count = zone_num;
    log_debug("Reset zone: current zone {}, num {}, size {}, load {}",
              ctx->current_zone, zone_num, ctx->info.zone_size,
              ctx->count.load());

    int stupid = 0;
    for (uint64_t slba = 0; slba < zone_num * ctx->info.zone_size;
         slba += ctx->info.zone_size) {
        stupid += 1;
        ctx->num_queued++;
        int rc = spdk_nvme_zns_reset_zone(
            ctx->ns, ctx->qpair, ctx->zslba + slba + ctx->info.zone_size, 0,
            reset_zone_complete, ctx);
        if (rc == -ENOMEM) {
            log_debug("Queueing io: {}, {}", rc, spdk_strerror(-rc));
        } else if (rc) {
            log_error("{} error while resetting zone: {}\n", spdk_strerror(-rc),
                      rc);
            spdk_nvme_ctrlr_free_io_qpair(ctx->qpair);
            spdk_app_stop(-1);
            return;
        }

        log_debug("Reset zone: slba {}: load {}", ctx->zslba + slba,
                  ctx->count.load());
    }

    while (ctx->num_queued) {
        spdk_nvme_qpair_process_completions(ctx->qpair, 0);
    }

    log_info("reset_zone end, load {}, stupid {}", ctx->count.load(), stupid);
}

static void test_start(void *arg1)
{
    log_info("test start\n");
    struct ZstoreContext *ctx = static_cast<struct ZstoreContext *>(arg1);

    zns_dev_init(ctx);

    zstore_init(ctx);

    z_get_device_info(ctx);

    // zone cap * lba_bytes ()
    log_info("zone cap: {}, lba bytes {}", ctx->info.zone_cap,
             ctx->info.lba_size);
    // ctx->buff_size = ctx->info.zone_cap * ctx->info.lba_size;
    ctx->buff_size = ctx->info.lba_size;
    uint32_t buf_align = ctx->info.lba_size;
    log_info("buffer size: {}, align {}", ctx->buff_size, buf_align);

    ctx->write_buff =
        static_cast<char *>(spdk_dma_zmalloc(ctx->buff_size, buf_align, NULL));
    if (!ctx->write_buff) {
        SPDK_ERRLOG("Failed to allocate buffer\n");
        spdk_nvme_ctrlr_free_io_qpair(ctx->qpair);
        spdk_nvme_detach(ctx->ctrlr);
        spdk_app_stop(-1);
        return;
    }
    ctx->read_buff =
        static_cast<char *>(spdk_dma_zmalloc(ctx->buff_size, buf_align, NULL));
    if (!ctx->read_buff) {
        SPDK_ERRLOG("Failed to allocate buffer\n");
        spdk_nvme_ctrlr_free_io_qpair(ctx->qpair);
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
    reset_zone(ctx);

    // write_zone(ctx);

    log_info("Test start finish");
    return;
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

    spdk_dma_free(ctx.write_buff);
    spdk_dma_free(ctx.read_buff);

    spdk_app_fini();

    log_info("zstore exits gracefully");
    return rc;
}
