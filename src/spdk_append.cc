#include "include/utils.hpp"
#include "include/zns_device.h"
#include "spdk/env.h"
#include "spdk/event.h"
#include "spdk/log.h"
#include "spdk/nvme.h"
#include "spdk/nvme_zns.h"
#include <cstdint>
#include <fstream>
#include <stdio.h>
// #include "spdk/nvmf_spec.h"
#include <atomic>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

const int zone_num = 1;
const int append_times = 100;
const int value = 10000000; // Integer value to set in the buffer

void memset64(void *dest, u64 val, usize bytes)
{
    assert(bytes % 8 == 0);
    u64 *cdest = (u64 *)dest;
    for (usize i = 0; i < bytes / 8; i++)
        cdest[i] = val;
}

int z_read(void *arg, uint64_t slba, void *buffer, uint64_t size)
{
    struct ZstoreContext *ctx = static_cast<struct ZstoreContext *>(arg);
    ERROR_ON_NULL(ctx->qpair, 1);
    ERROR_ON_NULL(buffer, 1);
    int rc = 0;

    int lbas = (size + ctx->info.lba_size - 1) / ctx->info.lba_size;
    int lbas_processed = 0;
    int step_size = (ctx->info.mdts / ctx->info.lba_size);
    int current_step_size = step_size;
    int slba_start = slba;

    while (lbas_processed < lbas) {
        Completion completion = {.done = false, .err = 0};
        if ((slba + lbas_processed + step_size) / ctx->info.zone_size >
            (slba + lbas_processed) / ctx->info.zone_size) {
            current_step_size =
                ((slba + lbas_processed + step_size) / ctx->info.zone_size) *
                    ctx->info.zone_size -
                lbas_processed - slba;
        } else {
            current_step_size = step_size;
        }
        current_step_size = lbas - lbas_processed > current_step_size
                                ? current_step_size
                                : lbas - lbas_processed;
        // printf("%d step %d  \n", slba_start, current_step_size);
        rc = spdk_nvme_ns_cmd_read(ctx->ns, ctx->qpair,
                                   (char *)buffer +
                                       lbas_processed * ctx->info.lba_size,
                                   slba_start,        /* LBA start */
                                   current_step_size, /* number of LBAs */
                                   __read_complete2, &completion, 0);
        if (rc != 0) {
            return 1;
        }
        POLL_QPAIR(ctx->qpair, completion.done);
        if (completion.err != 0) {
            return completion.err;
        }
        lbas_processed += current_step_size;
        slba_start = slba + lbas_processed;
    }
    return rc;
}

int z_append(void *arg, uint64_t slba, void *buffer, uint64_t size)
{
    struct ZstoreContext *ctx = static_cast<struct ZstoreContext *>(arg);
    ERROR_ON_NULL(ctx->qpair, 1);
    ERROR_ON_NULL(buffer, 1);

    int rc = 0;

    int lbas = (size + ctx->info.lba_size - 1) / ctx->info.lba_size;
    int lbas_processed = 0;
    int step_size = (ctx->info.zasl / ctx->info.lba_size);
    int current_step_size = step_size;
    int slba_start = (slba / ctx->info.zone_size) * ctx->info.zone_size;

    while (lbas_processed < lbas) {
        Completion completion = {.done = false, .err = 0};
        if ((slba + lbas_processed + step_size) / ctx->info.zone_size >
            (slba + lbas_processed) / ctx->info.zone_size) {
            current_step_size =
                ((slba + lbas_processed + step_size) / ctx->info.zone_size) *
                    ctx->info.zone_size -
                lbas_processed - slba;
        } else {
            current_step_size = step_size;
        }
        current_step_size = lbas - lbas_processed > current_step_size
                                ? current_step_size
                                : lbas - lbas_processed;
        rc = spdk_nvme_zns_zone_append(ctx->ns, ctx->qpair,
                                       (char *)buffer +
                                           lbas_processed * ctx->info.lba_size,
                                       slba_start,        /* LBA start */
                                       current_step_size, /* number of LBAs */
                                       __append_complete2, &completion, 0);
        if (rc != 0) {
            break;
        }
        POLL_QPAIR(ctx->qpair, completion.done);
        if (completion.err != 0) {
            return completion.err;
        }
        lbas_processed += current_step_size;
        slba_start = ((slba + lbas_processed) / ctx->info.zone_size) *
                     ctx->info.zone_size;
    }
    return rc;
}

static void zstore_exit(void *arg)
{
    struct ZstoreContext *ctx = static_cast<struct ZstoreContext *>(arg);
    // Increment the parameter
    // ctx->current_zone++;
    // Write the new value back to the file
    std::ofstream outputFile("../current_zone");
    if (outputFile.is_open()) {
        outputFile << ctx->current_zone;
        outputFile.close();
    }
}

static void close_complete(void *arg, const struct spdk_nvme_cpl *completion)
{
    struct ZstoreContext *ctx = static_cast<struct ZstoreContext *>(arg);
    log_info("close_complete: load {}", ctx->count.load());

    ctx->num_completed += 1;
    if (spdk_nvme_cpl_is_error(completion)) {
        SPDK_ERRLOG("nvme close zone error: %s\n",
                    spdk_nvme_cpl_get_status_string(&completion->status));
        ctx->num_fail += 1;
        ctx->num_queued -= 1;

        // spdk_nvme_ctrlr_free_io_qpair(ctx->qpair);
        zstore_exit(ctx);
        spdk_app_stop(-1);
        return;
    } else {
        ctx->num_success += 1;
        ctx->num_queued -= 1;
    }

    // TODO: same as reset
    ctx->count.fetch_sub(1);
    if (ctx->count.load() == 0) {
        log_info("close zone complete. load {}, queued {}\n", ctx->count.load(),
                 ctx->num_queued);
        zstore_exit(ctx);
        ctx->zstore_open = false;
        spdk_app_stop(0);
        log_info("app stop {}\n", ctx->num_queued);
        return;
    }
}

static void close_zone(void *arg)
{
    struct ZstoreContext *ctx = static_cast<struct ZstoreContext *>(arg);

    log_info("close_zone");

    ctx->count = zone_num;

    for (uint64_t slba = 0; slba < zone_num * ctx->info.zone_size;
         slba += ctx->info.zone_size) {
        ctx->num_queued++;
        int rc = spdk_nvme_zns_finish_zone(
            ctx->ns, ctx->qpair, ctx->zslba + slba + ctx->info.zone_size, 0,
            close_complete, ctx);
        if (rc == -ENOMEM) {
            log_debug("Queueing io: {}, {}", rc, spdk_strerror(-rc));
        } else if (rc) {
            log_error("{} error while closing zone: {}\n", spdk_strerror(-rc),
                      rc);
            // spdk_nvme_ctrlr_free_io_qpair(ctx->qpair);
            spdk_app_stop(-1);
            return;
        }

        log_debug("Close zone: slba {}: load {}", ctx->zslba + slba,
                  ctx->count.load());
    }

    while (ctx->num_queued && ctx->zstore_open) {
        spdk_nvme_qpair_process_completions(ctx->qpair, 0);
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

        // spdk_nvme_ctrlr_free_io_qpair(ctx->qpair);
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
        // u64 dw = *(u64 *)ctx->write_buff;
        // u64 dr = *(u64 *)ctx->read_buff;
        // printf("write: %d\n", dw);
        // printf("read: %d\n", dr);
    } else {
        log_info("read and write buffer are the same. load {}",
                 ctx->count.load());
        // u64 dw = *(u64 *)ctx->write_buff;
        // u64 dr = *(u64 *)ctx->read_buff;
        // printf("write: %d\n", dw);
        // printf("read: %d\n", dr);
    }

    ctx->count.fetch_add(1);
    if (ctx->count.load() == zone_num * append_times) {
        log_info("read zone complete. load {}\n", ctx->count.load());

        return;
        // zstore_exit(ctx);
        // spdk_app_stop(0);
        // log_info("app stop \n");
        // return;
        // close_zone(ctx);
    }
}

static void read_zone(void *arg)
{
    struct ZstoreContext *ctx = static_cast<struct ZstoreContext *>(arg);

    log_info("read_zone");
    ctx->count = 0;
    memset(ctx->read_buff, 0x34, ctx->buff_size);
    // std::cout << "Read buffer" << std::string(ctx->read_buff, 4096)
    //           << std::endl;

    int cmp_res = memcmp(ctx->write_buff, ctx->read_buff, ctx->buff_size);
    if (cmp_res != 0) {
        log_error("EXPECTED: read and write buffer are not the same!");
    }

    for (uint64_t slba = 0; slba < zone_num * ctx->info.zone_size;
         slba += ctx->info.zone_size) {
        for (int i = 0; i < append_times; i++) {
            ctx->num_queued++;
            // TODO: fix it
            // int rc = spdk_nvme_ns_cmd_read(ctx->ns, ctx->qpair,
            // ctx->read_buff,
            //                                ctx->zslba + slba + i, 1,
            //                                read_complete, ctx, 0);

            int rc = spdk_nvme_ns_cmd_read(
                ctx->ns, ctx->qpair, (ctx->read_buff + i * 4096),
                ctx->current_lba + i, 1, read_complete, ctx, 0);
            SPDK_NOTICELOG("read lba:0x%x to read buffer\n",
                           ctx->current_lba + i);
            if (rc) {
                log_error("{} error while reading from nvme: {} \n",
                          spdk_strerror(-rc), rc);
                // spdk_nvme_ctrlr_free_io_qpair(ctx->qpair);
                spdk_app_stop(-1);
                return;
            }
        }
    }
    while (ctx->num_queued && ctx->zstore_open) {
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

        // spdk_nvme_ctrlr_free_io_qpair(ctx->qpair);
        spdk_app_stop(-1);
        return;
    } else {
        ctx->num_success += 1;
        ctx->num_queued -= 1;
    }

    // log_info("append slba:0x%016x\n", completion->cdw0);
    SPDK_NOTICELOG("append slba:0x%016x\n", completion->cdw0);

    ctx->append_lbas.push_back(completion->cdw0);

    if (ctx->current_lba == 0) {
        ctx->current_lba = completion->cdw0;
    }

    ctx->count.fetch_sub(1);
    if (ctx->count.load() == 0) {
        log_info("write zone complete. load {}\n", ctx->count.load());
        return;
        // read_zone(ctx);
    }
}

static void write_zone(void *arg)
{
    log_info("write_zone");
    struct ZstoreContext *ctx = static_cast<struct ZstoreContext *>(arg);
    ctx->count = zone_num * append_times;

    // std::cout << "Write buffer" << std::string(ctx->write_buff, 4096)
    //           << std::endl;
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
                // spdk_nvme_ctrlr_free_io_qpair(ctx->qpair);
                spdk_app_stop(-1);
                return;
            }
            while (ctx->num_queued && ctx->zstore_open) {
                spdk_nvme_qpair_process_completions(ctx->qpair, 0);
            }
        }
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
        log_debug("reset zone complete: queued {} completed {} success {} "
                  "fail {}",
                  ctx->num_queued, ctx->num_completed, ctx->num_success,
                  ctx->num_fail);
        SPDK_ERRLOG("nvme io reset error: %s\n",
                    spdk_nvme_cpl_get_status_string(&cpl->status));
        // spdk_nvme_ctrlr_free_io_qpair(ctx->qpair);
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
        // write_zone(ctx);
    }
}

static void reset_zone(void *arg)
{
    log_info("reset_zone \n");
    struct ZstoreContext *ctx = static_cast<struct ZstoreContext *>(arg);
    ctx->count = zone_num;
    log_debug("Reset zone: current zone {}, num {}, size {}, load {}",
              ctx->current_zone, zone_num, ctx->info.zone_size,
              ctx->count.load());

    for (uint64_t slba = 0; slba < zone_num * ctx->info.zone_size;
         slba += ctx->info.zone_size) {
        ctx->num_queued++;
        int rc = spdk_nvme_zns_reset_zone(
            ctx->ns, ctx->qpair, ctx->zslba + slba + ctx->info.zone_size, 0,
            reset_zone_complete, ctx);
        SPDK_NOTICELOG("reset zone with lba:0x%x\n",
                       ctx->zslba + slba + ctx->info.zone_size);
        if (rc == -ENOMEM) {
            log_debug("Queueing io: {}, {}", rc, spdk_strerror(-rc));
        } else if (rc) {
            log_error("{} error while resetting zone: {}\n", spdk_strerror(-rc),
                      rc);
            // spdk_nvme_ctrlr_free_io_qpair(ctx->qpair);
            spdk_app_stop(-1);
            return;
        }

        log_debug("Reset zone: slba {}: load {}, queued {}", ctx->zslba + slba,
                  ctx->count.load(), ctx->num_queued);
    }

    log_debug("1: queued: {}", ctx->num_queued);
    while (ctx->num_queued && ctx->zstore_open) {
        spdk_nvme_qpair_process_completions(ctx->qpair, 0);
    }

    log_info("reset_zone end, load {}, ", ctx->count.load());
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
        snprintf(wbuf, 4096, "zstore:%d", value + i);

        // printf("write: %d\n", value + i);
        int rc = z_append(ctx, ctx->zslba, wbuf, 4096);
    }

    log_info("append lbs for loop");
    for (auto &i : ctx->append_lbas) {
        log_info("append lbs: {}", i);
    }

    ctx->current_lba = 0x5780342;

    // char **rbuf = new char *[append_times];
    auto rbuf1 = new char *[4096 * append_times];
    std::vector<u64> data1;

    log_info("read with z_append:");
    for (int i = 0; i < append_times; i++) {
        rbuf1[i] = (char *)z_calloc(ctx, 4096, sizeof(char));

        int rc = z_read(ctx, ctx->current_lba + i * 4096, rbuf1[i], 4096);

        // fprintf(stderr, "read [%lx] [%lx] [%lx]", *((uint64_t *)(buffer)),
        //         *((uint64_t *)(buffer + 512)), *((uint64_t *)(buffer +
        //         1024)));

        // u64 data;
        // data = *(u64 *)rbuf;
        // log_info("{}", data);

        // std::string myString = std::string((char *)valpt);
        // log_info("fuck{}", myString);
        //
        // std::string string1;
        // string1 = valpt;
        // log_info("fuck{}", string1);
        //
        // std::string s1;
        // s1.assign(valpt, valpt + 4096);
        // log_info("fuck{}", s1);
    }

    for (int i = 0; i < append_times; i++) {
        data1.push_back(*(u64 *)rbuf1[i]);
    }
    delete[] rbuf1;
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
