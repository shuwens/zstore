#include "include/http_server.h"
#include "include/zstore_controller.h"
#include "spdk/env.h"
#include "src/include/global.h"
#include "zstore_controller.cc"
#include <bits/stdc++.h>
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
    // opts.core_mask = "0xf";
    opts.core_mask = "0x1";
    opts.shm_id = -1;
    if (spdk_env_init(&opts) < 0) {
        return 1;
    }

    rc = spdk_thread_lib_init(nullptr, 0);
    if (rc < 0) {
        log_error("Unable to initialize SPDK thread lib.");
        return 1;
    }

    gZstoreController = new ZstoreController();
    gZstoreController->Init(false);

    if (!Configuration::UseDummyWorkload()) {
        log_info("Starting HTTP server with port 2000!\n");

        auto const address = net::ip::make_address("127.0.0.1");
        auto const port = 2000;
        auto const doc_root = std::make_shared<std::string>(".");
        auto const threads = std::max<int>(1, 8);

        // The io_context is required for all I/O
        net::io_context ioc{threads};

        // Create and launch a listening port
        std::make_shared<listener>(ioc, tcp::endpoint{address, port}, doc_root)
            ->run();

        // Run the I/O service on the requested number of threads
        std::vector<std::thread> v;
        v.reserve(threads - 1);
        for (auto i = threads - 1; i > 0; --i)
            v.emplace_back([&ioc] { ioc.run(); });
        ioc.run();

        return EXIT_SUCCESS;

    } else {
        sleep(10);
    }

    gZstoreController->Drain();

    spdk_env_thread_wait_all();

    // log_debug("XXXX");
    gZstoreController->zstore_cleanup();

    return rc;
}
