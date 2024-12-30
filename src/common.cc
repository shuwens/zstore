#include "include/common.h"
#include "include/configuration.h"
#include "include/zstore_controller.h"
#include "spdk/nvme_zns.h"
#include "spdk/string.h"
#include <tuple>

std::shared_mutex g_shared_mutex_;

static void dummy_disconnect_handler(struct spdk_nvme_qpair *qpair,
                                     void *poll_group_ctx)
{
}

int handleIoCompletions(void *args)
{

    struct spdk_nvme_poll_group *pollGroup =
        (struct spdk_nvme_poll_group *)args;
    int r = 0;
    r = spdk_nvme_poll_group_process_completions(pollGroup, 0,
                                                 dummy_disconnect_handler);
    return r > 0 ? SPDK_POLLER_BUSY : SPDK_POLLER_IDLE;
}

int ioWorker(void *args)
{
    IoThread *ioThread = (IoThread *)args;
    struct spdk_thread *thread = ioThread->thread;
    spdk_set_thread(thread);
    spdk_poller_register(handleIoCompletions, ioThread->group, 0);
    while (true) {
        spdk_thread_poll(thread, 0, 0);
    }
}

// Zone finish operations.
//
// Inst wrapper for zone finishprovides instrumentation for latency tracking.

void spdk_nvme_zone_finish_wrapper(
    struct spdk_thread *thread, struct spdk_nvme_ns *ns,
    struct spdk_nvme_qpair *qpair, uint64_t offset, uint32_t flags,
    std::move_only_function<void(Result<const spdk_nvme_cpl *>)> cb)
{
    auto cb_heap = new decltype(cb)(std::move(cb));
    auto fn = new std::move_only_function<void(void)>([=]() {
        int rc = spdk_nvme_zns_finish_zone(
            ns, qpair, offset, flags,
            [](void *arg, const spdk_nvme_cpl *completion) mutable {
                auto cb3 = reinterpret_cast<decltype(cb_heap)>(arg);
                (*cb3)(completion);
                delete cb3;
            },
            (void *)(cb_heap));
        if (rc != 0) {
            log_debug("{}: cmd finish zone failed offset {}",
                      spdk_strerror(-rc), offset);
            (*cb_heap)(outcome::failure(std::errc::io_error));
            delete cb_heap;
        }
    });
    thread_send_msg(
        thread,
        [](void *fn2) {
            auto rc = reinterpret_cast<decltype(fn)>(fn2);
            (*rc)();
            delete rc;
        },
        fn);
}

auto spdk_nvme_zone_finish_async(
    struct spdk_thread *thread, struct spdk_nvme_ns *ns,
    struct spdk_nvme_qpair *qpair, uint64_t offset,
    uint32_t flags) -> asio::awaitable<Result<const spdk_nvme_cpl *>>
{
    auto init = [](auto completion_handler, spdk_thread *thread,
                   spdk_nvme_ns *ns, spdk_nvme_qpair *qpair, uint64_t offset,
                   uint32_t flags) {
        spdk_nvme_zone_finish_wrapper(thread, ns, qpair, offset, flags,
                                      std::move(completion_handler));
    };

    return asio::async_initiate<decltype(asio::use_awaitable),
                                void(Result<const spdk_nvme_cpl *>)>(
        init, asio::use_awaitable, thread, ns, qpair, offset, flags);
}

// Instrumentation for zone finish
void spdk_nvme_zone_finish_wrapper_inst(
    Timer &timer, struct spdk_thread *thread, struct spdk_nvme_ns *ns,
    struct spdk_nvme_qpair *qpair, uint64_t offset, uint32_t flags,
    std::move_only_function<void(Result<const spdk_nvme_cpl *>)> cb)
{
    auto cb_heap = new std::pair<
        std::move_only_function<void(Result<const spdk_nvme_cpl *>)>, Timer &>(
        decltype(cb)(std::move(cb)), timer);
    auto fn = new std::move_only_function<void(void)>([=, &timer]() {
        timer.t4 = std::chrono::high_resolution_clock::now();
        int rc = spdk_nvme_zns_finish_zone(
            ns, qpair, offset, flags,
            [](void *arg, const spdk_nvme_cpl *completion) mutable {
                auto tuple_ = reinterpret_cast<decltype(cb_heap)>(arg);
                auto &cb3 = tuple_->first;
                tuple_->second.t5 = std::chrono::high_resolution_clock::now();
                (cb3)(completion);
                delete tuple_;
            },
            (void *)(cb_heap));
        if (rc != 0) {
            log_debug("{}: cmd read failed offset {}", spdk_strerror(-rc),
                      offset);
            (cb_heap->first)(outcome::failure(std::errc::io_error));
            delete cb_heap;
        }
    });
    thread_send_msg(
        thread,
        [](void *fn2) {
            auto rc = reinterpret_cast<decltype(fn)>(fn2);
            (*rc)();
            delete rc;
        },
        fn);
}

auto spdk_nvme_zone_finish_async_inst(
    Timer &timer, struct spdk_thread *thread, struct spdk_nvme_ns *ns,
    struct spdk_nvme_qpair *qpair, uint64_t offset,
    uint32_t flags) -> asio::awaitable<Result<const spdk_nvme_cpl *>>
{
    timer.t2 = std::chrono::high_resolution_clock::now();
    auto init = [](auto completion_handler, Timer *timer, spdk_thread *thread,
                   spdk_nvme_ns *ns, spdk_nvme_qpair *qpair, uint64_t offset,
                   uint32_t flags) {
        timer->t3 = std::chrono::high_resolution_clock::now();
        spdk_nvme_zone_finish_wrapper_inst(*timer, thread, ns, qpair, offset,
                                           flags,
                                           std::move(completion_handler));
    };
    return asio::async_initiate<decltype(asio::use_awaitable),
                                void(Result<const spdk_nvme_cpl *>)>(
        init, asio::use_awaitable, &timer, thread, ns, qpair, offset, flags);
}

auto zoneFinish(void *arg1) -> asio::awaitable<void>
{
    RequestContext *ctx = reinterpret_cast<RequestContext *>(arg1);
    auto ioCtx = ctx->ioContext;
    assert(ctx->ctrl != nullptr);

    Timer timer;
    const spdk_nvme_cpl *cpl;
    if (Configuration::GetSamplingRate() > 0) {
        timer.t1 = std::chrono::high_resolution_clock::now();
        auto res_cpl = co_await spdk_nvme_zone_finish_async_inst(
            timer, ctx->io_thread, ioCtx.ns, ioCtx.qpair, ioCtx.offset,
            ioCtx.flags);
        cpl = res_cpl.value();
        timer.t6 = std::chrono::high_resolution_clock::now();

        if (ctx->ctrl->mTotalCounts % Configuration::GetSamplingRate() == 0 ||
            tdiff_us(timer.t6, timer.t1) > 1000)
            log_debug(
                "t2-t1 {}us, t3-t2 {}us, t4-t3 {}us, t5-t4 {}us, t6-t5 {}us ",
                tdiff_us(timer.t2, timer.t1), tdiff_us(timer.t3, timer.t2),
                tdiff_us(timer.t4, timer.t3), tdiff_us(timer.t5, timer.t4),
                tdiff_us(timer.t6, timer.t5));
    } else {
        auto res_cpl = co_await spdk_nvme_zone_finish_async(
            ctx->io_thread, ioCtx.ns, ioCtx.qpair, ioCtx.offset, ioCtx.flags);
        // if (res_cpl.has_error()) {
        //     // log_error("cpl error status");
        //     co_return outcome::failure(std::errc::io_error);
        // } else
        //     cpl = res_cpl.value();
    }
    // if (spdk_nvme_cpl_is_error(cpl)) {
    //     // log_error("I/O error status: {}",
    //     //           spdk_nvme_cpl_get_status_string(&cpl->status));
    //     // log_debug("Unimplemented: put context back in pool");
    //     co_return outcome::failure(std::errc::io_error);
    // }

    ctx->success = true;

    ctx->ctrl->mManagementCounts++;
    assert(ctx->ctrl != nullptr);

    // co_return outcome::success();
}

// Zone append operations.
//
// Inst wrapper for zone append provides instrumentation for latency tracking.

void spdk_nvme_zone_append_wrapper(
    struct spdk_thread *thread, struct spdk_nvme_ns *ns,
    struct spdk_nvme_qpair *qpair, void *data, uint64_t offset, uint32_t size,
    uint32_t flags,
    std::move_only_function<void(Result<const spdk_nvme_cpl *>)> cb)
{
    if (Configuration::Debugging())
        log_debug("APPEND: offset {}, size {}", offset, size);
    auto cb_heap = new decltype(cb)(std::move(cb));
    auto fn = new std::move_only_function<void(void)>([=]() {
        int rc = spdk_nvme_zns_zone_append(
            ns, qpair, data, offset, size,
            [](void *arg, const spdk_nvme_cpl *completion) mutable {
                auto cb3 = reinterpret_cast<decltype(cb_heap)>(arg);
                (*cb3)(completion);
                delete cb3;
            },
            (void *)(cb_heap), flags);
        if (rc != 0) {
            log_debug("{} ({}): cmd append failed offset {}, size {}",
                      spdk_strerror(-rc), rc, offset, size);
            (*cb_heap)(outcome::failure(std::errc::io_error));
            delete cb_heap;
        }
    });
    thread_send_msg(
        thread,
        [](void *fn2) {
            auto rc = reinterpret_cast<decltype(fn)>(fn2);
            (*rc)();
            delete rc;
        },
        fn);
}

auto spdk_nvme_zone_append_async(
    struct spdk_thread *thread, struct spdk_nvme_ns *ns,
    struct spdk_nvme_qpair *qpair, void *data, uint64_t offset, uint32_t size,
    uint32_t flags) -> asio::awaitable<Result<const spdk_nvme_cpl *>>
{
    auto init = [](auto completion_handler, spdk_thread *thread,
                   spdk_nvme_ns *ns, spdk_nvme_qpair *qpair, void *data,
                   uint64_t offset, uint32_t size, uint32_t flags) {
        spdk_nvme_zone_append_wrapper(thread, ns, qpair, data, offset, size,
                                      flags, std::move(completion_handler));
    };
    return asio::async_initiate<decltype(asio ::use_awaitable),
                                void(Result<const spdk_nvme_cpl *>)>(
        init, asio::use_awaitable, thread, ns, qpair, data, offset, size,
        flags);
}

// Instrumentation for zone append
void spdk_nvme_zone_append_wrapper_inst(
    Timer &timer, struct spdk_thread *thread, struct spdk_nvme_ns *ns,
    struct spdk_nvme_qpair *qpair, void *data, uint64_t offset, uint32_t size,
    uint32_t flags,
    std::move_only_function<void(Result<const spdk_nvme_cpl *>)> cb)
{
    auto cb_heap = new std::pair<
        std::move_only_function<void(Result<const spdk_nvme_cpl *>)>, Timer &>(
        decltype(cb)(std::move(cb)), timer);
    auto fn = new std::move_only_function<void(void)>([=, &timer]() {
        timer.t4 = std::chrono::high_resolution_clock::now();
        int rc = spdk_nvme_zns_zone_append(
            ns, qpair, data, offset, size,
            [](void *arg, const spdk_nvme_cpl *completion) mutable {
                auto tuple_ = reinterpret_cast<decltype(cb_heap)>(arg);
                auto &cb3 = tuple_->first;
                tuple_->second.t5 = std::chrono::high_resolution_clock::now();
                (cb3)(completion);
                delete tuple_;
            },
            (void *)(cb_heap), flags);
        if (rc != 0) {
            log_debug("{}: cmd append failed offset {}, size {}",
                      spdk_strerror(-rc), offset, size);
            (cb_heap->first)(outcome::failure(std::errc::io_error));
            delete cb_heap;
        }
    });
    thread_send_msg(
        thread,
        [](void *fn2) {
            auto rc = reinterpret_cast<decltype(fn)>(fn2);
            (*rc)();
            delete rc;
        },
        fn);
}

auto spdk_nvme_zone_append_async_inst(
    Timer &timer, struct spdk_thread *thread, struct spdk_nvme_ns *ns,
    struct spdk_nvme_qpair *qpair, void *data, uint64_t offset, uint32_t size,
    uint32_t flags) -> asio::awaitable<Result<const spdk_nvme_cpl *>>
{
    timer.t2 = std::chrono::high_resolution_clock::now();
    auto init = [](auto completion_handler, Timer *timer, spdk_thread *thread,
                   spdk_nvme_ns *ns, spdk_nvme_qpair *qpair, void *data,
                   uint64_t offset, uint32_t size, uint32_t flags) {
        timer->t3 = std::chrono::high_resolution_clock::now();
        spdk_nvme_zone_append_wrapper_inst(*timer, thread, ns, qpair, data,
                                           offset, size, flags,
                                           std::move(completion_handler));
    };
    return asio::async_initiate<decltype(asio::use_awaitable),
                                void(Result<const spdk_nvme_cpl *>)>(
        init, asio::use_awaitable, &timer, thread, ns, qpair, data, offset,
        size, flags);
}

auto zoneAppend(void *arg1) -> asio::awaitable<void>
{
    RequestContext *ctx = reinterpret_cast<RequestContext *>(arg1);
    auto ioCtx = ctx->ioContext;
    assert(ctx->ctrl != nullptr);

    Timer timer;
    const spdk_nvme_cpl *cpl;
    if (Configuration::GetSamplingRate() > 0) {
        timer.t1 = std::chrono::high_resolution_clock::now();
        auto res_cpl = co_await spdk_nvme_zone_append_async_inst(
            timer, ctx->io_thread, ioCtx.ns, ioCtx.qpair, ioCtx.data,
            ioCtx.offset, ioCtx.size, ioCtx.flags);
        cpl = res_cpl.value();
        timer.t6 = std::chrono::high_resolution_clock::now();

        if (ctx->ctrl->mTotalCounts % Configuration::GetSamplingRate() == 0)
            log_debug(
                "t2-t1 {}us, t3-t2 {}us, t4-t3 {}us, t5-t4 {}us, t6-t5 {}us ",
                tdiff_us(timer.t2, timer.t1), tdiff_us(timer.t3, timer.t2),
                tdiff_us(timer.t4, timer.t3), tdiff_us(timer.t5, timer.t4),
                tdiff_us(timer.t6, timer.t5));
    } else {
        auto res_cpl = co_await spdk_nvme_zone_append_async(
            ctx->io_thread, ioCtx.ns, ioCtx.qpair, ioCtx.data, ioCtx.offset,
            ioCtx.size, ioCtx.flags);
        if (res_cpl.has_error()) {
            log_error("cpl error status");
            cpl = res_cpl.value();
            // return outcome::failure(std::errc::io_error);
        } else
            cpl = res_cpl.value();
    }

    // TODO handle zone full and open new zone
    // if (spdk_nvme_cpl_is_error(cpl)) {
    //     log_error("I/O error status: {}",
    //               spdk_nvme_cpl_get_status_string(&cpl->status));
    //     if (cpl->status.sc == SPDK_NVME_SC_ZONE_IS_FULL) {
    //         // if zone is full, we finish the current zone and open next zone
    //         log_info(
    //             "Zone is full, finish current zone {} and open next zone {}",
    //             ctx->device->GetZoneId() - 1, ctx->device->GetZoneId());
    //
    //         // seal current zone
    //         auto mgnt_slot =
    //             MakeManagementRequest(ctx->ctrl, ctx->device).value();
    //         co_await zoneFinish(mgnt_slot);
    //         // assert(ret && "zone finish failed");
    //
    //         // bump zone ID to next zone
    //         // ctx->device->OpenNextZone();
    //
    //         // send append request to new zone
    //         auto res_cpl = co_await spdk_nvme_zone_append_async(
    //             ctx->io_thread, ioCtx.ns, ioCtx.qpair, ioCtx.data,
    //             ioCtx.offset + Configuration::GetZoneDist(), ioCtx.size,
    //             ioCtx.flags);
    //         if (res_cpl.has_error()) {
    //             log_error("cpl error status");
    //         } else
    //             cpl = res_cpl.value();
    //     }
    // }

    // TODO: 1. update entry with LBA
    // TODO: 2. what do we return in response
    ctx->write_complete = true;
    ctx->success = true;
    ctx->append_lba = cpl->cdw0;

    ctx->ctrl->mTotalCounts++;
    if (Configuration::Debugging()) {
        log_debug("Total counts: {}", ctx->ctrl->mTotalCounts);
        assert(ctx->ctrl != nullptr);
    }

    // co_return outcome::success();
}

// Zone read operations.
//
// Inst wrapper for zone read provides instrumentation for latency tracking.

void spdk_nvme_zone_read_wrapper(
    struct spdk_thread *thread, struct spdk_nvme_ns *ns,
    struct spdk_nvme_qpair *qpair, void *data, uint64_t offset, uint32_t size,
    uint32_t flags,
    std::move_only_function<void(Result<const spdk_nvme_cpl *>)> cb)
{
    // log_debug("1111: offset {}, size {}", offset, size);
    auto cb_heap = new decltype(cb)(std::move(cb));
    auto fn = new std::move_only_function<void(void)>([=]() {
        int rc = spdk_nvme_ns_cmd_read(
            ns, qpair, data, offset, size,
            [](void *arg, const spdk_nvme_cpl *completion) mutable {
                auto cb3 = reinterpret_cast<decltype(cb_heap)>(arg);
                (*cb3)(completion);
                delete cb3;
            },
            (void *)(cb_heap), flags);
        if (rc != 0) {
            log_debug("{}: cmd read failed offset {}, size {}",
                      spdk_strerror(-rc), offset, size);
            (*cb_heap)(outcome::failure(std::errc::io_error));
            delete cb_heap;
        }
    });
    thread_send_msg(
        thread,
        [](void *fn2) {
            auto rc = reinterpret_cast<decltype(fn)>(fn2);
            (*rc)();
            delete rc;
        },
        fn);
}

auto spdk_nvme_zone_read_async(
    struct spdk_thread *thread, struct spdk_nvme_ns *ns,
    struct spdk_nvme_qpair *qpair, void *data, uint64_t offset, uint32_t size,
    uint32_t flags) -> asio::awaitable<Result<const spdk_nvme_cpl *>>
{
    auto init = [](auto completion_handler, spdk_thread *thread,
                   spdk_nvme_ns *ns, spdk_nvme_qpair *qpair, void *data,
                   uint64_t offset, uint32_t size, uint32_t flags) {
        spdk_nvme_zone_read_wrapper(thread, ns, qpair, data, offset, size,
                                    flags, std::move(completion_handler));
    };

    return asio ::async_initiate<decltype(asio::use_awaitable),
                                 void(Result<const spdk_nvme_cpl *>)>(
        init, asio::use_awaitable, thread, ns, qpair, data, offset, size,
        flags);
}

// Instrumentation for zone read
void spdk_nvme_zone_read_wrapper_inst(
    Timer &timer, struct spdk_thread *thread, struct spdk_nvme_ns *ns,
    struct spdk_nvme_qpair *qpair, void *data, uint64_t offset, uint32_t size,
    uint32_t flags,
    std::move_only_function<void(Result<const spdk_nvme_cpl *>)> cb)
{
    auto cb_heap = new std::pair<
        std::move_only_function<void(Result<const spdk_nvme_cpl *>)>, Timer &>(
        decltype(cb)(std::move(cb)), timer);
    auto fn = new std::move_only_function<void(void)>([=, &timer]() {
        timer.t4 = std::chrono::high_resolution_clock::now();
        int rc = spdk_nvme_ns_cmd_read(
            ns, qpair, data, offset, size,
            [](void *arg, const spdk_nvme_cpl *completion) mutable {
                auto tuple_ = reinterpret_cast<decltype(cb_heap)>(arg);
                auto &cb3 = tuple_->first;
                tuple_->second.t5 = std::chrono::high_resolution_clock::now();
                (cb3)(completion);
                delete tuple_;
            },
            (void *)(cb_heap), flags);
        if (rc != 0) {
            log_debug("{}: cmd read failed offset {}, size {}",
                      spdk_strerror(-rc), offset, size);
            (cb_heap->first)(outcome::failure(std::errc::io_error));
            delete cb_heap;
        }
    });
    thread_send_msg(
        thread,
        [](void *fn2) {
            auto rc = reinterpret_cast<decltype(fn)>(fn2);
            (*rc)();
            delete rc;
        },
        fn);
}

auto spdk_nvme_zone_read_async_inst(
    Timer &timer, struct spdk_thread *thread, struct spdk_nvme_ns *ns,
    struct spdk_nvme_qpair *qpair, void *data, uint64_t offset, uint32_t size,
    uint32_t flags) -> asio::awaitable<Result<const spdk_nvme_cpl *>>
{
    timer.t2 = std::chrono::high_resolution_clock::now();
    auto init = [](auto completion_handler, Timer *timer, spdk_thread *thread,
                   spdk_nvme_ns *ns, spdk_nvme_qpair *qpair, void *data,
                   uint64_t offset, uint32_t size, uint32_t flags) {
        timer->t3 = std::chrono::high_resolution_clock::now();
        spdk_nvme_zone_read_wrapper_inst(*timer, thread, ns, qpair, data,
                                         offset, size, flags,
                                         std::move(completion_handler));
    };
    return asio::async_initiate<decltype(asio::use_awaitable),
                                void(Result<const spdk_nvme_cpl *>)>(
        init, asio::use_awaitable, &timer, thread, ns, qpair, data, offset,
        size, flags);
}

auto zoneRead(void *arg1) -> asio::awaitable<void>
{
    RequestContext *ctx = reinterpret_cast<RequestContext *>(arg1);
    auto ioCtx = ctx->ioContext;
    assert(ctx->ctrl != nullptr);

    Timer timer;
    const spdk_nvme_cpl *cpl;
    if (Configuration::GetSamplingRate() > 0) {
        timer.t1 = std::chrono::high_resolution_clock::now();
        auto res_cpl = co_await spdk_nvme_zone_read_async_inst(
            timer, ctx->io_thread, ioCtx.ns, ioCtx.qpair, ioCtx.data,
            ioCtx.offset, ioCtx.size, ioCtx.flags);
        cpl = res_cpl.value();
        timer.t6 = std::chrono::high_resolution_clock::now();

        if (ctx->ctrl->mTotalCounts % Configuration::GetSamplingRate() == 0 ||
            tdiff_us(timer.t6, timer.t1) > 1000)
            log_debug(
                "t2-t1 {}us, t3-t2 {}us, t4-t3 {}us, t5-t4 {}us, t6-t5 {}us ",
                tdiff_us(timer.t2, timer.t1), tdiff_us(timer.t3, timer.t2),
                tdiff_us(timer.t4, timer.t3), tdiff_us(timer.t5, timer.t4),
                tdiff_us(timer.t6, timer.t5));
    } else {
        auto res_cpl = co_await spdk_nvme_zone_read_async(
            ctx->io_thread, ioCtx.ns, ioCtx.qpair, ioCtx.data, ioCtx.offset,
            ioCtx.size, ioCtx.flags);
        // if (res_cpl.has_error()) {
        //     // log_error("cpl error status");
        //     co_return outcome::failure(std::errc::io_error);
        // } else
        //     cpl = res_cpl.value();
    }
    // if (spdk_nvme_cpl_is_error(cpl)) {
    //     // log_error("I/O error status: {}",
    //     //           spdk_nvme_cpl_get_status_string(&cpl->status));
    //     // log_debug("Unimplemented: put context back in pool");
    //     co_return outcome::failure(std::errc::io_error);
    // }

    // For read, we swap the read date into the request body
    std::string body(static_cast<char *>(ioCtx.data),
                     ioCtx.size * Configuration::GetBlockSize());
    ctx->response_body = body;
    ctx->success = true;

    ctx->ctrl->mTotalCounts++;
    assert(ctx->ctrl != nullptr);

    // co_return outcome::success();
}

void thread_send_msg(spdk_thread *thread, spdk_msg_fn fn, void *args)
{
    if (spdk_thread_send_msg(thread, fn, args) < 0) {
        log_error("Thread send message failed: thread_name {}.",
                  spdk_thread_get_name(thread));
        exit(-1);
    }
}

void RequestContext::Clear()
{
    // TODO: double check clear is fully done
    available = true;
    // successBytes = 0;
    // targetBytes = 0;
    lba = 0;
    // cb_fn = nullptr;
    cb_args = nullptr;
    // curOffset = 0;
    ioOffset = 0;
    success = false;
    response_body = "";

    timestamp = ~0ull;

    // associatedRequests.clear(); // = nullptr;
    // associatedStripe = nullptr;
    // associatedRead = nullptr;

    // successBytes = 0;
    // targetBytes = 0;
}

// double RequestContext::GetElapsedTime() { return ctime - stime; }

// void RequestContext::Queue()
// {
//     // struct spdk_thread *th = ctrl->GetDispatchThread();
//     // thread_send_msg(ctrl->GetDispatchThread(), handleEventCompletion,
//     this);
//     // } else {
//     //     event_call(Configuration::GetDispatchThreadCoreId(),
//     //                handleEventCompletion2, this, nullptr);
//     // }
// }

// void RequestContext::PrintStats()
// {
//     // printf("RequestStats: %d %d %lu %d, iocontext: %p %p %lu %d\n", type,
//     //        status, lba, size, ioContext.data, ioContext.metadata,
//     //        ioContext.offset, ioContext.size);
// }

// void RequestContext::CopyFrom(const RequestContext &o)
// {
//     // type = o.type;
//     // status = o.status;
//
//     lba = o.lba;
//     size = o.size;
//     req_type = o.req_type;
//     data = o.data;
//     // meta = o.meta;
//     // pbaArray = o.pbaArray;
//     successBytes = o.successBytes;
//     targetBytes = o.targetBytes;
//     curOffset = o.curOffset;
//     // ioOffset = o.ioOffset;
//     // cb_fn = o.cb_fn;
//     cb_args = o.cb_args;
//
//     available = o.available;
//
//     ctrl = o.ctrl;
//     zoneId = o.zoneId;
//     offset = o.offset;
//     // append = o.append;
//
//     stime = o.stime;
//     ctime = o.ctime;
//
//     // associatedRequests = o.associatedRequests;
//     // associatedStripe = o.associatedStripe;
//     // associatedRead = o.associatedRead;
//
//     // needDegradedRead = o.needDegradedRead;
//     // needDegradedRead = o.needDecodeMeta;
//
//     ioContext = o.ioContext;
//     // gcTask = o.gcTask;
// }

RequestContextPool::RequestContextPool(uint32_t cap)
{
    capacity = cap;
    contexts = new RequestContext[capacity];
    for (uint32_t i = 0; i < capacity; ++i) {
        contexts[i].Clear();
        contexts[i].dataBuffer =
            (uint8_t *)spdk_zmalloc(Configuration::GetObjectSizeInBytes(),
                                    Configuration::GetBlockSize(), NULL,
                                    SPDK_ENV_SOCKET_ID_ANY, SPDK_MALLOC_DMA);
        // contexts[i].metadataBuffer =
        //     (uint8_t *)spdk_zmalloc(Configuration::GetMaxStripeUnitSize() /
        //                                 Configuration::GetBlockSize() *
        //                                 Configuration::GetMetadataSize(),
        //                             Configuration::GetBlockSize(), NULL,
        //                             SPDK_ENV_SOCKET_ID_ANY, SPDK_MALLOC_DMA);
        contexts[i].bufferSize = Configuration::GetObjectSizeInBytes();
        availableContexts.emplace_back(&contexts[i]);
    }
}

RequestContext *RequestContextPool::GetRequestContext(bool force)
{
    std::unique_lock lock(g_shared_mutex_);

    RequestContext *ctx = nullptr;
    if (availableContexts.empty() && force == false) {
        ctx = nullptr;
    } else {
        if (!availableContexts.empty()) {
            ctx = availableContexts.back();
            availableContexts.pop_back();
            ctx->Clear();
            ctx->available = false;
        } else {
            ctx = new RequestContext();
            ctx->dataBuffer = (uint8_t *)spdk_zmalloc(
                Configuration::GetObjectSizeInBytes(),
                Configuration::GetBlockSize(), NULL, SPDK_ENV_SOCKET_ID_ANY,
                SPDK_MALLOC_DMA);
            // ctx->metadataBuffer = (uint8_t *)spdk_zmalloc(
            //     Configuration::GetMaxStripeUnitSize() /
            //         Configuration::GetBlockSize() *
            //         Configuration::GetMetadataSize(),
            //     Configuration::GetBlockSize(), NULL, SPDK_ENV_SOCKET_ID_ANY,
            //     SPDK_MALLOC_DMA);
            ctx->bufferSize = Configuration::GetObjectSizeInBytes();
            ctx->Clear();
            ctx->available = false;
            exit(1);
        }
    }
    return ctx;
}

void RequestContextPool::ReturnRequestContext(RequestContext *slot)
{
    std::unique_lock lock(g_shared_mutex_);

    assert(slot->available);
    if (slot < contexts || slot >= contexts + capacity) {
        // test whether the returned slot is pre-allocated
        spdk_free(slot->dataBuffer);
        delete slot;
    } else {
        assert(availableContexts.size() <= capacity);
        availableContexts.emplace_back(slot);
    }
}

Result<MapEntry> createMapEntry(DevTuple tuple, u64 lba1, u32 len1, u64 lba2,
                                u32 len2, u64 lba3, u32 len3)
{
    auto [tgt1, tgt2, tgt3] = tuple;
    MapEntry entry = std::make_tuple(std::make_tuple(tgt1.first, lba1, len1),
                                     std::make_tuple(tgt2.first, lba2, len2),
                                     std::make_tuple(tgt3.first, lba3, len3));
    return entry;
}

Result<RequestContext *> MakeReadRequest(ZstoreController *zctrl_, Device *dev,
                                         uint64_t offset)
{
    RequestContext *slot = zctrl_->mRequestContextPool->GetRequestContext(true);
    slot->ctrl = zctrl_;

    auto ioCtx = slot->ioContext;
    ioCtx.ns = dev->GetNamespace();
    ioCtx.qpair = dev->GetIoQueue(0);
    ioCtx.data = slot->dataBuffer;
    ioCtx.offset = Configuration::GetZoneDist() * dev->GetZoneId() + offset;
    ioCtx.size =
        Configuration::GetObjectSizeInBytes() / Configuration::GetBlockSize();
    ioCtx.flags = 0;
    slot->ioContext = ioCtx;

    slot->io_thread = zctrl_->GetIoThread(0);
    slot->device = dev;
    slot->is_write = false;

    return slot;
}

Result<RequestContext *> MakeWriteRequest(ZstoreController *zctrl_, Device *dev,
                                          std::vector<u8> data)
{
    RequestContext *slot = zctrl_->mRequestContextPool->GetRequestContext(true);
    slot->ctrl = zctrl_;
    slot->dataBuffer = data.data();

    auto ioCtx = slot->ioContext;
    ioCtx.ns = dev->GetNamespace();
    ioCtx.qpair = dev->GetIoQueue(0);
    ioCtx.data = slot->dataBuffer;
    ioCtx.offset = Configuration::GetZoneDist() * dev->GetZoneId();
    ioCtx.size =
        Configuration::GetObjectSizeInBytes() / Configuration::GetBlockSize();
    ioCtx.flags = 0;
    slot->ioContext = ioCtx;

    slot->io_thread = zctrl_->GetIoThread(0);
    slot->device = dev;
    slot->is_write = true;

    return slot;
}

Result<RequestContext *> MakeManagementRequest(ZstoreController *zctrl_,
                                               Device *dev)
{
    RequestContext *slot = zctrl_->mRequestContextPool->GetRequestContext(true);
    slot->ctrl = zctrl_;

    auto ioCtx = slot->ioContext;
    ioCtx.ns = dev->GetNamespace();
    ioCtx.qpair = dev->GetIoQueue(0);
    ioCtx.offset = Configuration::GetZoneDist() * dev->GetZoneId();
    ioCtx.flags = 0;
    slot->ioContext = ioCtx;

    slot->io_thread = zctrl_->GetIoThread(0);
    slot->device = dev;

    return slot;
}
