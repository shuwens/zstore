#include "../include/object.h"
#include "../include/utils.h"
#include "../object.cc"
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>

int main()
{
    // Example of creating a ZstoreObject
    ZstoreObject obj;
    obj.entry.type = LogEntryType::kData;
    obj.datalen = 128; // Example data length
    obj.body = std::malloc(obj.datalen);
    std::memset(obj.body, 0xAB, obj.datalen); // Fill with example data
    std::strcpy(obj.key_hash, "example_key_hash");
    obj.key_size = static_cast<uint16_t>(std::strlen(obj.key_hash));

    // Serialize to buffer
    auto buffer = WriteZstoreObjectToBuffer(obj);

    // Deserialize back to ZstoreObject
    ZstoreObject new_obj;
    if (ReadBufferToZstoreObject(buffer.data(), buffer.size(), new_obj)) {
        log_info("Deserialization successful!");
        // Clean up the dynamically allocated memory
        if (new_obj.body)
            std::free(new_obj.body);
    } else {
        log_info("Deserialization failed!");
    }

    // Clean up
    if (obj.body)
        std::free(obj.body);

    return 0;
}
