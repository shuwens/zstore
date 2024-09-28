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
    opts.core_mask = "0xff";
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

        try {
            // Check command line arguments.
            if (argc != 3) {
                std::cerr << "Usage: " << argv[0] << " <address> <port>\n";
                std::cerr << "  For IPv4, try:\n";
                std::cerr << "    receiver 0.0.0.0 80\n";
                std::cerr << "  For IPv6, try:\n";
                std::cerr << "    receiver 0::0 80\n";
                return EXIT_FAILURE;
            }

            auto const address = net::ip::make_address(argv[1]);
            unsigned short port =
                static_cast<unsigned short>(std::atoi(argv[2]));

            net::io_context ioc{1};

            tcp::acceptor acceptor{ioc, {address, port}};
            tcp::socket socket{ioc};
            http_server(acceptor, socket);

            ioc.run();
        } catch (std::exception const &e) {
            std::cerr << "Error: " << e.what() << std::endl;
            return EXIT_FAILURE;
        }

    } else {
        sleep(10);
    }

    gZstoreController->Drain();

    spdk_env_thread_wait_all();

    // log_debug("XXXX");
    gZstoreController->zstore_cleanup();

    return rc;
}
