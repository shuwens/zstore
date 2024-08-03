#include "include/utils.hpp"
#include "include/zns_device.h"
#include "spdk/event.h"
#include "spdk/log.h"
#include "spdk/nvme.h"
#include <bits/stdc++.h>
#include <cstdint>
#include <cstdlib>
#include <fmt/core.h>
#include <fstream>
#include <iostream>
#include <stdio.h>
#include <vector>

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

static void zns_multipath(void *arg)
{
    char *node = "zstore1";
    log_info("Fn: zns_multipath for {}\n", node);
    struct ZstoreContext *ctx = static_cast<struct ZstoreContext *>(arg);
    struct spdk_nvme_io_qpair_opts qpair_opts = {};
    u64 starting_zone = ctx->current_zone - 1;

    std::vector<int> qds{64};
    // std::vector<int> qds{2, 64};
    // std::vector<int> qds{2, 4, 8, 16, 32, 64};

    for (auto qd : qds) {
        starting_zone += 1;
        ctx->current_zone = starting_zone;
        log_info("\nStarting with zone {}, queue depth {}, append times {}\n",
                 starting_zone, qd, append_times);
        ctx->qd = qd;
        qpair_opts.io_queue_size = ctx->qd;
        qpair_opts.io_queue_requests = ctx->qd;
        zns_dev_init(ctx, "192.168.1.121", "4420", "192.168.1.121", "5520");

        zstore_qpair_setup(ctx, qpair_opts);
        zstore_init(ctx);

        z_get_device_info(ctx);

        ctx->zstore_open = true;
        ctx->current_lba = 0;

        int rc1 = 0;
        int rc2 = 0;

        log_info("writing with z_append:");
        log_debug("here");
        log_info("{} append start lba {}", node, ctx->zslba);
        char **wbuf = (char **)calloc(1, sizeof(char **));
        for (int i = 0; i < append_times; i++) {
            rc1 = write_zstore_pattern(wbuf, &ctx->m1, ctx->m1.info.lba_size,
                                       node, i);
            assert(rc1 == 0);

            // APPEND
            rc1 = z_append(&ctx->m1, ctx->zslba, *wbuf, ctx->m1.info.lba_size);
            rc2 = z_append(&ctx->m2, ctx->zslba, *wbuf, ctx->m2.info.lba_size);
            assert(rc1 == 0 && rc2 == 0);
        }

        // ctx->current_lba = ;
        log_info("{} current lba for read is {}", node, ctx->current_lba);

        log_info("read with z_append:");
        char *rbuf1 =
            (char *)z_calloc(&ctx->m1, ctx->m1.info.lba_size, sizeof(char *));
        char *rbuf2 =
            (char *)z_calloc(&ctx->m2, ctx->m2.info.lba_size, sizeof(char *));
        // std::vector<std::string> data1;
        for (int i = 0; i < append_times * 2; i++) {
            rc1 = z_read(&ctx->m1, ctx->current_lba + i, rbuf1, 4096);
            rc2 = z_read(&ctx->m2, ctx->current_lba + i, rbuf2, 4096);
            assert(rc1 == 0 && rc2 == 0);
            // data1.push_back(*(char *)rbuf);
            // std::string str(rbuf);
            // data1.push_back(str);
            // printf("%d-th read %s\n", i, (char *)(rbuf));
        }

        zstore_qpair_teardown(ctx);

        std::ofstream of1("data1.txt");
        // std::ofstream of2("data2.txt");
        // for (auto d : data1) {
        //     log_info("{}", d);
        //     of1 << d << " ";
        // }
        // for (auto d : data2)
        //     of2 << d - data_off << " ";
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
    opts.name = "zns_multipath_opts";
    // opts.shutdown_cb = NULL;
    opts.shutdown_cb = test_cleanup;
    if ((rc = spdk_app_parse_args(argc, argv, &opts, NULL, NULL, NULL, NULL)) !=
        SPDK_APP_PARSE_ARGS_SUCCESS) {
        exit(rc);
    }

    struct ZstoreContext ctx = {};
    ctx.current_zone = current_zone;
    // ctx.verbose = true;

    rc = spdk_app_start(&opts, zns_multipath, &ctx);
    if (rc) {
        SPDK_ERRLOG("ERROR starting application\n");
    }

    log_info("zstore exits gracefully");
    spdk_app_fini();

    return rc;
}
