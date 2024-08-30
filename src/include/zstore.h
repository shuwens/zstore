#pragma once

// #include "../zstore.cc"
#include "global.h"
#include "helper.h"
#include "object.h"
#include "request_handler.h"
#include "utils.hpp"
#include "zns_device.h"
// #include "zstore.h"
#include <chrono>
#include <fmt/core.h>
#include <fstream>
#include <libxnvme.h>
#include <libxnvme_znd.h>

// void create_dummy_objects(Zstore zstore)
// {
//     log_info("Create dummy objects in table: foo, bar, test");
//
//     zstore.putObject("foo", "foo_data");
//     zstore.putObject("bar", "bar_data");
//     zstore.putObject("baz", "baz_data");
// }
static int http_server_fn(void *arg)
{
    log_info("Starting http server function");
    // Create and configure Zstore instance
    std::string zstore_name, bucket_name;
    zstore_name = "test";
    // Zstore zstore(zstore_name);

    // zstore.SetVerbosity(1);

    // create a bucket: this process is now manual, not via create/get bucket
    // zstore.buckets.push_back(AWS_S3_Bucket(bucket_name, "db"));

    // create_dummy_objects(zstore);
    // Start the web server controllers.
    ZstoreHandler h;
    CivetServer web_server = startWebServer(h);

    while (1) {
        sleep(1);
    }

    // Stop the web server.
    // mg_stop(web_server);
    mg_exit_library();

    return 0;
}
