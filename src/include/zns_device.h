#include "spdk/env.h"
#include "spdk/event.h"
#include "spdk/log.h"
#include "spdk/nvme.h"
#include "spdk/nvme_zns.h"
#include "spdk/nvmf_spec.h"
#include "utils.hpp"
#include "zns_utils.h"
#include <atomic>
#include <chrono>
#include <fstream>
using chrono_tp = std::chrono::high_resolution_clock::time_point;

// static const char *g_bdev_name = "Nvme1n2";
static const char *g_hostnqn = "nqn.2024-04.io.zstore:cnode1";
const int zone_num = 1;

const int append_times = 3;
// const int append_times = 1000;
// const int append_times = 16000;

const int value = 10000000; // Integer value to set in the buffer

typedef struct {
    uint64_t lba_size;
    uint64_t zone_size;
    uint64_t zone_cap;
    uint64_t mdts;
    uint64_t zasl;
    uint64_t lba_cap;
} DeviceInfo;

typedef struct {
    struct spdk_nvme_transport_id g_trid = {};
    struct spdk_nvme_ctrlr *ctrlr;
    spdk_nvme_ns *ns;
    DeviceInfo info = {};
} DeviceManager;

struct ZstoreContext {
    // specific parameters we control
    int qd = 0;
    u64 current_zone;
    bool verbose = false;

    // tmp values that matters in the run
    u64 current_lba = 0;
    std::vector<uint32_t> append_lbas;

    // --------------------------------------

    // spdk things we populate
    struct spdk_nvme_ctrlr *ctrlr = nullptr;
    struct spdk_nvme_transport_id g_trid = {};
    struct spdk_nvme_ns *ns = nullptr;
    struct spdk_nvme_qpair *qpair = nullptr;
    char *write_buff = nullptr;
    char *read_buff = nullptr;
    uint32_t buff_size;

    // device related
    bool device_support_meta = true;
    DeviceInfo info;
    u64 zslba;

    bool done = false;
    u64 num_queued = 0;
    u64 num_completed = 0;
    u64 num_success = 0;
    u64 num_fail = 0;

    u64 total_us = 0;

    bool zstore_open = false;

    std::atomic<int> count; // atomic count for concurrency
};

typedef struct {
    ZstoreContext *ctx;
    const char *traddr;
    const u_int8_t traddr_len;
    bool found;
} DeviceProber;

typedef struct {
} Zone;

typedef struct {
    bool done = false;
    int err = 0;
} Completion;
// Create 1 QPair for each thread that uses I/O.
// typedef struct {
//     spdk_nvme_qpair *qpair;
//     DeviceManager *man;
// } QPair;

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

static void zns_dev_init(void *arg)
{
    struct ZstoreContext *ctx = static_cast<struct ZstoreContext *>(arg);
    int rc = 0;
    ctx->ctrlr = NULL;
    ctx->ns = NULL;

    if (ctx->verbose)
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

    if (ctx->ctrlr == NULL && ctx->verbose) {
        fprintf(stderr,
                "spdk_nvme_connect() failed for transport address '%s'\n",
                trid.traddr);
        spdk_app_stop(-1);
        // pthread_kill(g_fuzz_td, SIGSEGV);
        // return NULL;
        // return rc;
    }

    // SPDK_NOTICELOG("Successfully started the application\n");
    // SPDK_NOTICELOG("Initializing NVMe controller\n");

    if (spdk_nvme_zns_ctrlr_get_data(ctx->ctrlr) && ctx->verbose) {
        printf("ZNS Specific Controller Data\n");
        printf("============================\n");
        printf("Zone Append Size Limit:      %u\n",
               spdk_nvme_zns_ctrlr_get_data(ctx->ctrlr)->zasl);
        printf("\n");
        printf("\n");

        printf("Active Namespaces\n");
        printf("=================\n");
        // for (nsid = spdk_nvme_ctrlr_get_first_active_ns(ctx->ctrlr); nsid !=
        // 0;
        //      nsid = spdk_nvme_ctrlr_get_next_active_ns(ctx->ctrlr, nsid)) {
        //     print_namespace(ctx->ctrlr,
        //                     spdk_nvme_ctrlr_get_ns(ctx->ctrlr, nsid));
        // }
    }
    // ctx->ns = spdk_nvme_ctrlr_get_ns(ctx->ctrlr, 1);

    // NOTE: must find zns ns
    // take any ZNS namespace, we do not care which.
    for (int nsid = spdk_nvme_ctrlr_get_first_active_ns(ctx->ctrlr); nsid != 0;
         nsid = spdk_nvme_ctrlr_get_next_active_ns(ctx->ctrlr, nsid)) {
        if (ctx->verbose)
            log_info("ns id: {}", nsid);

        struct spdk_nvme_ns *ns = spdk_nvme_ctrlr_get_ns(ctx->ctrlr, nsid);
        if (ns == NULL) {
            continue;
        }
        if (spdk_nvme_ns_get_csi(ns) != SPDK_NVME_CSI_ZNS) {
            continue;
        }
        ctx->ns = ns;

        if (ctx->verbose)
            print_namespace(ctx->ctrlr,
                            spdk_nvme_ctrlr_get_ns(ctx->ctrlr, nsid),
                            ctx->current_zone);

        break;
    }

    if (ctx->ns == NULL) {
        SPDK_ERRLOG("Could not get NVMe namespace\n");
        spdk_app_stop(-1);
        return;
    }
}

static void zstore_qpair_setup(void *arg, spdk_nvme_io_qpair_opts qpair_opts)
{
    struct ZstoreContext *ctx = static_cast<struct ZstoreContext *>(arg);
    int rc = 0;
    // 2. creating qpairs
    // struct spdk_nvme_io_qpair_opts qpair_opts;
    // spdk_nvme_ctrlr_get_default_io_qpair_opts(ctx->ctrlr, &qpair_opts,
    //                                           sizeof(qpair_opts));
    // qpair_opts.delay_cmd_submit = true;
    // qpair_opts.create_only = true;
    log_info("alloc qpair of queue size {}, request size {}",
             qpair_opts.io_queue_size, qpair_opts.io_queue_requests);
    ctx->qpair = spdk_nvme_ctrlr_alloc_io_qpair(ctx->ctrlr, &qpair_opts,
                                                sizeof(qpair_opts));
    // ctx->qpair = spdk_nvme_ctrlr_alloc_io_qpair(ctx->ctrlr, NULL, 0);

    if (ctx->qpair == NULL) {
        SPDK_ERRLOG("Could not allocate IO queue pair\n");
        spdk_app_stop(-1);
        return;
    }

    // 3. connect qpair
    // rc = spdk_nvme_ctrlr_connect_io_qpair(ctx->ctrlr, ctx->qpair);
    // if (rc) {
    //     log_error("Could not connect IO queue pair\n");
    //     spdk_app_stop(-1);
    //     return;
    // }
}

static void zstore_qpair_teardown(void *arg)
{
    struct ZstoreContext *ctx = static_cast<struct ZstoreContext *>(arg);

    // if (ctx->verbose)
    //     log_info("complete queued items {}, qd {}", ctx->num_queued,
    //     ctx->qd);
    // spdk_nvme_qpair_process_completions(ctx->qpair, 0);

    log_info("disconnect and free qpair");
    spdk_nvme_ctrlr_disconnect_io_qpair(ctx->qpair);
    spdk_nvme_ctrlr_free_io_qpair(ctx->qpair);
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
    ctx->info.zone_cap = zone_capacity;
    if (ctx->verbose)
        log_info("Zone size: {}, zone cap: {}, num of zones: {}\n", zone_size,
                 zone_capacity, num_zones);

    uint32_t blockSize = 4096;
    uint64_t storageSpace = 1024 * 1024 * 1024 * 1024ull;
    auto mMappingBlockUnitSize = blockSize * blockSize / 4;
    if (ctx->verbose)
        log_info(
            "block size: {}, storage space: {}, mapping block unit size: {}\n",
            blockSize, storageSpace, mMappingBlockUnitSize);
}

// TropoDB

#include "spdk/endian.h"
#include "spdk/env.h"
#include "spdk/log.h"
#include "spdk/nvme.h"
#include "spdk/nvme_intel.h"
#include "spdk/nvme_ocssd.h"
#include "spdk/nvme_zns.h"
#include "spdk/nvmf_spec.h"
#include "spdk/pci_ids.h"
#include "spdk/stdinc.h"
#include "spdk/string.h"
#include "spdk/util.h"
#include "spdk/uuid.h"
#include "spdk/vmd.h"

extern "C" {
#define ERROR_ON_NULL(x, err)                                                  \
    do {                                                                       \
        if ((x) == nullptr) {                                                  \
            return (err);                                                      \
        }                                                                      \
    } while (0)

#define POLL_QPAIR(qpair, target)                                              \
    do {                                                                       \
        while (!(target)) {                                                    \
            spdk_nvme_qpair_process_completions((qpair), 0);                   \
        }                                                                      \
    } while (0)
}

// typedef struct {
//     bool done = false;
//     int err = 0;
// } Completion;
//
// typedef struct {
//     uint64_t lba_size;
//     uint64_t zone_size;
//     uint64_t mdts;
//     uint64_t zasl;
//     uint64_t lba_cap;
// } DeviceInfo;

static void __operation_complete2(void *arg,
                                  const struct spdk_nvme_cpl *completion)
{
    Completion *completed = (Completion *)arg;
    completed->done = true;
    if (spdk_nvme_cpl_is_error(completion)) {
        completed->err = 1;
        return;
    }
    // SPDK_NOTICELOG("append slba:0x%016x\n", completion->cdw0);
}

static void __append_complete2(void *arg,
                               const struct spdk_nvme_cpl *completion)
{
    Completion *completed = (Completion *)arg;
    completed->done = true;
    if (spdk_nvme_cpl_is_error(completion)) {
        completed->err = 1;
        return;
    }
    // SPDK_NOTICELOG("append slba:0x%lx\n", completion->cdw0);
}

static void __read_complete2(void *arg, const struct spdk_nvme_cpl *completion)
{
    Completion *completed = (Completion *)arg;
    completed->done = true;
    if (spdk_nvme_cpl_is_error(completion)) {
        completed->err = 1;
        return;
    }
}

static void __operation_complete(void *arg,
                                 const struct spdk_nvme_cpl *completion)
{
    Completion *completed = (Completion *)arg;
    completed->done = true;
    if (spdk_nvme_cpl_is_error(completion)) {
        completed->err = 1;
        return;
    }
}

static void __append_complete(void *arg, const struct spdk_nvme_cpl *completion)
{
    __operation_complete(arg, completion);
}

static void __read_complete(void *arg, const struct spdk_nvme_cpl *completion)
{
    __operation_complete(arg, completion);
}

static void __reset_zone_complete(void *arg,
                                  const struct spdk_nvme_cpl *completion)
{
    __operation_complete(arg, completion);
}

static void __get_zone_head_complete(void *arg,
                                     const struct spdk_nvme_cpl *completion)
{
    __operation_complete(arg, completion);
}

int z_reset(void *arg)
{
    log_info("z_reset");
    struct ZstoreContext *ctx = static_cast<struct ZstoreContext *>(arg);
    ERROR_ON_NULL(ctx->qpair, 1);
    Completion completion = {.done = false};
    int rc = spdk_nvme_zns_reset_zone(ctx->ns, ctx->qpair,
                                      0, /* starting LBA of the zone to reset */
                                      true, /* don't reset all zones */
                                      __reset_zone_complete, &completion);
    if (rc != 0)
        return rc;
    POLL_QPAIR(ctx->qpair, completion.done);
    return rc;
}

int z_reset(void *arg, uint64_t slba, bool all)
{
    struct ZstoreContext *ctx = static_cast<struct ZstoreContext *>(arg);
    ERROR_ON_NULL(ctx->qpair, 1);
    Completion completion = {.done = false};
    int rc = spdk_nvme_zns_reset_zone(
        ctx->ns, ctx->qpair, slba, /* starting LBA of the zone to reset */
        all,                       /* don't reset all zones */
        __reset_zone_complete, &completion);
    if (rc != 0)
        return rc;
    POLL_QPAIR(ctx->qpair, completion.done);
    return rc;
}

int z_get_device_info(void *arg)
{
    struct ZstoreContext *ctx = static_cast<struct ZstoreContext *>(arg);
    // ERROR_ON_NULL(info, 1);
    // ERROR_ON_NULL(manager, 1);
    ERROR_ON_NULL(ctx->ctrlr, 1);
    ERROR_ON_NULL(ctx->ns, 1);

    u64 zone_dist = 0x80000; // zone size
    ctx->zslba = zone_dist * ctx->current_zone;

    const struct spdk_nvme_ns_data *ns_data = spdk_nvme_ns_get_data(ctx->ns);
    const struct spdk_nvme_zns_ns_data *ns_data_zns =
        spdk_nvme_zns_ns_get_data(ctx->ns);
    const struct spdk_nvme_ctrlr_data *ctrlr_data =
        spdk_nvme_ctrlr_get_data(ctx->ctrlr);
    const spdk_nvme_zns_ctrlr_data *ctrlr_data_zns =
        spdk_nvme_zns_ctrlr_get_data(ctx->ctrlr);
    union spdk_nvme_cap_register cap = spdk_nvme_ctrlr_get_regs_cap(ctx->ctrlr);
    ctx->info.lba_size = 1 << ns_data->lbaf[ns_data->flbas.format].lbads;
    ctx->info.zone_size = ns_data_zns->lbafe[ns_data->flbas.format].zsze;
    ctx->info.mdts = (uint64_t)1 << (12 + cap.bits.mpsmin + ctrlr_data->mdts);
    auto zasl = ctrlr_data_zns->zasl;
    ctx->info.zasl = zasl == 0 ? ctx->info.mdts
                               : (uint64_t)1 << (12 + cap.bits.mpsmin + zasl);
    ctx->info.lba_cap = ns_data->ncap;
    if (ctx->verbose)
        log_info(
            "Z Get Device Info: lbs size {}, zone size {}, mdts {}, zasl {}, "
            "lba cap {}, current zone {}, current zslba {}",
            ctx->info.lba_size, ctx->info.zone_size, ctx->info.mdts,
            ctx->info.zasl, ctx->info.lba_cap, ctx->current_zone, ctx->zslba);

    return 0;
}

void *z_calloc(void *arg, int nr, int size)
{
    struct ZstoreContext *ctx = static_cast<struct ZstoreContext *>(arg);
    uint32_t alligned_size = ctx->info.lba_size;
    uint32_t true_size = nr * size;
    if (true_size % alligned_size != 0) {
        return NULL;
    }
    void *temp_buffer =
        (char *)spdk_zmalloc(true_size, alligned_size, NULL,
                             SPDK_ENV_SOCKET_ID_ANY, SPDK_MALLOC_DMA);
    (void)ctx->qpair;
    return temp_buffer;
}

static void __complete(void *arg, const struct spdk_nvme_cpl *cpl)
{
    struct ZstoreContext *ctx = static_cast<struct ZstoreContext *>(arg);

    ctx->num_completed += 1;
    if (spdk_nvme_cpl_is_error(cpl)) {
        spdk_nvme_qpair_print_completion(ctx->qpair,
                                         (struct spdk_nvme_cpl *)cpl);
        fprintf(stderr, "Reset all zone error - status = %s\n",
                spdk_nvme_cpl_get_status_string(&cpl->status));
        ctx->num_fail += 1;
        ctx->num_queued -= 1;
        if (ctx->verbose)
            log_debug("reset zone complete: queued {} completed {} success {} "
                      "fail {}",
                      ctx->num_queued, ctx->num_completed, ctx->num_success,
                      ctx->num_fail);
        if (ctx->verbose)
            SPDK_ERRLOG("nvme io reset error: %s\n",
                        spdk_nvme_cpl_get_status_string(&cpl->status));
        // spdk_nvme_ctrlr_free_io_qpair(ctx->qpair);
        spdk_app_stop(-1);
        return;
    } else {
        ctx->num_success += 1;
        ctx->num_queued -= 1;
    }

    if (ctx->verbose)
        log_debug(
            "reset zone complete: queued {} completed {} success {} fail {}, "
            "load {}",
            ctx->num_queued, ctx->num_completed, ctx->num_success,
            ctx->num_fail, ctx->count.load());
}

int z_read(void *arg, uint64_t slba, void *buffer, uint64_t size)
{
    struct ZstoreContext *ctx = static_cast<struct ZstoreContext *>(arg);
    if (ctx->verbose)
        log_info("z_read: slba {}, size {}", slba, size);

    ERROR_ON_NULL(ctx->qpair, 1);
    ERROR_ON_NULL(buffer, 1);
    int rc = 0;

    int lbas = (size + ctx->info.lba_size - 1) / ctx->info.lba_size;
    int lbas_processed = 0;
    int step_size = (ctx->info.mdts / ctx->info.lba_size);
    int current_step_size = step_size;
    int slba_start = slba;

    while (lbas_processed < lbas) {
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
        ctx->num_queued += 1;
        if (ctx->verbose)
            log_info("cmd_read: slba_start {}, current step size {}, queued {}",
                     slba_start, current_step_size, ctx->num_queued);
        rc = spdk_nvme_ns_cmd_read(ctx->ns, ctx->qpair,
                                   (char *)buffer +
                                       lbas_processed * ctx->info.lba_size,
                                   slba_start,        /* LBA start */
                                   current_step_size, /* number of LBAs */
                                   __complete, ctx, 0);
        if (rc != 0) {
            return 1;
        }

        lbas_processed += current_step_size;
        slba_start = slba + lbas_processed;
    }
    while (ctx->num_queued) {
        if (ctx->verbose)
            log_info("qpair process completion: queued {}, qd {}",
                     ctx->num_queued, ctx->qd);

        spdk_nvme_qpair_process_completions(ctx->qpair, 0);
    }
    return rc;
}

int z_append(void *arg, uint64_t slba, void *buffer, uint64_t size)
{
    struct ZstoreContext *ctx = static_cast<struct ZstoreContext *>(arg);
    if (ctx->verbose)
        log_info("z_append: slba {}, size {}", slba, size);
    ERROR_ON_NULL(ctx->qpair, 1);
    ERROR_ON_NULL(buffer, 1);

    int rc = 0;

    int lbas = (size + ctx->info.lba_size - 1) / ctx->info.lba_size;
    int lbas_processed = 0;
    int step_size = (ctx->info.zasl / ctx->info.lba_size);
    int current_step_size = step_size;
    int slba_start = (slba / ctx->info.zone_size) * ctx->info.zone_size;

    while (lbas_processed < lbas) {
        // Completion completion = {.done = false, .err = 0};
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
        ctx->num_queued += 1;
        if (ctx->verbose)
            log_info(
                "zone_append: slba start {}, current step size {}, queued {}",
                slba_start, current_step_size, ctx->num_queued);
        rc = spdk_nvme_zns_zone_append(ctx->ns, ctx->qpair,
                                       (char *)buffer +
                                           lbas_processed * ctx->info.lba_size,
                                       slba_start,        /* LBA start */
                                       current_step_size, /* number of LBAs */
                                       __complete, ctx, 0);
        if (rc != 0) {
            break;
        }

        lbas_processed += current_step_size;
        slba_start = ((slba + lbas_processed) / ctx->info.zone_size) *
                     ctx->info.zone_size;
    }
    // while (ctx->num_queued >= ctx->qd) {
    while (ctx->num_queued) {
        if (ctx->verbose)
            log_info("qpair process completion: queued {}, qd {}",
                     ctx->num_queued, ctx->qd);
        spdk_nvme_qpair_process_completions(ctx->qpair, 0);
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

void z_free(void *arg, void *buffer)
{
    struct ZstoreContext *ctx = static_cast<struct ZstoreContext *>(arg);
    (void)buffer;
    (void)ctx->qpair;
    // free(buffer);
}

bool __probe_devices_cb(void *cb_ctx, const struct spdk_nvme_transport_id *trid,
                        struct spdk_nvme_ctrlr_opts *opts)
{
    DeviceProber *prober = (DeviceProber *)cb_ctx;
    if (!prober->traddr) {
        return false;
    }
    if (strlen((const char *)trid->traddr) < prober->traddr_len) {
        return false;
    }
    if (strncmp((const char *)trid->traddr, prober->traddr,
                prober->traddr_len) != 0) {
        return false;
    }
    (void)opts;
    return true;
}

void __attach_devices__cb(void *cb_ctx,
                          const struct spdk_nvme_transport_id *trid,
                          struct spdk_nvme_ctrlr *ctrlr,
                          const struct spdk_nvme_ctrlr_opts *opts)
{
    DeviceProber *prober = (DeviceProber *)cb_ctx;
    if (prober == NULL) {
        return;
    }
    prober->ctx->ctrlr = ctrlr;
    // take any ZNS namespace, we do not care which.
    for (int nsid = spdk_nvme_ctrlr_get_first_active_ns(ctrlr); nsid != 0;
         nsid = spdk_nvme_ctrlr_get_next_active_ns(ctrlr, nsid)) {
        struct spdk_nvme_ns *ns = spdk_nvme_ctrlr_get_ns(ctrlr, nsid);
        if (ns == NULL) {
            continue;
        }
        if (spdk_nvme_ns_get_csi(ns) != SPDK_NVME_CSI_ZNS) {
            continue;
        }
        prober->ctx->ns = ns;
        prober->found = true;
        break;
    }
    (void)trid;
    (void)opts;
    return;
}

int z_get_zone_head(void *arg, uint64_t slba, uint64_t *head)
{
    struct ZstoreContext *ctx = static_cast<struct ZstoreContext *>(arg);
    ERROR_ON_NULL(ctx->qpair, 1);
    // ERROR_ON_NULL(qpair->man, 1);
    size_t report_bufsize = spdk_nvme_ns_get_max_io_xfer_size(ctx->ns);
    uint8_t *report_buf = (uint8_t *)calloc(1, report_bufsize);
    Completion completion = {.done = false, .err = 0};
    int rc = spdk_nvme_zns_report_zones(
        ctx->ns, ctx->qpair, report_buf, report_bufsize, slba,
        SPDK_NVME_ZRA_LIST_ALL, true, __get_zone_head_complete, &completion);
    if (rc != 0) {
        return rc;
    }
    POLL_QPAIR(ctx->qpair, completion.done);
    if (completion.err != 0) {
        return completion.err;
    }
    // log_info("here");
    uint32_t zd_index = sizeof(struct spdk_nvme_zns_zone_report);
    struct spdk_nvme_zns_zone_desc *desc =
        (struct spdk_nvme_zns_zone_desc *)(report_buf + zd_index);
    log_info("Zone Capacity (in number of LBAs) {}, Zone Start LBA {}, Write "
             "Pointer (LBA) {}",
             desc->zcap, desc->zslba, desc->wp);
    *head = desc->wp;
    free(report_buf);
    log_info("zone head: {}", *head);
    return rc;
}

int z_destroy_qpair(void *arg)
{
    struct ZstoreContext *ctx = static_cast<struct ZstoreContext *>(arg);
    ERROR_ON_NULL(arg, 1);
    ERROR_ON_NULL(ctx->qpair, 1);
    spdk_nvme_ctrlr_free_io_qpair(ctx->qpair);
    ctx = NULL;
    free(ctx->qpair);
    return 0;
}
