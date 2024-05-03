#include <chrono>
#include <fmt/core.h>
#include <fstream>
#include <libxnvme.h>
#include <libxnvme_znd.h>

#include "utils.hpp"

using chrono_tp = std::chrono::high_resolution_clock::time_point;

class ZNSDevice
{
  public:
    xnvme_dev *dev = nullptr;
    u32 nsid;
    usize lba_bytes = 0;

    // no copying, that way lies double frees
    ZNSDevice(ZNSDevice &cpy) = delete;
    ZNSDevice operator=(ZNSDevice &cpy) = delete;

  public:
    ZNSDevice(std::string uri, u32 nsid) : nsid(nsid)
    {
        xnvme_opts opts = xnvme_opts_default();
        opts.be = "spdk";
        opts.nsid = nsid;
        dev = xnvme_dev_open(uri.c_str(), &opts);
        check_cond(dev == nullptr, "Failed to open device");
        int ret = xnvme_dev_derive_geo(dev);
        check_ret_neg(ret, "Failed to derive geometry of dev 1");

        auto geo = xnvme_dev_get_geo(dev);
        check_cond(geo->type != XNVME_GEO_ZONED,
                   "Device does not have zoned geometry");

        log_debug("Opened device {}, ns {}", uri, nsid);
        // xnvme_dev_pr(dev, XNVME_PR_DEF);

        lba_bytes = geo->lba_nbytes;
        log_debug("Device has blocks of {} bytes", lba_bytes);
    }
    ~ZNSDevice() { xnvme_dev_close(dev); }

    class DeviceBuf
    {
      private:
        DeviceBuf(DeviceBuf &cpy) = delete;
        DeviceBuf operator=(DeviceBuf &cpy) = delete;

      public:
        ZNSDevice &dev;
        void *buf = nullptr;
        DeviceBuf(ZNSDevice &dev, size_t bytes) : dev(dev)
        {
            buf = xnvme_buf_alloc(dev.dev, bytes);
            check_cond(buf == nullptr, "Failed to alloc size {} buf", bytes);
        }
        ~DeviceBuf() { xnvme_buf_free(dev.dev, buf); }
    };

    class DevQueue
    {
      private:
        DevQueue(DevQueue &cpy) = delete;
        DevQueue operator=(DevQueue &cpy) = delete;

      public:
        ZNSDevice &dev;
        xnvme_queue *q;

        u64 num_queued = 0;
        u64 num_completed = 0;
        u64 num_success = 0;
        u64 num_fail = 0;

        u64 total_us = 0;

        DevQueue(ZNSDevice &dev, u16 qd) : dev(dev)
        {
            auto re = xnvme_queue_init(dev.dev, qd, 0, &q);
            check_ret_neg(re, "Failed to init queue");

            xnvme_queue_set_cb(q, DevQueue::on_complete, this);
        }

        ~DevQueue()
        {
            print_stats();
            auto ret = xnvme_queue_term(q);
            if (ret < 0)
                log_error("Failed to destroy queue");
        }

        void drain()
        {
            auto ret = xnvme_queue_drain(q);
            check_ret_neg(ret, "Failed to drain queue");
        }

        class ZNSRequest
        {
          public:
            DevQueue &q;
            chrono_tp stime;

            ZNSRequest(DevQueue &q) : q(q) {}

            void start() { stime = std::chrono::high_resolution_clock::now(); }
            u64 end()
            {
                auto etime = std::chrono::high_resolution_clock::now();
                auto dur =
                    std::chrono::duration_cast<std::chrono::microseconds>(
                        etime - stime);
                return dur.count();
            }
        };

        int enq_append(u64 lba, usize bytes, void *buf)
        {
            assert(bytes % dev.lba_bytes == 0);
            assert(buf != nullptr);

            auto ctx = xnvme_queue_get_cmd_ctx(q);
            auto blocks = bytes / dev.lba_bytes;

            auto rq = new ZNSRequest(*this);
            xnvme_cmd_ctx_set_cb(ctx, on_complete, rq);

            int ret;
            while (true) {
                ret =
                    xnvme_znd_append(ctx, dev.nsid, lba, blocks, buf, nullptr);
                if (ret == -EBUSY || ret == -EAGAIN)
                    xnvme_queue_poke(q, 0);
                else
                    break;
            }

            rq->start();
            num_queued += 1;
            return ret;
        }

        int enq_read(u64 lba, usize bytes, void *buf)
        {
            assert(bytes % dev.lba_bytes == 0);
            assert(buf != nullptr);

            auto ctx = xnvme_queue_get_cmd_ctx(q);
            auto blocks = bytes / dev.lba_bytes;

            auto rq = new ZNSRequest(*this);
            xnvme_cmd_ctx_set_cb(ctx, on_complete, rq);

            int ret;
            while (true) {
                ret = xnvme_nvm_read(ctx, dev.nsid, lba, blocks, buf, nullptr);
                if (ret == -EBUSY || ret == -EAGAIN)
                    xnvme_queue_poke(q, 0);
                else
                    break;
            }

            rq->start();
            num_queued += 1;
            return ret;
        }

        void print_stats()
        {
            log_info("Queue stats: {} total, {} comp, {} succcess, {} fail",
                     num_queued, num_completed, num_success, num_fail);
        }

        static void on_complete(xnvme_cmd_ctx *ctx, void *cbarg)
        {
            auto r = static_cast<ZNSRequest *>(cbarg);
            r->q.total_us += r->end();
            r->q.num_completed += 1;

            if (xnvme_cmd_ctx_cpl_status(ctx) != 0) {
                // log_error("I/O request failed");
                // xnvme_cmd_ctx_pr(ctx, XNVME_PR_DEF);
                r->q.num_fail += 1;
            } else {
                r->q.num_success += 1;
            }

            delete r;
            // release the context
            xnvme_queue_put_cmd_ctx(ctx->async.queue, ctx);
        }
    };

    const xnvme_geo *geometry() { return xnvme_dev_get_geo(dev); }
    DevQueue create_queue(u16 qd) { return DevQueue(*this, qd); }
    DeviceBuf alloc(size_t nbytes) { return DeviceBuf(*this, nbytes); }

    xnvme_spec_znd_descr zone_desc(u64 lba)
    {
        xnvme_spec_znd_descr zone;
        xnvme_znd_descr_from_dev(dev, lba, &zone);
        return zone;
    }

    void append(u64 lba, usize bytes, void *buf)
    {
        assert(bytes % lba_bytes == 0);
        assert(buf != nullptr);

        auto blocks = bytes / lba_bytes;
        auto ctx = xnvme_cmd_ctx_from_dev(dev);

        // this should be synchronous
        auto res = xnvme_znd_append(&ctx, nsid, lba, blocks, buf, nullptr);

        check_ret_neg(res, "Failed append q lba {} blocks {}", lba, blocks);
        if (xnvme_cmd_ctx_cpl_status(&ctx) != 0) {
            log_error("Failed to append");
            xnvme_cmd_ctx_pr(&ctx, XNVME_PR_DEF);
        }
    }

    void read(u64 lba, usize bytes, void *buf)
    {
        assert(bytes % lba_bytes == 0);
        assert(buf != nullptr);

        auto blocks = bytes / lba_bytes;
        auto ctx = xnvme_cmd_ctx_from_dev(dev);
        // log_debug("Reading lba {:x} nlb {}", lba, blocks);
        auto res = xnvme_nvm_read(&ctx, nsid, lba, blocks, buf, nullptr);
        check_ret_neg(res, "Failed read lba 0x{:x} nlb {} ns {}", lba, blocks,
                      nsid);
        if (xnvme_cmd_ctx_cpl_status(&ctx) != 0) {
            log_error("Failed to read");
            xnvme_cmd_ctx_pr(&ctx, XNVME_PR_DEF);
        }
    }

    void finish_zone(u64 slba)
    {
        auto ctx = xnvme_cmd_ctx_from_dev(dev);
        auto res = xnvme_znd_mgmt_send(
            &ctx, nsid, slba, false, XNVME_SPEC_ZND_CMD_MGMT_SEND_FINISH,
            XNVME_SPEC_ZND_MGMT_OPEN_WITH_ZRWA, nullptr);
        check_ret_neg(res, "Failed to close zone");
        if (xnvme_cmd_ctx_cpl_status(&ctx) != 0) {
            log_error("Failed to close zone");
            xnvme_cmd_ctx_pr(&ctx, XNVME_PR_DEF);
        }
    }
};

void memset64(void *dest, u64 val, usize bytes)
{
    assert(bytes % 8 == 0);
    u64 *cdest = (u64 *)dest;
    for (usize i = 0; i < bytes / 8; i++)
        cdest[i] = val;
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        log_info("Usage: ./zstore <zone_num> <qd>");
        return 1;
    }

    u64 zone_dist = 0x80000;
    u64 zone_num = std::stoull(argv[1]);
    u16 qd = std::stoull(argv[2]);

    auto host = "10.0.0.2:23789";
    auto dev1 = ZNSDevice(host, 1);
    auto dev2 = ZNSDevice(host, 2);

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
        memset64((char *)buf1.buf + 4096 * i, i + data_off, 4096);
        memset64((char *)buf2.buf + 4096 * i, i + data_off, 4096);
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