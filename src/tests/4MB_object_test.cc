#include "../include/object.h"
#include "../include/utils.h"
#include "../object.cc"

// Assuming the previous implementation of ZstoreObject, toBuffer, and
// fromBuffer is available

// Helper function to take a larger object and split it into chunks
// of 4KB each
std::vector<ZstoreObject> splitDummyObjectIntoChunks(ZstoreObject obj)
{
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
        std::memcpy(chunk.body, "A", chunk.datalen);
        std::strcpy(chunk.key_hash, obj.key_hash);
        chunk.key_size = obj.key_size;
        chunk_vec.push_back(chunk);
    }

    return chunk_vec;
}

// Helper function to merge chunks into a single object
// ZstoreObject mergeDummyChunksIntoObject(std::vector<ZstoreObject> chunk_vec)
// {
//     ZstoreObject obj;
//     obj.entry.type = chunk_vec[0].entry.type;
//     obj.entry.seqnum = chunk_vec[0].entry.seqnum;
//     obj.entry.chunk_seqnum = 0;
//     obj.datalen = 0;
//     for (auto &chunk : chunk_vec) {
//         obj.datalen += chunk.datalen;
//     }
//     obj.body = std::malloc(obj.datalen);
//     u64 offset = 0;
//     for (auto &chunk : chunk_vec) {
//         // std::memcpy(chunk.body, "A", chunk.datalen);
//         std::memcpy(obj.body + offset, "A", chunk.datalen);
//         offset += chunk.datalen;
//     }
//     std::strcpy(obj.key_hash, chunk_vec[0].key_hash);
//     obj.key_size = chunk_vec[0].key_size;
//     return obj;
// }

// NOTE that we need to test the serialization and deserialization of
// ZstoreObject with the maximum data length (32 of 4K blocks)
void testObjectsWithMdts(u64 datalen)
{
    // calculate the number of chunks
    u64 num_chunks = datalen / Configuration::GetChunkSize();
    u64 remaining_data_len = datalen;
    log_info(
        "Testing objects with data length: {} bytes, {} MB, num of chunks {}",
        datalen, datalen / 1024 / 1024, num_chunks);

    // make fake object
    ZstoreObject obj;
    obj.entry.type = LogEntryType::kData;
    obj.entry.seqnum = 42;
    obj.entry.chunk_seqnum = 0;
    obj.datalen = datalen;
    obj.body = std::malloc(obj.datalen);
    std::memset(obj.body, 'A', obj.datalen);
    obj.key_size = std::strlen("test_key_hash");

    // get all chunks
    std::vector<ZstoreObject> chunk_vec = splitDummyObjectIntoChunks(obj);
    log_info("Chunk vec size: {}", chunk_vec.size());
    std::free(obj.body);

    // Skipped: write every zstore object into NVMe device

    // make fake object
    ZstoreObject chunk_header;
    chunk_header.entry.type = LogEntryType::kHeader;
    chunk_header.entry.seqnum = 42;
    chunk_header.entry.chunk_seqnum = 0;
    chunk_header.datalen = Configuration::GetChunkSize();
    chunk_header.body = std::malloc(obj.datalen);
    chunk_header.key_size = std::strlen("test_key_hash");
    log_info("Chunk header size: {}", sizeof(chunk_header));

    // TODO: why is std map default size 48?
    // write the chunk list
    ChunkList chunk_list;
    log_info("Chunk list size: {}", sizeof(chunk_list));
    for (u64 i = 0; i < num_chunks; i++) {
        // log_info("Chunk list: {} {}", i, kChunkSize);
        chunk_list[i] = std::make_tuple(i, Configuration::GetChunkSize());
    }
    chunk_header.body = serializeMap(chunk_list, chunk_header.datalen);
    log_info("Chunk list size: {}", sizeof(chunk_list));

    // Skipped: write chunk header zstore object into NVMe device

    //
    // Read data object from NVMe device
    //
    ChunkList chunk_list_read = deserializeMap(chunk_header.body);
    assert(chunk_list.size() == chunk_list_read.size());
    for (u64 i = 0; i < num_chunks; i++) {
        assert(chunk_list[i] == chunk_list_read[i]);
    }

    std::vector<ZstoreObject> deserialized_obj_vec;
    remaining_data_len = datalen;
    for (u64 i = 0; i < num_chunks; i++) {

        // 2. Serialize to buffer
        auto buffer = WriteZstoreObjectToBuffer(chunk_vec[i]);

        // 3. Deserialize back to a new ZstoreObject
        ZstoreObject deserialized_obj;
        bool success = ReadBufferToZstoreObject(buffer.data(), buffer.size(),
                                                deserialized_obj);
        // 4. Check if deserialization succeeded
        assert(success && "Deserialization failed!");

        deserialized_obj_vec.push_back(deserialized_obj);
    }

    for (u64 i = 0; i < num_chunks; i++) {
        // 5. Compare original and deserialized objects
        assert(chunk_vec[i].entry.type == deserialized_obj_vec[i].entry.type);
        assert(chunk_vec[i].entry.seqnum ==
               deserialized_obj_vec[i].entry.seqnum);
        assert(chunk_vec[i].entry.chunk_seqnum ==
               deserialized_obj_vec[i].entry.chunk_seqnum);
        assert(chunk_vec[i].datalen == deserialized_obj_vec[i].datalen);
        assert(chunk_vec[i].key_size == deserialized_obj_vec[i].key_size);
        assert(std::strcmp(chunk_vec[i].key_hash,
                           deserialized_obj_vec[i].key_hash) == 0);

        // 6. Compare the body contents
        if (chunk_vec[i].datalen > 0 && chunk_vec[i].body &&
            deserialized_obj_vec[i].body) {
            assert(std::memcmp(chunk_vec[i].body, deserialized_obj_vec[i].body,
                               chunk_vec[i].datalen) == 0);
        }

        // Clean up dynamically allocated memory
        if (chunk_vec[i].body)
            std::free(chunk_vec[i].body);
        if (deserialized_obj_vec[i].body)
            std::free(deserialized_obj_vec[i].body);

        log_info("Test passed for chunk {}", i);
    }
    // If all assertions pass
    log_info("Test passed: Serialization and deserialization are correct!  "
             "Data length: {}\n",
             datalen);
}

int main()
{
    // Run the test
    testObjectsWithMdts(4096 * 1024);            // 4 MB
    testObjectsWithMdts(4096 * 1024 * 8);        // 32 MB
    testObjectsWithMdts(4096 * 1024 * 8 * 2);    // 64 MB
    testObjectsWithMdts(4096 * 1024 / 4 * 256);  // 256 MB
    testObjectsWithMdts(4096 * 1024 * 1024 / 4); // 1 GB: broken

    return 0;
}
