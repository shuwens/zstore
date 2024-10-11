#pragma once
#include "global.h"
#include "spdk/nvme.h"
#include "utils.h"
// #include "zstore_controller.h"
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

std::shared_mutex g_shared_mutex_;
std::shared_mutex g_session_mutex_;

class ZstoreController;

struct DrainArgs {
    ZstoreController *ctrl;
    bool success;
    bool ready;
};

struct RequestContext {
    // The buffers are pre-allocated
    uint8_t *dataBuffer;

    // A user request use the following field:
    // Info: lba, size, req_type, data
    // pbaArray, successBytes, and targetBytes
    uint64_t lba;
    uint32_t size;
    uint8_t req_type;
    bool is_write;
    uint8_t *data;
    uint32_t successBytes;
    uint32_t targetBytes;
    uint32_t curOffset;
    void *cb_args;
    uint32_t ioOffset;

    bool available;

    // Used inside a Segment write/read
    ZstoreController *ctrl;
    uint32_t zoneId;
    uint32_t offset;

    double stime;
    double ctime;
    uint64_t timestamp;

    struct timeval timeA;

    struct {
        struct spdk_nvme_ns *ns;
        struct spdk_nvme_qpair *qpair;
        void *data;
        uint64_t offset;
        uint32_t size;
        spdk_nvme_cmd_cb cb;
        void *ctx;
        uint32_t flags;
    } ioContext;
    uint32_t bufferSize; // for recording partial writes

    // std::shared_ptr<std::string const> doc_root;
    HttpRequest request;
    MapEntry entry;
    // session *session_;
    // http::message_generator *msg;
    bool keep_alive;

    // closure
    std::function<void(HttpRequest)> read_fn;
    std::function<void(HttpRequest, MapEntry)> write_fn;
    // std::function<void(http::message_generator msg, bool keep_alive)> fn;
    // void apply() { value = function(value); }

    void Clear();
    void Queue();
    double GetElapsedTime();
    void PrintStats();
    void CopyFrom(const RequestContext &o);
};

struct IoThread {
    struct spdk_nvme_poll_group *group;
    struct spdk_thread *thread;
    uint32_t threadId;
    ZstoreController *controller;
};

struct RequestContextPool {
    RequestContext *contexts;
    std::vector<RequestContext *> availableContexts;
    uint32_t capacity;

    RequestContextPool(uint32_t cap);
    RequestContext *GetRequestContext(bool force);
    void ReturnRequestContext(RequestContext *slot);
};

inline uint64_t round_up(uint64_t value, uint64_t align)
{
    return (value + align - 1) / align * align;
}

inline uint64_t round_down(uint64_t value, uint64_t align)
{
    return value / align * align;
}

void completeOneEvent(void *arg, const struct spdk_nvme_cpl *completion);
void complete(void *arg, const struct spdk_nvme_cpl *completion);
void thread_send_msg(spdk_thread *thread, spdk_msg_fn fn, void *args);
void event_call(uint32_t core_id, spdk_event_fn fn, void *arg1, void *arg2);

int dispatchWorker(void *args);
int dispatchObjectWorker(void *args);
int ioWorker(void *args);
int httpWorker(void *args);
int dummyWorker(void *args);
void zoneRead(void *arg1);

double GetTimestampInUs();
double timestamp();
double gettimediff(struct timeval s, struct timeval e);

using chrono_tp = std::chrono::high_resolution_clock::time_point;

// struct arb_context {
//     int shm_id;
//     int outstanding_commands;
//     int num_namespaces;
//     int num_workers;
//     int rw_percentage;
//     int is_random;
//     int queue_depth;
//     int time_in_sec;
//     int io_count;
//     uint8_t latency_tracking_enable;
//     uint8_t arbitration_mechanism;
//     uint8_t arbitration_config;
//     uint32_t io_size_bytes;
//     uint32_t max_completions;
//     uint64_t tsc_rate;
//     const char *core_mask;
//     const char *workload_type;
// };

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

// Object and Map related
