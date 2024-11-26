#include "include/zstore_controller.h"
#include "include/configuration.h"
#include "include/http_server.h"
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/serialization/map.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/utility.hpp>
#include <fstream>
#include <iostream>
#include <spdk/nvme_zns.h>
#include <spdk/string.h>
#include <string>
#include <thread>

using tcp = boost::asio::ip::tcp; // from <boost/asio/ip/tcp.hpp>

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
Result<bool> ZstoreController::SearchBF(const ObjectKeyHash &key_hash)
{
    return mBF.contains(key_hash);
}

Result<bool> ZstoreController::UpdateBF(const ObjectKeyHash &key_hash)
{
    return mBF.insert(key_hash);
}

// Map APIs
Result<DevTuple>
ZstoreController::GetDevTupleForRandomReads(ObjectKeyHash key_hash)
{
    return std::make_tuple(
        std::make_pair("Zstore2Dev2", Configuration::GetZoneId()),
        std::make_pair("Zstore4Dev1", Configuration::GetZoneId()),
        std::make_pair("Zstore4Dev2", Configuration::GetZoneId()));
}

Result<DevTuple> ZstoreController::GetDevTuple(ObjectKeyHash key_hash)
{
    std::srand(key_hash); // seed the random number generator
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

Result<MapEntry> ZstoreController::CreateFakeObject(ObjectKeyHash key_hash,
                                                    DevTuple tuple)
{
    auto entry = createMapEntry(tuple, 0, 1, 0, 1, 0, 1);
    assert(entry.has_value());
    return entry.value();
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
}

Result<void> ZstoreController::ReadAllMap()
{
    log_debug("read dump map ");
    std::string filename = "map_data.bin";
    // Read the map back from the file
    // readMapFromFile(filename);
}

// this yields a list of devices for a given object key
//
// right now we create 120 (6*5*4) tuples, to consider ordering of every tuple
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
    // log_info("unreg controllers");
    unregister_controllers(mDevices);
    // log_info("cleanup ");
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

int ZstoreController::Init(bool object, int key_experiment, int phase)
{
    int rc = 0;
    verbose = Configuration::Verbose();

    // setQueuDepth(Configuration::GetQueueDepth());
    setContextPoolSize(Configuration::GetContextPoolSize());
    setNumOfDevices(Configuration::GetNumOfDevices() *
                    Configuration::GetNumOfTargets());
    setKeyExperiment(key_experiment);
    setPhase(phase);

    // TODO: set all parameters too
    log_debug(
        "Configuration: sector size {}, context pool size "
        "{}, targets {}, devices {}",
        Configuration::GetBlockSize(), Configuration::GetContextPoolSize(),
        Configuration::GetNumOfTargets(), Configuration::GetNumOfDevices());

    std::vector<std::tuple<std::string, std::string, std::string, u32, u32>>
        ip_port_devs{
            std::make_tuple("nqn.2024-04.io.zstore2:cnode1", "12.12.12.2",
                            "5520", Configuration::GetZoneId(),
                            Configuration::GetZoneId()),
            std::make_tuple("nqn.2024-04.io.zstore3:cnode1", "12.12.12.3",
                            "5520", Configuration::GetZoneId(),
                            Configuration::GetZoneId()),
            std::make_tuple("nqn.2024-04.io.zstore4:cnode1", "12.12.12.4",
                            "5520", Configuration::GetZoneId(),
                            Configuration::GetZoneId())};

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

    auto const address = asio::ip::make_address("12.12.12.1");
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

    log_info("ZstoreController Init finish");

    return rc;
}

// void ZstoreController::writeMapToFile(const std::string &filename)
// {
//     try {
//         std::ofstream file_out(filename, std::ios::out | std::ios::trunc);
//         if (!file_out.is_open()) {
//             throw std::runtime_error("Could not open file for writing");
//         }
//         boost::archive::text_oarchive archive(file_out);
//         archive << mMap; // Serialize the map to the file
//         file_out.close();
//         std::cout << "Map successfully written to file: " << filename
//                   << std::endl;
//     } catch (const std::exception &e) {
//         std::cerr << "Error writing map to file: " << e.what() << std::endl;
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
        log_info("Populate Map({},{}): random read", mKeyExperiment, mPhase);
        // Random Read
        auto zone_base =
            Configuration::GetZoneId() * Configuration::GetZoneDist();
        for (int i = 0; i < _map_size; i++) {
            auto zone_offset = i % 10 * Configuration::GetZoneDist();
            std::string device;
            if (i % 6 == 0) {
                device = "Zstore2Dev1";
            } else if (i % 6 == 1) {
                device = "Zstore2Dev2";
            } else if (i % 6 == 2) {
                device = "Zstore3Dev1";
            } else if (i % 6 == 3) {
                device = "Zstore3Dev2";
            } else if (i % 6 == 4) {
                device = "Zstore4Dev1";
            } else if (i % 6 == 5) {
                device = "Zstore4Dev2";
            }
            auto entry =
                createMapEntry(
                    std::make_tuple(
                        std::make_pair(device, Configuration::GetZoneId()),
                        std::make_pair("Zstore2Dev2",
                                       Configuration::GetZoneId()),
                        std::make_pair("Zstore3Dev1",
                                       Configuration::GetZoneId())),
                    i + zone_base + zone_offset,
                    Configuration::GetObjectSizeInBytes() /
                        Configuration::GetBlockSize(),
                    i + zone_base + zone_offset,
                    Configuration::GetObjectSizeInBytes() /
                        Configuration::GetBlockSize(),
                    i + zone_base + zone_offset,
                    Configuration::GetObjectSizeInBytes() /
                        Configuration::GetBlockSize())
                    .value();
            std::string hash_hex = sha256(std::to_string(i));
            // log_debug("Populate Map: index {}, key {}", i,
            //           "/db/" + std::to_string(i));
            unsigned long long hash =
                std::stoull(hash_hex.substr(0, 16), nullptr, 16);
            mMap.emplace(hash, entry);
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
