#pragma once
#include "spdk/env.h"
#include "spdk/event.h"
#include "spdk/log.h"
#include "spdk/nvme.h"
#include "spdk/nvme_intel.h"
#include "spdk/nvme_zns.h"
#include "spdk/nvmf_spec.h"
#include "spdk/stdinc.h"
#include "spdk/string.h"
#include "utils.hpp"
#include "zns_utils.h"
#include <atomic>
#include <chrono>
#include <fstream>

using chrono_tp = std::chrono::high_resolution_clock::time_point;

// static const char *g_bdev_name = "Nvme1n2";
static const char *g_hostnqn = "nqn.2024-04.io.zstore:cnode1";
const int zone_num = 1;

// Zone managment
u64 zone_dist = 0x80000; // zone size

// const int append_times = 64;
const int append_times = 10000;
// const int append_times = 12800;
// const int append_times = 16000;
// const int append_times = 12800;
// const int append_times = 128000;

const int value = 10000000; // Integer value to set in the buffer

static struct spdk_mempool *task_pool = NULL;

static TAILQ_HEAD(, ctrlr_entry)
    g_controllers = TAILQ_HEAD_INITIALIZER(g_controllers);
static TAILQ_HEAD(,
                  ns_entry) g_namespaces = TAILQ_HEAD_INITIALIZER(g_namespaces);
static TAILQ_HEAD(,
                  worker_thread) g_workers = TAILQ_HEAD_INITIALIZER(g_workers);

// static struct feature features[SPDK_NVME_FEAT_ARBITRATION + 1] = {};
// static struct spdk_nvme_transport_id g_trid = {};

typedef struct {
    uint64_t lba_size;
    uint64_t zone_size;
    uint64_t zone_cap;
    uint64_t mdts;
    uint64_t zasl;
    uint64_t lba_cap;
} DeviceInfo;

struct ctrlr_entry {
    struct spdk_nvme_ctrlr *ctrlr;
    // struct spdk_nvme_intel_rw_latency_page latency_page;
    TAILQ_ENTRY(ctrlr_entry) link;
    char name[1024];
};

struct ns_entry {
    struct {
        struct spdk_nvme_ctrlr *ctrlr;
        struct spdk_nvme_ns *ns;
    } nvme;

    TAILQ_ENTRY(ns_entry) link;
    uint32_t io_size_blocks;
    uint64_t size_in_ios;
    char name[1024];
};

struct ns_worker_ctx {
    struct ns_entry *entry;
    uint64_t io_completed;
    uint64_t current_queue_depth;
    uint64_t offset_in_ios;
    bool is_draining;
    struct spdk_nvme_qpair *qpair;
    TAILQ_ENTRY(ns_worker_ctx) link;

    DeviceInfo info = {};
    bool verbose = false;
    // tmp values that matters in the run
    u64 current_lba = 0;
    u64 current_zone = 0;
    std::vector<uint32_t> append_lbas;
    // device related
    bool device_support_meta = true;
    // DeviceInfo info;
    u64 zslba;
    bool zstore_open = false;
    int total_ops;

    // latency tracking
    chrono_tp stime;
    chrono_tp etime;
    std::vector<chrono_tp> stimes;
    std::vector<chrono_tp> etimes;
};

struct zstore_task {
    struct ns_worker_ctx *ns_ctx;
    void *buf;
};

struct worker_thread {
    TAILQ_HEAD(, ns_worker_ctx) ns_ctx;
    TAILQ_ENTRY(worker_thread) link;
    unsigned lcore;
    enum spdk_nvme_qprio qprio;
};

struct zstore_context {
    bool verbose = false;
    // int shm_id;
    int outstanding_commands;
    int num_namespaces;
    int num_workers;
    int rw_percentage;
    // int is_random;
    int queue_depth;
    int time_in_sec;
    int io_count;
    // uint8_t latency_tracking_enable;
    // uint8_t arbitration_mechanism;
    // uint8_t arbitration_config;
    uint32_t io_size_bytes;
    uint32_t max_completions;
    uint64_t tsc_rate;
    const char *core_mask;
    const char *workload_type;
};

static struct zstore_context g_zstore = {
    // .shm_id = -1,
    .outstanding_commands = 0,
    .num_namespaces = 0,
    .num_workers = 0,
    // .rw_percentage = 50,
    // .queue_depth = 2,
    .queue_depth = 64,
    .time_in_sec = 10,
    .io_count = 100000,
    // .latency_tracking_enable = 0,
    // .arbitration_mechanism = SPDK_NVME_CC_AMS_RR,
    // .arbitration_config = 0,
    .io_size_bytes = 4096,
    // .io_size_bytes = 131072,
    .max_completions = 0,
    /* Default 4 cores for urgent/high/medium/low */
    .core_mask = "0xf",
    .workload_type = "randrw",
};

// struct feature {
//     uint32_t result;
//     bool valid;
// };

struct ZstoreContext {
    // specific parameters we control
    int qd = 0;
    u64 current_zone;

    bool verbose = false;
    // --------------------------------------

    // spdk things we populate
    // DeviceManager m1;
    // DeviceManager m2;

    // bool done = false;

    std::atomic<int> count; // atomic count for concurrency
};

typedef struct {
    struct ns_worker_ctx *ns_ctx;
    const char *traddr;
    const u_int8_t traddr_len;
    bool found;
} DeviceProber;

// typedef struct {
// } Zone;

typedef struct {
    bool done = false;
    int err = 0;
} Completion;

static void zstore_qpair_setup(struct ns_worker_ctx *ns_ctx,
                               spdk_nvme_io_qpair_opts qpair_opts)
{
    // 2. creating qpairs
    // NOTE we don't want to modify anythng with the default qpair right now,
    // as it only controls the submission and completion queue (default to
    // 128/512)

    // spdk_nvme_ctrlr_get_default_io_qpair_opts(ctx->m1.ctrlr, &qpair_opts,
    //                                           sizeof(qpair_opts));
    // qpair_opts.delay_cmd_submit = true;
    // qpair_opts.create_only = true;

    log_info("alloc qpair of queue size {}, request size {}",
             qpair_opts.io_queue_size, qpair_opts.io_queue_requests);

    ns_ctx->qpair = spdk_nvme_ctrlr_alloc_io_qpair(
        ns_ctx->entry->nvme.ctrlr, &qpair_opts, sizeof(qpair_opts));
    // ctx->m2.qpair = spdk_nvme_ctrlr_alloc_io_qpair(ctx->m2.ctrlr,
    // &qpair_opts,
    //                                                sizeof(qpair_opts));

    if (ns_ctx->qpair == NULL) {
        log_error("Could not allocate IO queue pair\n");
        spdk_app_stop(-1);
        return;
    }
    // 3. connect qpair,
    // NOTE  we dont need to do this
    // rc = spdk_nvme_ctrlr_connect_io_qpair(ctx->ctrlr, ctx->qpair);
    // if (rc) {
    //     log_error("Could not connect IO queue pair\n");
    //     spdk_app_stop(-1);
    //     return;
    // }
}

static void zstore_qpair_teardown(struct ns_worker_ctx *ns_ctx)
{
    log_info("disconnect and free qpair");
    spdk_nvme_ctrlr_disconnect_io_qpair(ns_ctx->qpair);
    spdk_nvme_ctrlr_free_io_qpair(ns_ctx->qpair);
}

// TODO: make ZNS device class and put this in Init() or ctor
static void zstore_init(struct ns_worker_ctx *ns_ctx)
{
    if (spdk_nvme_ns_get_md_size(ns_ctx->entry->nvme.ns) == 0) {
        ns_ctx->device_support_meta = false;
    }

    // Adjust the capacity for user data = total capacity - footer size
    // The L2P table information at the end of the segment
    // Each block needs (LBA + timestamp + stripe ID, 20 bytes) for L2P table
    // recovery; we round the number to block size
    auto zone_size =
        spdk_nvme_zns_ns_get_zone_size_sectors(ns_ctx->entry->nvme.ns);
    u32 num_zones = spdk_nvme_zns_ns_get_num_zones(ns_ctx->entry->nvme.ns);
    u64 zone_capacity = 0;
    if (zone_size == 2ull * 1024 * 256) {
        zone_capacity = 1077 * 256; // hard-coded here since it is ZN540;
                                    // update this for emulated SSDs
    } else {
        zone_capacity = zone_size;
    }
    ns_ctx->info.zone_cap = zone_capacity;
    if (ns_ctx->verbose)
        log_info("Zone size: {}, zone cap: {}, num of zones: {}\n", zone_size,
                 zone_capacity, num_zones);

    uint32_t blockSize = 4096;
    uint64_t storageSpace = 1024 * 1024 * 1024 * 1024ull;
    auto mMappingBlockUnitSize = blockSize * blockSize / 4;
    if (ns_ctx->verbose)
        log_info(
            "block size: {}, storage space: {}, mapping block unit size: {}\n",
            blockSize, storageSpace, mMappingBlockUnitSize);
}

// TropoDB

#include "spdk/env.h"
// #include "spdk/log.h"
#include "spdk/nvme.h"
#include "spdk/nvme_zns.h"
// #include "spdk/nvmf_spec.h"

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

// static void __append_complete(void *arg, const struct spdk_nvme_cpl
// *completion)
// {
//     __operation_complete(arg, completion);
// }

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

int z_reset(struct ns_worker_ctx *ns_ctx)
{
    log_info("z_reset");
    ERROR_ON_NULL(ns_ctx->qpair, 1);
    Completion completion = {.done = false};
    int rc = spdk_nvme_zns_reset_zone(ns_ctx->entry->nvme.ns, ns_ctx->qpair,
                                      0, /* starting LBA of the zone to reset */
                                      true, /* don't reset all zones */
                                      __reset_zone_complete, &completion);

    if (rc != 0)
        return rc;
    POLL_QPAIR(ns_ctx->qpair, completion.done);
    return rc;
}

int z_reset(struct ns_worker_ctx *ns_ctx, uint64_t slba, bool all)
{
    ERROR_ON_NULL(ns_ctx->qpair, 1);
    Completion completion = {.done = false};
    int rc =
        spdk_nvme_zns_reset_zone(ns_ctx->entry->nvme.ns, ns_ctx->qpair,
                                 slba, /* starting LBA of the zone to reset */
                                 all,  /* don't reset all zones */
                                 __reset_zone_complete, &completion);
    if (rc != 0)
        return rc;
    POLL_QPAIR(ns_ctx->qpair, completion.done);
    return rc;
}

int z_get_device_info(struct ns_worker_ctx *ns_ctx, bool verbose)
{
    // ERROR_ON_NULL(info, 1);
    ERROR_ON_NULL(ns_ctx, 1);
    ERROR_ON_NULL(ns_ctx->entry->nvme.ctrlr, 1);
    ERROR_ON_NULL(ns_ctx->entry->nvme.ns, 1);

    ns_ctx->zslba = zone_dist * ns_ctx->current_zone;

    const struct spdk_nvme_ns_data *ns_data =
        spdk_nvme_ns_get_data(ns_ctx->entry->nvme.ns);
    const struct spdk_nvme_zns_ns_data *ns_data_zns =
        spdk_nvme_zns_ns_get_data(ns_ctx->entry->nvme.ns);
    const struct spdk_nvme_ctrlr_data *ctrlr_data =
        spdk_nvme_ctrlr_get_data(ns_ctx->entry->nvme.ctrlr);
    const spdk_nvme_zns_ctrlr_data *ctrlr_data_zns =
        spdk_nvme_zns_ctrlr_get_data(ns_ctx->entry->nvme.ctrlr);
    union spdk_nvme_cap_register cap =
        spdk_nvme_ctrlr_get_regs_cap(ns_ctx->entry->nvme.ctrlr);
    ns_ctx->info.lba_size = 1 << ns_data->lbaf[ns_data->flbas.format].lbads;
    ns_ctx->info.zone_size = ns_data_zns->lbafe[ns_data->flbas.format].zsze;
    ns_ctx->info.mdts = (uint64_t)1
                        << (12 + cap.bits.mpsmin + ctrlr_data->mdts);
    auto zasl = ctrlr_data_zns->zasl;
    ns_ctx->info.zasl = zasl == 0
                            ? ns_ctx->info.mdts
                            : (uint64_t)1 << (12 + cap.bits.mpsmin + zasl);
    ns_ctx->info.zasl = zasl == 0
                            ? ns_ctx->info.mdts
                            : (uint64_t)1 << (12 + cap.bits.mpsmin + zasl);
    ns_ctx->info.lba_cap = ns_data->ncap;

    if (verbose)
        log_info(
            "Z Get Device Info: lbs size {}, zone size {}, mdts {}, zasl {}, "
            "lba cap {}, current zone {}, current zslba {}",
            ns_ctx->info.lba_size, ns_ctx->info.zone_size, ns_ctx->info.mdts,
            ns_ctx->info.zasl, ns_ctx->info.lba_cap, ns_ctx->current_zone,
            ns_ctx->zslba);

    return 0;
}

void *z_calloc(struct ns_worker_ctx *ns_ctx, int nr, int size)
{
    uint32_t alligned_size = ns_ctx->info.lba_size;
    uint32_t true_size = nr * size;
    if (true_size % alligned_size != 0) {
        return NULL;
    }
    void *temp_buffer =
        (char *)spdk_zmalloc(true_size, alligned_size, NULL,
                             SPDK_ENV_SOCKET_ID_ANY, SPDK_MALLOC_DMA);
    (void)ns_ctx->qpair;
    return temp_buffer;
}

static void __append_complete(void *ctx, const struct spdk_nvme_cpl *cpl)
{
    struct ns_worker_ctx *ns_ctx = static_cast<struct ns_worker_ctx *>(ctx);
    ns_ctx->current_queue_depth--;
    ns_ctx->io_completed++;

    if (spdk_nvme_cpl_is_error(cpl)) {
        spdk_nvme_qpair_print_completion(ns_ctx->qpair,
                                         (struct spdk_nvme_cpl *)cpl);
        log_error("Completion failed {}",
                  spdk_nvme_cpl_get_status_string(&cpl->status));
        // if (ctx->verbose)
        //     log_debug("operation complete: queued {} completed {} success {}
        //     "
        //               "fail {}",
        //               ctx->num_queued, ctx->num_completed, ctx->num_success,
        //               ctx->num_fail);
        // NOTE: we are not exiting the program
        // spdk_nvme_ctrlr_free_io_qpair(ctx->qpair);
        // spdk_app_stop(-1);
        return;
    }
    if (ns_ctx->current_lba == 0) {
        log_info("setting current lba value: {}", cpl->cdw0);
        ns_ctx->current_lba = cpl->cdw0;
    }
}

static void __complete(void *ctx, const struct spdk_nvme_cpl *cpl)
{
    struct ns_worker_ctx *ns_ctx = static_cast<struct ns_worker_ctx *>(ctx);
    ns_ctx->current_queue_depth--;
    ns_ctx->io_completed++;

    if (spdk_nvme_cpl_is_error(cpl)) {
        spdk_nvme_qpair_print_completion(ns_ctx->qpair,
                                         (struct spdk_nvme_cpl *)cpl);
        log_error("Completion failed {}",
                  spdk_nvme_cpl_get_status_string(&cpl->status));
        // if (ctx->verbose)
        //     log_debug("operation complete: queued {} completed {} success {}
        //     "
        //               "fail {}",
        //               ctx->num_queued, ctx->num_completed, ctx->num_success,
        //               ctx->num_fail);
        // NOTE: we are not exiting the program
        // spdk_nvme_ctrlr_free_io_qpair(ctx->qpair);
        // spdk_app_stop(-1);
        return;
    }
    // if (ctx->current_lba == 0) {
    //     log_info("setting current lba value: {}", cpl->cdw0);
    //     ctx->current_lba = cpl->cdw0;
    // }
}

// static void submit_single_io(struct ns_worker_ctx *ns_ctx)
// {
//     struct zstore_task *task = NULL;
//     uint64_t offset_in_ios;
//     int rc;
//     struct ns_entry *entry = ns_ctx->entry;
//
//     task = static_cast<struct zstore_task *>(spdk_mempool_get(task_pool));
//     if (!task) {
//         fprintf(stderr, "Failed to get task from task_pool\n");
//         exit(1);
//     }
//
//     task->buf = spdk_dma_zmalloc(g_zstore.io_size_bytes, 0x200, NULL);
//     if (!task->buf) {
//         spdk_mempool_put(task_pool, task);
//         fprintf(stderr, "task->buf spdk_dma_zmalloc failed\n");
//         exit(1);
//     }
//
//     task->ns_ctx = ns_ctx;
//
//     if (rc != 0) {
//         fprintf(stderr, "starting I/O failed\n");
//     } else {
//         ns_ctx->current_queue_depth++;
//     }
// }

// static void task_complete(struct zstore_task *task)
// {
//     struct ns_worker_ctx *ns_ctx;
//
//     ns_ctx = task->ns_ctx;
//     ns_ctx->current_queue_depth--;
//     ns_ctx->io_completed++;
//
//     spdk_dma_free(task->buf);
//     spdk_mempool_put(task_pool, task);
//
//     /*
//      * is_draining indicates when time has expired for the test run
//      * and we are just waiting for the previously submitted I/O
//      * to complete.  In this case, do not submit a new I/O to replace
//      * the one just completed.
//      */
//     if (!ns_ctx->is_draining) {
//         submit_single_io(ns_ctx);
//     }
// }

int z_read(struct ns_worker_ctx *ns_ctx, uint64_t slba, void *buffer,
           uint64_t size)
{
    struct zstore_task *task = NULL;
    ERROR_ON_NULL(ns_ctx->qpair, 1);
    ERROR_ON_NULL(buffer, 1);
    int rc = 0;

    int lbas = (size + ns_ctx->info.lba_size - 1) / ns_ctx->info.lba_size;
    int lbas_processed = 0;
    int step_size = (ns_ctx->info.mdts / ns_ctx->info.lba_size);
    int current_step_size = step_size;
    int slba_start = slba;

    while (lbas_processed < lbas) {
        if ((slba + lbas_processed + step_size) / ns_ctx->info.zone_size >
            (slba + lbas_processed) / ns_ctx->info.zone_size) {
            current_step_size =
                ((slba + lbas_processed + step_size) / ns_ctx->info.zone_size) *
                    ns_ctx->info.zone_size -
                lbas_processed - slba;
        } else {
            current_step_size = step_size;
        }
        current_step_size = lbas - lbas_processed > current_step_size
                                ? current_step_size
                                : lbas - lbas_processed;
        // printf("%d step %d  \n", slba_start, current_step_size);
        // if (ctx->verbose)
        //     log_info("cmd_read: slba_start {}, current step size {}, queued
        //     {}",
        //              slba_start, current_step_size, ctx->num_queued);
        rc = spdk_nvme_ns_cmd_read(ns_ctx->entry->nvme.ns, ns_ctx->qpair,
                                   (char *)buffer +
                                       lbas_processed * ns_ctx->info.lba_size,
                                   slba_start,        /* LBA start */
                                   current_step_size, /* number of LBAs */
                                   __complete, task, 0);
        if (rc != 0) {
            // log_error("cmd read error: {}", rc);
            // if (rc == -ENOMEM) {
            //     spdk_nvme_qpair_process_completions(ctx->qpair, 0);
            //     rc = 0;
            // } else
            return 1;
        } else {
            ns_ctx->current_queue_depth++;
        }

        lbas_processed += current_step_size;
        slba_start = slba + lbas_processed;

        // while (ns_ctx->current_queue_depth >= ns_ctx->) {
        //     // if (ctx->verbose)
        //     //     log_info("qpair process completion: queued {}, qd {}",
        //     //              ctx->num_queued, ctx->qd);
        //     rc =
        //     spdk_nvme_qpair_process_completions(ns_ctx->entry->nvme.qpair,
        //                                              0);
        //     if (rc < 0) {
        //         log_error(
        //             "FAILED: spdk_nvme_qpair_process_completion(), err: %d",
        //             rc);
        //     }
        // }
    }

    return rc;
}

int z_append(struct ns_worker_ctx *ns_ctx, uint64_t slba, void *buffer,
             uint64_t size)
{
    struct zstore_task *task = NULL;
    // if (ctx->verbose)
    //     log_info("z_append: slba {}, size {}", slba, size);
    ERROR_ON_NULL(ns_ctx->qpair, 1);
    ERROR_ON_NULL(buffer, 1);

    int rc = 0;

    int lbas = (size + ns_ctx->info.lba_size - 1) / ns_ctx->info.lba_size;
    int lbas_processed = 0;
    int step_size = (ns_ctx->info.zasl / ns_ctx->info.lba_size);
    int current_step_size = step_size;
    int slba_start = (slba / ns_ctx->info.zone_size) * ns_ctx->info.zone_size;

    while (lbas_processed < lbas) {
        // Completion completion = {.done = false, .err = 0};
        if ((slba + lbas_processed + step_size) / ns_ctx->info.zone_size >
            (slba + lbas_processed) / ns_ctx->info.zone_size) {
            current_step_size =
                ((slba + lbas_processed + step_size) / ns_ctx->info.zone_size) *
                    ns_ctx->info.zone_size -
                lbas_processed - slba;
        } else {
            current_step_size = step_size;
        }
        current_step_size = lbas - lbas_processed > current_step_size
                                ? current_step_size
                                : lbas - lbas_processed;
        // if (ctx->verbose)
        //     log_info(
        //         "zone_append: slba start {}, current step size {}, queued
        //         {}", slba_start, current_step_size, ctx->num_queued);
        rc = spdk_nvme_zns_zone_append(
            ns_ctx->entry->nvme.ns, ns_ctx->qpair,
            (char *)buffer + lbas_processed * ns_ctx->info.lba_size,
            slba_start,        /* LBA start */
            current_step_size, /* number of LBAs */
            __append_complete, task, 0);
        if (rc != 0) {
            log_error("zone append starting I/O failed{}", rc);
            // if (rc == -ENOMEM) {
            //     spdk_nvme_qpair_process_completions(ctx->qpair, 0);
            //     rc = 0;
            // } else
            break;
        } else {
            ns_ctx->current_queue_depth++;
        }

        lbas_processed += current_step_size;
        slba_start = ((slba + lbas_processed) / ns_ctx->info.zone_size) *
                     ns_ctx->info.zone_size;

        // while (ns_ctx->entry->nvme.num_queued >= ns_ctx->entry->nvme.qd) {
        //     if (ns_ctx->entry->nvme.verbose)
        //         log_info("qpair process completion: queued {}, qd {}",
        //                  ns_ctx->entry->nvme.num_queued,
        //                  ns_ctx->entry->nvme.qd);
        //     rc =
        //     spdk_nvme_qpair_process_completions(ns_ctx->entry->nvme.qpair,
        //     0); if (rc < 0) {
        //         log_error(
        //             "FAILED: spdk_nvme_qpair_process_completion(), err: %d",
        //             rc);
        //     }
        // }
    }
    // if (ns_ctx->verbose)
    //     log_info("qpair process completion: queued {}, qd {}",
    //              ns_ctx->entry->nvme.num_queued, ns_ctx->entry->nvme.qd);

    return rc;
}

static void zstore_exit(struct ns_worker_ctx *ns_ctx)
{
    // Increment the parameter
    // ctx->current_zone++;
    // Write the new value back to the file
    std::ofstream outputFile("../current_zone");
    if (outputFile.is_open()) {
        outputFile << ns_ctx->current_zone;
        outputFile.close();
    }
}

void z_free(struct ns_worker_ctx *ns_ctx, void *buffer)
{
    (void)buffer;
    (void)ns_ctx->qpair;
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
    prober->ns_ctx->entry->nvme.ctrlr = ctrlr;
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
        prober->ns_ctx->entry->nvme.ns = ns;
        prober->found = true;
        break;
    }
    (void)trid;
    (void)opts;
    return;
}

int z_get_zone_head(struct ns_worker_ctx *ns_ctx, uint64_t slba, uint64_t *head)
{
    ERROR_ON_NULL(ns_ctx->qpair, 1);
    // ERROR_ON_NULL(qpair->man, 1);
    size_t report_bufsize =
        spdk_nvme_ns_get_max_io_xfer_size(ns_ctx->entry->nvme.ns);
    uint8_t *report_buf = (uint8_t *)calloc(1, report_bufsize);
    Completion completion = {.done = false, .err = 0};
    int rc = spdk_nvme_zns_report_zones(
        ns_ctx->entry->nvme.ns, ns_ctx->qpair, report_buf, report_bufsize, slba,
        SPDK_NVME_ZRA_LIST_ALL, true, __get_zone_head_complete, &completion);
    if (rc != 0) {
        return rc;
    }
    POLL_QPAIR(ns_ctx->qpair, completion.done);
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

int z_destroy_qpair(struct ns_worker_ctx *ns_ctx)
{
    ERROR_ON_NULL(ns_ctx, 1);
    ERROR_ON_NULL(ns_ctx->qpair, 1);
    spdk_nvme_ctrlr_free_io_qpair(ns_ctx->qpair);
    ns_ctx = NULL;
    free(ns_ctx->qpair);
    return 0;
}

static void register_ns(struct spdk_nvme_ctrlr *ctrlr, struct spdk_nvme_ns *ns)
{
    struct ns_entry *entry;
    const struct spdk_nvme_ctrlr_data *cdata;

    cdata = spdk_nvme_ctrlr_get_data(ctrlr);

    if (spdk_nvme_ns_get_size(ns) < g_zstore.io_size_bytes ||
        spdk_nvme_ns_get_extended_sector_size(ns) > g_zstore.io_size_bytes ||
        g_zstore.io_size_bytes % spdk_nvme_ns_get_extended_sector_size(ns)) {
        printf("WARNING: controller %-20.20s (%-20.20s) ns %u has invalid "
               "ns size %" PRIu64 " / block size %u for I/O size %u\n",
               cdata->mn, cdata->sn, spdk_nvme_ns_get_id(ns),
               spdk_nvme_ns_get_size(ns),
               spdk_nvme_ns_get_extended_sector_size(ns),
               g_zstore.io_size_bytes);
        return;
    }

    entry = (struct ns_entry *)malloc(sizeof(struct ns_entry));
    if (entry == NULL) {
        perror("ns_entry malloc");
        exit(1);
    }

    entry->nvme.ctrlr = ctrlr;
    entry->nvme.ns = ns;

    entry->size_in_ios = spdk_nvme_ns_get_size(ns) / g_zstore.io_size_bytes;
    entry->io_size_blocks =
        g_zstore.io_size_bytes / spdk_nvme_ns_get_sector_size(ns);

    snprintf(entry->name, 44, "%-20.20s (%-20.20s)", cdata->mn, cdata->sn);

    g_zstore.num_namespaces++;
    TAILQ_INSERT_TAIL(&g_namespaces, entry, link);
}

static void register_ctrlr(struct spdk_nvme_ctrlr *ctrlr)
{
    uint32_t nsid;
    struct spdk_nvme_ns *ns;

    struct ctrlr_entry *entry =
        (struct ctrlr_entry *)calloc(1, sizeof(struct ctrlr_entry));
    union spdk_nvme_cap_register cap = spdk_nvme_ctrlr_get_regs_cap(ctrlr);
    const struct spdk_nvme_ctrlr_data *cdata = spdk_nvme_ctrlr_get_data(ctrlr);

    if (entry == NULL) {
        perror("ctrlr_entry malloc");
        exit(1);
    }

    snprintf(entry->name, sizeof(entry->name), "%-20.20s (%-20.20s)", cdata->mn,
             cdata->sn);

    entry->ctrlr = ctrlr;
    TAILQ_INSERT_TAIL(&g_controllers, entry, link);

    for (nsid = spdk_nvme_ctrlr_get_first_active_ns(ctrlr); nsid != 0;
         nsid = spdk_nvme_ctrlr_get_next_active_ns(ctrlr, nsid)) {
        log_info("getting ns {}", nsid);
        ns = spdk_nvme_ctrlr_get_ns(ctrlr, nsid);
        if (ns == NULL) {
            continue;
        }
        if (spdk_nvme_ns_get_csi(ns) != SPDK_NVME_CSI_ZNS) {
            log_info("ns {} is not zns ns", nsid);
            // continue;
        } else {
            log_info("ns {} is zns ns", nsid);
        }
        register_ns(ctrlr, ns);
        // FIXME: why is it getting namespace 1 2 again?
        // force exit when we have two namespace
        break;
    }
}

static int register_workers(void)
{
    log_info("reg worksers");
    uint32_t i;
    struct worker_thread *worker;
    // enum spdk_nvme_qprio qprio = SPDK_NVME_QPRIO_URGENT;

    SPDK_ENV_FOREACH_CORE(i)
    {
        worker = (struct worker_thread *)calloc(1, sizeof(*worker));
        if (worker == NULL) {
            fprintf(stderr, "Unable to allocate worker\n");
            return -1;
        }

        TAILQ_INIT(&worker->ns_ctx);
        worker->lcore = i;
        TAILQ_INSERT_TAIL(&g_workers, worker, link);
        g_zstore.num_workers++;
        // FIXME: we have 4 cores, but we only want 1 worker for now
        if (g_zstore.num_workers == 1)
            break;
    }
    return 0;
}

static void zns_dev_init(struct zstore_context *ctx, std::string ip1,
                         std::string port1, std::string ip2, std::string port2)
{
    int rc = 0;
    if (ctx->verbose)
        log_info("Successfully started the application\n");

    // 1. connect nvmf device
    struct spdk_nvme_transport_id trid1 = {};
    snprintf(trid1.traddr, sizeof(trid1.traddr), "%s", ip1.c_str());
    snprintf(trid1.trsvcid, sizeof(trid1.trsvcid), "%s", port1.c_str());
    snprintf(trid1.subnqn, sizeof(trid1.subnqn), "%s", g_hostnqn);
    trid1.adrfam = SPDK_NVMF_ADRFAM_IPV4;
    trid1.trtype = SPDK_NVME_TRANSPORT_TCP;

    struct spdk_nvme_transport_id trid2 = {};
    snprintf(trid2.traddr, sizeof(trid2.traddr), "%s", ip2.c_str());
    snprintf(trid2.trsvcid, sizeof(trid2.trsvcid), "%s", port2.c_str());
    snprintf(trid2.subnqn, sizeof(trid2.subnqn), "%s", g_hostnqn);
    trid2.adrfam = SPDK_NVMF_ADRFAM_IPV4;
    trid2.trtype = SPDK_NVME_TRANSPORT_TCP;

    struct spdk_nvme_ctrlr_opts opts;
    spdk_nvme_ctrlr_get_default_ctrlr_opts(&opts, sizeof(opts));
    memcpy(opts.hostnqn, g_hostnqn, sizeof(opts.hostnqn));

    register_ctrlr(spdk_nvme_connect(&trid1, &opts, sizeof(opts)));
    // register_ctrlr(spdk_nvme_connect(&trid2, &opts, sizeof(opts)));

    log_info("Found {} namspaces", g_zstore.num_namespaces);
}

static int register_controllers(struct zstore_context *ctx)
{
    log_info("Initializing NVMe Controllers");

    zns_dev_init(ctx, "192.168.1.121", "4420", "192.168.1.121", "5520");

    if (g_zstore.num_namespaces == 0) {
        fprintf(stderr, "No valid namespaces to continue IO testing\n");
        return 1;
    }

    return 0;
}

static void unregister_controllers(void)
{
    struct ctrlr_entry *entry, *tmp;
    struct spdk_nvme_detach_ctx *detach_ctx = NULL;

    TAILQ_FOREACH_SAFE(entry, &g_controllers, link, tmp)
    {
        TAILQ_REMOVE(&g_controllers, entry, link);
        // if (g_zstore.latency_tracking_enable &&
        //     spdk_nvme_ctrlr_is_feature_supported(
        //         entry->ctrlr, SPDK_NVME_INTEL_FEAT_LATENCY_TRACKING)) {
        //     set_latency_tracking_feature(entry->ctrlr, false);
        // }
        spdk_nvme_detach_async(entry->ctrlr, &detach_ctx);
        free(entry);
    }

    while (detach_ctx && spdk_nvme_detach_poll_async(detach_ctx) == -EAGAIN) {
        ;
    }
}

static int associate_workers_with_ns(int current_zone)
{
    struct ns_entry *entry = TAILQ_FIRST(&g_namespaces);
    struct worker_thread *worker = TAILQ_FIRST(&g_workers);
    struct ns_worker_ctx *ns_ctx;
    int i, count;

    count = g_zstore.num_namespaces > g_zstore.num_workers
                ? g_zstore.num_namespaces
                : g_zstore.num_workers;
    log_info("worker {}, ns {}, count {}", g_zstore.num_workers,
             g_zstore.num_namespaces, count);
    // FIXME:
    count = 1;

    for (i = 0; i < count; i++) {
        if (entry == NULL) {
            break;
        }

        ns_ctx = (struct ns_worker_ctx *)malloc(sizeof(struct ns_worker_ctx));
        if (!ns_ctx) {
            return 1;
        }
        memset(ns_ctx, 0, sizeof(*ns_ctx));

        printf("Associating %s with lcore %d\n", entry->name, worker->lcore);
        ns_ctx->entry = entry;
        TAILQ_INSERT_TAIL(&worker->ns_ctx, ns_ctx, link);

        worker = TAILQ_NEXT(worker, link);
        if (worker == NULL) {
            worker = TAILQ_FIRST(&g_workers);
        }

        entry = TAILQ_NEXT(entry, link);
        if (entry == NULL) {
            entry = TAILQ_FIRST(&g_namespaces);
        }
    }

    return 0;
}

static int init_ns_worker_ctx(struct ns_worker_ctx *ns_ctx)
{
    struct spdk_nvme_ctrlr *ctrlr = ns_ctx->entry->nvme.ctrlr;
    struct spdk_nvme_io_qpair_opts opts;

    spdk_nvme_ctrlr_get_default_io_qpair_opts(ctrlr, &opts, sizeof(opts));
    // opts.qprio = qprio;

    ns_ctx->qpair = spdk_nvme_ctrlr_alloc_io_qpair(ctrlr, &opts, sizeof(opts));
    if (!ns_ctx->qpair) {
        printf("ERROR: spdk_nvme_ctrlr_alloc_io_qpair failed\n");
        return 1;
    }

    // allocate space for times
    ns_ctx->stimes.reserve(append_times);
    ns_ctx->etimes.reserve(append_times);

    return 0;
}

static void cleanup_ns_worker_ctx(struct ns_worker_ctx *ns_ctx)
{
    spdk_nvme_ctrlr_free_io_qpair(ns_ctx->qpair);
}

static void cleanup(uint32_t task_count)
{
    struct ns_entry *entry, *tmp_entry;
    struct worker_thread *worker, *tmp_worker;
    struct ns_worker_ctx *ns_ctx, *tmp_ns_ctx;

    TAILQ_FOREACH_SAFE(entry, &g_namespaces, link, tmp_entry)
    {
        TAILQ_REMOVE(&g_namespaces, entry, link);
        free(entry);
    };

    TAILQ_FOREACH_SAFE(worker, &g_workers, link, tmp_worker)
    {
        TAILQ_REMOVE(&g_workers, worker, link);

        /* ns_worker_ctx is a list in the worker */
        TAILQ_FOREACH_SAFE(ns_ctx, &worker->ns_ctx, link, tmp_ns_ctx)
        {
            TAILQ_REMOVE(&worker->ns_ctx, ns_ctx, link);
            free(ns_ctx);
        }

        free(worker);
    };

    if (spdk_mempool_count(task_pool) != (size_t)task_count) {
        fprintf(stderr, "task_pool count is %zu but should be %u\n",
                spdk_mempool_count(task_pool), task_count);
    }
    spdk_mempool_free(task_pool);
}

static void zstore_cleanup(u32 task_count)
{

    unregister_controllers();
    cleanup(task_count);

    spdk_env_fini();

    // if (rc != 0) {
    //     fprintf(stderr, "%s: errors occurred\n", argv[0]);
    // }
}
