#pragma once
#include "spdk/bdev.h"
#include "spdk/bdev_zone.h"
#include "spdk/env.h"
#include "spdk/event.h"
#include "spdk/log.h"
#include "spdk/stdinc.h"
#include "spdk/string.h"
#include "spdk/thread.h"
#include "utils.hpp"
#include "zstore.h"
#include <atomic>
// #include <spdk/env.h>
// #include <spdk/nvme.h>
// #include <spdk/nvme_tcp.h>
// #include <spdk/nvme_zns.h>

static const char *g_bdev_name = "Nvme1n2";

static void write_object_complete(struct spdk_bdev_io *bdev_io, bool success,
                                  void *cb_arg)
{
    struct zstore_context_t *ctx =
        static_cast<struct zstore_context_t *>(cb_arg);
    log_info("append lba:0x%lx\n", spdk_bdev_io_get_append_location(bdev_io));
    spdk_bdev_free_io(bdev_io);

    if (!success) {
        log_error("bdev io write zone error: %d\n", EIO);
        spdk_put_io_channel(ctx->bdev_io_channel);
        spdk_bdev_close(ctx->bdev_desc);
        spdk_app_stop(-1);
        return;
    }
    ctx->count.fetch_sub(1);
    if (ctx->count.load() == 0) {
        log_info("write objects complete.\n");
        log_info("Read objects.\n");
        // TODO: read object
        // read_zone(ctx);
    }
}

static void write_object(void *arg)
{
    int rc = 0;
    struct zstore_context_t *ctx = static_cast<struct zstore_context_t *>(arg);
    uint64_t zone_size = spdk_bdev_get_zone_size(ctx->bdev);

    int zone_num = 4;
    int append_times = 0x100;
    ctx->count = zone_num * append_times;
    memset(ctx->write_buff, 0x12, ctx->buff_size);

    for (uint64_t slba = 0; slba < zone_num * zone_size; slba += zone_size) {
        for (int i = 0; i < append_times; i++) {
            rc = spdk_bdev_zone_append(ctx->bdev_desc, ctx->bdev_io_channel,
                                       ctx->write_buff, slba, 1,
                                       write_object_complete, ctx);
            if (rc != 0) {
                log_error("%s error while write_zone: %d\n", spdk_strerror(-rc),
                          rc);
                spdk_put_io_channel(ctx->bdev_io_channel);
                spdk_bdev_close(ctx->bdev_desc);
                spdk_app_stop(-1);
            }
        }
    }
}

// static void create_dummy_objects(Zstore zstore)
static void create_dummy_objects(void *arg)
{
    log_info("Create dummy objects in table: foo, bar, test");

    // zstore.putObject("foo", "foo_data");
    // zstore.putObject("bar", "bar_data");
    // zstore.putObject("baz", "baz_data");
    //
    // std::string baz = zstore.getObject("baz");
    // assert(baz == "baz_data");
    // zstore.deleteObject("baz");
}

static void read_zone_complete(struct spdk_bdev_io *bdev_io, bool success,
                               void *cb_arg)
{
    struct zstore_context_t *ctx =
        static_cast<struct zstore_context_t *>(cb_arg);
    spdk_bdev_free_io(bdev_io);

    if (!success) {
        log_error("bdev io read zone error: %d\n", EIO);
        spdk_put_io_channel(ctx->bdev_io_channel);
        spdk_bdev_close(ctx->bdev_desc);
        spdk_app_stop(-1);
        return;
    }

    int cmp_res = memcmp(ctx->write_buff, ctx->read_buff, ctx->buff_size);
    if (cmp_res != 0) {
        log_error("read zone data error.\n");
        spdk_put_io_channel(ctx->bdev_io_channel);
        spdk_bdev_close(ctx->bdev_desc);
        spdk_app_stop(-1);
        return;
    }
    ctx->count.fetch_add(1);
    if (ctx->count.load() == 4 * 0x100) {
        log_info("read zone complete.\n");
        spdk_put_io_channel(ctx->bdev_io_channel);
        spdk_bdev_close(ctx->bdev_desc);
        spdk_app_stop(0);
        return;
    }

    memset(ctx->read_buff, 0x34, ctx->buff_size);

    uint64_t lba =
        ctx->count.load() / 0x100 * spdk_bdev_get_zone_size(ctx->bdev) +
        ctx->count.load() % 0x100; // lba calculation

    int rc =
        spdk_bdev_read_blocks(ctx->bdev_desc, ctx->bdev_io_channel,
                              ctx->read_buff, lba, 1, read_zone_complete, ctx);
    log_info("read lba:0x%lx\n", lba);
    if (rc != 0) {
        log_error("%s error while reading from bdev: %d\n", spdk_strerror(-rc),
                  rc);
        spdk_put_io_channel(ctx->bdev_io_channel);
        spdk_bdev_close(ctx->bdev_desc);
        spdk_app_stop(-1);
        return;
    }
}

static void read_zone(void *arg)
{
    int rc = 0;
    struct zstore_context_t *ctx = static_cast<struct zstore_context_t *>(arg);
    ctx->count = 0;
    memset(ctx->read_buff, 0x34, ctx->buff_size);
    rc = spdk_bdev_read_blocks(ctx->bdev_desc, ctx->bdev_io_channel,
                               ctx->read_buff, 0, 1, read_zone_complete, ctx);
    log_info("read lba:0x%x\n", 0x0);
    if (rc == -ENOMEM) {
        log_info("Queueing io\n");
        /* In case we cannot perform I/O now, queue I/O */
        ctx->bdev_io_wait.bdev = ctx->bdev;
        ctx->bdev_io_wait.cb_fn = read_zone;
        ctx->bdev_io_wait.cb_arg = ctx;
        spdk_bdev_queue_io_wait(ctx->bdev, ctx->bdev_io_channel,
                                &ctx->bdev_io_wait);
    } else if (rc) {
        log_error("%s error while reading from bdev: %d\n", spdk_strerror(-rc),
                  rc);
        spdk_put_io_channel(ctx->bdev_io_channel);
        spdk_bdev_close(ctx->bdev_desc);
        spdk_app_stop(-1);
    }
}

static void write_zone_complete(struct spdk_bdev_io *bdev_io, bool success,
                                void *cb_arg)
{
    struct zstore_context_t *ctx =
        static_cast<struct zstore_context_t *>(cb_arg);
    log_info("append lba:0x%lx\n", spdk_bdev_io_get_append_location(bdev_io));
    spdk_bdev_free_io(bdev_io);

    if (!success) {
        log_error("bdev io write zone error: %d\n", EIO);
        spdk_put_io_channel(ctx->bdev_io_channel);
        spdk_bdev_close(ctx->bdev_desc);
        spdk_app_stop(-1);
        return;
    }
    ctx->count.fetch_sub(1);
    if (ctx->count.load() == 0) {
        log_info("write zone complete.\n");
        read_zone(ctx);
    }
}

static void write_zone(void *arg)
{
    int rc = 0;
    struct zstore_context_t *ctx = static_cast<struct zstore_context_t *>(arg);
    uint64_t zone_size = spdk_bdev_get_zone_size(ctx->bdev);
    int zone_num = 4;
    int append_times = 0x100;
    ctx->count = zone_num * append_times;
    memset(ctx->write_buff, 0x12, ctx->buff_size);
    for (uint64_t slba = 0; slba < zone_num * zone_size; slba += zone_size) {
        for (int i = 0; i < append_times; i++) {
            rc = spdk_bdev_zone_append(ctx->bdev_desc, ctx->bdev_io_channel,
                                       ctx->write_buff, slba, 1,
                                       write_zone_complete, ctx);
            if (rc != 0) {
                log_error("%s error while write_zone: %d\n", spdk_strerror(-rc),
                          rc);
                spdk_put_io_channel(ctx->bdev_io_channel);
                spdk_bdev_close(ctx->bdev_desc);
                spdk_app_stop(-1);
            }
        }
    }
}

static void reset_zone_complete(struct spdk_bdev_io *bdev_io, bool success,
                                void *cb_arg)
{
    struct zstore_context_t *ctx =
        static_cast<struct zstore_context_t *>(cb_arg);
    spdk_bdev_free_io(bdev_io);

    if (!success) {
        log_error("bdev io reset zone error: %d\n", EIO);
        spdk_put_io_channel(ctx->bdev_io_channel);
        spdk_bdev_close(ctx->bdev_desc);
        spdk_app_stop(-1);
        return;
    }
    ctx->count.fetch_sub(1);
    if (ctx->count.load() == 0) {
        log_info("reset zone complete.\n");

        // TODO: start civet web http server and wait for http requests
        //
        // write_zone(ctx);
        create_dummy_objects(ctx);
    }
}

static void reset_zone(void *arg)
{
    struct zstore_context_t *ctx = static_cast<struct zstore_context_t *>(arg);
    int rc = 0;
    int zone_num = 10;
    ctx->count = zone_num;
    uint64_t zone_size = spdk_bdev_get_zone_size(ctx->bdev);
    for (uint64_t slba = 0; slba < zone_num * zone_size; slba += zone_size) {
        rc = spdk_bdev_zone_management(ctx->bdev_desc, ctx->bdev_io_channel,
                                       slba, SPDK_BDEV_ZONE_RESET,
                                       reset_zone_complete, ctx);
        if (rc == -ENOMEM) {
            log_info("Queueing io\n");
            /* In case we cannot perform I/O now, queue I/O */
            ctx->bdev_io_wait.bdev = ctx->bdev;
            ctx->bdev_io_wait.cb_fn = reset_zone;
            ctx->bdev_io_wait.cb_arg = ctx;
            spdk_bdev_queue_io_wait(ctx->bdev, ctx->bdev_io_channel,
                                    &ctx->bdev_io_wait);
        } else if (rc) {
            log_error("%s error while resetting zone: %d\n", spdk_strerror(-rc),
                      rc);
            spdk_put_io_channel(ctx->bdev_io_channel);
            spdk_bdev_close(ctx->bdev_desc);
            spdk_app_stop(-1);
        }
    }
}

static void test_bdev_event_cb(enum spdk_bdev_event_type type,
                               struct spdk_bdev *bdev, void *event_ctx)
{
    log_info("Unsupported bdev event: type %d\n", type);
}

static void start_zstore(void *arg1)
{
    struct zstore_context_t *ctx = static_cast<struct zstore_context_t *>(arg1);
    uint32_t buf_align;
    int rc = 0;
    ctx->bdev = NULL;
    ctx->bdev_desc = NULL;

    log_info("Successfully started the application\n");
    log_info("Opening the bdev %s\n", ctx->bdev_name);
    rc = spdk_bdev_open_ext(ctx->bdev_name, true, test_bdev_event_cb, NULL,
                            &ctx->bdev_desc);
    if (rc) {
        log_error("Could not open bdev: %s\n", ctx->bdev_name);
        spdk_app_stop(-1);
        return;
    }
    ctx->bdev = spdk_bdev_desc_get_bdev(ctx->bdev_desc);

    log_info("Opening io channel\n");
    /* Open I/O channel */
    ctx->bdev_io_channel = spdk_bdev_get_io_channel(ctx->bdev_desc);
    if (ctx->bdev_io_channel == NULL) {
        log_error("Could not create bdev I/O channel!!\n");
        spdk_bdev_close(ctx->bdev_desc);
        spdk_app_stop(-1);
        return;
    }
    ctx->buff_size = spdk_bdev_get_block_size(ctx->bdev) *
                     spdk_bdev_get_write_unit_size(ctx->bdev);
    buf_align = spdk_bdev_get_buf_align(ctx->bdev);
    ctx->write_buff =
        static_cast<char *>(spdk_dma_zmalloc(ctx->buff_size, buf_align, NULL));
    if (!ctx->write_buff) {
        log_error("Failed to allocate buffer\n");
        spdk_put_io_channel(ctx->bdev_io_channel);
        spdk_bdev_close(ctx->bdev_desc);
        spdk_app_stop(-1);
        return;
    }
    ctx->read_buff =
        static_cast<char *>(spdk_dma_zmalloc(ctx->buff_size, buf_align, NULL));
    if (!ctx->read_buff) {
        log_error("Failed to allocate buffer\n");
        spdk_put_io_channel(ctx->bdev_io_channel);
        spdk_bdev_close(ctx->bdev_desc);
        spdk_app_stop(-1);
        return;
    }

    if (!spdk_bdev_is_zoned(ctx->bdev)) {
        log_error("not a ZNS SSD\n");
        spdk_put_io_channel(ctx->bdev_io_channel);
        spdk_bdev_close(ctx->bdev_desc);
        spdk_app_stop(-1);
        return;
    }
    log_info("block size:%d write unit:%d zone size:%lx zone num:%ld max "
             "append size:%d max open zone:%d max active "
             "zone:%d\n",
             spdk_bdev_get_block_size(ctx->bdev),
             spdk_bdev_get_write_unit_size(ctx->bdev),
             spdk_bdev_get_zone_size(ctx->bdev),
             spdk_bdev_get_num_zones(ctx->bdev),
             spdk_bdev_get_max_zone_append_size(ctx->bdev),
             spdk_bdev_get_max_open_zones(ctx->bdev),
             spdk_bdev_get_max_active_zones(ctx->bdev));

    reset_zone(ctx);
}

int znd_start(int argc, char **argv, struct spdk_app_opts *opts,
              struct zstore_context_t *ctx)
{
    int rc = 0;
    spdk_app_opts_init(opts, sizeof(*opts));
    opts->name = "zstore_frontend";

    if ((rc = spdk_app_parse_args(argc, argv, opts, NULL, NULL, NULL, NULL)) !=
        SPDK_APP_PARSE_ARGS_SUCCESS) {
        exit(rc);
    }
    ctx->bdev_name = const_cast<char *>(g_bdev_name);

    rc = spdk_app_start(opts, start_zstore, ctx);
    if (rc) {
        log_error("ERROR starting application\n");
    }
    return rc;
}

int znd_teardown(struct zstore_context_t *ctx)
{
    int rc = 0;
    spdk_dma_free(ctx->write_buff);
    spdk_dma_free(ctx->read_buff);

    spdk_app_fini();
    return rc;
}
