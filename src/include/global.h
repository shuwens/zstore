#pragma once
#include "device.h"
#include "utils.hpp"
#include "zstore_controller.h"
#include <map>
#include <mutex>
#include <string>
#include <vector>

class ZstoreController;

struct DrainArgs {
    ZstoreController *ctrl;
    bool success;
    bool ready;
};

struct Request {
    ZstoreController *controller;
    uint64_t offset;
    uint32_t size;
    void *data;
    char type;
    zns_raid_request_complete cb_fn;
    void *cb_args;
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
        uint64_t offset;
        uint32_t size;
        spdk_nvme_cmd_cb cb;
        void *ctx;
        uint32_t flags;
    } ioContext;

    uint32_t bufferSize; // for recording partial writes

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

uint32_t qDepth = 256;
int g_current_zone = 0;

uint64_t gRequestSize = 4096;
bool gSequential = false;
bool gSkewed = false;
uint32_t gNumBuffers = 1024 * 128;
bool gCrash = false;
std::string gAccessTrace = "";

uint32_t gTestGc = 0; // 0: fill
bool gTestMode = false;
bool gUseLbaLock = false;
bool gHybridSize = false;
// 150GiB WSS for GC test
// uint64_t gWss = 150ull * 1024 * 1024 * 1024 / Configuration::GetBlockSize();
uint64_t gTrafficSize = 1ull * 1024 * 1024 * 1024 * 1024;

uint32_t gChunkSize = 4096 * 4;
uint32_t gWriteSizeUnit = 16 * 4096; // 256KiB still stuck, use 64KiB and try

std::string gTraceFile = "";
struct timeval tv1;

uint8_t *buffer_pool;

bool gVerify = false;
uint8_t *bitmap = nullptr;
uint32_t gNumOpenSegments = 1;
uint64_t gL2PTableSize = 0;
std::map<uint32_t, uint32_t> latCnt;

std::string dummy_device = "dummy_device";

// These data struct are not supposed to be global like this, but this is the
// simple way to do it. So sue me.

ZstoreController *gZstoreController;
