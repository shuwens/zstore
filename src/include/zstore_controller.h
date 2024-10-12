#pragma once
#include "common.h"
#include "configuration.h"
#include "device.h"
#include "global.h"
#include <boost/asio/dispatch.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/config.hpp>
#include <cstring>
#include <mutex>
#include <shared_mutex>
#include <spdk/env.h> // Include SPDK's environment header
#include <unistd.h>

namespace net = boost::asio; // from <boost/asio.hpp>

class ZstoreController
{
  public:
    ZstoreController(net::io_context &ioc) : mIoc_(ioc){};
    // The io_context is required for all I/O
    net::io_context &mIoc_;

    ~ZstoreController();
    int Init(bool object, int key_experiment);
    // Result<void> PopulateMap(bool bogus, int key_experiment);
    // Result<void> PopulateDevHash(int key_experiment);
    int PopulateDevHash(int key_experiment);
    int PopulateMap(bool bogus, int key_experiment);
    Result<DevTuple> GetDevTuple(ObjectKey object_key);
    int pivot;

    // ZStore Map: this maps key to tuple of ZNS target and lba
    // TODO
    // at some point we need to discuss the usage of flat hash map or unordered
    // map key -> tuple of <zns target, lba> std::unordered_map<std::string,
    // std::tuple<std::pair<std::string, int32_t>>>
    // std::unordered_map<std::string, std::tuple<MapEntry, MapEntry,
    // MapEntry>>
    // std::unordered_map<std::string, MapEntry, std::less<>> mMap;
    std::unordered_map<std::string, MapEntry> mMap;
    std::shared_mutex mMapMutex;

    // ZStore Device Consistent Hashmap: this maintains a consistent hash map
    // which maps object key to tupke of devices. Right now this is
    // pre-populated and just randomly
    // std::unordered_map<ObjectKey, DevTuple> mDevHash;
    std::vector<DevTuple> mDevHash;
    std::shared_mutex mDevHashMutex;

    // ZStore Bloom Filter: this maintains a bloom filter of hashes of
    // object name (key).
    //
    // For simplicity, right now we are just using a set to keep track of
    // the hashes
    std::unordered_set<std::string> mBF;
    std::shared_mutex mBFMutex;

    // Object APIs
    Result<bool> SearchBF(std::string key);
    Result<void> UpdateBF(std::string key);

    Result<MapEntry> FindObject(std::string key);
    Result<void> ReleaseObject(std::string key);

    Result<MapEntry> CreateObject(std::string key, DevTuple tuple);
    Result<void> PutObject(std::string key, MapEntry entry);

    Result<void> DeleteObject(std::string key);

    // threads
    void initIoThread();
    void initDispatchThread();
    void initCompletionThread();
    void initHttpThread();

    struct spdk_thread *GetIoThread(int id) { return mIoThread[id].thread; };
    struct spdk_thread *GetDispatchThread() { return mDispatchThread; }
    struct spdk_thread *GetHttpThread(int id) { return mHttpThread[id].thread; }
    struct spdk_thread *GetCompletionThread() { return mCompletionThread; }

    // SPDK components
    struct spdk_nvme_qpair *GetIoQpair();
    bool CheckIoQpair(std::string msg);

    void SetEventPoller(spdk_poller *p) { mEventsPoller = p; }
    void SetCompletionPoller(spdk_poller *p) { mCompletionPoller = p; }
    void SetDispatchPoller(spdk_poller *p) { mDispatchPoller = p; }
    void SetHttpPoller(spdk_poller *p) { mHttpPoller = p; }

    int GetQueueDepth() { return mQueueDepth; };

    int GetContextPoolSize() { return mContextPoolSize; };

    void setQueuDepth(int queue_depth) { mQueueDepth = queue_depth; };
    void setContextPoolSize(int context_pool_size)
    {
        mContextPoolSize = context_pool_size;
    };
    void setNumOfDevices(int num_of_device) { mN = num_of_device; };

    Device *GetDevice() { return mDevices[0]; };

    // Setting up SPDK
    void register_ctrlr(Device *device, struct spdk_nvme_ctrlr *ctrlr);
    void register_ns(struct spdk_nvme_ctrlr *ctrlr, struct spdk_nvme_ns *ns);

    int register_workers();
    int register_controllers(Device *device);
    void unregister_controllers(Device *device);
    int associate_workers_with_ns(Device *device);
    void zstore_cleanup();
    void zns_dev_init(Device *device, std::string ip1, std::string port1);
    void cleanup_ns_worker_ctx();
    void cleanup(uint32_t task_count);

    int init_ns_worker_ctx(struct ns_worker_ctx *ns_ctx,
                           enum spdk_nvme_qprio qprio);

    // TODO:
    void Append(uint64_t zslba, uint32_t size, void *data, void *cb_args);
    //
    // void Write(uint64_t offset, uint32_t size, void *data,
    //            zns_raid_request_complete cb_fn, void *cb_args);
    //
    Result<void> Read(uint64_t offset, HttpRequest request,
                      std::function<void(HttpRequest)> fn);
    //
    // void Execute(uint64_t offset, uint32_t size, void *data, bool is_write,
    //              zns_raid_request_complete cb_fn, void *cb_args);

    void Drain();

    void EnqueueWrite(RequestContext *ctx);
    void EnqueueRead(RequestContext *ctx);
    // void EnqueueReadReaping(RequestContext *ctx);
    std::queue<RequestContext *> &GetWriteQueue() { return mWriteQueue; }
    std::queue<RequestContext *> &GetReadQueue() { return mReadQueue; }

    // std::queue<RequestContext *> &GetEventsToDispatch();

    int GetWriteQueueSize() { return mWriteQueue.size(); };
    int GetReadQueueSize() { return mReadQueue.size(); };

    // int GetEventsToDispatchSize();

    std::queue<RequestContext *> &GetRequestQueue();
    std::shared_mutex &GetRequestQueueMutex();
    // std::shared_mutex &GetSessionMutex();
    // std::mutex &GetRequestQueueMutex();
    int GetRequestQueueSize();

    // void UpdateIndexNeedLock(uint64_t lba, PhysicalAddr phyAddr);
    // void UpdateIndex(uint64_t lba, PhysicalAddr phyAddr);
    int GetNumInflightRequests();

    void WriteInDispatchThread(RequestContext *ctx);
    void ReadInDispatchThread(RequestContext *ctx);
    // void EnqueueEvent(RequestContext *ctx);

    void ReclaimContexts();
    void Flush();
    void Dump();

    // uint32_t GetHeaderRegionSize();
    // uint32_t GetDataRegionSize();
    // uint32_t GetFooterRegionSize();

    bool Append(RequestContext *ctx, uint32_t offset);
    // bool Read(RequestContext *ctx, uint32_t pos, PhysicalAddr phyAddr);
    void Reset(RequestContext *ctx);
    bool IsResetDone();
    void WriteComplete(RequestContext *ctx);
    void ReadComplete(RequestContext *ctx);
    // void ReclaimReadContext(ReadContext *readContext);

    void AddZone(Zone *zone);
    const std::vector<Zone *> &GetZones();
    void PrintStats();

    bool start = false;
    chrono_tp stime;

    RequestContextPool *mRequestContextPool;
    std::unordered_set<RequestContext *> mInflightRequestContext;

    // mutable std::shared_mutex g_mutex_;
    // mutable std::shared_mutex context_pool_mutex_;
    // std::mutex context_pool_mutex_;

    // std::mutex mTaskPoolMutex;
    bool verbose;

    bool isDraining;

  private:
    // number of devices
    int mN;
    // context pool size
    int mContextPoolSize;

    // simple way to terminate the server
    // uint64_t tsc_end;

    RequestContext *getContextForUserRequest();
    void doWrite(RequestContext *context);
    void doRead(RequestContext *context);

    std::vector<Device *> mDevices;
    std::queue<RequestContext *> mRequestQueue;
    std::shared_mutex mRequestQueueMutex;

    std::shared_mutex mSessionMutex;

    spdk_poller *mEventsPoller = nullptr;
    spdk_poller *mDispatchPoller = nullptr;
    spdk_poller *mHttpPoller = nullptr;
    spdk_poller *mCompletionPoller = nullptr;

    int mQueueDepth = 1;

    IoThread mIoThread[16];
    struct spdk_thread *mDispatchThread;
    // struct spdk_thread *mHttpThread;
    IoThread mHttpThread[16];
    struct spdk_thread *mCompletionThread;

    // int64_t mNumInvalidBlocks = 0;
    // int64_t mNumBlocks = 0;

    std::queue<RequestContext *> mEventsToDispatch;
    std::queue<RequestContext *> mWriteQueue;
    std::queue<RequestContext *> mReadQueue;

    // in memory object tables, used only by kv store
    // std::map<std::string, kvobject> mem_obj_table;
    // std::mutex mem_obj_table_mutex;
    // ZstoreControllerMetadata mMeta;
    std::vector<Zone *> mZones;
    // std::vector<RequestContext> mResetContext;
};
