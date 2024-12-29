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

    auto ep_s = kym::endpoint::Dial(ip, 8987, opts);
    if (!ep_s.ok()) {
        std::cerr << "error dialing " << ep_s.status() << std::endl;
        return 1;
    }
    kym::endpoint::Endpoint *ep = ep_s.value();

    struct cinfo *ci;
    ep->GetConnectionInfo((void **)&ci);

    int size = 64;
    int n = 1000000;
    void *send = malloc(size);
    struct ibv_mr *send_mr =
        ibv_reg_mr(ep->GetPd(), send, size, IBV_ACCESS_LOCAL_WRITE);

    // Latency tests
    // Test latency for generic mr
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < n; i++) {
        auto stat =
            ep->PostWrite(i, send_mr->lkey, send, size,
                          ci->generic_addr + 2 * 1024 * 1024, ci->generic_key);
        if (!stat.ok()) {
            std::cerr << "Error writing " << stat << std::endl;
            return 1;
        }
        auto wc_s = ep->PollSendCq();
        if (!wc_s.ok()) {
            std::cerr << "error polling send cq for write " << wc_s.status()
                      << std::endl;
        }
    }
    auto end = std::chrono::high_resolution_clock::now();
    double dur =
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - start)
            .count() /
        1000.0;
    std::cout << "Mean write latency generic mr msg size " << size << " bytes"
              << std::endl;
    std::cout << dur / (double)n << std::endl;

    // Test latency for magic mr
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < n; i++) {
        auto stat =
            ep->PostWrite(i, send_mr->lkey, send, size,
                          ci->magic_addr + 2 * 1024 * 1024, ci->magic_key);
        if (!stat.ok()) {
            std::cerr << "Error writing " << stat << std::endl;
            return 1;
        }
        auto wc_s = ep->PollSendCq();
        if (!wc_s.ok()) {
            std::cerr << "error polling send cq for write " << wc_s.status()
                      << std::endl;
        }
    }
    end = std::chrono::high_resolution_clock::now();
    dur = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start)
              .count() /
          1000.0;
    std::cout << "Mean write latency magic mr msg size " << size << " bytes"
              << std::endl;
    std::cout << dur / (double)n << std::endl;

    // Test latency for magic mr over end of buffer
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < n; i++) {
        auto stat =
            ep->PostWrite(i, send_mr->lkey, send, size,
                          ci->magic_addr + 4 * 1024 * 1024 - 32, ci->magic_key);
        if (!stat.ok()) {
            std::cerr << "Error writing " << stat << std::endl;
            return 1;
        }
        auto wc_s = ep->PollSendCq();
        if (!wc_s.ok()) {
            std::cerr << "error polling send cq for write " << wc_s.status()
                      << std::endl;
        }
    }
    end = std::chrono::high_resolution_clock::now();
    dur = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start)
              .count() /
          1000.0;
    std::cout << "Mean write latency magic mr msg size " << size << " bytes"
              << std::endl;
    std::cout << "Writing over the end of the buffer" << std::endl;
    std::cout << dur / (double)n << std::endl;

    std::chrono::milliseconds timespan(1000);
    std::this_thread::sleep_for(timespan);

    auto stat = ep->PostImmidate(1, 4);
    if (!stat.ok()) {
        std::cerr << "Error sending end " << stat << std::endl;
        return 1;
    }
    auto wc_s = ep->PollSendCq();
    if (!wc_s.ok()) {
        std::cerr << "error polling send cq for end" << wc_s.status()
                  << std::endl;
    }
    stat = ep->Close();
    if (!stat.ok()) {
        std::cerr << "Error closing endpoint " << stat << std::endl;
        return 1;
    }

    return 0;
}
