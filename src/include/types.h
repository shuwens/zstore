#pragma once
#include "utils.h"
#include <boost/beast/http.hpp>
#include <fmt/chrono.h>
#include <shared_mutex>
#include <spdk/nvme.h>
#include <string>
#include <tuple>
#include <vector>

using chrono_tp = std::chrono::high_resolution_clock::time_point;
struct Timer {
    chrono_tp t1;
    chrono_tp t2;
    chrono_tp t3;
    chrono_tp t4;
    chrono_tp t5;
    chrono_tp t6;
    // bool check;
};

namespace http = boost::beast::http; // from <boost/beast/http.hpp>
typedef http::request<http::string_body> HttpRequest;
typedef http::response<http::string_body> HttpResponse;

typedef std::string ObjectKey;
typedef std::tuple<std::string, std::string, std::string> DevTuple;

// typedef std::pair<std::pair<std::string, std::string>, u64> TargetLbaPair;
typedef std::pair<std::string, u64> TargetLbaPair;

struct MapEntry {
    std::tuple<TargetLbaPair, TargetLbaPair, TargetLbaPair> data;

    std::string &first_tgt() { return std::get<0>(data).first; }
    u64 &first_lba() { return std::get<0>(data).second; }

    std::string &second_tgt() { return std::get<1>(data).first; }
    u64 &second_lba() { return std::get<1>(data).second; }

    std::string &third_tgt() { return std::get<2>(data).first; }
    u64 &third_lba() { return std::get<2>(data).second; }
};

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
    uint8_t *data;
    uint32_t successBytes;
    uint32_t targetBytes;
    uint32_t curOffset;
    void *cb_args;
    uint32_t ioOffset;

    bool available;

    // Used inside a Segment write/read
    ZstoreController *ctrl;
    struct spdk_thread *io_thread;
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
        // spdk_nvme_cmd_cb cb;
        // void *ctx;
        uint32_t flags;
    } ioContext;
    uint32_t bufferSize; // for recording partial writes

    // HttpRequest request;
    // bool keep_alive;

    // For write append
    bool is_write;
    bool write_complete;
    uint64_t append_lba;

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
