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
const int append_times = 1000;
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
    // int rw_percentage;
    // int is_random;
    int queue_depth;
    // int time_in_sec;
    int io_count;
    // uint8_t latency_tracking_enable;
    // uint8_t arbitration_mechanism;
    // uint8_t arbitration_config;
    uint32_t io_size_bytes;
    uint32_t max_completions;
    // uint64_t tsc_rate;
    const char *core_mask;
    const char *workload_type;
};

static struct zstore_context g_zstore = {
    // .shm_id = -1,
    .outstanding_commands = 0,
    .num_namespaces = 0,
    .num_workers = 0,
    // .rw_percentage = 50,
    .queue_depth = 64,
    // .time_in_sec = 60,
    .io_count = 100000,
    // .latency_tracking_enable = 0,
    // .arbitration_mechanism = SPDK_NVME_CC_AMS_RR,
    // .arbitration_config = 0,
    .io_size_bytes = 131072,
    .max_completions = 0,
    /* Default 4 cores for urgent/high/medium/low */
    .core_mask = "0xf",
    .workload_type = "randrw",
};

typedef struct {
    struct ns_worker_ctx *ns_ctx;
    const char *traddr;
    const u_int8_t traddr_len;
    bool found;
} DeviceProber;

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

static void task_complete(struct zstore_task *task);

static void io_complete(void *ctx, const struct spdk_nvme_cpl *completion);

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
