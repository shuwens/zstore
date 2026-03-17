#include "../common.cc"
#include "../device.cc"
#include "../include/common.h"
#include "../include/configuration.h"
#include "../include/device.h"
#include "../include/global.h"
#include "../include/object.h"
#include "../include/tinyxml2.h"
#include "../include/utils.h"
#include "../include/zstore_controller.h"
#include "../zstore_controller.cc"
#include "spdk/env.h"
#include "spdk/thread.h"
#include <bits/stdc++.h>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/write.hpp>
#include <cassert>
#include <coroutine>
#include <cstdio>
#include <cstdlib>
#include <fmt/core.h>
#include <sys/time.h>
#include <unistd.h>

using namespace std::literals;
using boost::asio::awaitable;
using boost::asio::co_spawn;
using boost::asio::detached;
using boost::asio::use_awaitable;
namespace this_coro = boost::asio::this_coro;

// https://github.com/boostorg/asio/blob/develop/example/cpp17/coroutines_ts/echo_server.cpp

ZstoreController *gZstoreController;

asio::awaitable<void> async_finish()
{

    auto dev = gZstoreController->GetDevice("Zstore2Dev1");
    auto slot = MakeManagementRequest(gZstoreController, dev).value();
    //         // MakeReadRequest(&zctrl_, dev1, lba, req).value();
    //         // auto res = co_await zoneRead(s1);

    auto rc = co_await zoneFinish(slot);
    // trace() << "cpp20 style " << quoted(msg) << std::endl;
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        log_info("Usage: ./finish_zones <start> <end>");
        return 1;
    }

    int start = std::stoull(argv[1]);
    int end = std::stoull(argv[2]);

    int rc;
    struct spdk_env_opts opts;
    spdk_nvme_trid_populate_transport(&g_trid, SPDK_NVME_TRANSPORT_TCP);
    snprintf(g_trid.subnqn, sizeof(g_trid.subnqn), "%s",
             SPDK_NVMF_DISCOVERY_NQN);
    if (g_dpdk_mem < 0) {
        log_error("Invalid DPDK memory size");
        return g_dpdk_mem;
    }
    g_dpdk_mem_single_seg = true;

    spdk_env_opts_init(&opts);
    opts.name = "Zstore";
    opts.mem_size = g_dpdk_mem;
    opts.hugepage_single_segments = g_dpdk_mem_single_seg;
    opts.core_mask = "0x7";
    opts.shm_id = -1;
    if (spdk_env_init(&opts) < 0) {
        return 1;
    }

    rc = spdk_thread_lib_init(nullptr, 0);
    if (rc < 0) {
        log_error("Unable to initialize SPDK thread lib.");
        return 1;
    }

    asio::io_context ioc{Configuration::GetNumHttpThreads()};
    gZstoreController = new ZstoreController(std::ref(ioc));
    gZstoreController->Init(false, 2, 1);

    log_info("Starting HTTP server with port 2000!");

    co_spawn(gZstoreController->mIoc_, async_finish, asio::detached);
    gZstoreController->mIoc_.run_for(1500ms);

    // try {
    //     boost::asio::signal_set signals(gZstoreController->mIoc_, SIGINT,
    //                                     SIGTERM);
    //     signals.async_wait(
    //         [&](auto, auto) { gZstoreController->mIoc_.stop(); });
    //
    //     log_info("Finishing zone {} to {}!\n", start, end);
    //     for (int i = start; i < end; i++) {
    //         co_spawn(, zoneFinish(slot), detached);
    //     }
    //     gZstoreController->mIoc_.run();
    // } catch (std::exception &e) {
    //     std::printf("Exception: %s\n", e.what());
    // }

    // co_await async_sleep(co_await asio::this_coro::executor,
    //                      std::chrono::microseconds(0),
    //                      asio::use_awaitable);

    // if (res.has_value()) {
    // yields 320 to 310k IOPS ZstoreObject deserialized_obj;
    // bool success = ReadBufferToZstoreObject(s1->dataBuffer, s1->size,
    //                                         deserialized_obj);
    // req.body() = s1->response_body; // not expensive
    //     s1->Clear();
    //     zctrl_.mRequestContextPool->ReturnRequestContext(s1);
    //     co_return handle_request(std::move(req));
    // } else {
    // yields 378k IOPS
    // s1->Clear();
    // zctrl_.mRequestContextPool->ReturnRequestContext(s1);
    // co_return handle_request(std::move(req));

    // if (gZstoreController->mKeyExperiment == 1) {
    //     log_info("1111");
    //     gZstoreController->mIoc_.run();
    //     // while (1)
    //     //     sleep(1);
    // } else {
    //     sleep(30);
    //     if (gZstoreController->mKeyExperiment == 2 &&
    //         gZstoreController->mPhase == 1) {
    //         log_info("Prepare phase is done, dumping all map and bloom
    //         filter"); gZstoreController->DumpAllMap();
    //     }
    // }

    // gZstoreController->Drain();
    spdk_env_thread_wait_all();
    gZstoreController->zstore_cleanup();

    return rc;
}
