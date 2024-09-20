#include "../include/object.h"
#include "../object.cc"
#include <cstdint>
#include <cstring>
#include <iostream>

int main()
{
    // Example data
    char key[] = "example_key";
    char body[] = "This is the body data.";
    uint16_t key_size = sizeof(key) - 1; // Exclude null terminator
    uint64_t datalen = sizeof(body) - 1; // Exclude null terminator

    // Allocate ZstoreObject and fill data
    ZstoreObject *obj =
        (ZstoreObject *)malloc(ZstoreObject::size(key_size, datalen));
    obj->entry.next_offset = 0;
    obj->entry.seqnum = 1;
    obj->entry.chunk_seqnum = 1;
    obj->entry.next = 0;
    obj->entry.key_size = key_size;
    obj->datalen = datalen;
    obj->body = malloc(datalen);
    std::memcpy(obj->body, body, datalen);
    std::memcpy(obj->key, key, key_size);

    // Buffer to write to (4096 bytes)
    char buffer[4096];

    // Write the object to the buffer
    if (write_to_buffer(obj, buffer, sizeof(buffer))) {
        std::cout << "ZstoreObject written to buffer successfully."
                  << std::endl;
    }

    // Read the object back from the buffer
    ZstoreObject *read_obj = read_from_buffer(buffer, sizeof(buffer));

    std::cout << "Read ZstoreObject:" << std::endl;
    std::cout << "Key: " << std::string(read_obj->key, read_obj->entry.key_size)
              << std::endl;
    std::cout << "Body: "
              << std::string(reinterpret_cast<char *>(read_obj->body),
                             read_obj->datalen)
              << std::endl;

    // Free allocated memory
    free(obj->body);
    free(obj);
    free(read_obj->body);
    free(read_obj);

    return 0;
}
