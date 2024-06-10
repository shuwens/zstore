#include <chrono>
#include <fmt/core.h>
#include <fstream>
#include <libxnvme.h>
#include <libxnvme_znd.h>

#include <curlpp/Options.hpp>
#include <curlpp/cURLpp.hpp>
#include <unistd.h>

#include "include/utils.h"
#include "include/zns_device.h"
#include "include/zstore.h"
#include "zstore/backend.cc"
#include "zstore/zstore.cc"

#include "zstore/http_server.cc"
// #include "zstore/webServer.cc"
// #include "include/webServer.h"

// #include "s3/aws_s3.h"

using chrono_tp = std::chrono::high_resolution_clock::time_point;

void memset64(void *dest, u64 val, usize bytes)
{
    assert(bytes % 8 == 0);
    u64 *cdest = (u64 *)dest;
    for (usize i = 0; i < bytes / 8; i++)
        cdest[i] = val;
}

// int begin_request(struct mg_connection *conn)
// {
//     Backend backend("test_backend");
//     return backend.DoRequest(conn);
// }

int main(int argc, char **argv)
{
    if (argc < 3) {
        log_info("Usage: ./zstore <zone_num> <qd>");
        return 1;
    }

    // Create and configure Zstore instance
    std::string zstore_name, bucket_name;
    zstore_name = "test";
    Zstore zstore(zstore_name);

    zstore.SetVerbosity(1);

    // create a bucket: this process is now manual, not via create/get bucket
    zstore.buckets.push_back(AWS_S3_Bucket(bucket_name, "db"));

    // Start the web server controllers.
    CivetServer web_server = startWebServer();
// struct mg_context* web_server = startWebServer();

    while (1) {
        sleep(1);
    }

    // Stop the web server.
    // mg_stop(web_server);
    mg_exit_library();

    return 0;
}
