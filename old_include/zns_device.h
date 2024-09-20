#pragma once
#include "spdk/env.h"
#include "spdk/log.h"
#include "spdk/nvme.h"
#include "spdk/nvme_intel.h"
#include "spdk/nvme_zns.h"
#include "spdk/nvmf_spec.h"
#include "spdk/stdinc.h"
#include "spdk/string.h"
#include "utils.hpp"
#include <atomic>
#include <bits/stdc++.h>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <fmt/core.h>
#include <fstream>
#include <stdio.h>
#include <vector>

using chrono_tp = std::chrono::high_resolution_clock::time_point;
static const char *g_hostnqn = "nqn.2024-04.io.zstore:cnode1";

struct ctrlr_entry {
    struct spdk_nvme_ctrlr *ctrlr;
    struct spdk_nvme_intel_rw_latency_page latency_page;
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

    // latency tracking
    chrono_tp stime;
    chrono_tp etime;
    std::vector<chrono_tp> stimes;
    std::vector<chrono_tp> etimes;
};

struct arb_task {
    struct ns_worker_ctx *ns_ctx;
    void *buf;
};

struct worker_thread {
    TAILQ_HEAD(, ns_worker_ctx) ns_ctx;
    TAILQ_ENTRY(worker_thread) link;
    unsigned lcore;
    enum spdk_nvme_qprio qprio;
};

struct arb_context {
    int outstanding_commands;
    int num_namespaces;
    int num_workers;
    int rw_percentage;
    int is_random;
    int queue_depth;
    int time_in_sec;
    int io_count;
    uint32_t io_size_bytes;
    uint32_t max_completions;
    uint64_t tsc_rate;
    const char *core_mask;
    const char *workload_type;
};

struct feature {
    uint32_t result;
    bool valid;
};

static struct spdk_mempool *task_pool = NULL;

static TAILQ_HEAD(, ctrlr_entry)
    g_controllers = TAILQ_HEAD_INITIALIZER(g_controllers);
static TAILQ_HEAD(,
                  ns_entry) g_namespaces = TAILQ_HEAD_INITIALIZER(g_namespaces);
static TAILQ_HEAD(,
                  worker_thread) g_workers = TAILQ_HEAD_INITIALIZER(g_workers);

static struct feature features[SPDK_NVME_FEAT_ARBITRATION + 1] = {};
static struct spdk_nvme_transport_id g_trid = {};

static struct arb_context g_zstore = {
    .outstanding_commands = 0,
    .num_workers = 0,
    .num_namespaces = 0,
    .rw_percentage = 50,
    .queue_depth = 64,
    .time_in_sec = 9,
    .io_count = 1000000,
    .io_size_bytes = 4096,
    .max_completions = 0,
    // .core_mask = "0xf",
    .core_mask = "0x1",
    .workload_type = "randrw",
};

static int g_dpdk_mem = 0;
static bool g_dpdk_mem_single_seg = false;

/*
 * For weighted round robin zstore mechanism, the smaller value between
 * weight and burst will be picked to execute the commands in one queue.
 */
#define USER_SPECIFIED_HIGH_PRIORITY_WEIGHT 32
#define USER_SPECIFIED_MEDIUM_PRIORITY_WEIGHT 16
#define USER_SPECIFIED_LOW_PRIORITY_WEIGHT 8
