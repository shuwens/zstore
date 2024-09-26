#pragma once

#include "device.h"
// #include "object.h"
#include <map>
#include <mutex>
#include <string>

static bool verbose = false;

// constexpr u64 zone_dist = 0x80000;
// These data struct are not supposed to be global like this, but this is the
// simple way to do it. So sue me.

// typedef lba

static struct mg_context *g_ctx; /* Set by start_civetweb() */

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
ZstoreController *gZstoreController;

// static struct spdk_mempool *task_pool = NULL;

// static struct ctrlr_entry *g_controller;
// static struct ns_entry *g_namespace;
// static struct worker_thread *g_worker;
