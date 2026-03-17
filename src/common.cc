#include "include/common.h"
#include "include/configuration.h"
#include "include/zstore_controller.h"
#include "spdk/nvme_zns.h"
#include "spdk/string.h"
#include <cassert>

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
    std::move_only_function<void(const spdk_nvme_cpl *)> cb)
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
        assert(rc == 0);
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

auto spdk_nvme_zone_finish_async(struct spdk_thread *thread,
                                 struct spdk_nvme_ns *ns,
                                 struct spdk_nvme_qpair *qpair, uint64_t offset,
                                 uint32_t flags)
    -> asio::awaitable<const spdk_nvme_cpl *>
{
    auto init = [](auto completion_handler, spdk_thread *thread,
                   spdk_nvme_ns *ns, spdk_nvme_qpair *qpair, uint64_t offset,
                   uint32_t flags) {
        spdk_nvme_zone_finish_wrapper(thread, ns, qpair, offset, flags,
                                      std::move(completion_handler));
    };

    return asio::async_initiate<decltype(asio::use_awaitable),
                                void(const spdk_nvme_cpl *)>(
        init, asio::use_awaitable, thread, ns, qpair, offset, flags);
}

// Instrumentation for zone finish
void spdk_nvme_zone_finish_wrapper_inst(
    Timer &timer, struct spdk_thread *thread, struct spdk_nvme_ns *ns,
    struct spdk_nvme_qpair *qpair, uint64_t offset, uint32_t flags,
    std::move_only_function<void(const spdk_nvme_cpl *)> cb)
{
    auto cb_heap =
        new std::pair<std::move_only_function<void(const spdk_nvme_cpl *)>,
                      Timer &>(decltype(cb)(std::move(cb)), timer);
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
        assert(rc == 0);
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

auto spdk_nvme_zone_finish_async_inst(Timer &timer, struct spdk_thread *thread,
                                      struct spdk_nvme_ns *ns,
                                      struct spdk_nvme_qpair *qpair,
                                      uint64_t offset, uint32_t flags)
    -> asio::awaitable<const spdk_nvme_cpl *>
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
                                void(const spdk_nvme_cpl *)>(
        init, asio::use_awaitable, &timer, thread, ns, qpair, offset, flags);
}

auto zoneFinish(void *arg1) -> asio::awaitable<void>
{
    RequestContext *ctx = reinterpret_cast<RequestContext *>(arg1);
    auto ioCtx = ctx->ioContext;
    assert(ctx->ctrl != nullptr);

    Timer timer;
    const spdk_nvme_cpl *cpl;
    if (Conf::GetSamplingRate() > 0) {
        timer.t1 = std::chrono::high_resolution_clock::now();
        cpl = co_await spdk_nvme_zone_finish_async_inst(
            timer, ctx->io_thread, ioCtx.ns, ioCtx.qpair, ioCtx.offset,
            ioCtx.flags);
        timer.t6 = std::chrono::high_resolution_clock::now();

        if (ctx->ctrl->mTotalCounts % Conf::GetSamplingRate() == 0 ||
            tdiff_us(timer.t6, timer.t1) > 1000)
            log_debug(
                "t2-t1 {}us, t3-t2 {}us, t4-t3 {}us, t5-t4 {}us, t6-t5 {}us ",
                tdiff_us(timer.t2, timer.t1), tdiff_us(timer.t3, timer.t2),
                tdiff_us(timer.t4, timer.t3), tdiff_us(timer.t5, timer.t4),
                tdiff_us(timer.t6, timer.t5));
    } else {
        cpl = co_await spdk_nvme_zone_finish_async(
            ctx->io_thread, ioCtx.ns, ioCtx.qpair, ioCtx.offset, ioCtx.flags);
    }
    if (spdk_nvme_cpl_is_error(cpl)) {
        log_error("I/O error status: {}",
                  spdk_nvme_cpl_get_status_string(&cpl->status));
        // log_debug("Unimplemented: put context back in pool");
    }

    ctx->success = true;

    ctx->ctrl->mManagementCounts++;
    assert(ctx->ctrl != nullptr);
}

// Zone append operations.
//
// Inst wrapper for zone append provides instrumentation for latency tracking.

void spdk_nvme_zone_append_wrapper(
    struct spdk_thread *thread, struct spdk_nvme_ns *ns,
    struct spdk_nvme_qpair *qpair, void *data, uint64_t offset, uint32_t size,
    uint32_t flags, std::move_only_function<void(const spdk_nvme_cpl *)> cb)
{
    if (Conf::IsDebugging())
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

        if (rc != 0)
            log_error("zone append failed {}", rc);
        assert(rc == 0);
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

auto spdk_nvme_zone_append_async(struct spdk_thread *thread,
                                 struct spdk_nvme_ns *ns,
                                 struct spdk_nvme_qpair *qpair, void *data,
                                 uint64_t offset, uint32_t size, uint32_t flags)
    -> asio::awaitable<const spdk_nvme_cpl *>
{
    auto init = [](auto completion_handler, spdk_thread *thread,
                   spdk_nvme_ns *ns, spdk_nvme_qpair *qpair, void *data,
                   uint64_t offset, uint32_t size, uint32_t flags) {
        spdk_nvme_zone_append_wrapper(thread, ns, qpair, data, offset, size,
                                      flags, std::move(completion_handler));
    };
    return asio::async_initiate<decltype(asio ::use_awaitable),
                                void(const spdk_nvme_cpl *)>(
        init, asio::use_awaitable, thread, ns, qpair, data, offset, size,
        flags);
}

// Instrumentation for zone append
void spdk_nvme_zone_append_wrapper_inst(
    Timer &timer, struct spdk_thread *thread, struct spdk_nvme_ns *ns,
    struct spdk_nvme_qpair *qpair, void *data, uint64_t offset, uint32_t size,
    uint32_t flags, std::move_only_function<void(const spdk_nvme_cpl *)> cb)
{
    auto cb_heap =
        new std::pair<std::move_only_function<void(const spdk_nvme_cpl *)>,
                      Timer &>(decltype(cb)(std::move(cb)), timer);
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
        assert(rc == 0);
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

auto spdk_nvme_zone_append_async_inst(Timer &timer, struct spdk_thread *thread,
                                      struct spdk_nvme_ns *ns,
                                      struct spdk_nvme_qpair *qpair, void *data,
                                      uint64_t offset, uint32_t size,
                                      uint32_t flags)
    -> asio::awaitable<const spdk_nvme_cpl *>
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
                                void(const spdk_nvme_cpl *)>(
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
    if (Conf::GetSamplingRate() > 0) {
        timer.t1 = std::chrono::high_resolution_clock::now();
        cpl = co_await spdk_nvme_zone_append_async_inst(
            timer, ctx->io_thread, ioCtx.ns, ioCtx.qpair, ioCtx.data,
            ioCtx.offset, ioCtx.size, ioCtx.flags);
        timer.t6 = std::chrono::high_resolution_clock::now();

        if (ctx->ctrl->mTotalCounts % Conf::GetSamplingRate() == 0)
            log_debug(
                "t2-t1 {}us, t3-t2 {}us, t4-t3 {}us, t5-t4 {}us, t6-t5 {}us ",
                tdiff_us(timer.t2, timer.t1), tdiff_us(timer.t3, timer.t2),
                tdiff_us(timer.t4, timer.t3), tdiff_us(timer.t5, timer.t4),
                tdiff_us(timer.t6, timer.t5));
    } else {
        cpl = co_await spdk_nvme_zone_append_async(
            ctx->io_thread, ioCtx.ns, ioCtx.qpair, ioCtx.data, ioCtx.offset,
            ioCtx.size, ioCtx.flags);
    }

    // TODO handle zone full and open new zone
    if (spdk_nvme_cpl_is_error(cpl)) {
        log_error("I/O error status: {}",
                  spdk_nvme_cpl_get_status_string(&cpl->status));
        //     if (cpl->status.sc == SPDK_NVME_SC_ZONE_IS_FULL) {
        //         // if zone is full, we finish the current zone and open next
        //         zone log_info(
        //             "Zone is full, finish current zone {} and open next zone
        //             {}", ctx->device->GetZoneId() - 1,
        //             ctx->device->GetZoneId());
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
        //             ioCtx.offset + Conf::GetZoneDist(), ioCtx.size,
        //             ioCtx.flags);
        //         if (res_cpl.has_error()) {
        //             log_error("cpl error status");
        //         } else
        //             cpl = res_cpl.value();
        //     }
    }

    // TODO: 1. update entry with LBA
    // TODO: 2. what do we return in response
    ctx->write_complete = true;
    ctx->success = true;
    ctx->append_lba = cpl->cdw0;

    ctx->ctrl->mTotalCounts++;
    if (Conf::IsDebugging()) {
        log_debug("Total counts: {}", ctx->ctrl->mTotalCounts);
        assert(ctx->ctrl != nullptr);
    }
}

// Zone read operations.
//
// Inst wrapper for zone read provides instrumentation for latency tracking.

void spdk_nvme_zone_read_wrapper(
    struct spdk_thread *thread, struct spdk_nvme_ns *ns,
    struct spdk_nvme_qpair *qpair, void *data, uint64_t offset, uint32_t size,
    uint32_t flags, std::move_only_function<void(const spdk_nvme_cpl *)> cb)
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
        if (rc != 0)
            log_error("zone read failed {}", rc);
        assert(rc == 0);
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

auto spdk_nvme_zone_read_async(struct spdk_thread *thread,
                               struct spdk_nvme_ns *ns,
                               struct spdk_nvme_qpair *qpair, void *data,
                               uint64_t offset, uint32_t size, uint32_t flags)
    -> asio::awaitable<const spdk_nvme_cpl *>
{
    auto init = [](auto completion_handler, spdk_thread *thread,
                   spdk_nvme_ns *ns, spdk_nvme_qpair *qpair, void *data,
                   uint64_t offset, uint32_t size, uint32_t flags) {
        spdk_nvme_zone_read_wrapper(thread, ns, qpair, data, offset, size,
                                    flags, std::move(completion_handler));
    };

    return asio ::async_initiate<decltype(asio::use_awaitable),
                                 void(const spdk_nvme_cpl *)>(
        init, asio::use_awaitable, thread, ns, qpair, data, offset, size,
        flags);
}

// Instrumentation for zone read
void spdk_nvme_zone_read_wrapper_inst(
    Timer &timer, struct spdk_thread *thread, struct spdk_nvme_ns *ns,
    struct spdk_nvme_qpair *qpair, void *data, uint64_t offset, uint32_t size,
    uint32_t flags, std::move_only_function<void(const spdk_nvme_cpl *)> cb)
{
    auto cb_heap =
        new std::pair<std::move_only_function<void(const spdk_nvme_cpl *)>,
                      Timer &>(decltype(cb)(std::move(cb)), timer);
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
        assert(rc == 0);
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

auto spdk_nvme_zone_read_async_inst(Timer &timer, struct spdk_thread *thread,
                                    struct spdk_nvme_ns *ns,
                                    struct spdk_nvme_qpair *qpair, void *data,
                                    uint64_t offset, uint32_t size,
                                    uint32_t flags)
    -> asio::awaitable<const spdk_nvme_cpl *>
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
                                void(const spdk_nvme_cpl *)>(
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
    if (Conf::GetSamplingRate() > 0) {
        timer.t1 = std::chrono::high_resolution_clock::now();
        cpl = co_await spdk_nvme_zone_read_async_inst(
            timer, ctx->io_thread, ioCtx.ns, ioCtx.qpair, ioCtx.data,
            ioCtx.offset, ioCtx.size, ioCtx.flags);
        timer.t6 = std::chrono::high_resolution_clock::now();

        if (ctx->ctrl->mTotalCounts % Conf::GetSamplingRate() == 0 ||
            tdiff_us(timer.t6, timer.t1) > 1000)
            log_debug(
                "t2-t1 {}us, t3-t2 {}us, t4-t3 {}us, t5-t4 {}us, t6-t5 {}us ",
                tdiff_us(timer.t2, timer.t1), tdiff_us(timer.t3, timer.t2),
                tdiff_us(timer.t4, timer.t3), tdiff_us(timer.t5, timer.t4),
                tdiff_us(timer.t6, timer.t5));
    } else {
        cpl = co_await spdk_nvme_zone_read_async(
            ctx->io_thread, ioCtx.ns, ioCtx.qpair, ioCtx.data, ioCtx.offset,
            ioCtx.size, ioCtx.flags);
        // log_debug("2222: offset {}, size {}", ioCtx.offset, ioCtx.size);
    }
    if (spdk_nvme_cpl_is_error(cpl)) {
        log_error("I/O error status: {}",
                  spdk_nvme_cpl_get_status_string(&cpl->status));
        // log_debug("Unimplemented: put context back in pool");
    }

    // For read, we swap the read date into the request body
    // ioCtx.data and ctx->dataBuffer are the same and has no cost
    // but constructing it std::string has cost
    ctx->response_body.assign(ctx->dataBuffer, ctx->bufferSize);

    ctx->success = true;
    ctx->ctrl->mTotalCounts++;
    assert(ctx->ctrl != nullptr);
}

// Dummy op: only used to test performance of async operations
void spdk_op_wrapper(struct spdk_thread *thread, struct spdk_nvme_ns *ns,
                     struct spdk_nvme_qpair *qpair, void *data, uint64_t offset,
                     uint32_t size, uint32_t flags,
                     std::move_only_function<void(const spdk_nvme_cpl *)> cb)
{
    auto cb_heap = new decltype(cb)(std::move(cb));
    auto fn = new std::move_only_function<void(void)>([=]() {
        auto f = [](void *arg, const spdk_nvme_cpl *completion) mutable {
            auto cb3 = reinterpret_cast<decltype(cb_heap)>(arg);
            (*cb3)(completion);
            delete cb3;
        };

        f((void *)(cb_heap), nullptr);
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

auto spdk_op_async(struct spdk_thread *thread, struct spdk_nvme_ns *ns,
                   struct spdk_nvme_qpair *qpair, void *data, uint64_t offset,
                   uint32_t size, uint32_t flags)
    -> asio::awaitable<const spdk_nvme_cpl *>
{
    auto init = [](auto completion_handler, spdk_thread *thread,
                   spdk_nvme_ns *ns, spdk_nvme_qpair *qpair, void *data,
                   uint64_t offset, uint32_t size, uint32_t flags) {
        spdk_op_wrapper(thread, ns, qpair, data, offset, size, flags,
                        std::move(completion_handler));
    };

    return asio ::async_initiate<decltype(asio::use_awaitable),
                                 void(const spdk_nvme_cpl *)>(
        init, asio::use_awaitable, thread, ns, qpair, data, offset, size,
        flags);
}

auto zoneReadDummy(void *arg1) -> asio::awaitable<void>
{
    RequestContext *ctx = reinterpret_cast<RequestContext *>(arg1);
    auto ioCtx = ctx->ioContext;
    assert(ctx->ctrl != nullptr);

    auto cpl = co_await spdk_op_async(ctx->io_thread, ioCtx.ns, ioCtx.qpair,
                                      ioCtx.data, ioCtx.offset, ioCtx.size,
                                      ioCtx.flags);

    // For read, we swap the read date into the request body
    // ioCtx.data and ctx->dataBuffer are the same and has no cost
    // but constructing it std::string has cost
    ctx->response_body.assign(ctx->dataBuffer, ctx->bufferSize);

    ctx->success = true;
    ctx->ctrl->mTotalCounts++;
    assert(ctx->ctrl != nullptr);
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
    available = true;
    success = false;
    cb_args = nullptr;
    response_body = "";
    timestamp = ~0ull;
}

RequestContextPool::RequestContextPool(uint32_t cap)
{
    // Set buffer size according to object size and MDTS
    u64 buffer_size = 0;
    if (Conf::GetObjectSizeInBytes() > Conf::GetChunkSize()) {
        buffer_size = Conf::GetChunkSize();
    } else {
        buffer_size = Conf::GetObjectSizeInBytes();
    }
    capacity = cap;
    contexts = new RequestContext[capacity];
    for (uint32_t i = 0; i < capacity; ++i) {
        contexts[i].Clear();
        contexts[i].bufferSize = buffer_size;
        contexts[i].dataBuffer =
            (char *)spdk_zmalloc(buffer_size, Conf::GetBlockSize(), NULL,
                                 SPDK_ENV_SOCKET_ID_ANY, SPDK_MALLOC_DMA);
        // TODO [design]  we need to pre-allocate the buffer for each request
        // somewhere so it is done here, maybe this can be moved somewhere
        // else
        contexts[i].response_body.reserve(buffer_size);
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
            // Set buffer size according to object size and MDTS
            u64 buffer_size = 0;
            if (Conf::GetObjectSizeInBytes() > Conf::GetChunkSize()) {
                buffer_size = Conf::GetChunkSize();
            } else {
                buffer_size = Conf::GetObjectSizeInBytes();
            }

            ctx = new RequestContext();
            ctx->bufferSize = buffer_size;
            ctx->dataBuffer =
                (char *)spdk_zmalloc(buffer_size, Conf::GetBlockSize(), NULL,
                                     SPDK_ENV_SOCKET_ID_ANY, SPDK_MALLOC_DMA);
            ctx->response_body.reserve(buffer_size);
            ctx->Clear();
            ctx->available = false;
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

Result<MapEntry> createMapEntry(DevTuple tuple, Lba lba1, Lba lba2, Lba lba3,
                                Length len)
{
    // tgt1: device id, zone id
    auto [tgt1, tgt2, tgt3] = tuple;
    Location loc1{static_cast<int>(tgt1.first), lba1};
    Location loc2{static_cast<int>(tgt2.first), lba2};
    Location loc3{static_cast<int>(tgt3.first), lba3};
    MapEntry entry{
        .locations = {loc1, loc2, loc3},
        .len = len,
        .tombstoned = false,
    };

    return entry;
}

Result<RequestContext *> MakeReadRequest(ZstoreController *zctrl_, Device *dev,
                                         uint64_t offset, Length len_in_bytes)
{
    auto len_in_blocks = len_in_bytes / Conf::GetBlockSize();
    RequestContext *slot;
    if (Conf::IsReplay()) {
        slot =
            zctrl_->mReplayRequestContextPool[len_in_blocks]->GetRequestContext(
                true);
    } else {
        slot = zctrl_->mRequestContextPool->GetRequestContext(true);
    }
    slot->ctrl = zctrl_;

    auto ioCtx = slot->ioContext;
    ioCtx.ns = dev->GetNamespace();
    ioCtx.qpair = dev->GetIoQueue(0);
    ioCtx.data = slot->dataBuffer;
    ioCtx.offset = Conf::GetZoneDist() * dev->GetZoneId() + offset;
    ioCtx.size = len_in_blocks;
    ioCtx.flags = 0;
    slot->ioContext = ioCtx;

    if (Conf::IsDebugging()) {
        log_debug("Size: {}", ioCtx.size);
        log_debug("Check qpair connected: {}",
                  spdk_nvme_qpair_is_connected(dev->GetIoQueue(0)));

        auto thread = zctrl_->GetIoThread(0);
        log_debug("Check Io thread {}: running {}, idle {}, exited {}",
                  dev->GetDeviceId(), spdk_thread_is_running(thread),
                  spdk_thread_is_idle(thread), spdk_thread_is_exited(thread));
    }

    slot->io_thread = zctrl_->GetIoThread(0);
    slot->device = dev;
    slot->is_write = false;

    return slot;
}

Result<RequestContext *> MakeWriteRequest(ZstoreController *zctrl_, Device *dev,
                                          std::vector<char> *buffer,
                                          Length len_in_bytes)
{
    auto len_in_blocks = len_in_bytes / Conf::GetBlockSize();
    RequestContext *slot;
    if (Conf::IsReplay()) {
        slot =
            zctrl_->mReplayRequestContextPool[len_in_blocks]->GetRequestContext(
                true);
    } else {
        slot = zctrl_->mRequestContextPool->GetRequestContext(true);
    }
    slot->ctrl = zctrl_;
    std::memcpy(slot->dataBuffer, buffer, sizeof(buffer));

    auto ioCtx = slot->ioContext;
    ioCtx.ns = dev->GetNamespace();
    ioCtx.qpair = dev->GetIoQueue(0);
    ioCtx.data = slot->dataBuffer;
    ioCtx.offset = Conf::GetZoneDist() * dev->GetZoneId();
    ioCtx.size = slot->bufferSize / Conf::GetBlockSize();
    assert(ioCtx.size == len_in_blocks);
    ioCtx.flags = 0;
    slot->ioContext = ioCtx;

    slot->io_thread = zctrl_->GetIoThread(0);
    slot->device = dev;
    slot->is_write = true;

    return slot;
}

Result<RequestContext *> MakeWriteChunk(ZstoreController *zctrl_, Device *dev,
                                        char *buffer, Length len_in_bytes)
{
    auto len_in_blocks = len_in_bytes / Conf::GetBlockSize();
    RequestContext *slot;
    if (Conf::IsReplay()) {
        slot =
            zctrl_->mReplayRequestContextPool[len_in_blocks]->GetRequestContext(
                true);
    } else {
        slot = zctrl_->mRequestContextPool->GetRequestContext(true);
    }
    slot->ctrl = zctrl_;
    // slot->dataBuffer = buffer;
    std::memcpy(slot->dataBuffer, buffer, slot->bufferSize);

    auto ioCtx = slot->ioContext;
    ioCtx.ns = dev->GetNamespace();
    ioCtx.qpair = dev->GetIoQueue(0);
    ioCtx.data = slot->dataBuffer;
    ioCtx.offset = Conf::GetZoneDist() * dev->GetZoneId();
    ioCtx.size = slot->bufferSize / Conf::GetBlockSize();
    assert(ioCtx.size == len_in_blocks);
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
    ioCtx.offset = Conf::GetZoneDist() * dev->GetZoneId();
    ioCtx.flags = 0;
    slot->ioContext = ioCtx;

    slot->io_thread = zctrl_->GetIoThread(0);
    slot->device = dev;

    return slot;
}
