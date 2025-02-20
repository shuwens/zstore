#pragma once

#include "configuration.h"
#include "types.h"
#include <openssl/sha.h>

typedef uint32_t timestamp_t;
typedef uint64_t bid_t; // block ID

// Data structure used to track all chunks to LBA tuple (lba, len)
typedef std::map<u64, LbaTuple> ChunkList;

// Log entry types:
// - Data: block following the log entry is data block
// - Header: block following the log entry is header block, which contains
// information to all the chunks
// - Spliter: block following the log entry is spliter block
enum class LogEntryType : uint8_t {
    kInvalid = 0,
    kData = 1,
    kHeader = 2,
    kSpliter = 3,
};

struct LogEntry {
    LogEntry()
        : type(LogEntryType::kInvalid), next_offset(0), seqnum(0),
          chunk_seqnum(0), next(0), key_size(0)
    {
    }
    LogEntryType type; // 1 byte
    // Offset to the next log entry on disk (0 if no next // entry)
    uint64_t next_offset;  // 8 bytes
    uint64_t seqnum;       // 8 bytes
    uint64_t chunk_seqnum; // 8 bytes
    uint64_t next;         // 8 bytes
    uint16_t key_size;     // 2 bytes

    // deprecated: last seen entry from map
    // MapEntry last_seen;

    char key_hash[65];

    // Function to compute the size of the log entry (for writing/reading)
    static size_t size(uint32_t value_size)
    {
        return sizeof(LogEntry) + value_size;
    }
};

struct ZstoreObject {
    ZstoreObject() : entry(), datalen(0), body(nullptr), key_size(0) {}
    struct LogEntry entry;
    uint64_t datalen;
    void *body;
    uint16_t key_size; // 2 bytes
    char key_hash[65];

    static size_t size(uint16_t key_size, uint64_t datalen)
    {
        return sizeof(ZstoreObject) + key_size + datalen;
    }
};

// https://stackoverflow.com/questions/2262386/generate-sha256-with-openssl-and-c/10632725

// Function to compute SHA-256 hash from a string_view
std::string sha256(std::string_view input);
ObjectKeyHash computeSHA256(const std::string &data);

std::string hashToCString(const ObjectKeyHash &hashKey);

// Function to convert a hexadecimal string to a std::array<uint8_t, 32>
ObjectKeyHash stringToHashKey(const std::string &hexString);

unsigned int arrayToSeed(const ObjectKeyHash &hashKey);

// -----------------------------------------------------

// Serialize and deserialize the ChunkList map
char *serializeMap(const ChunkList &map, u64 bufferSize);
ChunkList deserializeMap(void *buffer);
ChunkList deserializeDummyMap(std::string data, u64 num_chunks);

// Split an object into chunks
std::vector<ZstoreObject> splitObjectIntoChunks(ZstoreObject obj);
// Merge chunks into an object
void mergeChunksIntoObject(std::vector<RequestContext *> chunk_vec,
                           std::string req_body);

bool ReadBufferToZstoreObject(const char *buffer, size_t buffer_size,
                              ZstoreObject &obj);
std::vector<char> WriteZstoreObjectToBuffer(const ZstoreObject &obj);
