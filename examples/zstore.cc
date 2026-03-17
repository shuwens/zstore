#include "include/zstore.h"
// #include "global.h"
// #include "helper.h"
// #include <CivetServer.h>
#include <fstream>
#include <string>
#include <unistd.h>
#include <unordered_map>

void Zstore::putObject(const std::string &key, const std::string &data)
{
    log_info("PUT: key {}", key);
    ObjectMetadata metadata;
    metadata.key = key;
    metadata.size = data.size();
    metadata.creation_date = time(nullptr);
    metadata.modification_date = metadata.creation_date;

    log_info("PUT: key {} in gateway map", key);
    TargetLba target_lba1;
    target_lba1.first = "tgt1";
    target_lba1.second = get_lba();
    TargetLba target_lba2;
    target_lba2.first = "tgt2";
    target_lba2.second = get_lba();
    TargetLba target_lba3;
    target_lba3.first = "tgt3";
    target_lba3.second = get_lba();
    std::vector<TargetLba> target_lbas = {target_lba1, target_lba2,
                                          target_lba3};
    gateway_map[key] = target_lbas;

    log_info("PUT: key {} in target log", key);
    LogEntry entry;
    entry.key = key;
    entry.seq = 0;
    entry.chunk_seq = 0;
    entry.lba = get_lba();
    target_log.push_back(entry);

    // Write data to SSD
    writeData(data, metadata.data_offset);

    // Write metadata to SSD and update metadata index
    uint64_t metadata_offset;
    writeMetadata(metadata, metadata_offset);
    metadata_index[key] = metadata;
}

std::string Zstore::getObject(const std::string &key)
{
    log_info("GET: key {}", key);

    if (metadata_index.find(key) == metadata_index.end()) {
        throw std::runtime_error("Object not found");
    }
    ObjectMetadata metadata = metadata_index[key];
    std::string data;
    readData(metadata.data_offset, data);
    return data;
}

void Zstore::deleteObject(const std::string &key)
{

    log_info("DELETE: key {}", key);

    if (metadata_index.find(key) == metadata_index.end()) {
        throw std::runtime_error("Object not found");
    }
    metadata_index.erase(key);
    // Optionally, mark the data on SSD as deleted for garbage collection
}

// SSD

void Zstore::writeMetadata(const ObjectMetadata &metadata, uint64_t &offset)
{
    std::ofstream ofs("metadata_store.ssd", std::ios::binary | std::ios::app);
    offset = ofs.tellp();
    ofs.write(reinterpret_cast<const char *>(&metadata), sizeof(metadata));
    ofs.close();
}

void Zstore::readMetadata(uint64_t offset, ObjectMetadata &metadata)
{
    std::ifstream ifs("metadata_store.ssd", std::ios::binary);
    ifs.seekg(offset);
    ifs.read(reinterpret_cast<char *>(&metadata), sizeof(metadata));
    ifs.close();
}

void Zstore::writeData(const std::string &data, uint64_t &offset)
{
    std::ofstream ofs("data_store.ssd", std::ios::binary | std::ios::app);
    offset = ofs.tellp();
    ofs.write(data.c_str(), data.size());
    ofs.close();
}

void Zstore::readData(uint64_t offset, std::string &data)
{
    std::ifstream ifs("data_store.ssd", std::ios::binary);
    ifs.seekg(offset);
    std::getline(ifs, data, '\0');
    ifs.close();
}
