#include <chrono>
#include <fmt/core.h>
#include <fstream>
#include <libxnvme.h>
#include <libxnvme_znd.h>

#include "include/global.h"
#include "include/helper.h"
#include "include/kv_handler.h"
#include "include/utils.hpp"
#include "include/zns_device.h"
#include "include/zstore.h"

void create_dummy_objects()
{
    log_info("Create dummy objects in table: foo, bar, test");

    std::vector<std::string> keys;
    keys.push_back("foo");
    keys.push_back("bar");
    keys.push_back("test");

    int object_size = 64;
    for (const std::string &it : keys) {
        struct kvobject *o = (struct kvobject *)malloc(sizeof(*o) + object_size);
        o->name = it;
        o->data = malloc(object_size);
        o->len = object_size;

        mem_obj_table[it] = *o;
    }
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

    create_dummy_objects();
    // Start the web server controllers.
    KVstoreHandler h;
    CivetServer web_server = startWebServer(h);
    // struct mg_context* web_server = startWebServer();

    while (1) {
        sleep(1);
    }

    // Stop the web server.
    // mg_stop(web_server);
    mg_exit_library();

    return 0;
}