#pragma once
#include "types.h"
#include <boost/asio.hpp>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fmt/core.h>
#include <spdk/event.h>
#include <spdk/nvme.h>
#include <spdk/thread.h>
#include <vector>

namespace asio = boost::asio; // from <boost/asio.hpp>

inline uint64_t round_up(uint64_t value, uint64_t align)
{
    return (value + align - 1) / align * align;
}

inline uint64_t round_down(uint64_t value, uint64_t align)
{
    return value / align * align;
}

int ioWorker(void *args);
void thread_send_msg(spdk_thread *thread, spdk_msg_fn fn, void *args);

auto zoneRead(void *args) -> asio::awaitable<void>;
auto zoneAppend(void *args) -> asio::awaitable<void>;
auto zoneFinish(void *args) -> asio::awaitable<void>;

double GetTimestampInUs();
double timestamp();
double gettimediff(struct timeval s, struct timeval e);

// Object and Map related

Result<MapEntry> createMapEntry(DevTuple tuple, u64 lba1, u32 len1, u64 lba2,
                                u32 len2, u64 lba3, u32 len3);

class ZstoreController;
class Device;

// TODO: Use Length?
Result<RequestContext *> MakeReadRequest(ZstoreController *zctrl_, Device *dev,
                                         uint64_t offset);

Result<RequestContext *> MakeWriteRequest(ZstoreController *zctrl_, Device *dev,
                                          char *data);

Result<RequestContext *> MakeManagementRequest(ZstoreController *zctrl_,
                                               Device *dev);
