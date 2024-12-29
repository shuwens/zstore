#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <fstream>
#include <iostream>
#include <memory>
#include <ostream>
#include <string>
#include <thread>

#include <ctime>

// #include "cxxopts.hpp"

#include <infiniband/verbs.h>

#include "../../src/endpoint.cpp"
#include "../../src/include/conn.hpp"
#include "../../src/include/endpoint.hpp"
#include "../../src/include/error.hpp"
#include "../../src/include/magic_buffer.hpp"
#include "../../src/include/rdma_common.h"
#include "../../src/magic_buffer.cpp"

kym::endpoint::Options opts = {
    .qp_attr =
        {
            .cap =
                {
                    .max_send_wr = 1,
                    .max_recv_wr = 1,
                    .max_send_sge = 1,
                    .max_recv_sge = 1,
                    .max_inline_data = 8,
                },
            .qp_type = IBV_QPT_RC,
        },
    .responder_resources = 5,
    .initiator_depth = 5,
    .retry_count = 8,
    .rnr_retry_count = 0,
    .native_qp = false,
    .inline_recv = 0,
};

// cxxopts::ParseResult parse(int argc, char *argv[])
// {
//     cxxopts::Options options(argv[0], "sendrecv");
//     try {
//         options.add_options()("client", "Whether to as client only",
//                               cxxopts::value<bool>())(
//             "server", "Whether to as server only",
//             cxxopts::value<bool>())("i,address", "IP address to connect to",
//                                     cxxopts::value<std::string>())(
//             "s,size", "Size of message to exchange",
//             cxxopts::value<int>()->default_value("60"));
//
//         auto result = options.parse(argc, argv);
//         if (!result.count("address")) {
//             std::cerr << "Specify an address" << std::endl;
//             std::cerr << options.help({""}) << std::endl;
//             exit(1);
//         }
//
//         return result;
//     } catch (const cxxopts::OptionException &e) {
//         std::cerr << "error parsing options: " << e.what() << std::endl;
//         std::cerr << options.help({""}) << std::endl;
//         exit(1);
//     }
// }

struct cinfo {
    uint32_t generic_key;
    uint64_t generic_addr;
    uint32_t magic_key;
    uint64_t magic_addr;
};

int main(int argc, char *argv[])
{
    std::cout << "#### Testing write performance to ringbuffer####"
              << std::endl;

    // auto flags = parse(argc, argv);
    std::string ip = "12.12.12.1";

    auto ln_s = kym::endpoint::Listen(ip, 8987);
    if (!ln_s.ok()) {
        std::cerr << "Error listening" << ln_s.status() << std::endl;
        return 1;
    }
    auto ln = ln_s.value();

    // Allocate a page of normal heap memory
    int size = 4 * 1024 * 1024;
    void *generic = malloc(size);
    struct ibv_mr *generic_mr =
        ibv_reg_mr(ln->GetPd(), generic, size,
                   IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);

    // Allocate "magic" buffer
    auto magic_s = kym::ringbuffer::GetMagicBuffer(size);
    if (!magic_s.ok()) {
        std::cerr << "error allocating magic buffer " << magic_s.status()
                  << std::endl;
        return 1;
    }
    void *magic = magic_s.value();
    struct ibv_mr *magic_mr =
        ibv_reg_mr(ln->GetPd(), magic, 2 * size,
                   IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);

    struct cinfo ci;
    ci.generic_addr = (uint64_t)generic;
    ci.generic_key = generic_mr->lkey;
    ci.magic_addr = (uint64_t)magic;
    ci.magic_key = magic_mr->lkey;

    opts.private_data = &ci;
    opts.private_data_len = sizeof(ci);

    auto ep_s = ln->Accept(opts);
    if (!ep_s.ok()) {
        std::cerr << "error allocating magic buffer " << magic_s.status()
                  << std::endl;
        return 1;
    }
    kym::endpoint::Endpoint *ep = ep_s.value();

    // Wait until client sent a message singaling the end of the measurement
    void *wait = malloc(4);
    struct ibv_mr *wait_mr =
        ibv_reg_mr(ln->GetPd(), wait, 4, IBV_ACCESS_LOCAL_WRITE);
    auto stat = ep->PostRecv(1, wait_mr->lkey, wait, 4);
    if (!stat.ok()) {
        std::cerr << "error posting receive " << stat << std::endl;
        return 1;
    }
    std::cerr << "posted rcv" << std::endl;
    auto wc_s = ep->PollRecvCq();
    if (!stat.ok()) {
        std::cerr << "error polling receive cq " << wc_s.status() << std::endl;
    }

    ibv_dereg_mr(wait_mr);
    free(wait);

    ibv_dereg_mr(magic_mr);
    kym::ringbuffer::FreeMagicBuffer(magic, size);

    ibv_dereg_mr(generic_mr);
    free(generic);

    stat = ep->Close();
    if (!stat.ok()) {
        std::cerr << "Error closing endpoint " << stat << std::endl;
        return 1;
    }
    stat = ln->Close();
    if (!stat.ok()) {
        std::cerr << "Error closing listener " << stat << std::endl;
        return 1;
    }

    return 0;
}
