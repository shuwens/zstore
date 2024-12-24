#include "include/zstore_controller.h"
#include "include/configuration.h"
#include "include/http_server.h"
#include "src/include/utils.h"
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/outcome/utils.hpp>
#include <boost/serialization/map.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/utility.hpp>
#include <fstream>
#include <iostream>
#include <spdk/nvme_zns.h>
#include <spdk/string.h>
#include <string>
#include <thread>
#include <zookeeper/zookeeper.h>

using tcp = boost::asio::ip::tcp; // from <boost/asio/ip/tcp.hpp>
const std::string election_root_ = "/election";
const std::string tx_root_ = "/tx";
const std::string election_node_path = "/election/node_";
#define SESSION_TIMEOUT 30000
std::string g_data;

int ZstoreController::Init(bool object, int key_experiment, int phase)
{

    int rc = 0;
    verbose = Configuration::Verbose();

    setContextPoolSize(Configuration::GetContextPoolSize());
    setNumOfDevices(Configuration::GetNumOfDevices() *
                    Configuration::GetNumOfTargets());
    setKeyExperiment(key_experiment);
    setPhase(phase);

    if (mKeyExperiment == 1) {
        log_info("Init Zstore for random read, starting from zone {}",
                 mKeyExperiment, mPhase, Configuration::GetZoneId());
    } else if (mKeyExperiment == 2) {
        if (mPhase == 1) {
            log_info("Init Zstore for write and read.", mKeyExperiment, mPhase);
        } else if (mPhase == 2) {
            log_info("Init Zstore for write and read.", mKeyExperiment, mPhase);
            log_info("Run phase, load the map and the bloom filter");
        } else if (mPhase == 3) {
            log_info("Init Zstore for write and read simplified.",
                     mKeyExperiment, mPhase);
        }
    } else if (mKeyExperiment == 3) {
        // Target failure
    } else if (mKeyExperiment == 4) {
        // gateway failure
    } else if (mKeyExperiment == 5) {
        // Target and gateway failure
    } else if (mKeyExperiment == 6) {
        // Checkpoint
        if (mPhase == 1) {
            log_info("Init Zstore for Checkpointing Gateway 1", mKeyExperiment,
                     mPhase);
            SetGateway(1);
        } else if (mPhase == 2) {
            log_info("Init Zstore for Checkpointing Gateway 2", mKeyExperiment,
                     mPhase);
            SetGateway(2);
        }
        nodeName_ = "gateway_" + std::to_string(GetGateway());
    } else if (mKeyExperiment == 7) {
        // GC
    }

    // TODO: set all parameters too
    log_debug(
        "Configuration: sector size {}, context pool size "
        "{}, targets {}, devices {}",
        Configuration::GetBlockSize(), Configuration::GetContextPoolSize(),
        Configuration::GetNumOfTargets(), Configuration::GetNumOfDevices());

    std::vector<std::tuple<std::string, std::string, std::string, u32, u32>>
        ip_port_devs;
    boost::asio::ip::address address;
    if (mKeyExperiment == 6) {
        if (mPhase == 1) {
            address = asio::ip::make_address("12.12.12.1");
            ip_port_devs.push_back(std::make_tuple(
                "nqn.2024-04.io.zstore2:cnode1", "12.12.12.2", "5520",
                Configuration::GetZoneId(), Configuration::GetZoneId()));
            ip_port_devs.push_back(std::make_tuple(
                "nqn.2024-04.io.zstore3:cnode1", "12.12.12.3", "5520",
                Configuration::GetZoneId(), Configuration::GetZoneId()));
        } else if (mPhase == 2) {
            address = asio::ip::make_address("12.12.12.6");
            ip_port_devs.push_back(std::make_tuple(
                "nqn.2024-04.io.zstore4:cnode1", "12.12.12.4", "5520",
                Configuration::GetZoneId(), Configuration::GetZoneId()));
            ip_port_devs.push_back(std::make_tuple(
                "nqn.2024-04.io.zstore5:cnode1", "12.12.12.5", "5520",
                Configuration::GetZoneId(), Configuration::GetZoneId()));
        } else {
            log_error("Invalid phase");
        }
    } else {
        address = asio::ip::make_address("12.12.12.1");
        ip_port_devs.push_back(std::make_tuple(
            "nqn.2024-04.io.zstore2:cnode1", "12.12.12.2", "5520",
            Configuration::GetZoneId(), Configuration::GetZoneId()));
        ip_port_devs.push_back(std::make_tuple(
            "nqn.2024-04.io.zstore3:cnode1", "12.12.12.3", "5520",
            Configuration::GetZoneId(), Configuration::GetZoneId()));
        ip_port_devs.push_back(std::make_tuple(
            "nqn.2024-04.io.zstore4:cnode1", "12.12.12.4", "5520",
            Configuration::GetZoneId(), Configuration::GetZoneId()));
        ip_port_devs.push_back(std::make_tuple(
            "nqn.2024-04.io.zstore5:cnode1", "12.12.12.5", "5520",
            Configuration::GetZoneId(), Configuration::GetZoneId()));
    }

    for (auto &dev_tuple : ip_port_devs) {
        if (register_controllers(g_devices, dev_tuple) != 0) {
            rc = 1;
            zstore_cleanup();
            return rc;
        }
    }
    mDevices = g_devices;

    // Preallocate contexts for user requests
    // Sufficient to support multiple I/O queues of NVMe-oF target
    mRequestContextPool = new RequestContextPool(mContextPoolSize);

    if (mRequestContextPool == NULL) {
        log_error("could not initialize task pool");
        rc = 1;
        zstore_cleanup();
        return rc;
    }
    isDraining = false;
    // bogus setup for Map and BF

    // Create poll groups for the io threads and perform initialization
    for (int threadId = 0; threadId < Configuration::GetNumIoThreads();
         ++threadId) {
        mIoThread[threadId].group = spdk_nvme_poll_group_create(NULL, NULL);
        mIoThread[threadId].controller = this;
    }
    for (int i = 0; i < mN; ++i) {
        struct spdk_nvme_qpair **ioQueues = mDevices[i]->GetIoQueues();
        for (int threadId = 0; threadId < Configuration::GetNumIoThreads();
             ++threadId) {
            spdk_nvme_ctrlr_disconnect_io_qpair(ioQueues[threadId]);
            int rc = spdk_nvme_poll_group_add(mIoThread[threadId].group,
                                              ioQueues[threadId]);
            assert(rc == 0);
        }
        mDevices[i]->ConnectIoPairs();
    }

    if (Configuration::Debugging())
        CheckIoQpair("Starting all the threads");

    log_debug("ZstoreController launching threads");

    initIoThread();

    auto const port = 2000;

    // The io_context is required for all I/O
    auto const num_threads = Configuration::GetNumHttpThreads();
    asio::io_context ioc{num_threads};

    // Spawn a listening port
    asio::co_spawn(ioc, do_listen(tcp::endpoint{address, port}, *this),
                   [](std::exception_ptr e) {
                       if (e)
                           try {
                               std::rethrow_exception(e);
                           } catch (std::exception &e) {
                               std::cerr << "Error in acceptor: " << e.what()
                                         << "\n";
                           }
                   });

    std::vector<std::jthread> threads(num_threads);
    for (unsigned i = 0; i < num_threads; ++i) {
        threads[i] = std::jthread([&ioc, i, &threads] {
#ifdef PERF
            // Create a cpu_set_t object representing a set of CPUs.
            // Clear it and mark only CPU i as set.
            cpu_set_t cpuset;
            CPU_ZERO(&cpuset);
            CPU_SET(i + Configuration::GetHttpThreadCoreId(), &cpuset);
            std::string name = "zstore_ioc" + std::to_string(i);
            // log_info("HTTP server: Thread {} on core {}", i,
            //          i + Configuration::GetHttpThreadCoreId());
            int rc =
                pthread_setname_np(threads[i].native_handle(), name.c_str());
            if (rc != 0) {
                log_error("HTTP server: Error calling pthread_setname: {}", rc);
            }
            rc = pthread_setaffinity_np(threads[i].native_handle(),
                                        sizeof(cpu_set_t), &cpuset);
            if (rc != 0) {
                log_error(
                    "HTTP server: Error calling pthread_setaffinity_np: {}",
                    rc);
            }
            log_info("HTTP server: Thread {} on core {}", i,
                     i + Configuration::GetHttpThreadCoreId());
#endif
            ioc.run();
        });
    }

    log_info("Initialization complete. Launching workers.");

    rc = PopulateDevHash();
    assert(rc == 0);

    rc = PopulateMap();
    assert(rc == 0);

    pivot = 0;

    // Read zone headers

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

    if (mKeyExperiment == 6) {
        startZooKeeper();
        sleep(5);
        Checkpoint();
    }

    // RDMA circular buffer initialization

    log_info("ZstoreController Init finish");

    return rc;
}

// this yields a list of devices for a given object key
//
// right now we create 120 (6*5*4) tuples, to consider ordering of every
// tuple
int ZstoreController::PopulateDevHash()
{
    std::unique_lock lock(mDevHashMutex);
    std::vector<std::pair<TargetDev, u32>> tgt_dev_vec;

    // permutate Zstore2-Zstore4, Dev1-2
    // std::vector<std::string> tgt_list({"Zstore2", "Zstore3", "Zstore4"});
    // std::vector<std::string> dev_list({"Dev1", "Dev2"});
    // for (int i = 0; i < tgt_list.size(); i++) {
    //     for (int j = 0; j < dev_list.size(); j++) {
    //         tgt_dev_vec.push_back({tgt_list[i] + dev_list[j]});
    //     }
    // }

    // this seems to be stupid, but we are just manually adding the target
    // device and the zone we write to here
    tgt_dev_vec.push_back(
        {std::make_pair("Zstore2Dev1", Configuration::GetZoneId())});
    tgt_dev_vec.push_back(
        {std::make_pair("Zstore2Dev2", Configuration::GetZoneId())});
    tgt_dev_vec.push_back(
        {std::make_pair("Zstore3Dev1", Configuration::GetZoneId())});
    tgt_dev_vec.push_back(
        {std::make_pair("Zstore3Dev2", Configuration::GetZoneId())});
    tgt_dev_vec.push_back(
        {std::make_pair("Zstore4Dev1", Configuration::GetZoneId())});
    tgt_dev_vec.push_back(
        {std::make_pair("Zstore4Dev2", Configuration::GetZoneId())});

    for (int i = 0; i < tgt_dev_vec.size(); i++) {
        for (int j = 0; j < tgt_dev_vec.size(); j++) {
            if (i == j) {
                continue;
            }
            for (int k = 0; k < tgt_dev_vec.size(); k++) {
                if (i == k || j == k) {
                    continue;
                }
                mDevHash.push_back({std::make_tuple(
                    tgt_dev_vec[i], tgt_dev_vec[j], tgt_dev_vec[k])});
            }
        }
    }
    // for (int i = 0; i < mDevHash.size(); i++) {
    //     log_debug("DevHash: {}", i);
    //     log_debug("DevHash: {}", mDevHash[i]);
    // }
    return 0;
}

Result<DevTuple>
ZstoreController::GetDevTupleForRandomReads(ObjectKeyHash key_hash)
{
    return std::make_tuple(
        std::make_pair("Zstore2Dev2", Configuration::GetZoneId()),
        std::make_pair("Zstore4Dev1", Configuration::GetZoneId()),
        std::make_pair("Zstore4Dev2", Configuration::GetZoneId()));
}

Result<MapEntry> ZstoreController::CreateFakeObject(ObjectKeyHash key_hash,
                                                    DevTuple tuple)
{
    auto entry = createMapEntry(tuple, 0, 1, 0, 1, 0, 1);
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

// Deprecated
void tryDrainController(void *args)
{
    DrainArgs *drainArgs = (DrainArgs *)args;
    // drainArgs->ctrl->CheckSegments();
    // drainArgs->ctrl->ReclaimContexts();
    // drainArgs->ctrl->ProceedGc();
    drainArgs->success =
        drainArgs->ctrl->mRequestContextPool->availableContexts.size() ==
        drainArgs->ctrl->GetContextPoolSize();

    drainArgs->ready = true;
}

static void busyWait(bool *ready)
{
    while (!*ready) {
        if (spdk_get_thread() == nullptr) {
            std::this_thread::sleep_for(std::chrono::seconds(0));
        }
    }
}

// Bloom filter APIs
Result<bool>
ZstoreController::SearchRecentWriteMap(const ObjectKeyHash &key_hash)
{
    return mRecentWriteMap.contains(key_hash);
}

Result<bool>
ZstoreController::UpdateRecentWriteMap(const ObjectKeyHash &key_hash)
{
    return mRecentWriteMap.insert_or_assign(key_hash, GetGateway());
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

Result<std::vector<ObjectKeyHash>> ZstoreController::ListObjects()
{
    std::vector<ObjectKeyHash> keys;
    mMap.visit_all([&](auto &x) { keys.push_back(x.first); });
    return keys;
}

Result<bool> ZstoreController::AddGcObject(const TargetLbaTuple &tuple)
{
    return mGcSet.insert(tuple);
}

void ZstoreController::initIoThread()
{
    struct spdk_cpuset cpumask;
    for (int threadId = 0; threadId < Configuration::GetNumIoThreads();
         ++threadId) {
        spdk_cpuset_zero(&cpumask);
        spdk_cpuset_set_cpu(&cpumask,
                            Configuration::GetIoThreadCoreId(threadId), true);
        mIoThread[threadId].thread = spdk_thread_create("IoThread", &cpumask);
        assert(mIoThread[threadId].thread != nullptr);
        mIoThread[threadId].controller = this;
        int rc = spdk_env_thread_launch_pinned(
            Configuration::GetIoThreadCoreId(threadId), ioWorker,
            &mIoThread[threadId]);
        log_info("IO thread name {} id {} on core {}",
                 spdk_thread_get_name(mIoThread[threadId].thread),
                 spdk_thread_get_id(mIoThread[threadId].thread),
                 Configuration::GetIoThreadCoreId(threadId));
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

    if (verbose) {
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

// TODO: assume we only have one device, we should check all device in the
// end
bool ZstoreController::CheckIoQpair(std::string msg)
{
    assert(mDevices[0] != nullptr);
    assert(mDevices[0]->GetIoQueue(0) != nullptr);
    if (!spdk_nvme_qpair_is_connected(mDevices[0]->GetIoQueue(0)))
        exit(0);
    return spdk_nvme_qpair_is_connected(mDevices[0]->GetIoQueue(0));
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
    for (int i = 0; i < Configuration::GetNumIoThreads(); ++i) {
        thread_send_msg(mIoThread[i].thread, quit, nullptr);
    }
    // thread_send_msg(mDispatchThread, quit, nullptr);
    // for (int i = 0; i < Configuration::GetNumHttpThreads(); ++i) {
    //     thread_send_msg(mHttpThread[i].thread, quit, nullptr);
    // }
    // log_debug("drain io: {}", spdk_get_ticks());
    // log_debug("clean up ns worker");
    cleanup_ns_worker_ctx();
    // log_debug("end work fn");
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

void ZstoreController::Drain()
{
    // FIXME GetDevice default
    log_info("Perform draining on the system.");
    isDraining = true;
    DrainArgs args;
    args.ctrl = this;
    args.success = false;
    while (!args.success) {
        args.ready = false;
        // thread_send_msg(mDispatchThread, tryDrainController, &args);
        busyWait(&args.ready);
    }

    auto etime = std::chrono::high_resolution_clock::now();
    auto delta =
        std::chrono::duration_cast<std::chrono::microseconds>(etime - stime)
            .count();
    auto tput = mTotalCounts * g_micro_to_second / delta;

    // if (g->verbose)
    log_info("Total IO {}, total time {}ms, throughput {} IOPS", mTotalCounts,
             delta, tput);

    // log_debug("drain io: {}", spdk_get_ticks());
    log_debug("clean up ns worker");
    // ctrl->cleanup_ns_worker_ctx();
    // print_stats(ctrl);
    // exit(0);

    log_info("done .");
}

// void ZstoreController::writeMapToFile(const std::string &filename)
// {
//     try {
//         std::ofstream file_out(filename, std::ios::out |
//         std::ios::trunc); if (!file_out.is_open()) {
//             throw std::runtime_error("Could not open file for writing");
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
//             throw std::runtime_error("Could not open file for reading");
//         }
//         boost::archive::text_iarchive archive(file_in);
//         archive >> mMap; // Deserialize the map from the file
//         file_in.close();
//         std::cout << "Map successfully loaded from file: " << filename
//                   << std::endl;
//     } catch (const std::exception &e) {
//         std::cerr << "Error reading map from file: " << e.what() <<
//         std::endl;
//     }
// }

int ZstoreController::PopulateMap()
{
    // TODO: 4KiB, 4MiB, 1GiB ....
    if (mKeyExperiment == 1) {
        log_info("Populate Map({},{}): random read, starting from zone {}",
                 mKeyExperiment, mPhase, Configuration::GetZoneId());
        // Random Read
        u32 current_zone = Configuration::GetZoneId();
        u64 current_lba = 0;
        u64 zone_offset = 0;
        for (int i = 0; i < _map_size; i++) {
            std::string device;
            // if (i % 6 == 0) {
            //     device = "Zstore2Dev1";
            // } else if (i % 6 == 1) {
            //     device = "Zstore2Dev2";
            // } else if (i % 6 == 2) {
            //     device = "Zstore3Dev1";
            // } else if (i % 6 == 3) {
            //     device = "Zstore3Dev2";
            // } else if (i % 6 == 4) {
            //     device = "Zstore4Dev1";
            // } else if (i % 6 == 5) {
            //     device = "Zstore4Dev2";
            // }

            if (i % 2 == 0) {
                device = "Zstore2Dev1";
            } else if (i % 2 == 1) {
                device = "Zstore2Dev2";
            }
            // else if (i % 4 == 2) {
            //     device = "Zstore4Dev1";
            // } else if (i % 4 == 3) {
            //     device = "Zstore4Dev2";
            // }

            // the lba should be zone_num * 0x80000 + offset [0, 0x43500]
            zone_offset = current_lba % Configuration::GetZoneDist();
            if (zone_offset > Configuration::GetZoneCap()) {
                current_zone++;
                current_lba = current_zone * Configuration::GetZoneDist();
                log_debug("Populate Map for index {}, current lba {}: zone {} "
                          "is full, moving to next zone",
                          i, current_lba, current_zone);
            }

            auto entry =
                createMapEntry(
                    std::make_tuple(
                        std::make_pair(device, Configuration::GetZoneId()),
                        std::make_pair("Zstore2Dev2",
                                       Configuration::GetZoneId()),
                        std::make_pair("Zstore3Dev1",
                                       Configuration::GetZoneId())),
                    current_lba,
                    Configuration::GetObjectSizeInBytes() /
                        Configuration::GetBlockSize(),
                    current_lba,
                    Configuration::GetObjectSizeInBytes() /
                        Configuration::GetBlockSize(),
                    current_lba,
                    Configuration::GetObjectSizeInBytes() /
                        Configuration::GetBlockSize())
                    .value();
            mMap.emplace(computeSHA256(std::to_string(i)), entry);
            current_lba++;
        }
    } else if (mKeyExperiment == 2) {
        if (mPhase == 1) {
            log_info("Populate Map({},{}): write and read.", mKeyExperiment,
                     mPhase);
            //     log_info("Prepare phase, do nothing");
            // DumpAllMap();
        } else if (mPhase == 2) {
            log_info("Populate Map({},{}): write and read.", mKeyExperiment,
                     mPhase);
            log_info("Run phase, load the map and the bloom filter");
            // ReadAllMap();
        } else if (mPhase == 3) {
            log_info("Populate Map({},{}): write and read simplified.",
                     mKeyExperiment, mPhase);
        }

        // Sequential write (append) and read
        // for (int i = 0; i < 2'000'000; i++) {
        //     mMap.insert({"/db/" + std::to_string(i),
        //                  createMapEntry("device", i).value()});
        // }
    } else if (mKeyExperiment == 3) {
        // Target failure
    } else if (mKeyExperiment == 4) {
        // gateway failure
    } else if (mKeyExperiment == 5) {
        // Target and gateway failure
    } else if (mKeyExperiment == 6) {
        // GC
    } else if (mKeyExperiment == 7) {
        // Checkpoint
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
    char *buf = new char[256];
    int len = 256;

    String_vector children;
    if (zoo_get_children(mZkHandler, election_root_.c_str(), true, &children) ==
        ZOK) {
        if (children.count > 0) {
            for (int i = 0; i < children.count; i++) {
                std::string path = election_root_ + "/" + children.data[i];
                auto node = getNodeData(path);

                std::string tx_path = tx_root_ + "/" + node;
                int rc = zoo_get(mZkHandler, tx_path.c_str(), false, buf, &len,
                                 NULL);
                if (rc != ZOK) {
                    log_error("Error getting data from {}", tx_path);
                } else {
                    std::string input(buf);
                    log_info("data from {}: {}", tx_path, buf);
                    if (input == "empty") {
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

                    } else if (input == "abort") {
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

static void TxWatcher(zhandle_t *zkH, int type, int state, const char *path,
                      void *watcherCtx)
{
    ZstoreController *ctrl = static_cast<ZstoreController *>(watcherCtx);
    if (type == ZOO_CHANGED_EVENT) {
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
        // ZOO_NOTCONNECTED_STATE If you are using ACL, you should be aware
        // of an authentication failure state - ZOO_AUTH_FAILED_STATE
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
    mZkHandler = zookeeper_init(Configuration::GetZkHost().c_str(), ZkWatcher,
                                SESSION_TIMEOUT, 0, this, 0);
    if (!mZkHandler) {
        log_error("Error connecting to ZooKeeper server.");
        return;
    }
    log_info("Connecting to ZooKeeper server...");
}

/* NOTE workflow for persisting map
 * 1. zookeeper will select a server (leader) perform checkpoint
 * 2. leader will announce epoch change to all servers (N -> N+1). Each
 *    follower will (1) create new map and new bloom filter for N+1, and
 *    (2) return list of writes and wp for each device
 */
Result<void> ZstoreController::Checkpoint()
{
    // create /tx/nodeName_ under /tx for every znode
    if (leaderNodeName_ == nodeName_) {
        sleep(5);
        // increase epoch
        {
            auto current_epoch = GetEpoch();
            mEpoch += 1;
            auto new_epoch = GetEpoch();
        }
        Stat stat;

        // get all children
        String_vector children;
        if (zoo_get_children(mZkHandler, election_root_.c_str(), true,
                             &children) == ZOK) {
            if (children.count > 0) {
                for (int i = 0; i < children.count; i++) {
                    std::string path = election_root_ + "/" + children.data[i];
                    auto node = getNodeData(path);

                    std::string tx_path = tx_root_ + "/" + node;
                    log_info("create children under /tx: {}, tx_path {}", node,
                             tx_path);
                    if (zoo_exists(mZkHandler, tx_path.c_str(), false, &stat) !=
                        ZOK) {
                        log_info("{} does not exist", tx_path);
                        int rc = zoo_create(mZkHandler, tx_path.c_str(),
                                            "empty", 10, &ZOO_OPEN_ACL_UNSAFE,
                                            ZOO_EPHEMERAL, 0, 0);
                        if (rc != ZOK) {
                            log_error("Error creating znode {}", tx_path);
                        } else {
                            log_info("Success creating znode {}", tx_path);
                        }
                        rc = zoo_wexists(mZkHandler, tx_path.c_str(), TxWatcher,
                                         this, NULL);
                        if (rc != ZOK) {
                            log_error("Error setting watcher on {}", tx_path);
                        } else {
                            log_info("Success setting watcher on {}", tx_path);
                        }
                    }
                }
            }
            deallocate_String_vector(&children);
        }
    }

    sleep(5);
    std::string path = tx_root_ + "/" + nodeName_;
    int rc = zoo_set(mZkHandler, path.c_str(), "commit", 10, -1);
    if (rc != ZOK) {
        log_error("Error setting data to {}", path);
    } else {
        log_info("Success setting data to {}", path);
    }

    // move map
    {
        auto map_to_persist = mMap;
        auto recent_write_map_to_persist = mRecentWriteMap;

        mMap.clear();
        mRecentWriteMap.clear();
    }

    return outcome::success();
}
