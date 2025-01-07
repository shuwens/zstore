#include "include/object.h"
#include "include/zstore_controller.h"
#include "src/include/utils.h"
#include <boost/outcome/success_failure.hpp>

// Function to compute SHA256 hash
ObjectKeyHash computeSHA256(const std::string &data)
{
    ObjectKeyHash hash{};
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, data.data(), data.size());
    SHA256_Final(hash.data(), &sha256);
    return hash;
}

std::string hashToCString(const ObjectKeyHash &hashKey)
{
    char hashString[65]; // 64 characters for hex + 1 for null terminator
    for (size_t i = 0; i < hashKey.size(); ++i) {
        snprintf(&hashString[i * 2], 3, "%02x",
                 hashKey[i]); // Format each byte as two hex digits
    }
    return std::string(hashString);
}

ObjectKeyHash stringToHashKey(const std::string &hexString)
{
    if (hexString.size() != 64) {
        throw std::invalid_argument(
            "Invalid hash string length. Expected 64 hexadecimal characters.");
    }

    ObjectKeyHash hashKey{};
    for (size_t i = 0; i < hashKey.size(); ++i) {
        unsigned int byte;
        std::istringstream(hexString.substr(i * 2, 2)) >> std::hex >> byte;
        hashKey[i] = static_cast<uint8_t>(byte);
    }
    return hashKey;
}

// Function to convert a 32-byte array to an unsigned int seed
unsigned int arrayToSeed(const ObjectKeyHash &hashKey)
{
    unsigned int seed = 0;
    for (size_t i = 0; i < hashKey.size(); ++i) {
        seed = seed * 31 + hashKey[i]; // Combine bytes into a single value
    }
    return seed;
}

// void sha256_hash_string(unsigned char hash[SHA256_DIGEST_LENGTH],
//                         char outputBuffer[65])
// {
//     int i = 0;
//
//     for (i = 0; i < SHA256_DIGEST_LENGTH; i++) {
//         sprintf(outputBuffer + (i * 2), "%02x", hash[i]);
//     }
//
//     outputBuffer[64] = 0;
// }

// void sha256_string(const char *string, char outputBuffer[65])
// {
//     unsigned char hash[SHA256_DIGEST_LENGTH];
//     SHA256_CTX sha256;
//     SHA256_Init(&sha256);
//     SHA256_Update(&sha256, string, strlen(string));
//     SHA256_Final(hash, &sha256);
//     int i = 0;
//     for (i = 0; i < SHA256_DIGEST_LENGTH; i++) {
//         sprintf(outputBuffer + (i * 2), "%02x", hash[i]);
//     }
//     outputBuffer[64] = 0;
// }

// Function to compute SHA-256 hash from a string_view
std::string sha256(std::string_view input)
{
    // Buffer to store the SHA-256 hash
    unsigned char hash[SHA256_DIGEST_LENGTH];

    // Compute the SHA-256 hash
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, input.data(), input.size());
    SHA256_Final(hash, &sha256);

    // Convert hash to a hexadecimal string
    std::stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0')
           << static_cast<int>(hash[i]);
    }

    return ss.str();
}

// TODO the following functions need to be updated after  the design changes
// so far they do not cause memory leaks but correctness is not guaranteed
bool ReadBufferToZstoreObject(const char *buffer, size_t buffer_size,
                              ZstoreObject &obj)
{
    if (buffer_size < sizeof(LogEntry)) {
        return false; // Buffer is too small to even contain LogEntry
    }

    size_t offset = 0;

    // Read LogEntry part
    std::memcpy(&obj.entry, buffer + offset, sizeof(LogEntry));
    offset += sizeof(LogEntry);

    // Read key hash
    std::memcpy(obj.key_hash, buffer + offset, sizeof(obj.key_hash));
    offset += sizeof(obj.key_hash);

    // Read the key size
    if (offset + sizeof(uint16_t) > buffer_size) {
        return false; // Buffer overflow
    }
    std::memcpy(&obj.key_size, buffer + offset, sizeof(uint16_t));
    offset += sizeof(uint16_t);

    // Read data length
    if (offset + sizeof(uint64_t) > buffer_size) {
        return false; // Buffer overflow
    }
    std::memcpy(&obj.datalen, buffer + offset, sizeof(uint64_t));
    offset += sizeof(uint64_t);

    // Read body if it exists
    if (obj.datalen > 0) {
        obj.body = std::malloc(obj.datalen);
        if (obj.body == nullptr) {
            return false; // Memory allocation failure
        }

        if (offset + obj.datalen > buffer_size) {
            std::free(obj.body);
            return false; // Buffer overflow
        }
        std::memcpy(obj.body, buffer + offset, obj.datalen);
        offset += obj.datalen;
    }

    return true;
}

std::vector<char> WriteZstoreObjectToBuffer(const ZstoreObject &obj)
{
    // Calculate the total size
    size_t total_size = sizeof(LogEntry) + sizeof(obj.key_hash) +
                        sizeof(uint16_t) + sizeof(uint64_t) + obj.datalen;

    // Create a buffer
    std::vector<char> buffer(total_size);
    size_t offset = 0;

    // Write LogEntry
    std::memcpy(buffer.data() + offset, &obj.entry, sizeof(LogEntry));
    offset += sizeof(LogEntry);

    // Write key hash
    std::memcpy(buffer.data() + offset, obj.key_hash, sizeof(obj.key_hash));
    offset += sizeof(obj.key_hash);

    // Write key size
    std::memcpy(buffer.data() + offset, &obj.key_size, sizeof(uint16_t));
    offset += sizeof(uint16_t);

    // Write data length
    std::memcpy(buffer.data() + offset, &obj.datalen, sizeof(uint64_t));
    offset += sizeof(uint64_t);

    // Write body if it exists
    if (obj.datalen > 0 && obj.body != nullptr) {
        std::memcpy(buffer.data() + offset, obj.body, obj.datalen);
        offset += obj.datalen;
    }

    return buffer;
}

char *serializeMap(const ChunkList &map, u64 bufferSize)
{
    assert(bufferSize == Configuration::GetChunkSize());
    // Calculate total size required for the buffer
    // bufferSize = sizeof(size_t) + map.size() * (sizeof(u64) + 2 *
    // sizeof(u64));
    char *buffer = (char *)malloc(bufferSize); // Allocate the buffer
    char *ptr = static_cast<char *>(buffer);

    // Write the size of the map
    size_t mapSize = map.size();
    memcpy(ptr, &mapSize, sizeof(size_t));
    ptr += sizeof(size_t);

    // Write each key-value pair
    for (const auto &[key, value] : map) {
        memcpy(ptr, &key, sizeof(u64));
        ptr += sizeof(u64);

        u64 first = std::get<0>(value);
        u64 second = std::get<1>(value);

        memcpy(ptr, &first, sizeof(u64));
        ptr += sizeof(u64);

        memcpy(ptr, &second, sizeof(u64));
        ptr += sizeof(u64);
    }

    return buffer;
}

ChunkList deserializeMap(void *buffer)
{
    char *ptr = static_cast<char *>(buffer);

    // Read the size of the map
    size_t mapSize;
    memcpy(&mapSize, ptr, sizeof(size_t));
    ptr += sizeof(size_t);

    // Reconstruct the map
    ChunkList map;
    for (size_t i = 0; i < mapSize; ++i) {
        u64 key;
        memcpy(&key, ptr, sizeof(u64));
        ptr += sizeof(u64);

        u64 first, second;
        memcpy(&first, ptr, sizeof(u64));
        ptr += sizeof(u64);

        memcpy(&second, ptr, sizeof(u64));
        ptr += sizeof(u64);

        map[key] = std::make_tuple(first, second);
    }

    return map;
}

ChunkList deserializeDummyMap(std::string data, u64 num_chunks)
{
    ChunkList chunk_list;
    for (u64 i = 0; i < num_chunks; i++) {
        chunk_list[i] = std::make_tuple(i, Configuration::GetChunkSize());
    }

    return chunk_list;
}

// Helper function to take a larger object and split it into chunks
// of 4KB each
std::vector<ZstoreObject> splitObjectIntoChunks(ZstoreObject obj)
{
    assert(obj.datalen == Configuration::GetObjectSizeInBytes());
    u64 num_chunks = obj.datalen / Configuration::GetChunkSize();
    u64 remaining_data_len = obj.datalen;

    // we have n chunks and 1 chunk list to keep track of all the chunks
    std::vector<ZstoreObject> chunk_vec;
    chunk_vec.reserve(num_chunks);

    for (u64 i = 0; i < num_chunks; i++) {
        ZstoreObject chunk;
        chunk.entry.type = LogEntryType::kData;
        chunk.entry.seqnum = obj.entry.seqnum;
        chunk.entry.chunk_seqnum = i + 1;
        if (remaining_data_len >= Configuration::GetChunkSize()) {
            chunk.datalen = Configuration::GetChunkSize();
            remaining_data_len -= Configuration::GetChunkSize();
        } else {
            chunk.datalen = remaining_data_len;
        }
        chunk.body = std::malloc(chunk.datalen);
        std::memcpy(chunk.body, (char *)obj.body + (i * chunk.datalen),
                    chunk.datalen);
        // std::strcpy(chunk.key_hash, obj.key_hash);
        chunk.key_size = obj.key_size;
        chunk_vec.push_back(chunk);
    }

    return chunk_vec;
}

// Helper function to merge chunks into a single object
void mergeChunksIntoObject(std::vector<RequestContext *> chunk_vec,
                           std::string req_body)
{
    // auto chunk_len = Configuration::GetChunkSize();
    //
    // for (int i = 0; i < chunk_vec.size(); i++) {
    //     req_body.append(reinterpret_cast<char *>(chunk_vec[i].body),
    //     chunk_len);
    // }
}
