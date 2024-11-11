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
        log_info("-- Block size: {}", instance.GetBlockSize());
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

    static int GetQueueDepth() { return GetInstance().gQueueDepth; }

    static int GetContextPoolSize() { return GetInstance().gContextPoolSize; }

    static int GetNumOfTargets() { return GetInstance().gNumOfTargets; }

    static int GetNumOfDevices() { return GetInstance().gNumOfDevices; }

    static int GetMetadataSize() { return GetInstance().gMetadataSize; }

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

    // static uint32_t GetReceiverThreadCoreId()
    // {
    //     return GetInstance().gReceiverThreadCoreId;
    // }

    // static uint32_t GetIndexThreadCoreId()
    // {
    //     return GetInstance().gIndexThreadCoreId;
    // }

    static uint32_t GetDispatchThreadCoreId()
    {
        return GetInstance().gDispatchThreadCoreId;
    }

    // static uint32_t GetCompletionThreadCoreId()
    // {
    //     return GetInstance().gCompletionThreadCoreId;
    // }

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

    // static uint32_t GetTotalIo() { return GetInstance().gTotalIO; }

    static uint64_t GetZoneDist() { return GetInstance().gZoneDist; }

    static uint32_t GetDataBufferSizeInSector()
    {
        return GetInstance().gDataBufferSizeInSector;
    }

    static uint64_t GetZoneId1() { return GetInstance().gCurrentZone1; }
    static uint64_t GetZoneId2() { return GetInstance().gCurrentZone2; }

    // static uint64_t GetZslba()
    // {
    //     return GetInstance().gZoneDist * GetInstance().current_zone;
    // }

  private:
    // Hardcode because they won't change
    const uint64_t gZoneDist = 0x80000; // zone size

    uint64_t gStorageSpaceInBytes = 1024 * 1024 * 1024 * 1024ull; // 1TiB

    // NOTE this decides the number of threads *per device*, and the number of
    // IO queues are the same of num_of_IO_threads * num_of_device
    int gNumIoThreads = 1;
    int gNumHttpThreads = 6; // magic number

    uint32_t gDispatchThreadCoreId = 1;
    uint32_t gIoThreadCoreIdBase = 1;
    uint32_t gHttpThreadCoreIdBase = gIoThreadCoreIdBase + gNumIoThreads;

    int gBlockSize = 4096;
    int gMetadataSize = 64;
    bool gDeviceSupportMetadata = false;

    // int gZoneCapacity = 0;

    // Configured parameters
    int gQueueDepth = 1;
    int gContextPoolSize = 4096;

    // how many targets one gateway talks to
    int gNumOfTargets = 3;
    // how many devices/drives on a target
    int gNumOfDevices = 2;

    // 0 means no sampling, 1000 means 1/1000
    int gSamplingRate = 0;

    // manually set the zone id
    const int gCurrentZone1 = 148;
    const int gCurrentZone2 = 148;
    uint32_t gDataBufferSizeInSector = 1; // 1- 32

    bool gVerbose = false; // this will turn on all logs
    bool gDebug = false;   // this will turn on all checks

    bool gUseObject = false;
    // bool gUseDummyWorkload = false;
    bool gUseHttp = true;
    // TODO: use other spdk thread to work stealing
    bool gUseWorkStealing = false;
};
