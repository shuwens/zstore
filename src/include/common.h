#pragma once
#include "types.h"
#include "utils.h"
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

namespace net = boost::asio; // from <boost/asio.hpp>

inline uint64_t round_up(uint64_t value, uint64_t align)
{
    return (value + align - 1) / align * align;
}

inline uint64_t round_down(uint64_t value, uint64_t align)
{
    return value / align * align;
}

int httpWorker(void *args);
int ioWorker(void *args);
int dummyWorker(void *args);
int dispatchWorker(void *args);
int dispatchObjectWorker(void *args);

// void completeOneEvent(void *arg, const struct spdk_nvme_cpl *completion);
// void complete(void *arg, const struct spdk_nvme_cpl *completion);
void thread_send_msg(spdk_thread *thread, spdk_msg_fn fn, void *args);
// void event_call(uint32_t core_id, spdk_event_fn fn, void *arg1, void *arg2);

auto zoneRead(void *args) -> net::awaitable<void>;
auto zoneAppend(void *args) -> net::awaitable<void>;

double GetTimestampInUs();
double timestamp();
double gettimediff(struct timeval s, struct timeval e);

// Object and Map related

Result<MapEntry> createMapEntry(DevTuple tuple, u64 lba1, u32 len1, u64 lba2,
                                u32 len2, u64 lba3, u32 len3);

Result<DevTuple> GetDevTuple(ObjectKey object_key);

class ZstoreController;
class Device;

Result<RequestContext *> MakeReadRequest(ZstoreController *zctrl_, Device *dev,
                                         uint64_t offset, HttpRequest request);

Result<RequestContext *> MakeWriteRequest(ZstoreController *zctrl_, Device *dev,
                                          HttpRequest request,
                                          std::vector<uint8_t> data);
