#pragma once

#include "conn.hpp"
#include "endpoint.hpp"
#include "error.hpp"
#include "magic_buffer.hpp"
#include "types.h"
#include <boost/asio/awaitable.hpp>
#include <boost/circular_buffer.hpp>
#include <boost/unordered/concurrent_flat_map.hpp>
#include <boost/unordered/concurrent_flat_set.hpp>

#include <cstring>
#include <queue>
#include <shared_mutex>
#include <spdk/env.h>
#include <spdk/thread.h>
#include <thread>
#include <unistd.h>

#define THREADED 1
#include <zookeeper/zookeeper.h>

namespace asio = boost::asio; // from <boost/asio.hpp>

// We store the SHA256 hash of the object key as the key in the Zstore Map, and
// the value in the Zstore Map is the tuple of the target device and the LBA
using ZstoreMap =
    boost::concurrent_flat_map<ObjectKeyHash, MapEntry, ArrayHash>;

// We record hash of recent writes as a concurrent hashmap, where the key is
// the hash, and the value is the gateway of the writes.
using ZstoreRecentWriteMap = boost::concurrent_flat_map<ObjectKeyHash, u8>;

// We record the target device and LBA of the blocks that we need to GC
using ZstoreGcSet = boost::concurrent_flat_set<TargetLbaTuple>;

// We use a circular buffer to store RDMA writes, note that we have two designs
// for this. We are choosing the first design for now, as it is more efficient:
// 1. use a 64 bytes entry for the circular buffer, where the upper 32 bytes is
//    the hash of object key, and the lower 32 bytes stores the current epoch,
//    and the value is the target device and LBA
// 2. use a 64 bits entry for the circular buffer, where the upper 31 bytes is
//   the hash of object key, and the lower bit signals the epoch change
//  (0: no change, 1: epoch change), and the value is the target device and LBA
using RdmaBuffer = boost::circular_buffer<BufferEntry>;

class Device;
class Zone;

class ZstoreController
{
  public:
    int mKeyExperiment;
    // 1: Random Read
    // 2: Sequential write (append) and read
    // 3: Checkpoint
    // 4: Target failure
    // 5: gateway failure
    // 6: Target and gateway failure
    // 7: GC

    int mOption;

    // Zookeeper
    Result<void> ZookeeperJoin();
    Result<void> ZookeeperElect();
    Result<void> ZookeeperAnnounce();

    /* NOTE workflow for persisting map
     * 1. zookeeper will select a server (leader) perform checkpoint
     * 2. leader will announce epoch change to all servers (N -> N+1). Each
     *    follower will (1) create new map and new bloom filter for N+1, and
     *    (2) return list of writes and wp for each device
     */
    Result<void> Checkpoint();

    int PopulateMap();
    Result<void> DumpAllMap();
    Result<void> ReadAllMap();
    void writeMapToFile(const std::string &filename);
    void readMapFromFile(const std::string &filename);

    int ReadZoneHeaders();
    int SetupZookeeper();
    int SetupHttpThreads();
    Result<void> SendRecordsToGateway();

    int PopulateDevHash();
    Result<DevTuple> GetDevTuple(ObjectKeyHash object_key_hash);
    Result<DevTuple> GetDevTupleForRandomReads(ObjectKeyHash key_hash);

    int pivot;
    int queue_depth = 0;

    // ZStore Device Consistent Hashmap: this maintains a consistent hash map
    // which maps object key to tuple of devices. Right now this is
    // pre-populated and just randomly
    std::vector<DevTuple> mDevHash;
    std::shared_mutex mDevHashMutex;

    // ZStore Map: this maps key to tuple of ZNS target and lba
    ZstoreMap mMap;
    // Map APIs
    Result<bool> PutObject(const ObjectKeyHash &key_hash, MapEntry entry);
    std::optional<MapEntry> GetObject(const ObjectKeyHash &key_hash);
    Result<std::vector<ObjectKeyHash>> ListObjects();
    Result<MapEntry> CreateFakeObject(ObjectKeyHash key_hash, DevTuple tuple);
    Result<MapEntry> DeleteObject(const ObjectKeyHash &key_hash);

    // ZStore Bloom Filter: this maintains a bloom filter of hashes of
    // object name (key).
    //
    // For simplicity, right now we are just using a hash map set to keep track
    // of the hashes
    ZstoreRecentWriteMap mRecentWriteMap;
    // Bloomfilter APIs
    Result<bool> SearchRecentWriteMap(const ObjectKeyHash &key_hash);
    Result<bool> UpdateRecentWriteMap(const ObjectKeyHash &key_hash);

    // ZStore GC Map: we keep tracks of blocks that we need to GC. Note that we
    // can potentially optimize this to be per zone tracking, which will help
    // scaning it
    ZstoreGcSet mGcSet;
    Result<bool> AddGcObject(const TargetLbaTuple &tuple);

    ZstoreController(asio::io_context &ioc) : mIoc_(ioc) {};
    // The io_context is required for all I/O
    asio::io_context &mIoc_;

    ~ZstoreController();
    int Init(bool object, int key_experiment, int option);

    // threads
    void initIoThread();
    struct spdk_thread *GetIoThread(int id) { return mIoThread[id].thread; };

    // SPDK components
    struct spdk_nvme_qpair *GetIoQpair();
    bool CheckIoQpair(std::string msg);
    bool CheckIoThread(std::string msg);
    int GetQueueDepth() { return mQueueDepth; };
    void setQueuDepth(int queue_depth) { mQueueDepth = queue_depth; };
    void setKeyExperiment(int key) { mKeyExperiment = key; };
    void setOption(int option) { mOption = option; };

    void SetEventPoller(spdk_poller *p) { mEventsPoller = p; }
    int ConfigureSpdkQpairs();

    int GetContextPoolSize() { return mContextPoolSize; };
    void setContextPoolSize(int context_pool_size)
    {
        mContextPoolSize = context_pool_size;
    };

    void setNumOfDevices(int num_of_device) { mN = num_of_device; };

    int SetParameters(int key_experiment, int option);

    // Setting up SPDK
    void register_ctrlr(std::vector<Device *> &g_devices,
                        struct spdk_nvme_ctrlr *ctrlr, const char *traddr,
                        const uint32_t zone_id1, const uint32_t zone_id2);
    void register_ns(struct spdk_nvme_ctrlr *ctrlr, struct spdk_nvme_ns *ns);

    int register_workers();
    int register_controllers(
        std::vector<Device *> &g_devices,
        const std::tuple<std::string, std::string, std::string, u32, u32>
            &dev_tuple);
    void unregister_controllers(std::vector<Device *> &g_devices);
    void zstore_cleanup();
    void zns_dev_init(std::vector<Device *> &g_devices,
                      const std::tuple<std::string, std::string, std::string,
                                       u32, u32> &dev_tuple);

    int associate_workers_with_ns(Device *device);
    void cleanup_ns_worker_ctx();
    void cleanup(uint32_t task_count);
    int init_ns_worker_ctx(struct ns_worker_ctx *ns_ctx,
                           enum spdk_nvme_qprio qprio);

    void Drain();
    void ReclaimContexts();
    void Flush();
    void Dump();

    void Reset(RequestContext *ctx);
    bool IsResetDone();

    void AddZone(Zone *zone);
    const std::vector<Zone *> &GetZones();
    void PrintStats();

    bool start = false;
    chrono_tp stime;

    RequestContextPool *mRequestContextPool;
    // std::unordered_set<RequestContext *> mInflightRequestContext;

    // bool verbose;
    bool isDraining;

    // debug
    // std::map<uint32_t, uint64_t> mReadCounts;
    // uint64_t mTotalReadCounts = 0;
    uint64_t mTotalCounts = 0;
    uint64_t mManagementCounts = 0;

    // Device API: getters
    Device *GetDevice(const std::string &target_dev)
    {
        // NOTE currently we don't create mapping between target device to
        // Device object, as such mapping is static. As such we manually do a
        // switch on all target device cases
        if (target_dev == "Zstore2Dev1") {
            return mDevices[0];
        } else if (target_dev == "Zstore2Dev2") {
            return mDevices[1];
        } else if (target_dev == "Zstore3Dev1") {
            return mDevices[2];
        } else if (target_dev == "Zstore3Dev2") {
            return mDevices[3];
        } else if (target_dev == "Zstore4Dev1") {
            return mDevices[4];
        } else if (target_dev == "Zstore4Dev2") {
            return mDevices[5];
        } else if (target_dev == "Zstore5Dev1") {
            return mDevices[6];
        } else if (target_dev == "Zstore5Dev2") {
            return mDevices[7];
        } else {
            log_error("target device does not exist {}", target_dev);
            return nullptr;
        }
    };

    Device *GetDevice(const unsigned long &index)
    {
        if (index >= mDevices.size()) {
            log_error("Get Device: index out of bound {}", index);
            return nullptr;
        }
        return mDevices[index];
    };

    void SetGateway(u8 gateway) { mGateway = gateway; };
    u8 GetGateway() { return mGateway; };
    void ZkSet(const std::string &path, const char *data);
    void Map2Tx(const ZstoreMap &hashmap, std::vector<char *> &tx_map);

    // zookeeper handler: these have to be public
    void checkChildrenChange();
    void checkTxChange();
    // void watchPredecessor(zhandle_t *zzh, int type, int state, const char
    // *path,
    //                       void *watcherCtx);
    void createZnodes();
    void startZooKeeper();
    std::string getNodeData(const std::string &path);

    // Keeping track of the connection state
    int mZkConnected;
    int mZkExpired;
    // *zkHandler handles the connection with Zookeeper
    zhandle_t *mZkHandler;
    // std::string currentNodePath;
    std::string nodeName_;
    // std::string predecessorNodePath;
    std::string leaderNodeName_;

    // epoch
    u8 GetEpoch() { return mEpoch; };
    void SetEpoch(u8 epoch) { mEpoch = epoch; };

    // RDMA buffers: 64 entry
    kym::endpoint::Options mRdmaOpts = {
        .qp_attr =
            {
                .cap =
                    {
                        .max_send_wr = 1,
                        .max_recv_wr = 1,
                        .max_send_sge = 1,
                        .max_recv_sge = 1,
                        .max_inline_data = 8,
                    },
                .qp_type = IBV_QPT_RC,
            },
        .responder_resources = 5,
        .initiator_depth = 5,
        .retry_count = 8,
        .rnr_retry_count = 0,
        .native_qp = false,
        .inline_recv = 0,
    };

    // uint32_t magic_key;
    // uint64_t magic_addr;
    // int mRdmaBufferSize = 1024;
    // RDMA send
    // std::jthread *rdma_client_thread;
    kym::endpoint::Endpoint *clientEndpoint;
    // struct ibv_mr *send_mr;
    // struct cinfo *client_ci;
    // RDMA recv
    // std::jthread *rdma_server_thread;
    kym::endpoint::Endpoint *serverEndpoint;
    // struct ibv_mr *magic_mr;
    // struct cinfo *server_ci;

  private:
    u8 mEpoch = 0;
    u8 mGateway = 0;
    // number of devices
    int mN;
    // context pool size
    int mContextPoolSize;
    int mRandReadMapSize = 1'000'000;

    // int mCkptMapSize = 500'000; //
    // int mCkptMapSize = 1'000'000; // done
    int mCkptMapSize = 2'000'000; //
    int mCkptRecentMapSize = mCkptMapSize * 0.01;

    bool mCkpt = false;

    std::vector<Device *> mDevices;

    spdk_poller *mEventsPoller = nullptr;
    // spdk_poller *mDispatchPoller = nullptr;
    // spdk_poller *mHttpPoller = nullptr;
    // spdk_poller *mCompletionPoller = nullptr;

    int mQueueDepth = 1;

    IoThread mIoThread[16];

    std::jthread mRdmaThread;

    std::vector<Zone *> mZones;
    std::string mSelfIp;
    std::vector<std::jthread> mHttpThreads;

    chrono_tp mCkptStart;
};
