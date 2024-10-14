#pragma once
#include "device.h"
#include "http_server.h"
#include "utils.h"
#include "zstore_controller.h"
#include <boost/beast/http.hpp>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <vector>

using chrono_tp = std::chrono::high_resolution_clock::time_point;

static struct spdk_nvme_transport_id g_trid = {};

static std::vector<Device *> g_devices;
static int g_dpdk_mem = 0;
static bool g_dpdk_mem_single_seg = false;

static int g_micro_to_second = 1'000'000;

std::shared_mutex g_shared_mutex_;
std::shared_mutex g_session_mutex_;

namespace http = boost::beast::http; // from <boost/beast/http.hpp>
typedef http::request<http::string_body> HttpRequest;

typedef std::string ObjectKey;
typedef std::tuple<std::pair<std::string, std::string>,
                   std::pair<std::string, std::string>,
                   std::pair<std::string, std::string>>
    DevTuple;
typedef std::pair<std::pair<std::string, std::string>, u64> TargetLbaPair;

struct MapEntry {
    std::tuple<TargetLbaPair, TargetLbaPair, TargetLbaPair> data;

    std::pair<std::string, std::string> &first_tgt()
    {
        return std::get<0>(data).first;
    }
    u64 &first_lba() { return std::get<0>(data).second; }

    std::pair<std::string, std::string> &second_tgt()
    {
        return std::get<1>(data).first;
    }
    u64 &second_lba() { return std::get<1>(data).second; }

    std::pair<std::string, std::string> &third_tgt()
    {
        return std::get<2>(data).first;
    }
    u64 &third_lba() { return std::get<2>(data).second; }
};

class ZstoreController;
class session;

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
        uint64_t offset; // lba
        uint32_t size;   // lba_count
        spdk_nvme_cmd_cb cb;
        void *ctx;
        uint32_t flags;
    } ioContext;
    uint32_t bufferSize; // for recording partial writes

    HttpRequest request;
    bool keep_alive;

    // closure
    bool is_write;
    MapEntry entry;
    std::function<void(HttpRequest)> read_fn;
    std::function<void(HttpRequest, MapEntry)> write_fn;

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

static bool verbose = false;

// constexpr u64 zone_dist = 0x80000;
// These data struct are not supposed to be global like this, but this is the
// simple way to do it. So sue me.

// typedef lba

// a simple test program to ZapRAID
// uint64_t gSize = 64 * 1024 * 1024 / Configuration::GetBlockSize();
// uint64_t gSize = 5;
// uint64_t gSize = 1;

// uint32_t qDepth = 256;
// int g_current_zone = 0;

// uint64_t gRequestSize = 4096;
// bool gSequential = false;
// bool gSkewed = false;
// uint32_t gNumBuffers = 1024 * 128;
// bool gCrash = false;
// std::string gAccessTrace = "";

// uint32_t gTestGc = 0; // 0: fill
// bool gTestMode = false;
// bool gUseLbaLock = false;
// bool gHybridSize = false;
// 150GiB WSS for GC test
// uint64_t gWss = 150ull * 1024 * 1024 * 1024 / Configuration::GetBlockSize();
// uint64_t gTrafficSize = 1ull * 1024 * 1024 * 1024 * 1024;

// uint32_t gChunkSize = 4096 * 4;
// uint32_t gWriteSizeUnit = 16 * 4096; // 256KiB still stuck, use 64KiB and try

// std::string gTraceFile = "";
// struct timeval tv1;

// uint8_t *buffer_pool;
//
// bool gVerify = false;
// uint8_t *bitmap = nullptr;
// uint32_t gNumOpenSegments = 1;
// uint64_t gL2PTableSize = 0;
// std::map<uint32_t, uint32_t> latCnt;

// std::string dummy_device = "dummy_device";

// These data struct are not supposed to be global like this, but this is the
// simple way to do it. So sue me.

ZstoreController *gZstoreController;
