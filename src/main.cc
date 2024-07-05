#include <chrono>
#include <fmt/core.h>
#include <fstream>
#include <libxnvme.h>
#include <libxnvme_znd.h>

#include "include/global.h"
#include "include/helper.h"
#include "include/object.h"
#include "include/request_handler.h"
#include "include/utils.hpp"
#include "include/zns_device.h"
#include "include/zstore.h"
#include "zstore.cc"

void create_dummy_objects(Zstore zstore)
{
    log_info("Create dummy objects in table: foo, bar, test");

    zstore.putObject("foo", "foo_data");
    zstore.putObject("bar", "bar_data");
    zstore.putObject("baz", "baz_data");
}

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
    // zstore.buckets.push_back(AWS_S3_Bucket(bucket_name, "db"));

    create_dummy_objects(zstore);
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