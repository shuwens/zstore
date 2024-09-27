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

void create_dummy_objects(Zstore zstore)
{
    log_info("Create dummy objects in table: foo, bar, test");

    zstore.putObject("foo", "foo_data");
    zstore.putObject("bar", "bar_data");
    zstore.putObject("baz", "baz_data");
}

int main(int argc, char **argv)
{
    int rc;

    struct spdk_env_opts opts;

    // rc = parse_args(argc, argv);
    // if (rc != 0) {
    //     return rc;
    // }

    // spdk_nvme_trid_populate_transport(&g_trid, SPDK_NVME_TRANSPORT_PCIE);
    spdk_nvme_trid_populate_transport(&g_trid, SPDK_NVME_TRANSPORT_TCP);
    snprintf(g_trid.subnqn, sizeof(g_trid.subnqn), "%s",
             SPDK_NVMF_DISCOVERY_NQN);

    // g_dpdk_mem = spdk_strtol(optarg, 10);
    if (g_dpdk_mem < 0) {
        log_error("Invalid DPDK memory size");
        return g_dpdk_mem;
    }
    // if (spdk_nvme_transport_id_parse(&g_trid, optarg) != 0) {
    //     log_error("Error parsing transport address");
    //     return 1;
    // }
    g_dpdk_mem_single_seg = true;

    spdk_env_opts_init(&opts);
    opts.name = "arb";
    opts.mem_size = g_dpdk_mem;
    opts.hugepage_single_segments = g_dpdk_mem_single_seg;
    // opts.core_mask = g_arbitration.core_mask;
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

    sleep(10);
    gZstoreController->Drain();

    spdk_env_thread_wait_all();

    // log_debug("XXXX");
    gZstoreController->zstore_cleanup();

    log_debug("XXXX");
    return rc;
}
