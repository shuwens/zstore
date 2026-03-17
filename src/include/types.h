#pragma once
#include "device.h"
#include "rdma_utils.h"
#include "utils.h"
#include <array>
#include <boost/beast/http.hpp>
#include <boost/circular_buffer.hpp>
#include <boost/thread.hpp>
#include <boost/thread/condition_variable.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/unordered/concurrent_flat_map.hpp>
#include <boost/unordered/concurrent_flat_set.hpp>
#include <cmath>
#include <fmt/chrono.h>
#include <spdk/nvme.h>

using namespace std;

//
// Aliasing to make life easier
//

// HTTP stuff
namespace http = boost::beast::http; // from <boost/beast/http.hpp>
namespace asio = boost::asio;        // from <boost/asio.hpp>
using HttpRequest = http::request<http::string_body>;
using HttpResponse = http::response<http::string_body>;

//
// Zstore types
//

// Basic Zstore types
using ObjectKey = std::string;
using ObjectKeyHash = std::array<uint8_t, 32>;
using TargetDev = std::string;
// alias to make my life easier
using Lba = u64; // offset
using Length = u32;
using DeviceId = int;
using ZoneId = u32;

struct Location {
    DeviceId device_id;
    Lba lba;
};

struct MapEntry {
    Location locations[3]; // or std::array<Location, 3>
    Length len;
    bool tombstoned;
};

// Device types
// typedef std::tuple<TargetDev, Lba, Length> TargetLbaTuple;
// typedef tuple<pair<TargetDev, u32>, pair<TargetDev, u32>, pair<TargetDev,
// u32>> DevTuple;
// typedef std::tuple<uint8_t, Lba, Length> TargetLbaTuple;
using LbaTuple = std::tuple<Lba, Length>;
// device id, zone id
using DevTuple = tuple<pair<u8, u32>, pair<u8, u32>, pair<u8, u32>>;

// This is the most important data structure of Zstore as it holds the mapping
// of object key to object location on the storage device.
//
// There are a couple of future improvements that can be made:
// - we can use a sha256 hash of the object key to reduce the size of the key
// - we can use the index of target device instead of the target device name
// - when we GC or renew epoch, it seems like we will need to create a blocking
// operation on the map
// https://www.boost.org/doc/libs/1_86_0/libs/unordered/doc/html/unordered.html#concurrent_blocking_operations
// using MapEntry = std::tuple<TargetLbaTuple, TargetLbaTuple, TargetLbaTuple>;
// using MapEntry = std::tuple<TargetLbaTuple, TargetLbaTuple, TargetLbaTuple>;

// Circular buffer for RDMA writes: 112 bytes padded to 128 bytes
struct BufferEntry {
    ObjectKeyHash key_hash; // SHA256 hash: 32 bytes
    uint8_t epoch;          // Epoch: 1 byte
    bool is_index;          // Is index: 1 byte
    MapEntry value;         // Target device and LBA: 112 bytes
    uint8_t padding[16];    // Padding: 112 bytes
};

// We use a circular buffer to store RDMA writes, note that we have two designs
// for this. We are choosing the first design for now, as it is more efficient:
// 1. use a 64 bytes entry for the circular buffer, where the upper 32 bytes is
//    the hash of object key, and the lower 32 bytes stores the current epoch,
//    and the value is the target device and LBA
// 2. use a 64 bits entry for the circular buffer, where the upper 31 bytes is
//   the hash of object key, and the lower bit signals the epoch change
//  (0: no change, 1: epoch change), and the value is the target device and LBA
// using RdmaBuffer = boost::circular_buffer<BufferEntry>;

// Custom hash function for std::array<uint8_t, 32>
struct ArrayHash {
    std::size_t operator()(const std::array<uint8_t, 32> &key) const
    {
        std::size_t hash = 0;
        for (auto byte : key) {
            hash = hash * 31 + byte; // Simple hash combination
        }
        return hash;
    }
};

// We store the SHA256 hash of the object key as the key in the Zstore Map, and
// the value in the Zstore Map is the tuple of the target device and the LBA
using ZstoreMap =
    boost::concurrent_flat_map<ObjectKeyHash, MapEntry, ArrayHash>;

// We record hash of recent writes as a concurrent hashmap, where the key is
// the hash, and the value is the gateway of the writes.
using InflightWriteSet = boost::concurrent_flat_set<ObjectKeyHash>;

// this stores keys that we need to broadcast
using BroadcastSet = boost::concurrent_flat_set<ObjectKeyHash>;

struct CondEntry {
    boost::mutex mtx;
    boost::condition_variable cv;
    bool released = false;
};

using CondMap =
    boost::concurrent_flat_map<ObjectKeyHash, std::shared_ptr<CondEntry>>;

// We record the target device and LBA of the blocks that we need to GC
// using ZstoreGcSet = boost::concurrent_flat_set<TargetLbaTuple>;

class ZstoreController;

struct DrainArgs {
    ZstoreController *ctrl;
    bool success;
    bool ready;
};

struct RequestContext {
    ZstoreController *ctrl;
    struct spdk_thread *io_thread;
    Device *device;
    ZoneId zone_id;
    void *cb_args;

    struct {
        struct spdk_nvme_ns *ns;
        struct spdk_nvme_qpair *qpair;
        char *data;
        Lba offset;  // lba
        Length size; // length: buffer / block size
        uint32_t flags;
        // only used for management commands
        spdk_nvme_cmd_cb cb;
        void *ctx;
    } ioContext;
    // for recording partial writes
    uint32_t bufferSize; // actual buffer size
    char *dataBuffer;    // The buffers are pre-allocated

    // uint64_t lba;
    // uint32_t ioOffset;
    // uint32_t offset;

    // State
    bool success;
    bool available;

    // For write append
    bool is_write;
    bool write_complete;
    Lba append_lba;
    std::string response_body;

    double stime;
    double ctime;
    uint64_t timestamp;
    struct timeval timeA;

    void Clear();
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

// From SimpleSZD
/**
 * @brief Holds general information about a ZNS device.
 */
typedef struct {
    Lba lba_size;      // Size of one block, also as logical block address.
    ZoneId zone_size;  //  Size of one zone in lbas.
    uint64_t zone_cap; // Size of user availabe space in one zone.
    uint64_t mdts;     // Maximum data transfer size in bytes.
    uint64_t zasl;     // Maximum size of one append command in bytes.
    Lba lba_cap;       // Amount of lbas available on the device.
    Lba min_lba;       // Minimum lba that is allowed to be written to.
    Lba max_lba;       // Maximum lba that is allowed to be written to.
    const char *name;  // Name used by SPDK to identify device.
} DeviceInfo;
extern const DeviceInfo DeviceInfo_default;

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

// const char *DEFAULT_PORT = "12345";
const size_t RDMA_BUFFER_SIZE = 10 * 1024 * 1024;

enum message_id { MSG_INVALID = 0, MSG_MR, MSG_READY, MSG_DONE };

struct message {
    int id;

    union {
        struct {
            uint64_t addr;
            uint32_t rkey;
        } mr;
    } data;
};

struct RdmaClient {
    struct client_context ctx;
    ZstoreController *ctrl;
};
