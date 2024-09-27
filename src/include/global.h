#pragma once

#include "device.h"
#include "utils.hpp"
#include <CivetServer.h>
#include <fstream>
#include <iostream>
#include <list>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

// int obj_cmp(const void *arg1, const void *arg2)
// {
//     const struct object *o1 = arg1, *o2 = arg2;
//     return strcmp(o1->name, o2->name);
// }

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
ZstoreController *gZstoreController;

// static struct spdk_mempool *task_pool = NULL;

// static struct ctrlr_entry *g_controller;
// static struct ns_entry *g_namespace;
// static struct worker_thread *g_worker;

static struct mg_context *g_ctx; /* Set by start_civetweb() */

// These data struct are not supposed to be global like this, but this is the
// simple way to do it. So sue me.

#define TMP_OBJECT(var, key)                                                   \
    struct object var;                                                         \
    var.name = key;

struct object {
    int len;
    void *data;       /* points to after 'name' */
    std::string name; /* null terminated */
};

// in memory object tables
std::map<std::string, object> mem_obj_table;
std::mutex mem_obj_table_mutex;

// object tables
std::map<std::string, object> obj_table;
std::mutex obj_table_mutex;

void handler(int sig)
{
    mg_stop(g_ctx);
    printf("stopped\n");
    exit(0);
}

void parse_uri(const char *uri, char *bkt, char *key)
{
    char c;
    *bkt = *key = 0;

    if (*uri == '/')
        uri++;

    while ((c = *uri++) != '/' && c != 0)
        *bkt++ = c;
    *bkt = 0;

    if (c == 0)
        return;

    while ((c = *uri++) != 0)
        *key++ = c;
    *key = 0;
}

char *timestamp(char *buf, size_t len)
{
    time_t t = time(NULL);
    struct tm *tm = gmtime(&t);
    strftime(buf, len, "%FT%T.000Z", tm);
    return buf;
}

/*
void start() { stime = std::chrono::high_resolution_clock::now(); }
            u64 end()
            {
                auto etime = std::chrono::high_resolution_clock::now();
                auto dur =
                    std::chrono::duration_cast<std::chrono::microseconds>(
                        etime - stime);
                return dur.count();
            }
*/
