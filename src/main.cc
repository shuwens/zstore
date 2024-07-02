#include <chrono>
#include <fmt/core.h>
#include <fstream>
#include <libxnvme.h>
#include <libxnvme_znd.h>

#include "include/utils.hpp"
#include "include/zns_device.h"
#include "include/global.h"
#include "include/zstore.h"
#include "include/helper.h"


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
    //zstore.buckets.push_back(AWS_S3_Bucket(bucket_name, "db"));

    create_dummy_objects();
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