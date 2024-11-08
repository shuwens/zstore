#include "include/object.h"
#include "include/zstore_controller.h"
#include <boost/outcome/success_failure.hpp>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <memory>
#include <sstream>
#include <stdlib.h>
#include <string.h>
#include <string>

void sha256_hash_string(unsigned char hash[SHA256_DIGEST_LENGTH],
                        char outputBuffer[65])
{
    int i = 0;

    for (i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        sprintf(outputBuffer + (i * 2), "%02x", hash[i]);
    }

    outputBuffer[64] = 0;
}

void sha256_string(const char *string, char outputBuffer[65])
{
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, string, strlen(string));
    SHA256_Final(hash, &sha256);
    int i = 0;
    for (i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        sprintf(outputBuffer + (i * 2), "%02x", hash[i]);
    }
    outputBuffer[64] = 0;
}

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

bool ReadBufferToZstoreObject(const uint8_t *buffer, size_t buffer_size,
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

std::vector<uint8_t> WriteZstoreObjectToBuffer(const ZstoreObject &obj)
{
    // Calculate the total size
    size_t total_size = sizeof(LogEntry) + sizeof(obj.key_hash) +
                        sizeof(uint16_t) + sizeof(uint64_t) + obj.datalen;

    // Create a buffer
    std::vector<uint8_t> buffer(total_size);
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
