#include "include/utils.hpp"
#include "include/zns_device.h"
#include "spdk/endian.h"
#include "spdk/env.h"
#include "spdk/event.h"
#include "spdk/log.h"
#include "spdk/nvme.h"
#include "spdk/nvme_intel.h"
#include "spdk/nvme_ocssd.h"
#include "spdk/nvme_zns.h"
#include "spdk/nvmf_spec.h"
#include "spdk/pci_ids.h"
#include "spdk/stdinc.h"
#include "spdk/string.h"
#include "spdk/thread.h"
#include "spdk/util.h"
#include "spdk/uuid.h"
#include "spdk/vmd.h"
#include <atomic>

static const char *g_bdev_name = "Nvme1n2";
static const char *g_hostnqn = "nqn.2024-04.io.zstore:cnode1";

// struct ZstoreContext {
struct rwtest_context_t {
    struct spdk_nvme_ctrlr *ctrlr = nullptr;
    struct spdk_nvme_ns *ns = nullptr;
    struct spdk_nvme_qpair *qpair = nullptr;
    char *write_buff = nullptr;
    char *read_buff = nullptr;
    uint32_t buff_size;
    // char *bdev_name;

    std::atomic<int> count; // atomic count for concurrency
};

static void read_zone_complete(void *arg,
                               const struct spdk_nvme_cpl *completion)
{
    struct rwtest_context_t *ctx = static_cast<struct rwtest_context_t *>(arg);

    if (spdk_nvme_cpl_is_error(completion)) {
        SPDK_ERRLOG("bdev io read zone error\n");
        spdk_nvme_ctrlr_free_io_qpair(ctx->qpair);
        spdk_app_stop(-1);
        return;
    }

    // compare read and write
    int cmp_res = memcmp(ctx->write_buff, ctx->read_buff, ctx->buff_size);
    if (cmp_res != 0) {
        SPDK_ERRLOG("read zone data error.\n");
        spdk_nvme_ctrlr_free_io_qpair(ctx->qpair);
        spdk_app_stop(-1);
        return;
    }
    ctx->count.fetch_add(1);
    if (ctx->count.load() == 4 * 0x100) {
        SPDK_NOTICELOG("read zone complete.\n");
        spdk_nvme_ctrlr_free_io_qpair(ctx->qpair);
        spdk_app_stop(0);
        return;
    }

    memset(ctx->read_buff, 0x34, ctx->buff_size);
    uint64_t lba =
        ctx->count.load() / 0x100 * spdk_nvme_ns_get_num_sectors(ctx->ns) +
        ctx->count.load() % 0x100;

    int rc = spdk_nvme_ns_cmd_read(ctx->ns, ctx->qpair, ctx->read_buff, lba, 1,
                                   read_zone_complete, ctx, 0);
    SPDK_NOTICELOG("read lba:0x%lx\n", lba);
    if (rc != 0) {
        SPDK_ERRLOG("error while reading from bdev: %d\n", rc);
        spdk_nvme_ctrlr_free_io_qpair(ctx->qpair);
        spdk_app_stop(-1);
        return;
    }
}

static void read_zone(void *arg)
{
    struct rwtest_context_t *ctx = static_cast<struct rwtest_context_t *>(arg);
    ctx->count = 0;
    memset(ctx->read_buff, 0x34, ctx->buff_size);
    int rc = spdk_nvme_ns_cmd_read(ctx->ns, ctx->qpair, ctx->read_buff, 0, 1,
                                   read_zone_complete, ctx, 0);
    SPDK_NOTICELOG("read lba:0x%x\n", 0x0);
    if (rc) {
        SPDK_ERRLOG("error while reading from bdev: %d\n", rc);
        spdk_nvme_ctrlr_free_io_qpair(ctx->qpair);
        spdk_app_stop(-1);
    }
}

static void write_zone_complete(void *arg,
                                const struct spdk_nvme_cpl *completion)
{
    struct rwtest_context_t *ctx = static_cast<struct rwtest_context_t *>(arg);

    if (spdk_nvme_cpl_is_error(completion)) {
        SPDK_ERRLOG("bdev io write zone error\n");
        spdk_nvme_ctrlr_free_io_qpair(ctx->qpair);
        spdk_app_stop(-1);
        return;
    }
    ctx->count.fetch_sub(1);
    if (ctx->count.load() == 0) {
        SPDK_NOTICELOG("write zone complete.\n");
        read_zone(ctx);
    }
}

static void write_zone(void *arg)
{
    struct rwtest_context_t *ctx = static_cast<struct rwtest_context_t *>(arg);
    uint64_t zone_size = spdk_nvme_ns_get_num_sectors(ctx->ns);
    int zone_num = 4;
    int append_times = 0x100;
    ctx->count = zone_num * append_times;
    memset(ctx->write_buff, 0x12, ctx->buff_size);
    for (uint64_t slba = 0; slba < zone_num * zone_size; slba += zone_size) {
        for (int i = 0; i < append_times; i++) {
            int rc =
                spdk_nvme_ns_cmd_write(ctx->ns, ctx->qpair, ctx->write_buff,
                                       slba, 1, write_zone_complete, ctx, 0);
            if (rc != 0) {
                SPDK_ERRLOG("error while write_zone: %d\n", rc);
                spdk_nvme_ctrlr_free_io_qpair(ctx->qpair);
                spdk_app_stop(-1);
                return;
            }
        }
    }
}

static void reset_zone_complete(void *arg,
                                const struct spdk_nvme_cpl *completion)
{
    log_info("reset zone complete");
    struct rwtest_context_t *ctx = static_cast<struct rwtest_context_t *>(arg);

    if (spdk_nvme_cpl_is_error(completion)) {
        SPDK_ERRLOG("bdev io reset zone error\n");
        spdk_nvme_ctrlr_free_io_qpair(ctx->qpair);
        spdk_app_stop(-1);
        return;
    }
    ctx->count.fetch_sub(1);
    if (ctx->count.load() == 0) {
        SPDK_NOTICELOG("reset zone complete.\n");
        write_zone(ctx);
    }
}

static void reset_zone(void *arg)
{
    log_info("reset zone");
    struct rwtest_context_t *ctx = static_cast<struct rwtest_context_t *>(arg);
    int zone_num = 10;
    ctx->count = zone_num;
    uint64_t zone_size = spdk_nvme_ns_get_num_sectors(ctx->ns);
    for (uint64_t slba = 0; slba < zone_num * zone_size; slba += zone_size) {
        int rc = spdk_nvme_zns_reset_zone(ctx->ns, ctx->qpair, slba, true,
                                          reset_zone_complete, ctx);
        if (rc != 0) {
            SPDK_ERRLOG("error while resetting zone: %d\n", rc);
            spdk_nvme_ctrlr_free_io_qpair(ctx->qpair);
            spdk_app_stop(-1);
            return;
        }
    }
}

static void test_start(void *arg1)
{
    log_info("test start");
    struct rwtest_context_t *ctx = static_cast<struct rwtest_context_t *>(arg1);
    uint32_t buf_align;
    int rc = 0;

    struct spdk_nvme_transport_id trid = {};
    int nsid = 0;

    snprintf(trid.traddr, sizeof(trid.traddr), "%s", "192.168.1.121");
    snprintf(trid.trsvcid, sizeof(trid.trsvcid), "%s", "4420");
    // snprintf(trid.subnqn, sizeof(trid.subnqn), "%s",
    // SPDK_NVMF_DISCOVERY_NQN);
    snprintf(trid.subnqn, sizeof(trid.subnqn), "%s", g_hostnqn);
    trid.adrfam = SPDK_NVMF_ADRFAM_IPV4;
    trid.trtype = SPDK_NVME_TRANSPORT_TCP;
    log_info("nvme connect");

    struct spdk_nvme_ctrlr_opts opts;

    spdk_nvme_ctrlr_get_default_ctrlr_opts(&opts, sizeof(opts));
    memcpy(opts.hostnqn, g_hostnqn, sizeof(opts.hostnqn));
    ctx->ctrlr = spdk_nvme_connect(&trid, &opts, sizeof(opts));
    // ctx->ctrlr = spdk_nvme_connect(&trid, NULL, 0);

    if (ctx->ctrlr == NULL) {
        fprintf(stderr,
                "spdk_nvme_connect() failed for transport address '%s'\n",
                trid.traddr);
        spdk_app_stop(-1);
        // pthread_kill(g_fuzz_td, SIGSEGV);
        // return NULL;
        // return rc;
    }

    SPDK_NOTICELOG("Successfully started the application\n");
    SPDK_NOTICELOG("Initializing NVMe controller\n");

    if (spdk_nvme_zns_ctrlr_get_data(ctx->ctrlr)) {
        printf("ZNS Specific Controller Data\n");
        printf("============================\n");
        printf("Zone Append Size Limit:      %u\n",
               spdk_nvme_zns_ctrlr_get_data(ctx->ctrlr)->zasl);
        printf("\n");
        printf("\n");
    }

    printf("Active Namespaces\n");
    printf("=================\n");
    for (nsid = spdk_nvme_ctrlr_get_first_active_ns(ctx->ctrlr); nsid != 0;
         nsid = spdk_nvme_ctrlr_get_next_active_ns(ctx->ctrlr, nsid)) {
        print_namespace(ctx->ctrlr, spdk_nvme_ctrlr_get_ns(ctx->ctrlr, nsid));
    }

    ctx->ns = spdk_nvme_ctrlr_get_ns(ctx->ctrlr, 1);
    if (ctx->ns == NULL) {
        SPDK_ERRLOG("Could not get NVMe namespace\n");
        spdk_app_stop(-1);
        return;
    }

    ctx->qpair = spdk_nvme_ctrlr_alloc_io_qpair(ctx->ctrlr, NULL, 0);
    if (ctx->qpair == NULL) {
        SPDK_ERRLOG("Could not allocate IO queue pair\n");
        spdk_app_stop(-1);
        return;
    }

    ctx->buff_size = spdk_nvme_ns_get_sector_size(ctx->ns) *
                     spdk_nvme_ns_get_md_size(ctx->ns);
    buf_align = spdk_nvme_ns_get_optimal_io_boundary(ctx->ns);

    ctx->write_buff =
        static_cast<char *>(spdk_dma_zmalloc(ctx->buff_size, buf_align, NULL));
    ctx->write_buff = static_cast<char *>(
        spdk_zmalloc(ctx->buff_size, buf_align, NULL, SPDK_ENV_SOCKET_ID_ANY,
                     SPDK_MALLOC_DMA));

    if (!ctx->write_buff) {
        SPDK_ERRLOG("Failed to allocate buffer\n");
        spdk_nvme_ctrlr_free_io_qpair(ctx->qpair);
        spdk_app_stop(-1);
        return;
    }
    ctx->read_buff = static_cast<char *>(
        spdk_zmalloc(ctx->buff_size, buf_align, NULL, SPDK_ENV_SOCKET_ID_ANY,
                     SPDK_MALLOC_DMA));
    if (!ctx->read_buff) {
        SPDK_ERRLOG("Failed to allocate buffer\n");
        spdk_nvme_ctrlr_free_io_qpair(ctx->qpair);
        spdk_app_stop(-1);
        return;
    }

    SPDK_NOTICELOG("block size: %d, md size: %d, zone size: %lx, max open "
                   "zone: %d,max active zone: %d\n ",
                   spdk_nvme_ns_get_sector_size(ctx->ns),
                   spdk_nvme_ns_get_md_size(ctx->ns),
                   spdk_nvme_ns_get_num_sectors(ctx->ns),
                   spdk_nvme_zns_ns_get_max_open_zones(ctx->ns),
                   spdk_nvme_zns_ns_get_max_active_zones(ctx->ns));

    reset_zone(ctx);
}

int main(int argc, char **argv)
{
    struct spdk_app_opts opts = {};
    int rc = 0;
    // struct ZstoreContect ctx = {};
    struct rwtest_context_t ctx = {};

    spdk_app_opts_init(&opts, sizeof(opts));
    opts.name = "test_nvme";

    if ((rc = spdk_app_parse_args(argc, argv, &opts, NULL, NULL, NULL, NULL)) !=
        SPDK_APP_PARSE_ARGS_SUCCESS) {
        exit(rc);
    }
    // ctx.bdev_name = const_cast<char *>(g_bdev_name);
    log_info("HERE");
    rc = spdk_app_start(&opts, test_start, &ctx);
    if (rc) {
        SPDK_ERRLOG("ERROR starting application\n");
    }

    spdk_dma_free(ctx.write_buff);
    spdk_dma_free(ctx.read_buff);

    spdk_app_fini();

    return rc;
}
