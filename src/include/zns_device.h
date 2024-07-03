#pragma once

#include <chrono>
#include <fmt/core.h>
#include <fstream>
#include <libxnvme.h>
#include <libxnvme_znd.h>

#include "utils.hpp"
using chrono_tp = std::chrono::high_resolution_clock::time_point;

/**
 * @brief Represents a Zoned Namespace (ZNS) device.
 *
 * The `ZNSDevice` class provides an interface to interact with a ZNS device.
 * It encapsulates the functionality to open and close the device, allocate and
 * free buffers, perform append and read operations, and manage zones.
 */
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

    /**
     * @class DeviceBuf
     * @brief Represents a device buffer used for I/O operations.
     *
     * The DeviceBuf class provides a wrapper for managing device buffers used
     * for I/O operations. It allocates and frees the buffer memory using the
     * xnvme_buf_alloc and xnvme_buf_free functions. The buffer is associated
     * with a ZNSDevice object, which represents the underlying device. The
     * class also disables the copy constructor and copy assignment operator to
     * prevent unintended copying.
     */
    class DeviceBuf
    {
      private:
        DeviceBuf(DeviceBuf &cpy) = delete;
        DeviceBuf operator=(DeviceBuf &cpy) = delete;

      public:
        ZNSDevice &dev; ///< Reference to the ZNSDevice object associated with
                        ///< the buffer
        void *buf = nullptr; ///< Pointer to the allocated buffer memory

        /**
         * @brief Constructs a DeviceBuf object with the specified device and
         * buffer size.
         *
         * @param dev The ZNSDevice object associated with the buffer
         * @param bytes The size of the buffer in bytes
         */
        DeviceBuf(ZNSDevice &dev, size_t bytes) : dev(dev)
        {
            buf = xnvme_buf_alloc(dev.dev, bytes);
            check_cond(buf == nullptr, "Failed to alloc size {} buf", bytes);
        }

        /**
         * @brief Destructor that frees the allocated buffer memory.
         */
        ~DeviceBuf() { xnvme_buf_free(dev.dev, buf); }
    };

    /**
     * @class DevQueue
     * @brief Represents a queue for submitting I/O requests to a ZNSDevice.
     *
     * The DevQueue class provides functionality for initializing and managing a
     * queue for submitting I/O requests to a ZNSDevice. It tracks statistics
     * such as the number of queued requests, completed requests, successful
     * requests, and failed requests. It also provides methods for draining the
     * queue and printing the queue statistics.
     *
     * The DevQueue class also contains an inner class ZNSRequest, which
     * represents an individual I/O request. Each ZNSRequest is associated with
     * a DevQueue and tracks the start and end time of the request for
     * calculating the duration.
     */
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

        /**
         * @brief Represents a ZNS (Zoned Namespace) request.
         *
         * This class encapsulates a ZNS request and provides methods to measure
         * the execution time of the request.
         */
        class ZNSRequest
        {
          public:
            DevQueue &q; /**< Reference to the DevQueue object associated with
                            the request. */
            chrono_tp stime; /**< Start time of the request. */

            /**
             * @brief Constructs a ZNSRequest object.
             *
             * @param q Reference to the DevQueue object associated with the
             * request.
             */
            ZNSRequest(DevQueue &q) : q(q) {}

            /**
             * @brief Starts the timer for the request execution time.
             */
            void start() { stime = std::chrono::high_resolution_clock::now(); }

            /**
             * @brief Ends the timer and returns the execution time of the
             * request.
             *
             * @return The execution time of the request in microseconds.
             */
            u64 end()
            {
                auto etime = std::chrono::high_resolution_clock::now();
                auto dur =
                    std::chrono::duration_cast<std::chrono::microseconds>(
                        etime - stime);
                return dur.count();
            }
        };

        /**
         * Enqueues an append command to the ZNS device.
         *
         * @param lba The logical block address to append data to.
         * @param bytes The number of bytes to append.
         * @param buf Pointer to the buffer containing the data to be appended.
         * @return 0 on success, or a negative error code on failure.
         */
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

        /**
         * Enqueues a read command to the device.
         *
         * @param lba The logical block address to read from.
         * @param bytes The number of bytes to read.
         * @param buf The buffer to store the read data.
         * @return The result of the read command.
         */
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

        /**
         * Callback function called when an I/O operation is completed.
         *
         * @param ctx The command context associated with the completed
         * operation.
         * @param cbarg A pointer to the callback argument, which is a
         * ZNSRequest object.
         */
        static void on_complete(xnvme_cmd_ctx *ctx, void *cbarg)
        {
            auto r = static_cast<ZNSRequest *>(cbarg);
            r->q.total_us += r->end();
            r->q.num_completed += 1;

            if (xnvme_cmd_ctx_cpl_status(ctx) != 0) {
                log_error("I/O request failed");
                xnvme_cmd_ctx_pr(ctx, XNVME_PR_DEF);
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

    /**
     * Finish a zone by sending a management command to close it.
     *
     * @param slba The starting logical block address (LBA) of the zone to
     * finish.
     */
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
