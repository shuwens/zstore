#include "../include/object.h"
#include "../include/utils.h"
#include "../object.cc"
#include <cassert>
#include <cassert> // For assert
#include <cstring>
#include <iostream>

// Assuming the previous implementation of ZstoreObject, toBuffer, and
// fromBuffer is available

void testSerializationDeserialization(int datalen)
{
    // 1. Create and initialize a ZstoreObject
    ZstoreObject original_obj;
    original_obj.entry.type = LogEntryType::kData;
    original_obj.entry.seqnum = 42;
    original_obj.entry.chunk_seqnum = 24;
    original_obj.datalen = datalen; // Example data length
    original_obj.body = std::malloc(original_obj.datalen);
    std::memset(original_obj.body, 0xCD,
                original_obj.datalen); // Fill with example data (0xCD)
    std::strcpy(original_obj.key_hash, "test_key_hash");
    original_obj.key_size =
        static_cast<uint16_t>(std::strlen(original_obj.key_hash));

    // 2. Serialize to buffer
    auto buffer = WriteZstoreObjectToBuffer(original_obj);

    // 3. Deserialize back to a new ZstoreObject
    ZstoreObject deserialized_obj;
    bool success = ReadBufferToZstoreObject(buffer.data(), buffer.size(),
                                            deserialized_obj);

    // 4. Check if deserialization succeeded
    assert(success && "Deserialization failed!");

    // 5. Compare original and deserialized objects
    assert(original_obj.entry.type == deserialized_obj.entry.type);
    assert(original_obj.entry.seqnum == deserialized_obj.entry.seqnum);
    assert(original_obj.entry.chunk_seqnum ==
           deserialized_obj.entry.chunk_seqnum);
    assert(original_obj.datalen == deserialized_obj.datalen);
    assert(original_obj.key_size == deserialized_obj.key_size);
    assert(std::strcmp(original_obj.key_hash, deserialized_obj.key_hash) == 0);

    // 6. Compare the body contents
    if (original_obj.datalen > 0 && original_obj.body &&
        deserialized_obj.body) {
        assert(std::memcmp(original_obj.body, deserialized_obj.body,
                           original_obj.datalen) == 0);
    }

    // Clean up dynamically allocated memory
    if (original_obj.body)
        std::free(original_obj.body);
    if (deserialized_obj.body)
        std::free(deserialized_obj.body);

    // If all assertions pass
    log_info("Test passed: Serialization and deserialization are correct!  "
             "Data length: {}",
             datalen);
}

int main()
{
    // Run the test
    testSerializationDeserialization(128);
    testSerializationDeserialization(1024);
    testSerializationDeserialization(4096);
    testSerializationDeserialization(4096 * 4);
    testSerializationDeserialization(4096 * 16);

    return 0;
}
