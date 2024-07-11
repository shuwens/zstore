// #include <cassert>
#define FMT_HEADER_ONLY
#include <fmt/core.h>
// #include "include/global.h"
// #include <libxnvme.h>
// #include <libxnvme_znd.h>
// #include <spdk/event.h>
#include "include/spdk_zns.h"
#include "include/utils.hpp"
#include "include/zstore.h"
// #include "spdk/bdev.h"
// #include "spdk/bdev_zone.h"
#include "spdk/env.h"
#include "spdk/event.h"
#include "spdk/log.h"
// #include "spdk/stdinc.h"
// #include "spdk/string.h"
// #include "spdk/thread.h"
// #include "zstore.cc"
// #include <atomic>

int main(int argc, char **argv)
{
    // if (argc < 3) {
    //     log_info("Usage: ./zstore <zone_num> <qd>");
    //     return 1;
    // }

    int rc = 0;

    struct spdk_app_opts opts = {};
    struct zstore_context_t ctx = {};

    // u64 zone_num = std::stoull(argv[1]);
    // u16 qd = std::stoull(argv[2]);

    // ctx.zone_num = std::stoull(argv[1]);
    // ctx.zone_num = std::stoull(argv[1]);
    log_debug("Starting Zstore with zone_num={}", ctx.zone_num);

    rc = znd_start(argc, argv, &opts, &ctx);

    // auto host = "192.168.1.121:4420";

    // Create and configure Zstore instance
    std::string zstore_name, bucket_name;
    zstore_name = "test";
    Zstore zstore(zstore_name);

    zstore.SetVerbosity(1);

    // create a bucket: this process is now manual, not via create/get bucket
    // zstore.buckets.push_back(AWS_S3_Bucket(bucket_name, "db"));

    // create_dummy_objects(zstore);

    rc = znd_teardown(&ctx);
    return rc;
}
