#include "include/configuration.h"
#include "include/global.h"
#include "include/zstore_controller.h"
#include "spdk/env.h"
#include "spdk/thread.h"
#include "src/include/utils.h"
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
    int phase = std::stoull(argv[2]);

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
    // opts.core_mask = "0x7"; // b, 7, f
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
    rc = gZstoreController->Init(false, key_experiment, phase);
    log_info("Init ZstoreController: {}", rc);

    // TODO why is here never reached?
    log_info("Starting HTTP server with port 2000!\n");

    if (gZstoreController->mKeyExperiment == 1) {
        // log_info("1111");
        // gZstoreController->mIoc_.run();
        // while (1)
        //     sleep(1);
    } else if (gZstoreController->mKeyExperiment == 6) {
        // sleep(10);
        auto rc = gZstoreController->Checkpoint();
        assert(rc.has_value());
    } else {
        sleep(30);
        if (gZstoreController->mKeyExperiment == 2 &&
            gZstoreController->mOption == 1) {
            log_info("Prepare phase is done, dumping all map and bloom filter");
            auto rc = gZstoreController->DumpAllMap();
            assert(rc.has_value());
        }
    }

    gZstoreController->Drain();
    spdk_env_thread_wait_all();
    gZstoreController->zstore_cleanup();

    return rc;
}
