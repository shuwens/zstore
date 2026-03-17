#include "include/configuration.h"
#include "include/global.h"
#include "include/http_server.h"
#include "include/object.h"
#include "include/rdma_common.h"
#include "include/rdma_utils.h"
#include "include/utils.h"
#include <boost/outcome/success_failure.hpp>
#include <boost/outcome/utils.hpp>
#include <boost/serialization/map.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/utility.hpp>
#include <cassert>
#include <fstream>
#include <infiniband/verbs.h>
#include <random>
#include <spdk/nvme_zns.h>
#include <spdk/string.h>
// #include <system_error>
#include <utility>

using namespace std;
using tcp = boost::asio::ip::tcp; // from <boost/asio/ip/tcp.hpp>
const string election_root_ = "/election";
const string tx_root_ = "/tx";
#define SESSION_TIMEOUT 30000
string g_data;

Result<bool> RdmaClientConnect(struct RdmaClient &client, const int mOption)
{
    log_error("RdmaClientConnect");
    client.ctx.server_name = "zstore1";
    rc_init(on_pre_conn,
            NULL, // on connect
            on_completion,
            NULL); // on disconnect

    static char port[20]; // static so it persists after function returns
    snprintf(port, sizeof(port), "%d", 12345);

    if (mOption == 1) {
        rc_client_loop("12.12.12.5", port, &client.ctx, *client.ctrl);
    } else if (mOption == 5) {
        rc_client_loop("12.12.12.1", port, &client.ctx, *client.ctrl);
    }

    return outcome::success(true);
}

int ZstoreController::Init(bool object, int key_experiment, int option)
{
    int rc = 0;

    rc = SetParameters(key_experiment, option);
    assert(rc == 0);

    // Preallocate contexts for user requests
    // Sufficient to support multiple I/O queues of NVMe-oF target

    if (Conf::IsReplay()) {
        for (auto object_size_in_blocks : mObjectSizeInBlocks) {
            auto pool_size = mObjectSizePoolMap[object_size_in_blocks];
            if (Conf::IsDebugging())
                log_info("Object size in blocks {}, pool size {}",
                         object_size_in_blocks, pool_size);
            mReplayRequestContextPool[object_size_in_blocks] =
                new RequestContextPool(pool_size);
            if (mReplayRequestContextPool[object_size_in_blocks] == NULL) {
                log_error("could not initialize task pool");
                rc = 1;
                zstore_cleanup();
                return rc;
            }
        }
    } else {
        mRequestContextPool = new RequestContextPool(mContextPoolSize);
        if (mRequestContextPool == NULL) {
            log_error("could not initialize task pool");
            rc = 1;
            zstore_cleanup();
            return rc;
        }
    }

    rc = ConfigureSpdkQpairs();
    assert(rc == 0);

    auto num_ioc_threads = Conf::GetNumHttpThreads();
    // The io_context is required for all I/O
    // asio::io_context ioc{num_ioc_threads};
    boost::asio::ip::address address = asio::ip::make_address(mSelfIp);
    auto const port = 2000;

    // Spawn a listening port
    asio::co_spawn(mIoc, do_listen(tcp::endpoint{address, port}, *this),
                   [](exception_ptr e) {
                       if (e)
                           try {
                               rethrow_exception(e);
                           } catch (exception &e) {
                               log_error("Error in listener: {}", e.what());
                           }
                   });

    mHttpThreads.resize(num_ioc_threads);
    for (int i = 0; i < num_ioc_threads; ++i) {
        mHttpThreads[i] = jthread([i, this] {
#ifdef PERF
            cpu_set_t cpuset;
            CPU_ZERO(&cpuset);
            CPU_SET(i + Conf::GetHttpThreadCoreId(), &cpuset);
            string name = "zstore_ioc" + to_string(i);
            int rc = pthread_setname_np(mHttpThreads[i].native_handle(),
                                        name.c_str());
            assert(rc == 0);
            rc = pthread_setaffinity_np(mHttpThreads[i].native_handle(),
                                        sizeof(cpu_set_t), &cpuset);
            assert(rc == 0);

            if (Conf::IsDebugging())
                log_info("HTTP server: Thread {} on core {}", i,
                         i + Conf::GetHttpThreadCoreId());
#endif
            // ioc.run();
            for (;;)
                mIoc.poll();
        });
    }

    // rc = SetupHttpThreads();
    // assert(rc == 0);

    rc = PopulateDevHash();
    assert(rc == 0);

    rc = PopulateMap();
    assert(rc == 0);

    rc = ReadZoneHeaders();
    assert(rc == 0);

    if (mKeyExperiment == 6) {
        // if (Conf::GetNumOfGw() > 1) {
        rc = SetupZookeeper();
        assert(rc == 0);
    }

    if (Conf::IsDebugging())
        CheckIoThread("Starting all the threads");

    // connect to RDMA server socket
    // std::string ip;
    // if (mSelfIp == "12.12.12.1")
    //     ip = "12.12.12.5";
    // else
    //     ip = "12.12.12.1";

    sleep(5);

    // mRdmaClient.src = new char[128];
    //
    // if (!mRdmaClient.src) {
    //     rdma_error("Failed to allocate memory : -ENOMEM\n");
    //     return -ENOMEM;
    // }

    mRdmaClient.ctrl = this;
    auto ret = RdmaClientConnect(mRdmaClient, option);

    log_info("ZstoreController Init finish");
    return 0;
}

vector<tuple<string, string, string, u32, u32>> populateIpPorts(int option,
                                                                string self_ip)
{
    vector<tuple<string, string, string, u32, u32>> ip_port_devs;
    string port;
    if (option == 1) {
        port = "5520";
    } else if (option == 5) {
        port = "5521";
    } else if (option == 6) {
        port = "5522";
    } else {
        log_error("Invalid gateway server");
    }
    ip_port_devs.push_back(
        std::make_tuple("nqn.2024-04.io.zstore2:cnode1", "12.12.12.2", port,
                        Conf::GetZoneId(), Conf::GetZoneId()));
    ip_port_devs.push_back(
        std::make_tuple("nqn.2024-04.io.zstore3:cnode1", "12.12.12.3", port,
                        Conf::GetZoneId(), Conf::GetZoneId()));
    ip_port_devs.push_back(
        std::make_tuple("nqn.2024-04.io.zstore5:cnode1", "12.12.12.5", port,
                        Conf::GetZoneId(), Conf::GetZoneId()));
    return ip_port_devs;
}

int ZstoreController::SetParameters(int key_experiment, int option)
{
    pivot = 0;

    setContextPoolSize(Conf::GetContextPoolSize());
    setOption(option);
    setNumOfDevices(Conf::GetNumOfDevices() * Conf::GetNumOfTargets());
    setKeyExperiment(key_experiment);
    if (Conf::IsDebugging())
        log_info("Init ZstoreController with {} devices", mN);

    SetGateway(mOption);
    nodeName_ = "gateway_" + to_string(GetGateway());
    if (Conf::IsDebugging())
        log_info("Key experiment {}, gateway {}", mKeyExperiment, mOption);

    // TODO: set all parameters too
    log_debug("Conf: sector size {}, context pool size "
              "{}, targets {}, devices {}",
              Conf::GetBlockSize(), Conf::GetContextPoolSize(),
              Conf::GetNumOfTargets(), Conf::GetNumOfDevices());

    if (mOption == 1) {
        mSelfIp = "12.12.12.1";
    } else if (mOption == 5) {
        mSelfIp = "12.12.12.5";
    } else if (mOption == 6) {
        mSelfIp = "12.12.12.6";
    } else {
        log_error("Invalid gateway server");
    }
    auto ip_port_devs = populateIpPorts(mOption, mSelfIp);

    auto RdmaPortBase = 8980;
    log_info("RDMA server: listen ip {} port {}", mSelfIp, RdmaPortBase);

    // RDMA server thread
    mRdmaServerThread = jthread([this] {
        rc_init(on_pre_conn, on_connection, on_completion, on_disconnect);

        printf("waiting for connections. interrupt (^C) to exit.\n");

        static char port[20]; // static so it persists after function returns
        snprintf(port, sizeof(port), "%d", 12345);

        rc_server_loop(port, *this);
    });
    mRdmaServerThread.detach();

    int rc = 0;
    for (auto &dev_tuple : ip_port_devs) {
        if (register_controllers(g_devices, dev_tuple) != 0) {
            rc = 1;
            zstore_cleanup();
            return rc;
        }
    }
    mDevices = g_devices;
    if (Conf::IsDebugging())
        log_info("SPDK {} devices registered", mN);

    return 0;
}

int ZstoreController::ConfigureSpdkQpairs()
{
    if (Conf::IsDebugging())
        log_debug("Configure SPDK qpairs");
    // Create poll groups for the io threads and perform initialization
    for (int threadId = 0; threadId < Conf::GetNumIoThreads(); ++threadId) {
        mIoThread[threadId].group = spdk_nvme_poll_group_create(NULL, NULL);
        mIoThread[threadId].controller = this;
    }
    for (int i = 0; i < mN; ++i) {
        struct spdk_nvme_qpair **ioQueues = mDevices[i]->GetIoQueues();
        for (int threadId = 0; threadId < Conf::GetNumIoThreads(); ++threadId) {
            spdk_nvme_ctrlr_disconnect_io_qpair(ioQueues[threadId]);
            int rc = spdk_nvme_poll_group_add(mIoThread[threadId].group,
                                              ioQueues[threadId]);
            assert(rc == 0);
        }
        mDevices[i]->ConnectIoPairs();
    }

    if (Conf::IsDebugging())
        CheckIoQpair("Starting all the threads");

    initIoThread();

    if (Conf::IsDebugging())
        log_info("Configure SPDK qpairs finish");
    return 0;
}

int ZstoreController::ReadZoneHeaders()
{
    // Valid (full and open) zones and their headers
    std::map<uint64_t, uint8_t *> zonesAndHeaders[mN];
    for (int i = 0; i < mN; ++i) {
        // if (i == failedDriveId) {
        //     continue;
        // }

        // log_debug("read zone and headers {}.", i);
        // TODO: right now we sort of just pick read and write zone,
        // this should be done smartly
        // FIXME bug
        // mDevices[i]->ReadZoneHeaders(zonesAndHeaders[i]);
        // mDevices[i]->GetZoneHeaders(zonesAndHeaders[i]);
    }
    if (Conf::IsDebugging())
        log_info("Read zone headers finish");
    return 0;
}

int ZstoreController::SetupZookeeper()
{
    startZooKeeper();
    sleep(2);
    auto rc = Checkpoint();
    assert(rc && "Checkpoint failed");

    log_info("Zookeeper setup finish");
    return 0;
}

int ZstoreController::SetupHttpThreads()
{
    // log_debug("Setup HTTP threads");
    log_error("Not implemented");

    // log_info("HTTP server setup finish");
    return 0;
}

// this yields a list of devices for a given object key
//
// right now we create 120 (6*5*4) tuples, to consider ordering of every
// tuple
int ZstoreController::PopulateDevHash()
{
    std::unique_lock lock(mDevHashMutex);
    std::vector<std::pair<u8, u32>> tgt_dev_vec;

    tgt_dev_vec.push_back({std::make_pair(0, Conf::GetZoneId())});
    tgt_dev_vec.push_back({std::make_pair(1, Conf::GetZoneId())});
    tgt_dev_vec.push_back({std::make_pair(2, Conf::GetZoneId())});
    tgt_dev_vec.push_back({std::make_pair(3, Conf::GetZoneId())});
    tgt_dev_vec.push_back({std::make_pair(4, Conf::GetZoneId())});
    tgt_dev_vec.push_back({std::make_pair(5, Conf::GetZoneId())});
    for (unsigned long i = 0; i < tgt_dev_vec.size(); i++) {
        for (unsigned long j = 0; j < tgt_dev_vec.size(); j++) {
            if (i == j) {
                continue;
            }
            for (unsigned long k = 0; k < tgt_dev_vec.size(); k++) {
                if (i == k || j == k) {
                    continue;
                }
                mDevHash.push_back({std::make_tuple(
                    tgt_dev_vec[i], tgt_dev_vec[j], tgt_dev_vec[k])});
            }
        }
    }
    if (Conf::IsDebugging())
        log_info("DevHash size {}", mDevHash.size());
    return 0;
}

Result<DevTuple>
ZstoreController::GetDevTupleForRandomReads(ObjectKeyHash key_hash)
{
    return std::make_tuple(std::make_pair(1, Conf::GetZoneId()),
                           std::make_pair(3, Conf::GetZoneId()),
                           std::make_pair(4, Conf::GetZoneId()));
}

Result<MapEntry> ZstoreController::CreateFakeObject(ObjectKeyHash key_hash,
                                                    DevTuple tuple)
{
    auto entry = createMapEntry(tuple, 0, 0, 0, 1);
    assert(entry.has_value());
    return entry.value();
}

// Helper function to write a single string to the file
void writeString(std::ofstream &outFile, const std::string &str)
{
    size_t size = str.size();
    outFile.write(reinterpret_cast<const char *>(&size), sizeof(size));
    outFile.write(str.c_str(), size);
}

// Helper function to read a single string from the file
std::string readString(std::ifstream &inFile)
{
    size_t size;
    inFile.read(reinterpret_cast<char *>(&size), sizeof(size));
    std::string str(size, '\0');
    inFile.read(&str[0], size);
    return str;
}

// Inflight Map APIs

Result<bool> ZstoreController::SearchInflight(const ObjectKeyHash &key_hash)
{
    return mInflight.contains(key_hash);
}

// This is highly inefficient, as we are doing the process everytime
// We are also not batching the broadcast
Result<bool>
ZstoreController::BroadcastToInflight(const ObjectKeyHash &key_hash)
{

    // int map_entry_size = sizeof(BufferEntry);
    // if (Conf::IsDebugging())
    //     log_debug(
    //         "Map entry size {}, key hash size {}, buffer entry size
    //         {} ",
    //         sizeof(CompactedMapEntry),
    //         sizeof(ObjectKeyHash), sizeof(BufferEntry));
    // assert(map_entry_size == 128);
    // void *send = malloc(map_entry_size);
    // struct ibv_mr *send_mr = ibv_reg_mr(mRdmaClientEndpoint->GetPd(),
    // send,
    //                                     map_entry_size,
    //                                     IBV_ACCESS_LOCAL_WRITE);
    //
    // auto entry = new BufferEntry();
    // entry->key_hash = key_hash;
    // entry->epoch = GetEpoch();
    // entry->is_index = false;
    //
    // auto do_write = mRdmaClientEndpoint->PostWrite(
    //     reinterpret_cast<uint64_t>(&entry), send_mr->lkey, send,
    //     map_entry_size, mRdmaClientCi->magic_addr + 2 * 1024 * 1024,
    //     mRdmaClientCi->magic_key);
    // if (!do_write.ok()) {
    //     std::cerr << "Error writing " << do_write << std::endl;
    //     return outcome::failure(std::error_code());
    // }
    // auto wc_s = mRdmaClientEndpoint->PollSendCq();
    // if (!wc_s.ok()) {
    //     std::cerr << "error polling send cq for write " << wc_s.status()
    //               << std::endl;
    // }
    //
    // if (Conf::IsDebugging())
    //     log_debug("EOB {}", key_hash);
    return outcome::success(true);
}

Result<bool> ZstoreController::BroadcastToInflight(
    const ObjectKeyHash &key_hash, const RequestContext *s1,
    const RequestContext *s2, const RequestContext *s3)
{
    // if (mRdmaClientEndpoint == nullptr) {
    //     if (Conf::IsDebugging()) {
    //         log_info("RDMA server not initialized");
    //         log_debug("Broadcasting to inflight {}", key_hash);
    //     }
    //     std::string ip;
    //     if (mSelfIp == "12.12.12.1")
    //         ip = "12.12.12.5";
    //     else
    //         ip = "12.12.12.1";
    //
    //     auto ep_s = kym::endpoint::Dial(ip, 8980, mRdmaOpts);
    //     if (!ep_s.ok()) {
    //         std::cerr << "error dialing " << ep_s.status() << std::endl;
    //         return outcome::failure(std::error_code());
    //     }
    //     mRdmaClientEndpoint = ep_s.value();
    //
    //     mRdmaClientEndpoint->GetConnectionInfo((void **)&mRdmaClientCi);
    // }
    //
    // int map_entry_size = sizeof(BufferEntry);
    // if (Conf::IsDebugging())
    //     log_debug("Map entry size {}, key hash size {}, buffer entry size
    //     {}",
    //               sizeof(CompactedMapEntry), sizeof(ObjectKeyHash),
    //               sizeof(BufferEntry));
    // assert(map_entry_size == 128);
    // void *send = malloc(map_entry_size);
    // struct ibv_mr *send_mr = ibv_reg_mr(mRdmaClientEndpoint->GetPd(),
    // send,
    //                                     map_entry_size,
    //                                     IBV_ACCESS_LOCAL_WRITE);
    //
    // auto entry = new BufferEntry();
    // entry->key_hash = key_hash;
    // entry->epoch = GetEpoch();
    // entry->is_index = true;
    // if (Conf::IsDebugging())
    //     log_debug("S1: ioctx offset {}, ioctx size {}, append lba {}",
    //               s1->ioContext.offset, s1->ioContext.size,
    //               s1->append_lba);
    //
    // // assert(s1->ioContext.offset == s1->append_lba);
    // auto s1_tuple = make_tuple(s1->device->GetDeviceId(), s1->append_lba,
    //                            s1->ioContext.size);
    // auto s2_tuple = make_tuple(s2->device->GetDeviceId(), s2->append_lba,
    //                            s2->ioContext.size);
    // auto s3_tuple = make_tuple(s3->device->GetDeviceId(), s3->append_lba,
    //                            s3->ioContext.size);
    // entry->value = make_tuple(s1_tuple, s2_tuple, s3_tuple);
    //
    // auto do_write = mRdmaClientEndpoint->PostWrite(
    //     reinterpret_cast<uint64_t>(&entry), send_mr->lkey, send,
    //     map_entry_size, mRdmaClientCi->magic_addr + 2 * 1024 * 1024,
    //     mRdmaClientCi->magic_key);
    // if (!do_write.ok()) {
    //     std::cerr << "Error writing " << do_write << std::endl;
    //     return outcome::failure(std::error_code());
    // }
    // auto wc_s = mRdmaClientEndpoint->PollSendCq();
    // if (!wc_s.ok()) {
    //     std::cerr << "error polling send cq for write " << wc_s.status()
    //               << std::endl;
    // }
    return outcome::success(true);
}

// Map APIs
Result<DevTuple> ZstoreController::GetDevTuple(ObjectKeyHash key_hash)
{
    unsigned int seed = arrayToSeed(key_hash);
    std::srand(seed); // seed the random number generator
                      // with the hash of the object key
    int random_index = std::rand() % mDevHash.size();
    return mDevHash[random_index];
}

Result<bool> ZstoreController::PutObject(const ObjectKeyHash &key_hash,
                                         const MapEntry entry)
{
    return mMap.insert_or_assign(key_hash, entry);
}

std::optional<MapEntry>
ZstoreController::GetObject(const ObjectKeyHash &key_hash)
{
    std::optional<MapEntry> o;
    mMap.visit(key_hash, [&](const auto &x) { o = x.second; });
    return o;
}

Result<MapEntry> ZstoreController::DeleteObject(const ObjectKeyHash &key_hash)
{
    MapEntry entry;
    mMap.visit(key_hash, [&](const auto &x) { entry = x.second; });
    mMap.erase(key_hash);
    return entry;
}

Result<std::vector<ObjectKeyHash>>
ZstoreController::ListObjects(const int max_keys)
{
    std::vector<ObjectKeyHash> keys;
    int count = 0;
    mMap.visit_all([&](auto &x) {
        if (count < max_keys) {
            keys.push_back(x.first);
            count++;
        }
    });
    return keys;
}

// Result<bool> ZstoreController::AddGcObject(const TargetLbaTuple &tuple)
// {
//     return mGcSet.insert(tuple);
// }

void ZstoreController::initIoThread()
{
    struct spdk_cpuset cpumask;
    for (int threadId = 0; threadId < Conf::GetNumIoThreads(); ++threadId) {
        spdk_cpuset_zero(&cpumask);
        spdk_cpuset_set_cpu(&cpumask, Conf::GetIoThreadCoreId(threadId), true);
        mIoThread[threadId].thread = spdk_thread_create("IoThread", &cpumask);
        assert(mIoThread[threadId].thread != nullptr);
        mIoThread[threadId].controller = this;
        int rc = spdk_env_thread_launch_pinned(
            Conf::GetIoThreadCoreId(threadId), ioWorker, &mIoThread[threadId]);
        log_info("IO thread name {} id {} on core {}",
                 spdk_thread_get_name(mIoThread[threadId].thread),
                 spdk_thread_get_id(mIoThread[threadId].thread),
                 Conf::GetIoThreadCoreId(threadId));
        if (rc < 0) {
            log_error("Failed to launch IO thread error: {} {}", strerror(rc),
                      spdk_strerror(rc));
        }
    }
}

void ZstoreController::register_ctrlr(std::vector<Device *> &g_devices,
                                      struct spdk_nvme_ctrlr *ctrlr,
                                      const char *traddr, const u32 zone_id1,
                                      const u32 zone_id2)
{
    struct spdk_nvme_ns *ns;

    Device *device1 = new Device();
    ns = spdk_nvme_ctrlr_get_ns(ctrlr, 1);
    if (spdk_nvme_ns_get_csi(ns) != SPDK_NVME_CSI_ZNS)
        log_info("ns {} is not zns ns", 1);
    device1->Init(ctrlr, 1, zone_id1);
    device1->SetDeviceTransportAddress(traddr);
    g_devices.emplace_back(device1);

    Device *device2 = new Device();
    ns = spdk_nvme_ctrlr_get_ns(ctrlr, 2);
    if (spdk_nvme_ns_get_csi(ns) != SPDK_NVME_CSI_ZNS)
        log_info("ns {} is not zns ns", 2);
    device2->Init(ctrlr, 2, zone_id2);
    device2->SetDeviceTransportAddress(traddr);
    g_devices.emplace_back(device2);

    // TODO log and store stats
    auto zone_size_sectors = spdk_nvme_zns_ns_get_zone_size_sectors(ns);
    auto zone_size_bytes = spdk_nvme_zns_ns_get_zone_size(ns);
    auto num_zones = spdk_nvme_zns_ns_get_num_zones(ns);
    uint32_t max_open_zones = spdk_nvme_zns_ns_get_max_open_zones(ns);
    uint32_t active_zones = spdk_nvme_zns_ns_get_max_active_zones(ns);
    uint32_t max_zone_append_size =
        spdk_nvme_zns_ctrlr_get_max_zone_append_size(ctrlr);
    auto metadata_size = spdk_nvme_ns_get_md_size(ns);

    if (Conf::IsDebugging()) {
        log_info("Zone size: sectors {}, bytes {}", zone_size_sectors,
                 zone_size_bytes);
        log_info("Zones: num {}, max open {}, active {}", num_zones,
                 max_open_zones, active_zones);
        log_info("Max zones append size: {}, metadata size {}",
                 max_zone_append_size, metadata_size);
    }
}

void ZstoreController::zns_dev_init(
    std::vector<Device *> &g_devices,
    const std::tuple<std::string, std::string, std::string, u32, u32>
        &dev_tuple)
{
    // 1. connect nvmf device
    auto [nqn, ip, port, zone_id1, zone_id2] = dev_tuple;
    struct spdk_nvme_transport_id trid = {};
    snprintf(trid.traddr, sizeof(trid.traddr), "%s", ip.c_str());
    snprintf(trid.trsvcid, sizeof(trid.trsvcid), "%s", port.c_str());
    snprintf(trid.subnqn, sizeof(trid.subnqn), "%s", nqn.c_str());
    trid.adrfam = SPDK_NVMF_ADRFAM_IPV4;
    trid.trtype = SPDK_NVME_TRANSPORT_RDMA;

    struct spdk_nvme_ctrlr_opts opts;
    spdk_nvme_ctrlr_get_default_ctrlr_opts(&opts, sizeof(opts));
    snprintf(opts.hostnqn, sizeof(opts.hostnqn), "%s", nqn.c_str());
    // NOTE: disable keep alive timeout
    opts.keep_alive_timeout_ms = 0;
    register_ctrlr(g_devices, spdk_nvme_connect(&trid, &opts, sizeof(opts)),
                   trid.traddr, zone_id1, zone_id2);
}

int ZstoreController::register_controllers(
    std::vector<Device *> &g_devices,
    const std::tuple<std::string, std::string, std::string, u32, u32>
        &dev_tuple)
{
    zns_dev_init(g_devices, dev_tuple);
    return 0;
}

void ZstoreController::unregister_controllers(std::vector<Device *> &g_devices)
{
    // struct spdk_nvme_detach_ctx *detach_ctx = NULL;
}

Result<void> ZstoreController::DumpAllMap()
{
    log_debug("dump map ");
    // Write the map to a file
    std::string filename = "map_data.bin";
    // writeMapToFile(filename);
    return outcome::success();
}

Result<void> ZstoreController::ReadAllMap()
{
    log_debug("read dump map ");
    std::string filename = "map_data.bin";
    // Read the map back from the file
    // readMapFromFile(filename);
    return outcome::success();
}

// TODO: assume we only have one device, we should check all device in
// the end
bool ZstoreController::CheckIoQpair(std::string msg)
{
    assert(mDevices[0] != nullptr);
    assert(mDevices[0]->GetIoQueue(0) != nullptr);
    if (!spdk_nvme_qpair_is_connected(mDevices[0]->GetIoQueue(0)))
        exit(0);
    return spdk_nvme_qpair_is_connected(mDevices[0]->GetIoQueue(0));
}

bool ZstoreController::CheckIoThread(std::string msg)
{
    log_debug("Check GetIoThread: {}", msg);
    for (int i = 0; i < mN; ++i) {
        auto thread = GetIoThread(0);
        assert(thread != nullptr);
        log_debug("Check Io thread {}: running {}, idle {}, exited {}", i,
                  spdk_thread_is_running(thread), spdk_thread_is_idle(thread),
                  spdk_thread_is_exited(thread));
        // assert(spdk_thread_is_running(thread) && "Thread should be
        // running"); assert(!spdk_thread_is_idle(thread) && "Thread should
        // not be idle"); assert(!spdk_thread_is_exited(thread) && "Thread
        // should not be exited");
    }

    log_debug("Check Device GetIoThread: {}", msg);
    // for (int i = 0; i < mN; ++i) {
    //     auto thread = mDevices[i]->GetIoThread();
    //     log_debug("Check Io thread {}: running {}, idle {}, exited {}",
    //     i,
    //               spdk_thread_is_running(thread),
    //               spdk_thread_is_idle(thread),
    //               spdk_thread_is_exited(thread));
    // assert(thread != nullptr);
    // assert(spdk_thread_is_running(thread) && "Thread is running");
    // assert(spdk_thread_is_idle(mIoThread[threadId].thread) &&
    //        "Thread should be idle");
    // assert(spdk_thread_is_running(mIoThread[threadId].thread) &&
    //        "Thread should be running");
    // assert(!spdk_thread_is_exited(mIoThread[threadId].thread) &&
    //        "Thread should not be exited");
    // }
    return 0;
}

struct spdk_nvme_qpair *ZstoreController::GetIoQpair()
{
    assert(mDevices[0] != nullptr);
    assert(mDevices[0]->GetIoQueue(0) != nullptr);
    return mDevices[0]->GetIoQueue(0);
}

static auto quit(void *args) { exit(0); }

ZstoreController::~ZstoreController()
{
    for (int i = 0; i < Conf::GetNumIoThreads(); ++i) {
        thread_send_msg(mIoThread[i].thread, quit, nullptr);
    }
    // thread_send_msg(mDispatchThread, quit, nullptr);
    // for (int i = 0; i < Conf::GetNumHttpThreads(); ++i) {
    //     thread_send_msg(mHttpThread[i].thread, quit, nullptr);
    // }
    // log_debug("drain io: {}", spdk_get_ticks());
    // log_debug("clean up ns worker");
    cleanup_ns_worker_ctx();
}

void ZstoreController::zstore_cleanup()
{
    log_info("unreg controllers");
    unregister_controllers(mDevices);
    log_info("cleanup ");
    cleanup(0);
    spdk_env_fini();
}

void ZstoreController::cleanup_ns_worker_ctx()
{
    log_info("here");
    // FIXME
    // thread_send_msg(mIoThread.thread, quit, nullptr);
    // spdk_nvme_ctrlr_free_io_qpair(mDevices[0]->GetIoQueue());
}

void ZstoreController::cleanup(uint32_t task_count)
{
    // free(mNamespace);
    // free(mWorker->ns_ctx);
    // free(mWorker);
    // if (spdk_mempool_count(mTaskPool) != (size_t)task_count) {
    //     log_error("mTaskPool count is {} but should be {}",
    //               spdk_mempool_count(mTaskPool), task_count);
    // }
    // spdk_mempool_free(mTaskPool);
}

// void ZstoreController::writeMapToFile(const std::string &filename)
// {
//     try {
//         std::ofstream file_out(filename, std::ios::out |
//         std::ios::trunc); if (!file_out.is_open()) {
//             throw std::runtime_error("Could not open file for
//             writing");
//         }
//         boost::archive::text_oarchive archive(file_out);
//         archive << mMap; // Serialize the map to the file
//         file_out.close();
//         std::cout << "Map successfully written to file: " << filename
//                   << std::endl;
//     } catch (const std::exception &e) {
//         std::cerr << "Error writing map to file: " << e.what() <<
//         std::endl;
//     }
// }
//
// void ZstoreController::readMapFromFile(const std::string &filename)
// {
//     try {
//         std::ifstream file_in(filename, std::ios::in);
//         if (!file_in.is_open()) {
//             throw std::runtime_error("Could not open file for
//             reading");
//         }
//         boost::archive::text_iarchive archive(file_in);
//         archive >> mMap; // Deserialize the map from the file
//         file_in.close();
//         std::cout << "Map successfully loaded from file: " <<
//         filename
//                   << std::endl;
//     } catch (const std::exception &e) {
//         std::cerr << "Error reading map from file: " << e.what() <<
//         std::endl;
//     }
// }

std::string getDevice(int i)
{
    std::string device;
    if (i % 6 == 0)
        device = "Zstore2Dev1";
    else if (i % 6 == 1)
        device = "Zstore2Dev2";
    else if (i % 6 == 2)
        device = "Zstore3Dev1";
    else if (i % 6 == 3)
        device = "Zstore3Dev2";
    else if (i % 6 == 4)
        device = "Zstore4Dev1";
    else if (i % 6 == 5)
        device = "Zstore4Dev2";

    return device;
}

u8 getDeviceId(int i) { return i % 6; }

void populateCkptMap(int mKeyExperiment, int mOption, int mCkptMapSize,
                     int mCkptRecentMapSize, ZstoreMap &mMap,
                     InflightWriteSet &mInflight)
{
    log_info("Populate Map({},{}): random read, starting from zone {}",
             mKeyExperiment, mOption, Conf::GetZoneId());
    u32 current_zone = Conf::GetZoneId();
    u64 current_lba = 0;
    u64 zone_offset = 0;
    auto len = Conf::GetObjectSizeInBytes() / Conf::GetBlockSize();
    for (int i = 0; i < mCkptMapSize; i++) {
        int dev_id;
        std::string device;
        if (i % 2 == 0)
            // device = "Zstore2Dev1";
            dev_id = 0;
        else if (i % 2 == 1)
            // device = "Zstore2Dev2";
            dev_id = 1;

        // the lba should be zone_num * 0x80000 + offset [0, 0x43500]
        zone_offset = current_lba % Conf::GetZoneDist();
        if (zone_offset > Conf::GetZoneCap()) {
            current_zone++;
            current_lba = current_zone * Conf::GetZoneDist();
            log_debug("Populate Map for index {}, current lba {}: "
                      "zone {} "
                      "is full, moving to next zone",
                      i, current_lba, current_zone);
        }

        auto zone_id = Conf::GetZoneId();
        auto entry =
            createMapEntry(std::make_tuple(std::make_pair(dev_id, zone_id),
                                           std::make_pair(1, zone_id),
                                           std::make_pair(2, zone_id)),
                           current_lba, current_lba, current_lba, len)
                .value();
        mMap.emplace(computeSHA256(std::to_string(i)), entry);
        current_lba++;
    }

    for (int i = 0; i < mCkptRecentMapSize; i++) {
        // mInflight.emplace(computeSHA256(std::to_string(i)), i);
        mInflight.emplace(computeSHA256(std::to_string(i)));
    }
}

bool gcChance(float ratio)
{
    static std::random_device rd;
    static std::mt19937 gen(rd()); // Mersenne Twister RNG
    static std::uniform_real_distribution<> dis(0.0, 1.0); // Uniform [0.0, 1.0)

    return dis(gen) < ratio;
}

void populateGcMap(int mKeyExperiment, int mOption, int mCkptMapSize,
                   int mCkptRecentMapSize, ZstoreMap &mMap, float ratio)
{
    log_info("Populate Map({},{}): random read, starting from zone {}",
             mKeyExperiment, mOption, Conf::GetZoneId());
    u32 current_zone = Conf::GetZoneId();
    u64 current_lba = 0;
    u64 zone_offset = 0;
    auto len = Conf::GetObjectSizeInBytes() / Conf::GetBlockSize();
    for (int i = 0; i < mCkptMapSize; i++) {
        int dev_id = i % 6;

        // the lba should be zone_num * 0x80000 + offset [0, 0x43500]
        zone_offset = current_lba % Conf::GetZoneDist();
        if (zone_offset > Conf::GetZoneCap()) {
            current_zone++;
            current_lba = current_zone * Conf::GetZoneDist();
            log_debug("Populate Map for index {}, current lba {}: "
                      "zone {} "
                      "is full, moving to next zone",
                      i, current_lba, current_zone);
        }

        auto zone_id = Conf::GetZoneId();
        auto entry =
            createMapEntry(std::make_tuple(std::make_pair(dev_id, zone_id),
                                           std::make_pair(1, zone_id),
                                           std::make_pair(2, zone_id)),
                           current_lba, current_lba, current_lba, len)
                .value();
        // if (gcChance(ratio))
        entry.tombstoned = true;
        mMap.emplace(computeSHA256(std::to_string(i)), entry);
        current_lba++;
    }
}

int ZstoreController::PopulateMap()
{
    if (mKeyExperiment == 1) {
        // Random Read
        log_info("Populate Map({},{}): random read, starting from zone {}",
                 mKeyExperiment, mOption, Conf::GetZoneId());
        u32 current_zone = Conf::GetZoneId();
        u64 current_lba = 0;
        u64 zone_offset = 0;
        auto len = Conf::GetObjectSizeInBytes() / Conf::GetBlockSize();
        for (int i = 0; i < mRandReadMapSize; i++) {
            auto device = getDeviceId(i);
            // the lba should be zone_num * 0x80000 + offset [0, 0x43500]
            zone_offset = current_lba % Conf::GetZoneDist();
            if (zone_offset > Conf::GetZoneCap()) {
                current_zone++;
                current_lba = current_zone * Conf::GetZoneDist();
                log_debug("Populate Map for index {}, current lba {}: "
                          "zone {} is full, moving to next zone",
                          i, current_lba, current_zone);
            }
            auto zone_id = Conf::GetZoneId();
            auto entry =
                createMapEntry(std::make_tuple(std::make_pair(device, zone_id),
                                               std::make_pair(1, zone_id),
                                               std::make_pair(2, zone_id)),
                               current_lba, current_lba, current_lba, len)
                    .value();
            mMap.emplace(computeSHA256(std::to_string(i)), entry);
            current_lba++;
        }
    } else if (mKeyExperiment == 2) {
        if (Conf::IsReplay())
            log_info("Populate no map({},{}): replaying workload.",
                     mKeyExperiment, mOption);
        else
            log_info("Populate no map({},{}): write and read workload.",
                     mKeyExperiment, mOption);

    } else if (mKeyExperiment == 3) {
        // Checkpoint
        if (mOption == 1) {
            log_info("Populate Map({},{}): write and read.", mKeyExperiment,
                     mOption);
            populateCkptMap(mKeyExperiment, mOption, mCkptMapSize,
                            mCkptRecentMapSize, mMap, mInflight);
        } else {
            log_info("Populate no map({},{}): write and read.", mKeyExperiment,
                     mOption);
        }
    } else if (mKeyExperiment == 5) {
        // gateway failure
        if (mOption == 1) {
            log_info("Populate Map({},{}): write and read.", mKeyExperiment,
                     mOption);
            populateCkptMap(mKeyExperiment, mOption, mCkptMapSize,
                            mCkptRecentMapSize, mMap, mInflight);
        } else {
            log_info("Populate no map({},{}): write and read.", mKeyExperiment,
                     mOption);
        }
    } else if (mKeyExperiment == 6) {
        // GC
        if (mOption == 1) {
            log_info("Populate Map({},{}): write and read.", mKeyExperiment,
                     mOption);
            populateGcMap(mKeyExperiment, mOption, mCkptMapSize,
                          mCkptRecentMapSize, mMap, 0.1);
        } else {
            log_info("Populate no map({},{}): write and read.", mKeyExperiment,
                     mOption);
        }
    } else if (mKeyExperiment == 7) {
        // Target and gateway failure
    }
    // else if (key_experiment == 1) {}
    return 0;
}

// The following functions are used to perform leader election and
// announcement using Zookeeper. Zookeeper recipers:
// https://zookeeper.apache.org/doc/current/recipes.html
// other source:
// https://gist.github.com/ochinchina/38e70fc3d8fa457d5cd8

void ZstoreController::createZnodes()
{
    Stat stat;
    // create root node for leader election
    if (zoo_exists(mZkHandler, election_root_.c_str(), false, &stat) != ZOK) {
        // log_info("{} does not exist", election_root_);
        int rc =
            zoo_create(mZkHandler, election_root_.c_str(), "leader election",
                       10, &ZOO_OPEN_ACL_UNSAFE, 0, 0, 0);
        if (rc != ZOK) {
            log_error("Error creating znode {}", election_root_);
        } else {
            log_info("Success creating znode {}", election_root_);
        }
    }

    // create root node for transaction: persist map
    if (zoo_exists(mZkHandler, tx_root_.c_str(), false, &stat) != ZOK) {
        // log_info("{} does not exist", tx_root_);
        int rc = zoo_create(mZkHandler, tx_root_.c_str(), "transaction", 10,
                            &ZOO_OPEN_ACL_UNSAFE, 0, 0, 0);
        if (rc != ZOK) {
            log_error("Error creating znode {}", tx_root_);
        } else {
            log_info("Success creating znode {}", tx_root_);
        }
    }
    std::string path = election_root_ + "/n_";

    if (!nodeName_.empty()) {
        if (zoo_create(mZkHandler, path.c_str(), nodeName_.data(),
                       nodeName_.length(), &ZOO_OPEN_ACL_UNSAFE,
                       ZOO_SEQUENCE | ZOO_EPHEMERAL, 0, 0) == ZOK) {
            log_info("success to create {}", path);
        }
    }

    checkChildrenChange();
}

static void ZkWatcher(zhandle_t *zkH, int type, int state, const char *path,
                      void *watcherCtx);

std::string ZstoreController::getNodeData(const std::string &path)
{
    char *buf = new char[256];
    int len = 256;

    for (;;) {
        Stat stat;
        if (zoo_get(mZkHandler, path.c_str(), false, buf, &len, &stat) == ZOK) {
            if (stat.dataLength > len) {
                delete[] buf;
                buf = new char[stat.dataLength];
                len = stat.dataLength;
            } else {
                std::string result(buf, len);
                delete[] buf;
                return result;
            }

        } else {
            delete[] buf;
            return "";
        }
    }
}

void ZstoreController::checkTxChange()
{
    log_info("checkTxChange");
    int len = 6;
    char *buf = new char[len];

    String_vector children;
    if (zoo_get_children(mZkHandler, tx_root_.c_str(), true, &children) ==
        ZOK) {
        if (children.count > 0) {
            for (int i = 0; i < children.count; i++) {
                std::string tx_path = tx_root_ + "/" + children.data[i];
                // auto node = getNodeData(path);
                // std::string tx_path = tx_root_ + "/" + node;
                int rc = zoo_get(mZkHandler, tx_path.c_str(), false, buf, &len,
                                 NULL);
                if (rc != ZOK) {
                    log_error("Error getting data from {}", tx_path);
                } else {
                    std::string input(buf, len);
                    log_info("data from {}: {}", tx_path, buf);
                    if (input == "empty_") {
                        log_info("node {} has not changed ", tx_path);
                    } else if (input == "commit") {
                        // log_info("commit data from {}", tx_path);
                        // clear the data
                        int rc = zoo_delete(mZkHandler, tx_path.c_str(), -1);
                        if (rc != ZOK) {
                            log_error("Error deleting data from {}", tx_path);
                        } else {
                            log_info("Success deleting data from {}", tx_path);
                        }

                    } else if (input == "abort_") {
                        // log_info("abort data from {}", tx_path);
                        // clear the data
                        int rc = zoo_delete(mZkHandler, tx_path.c_str(), -1);
                        if (rc != ZOK) {
                            log_error("Error deleting data from {}", tx_path);
                        } else {
                            log_info("Success deleting data from {}", tx_path);
                        }
                    }
                }
            }
        } else {
            if (mCkpt) {
                auto mCkptEnd = std::chrono::high_resolution_clock::now();
                auto duration =
                    std::chrono::duration_cast<std::chrono::seconds>(
                        mCkptEnd - mCkptStart);
                log_debug("Hooray: Total checkpoint duration: {} seconds",
                          duration.count());
            }
        }
        deallocate_String_vector(&children);
    }
}

void ZstoreController::checkChildrenChange()
{
    String_vector children;
    if (zoo_get_children(mZkHandler, election_root_.c_str(), true, &children) ==
        ZOK) {
        if (children.count > 0) {
            int min_seq = 0;
            int j = 0;

            // find the leader
            for (int i = 0; i < children.count; i++) {
                // log_info("children: {}", children.data[i]);
                int t = ::atoi(&children.data[i][2]);
                if (i == 0 || min_seq > t) {
                    min_seq = t;
                    j = i;
                }
            }

            // get the data
            std::string path = election_root_ + "/" + children.data[j];
            log_info("leader: {}", getNodeData(path));
            leaderNodeName_ = getNodeData(path);
        }
        deallocate_String_vector(&children);
    }
}

static void LeaderWatcher(zhandle_t *zkH, int type, int state, const char *path,
                          void *watcherCtx)
{
    ZstoreController *ctrl = static_cast<ZstoreController *>(watcherCtx);
    if (type == ZOO_CHANGED_EVENT) {
        log_debug("Follower detects the leader change, and ACK the epoch");
        std::string path = tx_root_ + "/" + ctrl->nodeName_;
        ctrl->ZkSet(path, "commit");
    }
    if (state == ZOO_CONNECTED_STATE) {
        log_debug("1111");
        if (type == ZOO_CHILD_EVENT) {
            log_debug("2222");
            ctrl->checkChildrenChange();
        } else {
            log_debug("3333");
            ctrl->createZnodes();
        }
    }
}

static void TxWatcher(zhandle_t *zkH, int type, int state, const char *path,
                      void *watcherCtx)
{
    ZstoreController *ctrl = static_cast<ZstoreController *>(watcherCtx);
    if (type == ZOO_CHANGED_EVENT) {
        log_debug("TxWatcher detects the change");
        ctrl->checkTxChange();
    }
}

static void ZkWatcher(zhandle_t *zkH, int type, int state, const char *path,
                      void *watcherCtx)
{
    ZstoreController *ctrl = static_cast<ZstoreController *>(watcherCtx);
    if (type == ZOO_SESSION_EVENT) {
        // state refers to states of zookeeper connection.
        // To keep it simple, we would demonstrate these 3:
        // ZOO_EXPIRED_SESSION_STATE, ZOO_CONNECTED_STATE,
        // ZOO_NOTCONNECTED_STATE If you are using ACL, you should be
        // aware of an authentication failure state -
        // ZOO_AUTH_FAILED_STATE
        if (state == ZOO_CONNECTED_STATE) {
            ctrl->mZkConnected = 1;
        } else if (state == ZOO_NOTCONNECTED_STATE) {
            ctrl->mZkConnected = 0;
        } else if (state == ZOO_EXPIRED_SESSION_STATE) {
            ctrl->mZkExpired = 1;
            ctrl->mZkConnected = 0;
            zookeeper_close(zkH);
        }
    }
    if (state == ZOO_CONNECTED_STATE) {
        if (type == ZOO_CHILD_EVENT) {
            ctrl->checkChildrenChange();
        } else {
            ctrl->createZnodes();
        }
    }
}

void ZstoreController::startZooKeeper()
{
    // zoo_set_debug_level(ZOO_LOG_LEVEL_DEBUG);
    zoo_set_debug_level(ZOO_LOG_LEVEL_ERROR);
    mZkHandler = zookeeper_init(Conf::GetZkHost().c_str(), ZkWatcher,
                                SESSION_TIMEOUT, 0, this, 0);
    if (!mZkHandler) {
        log_error("Error connecting to ZooKeeper server.");
        return;
    }
    log_info("Connecting to ZooKeeper server...");
}

std::vector<void *> dumpMap(const ZstoreMap &hashmap)
{
    std::vector<void *> buffer;
    hashmap.visit_all([&buffer](auto &x) {
        buffer.push_back(
            reinterpret_cast<void *>(const_cast<MapEntry *>(&x.second)));
    });
    return buffer;
}

std::vector<void *> dumpRecentWriteMap(const InflightWriteSet &hashmap)
{
    std::vector<void *> buffer;
    // hashmap.visit_all([&buffer](auto &x) {
    //     buffer.push_back(
    //         reinterpret_cast<void *>(const_cast<std::vector<u8>
    //         *>(&x.second)));
    // });
    return buffer;
}

// Broadcast APIs
Result<bool> ZstoreController::AddToBroadcast(const ObjectKeyHash &key_hash)
{

    if (Conf::IsDebugging())
        log_debug("AddToBroadcast {}", key_hash);
    mBroadcast.insert(key_hash);
    return outcome::success();
}

// Result<bool>
// ZstoreController::RemoveFromBroadcast(const ObjectKeyHash &key_hash)
// {
//     mBroadcast.erase(key_hash);
//     return outcome::success();
// }

void ZstoreController::WaitForPhase2(ObjectKeyHash key)
{
    auto cond = std::make_shared<CondEntry>();
    mCondMap.insert({key, cond}); // or emplace

    boost::unique_lock<boost::mutex> lock(cond->mtx);
    cond->cv.wait(lock, [&] { return cond->released; });

    log_info("Key {} released.", key);

    mCondMap.erase(key);

    log_error("Unimplemented, how to perform read in this case?");
    // do read here?
}

void ZstoreController::ReleaseKey(ObjectKeyHash key)
{
    mCondMap.visit(key, [&](const auto &it) {
        auto cond = it.second;
        {
            boost::lock_guard<boost::mutex> lock(cond->mtx);
            cond->released = true;
        }
        cond->cv.notify_all();
    });
}

// FIXME
Result<void> ZstoreController::SendRecordsToGateway()
{
    // namespace chrono = std::chrono;
    // using clock_type = chrono::high_resolution_clock;
    // std::string ip = "12.12.12.1";
    //
    // auto ep_s = kym::endpoint::Dial(ip, 8987, mRdmaOpts);
    // if (!ep_s.ok()) {
    //     std::cerr << "error dialing " << ep_s.status() << std::endl;
    //     return outcome::failure(std::error_code());
    // }
    // kym::endpoint::Endpoint *ep = ep_s.value();
    //
    // struct cinfo *ci;
    // ep->GetConnectionInfo((void **)&ci);
    //
    // std::vector<void *> map_buffer = dumpMap(mMap);
    // int n = map_buffer.size();
    // int map_entry_size = sizeof(MapEntry);
    // void *send = malloc(map_entry_size);
    // struct ibv_mr *send_mr =
    //     ibv_reg_mr(ep->GetPd(), send, map_entry_size,
    //     IBV_ACCESS_LOCAL_WRITE);
    //
    // auto start = clock_type::now();
    // for (auto &x : map_buffer) {
    //     auto stat = ep->PostWrite(
    //         reinterpret_cast<uint64_t>(&x), send_mr->lkey, send,
    //         map_entry_size, ci->magic_addr + 2 * 1024 * 1024,
    //         ci->magic_key);
    //     if (!stat.ok()) {
    //         std::cerr << "Error writing " << stat << std::endl;
    //         return outcome::failure(std::error_code());
    //     }
    //     auto wc_s = ep->PollSendCq();
    //     if (!wc_s.ok()) {
    //         std::cerr << "error polling send cq for write " <<
    //         wc_s.status()
    //                   << std::endl;
    //     }
    // }
    // auto end = clock_type::now();
    // auto dur = chrono::duration_cast<chrono::microseconds>(end -
    // start).count(); log_info("Total write latency for map: {}", dur);
    //
    // std::vector<void *> recent_buffer = dumpRecentWriteMap(mInflightMap);
    // n = recent_buffer.size();
    // int size = sizeof(u8);
    // send = malloc(size);
    // send_mr = ibv_reg_mr(ep->GetPd(), send, size,
    // IBV_ACCESS_LOCAL_WRITE);
    //
    // // Test latency for magic mr
    // start = clock_type::now();
    // for (auto &x : recent_buffer) {
    //     auto stat = ep->PostWrite(reinterpret_cast<uint64_t>(&x),
    //     send_mr->lkey,
    //                               send, size, ci->magic_addr + 2 * 1024 *
    //                               1024, ci->magic_key);
    //     if (!stat.ok()) {
    //         std::cerr << "Error writing " << stat << std::endl;
    //         return outcome::failure(std::error_code());
    //     }
    //     auto wc_s = ep->PollSendCq();
    //     if (!wc_s.ok()) {
    //         std::cerr << "error polling send cq for write " <<
    //         wc_s.status()
    //                   << std::endl;
    //     }
    // }
    // end = clock_type::now();
    // dur = chrono::duration_cast<chrono::microseconds>(end -
    // start).count(); log_info("Total write latency for recent write map:
    // {}", dur);

    return outcome::success();
}

void ZstoreController::ZkSet(const std::string &path, const char *data)
{
    int rc = zoo_set(mZkHandler, path.c_str(), data, 10, -1);
    if (rc != ZOK) {
        log_error("Error setting data to {}", path);
    } else {
        log_info("Success setting data to {}", path);
    }
}

void ZstoreController::Map2Tx(const ZstoreMap &hashmap,
                              std::vector<char *> &tx_map)
{
    std::vector<char *> tmpBuffer;
    tmpBuffer.reserve(mMap.size());
    char *buffer = new char[sizeof(ObjectKeyHash) + sizeof(MapEntry)];
    log_debug("size of tmpBuffer: {}",
              sizeof(ObjectKeyHash) + sizeof(MapEntry));
    hashmap.visit_all([&tmpBuffer, buffer](auto &x) {
        std::memcpy(buffer, &x.first, sizeof(ObjectKeyHash));
        std::memcpy(buffer + sizeof(ObjectKeyHash), &x.second,
                    sizeof(MapEntry));
        tmpBuffer.push_back(buffer);
    });

    // Split tempBuffer into 4096-byte chunks
    size_t bufferSize;
    if (Conf::GetObjectSizeInBytes() > Conf::GetChunkSize()) {
        bufferSize = Conf::GetChunkSize();
    } else
        bufferSize = Conf::GetObjectSizeInBytes();

    size_t totalSize = tmpBuffer.size();
    size_t numBuffers =
        (totalSize + bufferSize - 1) / bufferSize; // Round up division
    tx_map.resize(numBuffers);

    for (size_t i = 0; i < numBuffers; ++i) {
        tx_map[i] = new char[bufferSize];
        size_t copySize = std::min(bufferSize, totalSize - i * bufferSize);
        std::memcpy(tx_map[i], tmpBuffer.data() + i * bufferSize, copySize);

        // Zero out remaining space if not a full buffer
        if (copySize < bufferSize) {
            std::memset(tx_map[i] + copySize, 0, bufferSize - copySize);
        }
    }
}

void ZstoreController::Map2Gc(
    const ZstoreMap &hashmap, std::vector<char *> &prunedObjects,
    std::vector<std::pair<Location, Length>> &gcObjectsToMove)
{
    char *buffer = new char[sizeof(ObjectKeyHash) + sizeof(MapEntry)];
    log_debug("size of tmpBuffer: {}",
              sizeof(ObjectKeyHash) + sizeof(MapEntry));
    hashmap.visit_all([&prunedObjects, &gcObjectsToMove, buffer](auto &x) {
        std::memcpy(buffer, &x.first, sizeof(ObjectKeyHash));
        std::memcpy(buffer + sizeof(ObjectKeyHash), &x.second,
                    sizeof(MapEntry));
        auto [loc, len, tombstone] = x.second;
        if (!tombstone) {
            // log_debug("tombstone: {}", tombstone);
            prunedObjects.push_back(buffer);
        }
        // log_debug("location: first {}, second {}, third {}",
        //           loc[0].device_id, loc[1].device_id,
        //           loc[2].device_id);
        if (tombstone && (loc[0].device_id == 0 || loc[1].device_id == 0 ||
                          loc[2].device_id == 0)) {
            if (loc[0].device_id == 0) {
                gcObjectsToMove.push_back(std::make_pair(loc[1], len));
            } else if (loc[1].device_id == 0) {
                gcObjectsToMove.push_back(std::make_pair(loc[2], len));
            } else if (loc[2].device_id == 0) {
                gcObjectsToMove.push_back(std::make_pair(loc[0], len));
            }
        }
    });

    log_info("GC objects (on device 1) size: {}, tombstone objects size: {}",
             gcObjectsToMove.size(), prunedObjects.size());
}

/* NOTE workflow for persisting map
 * 1. zookeeper will select a server (leader) perform checkpoint
 * 2. leader will announce epoch change to all servers (N -> N+1). Each
 *    follower will (1) create new map and new bloom filter for N+1,
 *    (2) the leader will take existing map and write to disk, (3) if the
 *    follower detects that the leader has persisted the map, it will set
 *    /tx/follower to commit
 */
Result<void> ZstoreController::Checkpoint()
{
    assert(!Conf::IsReplay() &&
           "Checkpointing should not be used in replay mode");
    // create /tx/nodeName_ under /tx for every znode
    if (nodeName_ == leaderNodeName_) {
        log_debug("Checkpoint start");
        // increase epoch
        {
            auto current_epoch = GetEpoch();
            mEpoch += 1;
            auto new_epoch = GetEpoch();
        }
        Stat stat;

        // checkpoint has already started or not
        mCkptStart = std::chrono::high_resolution_clock::now();
        mCkpt = true;

        // get all children
        String_vector children;
        int rc = zoo_get_children(mZkHandler, election_root_.c_str(), true,
                                  &children);
        assert(rc == ZOK);

        if (children.count > 0) {
            for (int i = 0; i < children.count; i++) {
                std::string path = election_root_ + "/" + children.data[i];
                auto node = getNodeData(path);

                std::string tx_path = tx_root_ + "/" + node;
                log_info("create children under /tx: {}, tx_path {}", node,
                         tx_path);
                if (zoo_exists(mZkHandler, tx_path.c_str(), false, &stat) !=
                    ZOK) {
                    if (Conf::IsDebugging())
                        log_info("{} does not exist", tx_path);
                    int rc =
                        zoo_create(mZkHandler, tx_path.c_str(), "empty_", 6,
                                   &ZOO_OPEN_ACL_UNSAFE, ZOO_EPHEMERAL, 0, 0);
                    if (rc != ZOK) {
                        log_error("Error creating znode {}", tx_path);
                    } else {
                        if (Conf::IsDebugging())
                            log_info("Success creating znode {}", tx_path);
                    }

                    if (node != leaderNodeName_) {
                        log_debug("Setting TX watcher on {}", tx_path);
                        // leader needs to watch if follower has committed
                        rc = zoo_wexists(mZkHandler, tx_path.c_str(), TxWatcher,
                                         this, NULL);
                        if (rc != ZOK) {
                            log_error("Error setting watcher on {}", tx_path);
                        } else {
                            if (Conf::IsDebugging())
                                log_info("Success setting watcher on {}",
                                         tx_path);
                        }
                    }
                }
            }
        }
        sleep(1);

        // std::string workload = "checkpoint";
        std::string workload = "gc";

        if (mHasRunCkpt)
            return outcome::success();

        log_error("\nRun\n");
        sleep(1);

        // Checkpoint
        if (workload == "checkpoint") {
            log_error("\nTX map start: start WRK\n");
            mHasRunCkpt = true;

            std::vector<char *> tx_map;
            Map2Tx(mMap, tx_map);

            // we might want to set a threshold
            assert(tx_map.size() <= mContextPoolSize);
            std::vector<RequestContext *> reqs;
            reqs.reserve(tx_map.size());
            char *buffer = (char *)spdk_zmalloc(
                Conf::GetObjectSizeInBytes(), Conf::GetBlockSize(), NULL,
                SPDK_ENV_SOCKET_ID_ANY, SPDK_MALLOC_DMA);
            auto dev = GetDevice("Zstore2Dev1");
            for (u64 i = 0; i < tx_map.size(); i++) {
                buffer = tx_map[i];
                auto slot = MakeWriteChunk(this, dev, buffer,
                                           Conf::GetObjectSizeInBytes())
                                .value();
                reqs.push_back(slot);
            }
            log_info("reqs size: {}", reqs.size());

            asio::io_context ctx{10};
            asio::co_spawn(
                ctx,
                [reqs]() -> asio::awaitable<void> {
                    auto ex = co_await asio::this_coro::executor;
                    using Task = decltype(co_spawn(ex, zoneAppend(reqs[0]),
                                                   asio::deferred));

                    constexpr size_t MAX_CONCURRENCY = 256;

                    for (size_t start = 0; start < reqs.size();
                         start += MAX_CONCURRENCY) {
                        size_t end =
                            std::min(start + MAX_CONCURRENCY, reqs.size());
                        std::vector<Task> batch;

                        for (size_t i = start; i < end; ++i) {
                            batch.push_back(co_spawn(ex, zoneAppend(reqs[i]),
                                                     asio::deferred));
                        }

                        auto group = asio::experimental::make_parallel_group(
                            std::move(batch));
                        auto result = co_await group.async_wait(
                            asio::experimental::wait_for_all(),
                            asio::use_awaitable);
                    }
                },
                asio::detached);
            ctx.run();

            for (auto &slot : reqs) {
                slot->Clear();
                if (Conf::IsReplay())
                    log_error("Do not run replay with checkpointing!!!");
                else
                    mRequestContextPool->ReturnRequestContext(slot);
            }

            // leader commit since all data is written
            std::string path = tx_root_ + "/" + leaderNodeName_;
            ZkSet(path, "commit");
            log_error("\nTX map finished: start WRK\n");

        } // end of Checkpoint
        else if (workload == "gc") {
            log_error("\nGC start\n");
            mHasRunCkpt = true;

            std::vector<char *> prunedObjects;
            prunedObjects.reserve(mMap.size());

            std::vector<std::pair<Location, Length>> gcObjectsToMove;
            gcObjectsToMove.reserve(mMap.size() / 100);

            Map2Gc(mMap, prunedObjects, gcObjectsToMove);

            // we might want to set a threshold
            std::vector<RequestContext *> read_reqs;
            read_reqs.reserve(gcObjectsToMove.size());

            auto dev = GetDevice("Zstore2Dev1");
            for (u64 i = 0; i < gcObjectsToMove.size(); i++) {
                auto slot =
                    MakeReadRequest(this, dev, gcObjectsToMove[i].first.lba,
                                    Conf::GetObjectSizeInBytes())
                        .value();
                read_reqs.push_back(slot);
            }
            log_error("read_reqs size: {}", read_reqs.size());

            // we can do 500 but not 1000 at a time
            asio::io_context ctx{10};
            asio::co_spawn(
                ctx,
                [read_reqs]() -> asio::awaitable<void> {
                    auto ex = co_await asio::this_coro::executor;
                    using Task = decltype(co_spawn(ex, zoneRead(read_reqs[0]),
                                                   asio::deferred));
                    constexpr size_t MAX_CONCURRENCY = 256;

                    for (size_t start = 0; start < read_reqs.size();
                         start += MAX_CONCURRENCY) {
                        size_t end =
                            std::min(start + MAX_CONCURRENCY, read_reqs.size());
                        std::vector<Task> batch;

                        for (size_t i = start; i < end; ++i) {
                            batch.push_back(co_spawn(ex, zoneRead(read_reqs[i]),
                                                     asio::deferred));
                        }

                        auto group = asio::experimental::make_parallel_group(
                            std::move(batch));
                        auto result = co_await group.async_wait(
                            asio::experimental::wait_for_all(),
                            asio::use_awaitable);
                    }
                },
                asio::detached);
            ctx.run();

            std::vector<RequestContext *> write_reqs;
            write_reqs.reserve(gcObjectsToMove.size());

            for (auto &slot : read_reqs) {
                write_reqs.push_back(
                    MakeWriteChunk(this, dev, slot->dataBuffer,
                                   Conf::GetObjectSizeInBytes())
                        .value());
                slot->Clear();
                mRequestContextPool->ReturnRequestContext(slot);
            }
            read_reqs.clear();

            log_error("write_reqs size: {}", write_reqs.size());

            asio::io_context ctx2;
            asio::co_spawn(
                ctx2,
                [write_reqs]() -> asio::awaitable<void> {
                    auto ex = co_await asio::this_coro::executor;
                    using Task = decltype(co_spawn(
                        ex, zoneAppend(write_reqs[0]), asio::deferred));
                    constexpr size_t MAX_CONCURRENCY = 256;

                    for (size_t start = 0; start < write_reqs.size();
                         start += MAX_CONCURRENCY) {
                        size_t end = std::min(start + MAX_CONCURRENCY,
                                              write_reqs.size());
                        std::vector<Task> batch;

                        for (size_t i = start; i < end; ++i) {
                            batch.push_back(co_spawn(
                                ex, zoneAppend(write_reqs[i]), asio::deferred));
                        }

                        auto group = asio::experimental::make_parallel_group(
                            std::move(batch));
                        auto result = co_await group.async_wait(
                            asio::experimental::wait_for_all(),
                            asio::use_awaitable);
                    }
                },
                asio::detached);
            ctx2.run();
            for (auto &slot : write_reqs) {
                slot->Clear();
                mRequestContextPool->ReturnRequestContext(slot);
            }
            write_reqs.clear();

            // leader commit since all data is written
            std::string path = tx_root_ + "/" + leaderNodeName_;
            ZkSet(path, "commit");
            log_error("\nGC finished\n");
        } // end of GC
        else if (workload == "target_recovery") {
        } // end of target recovery
        else if (workload == "gateway_recovery") {
        } // end of gateway recovery
    } else {
        // follower

        // Deprecated:
        // Send all records in current gateway to the leader gateway
        // auto ret = SendRecordsToGateway();
        // assert(ret && "SendRecordsToGateway failed");

        // follower sets a watcher on /tx/leader to check if the leader has
        // persisted map
        std::string path = tx_root_ + "/" + leaderNodeName_;
        log_debug("Setting Leader watcher on {}", path);

        auto node = getNodeData(path);
        log_info("data from {}: {}", path, node);

        int rc =
            zoo_wexists(mZkHandler, path.c_str(), LeaderWatcher, this, NULL);
        if (rc != ZOK) {
            log_error("Error setting watcher on leader {}", path);
        } else {
            if (Conf::IsDebugging())
                log_info("Success setting watcher on leader {}", path);
        }

        // move map
        {
            auto map_to_persist = mMap;
            auto recent_write_map_to_persist = mInflight;

            mMap.clear();
            mInflight.clear();
        }
    }
    return outcome::success();
}
