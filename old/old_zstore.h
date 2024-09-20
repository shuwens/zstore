#pragma once
#include "global.h"
#include "object.h"
#include "spdk/bdev.h"
#include "spdk/bdev_zone.h"
#include "spdk/env.h"
#include "spdk/event.h"
#include "spdk/log.h"
#include "spdk/stdinc.h"
#include "spdk/string.h"
#include "spdk/thread.h"
#include "utils.hpp"
#include <atomic>
#include <cstdint>
#include <iostream>
#include <unistd.h>
#include <unordered_map>
#include <vector>

typedef std::pair<std::string, int32_t> TargetLba;

struct zstore_context_t {
    // spdk
    struct spdk_bdev *bdev;
    struct spdk_bdev_desc *bdev_desc;
    struct spdk_io_channel *bdev_io_channel;
    struct spdk_bdev_io_wait_entry bdev_io_wait;

    // buffers
    char *write_buff;
    char *read_buff;
    uint32_t buff_size;
    char *bdev_name;
    u64 zone_num;

    // atomic count for currency
    std::atomic<int> count;
};

/**
 * @class Zstore
 * @brief Represents a Zstore object.
 *
 * The Zstore class is a subclass of CivetHandler and provides functionality for
 * managing Zstore objects. It allows setting the verbosity level and provides a
 * destructor.
 */
class Zstore // : public CivetHandler
{
  private:
    std::string name;
    int verbose;

    std::unordered_map<std::string, ObjectMetadata>
        metadata_index; // Hash Table
    // Alternatively, use a B+ Tree for range queries
    // BPlusTree<std::string, ObjectMetadata> metadata_index;

    // object tables, used only by zstore
    // key -> tuple of <zns target, lba>
    std::unordered_map<std::string, std::vector<TargetLba>> gateway_map;
    // std::mutex obj_table_mutex;
    std::vector<LogEntry> target_log;

    // TODO: missing initialization to read lba for each target. Use the read
    // write code.
    int32_t start_lba = 0;

    // Functions for SSD I/O operations
    void writeMetadata(const ObjectMetadata &metadata, uint64_t &offset);
    void readMetadata(uint64_t offset, ObjectMetadata &metadata);
    void writeData(const std::string &data, uint64_t &offset);
    void readData(uint64_t offset, std::string &data);

  public:
    void putObject(const std::string &key, const std::string &data);
    std::string getObject(const std::string &key);
    void deleteObject(const std::string &key);

    constexpr int32_t get_lba() { return start_lba; };
    bool set_lba(int32_t last_lba)
    {
        start_lba = last_lba;
        return true;
    };

    /**
     * @brief Constructs a Zstore object with the specified name.
     *
     * @param name The name of the Zstore object.
     */
    Zstore(const std::string &name) : name(name){};

    /**
     * @brief Destroys the Zstore object.
     */
    ~Zstore(){};

    /**
     * @brief Sets the verbosity level of the Zstore object.
     *
     * @param v The verbosity level to set.
     */
    void SetVerbosity(int v) { verbose = v; }

    // std::list<AWS_S3_Bucket> buckets;
};
