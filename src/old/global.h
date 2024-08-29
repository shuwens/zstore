#pragma once

#include <map>
#include <mutex>
#include <string>

#include "object.h"

constexpr u64 zone_dist = 0x80000;

// These data struct are not supposed to be global like this, but this is the
// simple way to do it. So sue me.

// typedef lba

static struct mg_context *g_ctx; /* Set by start_civetweb() */

// object tables, used only by zstore
// key -> tuple of <zns target, lba>
// std::map<std::string, std::tuple<std::pair<std::string, int32_t>>>
// zstore_map; std::mutex obj_table_mutex;

// in memory object tables, used only by kv store
std::map<std::string, kvobject> mem_obj_table;
std::mutex mem_obj_table_mutex;
