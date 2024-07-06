#include <cassert>
// #include <chrono>
#include <fmt/core.h>
// #include <fstream>
#include <libxnvme.h>
#include <libxnvme_znd.h>

#include "include/global.h"
#include "include/helper.h"
// #include "include/object.h"
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

    std::string baz = zstore.getObject("baz");
    assert(baz == "baz_data");
    zstore.deleteObject("baz");
}

void create_dummy_objects(Zstore zstore, u64 zone_num, u16 qd, ZNSDevice *dev)
{
    log_info("Create dummy objects in table: foo, bar, test");

    // zstore.putObject("foo", "foo_data");
    // zstore.putObject("bar", "bar_data");
    // zstore.putObject("baz", "baz_data");

    // std::string baz = zstore.getObject("baz");
    // assert(baz == "baz_data");
    // zstore.deleteObject("baz");

    u64 zslba = zone_num * zone_dist;

    auto zone1 = dev->zone_desc(zslba);
    auto wq1 = dev->create_queue(qd);
    auto buf1 = dev->alloc(zone1.zcap * dev->lba_bytes);

    auto r1 = wq1.enq_append(zslba, 4096, (char *)"foo_data");
    if (r1 < 0) // bail if we fail to queue
        log_error("enq append failed", r1);

    wq1.drain();
    // dev1.finish_zone(zslba);
    log_info("\nAppending done");
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        log_info("Usage: ./zstore <zone_num> <qd>");
        return 1;
    }

    u64 zone_num = std::stoull(argv[1]);
    u16 qd = std::stoull(argv[2]);

    auto host = "192.168.1.121:4420";
    auto dev1 = new ZNSDevice(host, 1);

    // Create and configure Zstore instance
    std::string zstore_name, bucket_name;
    zstore_name = "test";
    Zstore zstore(zstore_name);

    zstore.SetVerbosity(1);

    // create a bucket: this process is now manual, not via create/get bucket
    // zstore.buckets.push_back(AWS_S3_Bucket(bucket_name, "db"));

    // create_dummy_objects(zstore);
    create_dummy_objects(zstore, zone_num, qd, dev1);

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