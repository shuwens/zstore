#include "include/zstore_controller.h"
#include "include/common.h"
#include "include/configuration.h"
#include "include/device.h"
#include "include/global.h"
#include "include/http_server.h"
#include "include/object.h"
#include <fstream>
#include <iostream>
#include <spdk/nvme_zns.h>
#include <spdk/string.h>
#include <string>
#include <thread>

int zone_offset = 1808277;
// int zone_offset = 0;

// namespace beast = boost::beast; // from <boost/beast.hpp>
// namespace http = beast::http;     // from <boost/beast/http.hpp>
namespace net = boost::asio;      // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp; // from <boost/asio/ip/tcp.hpp>

void ZstoreController::initHttpThread()
{
    struct spdk_cpuset cpumask;
    for (int threadId = 0; threadId < Configuration::GetNumHttpThreads();
         ++threadId) {
        spdk_cpuset_zero(&cpumask);
        spdk_cpuset_set_cpu(&cpumask,
                            Configuration::GetHttpThreadCoreId(threadId), true);
        mHttpThread[threadId].thread =
            spdk_thread_create("HttpThread", &cpumask);
        assert(mHttpThread[threadId].thread != nullptr);
        mHttpThread[threadId].controller = this;
        int rc = spdk_env_thread_launch_pinned(
            Configuration::GetHttpThreadCoreId(threadId), httpWorker,
            &mHttpThread[threadId]);
        log_info("Http thread name {} id {} on core {}",
                 spdk_thread_get_name(mHttpThread[threadId].thread),
                 spdk_thread_get_id(mHttpThread[threadId].thread),
                 Configuration::GetHttpThreadCoreId(threadId));
        if (rc < 0) {
            log_error("Failed to launch Http thread error: {} {}", strerror(rc),
                      spdk_strerror(rc));
        }
    }
}

void ZstoreController::initDispatchThread()
{
    struct spdk_cpuset cpumask;
    spdk_cpuset_zero(&cpumask);
    spdk_cpuset_set_cpu(&cpumask, Configuration::GetDispatchThreadCoreId(),
                        true);
    mDispatchThread = spdk_thread_create("DispatchThread", &cpumask);
    log_info("Create {} (id {}) on Core {}",
             spdk_thread_get_name(mDispatchThread),
             spdk_thread_get_id(mDispatchThread),
             Configuration::GetDispatchThreadCoreId());

    int rc;
    if (Configuration::UseObject()) {
        log_info("Dispatch object worker");
        rc = spdk_env_thread_launch_pinned(
            Configuration::GetDispatchThreadCoreId(), dispatchObjectWorker,
            this);
    } else {
        log_info("Not using object");
        rc = spdk_env_thread_launch_pinned(
            Configuration::GetDispatchThreadCoreId(), dispatchWorker, this);
    }
    if (rc < 0) {
        log_error("Failed to launch dispatch thread error: {} {}", strerror(rc),
                  spdk_strerror(rc));
    }
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

    // device 1
    Device *device1 = new Device();
    ns = spdk_nvme_ctrlr_get_ns(ctrlr, 1);
    if (spdk_nvme_ns_get_csi(ns) != SPDK_NVME_CSI_ZNS) {
        log_info("ns {} is not zns ns", 1);
    }
    device1->Init(ctrlr, 1, zone_id1);
    device1->SetDeviceTransportAddress(traddr);
    g_devices.emplace_back(device1);

    Device *device2 = new Device();
    ns = spdk_nvme_ctrlr_get_ns(ctrlr, 2);
    if (spdk_nvme_ns_get_csi(ns) != SPDK_NVME_CSI_ZNS) {
        log_info("ns {} is not zns ns", 2);
    }
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
    const std::tuple<std::string, std::string, u32, u32> &dev_tuple)
{
    char *g_hostnqn = "nqn.2024-04.io.zstore:cnode1";
    // 1. connect nvmf device
    struct spdk_nvme_transport_id trid = {};

    auto [ip, port, zone_id1, zone_id2] = dev_tuple;
    snprintf(trid.traddr, sizeof(trid.traddr), "%s", ip.c_str());
    snprintf(trid.trsvcid, sizeof(trid.trsvcid), "%s", port.c_str());
    snprintf(trid.subnqn, sizeof(trid.subnqn), "%s", g_hostnqn);
    trid.adrfam = SPDK_NVMF_ADRFAM_IPV4;
    trid.trtype = SPDK_NVME_TRANSPORT_TCP;

    struct spdk_nvme_ctrlr_opts opts;
    spdk_nvme_ctrlr_get_default_ctrlr_opts(&opts, sizeof(opts));
    snprintf(opts.hostnqn, sizeof(opts.hostnqn), "%s", g_hostnqn);
    // NOTE: disable keep alive timeout
    opts.keep_alive_timeout_ms = 0;
    register_ctrlr(g_devices, spdk_nvme_connect(&trid, &opts, sizeof(opts)),
                   trid.traddr, zone_id1, zone_id2);
}

int ZstoreController::register_controllers(
    std::vector<Device *> &g_devices,
    const std::tuple<std::string, std::string, u32, u32> &dev_tuple)
{
    // log_info("Initializing NVMe Controllers");

    // RDMA
    // zns_dev_init(ctx, "192.168.100.9", "5520");
    // TCP
    zns_dev_init(g_devices, dev_tuple);

    return 0;
}

void ZstoreController::unregister_controllers(std::vector<Device *> &g_devices)
{
    // struct spdk_nvme_detach_ctx *detach_ctx = NULL;
}

int ZstoreController::PopulateMap()
{
    log_info("populate map: mKeyExperiment {}, mPhase {}", mKeyExperiment,
             mPhase);
    unsigned char hash[SHA256_DIGEST_LENGTH];
    if (mKeyExperiment == 1) {
        // Random Read
        for (int i = 0; i < 2'000'000; i++) {
            auto entry =
                createMapEntry(std::make_tuple(
                                   std::make_pair("Zstore2Dev1",
                                                  Configuration::GetZoneId1()),
                                   std::make_pair("Zstore2Dev2",
                                                  Configuration::GetZoneId1()),
                                   std::make_pair("Zstore3Dev1",
                                                  Configuration::GetZoneId2())),
                               i + zone_offset, 1, i + zone_offset, 1,
                               i + zone_offset, 1)
                    .value();
            sha256("/db/" + std::to_string(i), hash);
            mMap.insert({hash, entry});
        }
    } else if (mKeyExperiment == 2) {
        if (mPhase == 1) {
            log_info("Prepare phase, do nothing");
            for (int i = 0; i < 2'000; i++) {
                auto entry =
                    createMapEntry(
                        std::make_tuple(
                            std::make_pair("Zstore2Dev1",
                                           Configuration::GetZoneId1()),
                            std::make_pair("Zstore2Dev2",
                                           Configuration::GetZoneId1()),
                            std::make_pair("Zstore3Dev1",
                                           Configuration::GetZoneId2())),
                        i + zone_offset, 1, i + zone_offset, 1, i + zone_offset,
                        1)
                        .value();
                sha256("/db/" + std::to_string(i), hash);
                mMap.insert({hash, entry});
            }
            DumpAllMap();
        } else if (mPhase == 2) {
            log_info("Run phase, load the map and the bloom filter");
            ReadAllMap();
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

// Function to write the map to a file
void ZstoreController::writeMapToFile(const std::string &filename)
{
    log_debug("11111");
    std::ofstream outFile(filename, std::ios::binary);
    if (!outFile) {
        std::cerr << "Error opening file for writing: " << filename
                  << std::endl;
    }

    log_debug("11111");
    size_t mapSize = 0;
    mMap.visit_all([&mapSize](auto &x) { ++mapSize; });

    // Write the map size
    outFile.write(reinterpret_cast<const char *>(&mapSize), sizeof(mapSize));

    log_debug("11111");
    // Iterate over each key-value pair and write them to the file
    mMap.visit_all([&outFile](auto &x) {
        // Serialize the key
        log_debug("2222");
        outFile.write(reinterpret_cast<const char *>(x.first),
                      33); // Assuming 32-byte hash

        log_debug("2222");
        // Serialize the entry
        auto writeTargetLbaTuple = [&outFile](const TargetLbaTuple &tuple) {
            writeString(outFile, std::get<0>(tuple)); // TargetDev
            outFile.write(reinterpret_cast<const char *>(&std::get<1>(tuple)),
                          sizeof(Lba)); // Lba
            outFile.write(reinterpret_cast<const char *>(&std::get<2>(tuple)),
                          sizeof(Length)); // Length
        };
        log_debug("2222");
        writeTargetLbaTuple(std::get<0>(x.second));
        writeTargetLbaTuple(std::get<1>(x.second));
        writeTargetLbaTuple(std::get<2>(x.second));
        log_debug("2222");
    });
}

// Function to read the map from a file
void ZstoreController::readMapFromFile(const std::string &filename)
{
    std::ifstream inFile(filename, std::ios::binary);
    if (!inFile) {
        std::cerr << "Error opening file for reading: " << filename
                  << std::endl;
    }

    size_t mapSize;
    inFile.read(reinterpret_cast<char *>(&mapSize), sizeof(mapSize));

    for (size_t i = 0; i < mapSize; ++i) {
        // Deserialize the key
        unsigned char *key = new unsigned char[32];
        inFile.read(reinterpret_cast<char *>(key), 32);

        // Deserialize the entry
        auto readTargetLbaTuple = [&inFile]() -> TargetLbaTuple {
            std::string dev = readString(inFile);
            Lba lba;
            Length length;
            inFile.read(reinterpret_cast<char *>(&lba), sizeof(Lba));
            inFile.read(reinterpret_cast<char *>(&length), sizeof(Length));
            return std::make_tuple(dev, lba, length);
        };

        MapEntry entry = std::make_tuple(
            readTargetLbaTuple(), readTargetLbaTuple(), readTargetLbaTuple());
        mMap.emplace(key, entry);
    }
}

Result<void> ZstoreController::DumpAllMap()
{
    log_debug("dump map ");
    // Write the map to a file
    std::string filename = "map_data.bin";
    writeMapToFile(filename);
}

Result<void> ZstoreController::ReadAllMap()
{
    log_debug("read dump map ");
    std::string filename = "map_data.bin";
    // Read the map back from the file
    readMapFromFile(filename);
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
        {std::make_pair("Zstore2Dev1", Configuration::GetZoneId1())});
    tgt_dev_vec.push_back(
        {std::make_pair("Zstore2Dev2", Configuration::GetZoneId1())});
    tgt_dev_vec.push_back(
        {std::make_pair("Zstore3Dev1", Configuration::GetZoneId2())});
    tgt_dev_vec.push_back(
        {std::make_pair("Zstore3Dev2", Configuration::GetZoneId1())});
    tgt_dev_vec.push_back(
        {std::make_pair("Zstore4Dev1", Configuration::GetZoneId1())});
    tgt_dev_vec.push_back(
        {std::make_pair("Zstore4Dev2", Configuration::GetZoneId1())});

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

int ZstoreController::Init(bool object, int key_experiment, int phase)
{
    int rc = 0;
    verbose = Configuration::Verbose();

    setQueuDepth(Configuration::GetQueueDepth());
    setContextPoolSize(Configuration::GetContextPoolSize());
    setNumOfDevices(Configuration::GetNumOfDevices() *
                    Configuration::GetNumOfTargets());
    setKeyExperiment(key_experiment);
    setPhase(phase);

    // TODO: set all parameters too
    log_debug("Configuration: sector size {}, queue size {}, context pool size "
              "{}, targets {}, devices {}",
              Configuration::GetBlockSize(), Configuration::GetQueueDepth(),
              Configuration::GetContextPoolSize(),
              Configuration::GetNumOfTargets(),
              Configuration::GetNumOfDevices());

    std::vector<std::tuple<std::string, std::string, u32, u32>> ip_port_devs{
        std::make_tuple("12.12.12.2", "5520", Configuration::GetZoneId1(),
                        Configuration::GetZoneId1()),
        std::make_tuple("12.12.12.3", "5520", Configuration::GetZoneId2(),
                        Configuration::GetZoneId1()),
        std::make_tuple("12.12.12.4", "5520", Configuration::GetZoneId1(),
                        Configuration::GetZoneId1())};
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
    initDispatchThread();

    auto const address = net::ip::make_address("127.0.0.1");
    auto const port = 2000;

    // The io_context is required for all I/O
    auto const num_threads = 3;
    net::io_context ioc{num_threads};

    // Spawn a listening port
    boost::asio::co_spawn(ioc, do_listen(tcp::endpoint{address, port}, *this),
                          [](std::exception_ptr e) {
                              if (e)
                                  try {
                                      std::rethrow_exception(e);
                                  } catch (std::exception &e) {
                                      std::cerr
                                          << "Error in acceptor: " << e.what()
                                          << "\n";
                                  }
                          });

    std::vector<std::jthread> threads(num_threads);
    for (unsigned i = 0; i < num_threads; ++i) {
        threads[i] = std::jthread([&ioc, i, &threads] {
            // Create a cpu_set_t object representing a set of CPUs.
            // Clear it and mark only CPU i as set.
            cpu_set_t cpuset;
            CPU_ZERO(&cpuset);
            CPU_SET(i % 3 + Configuration::GetHttpThreadCoreId(), &cpuset);
            std::string name =
                "zstore_ioc" +
                std::to_string(i + Configuration::GetHttpThreadCoreId());
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
                     i % 3 + Configuration::GetHttpThreadCoreId());
            ioc.run();
        });
    }

    // This was using SPDK threads as HTTP threads
    // for (int threadId = 0; threadId < Configuration::GetNumHttpThreads();
    //      ++threadId) {
    //     mHttpThread[threadId].group = spdk_nvme_poll_group_create(NULL,
    //     NULL); mHttpThread[threadId].controller = this;
    // }
    // initHttpThread();

    log_info("Initialization complete. Launching workers.");

    // auto ret = PopulateDevHash(key_experiment);
    // assert(ret.has_value());
    rc = PopulateDevHash();
    assert(rc == 0);

    rc = PopulateMap();
    assert(rc == 0);

    pivot = 0;

    // Read zone headers

    // Valid (full and open) zones and their headers
    std::map<uint64_t, uint8_t *> zonesAndHeaders[mN];
    for (int i = 0; i < mN; ++i) {
        log_debug("read zone and headers {}.", i);
        // if (i == failedDriveId) {
        //     continue;
        // }

        // TODO: right now we sort of just pick read and write zone,
        // this should be done smartly
        // FIXME bug
        // mDevices[i]->ReadZoneHeaders(zonesAndHeaders[i]);
        // mDevices[i]->GetZoneHeaders(zonesAndHeaders[i]);
    }

    log_info("ZstoreController Init finish");

    ioc.run();

    return rc;
}

// void ZstoreController::ReadInDispatchThread(RequestContext *ctx)
// {
//     thread_send_msg(GetIoThread(0), zoneRead, ctx);
// }

// void ZstoreController::WriteInDispatchThread(RequestContext *ctx)
// {
//     thread_send_msg(GetIoThread(0), zoneRead, ctx);
// }

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
    thread_send_msg(mDispatchThread, quit, nullptr);
    for (int i = 0; i < Configuration::GetNumHttpThreads(); ++i) {
        thread_send_msg(mHttpThread[i].thread, quit, nullptr);
    }
    log_debug("drain io: {}", spdk_get_ticks());
    log_debug("clean up ns worker");
    cleanup_ns_worker_ctx();
    //
    //     std::vector<uint64_t> deltas1;
    //     for (int i = 0; i < zctrlr->mWorker->ns_ctx->stimes.size();
    //     i++)
    //     {
    //         deltas1.push_back(
    //             std::chrono::duration_cast<std::chrono::microseconds>(
    //                 zctrlr->mWorker->ns_ctx->etimes[i] -
    //                 zctrlr->mWorker->ns_ctx->stimes[i])
    //                 .count());
    //     }
    //     auto sum1 = std::accumulate(deltas1.begin(), deltas1.end(),
    //     0.0); auto mean1 = sum1 / deltas1.size(); auto sq_sum1 =
    //     std::inner_product(deltas1.begin(), deltas1.end(),
    //                                       deltas1.begin(), 0.0);
    //     auto stdev1 = std::sqrt(sq_sum1 / deltas1.size() - mean1 *
    //     mean1); log_info("qd: {}, mean {}, std {}",
    //              zctrlr->mWorker->ns_ctx->io_completed, mean1,
    //              stdev1);
    //
    //     // clearnup
    //     deltas1.clear();
    //     zctrlr->mWorker->ns_ctx->etimes.clear();
    //     zctrlr->mWorker->ns_ctx->stimes.clear();
    //     // }
    //
    log_debug("end work fn");
    // print_stats(this);
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
    //
    // free(mWorker->ns_ctx);
    // free(mWorker);

    // if (spdk_mempool_count(mTaskPool) != (size_t)task_count) {
    //     log_error("mTaskPool count is {} but should be {}",
    //               spdk_mempool_count(mTaskPool), task_count);
    // }
    // spdk_mempool_free(mTaskPool);
}

// Result<void> ZstoreController::Read(u64 offset, Device *dev, HttpRequest
// req_,
//                                     std::function<void(HttpRequest)>
//                                     closure)
// {
// RequestContext *slot =
// mRequestContextPool->GetRequestContext(true); slot->ctrl = this;
// assert(slot->ctrl == this);
//
// auto ioCtx = slot->ioContext;
// ioCtx.ns = dev->GetNamespace();
// ioCtx.qpair = dev->GetIoQueue(0);
// ioCtx.data = slot->dataBuffer;
// ioCtx.offset = Configuration::GetZslba() + offset;
// ioCtx.size = Configuration::GetDataBufferSizeInSector();
// ioCtx.cb = complete;
// ioCtx.ctx = slot;
// ioCtx.flags = 0;
// slot->ioContext = ioCtx;
//
// slot->request = std::move(req_);
// // slot->read_fn = closure;
// assert(slot->ioContext.cb != nullptr);
// assert(slot->ctrl != nullptr);
// {
//     // std::unique_lock lock(mRequestQueueMutex);
//     EnqueueRead(slot);
// }
// }

// net::awaitable<void> ZstoreController::EnqueueRead(RequestContext *ctx)
// {
//     mReadQueue.push(ctx);
// }

// void ZstoreController::EnqueueWrite(RequestContext *ctx)
// {
//     mReadQueue.push(ctx);
//     // mWriteQueue.push(ctx);
// }

std::queue<RequestContext *> &ZstoreController::GetRequestQueue()
{
    return mRequestQueue;
}

std::shared_mutex &ZstoreController::GetRequestQueueMutex()
{
    return mRequestQueueMutex;
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
        thread_send_msg(mDispatchThread, tryDrainController, &args);
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

// Bloom filter
Result<bool> ZstoreController::SearchBF(const ObjectKeyHash &key_hash)
{
    return mBF.contains(key_hash);
}

Result<bool> ZstoreController::UpdateBF(const ObjectKeyHash &key_hash)
{
    mBF.insert({key_hash});
    return true;
}

// Map

Result<DevTuple>
ZstoreController::GetDevTupleForRandomReads(ObjectKeyHash key_hash)
{
    return std::make_tuple(
        std::make_pair("Zstore2Dev1", Configuration::GetZoneId1()),
        std::make_pair("Zstore2Dev2", Configuration::GetZoneId1()),
        std::make_pair("Zstore3Dev1", Configuration::GetZoneId2()));
    // ok, ok, zone full
    // return std::make_tuple("Zstore2Dev1", "Zstore2Dev2", "Zstore3Dev1");
    // invalid op code, invalid op code, ok
    // return std::make_tuple("Zstore3Dev2", "Zstore4Dev1", "Zstore4Dev2");

    // full, invalid op code, invalid op code
    // return std::make_tuple("Zstore3Dev1", "Zstore3Dev2", "Zstore4Dev1");

    // zstore 2: dev 1, dev 2 80, 80
    // zstore 3: dev 1, dev 2 115, 80
    // zstore 4: dev 1, dev 2 invlida, invalida
}

// Function to convert std::string to an unsigned int seed
unsigned int stringToSeed(const std::string &str)
{
    unsigned int seed = 0;
    for (char c : str) {
        seed += static_cast<unsigned int>(c);
    }
    return seed;
}

// Simple hash function to convert a char string into an unsigned int
unsigned int hashToSeed(unsigned char *str)
{
    unsigned int hash = 0;
    while (*str) {
        hash = (hash * 31) +
               *str; // Use a prime number to combine character values
        ++str;
    }
    return hash;
}

Result<DevTuple> ZstoreController::GetDevTuple(ObjectKeyHash key_hash)
{
    std::srand(hashToSeed(key_hash)); // seed the random number generator
                                      // with the hash of the object key
    int random_index = std::rand() % mDevHash.size();
    return mDevHash[random_index];
}

Result<bool> ZstoreController::PutObject(const ObjectKeyHash &key_hash,
                                         const MapEntry entry)
{
    return mMap.insert_or_assign(key_hash, entry);
}

Result<bool> ZstoreController::GetObject(const ObjectKeyHash &key_hash,
                                         MapEntry &entry)
{
    return mMap.cvisit(key_hash, [&entry](const auto &x) {
        // entry = x.second->value;
        entry = x.second;
    });
}

Result<MapEntry> ZstoreController::CreateFakeObject(ObjectKeyHash key_hash,
                                                    DevTuple tuple)
{
    auto entry = createMapEntry(tuple, 0, 1, 0, 1, 0, 1);
    assert(entry.has_value());
    return entry.value();
}

// Result<void> ZstoreController::UpdateMap(ObjectKey key, MapEntry entry)
// {
//     mMap.visit(key, [=](auto &x) { x.second = entry; });
// }
