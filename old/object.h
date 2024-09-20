#pragma once
#include "utils.hpp"

//
// KV Store
//
#define TMP_KVOBJECT(var, key)                                                 \
    struct kvobject var;                                                       \
    var.name = key;

struct kvobject {
    int len;
    void *data;       /* points to after 'name' */
    std::string name; /* null terminated */
};

//
// Objects
//
struct ObjectMetadata {
    std::string key;
    uint64_t size;
    uint64_t creation_date;
    uint64_t modification_date;
    uint64_t data_offset; // Offset on SSD where data is stored
};

//
// Log entry
//
struct LogEntry{
    std::string key;
    u32 seq;
    u32 chunk_seq;
    u32 lba;
};