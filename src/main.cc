#include "include/http_server.h"
#include "include/zstore_controller.h"
#include "spdk/env.h"
#include "src/include/global.h"
#include "zstore_controller.cc"
#include <algorithm>
#include <bits/stdc++.h>
#include <boost/asio/executor_work_guard.hpp>
#include <cassert>
#include <cstdlib>
#include <fmt/core.h>
#include <sys/time.h>
#include <unistd.h>

int main(int argc, char **argv)
{
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
    opts.core_mask = "0xfff";
    opts.shm_id = -1;
    if (spdk_env_init(&opts) < 0) {
        return 1;
    }

    rc = spdk_thread_lib_init(nullptr, 0);
    if (rc < 0) {
        log_error("Unable to initialize SPDK thread lib.");
        return 1;
    }

    net::io_context ioc{threads};
    gZstoreController = new ZstoreController(std::ref(ioc));
    gZstoreController->Init(false);

    if (!Configuration::UseDummyWorkload()) {
        log_info("Starting HTTP server with port 2000!\n");

        // const int threads = 4;
        // net::io_context ioc{threads};

        // Create and launch a listening port
        // std::make_shared<listener>(ioc, tcp::endpoint{address, port},
        // doc_root)
        //     ->run();

        // Run the I/O service on the requested number of threads
        // gZstoreController->v.reserve(threads - 1);

        // for (auto i = threads - 1; i > 0; --i)
        //     gZstoreController->v.emplace_back([&ioc] { ioc.run(); });
        // ioc.run();
        // boost::asio::executor_work_guard<boost::asio::io_context::executor_type>
        //     work{gZstoreController->mIoc.get_executor()};

        // boost::asio::executor_work_guard<
        //     boost::asio::io_context::executor_type> =
        //     boost::asio::make_work_guard(
        //         gZstoreController->mIoc.get_executor());

        // for (auto i = threads - 1; i > 0; --i) {
        //     log_debug("i is {}", i);
        //     gZstoreController->mHttpThreads.emplace_back(
        //         []() { gZstoreController->mIoc.run(); });
        // }
        // std::ranges::generate(gZstoreController->mHttpThreads, [&]() {
        //     return std::thread([&]() { gZstoreController->mIoc.run(); });
        // });
        // gZstoreController->mIoc.run();

        // ranges::generate(m_extra_threads, [&]() {
        //     return std::thread([&]() { context().run(); });
        // });

        // context().run();

        // return EXIT_SUCCESS;
        while (1) {
            // while (ctrl->mIoc_.poll()) {
            auto etime = std::chrono::high_resolution_clock::now();
            auto delta = std::chrono::duration_cast<std::chrono::microseconds>(
                             etime - gZstoreController->stime)
                             .count();
            auto tput = gZstoreController->GetDevice()->mTotalCounts *
                        g_micro_to_second / delta;

            // if (g->verbose)
            log_info("Total IO {}, total time {}ms, throughput {} IOPS",
                     gZstoreController->GetDevice()->mTotalCounts, delta, tput);

            sleep(1);
        }
    } else {
        sleep(10);
    }

    gZstoreController->Drain();

    spdk_env_thread_wait_all();

    gZstoreController->zstore_cleanup();

    return rc;
}
