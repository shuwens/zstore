// #include "../../common.cc"
#include "../../include/common.h"
#include "../../include/configuration.h"
#include "../../include/global.h"
#include "../../include/types.h"
#include "../../include/zstore_controller.h"
// #include "../../zstore_controller.cc"
#include <boost/asio.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <fstream>
#include <iostream>
#include <string>
#include <tuple>

using namespace std;

ZstoreController *gZstore;

void setup_test(int key_experiment, int phase, std::string message)
{
    struct spdk_env_opts opts;
    spdk_nvme_trid_populate_transport(&g_trid, SPDK_NVME_TRANSPORT_TCP);
    snprintf(g_trid.subnqn, sizeof(g_trid.subnqn), "%s",
             SPDK_NVMF_DISCOVERY_NQN);
    assert(g_dpdk_mem >= 0);
    g_dpdk_mem_single_seg = true;

    spdk_env_opts_init(&opts);
    opts.name = message.c_str();
    opts.mem_size = g_dpdk_mem;
    opts.hugepage_single_segments = g_dpdk_mem_single_seg;
    opts.core_mask = "0xfff";
    opts.shm_id = -1;
    assert(spdk_env_init(&opts) == 0);

    int rc = spdk_thread_lib_init(nullptr, 0);
    assert(rc == 0);
    asio::io_context ioc{Configuration::GetNumHttpThreads()};
    gZstore = new ZstoreController(std::ref(ioc));
    rc = gZstore->Init(false, key_experiment, phase);
    log_info("ZstoreController init: %d", rc);
}
