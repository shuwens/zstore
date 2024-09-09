#pragma once
#include "utils.hpp"
#include <string>

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

    // static int GetMetadataSize() { return GetInstance().gMetadataSize; }

    static int GetNumIoThreads() { return GetInstance().gNumIoThreads; }

    // static bool GetDeviceSupportMetadata()
    // {
    //     return GetInstance().gDeviceSupportMetadata;
    // }

    // static void SetDeviceSupportMetadata(bool flag)
    // {
    //     GetInstance().gDeviceSupportMetadata = flag;
    // }

    static uint32_t GetReceiverThreadCoreId()
    {
        return GetInstance().gReceiverThreadCoreId;
    }

    static uint32_t GetHttpThreadCoreId()
    {
        return GetInstance().gHttpThreadCoreId;
    }

    static uint32_t GetIndexThreadCoreId()
    {
        return GetInstance().gIndexThreadCoreId;
    }

    static uint32_t GetDispatchThreadCoreId()
    {
        return GetInstance().gDispatchThreadCoreId;
    }

    static uint32_t GetCompletionThreadCoreId()
    {
        return GetInstance().gCompletionThreadCoreId;
    }

    static uint32_t GetIoThreadCoreId()
    {
        return GetInstance().gIoThreadCoreIdBase;
    }

    static uint32_t GetIoThreadCoreId(uint32_t thread_id)
    {
        return GetInstance().gIoThreadCoreIdBase + thread_id;
    }

    static void SetStorageSpaceInBytes(uint64_t storageSpaceInBytes)
    {
        GetInstance().gStorageSpaceInBytes = storageSpaceInBytes;
    }

    static uint64_t GetStorageSpaceInBytes()
    {
        return GetInstance().gStorageSpaceInBytes;
    }

    // static void SetL2PTableSizeInBytes(uint64_t bytes)
    // {
    //     GetInstance().gL2PTableSize = bytes;
    // }

    // static uint64_t GetL2PTableSizeInBytes()
    // {
    //     return GetInstance().gL2PTableSize;
    // }

  private:
    // uint32_t gRebootMode = 0; // 0: new, 1: restart, 2: rebuild.

    // StripeConfig *gStripeConfig = new StripeConfig[1];
    int gBlockSize = 4096;
    int gMetadataSize = 64;
    int gNumIoThreads = 1;
    // bool gDeviceSupportMetadata = true;
    int gZoneCapacity = 0;

    uint64_t gStorageSpaceInBytes = 1024 * 1024 * 1024 * 1024ull; // 1TiB

    // SystemMode gSystemMode = ZAPRAID;

    uint32_t gReceiverThreadCoreId = 3;
    uint32_t gDispatchThreadCoreId = 4;
    // Not used for now; functions collocated with dispatch thread.
    uint32_t gCompletionThreadCoreId = 5;
    uint32_t gIndexThreadCoreId = 6;
    uint32_t gHttpThreadCoreId = 7;
    uint32_t gIoThreadCoreIdBase = 8;

    int gLargeRequestThreshold = 16 * 1024;
};
