#include <fmt/core.h>
#include <libxnvme.h>
#include <libxnvme_znd.h>

#include "utils.hpp"

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
    }
    ~ZNSDevice() { xnvme_dev_close(dev); }

    class DeviceBuf
    {
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
      public:
        ZNSDevice &dev;
        xnvme_queue *q;

        u64 num_queued;
        u64 num_completed;
        u64 num_success;
        u64 num_fail;

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

        int enq_append(u64 lba, usize bytes, void *buf)
        {
            assert(bytes % dev.lba_bytes == 0);
            assert(buf != nullptr);

            auto ctx = xnvme_queue_get_cmd_ctx(q);
            auto blocks = bytes / dev.lba_bytes;

            int ret;
            while (true) {
                ret =
                    xnvme_znd_append(ctx, dev.nsid, lba, blocks, buf, nullptr);
                if (ret == -EBUSY || ret == -EAGAIN)
                    xnvme_queue_poke(q, 0);
                else
                    break;
            }

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
            auto q = static_cast<DevQueue *>(cbarg);
            q->num_completed += 1;

            if (xnvme_cmd_ctx_cpl_status(ctx) != 0) {
                // log_error("I/O request failed");
                // xnvme_cmd_ctx_pr(ctx, XNVME_PR_DEF);
                q->num_fail += 1;
            }

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
    auto host = "192.168.1.156:23789";
    auto dev1 = ZNSDevice(host, 1);
    auto dev2 = ZNSDevice(host, 2);

    u64 zone_dist = 0x80000;
    u64 zone_num = 7;

    u64 zslba = zone_num * zone_dist;
    u16 qd = 64;

    auto zone1 = dev1.zone_desc(zslba);
    auto q1 = dev1.create_queue(qd);
    auto buf1 = dev1.alloc(zone1.zcap * dev1.lba_bytes);

    auto zone2 = dev2.zone_desc(zslba);
    auto q2 = dev2.create_queue(qd);
    auto buf2 = dev2.alloc(zone1.zcap * dev1.lba_bytes);

    auto zcap = std::min(zone1.zcap, zone2.zcap);
    // std::unique_ptr<char[]> buf(new char[zcap * 4096]);

    // NOTE we're not explicitly opening zones for now

    // fill bufs with repeated numbers corresponding to lba
    for (u64 i = 0; i < zcap; i++) {
        memset64(buf1.buf, i, dev1.lba_bytes);
        memset64(buf2.buf, i, dev2.lba_bytes);
        // memset64(buf.get(), i, zcap * 4096);
    }

    // append one block at a time for max re-ordering chance
    auto num_appends = zcap - 100;
    log_info("Appending {} entries of 4k each", num_appends);
    for (u64 i = 0; i < num_appends; i++) {
        if (i % 100 == 0)
            fmt::print(".");
        if (i % 1000 == 0)
            fmt::print("|{}\n", i / 1000);

        // auto r1 = q1.enq_append(zslba, 4096, buf.get() + i * 4096);
        // auto r2 = q2.enq_append(zslba, 4096, buf.get() + i * 4096);
        auto r1 = q1.enq_append(zslba, 4096, (char *)buf1.buf + i * 4096);
        auto r2 = q2.enq_append(zslba, 4096, (char *)buf2.buf + i * 4096);

        if (r1 < 0 || r2 < 0) // bail if we fail to queue
            break;
    }
    log_info("\nAppending done");

    q1.drain();
    q2.drain();

    return 0;
}