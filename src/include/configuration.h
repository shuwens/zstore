#pragma once
#include "utils.h"

class Configuration
{
  public:
    static Configuration &GetInstance()
    {
        static Configuration instance;
        return instance;
    }

    static void PrintConfigurations()
    {
        Configuration &instance = GetInstance();
        // const char *systemModeStrs[] = {"ZoneWrite-Only", "ZoneAppend-Only",
        //                                 "ZapRAID", "RAIZN-Simple"};
        // printf("ZapRAID Configuration:\n");
        log_info(
            "-- Block size: {}, Number of targets {}, Number of devices {}",
            instance.GetBlockSize(), instance.GetNumOfTargets(),
            instance.GetNumOfDevices());
        // for (int i = 0; i < instance.gNumOpenSegments; i++) {
        //     printf("-- Raid mode: %d %d %d %d %d | %d--\n",
        //            instance.gStripeConfig[i].size,
        //            instance.gStripeConfig[i].dataSize,
        //            instance.gStripeConfig[i].paritySize,
        //            instance.gStripeConfig[i].unitSize,
        //            instance.gStripeConfig[i].groupSize,
        //            instance.gRaidScheme);
        // }
        // printf("-- System mode: %s --\n",
        //        systemModeStrs[(int)instance.gSystemMode]);
        // printf("-- GC Enable: %d --\n", instance.gEnableGc);
        // printf("-- Framework Enable: %d --\n",
        // instance.gEnableEventFramework);
        log_info("-- Storage size: {} -- ({} GiB)\n",
                 instance.gStorageSpaceInBytes,
                 instance.gStorageSpaceInBytes / 1024 / 1024 / 1024);
    }

    static void SetBlockSize(int blockSize)
    {
        GetInstance().gBlockSize = blockSize;
    }

    static int GetBlockSize() { return GetInstance().gBlockSize; }

    // static int GetQueueDepth() { return GetInstance().gQueueDepth; }

    static int GetContextPoolSize() { return GetInstance().gContextPoolSize; }

    static void SetNumOfTargets(int new_targets)
    {
        GetInstance().gNumOfTargets = new_targets;
    }

    static int GetNumOfTargets() { return GetInstance().gNumOfTargets; }

    static int GetNumOfDevices() { return GetInstance().gNumOfDevices; }

    static int GetMetadataSize() { return GetInstance().gMetadataSize; }

    static u64 GetChunkSize() { return GetInstance().gChunkSize; }

    static int GetNumIoThreads() { return GetInstance().gNumIoThreads; }

    static int GetNumHttpThreads() { return GetInstance().gNumHttpThreads; }

    static int GetSamplingRate() { return GetInstance().gSamplingRate; }

    static bool Verbose() { return GetInstance().gVerbose; }
    static bool Debugging() { return GetInstance().gDebug; }
    // static bool UseDummyWorkload() { return GetInstance().gUseDummyWorkload;
    // }
    static bool UseObject() { return GetInstance().gUseObject; }
    static bool UseHttp() { return GetInstance().gUseHttp; }
    static bool UseWorkStealing() { return GetInstance().gUseWorkStealing; }

    static bool GetDeviceSupportMetadata()
    {
        return GetInstance().gDeviceSupportMetadata;
    }

    static void SetDeviceSupportMetadata(bool flag)
    {
        GetInstance().gDeviceSupportMetadata = flag;
    }

    static uint32_t GetIoThreadCoreId()
    {
        return GetInstance().gIoThreadCoreIdBase;
    }

    static uint32_t GetIoThreadCoreId(uint32_t thread_id)
    {
        return GetInstance().gIoThreadCoreIdBase + thread_id;
    }

    static uint32_t GetHttpThreadCoreId()
    {
        return GetInstance().gHttpThreadCoreIdBase;
    }

    static uint32_t GetHttpThreadCoreId(uint32_t thread_id)
    {
        return GetInstance().gHttpThreadCoreIdBase + thread_id;
    }

    static void SetStorageSpaceInBytes(uint64_t storageSpaceInBytes)
    {
        GetInstance().gStorageSpaceInBytes = storageSpaceInBytes;
    }

    static uint64_t GetStorageSpaceInBytes()
    {
        return GetInstance().gStorageSpaceInBytes;
    }

    // NOTE
    // zone size is     0x80000: 524288
    // zone capacity is 0x43500: 275456

    static uint64_t GetZoneDist() { return GetInstance().gZoneDist; }
    static uint64_t GetZoneCap() { return GetInstance().gZoneCap; }

    // static uint32_t GetDataBufferSizeInSector()
    // {
    //     return GetInstance().gDataBufferSizeInSector;
    // }

    static uint32_t GetObjectSizeInBytes()
    {
        return GetInstance().gObjectSizeInBytes;
    }

    static uint64_t GetZoneId() { return GetInstance().gCurrentZone; }

    static std::string GetZkHost() { return GetInstance().gZookeeperUrl; }

  private:
    // Hardcode because they won't change
    const uint64_t gZoneDist = 524288; // zone size: 0x80000
    const uint64_t gZoneCap = 275456;  // zone capacity: 0x43500

    uint64_t gStorageSpaceInBytes = 1024 * 1024 * 1024 * 1024ull; // 1TiB

    // NOTE this decides the number of SPDK threads
    int gNumIoThreads = 1;
    int gNumHttpThreads = 6; // magic number

    uint32_t gIoThreadCoreIdBase = 1;
    uint32_t gHttpThreadCoreIdBase = gIoThreadCoreIdBase + gNumIoThreads;

    // We dont use metadata
    int gMetadataSize = 0;
    bool gDeviceSupportMetadata = false;
    int gBlockSize = 4096;

    // Configured parameters
    int gContextPoolSize = 4096;
    u64 gChunkSize = 4096 * 32; // 4KB

    // how many targets one gateway talks to
    int gNumOfTargets = 4;
    // how many devices/drives on a target
    int gNumOfDevices = 2;

    // 0 means no sampling, 1000 means 1/1000
    int gSamplingRate = 0;

    // manually set the zone id
    const int gCurrentZone = 27; // write
    // const int gCurrentZone = 0;
    uint32_t gObjectSizeInBytes = 4096; // 4kB
    // uint32_t gObjectSizeInBytes = 4096 * 1024; // 4MB
    // uint32_t gObjectSizeInBytes = 4096 * 32;

    std::string gZookeeperUrl = "12.12.12.1:2181";

    // FIXME:
    //
    // large object: single run can go to 128 blocks
    // http server can only go to 16 blocks
    //
    // read is 64 blocks?

    bool gVerbose = false; // this will turn on all logs
    bool gDebug = false;   // this will turn on all checks and log

    // bool gUseDummyWorkload = false;
    bool gUseObject = false;
    bool gUseHttp = true;
    // TODO: use other spdk thread to work stealing
    bool gUseWorkStealing = false;
};
