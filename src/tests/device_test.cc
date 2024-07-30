#include "../include/utils.hpp"
#include "../include/zns_device.h"
#include "spdk/env.h"
#include "spdk/event.h"
#include "spdk/log.h"
#include "spdk/nvme.h"
#include "spdk/nvme_zns.h"
#include "src/include/utils.hpp"
#include <cstdint>
#include <cstdlib>
#include <fmt/core.h>
#include <fstream>
#include <iostream>
#include <stdio.h>

extern "C" {

#define DEBUG
#ifdef DEBUG
#define DEBUG_TEST_PRINT(str, code)                                            \
    do {                                                                       \
        if ((code) == 0) {                                                     \
            printf("%s\x1B[32m%u\x1B[0m\n", (str), (code));                    \
        } else {                                                               \
            printf("%s\x1B[31m%u\x1B[0m\n", (str), (code));                    \
        }                                                                      \
    } while (0)
#else
#define DEBUG_TEST_PRINT(str, code)                                            \
    do {                                                                       \
    } while (0)
#endif

#define VALID(rc) assert((rc) == 0)
#define INVALID(rc) assert((rc) != 0)

/* Function used for validating linking and build issues.*/
int __TEST_interface()
{
    printf("TEST TEXT PRINTING\n");
    return 2022;
}

int z_init(void *arg)
{
    struct ZstoreContext *ctx = static_cast<struct ZstoreContext *>(arg);
    // *manager = (DeviceManager *)calloc(1, sizeof(DeviceManager));
    // ERROR_ON_NULL(*ctx, 1);
    // Setup options
    struct spdk_env_opts opts;
    opts.name = "m1";
    spdk_env_opts_init(&opts);
    // Setup SPDK
    ctx->g_trid = {};
    spdk_nvme_trid_populate_transport(&ctx->g_trid, SPDK_NVME_TRANSPORT_PCIE);
    if (spdk_env_init(&opts) < 0) {
        free(ctx);
        return 2;
    }
    // setup stubctx.info
    ctx->info = {
        .lba_size = 0, .zone_size = 0, .mdts = 0, .zasl = 0, .lba_cap = 0};
    return 0;
}

int z_close(void *arg)
{
    struct ZstoreContext *ctx = static_cast<struct ZstoreContext *>(arg);
    // ERROR_ON_NULL(manager->ctrlr, 1);
    spdk_nvme_detach_async(ctx->ctrlr, nullptr);
    ctx->ctrlr = nullptr;
    ctx->ns = nullptr;
    ctx->info = {
        .lba_size = 0, .zone_size = 0, .mdts = 0, .zasl = 0, .lba_cap = 0};
    return 0;
}
int z_shutdown(void *arg)
{
    struct ZstoreContext *ctx = static_cast<struct ZstoreContext *>(arg);
    // ERROR_ON_NULL(manager, 1);
    int rc = 0;
    if (ctx->ctrlr != NULL) {
        rc = z_close(ctx) | rc;
    }
    free(ctx);
    return rc;
}

int z_open(void *arg, const char *traddr)
{
    struct ZstoreContext *ctx = static_cast<struct ZstoreContext *>(arg);
    DeviceProber prober = {.ctx = ctx,
                           .traddr = traddr,
                           .traddr_len = (u_int8_t)strlen(traddr),
                           .found = false};
    // Find and open device
    struct spdk_nvme_probe_ctx *probe_ctx;
    probe_ctx = (struct spdk_nvme_probe_ctx *)spdk_nvme_probe(
        &ctx->g_trid, &prober, (spdk_nvme_probe_cb)__probe_devices_cb,
        (spdk_nvme_attach_cb)__attach_devices__cb, NULL);
    if (probe_ctx != 0) {
        spdk_env_fini();
        return 1;
    }
    if (!prober.found) {
        return 2;
    }
    return z_get_device_info(ctx);
}

int z_remote_open(void *arg, const char *traddr)
{
    struct ZstoreContext *ctx = static_cast<struct ZstoreContext *>(arg);
    DeviceProber prober = {.ctx = ctx,
                           .traddr = traddr,
                           .traddr_len = (u_int8_t)strlen(traddr),
                           .found = false};
    // Find and open device
    struct spdk_nvme_probe_ctx *probe_ctx;
    probe_ctx = (struct spdk_nvme_probe_ctx *)spdk_nvme_probe(
        &ctx->g_trid, &prober, (spdk_nvme_probe_cb)__probe_devices_cb,
        (spdk_nvme_attach_cb)__attach_devices__cb, NULL);
    if (probe_ctx != 0) {
        spdk_env_fini();
        return 1;
    }
    if (!prober.found) {
        return 2;
    }
    return z_get_device_info(ctx);
}

int z_local_open(void *arg, const char *traddr)
{
    struct ZstoreContext *ctx = static_cast<struct ZstoreContext *>(arg);
    DeviceProber prober = {.ctx = ctx,
                           .traddr = traddr,
                           .traddr_len = (u_int8_t)strlen(traddr),
                           .found = false};
    // Find and open device
    struct spdk_nvme_probe_ctx *probe_ctx;
    probe_ctx = (struct spdk_nvme_probe_ctx *)spdk_nvme_probe(
        &ctx->g_trid, &prober, (spdk_nvme_probe_cb)__probe_devices_cb,
        (spdk_nvme_attach_cb)__attach_devices__cb, NULL);
    if (probe_ctx != 0) {
        spdk_env_fini();
        return 1;
    }
    if (!prober.found) {
        return 2;
    }
    return z_get_device_info(ctx);
}

int write_pattern(char **pattern, void *arg, int32_t size, int32_t jump)
{
    struct ZstoreContext *ctx = static_cast<struct ZstoreContext *>(arg);
    if (*pattern != NULL) {
        z_free(ctx->qpair, *pattern);
    }
    *pattern = (char *)z_calloc(ctx, size, sizeof(char *));
    if (*pattern == NULL) {
        return 1;
    }
    for (int j = 0; j < size; j++) {
        (*pattern)[j] = j % 200 + jump;
    }
    return 0;
}

int write_zstore_pattern(char **pattern, void *arg, int32_t size,
                         char *test_str)
{
    struct ZstoreContext *ctx = static_cast<struct ZstoreContext *>(arg);
    if (*pattern != NULL) {
        z_free(ctx->qpair, *pattern);
    }
    *pattern = (char *)z_calloc(ctx, size, sizeof(char *));
    if (*pattern == NULL) {
        return 1;
    }
    snprintf(*pattern, ctx->info.lba_size, "%s", test_str);
    // for (int j = 0; j < size; j++) {
    //     (*pattern)[j] = j % 200 + jump;
    // }
    return 0;
}

int main(int argc, char **argv)
{
    printf("----------------------UNDEFINED----------------------\n");
    int rc = __TEST_interface();
    DEBUG_TEST_PRINT("interface setup ", rc);
    assert(rc == 2022);

    // init spdk
    printf("----------------------INIT----------------------\n");
    struct ZstoreContext ctx = {};
    // struct ZstoreContext *ctx = static_cast<struct ZstoreContext *>(arg1);
    // DeviceManager **manager =
    //     (DeviceManager **)calloc(1, sizeof(DeviceManager));
    rc = z_init(&ctx);
    VALID(rc);

    // try non-existent device
    rc = z_open(&ctx, "non-existent traddr");
    DEBUG_TEST_PRINT("non-existent return code ", rc);
    INVALID(rc);

    // try existing device
    rc = z_open(&ctx, "0000:06:00.0");
    DEBUG_TEST_PRINT("existing return code ", rc);
    VALID(rc);

    // ensure that everything from this device is OK
    assert(ctx.ctrlr != NULL);
    assert(ctx.ns != NULL);
    assert(ctx.info.lba_size > 0);
    assert(ctx.info.mdts > 0);
    assert(ctx.info.zasl > 0);
    assert(ctx.info.zone_size > 0);
    assert(ctx.info.lba_cap > 0);

    // create qpair
    // QPair *&ctx = (QPair **)calloc(1, sizeof(QPair *));
    // rc = z_create_qpair(*manager, qpair);
    ctx.qpair = spdk_nvme_ctrlr_alloc_io_qpair(ctx.ctrlr, NULL, 0);
    DEBUG_TEST_PRINT("Qpair creation code ", rc);
    VALID(rc);
    assert(ctx.qpair != nullptr);

    // get and verify data (based on ZNS QEMU image)
    // DeviceInfoctx.info = {};
    rc = z_get_device_info(&ctx);
    DEBUG_TEST_PRINT("getctx.info code ", rc);
    VALID(rc);
    printf("lba size is %d\n", ctx.info.lba_size);
    printf("zone size is %d\n", ctx.info.zone_size);
    printf("mdts is %d\n", ctx.info.mdts);
    printf("zasl is %d\n", ctx.info.zasl);
    printf("lba_cap is %d\n", ctx.info.lba_cap);

    uint64_t write_head;
    printf("----------------------WORKLOAD SMALL----------------------\n");
    // make space by resetting the device zones
    rc = z_reset(&ctx, 0, true);
    DEBUG_TEST_PRINT("reset all code ", rc);
    VALID(rc);
    rc = z_get_zone_head(&ctx, 0, &write_head);
    VALID(rc);
    assert(write_head == 0);

    char **pattern_1 = (char **)calloc(1, sizeof(char **));
    rc = write_pattern(pattern_1, &ctx, ctx.info.lba_size, 10);
    VALID(rc);
    log_debug("lba: {}", ctx.info.lba_size);
    rc = z_append(&ctx, 0, *pattern_1, ctx.info.lba_size);
    DEBUG_TEST_PRINT("append alligned ", rc);
    VALID(rc);

    rc = z_get_zone_head(&ctx, 0, &write_head);
    VALID(rc);
    assert(write_head == 1);
    char **pattern_2 = (char **)calloc(1, sizeof(char **));
    rc = write_pattern(pattern_2, &ctx, ctx.info.zasl, 13);
    VALID(rc);
    rc = z_append(&ctx, 0, *pattern_2, ctx.info.zasl);
    DEBUG_TEST_PRINT("append zasl ", rc);
    VALID(rc);
    rc = z_get_zone_head(&ctx, 0, &write_head);
    VALID(rc);
    assert(write_head == 1 + ctx.info.zasl / ctx.info.lba_size);
    char *pattern_read_1 =
        (char *)z_calloc(&ctx, ctx.info.lba_size, sizeof(char *));
    rc = z_read(&ctx, 0, pattern_read_1, ctx.info.lba_size);
    DEBUG_TEST_PRINT("read alligned ", rc);
    VALID(rc);
    for (int i = 0; i < ctx.info.lba_size; i++) {
        assert((char *)(pattern_read_1)[i] == (char *)(*pattern_1)[i]);
    }
    char *pattern_read_2 =
        (char *)z_calloc(&ctx, ctx.info.zasl, sizeof(char *));
    rc = z_read(&ctx, 1, pattern_read_2, ctx.info.zasl);
    DEBUG_TEST_PRINT("read zasl ", rc);
    VALID(rc);
    for (int i = 0; i < ctx.info.zasl; i++) {
        assert((char *)(pattern_read_2)[i] == (char *)(*pattern_2)[i]);
        // printf("%d-th write %c, read %c\n", i, (char *)(*pattern_2)[i],
        //        (char *)(pattern_read_2)[i]);
    }

    printf("----------------------WORKLOAD ZSTORE----------------------\n");
    log_debug("1");
    char **pattern_zstore = (char **)calloc(1, sizeof(char **));
    // char *wbuf = (char *)z_calloc(ctx, 4096, sizeof(char));

    log_debug("2");
    // rc = write_pattern(pattern_zstore, &ctx, ctx.info.lba_size, 42);
    rc = write_zstore_pattern(pattern_zstore, &ctx, ctx.info.lba_size,
                              "test_zstore:42");

    log_debug("3");
    rc = z_append(&ctx, 0, *pattern_zstore, ctx.info.lba_size);
    DEBUG_TEST_PRINT("append zstore testing content ", rc);
    VALID(rc);

    rc = z_get_zone_head(&ctx, 0, &write_head);
    VALID(rc);
    assert(write_head == 3);
    assert(write_head == 2 + ctx.info.zasl / ctx.info.lba_size);
    char *pattern_read_zstore =
        (char *)z_calloc(&ctx, ctx.info.lba_size, sizeof(char *));
    rc = z_read(&ctx, 2, pattern_read_zstore, ctx.info.lba_size);
    DEBUG_TEST_PRINT("read zstore testing content ", rc);
    VALID(rc);
    log_debug("here");
    for (int i = 0; i < ctx.info.lba_size; i++) {
        // log_debug("{}", i);
        assert((char *)(pattern_read_zstore)[i] ==
               (char *)(*pattern_zstore)[i]);
        // printf("%d-th write %c, read %c\n", i, (char *)(*pattern_zstore)[i],
        //        (char *)(pattern_read_zstore)[i]);
    }

    rc = z_reset(&ctx, 0, true);
    DEBUG_TEST_PRINT("reset all ", rc);
    VALID(rc);
    rc = z_read(&ctx, 1, pattern_read_2, ctx.info.zasl);
    DEBUG_TEST_PRINT("verify empty first zone ", rc);
    VALID(rc);
    for (int i = 0; i < ctx.info.zasl; i++) {
        assert((char *)(pattern_read_2)[i] == 0);
    }

    printf("----------------------WORKLOAD FILL----------------------\n");
    char **pattern_3 = (char **)calloc(1, sizeof(char **));
    rc = write_pattern(pattern_3, &ctx, ctx.info.lba_size * ctx.info.lba_cap,
                       19);
    VALID(rc);

    log_info("FAIL: before pattern3 append");
    log_debug("lba size {}, lba cap {}", ctx.info.lba_size, ctx.info.lba_cap);
    log_debug("zone size {}, lba size * zone size {}", ctx.info.zone_size,
              ctx.info.lba_size * ctx.info.zone_size);
    // NOTE(shuwen): the following calculation is wrong. lba_cap is already the
    // correct bytes, so it should not times lba_size
    // rc = z_append(&ctx, 0, *pattern_3, ctx.info.lba_size *
    // ctx.info.zone_size);
    rc = z_append(&ctx, 0, *pattern_3, ctx.info.lba_cap);
    DEBUG_TEST_PRINT("fill entire device ", rc);
    VALID(rc);

    // WTF?
    // for (int i = 0; i < ctx.info.lba_cap / ctx.info.zone_size; i++) {
    //     rc = z_get_zone_head(&ctx, i * ctx.info.zone_size, &write_head);
    //     VALID(rc);
    //     log_info("write head for {}: {}", i, write_head);
    //     assert(write_head == ~0lu);
    // }

    char *pattern_read_3 =
        (char *)z_calloc(&ctx, ctx.info.lba_cap, sizeof(char *));
    rc = z_read(&ctx, 0, pattern_read_3, ctx.info.lba_cap);
    DEBUG_TEST_PRINT("read entire device ", rc);
    VALID(rc);
    for (int i = 0; i < ctx.info.lba_cap; i++) {
        assert((char *)(pattern_read_3)[i] == (char *)(*pattern_3)[i]);
    }
    rc = z_reset(&ctx, ctx.info.zone_size, false);
    rc = z_reset(&ctx, ctx.info.zone_size * 2, false) | rc;
    DEBUG_TEST_PRINT("reset zone 2,3 ", rc);
    VALID(rc);
    log_info("write head {}", write_head);
    rc = z_get_zone_head(&ctx, 1, &write_head);
    // rc = z_get_zone_head(&ctx, 0, &write_head);
    VALID(rc);
    log_info("write head {}", write_head);

    // WTF
    // assert(write_head == ~0lu);

    rc = z_get_zone_head(&ctx, ctx.info.zone_size, &write_head);
    VALID(rc);
    assert(write_head == ctx.info.zone_size);
    rc = z_get_zone_head(&ctx, ctx.info.zone_size * 2, &write_head);
    VALID(rc);
    assert(write_head == ctx.info.zone_size * 2);
    char *pattern_read_4 =
        (char *)z_calloc(&ctx, ctx.info.zone_size, sizeof(char *));
    rc = z_read(&ctx, 0, pattern_read_4, ctx.info.zone_size);
    DEBUG_TEST_PRINT("read zone 1 ", rc);
    VALID(rc);
    for (int i = 0; i < ctx.info.zone_size; i++) {
        assert((char *)(pattern_read_4)[i] == (char *)(*pattern_3)[i]);
    }
    rc = z_read(&ctx, ctx.info.zone_size, pattern_read_4, ctx.info.zone_size);
    DEBUG_TEST_PRINT("read zone 2 ", rc);
    VALID(rc);
    for (int i = 0; i < ctx.info.zone_size; i++) {
        assert((char *)(pattern_read_4)[i] == 0);
    }
    rc = z_read(&ctx, ctx.info.zone_size * 2, pattern_read_4,
                ctx.info.zone_size);
    DEBUG_TEST_PRINT("read zone 3 ", rc);
    VALID(rc);
    for (int i = 0; i < ctx.info.zone_size; i++) {
        assert((char *)(pattern_read_4)[i] == 0);
    }
    rc = z_read(&ctx, ctx.info.zone_size * 3, pattern_read_4,
                ctx.info.zone_size);
    DEBUG_TEST_PRINT("read zone 4 ", rc);
    VALID(rc);
    // FIXME
    // for (int i = 0; i < ctx.info.zone_size; i++) {
    //     assert((char *)(pattern_read_4)[i] ==
    //            (char *)(*pattern_3)[i + ctx.info.zone_size * 3 *
    //                                         ctx.info.lba_size]);
    // }
    rc = z_reset(&ctx, 0, true);
    DEBUG_TEST_PRINT("reset all ", rc);
    VALID(rc);

    printf("----------------------WORKLOAD EDGE----------------------\n");
    // FIXME
    // rc = z_append(&ctx, 0, *pattern_3,
    //               ctx.info.lba_size * (ctx.info.zone_size - 3));
    // DEBUG_TEST_PRINT("zone friction part 1: append 1 zoneborder - 3 ", rc);
    // VALID(rc);
    // rc = z_get_zone_head(&ctx, 0, &write_head);
    // VALID(rc);
    // assert(write_head == ctx.info.zone_size - 3);
    // rc = z_append(&ctx, ctx.info.zone_size - 3,
    //               *pattern_3 + ctx.info.lba_size * (ctx.info.zone_size - 3),
    //               ctx.info.lba_size * 6);
    // DEBUG_TEST_PRINT("zone friction part 2: append 1 zoneborder + 6 ", rc);
    // VALID(rc);
    // rc = z_get_zone_head(&ctx, 0, &write_head);
    // VALID(rc);
    // assert(write_head == ~0lu);
    // rc = z_get_zone_head(&ctx, ctx.info.zone_size, &write_head);
    // VALID(rc);
    // assert(write_head == ctx.info.zone_size + 3);
    // rc = z_append(&ctx, ctx.info.zone_size + 3,
    //               *pattern_3 + ctx.info.lba_size * (ctx.info.zone_size + 3),
    //               ctx.info.lba_size * 13);
    // DEBUG_TEST_PRINT("zone friction part 3: append 1 zoneborder + 16 ", rc);
    // VALID(rc);
    // rc = z_get_zone_head(&ctx, ctx.info.zone_size, &write_head);
    // VALID(rc);
    // assert(write_head == ctx.info.zone_size + 16);
    // rc = z_read(&ctx, 0, pattern_read_4,
    //             ctx.info.lba_size * (ctx.info.zone_size - 3));
    // DEBUG_TEST_PRINT("zone friction part 4: read 1 zoneborder - 3 ", rc);
    // VALID(rc);
    // rc = z_read(&ctx, ctx.info.zone_size - 3,
    //             pattern_read_4 + ctx.info.lba_size * (ctx.info.zone_size -
    //             3), ctx.info.lba_size * 6);
    // DEBUG_TEST_PRINT("zone friction part 5: read 1 zoneborder + 3 ", rc);
    // VALID(rc);
    // rc = z_read(&ctx, ctx.info.zone_size + 3,
    //             pattern_read_4 + ctx.info.lba_size * (ctx.info.zone_size +
    //             3), ctx.info.lba_size * 13);
    // DEBUG_TEST_PRINT("zone friction part 6: read 1 zoneborder + 16 ", rc);
    // VALID(rc);
    // for (int i = 0; i < ctx.info.lba_size * (ctx.info.zone_size + 15); i++) {
    //     assert((char *)(pattern_read_4)[i] == (char *)(*pattern_3)[i]);
    // }

    // destroy qpair
    printf("----------------------CLOSE----------------------\n");
    // rc = z_destroy_qpair(&ctx);
    // DEBUG_TEST_PRINT("valid destroy code ", rc);
    // VALID(rc);

    // close device
    rc = z_close(&ctx);
    DEBUG_TEST_PRINT("valid close code ", rc);
    VALID(rc);

    // can not close twice
    // rc = z_close(&ctx);
    // DEBUG_TEST_PRINT("invalid close code ", rc);
    // INVALID(rc);

    // rc = z_shutdown(&ctx);
    // DEBUG_TEST_PRINT("valid shutdown code ", rc);
    // VALID(rc);

    // cleanup local
    free(pattern_1);
    free(pattern_2);
    free(pattern_3);

    // free(&ctx);
    // free(manager);
}
}
