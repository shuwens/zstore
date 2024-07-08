#include <chrono>
#include <fmt/core.h>
#include <fstream>
#include <libxnvme.h>
#include <libxnvme_znd.h>

#include "include/utils.hpp"
#include "include/zns_device.h"

using chrono_tp = std::chrono::high_resolution_clock::time_point;

void memset64(void *dest, u64 val, usize bytes)
{
    assert(bytes % 8 == 0);
    u64 *cdest = (u64 *)dest;
    for (usize i = 0; i < bytes / 8; i++)
        cdest[i] = val;
}

int main(int argc, char **argv)
{

    if (argc < 4) {
        log_info("Usage: ./zstore <node> <zone_num> <qd>");
        return 1;
    }

    u64 zone_dist = 0x80000;
    u64 zone_num = std::stoull(argv[2]);
    u16 qd = std::stoull(argv[3]);
    u16 node = std::stoull(argv[1]);

    std::string host1;
    std::string host2;
    if (node == 1) {
        host1 = "192.168.1.121:4420";
        host2 = "192.168.1.121:5520";
    } else if (node == 2) {
        host1 = "192.168.1.121:6620";
        host2 = "192.168.1.121:7720";
    } else if (node == 4) {
        host1 = "192.168.1.121:8820";
        host2 = "192.168.1.121:9920";
    } else
        return 1;

    auto dev1 = ZNSDevice(host1, 1);
    auto dev2 = ZNSDevice(host2, 2);

    u64 zslba = zone_num * zone_dist;

    auto zone1 = dev1.zone_desc(zslba);
    auto wq1 = dev1.create_queue(qd);
    auto buf1 = dev1.alloc(zone1.zcap * dev1.lba_bytes);

    auto zone2 = dev2.zone_desc(zslba);
    auto wq2 = dev2.create_queue(qd);
    auto buf2 = dev2.alloc(zone1.zcap * dev1.lba_bytes);

    auto zcap = std::min(zone1.zcap, zone2.zcap);

    // NOTE we're not explicitly opening zones for now

    // fill bufs with repeated numbers corresponding to lba
    u64 data_off = 0xdeadbeef;
    for (u64 i = 0; i < zcap; i++) {
        memset64((char *)buf1.buf + 4096 * i, i + data_off + qd, 4096);
        memset64((char *)buf2.buf + 4096 * i, i + data_off + qd, 4096);
    }

    // append one block at a time for max re-ordering chance
    u64 num_appends = 128'000;
    log_info("Appending to zone {} (lba 0x{:x}), {} entries of 4k each",
             zone_num, zslba, num_appends);
    for (u64 i = 0; i < num_appends; i++) {
        if (i % 100 == 0)
            fmt::print(".");
        if (i % 1000 == 999)
            fmt::print("|{}\n", i / 1000);

        auto r1 = wq1.enq_append(zslba, 4096, (char *)buf1.buf + i * 4096);
        auto r2 = wq2.enq_append(zslba, 4096, (char *)buf2.buf + i * 4096);
        if (r1 < 0 || r2 < 0) // bail if we fail to queue
            break;
    }

    wq1.drain();
    wq2.drain();
    dev1.finish_zone(zslba);
    dev2.finish_zone(zslba);
    log_info("\nAppending done");

    auto total_write_us = wq1.total_us + wq2.total_us;
    auto total_write_num = wq1.num_completed + wq2.num_completed;
    auto avg_write_us = total_write_us / total_write_num;
    log_info("Wrote {} blocks in {} us, avg {} us", total_write_num,
             total_write_us, avg_write_us);

    log_info("Reading back from zone {}", zone_num);

    auto rbuf1 = new char[4096 * num_appends];
    auto rbuf2 = new char[4096 * num_appends];
    std::vector<u64> data1;
    std::vector<u64> data2;

    auto rq1 = dev1.create_queue(qd);
    auto rq2 = dev2.create_queue(qd);

    for (u64 i = 0; i < num_appends; i++) {
        if (i % 100 == 0)
            fmt::print("*");
        if (i % 1000 == 0)
            fmt::print("|{}\n", i / 1000);

        rq1.enq_read(zslba + i, 4096, rbuf1 + i * 4096);
        rq2.enq_read(zslba + i, 4096, rbuf2 + i * 4096);

        data1.push_back(*(u64 *)rbuf1);
        data2.push_back(*(u64 *)rbuf2);
    }

    delete[] rbuf2;
    delete[] rbuf1;

    auto tot_read_us = rq1.total_us + rq2.total_us;
    auto tot_read_num = rq1.num_completed + rq2.num_completed;
    auto avg_read_us = tot_read_us / tot_read_num;
    log_info("Read {} blocks in {} us, avg {} us", tot_read_num, tot_read_us,
             avg_read_us);

    u64 mismatches = 0;
    for (u64 i = 0; i < num_appends; i++) {
        if (data1[i] != data2[i])
            mismatches += 1;
    }
    log_info("Found {} mismatches", mismatches);

    std::ofstream of1("data1.txt");
    std::ofstream of2("data2.txt");
    for (auto d : data1)
        of1 << d - data_off << " ";
    for (auto d : data2)
        of2 << d - data_off << " ";

    log_info("Wrote to files data1.txt and data2.txt");
    return 0;
}
