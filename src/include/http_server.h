#pragma once
#include "boost_utils.h"
#include "common.h"
#include "types.h"
#include "zstore_controller.h"
#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/version.hpp>
#include <boost/config.hpp>
#include <fmt/core.h>
#include <functional>
#include <iostream>

using namespace boost::asio::experimental::awaitable_operators;
using HttpMsg = http::message_generator;

namespace http = boost::beast::http; // from <boost/beast/http.hpp>
namespace net = boost::asio;         // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;    // from <boost/asio/ip/tcp.hpp>

using tcp_stream = typename boost::beast::tcp_stream::rebind_executor<
    net::use_awaitable_t<>::executor_with_default<net::any_io_executor>>::other;

//------------------------------------------------------------------------------

//         if (req_.method() == http::verb::get) {
//             // NOTE: READ path: see section 3.4
//             auto object_key = req_.target();
//
//             if (zctrl_.SearchBF(object_key).value()) {
//                 log_info("Object {} is recently modified", object_key);
//                 log_error("Unimplemented!!!");
//             }
//
// /
// }
//         } else if (req_.method() == http::verb::post ||
//                    req_.method() == http::verb::put) {
//             // NOTE: Write path: see section 3.3
//
//             auto object_key = req_.target();
//             auto object_value = req_.body();
//             // log_debug("key {}, value {}", req_.target(), req_.body());
//
//             // TODO: populate the map with consistent hashes
//             auto dev_tuple = zctrl_.GetDevTuple(object_key).value();
//             auto entry = zctrl_.CreateObject(object_key, dev_tuple);
//             assert(entry.has_value());
//
//             if (!zctrl_.isDraining &&
//                 zctrl_.mRequestContextPool->availableContexts.size() > 0)
//                 { if (!zctrl_.start) {
//                     zctrl_.start = true;
//                     zctrl_.stime =
//                     std::chrono::high_resolution_clock::now();
//                 }
//                 // if (zctrl_.verbose)
//                 // log_debug("222");
//
//                 auto closure_ = [this, self = shared_from_this()](
//                                     HttpRequest req_, MapEntry entry) {
//                     auto object_key = req_.target();
//
//                     // update lba in map
//                     auto rc = zctrl_.PutObject(object_key,
//                     entry).value();
//                     // FIXME:we need to handle the failure
//                     // assert(rc == true);
//                     // if (rc == false)
//                     //     log_debug("Inserting object {} failed",
//                     object_key);
//
//                     // update and broadcast BF
//                     auto rc2 = zctrl_.UpdateBF(object_key);
//                     assert(rc2.has_value());
//
//                     // send ack back to client
//                     http::message_generator msg =
//                         handle_request(std::move(req_));
//                     bool keep_alive = msg.keep_alive();
//                     beast::async_write(stream_, std::move(msg),
//                                        beast::bind_front_handler(
//                                            &session::on_write,
//                                            shared_from_this(),
//                                            keep_alive));
//                 };
//
//                 auto slot = MakeWriteRequest(
//                     &zctrl_, zctrl_.GetDevice(entry.value().first_tgt()),
//                     req_, entry.value(), closure_);
//
//                 assert(slot.has_value());
//                 {
//                     std::unique_lock lock(zctrl_.GetRequestQueueMutex());
//                     zctrl_.EnqueueWrite(slot.value());
//                 }
//                 if (zctrl_.verbose)
//                     log_debug("666");
//             }
//         } else {
//             log_error("Request is not a supported operation\n");
//         }
//     }

/* This function implements the core logic of async
 */
auto awaitable_on_request(HttpRequest req,
                          ZstoreController &zctrl_) -> net::awaitable<HttpMsg>
{
    // Handle the request
    if (req.method() == http::verb::get) {
        // NOTE: READ path: see section 3.4
        auto object_key = req.target();

        MapEntry entry;
        auto rc = zctrl_.GetObject(object_key, entry).value();
        assert(rc == true);

        // if (!zctrl_.isDraining &&
        //     zctrl_.mRequestContextPool->availableContexts.size() > 0) {
        //     if (!zctrl_.start) {
        //         zctrl_.start = true;
        //         zctrl_.stime = std::chrono::high_resolution_clock::now();
        //     }
        //
        // entry.first_tgt());
        auto dev = zctrl_.GetDevice(entry.first_tgt());
        // log_debug("111 ");
        //
        auto slot =
            MakeReadRequest(&zctrl_, dev, entry.first_lba(), req).value();
        //     {
        //         // std::unique_lock lock(zctrl_.GetRequestQueueMutex());
        //         // co_await zctrl_.EnqueueRead(slot.value());

        // co_await zoneRead(slot);

        //     }
        //
        //     // TODO: this should move to reclaim context and in controller
        //     // ctx->available = true;
        slot->Clear();
        zctrl_.mRequestContextPool->ReturnRequestContext(slot);
        //
        //     // co_return co_await async_on_request(std::move(req),
        //     //                                     net::use_awaitable);
        // }
        co_return handle_request(std::move(req));

    } else {
        // log_debug("HTTP method is not implemented yet!!!");
        // HttpMsg msg = handle_request(std::move(req));
        // co_return msg;
        co_return handle_request(std::move(req));
    }

    // co_return co_await async_add(arg1, arg2, net::use_awaitable);
}

// Handles an HTTP server connection
net::awaitable<void> do_session(tcp_stream stream, ZstoreController &zctrl)
{
    // This buffer is required to persist across reads
    beast::flat_buffer buffer;

    // This lambda is used to send messages
    try {
        for (;;) {
            // Set the timeout.
            stream.expires_after(std::chrono::seconds(30));

            // Read a request
            http::request<http::string_body> req;
            co_await http::async_read(stream, buffer, req);

            // HttpMsg msg = co_await
            // (awaitable_on_request(std::move(req)));
            HttpMsg msg =
                co_await (awaitable_on_request(std::move(req), zctrl));
            // fmt::print("Result: {}\n", msg);

            // Determine if we should close the connection
            bool keep_alive = msg.keep_alive();

            // Send the response
            co_await beast::async_write(stream, std::move(msg),
                                        net::use_awaitable);

            if (!keep_alive) {
                // This means we should close the connection, usually
                // because the response indicated the "Connection: close"
                // semantic.
                break;
            }
        }
    } catch (boost::system::system_error &se) {
        if (se.code() != http::error::end_of_stream)
            throw;
    }

    // Send a TCP shutdown
    boost::beast::error_code ec;
    stream.socket().shutdown(tcp::socket::shutdown_send, ec);

    // At this point the connection is closed gracefully
    // we ignore the error because the client might have
    // dropped the connection already.
}

//------------------------------------------------------------------------------

// Accepts incoming connections and launches the sessions
net::awaitable<void> do_listen(tcp::endpoint endpoint, ZstoreController &zctrl)
{
    // Open the acceptor
    auto acceptor = net::use_awaitable.as_default_on(
        tcp::acceptor(co_await net::this_coro::executor));
    acceptor.open(endpoint.protocol());

    // Allow address reuse
    acceptor.set_option(net::socket_base::reuse_address(true));

    // Bind to the server address
    acceptor.bind(endpoint);

    // Start listening for connections
    acceptor.listen(net::socket_base::max_listen_connections);

    for (;;)
        boost::asio::co_spawn(
            acceptor.get_executor(),
            do_session(tcp_stream(co_await acceptor.async_accept()), zctrl),
            [](std::exception_ptr e) {
                if (e)
                    try {
                        std::rethrow_exception(e);
                    } catch (std::exception &e) {
                        std::cerr << "Error in session: " << e.what() << "\n";
                    }
            });
}
