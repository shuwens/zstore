#pragma once

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
