#include "include/zns_utils.h"
#include "include/zstore_controller.h"
#include "spdk/env.h"
#include "src/include/global.h"
#include "zstore_controller.cc"
#include <bits/stdc++.h>
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <fmt/core.h>
#include <sys/time.h>
#include <unistd.h>

CivetServer startWebServer()
{
    log_info("Starting HTTP server with port 2000!\n");
    mg_init_library(0);

    const char *options[] = {// "listening_ports", port_str,
                             "listening_ports", "2000", "tcp_nodelay", "1",
                             "num_threads", "1000", "enable_keep_alive", "yes",
                             //"max_request_size", "65536",
                             0};

    std::vector<std::string> cpp_options;
    for (int i = 0; i < (sizeof(options) / sizeof(options[0]) - 1); i++) {
        cpp_options.push_back(options[i]);
    }

    CivetServer server(cpp_options); // <-- C++ style start
    ZstoreHandler h;
    server.addHandler("", h);

    while (!exitNow) {
        sleep(1);
    }

    return server;
}

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
        std::string zstore_name, bucket_name;
        zstore_name = "test";
        Zstore zstore(zstore_name);

        zstore.SetVerbosity(1);

        // create a bucket: this process is now manual, not via create/get
        // bucket
        // zstore.buckets.push_back(AWS_S3_Bucket(bucket_name, "db"));

        // create_dummy_objects();
        // Start the web server controllers.
        CivetServer web_server = startWebServer();
        // struct mg_context* web_server = startWebServer();

        while (1) {
            sleep(1);
        }

        // Stop the web server.
        // mg_stop(web_server);
        mg_exit_library();

    } else {
        sleep(10);
    }

    gZstoreController->Drain();

    spdk_env_thread_wait_all();

    // log_debug("XXXX");
    gZstoreController->zstore_cleanup();

    return rc;
}
