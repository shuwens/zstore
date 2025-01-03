#pragma once

#include "types.h"
#include <boost/asio.hpp>
#include <spdk/event.h>

namespace asio = boost::asio; // from <boost/asio.hpp>

// utils
inline uint64_t round_up(uint64_t value, uint64_t align)
{
    return (value + align - 1) / align * align;
}

inline uint64_t round_down(uint64_t value, uint64_t align)
{
    return value / align * align;
}

double GetTimestampInUs();
double timestamp();
double gettimediff(struct timeval s, struct timeval e);

int ioWorker(void *args);
void thread_send_msg(spdk_thread *thread, spdk_msg_fn fn, void *args);

// Zstore related
auto zoneRead(void *args) -> asio::awaitable<void>;
auto zoneAppend(void *args) -> asio::awaitable<void>;
auto zoneFinish(void *args) -> asio::awaitable<void>;

// Object and Map related

Result<MapEntry> createMapEntry(DevTuple tuple, u64 lba1, u32 len1, u64 lba2,
                                u32 len2, u64 lba3, u32 len3);

class ZstoreController;
class Device;

// TODO: Use Length?
Result<RequestContext *> MakeReadRequest(ZstoreController *zctrl_, Device *dev,
                                         uint64_t offset);

Result<RequestContext *> MakeWriteRequest(ZstoreController *zctrl_, Device *dev,
                                          HttpRequest &req);

Result<RequestContext *> MakeManagementRequest(ZstoreController *zctrl_,
                                               Device *dev);
