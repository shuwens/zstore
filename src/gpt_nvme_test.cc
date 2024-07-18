#include "include/utils.hpp"
#include "spdk/env.h"
#include "spdk/event.h"
#include "spdk/log.h"
#include "spdk/nvme.h"
#include "spdk/stdinc.h"
#include "spdk/string.h"
#include "spdk/thread.h"

#include <atomic>

static const char *g_nvme_name = "Nvme1n2";

struct rwtest_context_t {
    struct spdk_nvme_ctrlr *ctrlr;
    struct spdk_nvme_ns *ns;
    struct spdk_nvme_qpair *qpair;
    char *write_buff;
    char *read_buff;
    uint32_t buff_size;
    char *nvme_name;
    struct spdk_nvme_io_qpair_opts qpair_opts;

    std::atomic<int> count; // atomic count for concurrency
};

static void read_complete(void *arg, const struct spdk_nvme_cpl *completion)
{
    struct rwtest_context_t *test_context =
        static_cast<struct rwtest_context_t *>(arg);

    if (spdk_nvme_cpl_is_error(completion)) {
        SPDK_ERRLOG("nvme io read error: %s\n",
                    spdk_nvme_cpl_get_status_string(&completion->status));
        spdk_nvme_ctrlr_free_io_qpair(test_context->qpair);
        spdk_app_stop(-1);
        return;
    }

    // compare read and write
    int cmp_res = memcmp(test_context->write_buff, test_context->read_buff,
                         test_context->buff_size);
    if (cmp_res != 0) {
        SPDK_ERRLOG("read data error.\n");
        spdk_nvme_ctrlr_free_io_qpair(test_context->qpair);
        spdk_app_stop(-1);
        return;
    }

    test_context->count.fetch_add(1);
    if (test_context->count.load() == 4 * 0x100) {
        SPDK_NOTICELOG("read complete.\n");
        spdk_nvme_ctrlr_free_io_qpair(test_context->qpair);
        spdk_app_stop(0);
        return;
    }

    memset(test_context->read_buff, 0x34, test_context->buff_size);
    uint64_t lba = test_context->count.load();

    int rc = spdk_nvme_ns_cmd_read(test_context->ns, test_context->qpair,
                                   test_context->read_buff, lba, 1,
                                   read_complete, test_context, 0);
    SPDK_NOTICELOG("read lba:0x%lx\n", lba);
    if (rc != 0) {
        SPDK_ERRLOG("error while reading from nvme: %d\n", rc);
        spdk_nvme_ctrlr_free_io_qpair(test_context->qpair);
        spdk_app_stop(-1);
        return;
    }
}

static void read(void *arg)
{
    struct rwtest_context_t *test_context =
        static_cast<struct rwtest_context_t *>(arg);
    test_context->count = 0;
    memset(test_context->read_buff, 0x34, test_context->buff_size);
    int rc = spdk_nvme_ns_cmd_read(test_context->ns, test_context->qpair,
                                   test_context->read_buff, 0, 1, read_complete,
                                   test_context, 0);
    SPDK_NOTICELOG("read lba:0x%x\n", 0x0);
    if (rc != 0) {
        SPDK_ERRLOG("error while reading from nvme: %d\n", rc);
        spdk_nvme_ctrlr_free_io_qpair(test_context->qpair);
        spdk_app_stop(-1);
    }
}

static void write_complete(void *arg, const struct spdk_nvme_cpl *completion)
{
    struct rwtest_context_t *test_context =
        static_cast<struct rwtest_context_t *>(arg);

    if (spdk_nvme_cpl_is_error(completion)) {
        SPDK_ERRLOG("nvme io write error: %s\n",
                    spdk_nvme_cpl_get_status_string(&completion->status));
        spdk_nvme_ctrlr_free_io_qpair(test_context->qpair);
        spdk_app_stop(-1);
        return;
    }

    test_context->count.fetch_sub(1);
    if (test_context->count.load() == 0) {
        SPDK_NOTICELOG("write complete.\n");
        read(test_context);
    }
}

static void write(void *arg)
{
    struct rwtest_context_t *test_context =
        static_cast<struct rwtest_context_t *>(arg);
    int append_times = 0x100;
    test_context->count = append_times;
    memset(test_context->write_buff, 0x12, test_context->buff_size);

    for (int i = 0; i < append_times; i++) {
        int rc = spdk_nvme_ns_cmd_write(test_context->ns, test_context->qpair,
                                        test_context->write_buff, i, 1,
                                        write_complete, test_context, 0);
        if (rc != 0) {
            SPDK_ERRLOG("error while write: %d\n", rc);
            spdk_nvme_ctrlr_free_io_qpair(test_context->qpair);
            spdk_app_stop(-1);
        }
    }
}

static void reset_complete(void *arg, const struct spdk_nvme_cpl *completion)
{
    struct rwtest_context_t *test_context =
        static_cast<struct rwtest_context_t *>(arg);

    if (spdk_nvme_cpl_is_error(completion)) {
        SPDK_ERRLOG("nvme io reset error: %s\n",
                    spdk_nvme_cpl_get_status_string(&completion->status));
        spdk_nvme_ctrlr_free_io_qpair(test_context->qpair);
        spdk_app_stop(-1);
        return;
    }

    test_context->count.fetch_sub(1);
    if (test_context->count.load() == 0) {
        SPDK_NOTICELOG("reset complete.\n");
        write(test_context);
    }
}

static void reset(void *arg)
{
    struct rwtest_context_t *test_context =
        static_cast<struct rwtest_context_t *>(arg);
    int zone_num = 10;
    test_context->count = zone_num;

    for (int i = 0; i < zone_num; i++) {
        int rc = spdk_nvme_ns_cmd_zone_management(
            test_context->ns, test_context->qpair,
            SPDK_NVME_ZONE_MANAGEMENT_SEND, SPDK_NVME_ZONE_RESET, i,
            reset_complete, test_context);
        if (rc != 0) {
            SPDK_ERRLOG("error while resetting zone: %d\n", rc);
            spdk_nvme_ctrlr_free_io_qpair(test_context->qpair);
            spdk_app_stop(-1);
        }
    }
}

static void test_nvme_event_cb(void *arg, const struct spdk_nvme_cpl *cpl)
{
    SPDK_NOTICELOG("Unsupported nvme event: %s\n",
                   spdk_nvme_cpl_get_status_string(&cpl->status));
}

static void test_start(void *arg1)
{
    struct rwtest_context_t *test_context =
        static_cast<struct rwtest_context_t *>(arg1);
    uint32_t buf_align;
    int rc = 0;
    test_context->ctrlr = NULL;
    test_context->ns = NULL;

    SPDK_NOTICELOG("Successfully started the application\n");

    test_context->ctrlr = spdk_nvme_connect(NULL, NULL, 0);
    if (test_context->ctrlr == NULL) {
        SPDK_ERRLOG("Could not connect to NVMe controller: %s\n",
                    test_context->nvme_name);
        spdk_app_stop(-1);
        return;
    }

    test_context->ns = spdk_nvme_ctrlr_get_ns(test_context->ctrlr, 1);
    if (test_context->ns == NULL) {
        SPDK_ERRLOG("Could not get namespace\n");
        spdk_nvme_detach(test_context->ctrlr);
        spdk_app_stop(-1);
        return;
    }

    test_context->qpair = spdk_nvme_ctrlr_alloc_io_qpair(
        test_context->ctrlr, &test_context->qpair_opts, 0);
    if (test_context->qpair == NULL) {
        SPDK_ERRLOG("Could not allocate I/O qpair\n");
        spdk_nvme_detach(test_context->ctrlr);
        spdk_app_stop(-1);
        return;
    }

    test_context->buff_size = spdk_nvme_ns_get_sector_size(test_context->ns);
    buf_align = spdk_nvme_ns_get_sector_size(test_context->ns);
    // log_info("buffer size: %d", test_context->buff_size);
    // log_info("buffer align: %lu", buf_align);
    test_context->write_buff = static_cast<char *>(
        spdk_dma_zmalloc(test_context->buff_size, buf_align, NULL));
    if (!test_context->write_buff) {
        SPDK_ERRLOG("Failed to allocate buffer\n");
        spdk_nvme_ctrlr_free_io_qpair(test_context->qpair);
        spdk_nvme_detach(test_context->ctrlr);
        spdk_app_stop(-1);
        return;
    }
    test_context->read_buff = static_cast<char *>(
        spdk_dma_zmalloc(test_context->buff_size, buf_align, NULL));
    if (!test_context->read_buff) {
        SPDK_ERRLOG("Failed to allocate buffer\n");
        spdk_nvme_ctrlr_free_io_qpair(test_context->qpair);
        spdk_nvme_detach(test_context->ctrlr);
        spdk_app_stop(-1);
        return;
    }

    if (!spdk_nvme_ns_is_zoned(test_context->ns)) {
        SPDK_ERRLOG("not a ZNS SSD\n");
        spdk_nvme_ctrlr_free_io_qpair(test_context->qpair);
        spdk_nvme_detach(test_context->ctrlr);
        spdk_app_stop(-1);
        return;
    }

    SPDK_NOTICELOG("sector size:%d zone size:%lx\n",
                   spdk_nvme_ns_get_sector_size(test_context->ns),
                   spdk_nvme_ns_get_size(test_context->ns));
    reset(test_context);
}

int main(int argc, char **argv)
{
    struct spdk_app_opts opts = {};
    int rc = 0;
    struct rwtest_context_t test_context = {};

    spdk_app_opts_init(&opts, sizeof(opts));
    opts.name = "test_nvme";

    if ((rc = spdk_app_parse_args(argc, argv, &opts, NULL, NULL, NULL, NULL)) !=
        SPDK_APP_PARSE_ARGS_SUCCESS) {
        exit(rc);
    }
    test_context.nvme_name = const_cast<char *>(g_nvme_name);

    rc = spdk_app_start(&opts, test_start, &test_context);
    if (rc) {
        SPDK_ERRLOG("ERROR starting application\n");
    }

    spdk_dma_free(test_context.write_buff);
    spdk_dma_free(test_context.read_buff);

    spdk_app_fini();
    return rc;
}
