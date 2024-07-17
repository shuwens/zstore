#include "include/utils.hpp"
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
    struct rwtest_context_t *test_context =
        static_cast<struct rwtest_context_t *>(arg);

    if (spdk_nvme_cpl_is_error(completion)) {
        SPDK_ERRLOG("bdev io read zone error\n");
        spdk_nvme_ctrlr_free_io_qpair(test_context->qpair);
        spdk_app_stop(-1);
        return;
    }

    // compare read and write
    int cmp_res = memcmp(test_context->write_buff, test_context->read_buff,
                         test_context->buff_size);
    if (cmp_res != 0) {
        SPDK_ERRLOG("read zone data error.\n");
        spdk_nvme_ctrlr_free_io_qpair(test_context->qpair);
        spdk_app_stop(-1);
        return;
    }
    test_context->count.fetch_add(1);
    if (test_context->count.load() == 4 * 0x100) {
        SPDK_NOTICELOG("read zone complete.\n");
        spdk_nvme_ctrlr_free_io_qpair(test_context->qpair);
        spdk_app_stop(0);
        return;
    }

    memset(test_context->read_buff, 0x34, test_context->buff_size);
    uint64_t lba = test_context->count.load() / 0x100 *
                       spdk_nvme_ns_get_num_sectors(test_context->ns) +
                   test_context->count.load() % 0x100;

    int rc = spdk_nvme_ns_cmd_read(test_context->ns, test_context->qpair,
                                   test_context->read_buff, lba, 1,
                                   read_zone_complete, test_context, 0);
    SPDK_NOTICELOG("read lba:0x%lx\n", lba);
    if (rc != 0) {
        SPDK_ERRLOG("error while reading from bdev: %d\n", rc);
        spdk_nvme_ctrlr_free_io_qpair(test_context->qpair);
        spdk_app_stop(-1);
        return;
    }
}

static void read_zone(void *arg)
{
    struct rwtest_context_t *test_context =
        static_cast<struct rwtest_context_t *>(arg);
    test_context->count = 0;
    memset(test_context->read_buff, 0x34, test_context->buff_size);
    int rc = spdk_nvme_ns_cmd_read(test_context->ns, test_context->qpair,
                                   test_context->read_buff, 0, 1,
                                   read_zone_complete, test_context, 0);
    SPDK_NOTICELOG("read lba:0x%x\n", 0x0);
    if (rc) {
        SPDK_ERRLOG("error while reading from bdev: %d\n", rc);
        spdk_nvme_ctrlr_free_io_qpair(test_context->qpair);
        spdk_app_stop(-1);
    }
}

static void write_zone_complete(void *arg,
                                const struct spdk_nvme_cpl *completion)
{
    struct rwtest_context_t *test_context =
        static_cast<struct rwtest_context_t *>(arg);

    if (spdk_nvme_cpl_is_error(completion)) {
        SPDK_ERRLOG("bdev io write zone error\n");
        spdk_nvme_ctrlr_free_io_qpair(test_context->qpair);
        spdk_app_stop(-1);
        return;
    }
    test_context->count.fetch_sub(1);
    if (test_context->count.load() == 0) {
        SPDK_NOTICELOG("write zone complete.\n");
        read_zone(test_context);
    }
}

static void write_zone(void *arg)
{
    struct rwtest_context_t *test_context =
        static_cast<struct rwtest_context_t *>(arg);
    uint64_t zone_size = spdk_nvme_ns_get_num_sectors(test_context->ns);
    int zone_num = 4;
    int append_times = 0x100;
    test_context->count = zone_num * append_times;
    memset(test_context->write_buff, 0x12, test_context->buff_size);
    for (uint64_t slba = 0; slba < zone_num * zone_size; slba += zone_size) {
        for (int i = 0; i < append_times; i++) {
            int rc = spdk_nvme_ns_cmd_write(
                test_context->ns, test_context->qpair, test_context->write_buff,
                slba, 1, write_zone_complete, test_context, 0);
            if (rc != 0) {
                SPDK_ERRLOG("error while write_zone: %d\n", rc);
                spdk_nvme_ctrlr_free_io_qpair(test_context->qpair);
                spdk_app_stop(-1);
                return;
            }
        }
    }
}

static void reset_zone_complete(void *arg,
                                const struct spdk_nvme_cpl *completion)
{
    struct rwtest_context_t *test_context =
        static_cast<struct rwtest_context_t *>(arg);

    if (spdk_nvme_cpl_is_error(completion)) {
        SPDK_ERRLOG("bdev io reset zone error\n");
        spdk_nvme_ctrlr_free_io_qpair(test_context->qpair);
        spdk_app_stop(-1);
        return;
    }
    test_context->count.fetch_sub(1);
    if (test_context->count.load() == 0) {
        SPDK_NOTICELOG("reset zone complete.\n");
        write_zone(test_context);
    }
}

static void reset_zone(void *arg)
{
    struct rwtest_context_t *test_context =
        static_cast<struct rwtest_context_t *>(arg);
    int zone_num = 10;
    test_context->count = zone_num;
    uint64_t zone_size = spdk_nvme_ns_get_num_sectors(test_context->ns);
    for (uint64_t slba = 0; slba < zone_num * zone_size; slba += zone_size) {
        int rc = spdk_nvme_zns_reset_zone(test_context->ns, test_context->qpair,
                                          slba, true, reset_zone_complete,
                                          test_context);
        if (rc != 0) {
            SPDK_ERRLOG("error while resetting zone: %d\n", rc);
            spdk_nvme_ctrlr_free_io_qpair(test_context->qpair);
            spdk_app_stop(-1);
            return;
        }
    }
}

static void test_start(void *arg1)
{
    log_info("test start");
    struct rwtest_context_t *test_context =
        static_cast<struct rwtest_context_t *>(arg1);
    uint32_t buf_align;
    int rc = 0;

    struct spdk_nvme_transport_id trid = {};
    int nsid = 0;

    snprintf(trid.traddr, sizeof(trid.traddr), "%s", "192.168.1.121");
    snprintf(trid.trsvcid, sizeof(trid.trsvcid), "%s", "4420");
    snprintf(trid.subnqn, sizeof(trid.subnqn), "%s", SPDK_NVMF_DISCOVERY_NQN);
    trid.adrfam = SPDK_NVMF_ADRFAM_IPV4;
    trid.trtype = SPDK_NVME_TRANSPORT_TCP;
    log_info("nvme connect");
    test_context->ctrlr = spdk_nvme_connect(&trid, NULL, 0);
    if (test_context->ctrlr == NULL) {
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
    // const char *traddr = "0000:05:00.0";
    // rc = spdk_nvme_probe(traddr, test_context, probe_cb, attach_cb, NULL);
    // if (rc) {
    //     SPDK_ERRLOG("Could not initialize NVMe controller\n");
    //     spdk_app_stop(-1);
    //     return;
    // }

    // test_context->ctrlr = spdk_nvme_ctrlr_get_first();
    // if (test_context->ctrlr == NULL) {
    //     SPDK_ERRLOG("No NVMe controller found\n");
    //     spdk_app_stop(-1);
    //     return;
    // }

    //     test_context->ns = spdk_nvme_ctrlr_get_ns(test_context->ctrlr, 1);
    //     if (test_context->ns == NULL) {
    //         SPDK_ERRLOG("Could not get NVMe namespace\n");
    //         spdk_app_stop(-1);
    //         return;
    //     }

    //     test_context->qpair =
    //         spdk_nvme_ctrlr_alloc_io_qpair(test_context->ctrlr, NULL, 0);
    //     if (test_context->qpair == NULL) {
    //         SPDK_ERRLOG("Could not allocate IO queue pair\n");
    //         spdk_app_stop(-1);
    //         return;
    //     }

    //     test_context->buff_size =
    //     spdk_nvme_ns_get_sector_size(test_context->ns) *
    //                               spdk_nvme_ns_get_md_size(test_context->ns);
    //     buf_align = spdk_nvme_ns_get_optimal_io_boundary(test_context->ns);

    //     // test_context->write_buff = static_cast<char *>(
    //     //     spdk_dma_zmalloc(test_context->buff_size, buf_align, NULL));
    //     test_context->write_buff = static_cast<char *>(
    //         spdk_zmalloc(test_context->buff_size, buf_align, NULL,
    //                      SPDK_ENV_SOCKET_ID_ANY, SPDK_MALLOC_DMA));

    //     if (!test_context->write_buff) {
    //         SPDK_ERRLOG("Failed to allocate buffer\n");
    //         spdk_nvme_ctrlr_free_io_qpair(test_context->qpair);
    //         spdk_app_stop(-1);
    //         return;
    //     }
    //     test_context->read_buff = static_cast<char *>(
    //         spdk_zmalloc(test_context->buff_size, buf_align, NULL,
    //                      SPDK_ENV_SOCKET_ID_ANY, SPDK_MALLOC_DMA));
    //     if (!test_context->read_buff) {
    //         SPDK_ERRLOG("Failed to allocate buffer\n");
    //         spdk_nvme_ctrlr_free_io_qpair(test_context->qpair);
    //         spdk_app_stop(-1);
    //         return;
    //     }

    //     SPDK_NOTICELOG(
    //         "block size: %d, md size: %d, zone size: %lx, max open zone: %d,
    //         max " "active zone: %d\n",
    //         spdk_nvme_ns_get_sector_size(test_context->ns),
    //         spdk_nvme_ns_get_md_size(test_context->ns),
    //         spdk_nvme_ns_get_num_sectors(test_context->ns),
    //         spdk_nvme_zns_ns_get_max_open_zones(test_context->ns),
    //         spdk_nvme_zns_ns_get_max_active_zones(test_context->ns));

    //     reset_zone(test_context);
}

int main(int argc, char **argv)
{
    struct spdk_app_opts opts = {};
    int rc = 0;
    // struct ZstoreContect ctx = {};
    struct rwtest_context_t test_context = {};

    spdk_app_opts_init(&opts, sizeof(opts));
    opts.name = "test_nvme";

    if ((rc = spdk_app_parse_args(argc, argv, &opts, NULL, NULL, NULL, NULL)) !=
        SPDK_APP_PARSE_ARGS_SUCCESS) {
        exit(rc);
    }
    // test_context.bdev_name = const_cast<char *>(g_bdev_name);
    log_info("HERE");
    rc = spdk_app_start(&opts, test_start, &test_context);
    if (rc) {
        SPDK_ERRLOG("ERROR starting application\n");
    }

    spdk_dma_free(test_context.write_buff);
    spdk_dma_free(test_context.read_buff);

    spdk_app_fini();

    //     // Example buffer to write
    //     char write_buffer[4096];
    //     memset(write_buffer, 0x5A, sizeof(write_buffer)); // Fill buffer
    //     with data

    //     // Zone append example
    //     uint64_t zslba = 0; // Starting LBA of the zone (adjust as
    //     needed) if (spdk_nvme_zns_zone_append(ctx->ns, ctx->qpair,
    //     write_buffer, zslba,
    //                                   sizeof(write_buffer) / 512, NULL,
    //                                   NULL)) {
    //         fprintf(stderr, "Zone append command failed\n");
    //         spdk_nvme_ctrlr_free_io_qpair(ctx->qpair);
    //         spdk_nvme_detach(ctx->ctrlr);
    //         spdk_env_fini();
    //         return 1;
    //     }

    //     // Wait for completion (you might want to implement a proper
    //     completion
    //     // callback)
    //     spdk_nvme_qpair_process_completions(ctx->qpair, 0);

    //     // Clean up
    //     spdk_nvme_ctrlr_free_io_qpair(ctx->qpair);
    //     spdk_nvme_detach(ctx->ctrlr);
    //     spdk_env_fini();

    // if (spdk_nvme_probe(&trid, NULL, probe_cb, attach_cb, NULL)) {
    //     fprintf(stderr, "spdk_nvme_probe() failed\n");
    //     return 1;
    // }

    // if ((rc = spdk_app_parse_args(argc, argv, &opts, NULL, NULL, NULL,
    // NULL))
    // !=
    //     SPDK_APP_PARSE_ARGS_SUCCESS) {
    //     exit(rc);
    // }
    // test_context.bdev_name = const_cast<char *>(g_bdev_name);

    // rc = spdk_app_start(&opts, test_start, &test_context);
    // if (rc) {
    //     SPDK_ERRLOG("ERROR starting application\n");
    // }
    //
    // spdk_dma_free(test_context.write_buff);
    // spdk_dma_free(test_context.read_buff);
    //
    // spdk_app_fini();
    return rc;
}
