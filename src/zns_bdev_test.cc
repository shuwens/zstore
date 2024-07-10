// rw_test.cc
/*   SPDX-License-Identifier: BSD-3-Clause
 *   Copyright (C) 2018 Intel Corporation.
 *   All rights reserved.
 */

#include "spdk/bdev.h"
#include "spdk/bdev_zone.h"
#include "spdk/env.h"
#include "spdk/event.h"
#include "spdk/log.h"
#include "spdk/stdinc.h"
#include "spdk/string.h"
#include "spdk/thread.h"

#include <atomic>

static const char *g_bdev_name = "Nvme1n2";

struct rwtest_context_t {
    struct spdk_bdev *bdev;
    struct spdk_bdev_desc *bdev_desc;
    struct spdk_io_channel *bdev_io_channel;
    char *write_buff;
    char *read_buff;
    uint32_t buff_size;
    char *bdev_name;
    struct spdk_bdev_io_wait_entry bdev_io_wait;

    std::atomic<int> count; // 原子变量，避免并发修改冲突
};

static void read_zone_complete(struct spdk_bdev_io *bdev_io, bool success,
                               void *cb_arg)
{
    struct rwtest_context_t *test_context =
        static_cast<struct rwtest_context_t *>(cb_arg);
    spdk_bdev_free_io(bdev_io);

    if (!success) {
        SPDK_ERRLOG("bdev io read zone error: %d\n", EIO);
        spdk_put_io_channel(test_context->bdev_io_channel);
        spdk_bdev_close(test_context->bdev_desc);
        spdk_app_stop(-1);
        return;
    }
    // 比对读缓冲区与写缓冲区内容
    int cmp_res = memcmp(test_context->write_buff, test_context->read_buff,
                         test_context->buff_size);
    if (cmp_res != 0) {
        SPDK_ERRLOG("read zone data error.\n");
        spdk_put_io_channel(test_context->bdev_io_channel);
        spdk_bdev_close(test_context->bdev_desc);
        spdk_app_stop(-1);
        return;
    }
    test_context->count.fetch_add(1);
    if (test_context->count.load() == 4 * 0x100) { // 读取测试完成，结束测试
        SPDK_NOTICELOG("read zone complete.\n");
        spdk_put_io_channel(test_context->bdev_io_channel);
        spdk_bdev_close(test_context->bdev_desc);
        spdk_app_stop(0);
        return;
    }

    memset(test_context->read_buff, 0x34, test_context->buff_size);
    // 循环读取，直至读到最后一个测试LBA
    uint64_t lba = test_context->count.load() / 0x100 *
                       spdk_bdev_get_zone_size(test_context->bdev) +
                   test_context->count.load() % 0x100; // 计算对应的LBA

    int rc = spdk_bdev_read_blocks(
        test_context->bdev_desc, test_context->bdev_io_channel,
        test_context->read_buff, lba, 1, read_zone_complete, test_context);
    SPDK_NOTICELOG("read lba:0x%lx\n", lba);
    if (rc != 0) {
        SPDK_ERRLOG("%s error while reading from bdev: %d\n",
                    spdk_strerror(-rc), rc);
        spdk_put_io_channel(test_context->bdev_io_channel);
        spdk_bdev_close(test_context->bdev_desc);
        spdk_app_stop(-1);
        return;
    }
}

static void read_zone(void *arg)
{
    int rc = 0;
    struct rwtest_context_t *test_context =
        static_cast<struct rwtest_context_t *>(arg);
    test_context->count = 0;
    memset(test_context->read_buff, 0x34,
           test_context->buff_size); // 读取前缓冲区内容为0x34
    rc = spdk_bdev_read_blocks(
        test_context->bdev_desc, test_context->bdev_io_channel,
        test_context->read_buff, 0, 1, read_zone_complete, test_context);
    SPDK_NOTICELOG("read lba:0x%x\n", 0x0);
    if (rc == -ENOMEM) {
        SPDK_NOTICELOG("Queueing io\n");
        /* In case we cannot perform I/O now, queue I/O */
        test_context->bdev_io_wait.bdev = test_context->bdev;
        test_context->bdev_io_wait.cb_fn = read_zone;
        test_context->bdev_io_wait.cb_arg = test_context;
        spdk_bdev_queue_io_wait(test_context->bdev,
                                test_context->bdev_io_channel,
                                &test_context->bdev_io_wait);
    } else if (rc) {
        SPDK_ERRLOG("%s error while reading from bdev: %d\n",
                    spdk_strerror(-rc), rc);
        spdk_put_io_channel(test_context->bdev_io_channel);
        spdk_bdev_close(test_context->bdev_desc);
        spdk_app_stop(-1);
    }
}

static void write_zone_complete(struct spdk_bdev_io *bdev_io, bool success,
                                void *cb_arg)
{
    struct rwtest_context_t *test_context =
        static_cast<struct rwtest_context_t *>(cb_arg);
    SPDK_NOTICELOG("append lba:0x%lx\n", spdk_bdev_io_get_append_location(
                                             bdev_io)); // 打印成功append位置
    spdk_bdev_free_io(bdev_io);

    if (!success) {
        SPDK_ERRLOG("bdev io write zone error: %d\n", EIO);
        spdk_put_io_channel(test_context->bdev_io_channel);
        spdk_bdev_close(test_context->bdev_desc);
        spdk_app_stop(-1);
        return;
    }
    test_context->count.fetch_sub(1);
    if (test_context->count.load() == 0) { // zone写入完成，开始读取数据验证
        SPDK_NOTICELOG("write zone complete.\n");
        read_zone(test_context);
    }
}
static void write_zone(void *arg)
{
    int rc = 0;
    struct rwtest_context_t *test_context =
        static_cast<struct rwtest_context_t *>(arg);
    uint64_t zone_size = spdk_bdev_get_zone_size(test_context->bdev);
    // 往4个zone的前0x100个block李写入0x12
    int zone_num = 4;
    int append_times = 0x100;
    test_context->count = zone_num * append_times;
    memset(test_context->write_buff, 0x12, test_context->buff_size);
    for (uint64_t slba = 0; slba < zone_num * zone_size; slba += zone_size) {
        for (int i = 0; i < append_times; i++) {
            rc = spdk_bdev_zone_append(test_context->bdev_desc,
                                       test_context->bdev_io_channel,
                                       test_context->write_buff, slba, 1,
                                       write_zone_complete, test_context);
            if (rc != 0) {
                SPDK_ERRLOG("%s error while write_zone: %d\n",
                            spdk_strerror(-rc), rc);
                spdk_put_io_channel(test_context->bdev_io_channel);
                spdk_bdev_close(test_context->bdev_desc);
                spdk_app_stop(-1);
            }
        }
    }
}
static void reset_zone_complete(struct spdk_bdev_io *bdev_io, bool success,
                                void *cb_arg)
{
    struct rwtest_context_t *test_context =
        static_cast<struct rwtest_context_t *>(cb_arg);
    spdk_bdev_free_io(bdev_io);

    if (!success) {
        SPDK_ERRLOG("bdev io reset zone error: %d\n", EIO);
        spdk_put_io_channel(test_context->bdev_io_channel);
        spdk_bdev_close(test_context->bdev_desc);
        spdk_app_stop(-1);
        return;
    }
    test_context->count.fetch_sub(1);
    if (test_context->count.load() == 0) { // zone重置完成，开始写入数据
        SPDK_NOTICELOG("reset zone complete.\n");
        write_zone(test_context);
    }
}

static void reset_zone(void *arg)
{
    struct rwtest_context_t *test_context =
        static_cast<struct rwtest_context_t *>(arg);
    int rc = 0;
    // 重置前10个zone
    int zone_num = 10;
    test_context->count = zone_num;
    uint64_t zone_size = spdk_bdev_get_zone_size(test_context->bdev);
    for (uint64_t slba = 0; slba < zone_num * zone_size; slba += zone_size) {
        rc = spdk_bdev_zone_management(
            test_context->bdev_desc, test_context->bdev_io_channel, slba,
            SPDK_BDEV_ZONE_RESET, reset_zone_complete, test_context);
        if (rc == -ENOMEM) {
            SPDK_NOTICELOG("Queueing io\n");
            /* In case we cannot perform I/O now, queue I/O */
            test_context->bdev_io_wait.bdev = test_context->bdev;
            test_context->bdev_io_wait.cb_fn = reset_zone;
            test_context->bdev_io_wait.cb_arg = test_context;
            spdk_bdev_queue_io_wait(test_context->bdev,
                                    test_context->bdev_io_channel,
                                    &test_context->bdev_io_wait);
        } else if (rc) {
            SPDK_ERRLOG("%s error while resetting zone: %d\n",
                        spdk_strerror(-rc), rc);
            spdk_put_io_channel(test_context->bdev_io_channel);
            spdk_bdev_close(test_context->bdev_desc);
            spdk_app_stop(-1);
        }
    }
}

static void test_bdev_event_cb(enum spdk_bdev_event_type type,
                               struct spdk_bdev *bdev, void *event_ctx)
{
    SPDK_NOTICELOG("Unsupported bdev event: type %d\n", type);
}

static void test_start(void *arg1)
{
    struct rwtest_context_t *test_context =
        static_cast<struct rwtest_context_t *>(arg1);
    uint32_t buf_align;
    int rc = 0;
    test_context->bdev = NULL;
    test_context->bdev_desc = NULL;

    SPDK_NOTICELOG("Successfully started the application\n");
    SPDK_NOTICELOG("Opening the bdev %s\n", test_context->bdev_name);
    rc = spdk_bdev_open_ext(test_context->bdev_name, true, test_bdev_event_cb,
                            NULL, &test_context->bdev_desc);
    if (rc) {
        SPDK_ERRLOG("Could not open bdev: %s\n", test_context->bdev_name);
        spdk_app_stop(-1);
        return;
    }
    test_context->bdev = spdk_bdev_desc_get_bdev(test_context->bdev_desc);

    SPDK_NOTICELOG("Opening io channel\n");
    /* Open I/O channel */
    test_context->bdev_io_channel =
        spdk_bdev_get_io_channel(test_context->bdev_desc);
    if (test_context->bdev_io_channel == NULL) {
        SPDK_ERRLOG("Could not create bdev I/O channel!!\n");
        spdk_bdev_close(test_context->bdev_desc);
        spdk_app_stop(-1);
        return;
    }
    test_context->buff_size = spdk_bdev_get_block_size(test_context->bdev) *
                              spdk_bdev_get_write_unit_size(test_context->bdev);
    buf_align = spdk_bdev_get_buf_align(test_context->bdev);
    test_context->write_buff = static_cast<char *>(
        spdk_dma_zmalloc(test_context->buff_size, buf_align, NULL));
    if (!test_context->write_buff) {
        SPDK_ERRLOG("Failed to allocate buffer\n");
        spdk_put_io_channel(test_context->bdev_io_channel);
        spdk_bdev_close(test_context->bdev_desc);
        spdk_app_stop(-1);
        return;
    }
    test_context->read_buff = static_cast<char *>(
        spdk_dma_zmalloc(test_context->buff_size, buf_align, NULL));
    if (!test_context->read_buff) {
        SPDK_ERRLOG("Failed to allocate buffer\n");
        spdk_put_io_channel(test_context->bdev_io_channel);
        spdk_bdev_close(test_context->bdev_desc);
        spdk_app_stop(-1);
        return;
    }

    if (!spdk_bdev_is_zoned(test_context->bdev)) {
        SPDK_ERRLOG("not a ZNS SSD\n");
        spdk_put_io_channel(test_context->bdev_io_channel);
        spdk_bdev_close(test_context->bdev_desc);
        spdk_app_stop(-1);
        return;
    }
    // 打印ZNS SSD一些信息
    SPDK_NOTICELOG("block size:%d write unit:%d zone size:%lx zone num:%ld max "
                   "append size:%d max open zone:%d max active "
                   "zone:%d\n",
                   spdk_bdev_get_block_size(test_context->bdev),
                   spdk_bdev_get_write_unit_size(test_context->bdev),
                   spdk_bdev_get_zone_size(test_context->bdev),
                   spdk_bdev_get_num_zones(test_context->bdev),
                   spdk_bdev_get_max_zone_append_size(test_context->bdev),
                   spdk_bdev_get_max_open_zones(test_context->bdev),
                   spdk_bdev_get_max_active_zones(test_context->bdev));
    reset_zone(test_context);
}

int main(int argc, char **argv)
{
    struct spdk_app_opts opts = {};
    int rc = 0;
    struct rwtest_context_t test_context = {};

    spdk_app_opts_init(&opts, sizeof(opts));
    opts.name = "test_bdev";

    if ((rc = spdk_app_parse_args(argc, argv, &opts, NULL, NULL, NULL, NULL)) !=
        SPDK_APP_PARSE_ARGS_SUCCESS) {
        exit(rc);
    }
    test_context.bdev_name = const_cast<char *>(g_bdev_name);

    rc = spdk_app_start(&opts, test_start, &test_context);
    if (rc) {
        SPDK_ERRLOG("ERROR starting application\n");
    }

    spdk_dma_free(test_context.write_buff);
    spdk_dma_free(test_context.read_buff);

    spdk_app_fini();
    return rc;
}
