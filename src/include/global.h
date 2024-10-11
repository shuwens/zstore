#pragma once
// #include "device.h"
// #include "http_server.h"
#include "utils.h"
// #include "zstore_controller.h"
#include <boost/beast/http.hpp>
#include <iostream>
#include <map>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

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

// typedef std::unordered_map<std::string, MapEntry>::const_iterator MapIter;

namespace http = boost::beast::http; // from <boost/beast/http.hpp>
typedef http::request<http::string_body> HttpRequest;

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
