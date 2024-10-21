#include "include/configuration.h"
#include "include/global.h"
#include "include/zstore_controller.h"
#include "spdk/env.h"
#include "spdk/thread.h"
#include <bits/stdc++.h>
#include <boost/asio/executor_work_guard.hpp>
#include <cassert>
#include <cstdlib>
#include <fmt/core.h>
#include <sys/time.h>
#include <unistd.h>

ZstoreController *gZstoreController;

int main(int argc, char **argv)
{
    if (argc < 3) {
        log_info("Usage: ./zstore <key_experiment> <dummy>");
        return 1;
    }

    int key_experiment = std::stoull(argv[1]);
    int dummy = std::stoull(argv[2]);

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
    opts.core_mask = "0xf";
    opts.shm_id = -1;
    if (spdk_env_init(&opts) < 0) {
        return 1;
    }

    rc = spdk_thread_lib_init(nullptr, 0);
    if (rc < 0) {
        log_error("Unable to initialize SPDK thread lib.");
        return 1;
    }

    net::io_context ioc{Configuration::GetNumHttpThreads()};
    gZstoreController = new ZstoreController(std::ref(ioc));
    gZstoreController->Init(false, key_experiment);

    log_debug("XXXX");

    // gZstoreController->mIoc_.run();

    if (!Configuration::UseDummyWorkload()) {
        log_info("Starting HTTP server with port 2000!\n");

        // FIXME only tput on Zstore2Dev1
        while (1) {
            //     auto etime = std::chrono::high_resolution_clock::now();
            //     auto delta =
            //     std::chrono::duration_cast<std::chrono::microseconds>(
            //                      etime - gZstoreController->stime)
            //                      .count();
            //     auto tput =
            //         gZstoreController->mTotalCounts * g_micro_to_second /
            //         delta;
            //
            //     log_info("Total IO {}, total time {}ms, throughput {} IOPS",
            //              gZstoreController->mTotalCounts, delta, tput);
            //     {
            //         gZstoreController->stime =
            //             std::chrono::high_resolution_clock::now();
            //         gZstoreController->mTotalCounts = 0;
            //     }
            sleep(30);
        }
    } else {
        sleep(10);
    }

    gZstoreController->Drain();
    spdk_env_thread_wait_all();
    gZstoreController->zstore_cleanup();

    return rc;
}
