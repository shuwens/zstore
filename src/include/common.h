#pragma once
#include "global.h"
#include "object.h"
#include "spdk/nvme.h"
#include "utils.h"
#include "zstore_controller.h"
#include <bits/stdc++.h>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fmt/core.h>
#include <spdk/event.h>
#include <spdk/nvme.h>
#include <spdk/thread.h>
#include <string>

inline uint64_t round_up(uint64_t value, uint64_t align)
{
    return (value + align - 1) / align * align;
}

inline uint64_t round_down(uint64_t value, uint64_t align)
{
    return value / align * align;
}

void completeOneEvent(void *arg, const struct spdk_nvme_cpl *completion);
void complete(void *arg, const struct spdk_nvme_cpl *completion);
void thread_send_msg(spdk_thread *thread, spdk_msg_fn fn, void *args);
void event_call(uint32_t core_id, spdk_event_fn fn, void *arg1, void *arg2);

double GetTimestampInUs();
double timestamp();
double gettimediff(struct timeval s, struct timeval e);

// Object and Map related

Result<MapEntry> createMapEntry(DevTuple tuple, int32_t lba1, int32_t lba2,
                                int32_t lba3);

Result<DevTuple> GetDevTuple(ObjectKey object_key);

Result<RequestContext *>
MakeReadRequest(ZstoreController *zctrl_, Device *dev, uint64_t offset,
                HttpRequest request, std::function<void(HttpRequest)> closure);

Result<RequestContext *>
MakeWriteRequest(ZstoreController *zctrl_, Device *dev, HttpRequest request,
                 MapEntry entry,
                 std::function<void(HttpRequest, MapEntry)> closure);
