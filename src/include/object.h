#pragma once
#include "common.h"
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>

typedef uint32_t timestamp_t;
typedef uint64_t bid_t; // block ID

struct LogEntry {
    uint64_t next_offset;  // Offset to the next log entry on disk (0 if no next
                           // entry)
    uint64_t seqnum;       // 8 bytes
    uint64_t chunk_seqnum; // 8 bytes
    uint64_t next;         // 8 bytes
    uint16_t key_size;     // 2 bytes

    // last seen entry from map
    MapEntry last_seen;

    char key[];

    // Function to compute the size of the log entry (for writing/reading)
    static size_t size(uint32_t value_size)
    {
        return sizeof(LogEntry) + value_size;
    }
};

struct ZstoreObject {
    // ZstoreObject() : keylen(0), key(nullptr), datalen(0), body(nullptr) {}
    struct LogEntry entry;
    uint64_t datalen;
    void *body;
    uint16_t key_size; // 2 bytes
    char key[];

    static size_t size(uint16_t key_size, uint64_t datalen)
    {
        return sizeof(ZstoreObject) + key_size + datalen;
    }
};

// Object and Map related

// -----------------------------------------------------

bool write_to_buffer(ZstoreObject *obj, char *buffer, size_t buffer_size);

ZstoreObject *read_from_buffer(const char *buffer, size_t buffer_size);

ZstoreObject *ReadObject(uint64_t offset, void *ctx);

// Result<struct ZstoreObject *> AppendObject(uint64_t offset,
//                                            struct obj_object *doc, void
//                                            *ctx);
