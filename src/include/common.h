#pragma once
#include "object.h"
#include "spdk/nvme.h"
#include "spdk/nvme_intel.h"
#include "utils.hpp"
#include "zstore_controller.h"
#include <bits/stdc++.h>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fmt/core.h>
#include <spdk/event.h>
#include <spdk/nvme.h>
#include <spdk/thread.h>
#include <string>
#include <vector>

std::shared_mutex g_shared_mutex_;
// std::mutex g_mutex_;
std::shared_mutex g_session_mutex_;

typedef std::pair<std::string, int32_t> MapEntry;
typedef std::unordered_map<std::string, MapEntry>::const_iterator MapIter;

// class ZstoreController;
// struct RequestContext;

inline uint64_t round_up(uint64_t value, uint64_t align)
{
    return (value + align - 1) / align * align;
}

inline uint64_t round_down(uint64_t value, uint64_t align)
{
    return value / align * align;
}

typedef void (*zns_raid_request_complete)(void *cb_arg);

void completeOneEvent(void *arg, const struct spdk_nvme_cpl *completion);
void complete(void *arg, const struct spdk_nvme_cpl *completion);
void thread_send_msg(spdk_thread *thread, spdk_msg_fn fn, void *args);
void event_call(uint32_t core_id, spdk_event_fn fn, void *arg1, void *arg2);

double GetTimestampInUs();
double timestamp();
double gettimediff(struct timeval s, struct timeval e);

using chrono_tp = std::chrono::high_resolution_clock::time_point;

struct arb_context {
    int shm_id;
    int outstanding_commands;
    int num_namespaces;
    int num_workers;
    int rw_percentage;
    int is_random;
    int queue_depth;
    int time_in_sec;
    int io_count;
    uint8_t latency_tracking_enable;
    uint8_t arbitration_mechanism;
    uint8_t arbitration_config;
    uint32_t io_size_bytes;
    uint32_t max_completions;
    uint64_t tsc_rate;
    const char *core_mask;
    const char *workload_type;
};

static struct spdk_nvme_transport_id g_trid = {};

// static struct arb_context g_arbitration = {
//     .shm_id = -1,
//     .outstanding_commands = 0,
//     .num_workers = 0,
//     .num_namespaces = 0,
//     .rw_percentage = 50,
//     .queue_depth = 64,
//     .time_in_sec = 9,
//     .io_count = 1000000,
//     .latency_tracking_enable = 0,
//     .arbitration_mechanism = SPDK_NVME_CC_AMS_RR,
//     .arbitration_config = 0,
//     .io_size_bytes = 4096,
//     // .io_size_bytes = 131072,
//     .max_completions = 0,
//     /* Default 4 cores for urgent/high/medium/low */
//     // .core_mask = "0xf",
//     .core_mask = "0x1ff",
//     .workload_type = "randrw",
// };

static int g_dpdk_mem = 0;
static bool g_dpdk_mem_single_seg = false;

static int g_micro_to_second = 1'000'000;

/*
 * For weighted round robin arbitration mechanism, the smaller value between
 * weight and burst will be picked to execute the commands in one queue.
 */
#define USER_SPECIFIED_HIGH_PRIORITY_WEIGHT 32
#define USER_SPECIFIED_MEDIUM_PRIORITY_WEIGHT 16
#define USER_SPECIFIED_LOW_PRIORITY_WEIGHT 8

// Object and Map related

Result<MapEntry> createMapEntry(std::string device, int32_t lba);

void updateMapEntry(MapEntry entry, std::string device, int32_t lba);
