#pragma once
#include <boost/beast/http.hpp>
#include <spdk/nvme.h>
#include <vector>

static struct spdk_nvme_transport_id g_trid = {};

class Device;
static std::vector<Device *> g_devices;
static int g_dpdk_mem = 0;
static bool g_dpdk_mem_single_seg = false;

static int g_micro_to_second = 1'000'000;

// std::shared_mutex g_session_mutex_;

// static bool verbose = false;

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
