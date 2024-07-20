#include "spdk/env.h"
#include "spdk/event.h"
#include "spdk/log.h"
#include "spdk/nvme.h"
#include "spdk/nvme_zns.h"
#include "spdk/nvmf_spec.h"
#include "utils.hpp"
#include "zns_utils.h"
#include <atomic>

// static const char *g_bdev_name = "Nvme1n2";
static const char *g_hostnqn = "nqn.2024-04.io.zstore:cnode1";

struct ZstoreContext {
    // spdk things
    struct spdk_nvme_ctrlr *ctrlr = nullptr;
    struct spdk_nvme_ns *ns = nullptr;
    struct spdk_nvme_qpair *qpair = nullptr;
    char *write_buff = nullptr;
    char *read_buff = nullptr;
    uint32_t buff_size;
    // char *bdev_name;

    // device related
    bool device_support_meta = true;

    bool done = false;
    u64 num_queued = 0;
    u64 num_completed = 0;
    u64 num_success = 0;
    u64 num_fail = 0;

    u64 total_us = 0;

    std::atomic<int> count; // atomic count for concurrency
};

inline void spin_complete(struct ZstoreContext *ctx)
{
    while (spdk_nvme_qpair_process_completions(ctx->qpair, 0) == 0) {
        ;
    }
}

void complete(void *arg, const struct spdk_nvme_cpl *completion)
{
    bool *done = (bool *)arg;
    *done = true;

    if (spdk_nvme_cpl_is_error(completion)) {
        fprintf(stderr, "I/O error status: %s\n",
                spdk_nvme_cpl_get_status_string(&completion->status));
        fprintf(stderr, "I/O failed, aborting run\n");
        assert(0);
    }
}

// static void test_nvme_event_cb(void *arg, const struct spdk_nvme_cpl *cpl)
// {
//     SPDK_NOTICELOG("Unsupported nvme event: %s\n",
//                    spdk_nvme_cpl_get_status_string(&cpl->status));
// }

// TODO: unused code
static void unused_zns_dev_init(void *arg)
{
    int ret = 0;
    struct spdk_env_opts opts;
    spdk_env_opts_init(&opts);
    opts.core_mask = "0x1fc";
    if (spdk_env_init(&opts) < 0) {
        fprintf(stderr, "Unable to initialize SPDK env.\n");
        exit(-1);
    }

    ret = spdk_thread_lib_init(nullptr, 0);
    if (ret < 0) {
        fprintf(stderr, "Unable to initialize SPDK thread lib.\n");
        exit(-1);
    }
    // ret = spdk_nvme_probe(NULL, NULL, probe_cb, attach_cb, NULL);
    if (ret < 0) {
        fprintf(stderr, "Unable to probe devices\n");
        exit(-1);
    }
}

static void zns_dev_init(void *arg)
{
    struct ZstoreContext *ctx = static_cast<struct ZstoreContext *>(arg);
    int rc = 0;
    ctx->ctrlr = NULL;
    ctx->ns = NULL;

    SPDK_NOTICELOG("Successfully started the application\n");

    // 1. connect nvmf device
    struct spdk_nvme_transport_id trid = {};
    int nsid = 0;

    snprintf(trid.traddr, sizeof(trid.traddr), "%s", "192.168.1.121");
    snprintf(trid.trsvcid, sizeof(trid.trsvcid), "%s", "4420");
    // snprintf(trid.subnqn, sizeof(trid.subnqn), "%s",
    // SPDK_NVMF_DISCOVERY_NQN);
    snprintf(trid.subnqn, sizeof(trid.subnqn), "%s", g_hostnqn);
    trid.adrfam = SPDK_NVMF_ADRFAM_IPV4;
    trid.trtype = SPDK_NVME_TRANSPORT_TCP;

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

    // 2. creating qpairs
    struct spdk_nvme_io_qpair_opts qpair_opts;
    spdk_nvme_ctrlr_get_default_io_qpair_opts(ctx->ctrlr, &qpair_opts,
                                              sizeof(qpair_opts));
    qpair_opts.delay_cmd_submit = true;
    qpair_opts.create_only = true;
    ctx->qpair = spdk_nvme_ctrlr_alloc_io_qpair(ctx->ctrlr, &qpair_opts,
                                                sizeof(qpair_opts));
    if (ctx->qpair == NULL) {
        SPDK_ERRLOG("Could not allocate IO queue pair\n");
        spdk_app_stop(-1);
        return;
    }

    // 3. connect qpair
    rc = spdk_nvme_ctrlr_connect_io_qpair(ctx->ctrlr, ctx->qpair);
    if (rc) {
        log_error("Could not connect IO queue pair\n");
        spdk_app_stop(-1);
        return;
    }
}

// TODO: make ZNS device class and put this in Init() or ctor
static void zstore_init(void *arg)
{
    struct ZstoreContext *ctx = static_cast<struct ZstoreContext *>(arg);
    if (spdk_nvme_ns_get_md_size(ctx->ns) == 0) {
        ctx->device_support_meta = false;
    }

    // Adjust the capacity for user data = total capacity - footer size
    // The L2P table information at the end of the segment
    // Each block needs (LBA + timestamp + stripe ID, 20 bytes) for L2P table
    // recovery; we round the number to block size
    auto zone_size = spdk_nvme_zns_ns_get_zone_size_sectors(ctx->ns);
    u32 num_zones = spdk_nvme_zns_ns_get_num_zones(ctx->ns);
    u64 zone_capacity = 0;
    if (zone_size == 2ull * 1024 * 256) {
        zone_capacity = 1077 * 256; // hard-coded here since it is ZN540;
                                    // update this for emulated SSDs
    } else {
        zone_capacity = zone_size;
    }
    log_info("Zone size: {}, zone cap: {}, num of zones: {}\n", zone_size,
             zone_capacity, num_zones);

    uint32_t blockSize = 4096;
    uint64_t storageSpace = 1024 * 1024 * 1024 * 1024ull;
    auto mMappingBlockUnitSize = blockSize * blockSize / 4;
}
