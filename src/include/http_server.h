#pragma once
#include "boost_utils.h"
#include "common.h"
#include "types.h"
#include "zstore_controller.h"
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <functional>

using namespace boost::asio::experimental::awaitable_operators;

namespace http = boost::beast::http; // from <boost/beast/http.hpp>
namespace net = boost::asio;         // from <boost/asio.hpp>

using HttpMsg = http::message_generator;
using tcp = boost::asio::ip::tcp; // from <boost/asio/ip/tcp.hpp>
using tcp_stream = typename boost::beast::tcp_stream::rebind_executor<
    net::use_awaitable_t<>::executor_with_default<net::any_io_executor>>::other;

// This function implements the core logic of async
auto awaitable_on_request(HttpRequest req,
                          ZstoreController &zctrl_) -> net::awaitable<HttpMsg>
{
    auto object_key = req.target();
    if (req.method() == http::verb::get) {
        // NOTE: READ path: see section 3.4

        MapEntry entry;
        if (zctrl_.mKeyExperiment == 1) {
            // random read
            // hardcode entry value for benchmarking
            entry = createMapEntry(
                        zctrl_.GetDevTupleForRandomReads(object_key).value(),
                        zctrl_.mTotalCounts - 1, zctrl_.mTotalCounts,
                        zctrl_.mTotalCounts + 1)
                        .value();
        } else {
            auto rc = zctrl_.GetObject(object_key, entry).value();
            assert(rc == true);
        }

        if (zctrl_.SearchBF(object_key).value()) {
            log_info("Object {} is recently modified", object_key);
            log_error("Unimplemented!!!");
        }

        if (!zctrl_.isDraining &&
            zctrl_.mRequestContextPool->availableContexts.size() > 1) {

            // if (zctrl_.verbose)
            // log_debug("Tuple to read: {} {} {}", entry.first_tgt(),
            //           entry.second_tgt(), entry.third_tgt());

            auto dev1 = zctrl_.GetDevice(entry.first_tgt());
            // auto dev2 = zctrl_.GetDevice(entry.second_tgt());
            // auto dev3 = zctrl_.GetDevice(entry.third_tgt());

            auto s1 =
                MakeReadRequest(&zctrl_, dev1, entry.first_lba(), req).value();
            // auto s2 =
            //     MakeReadRequest(&zctrl_, dev2, entry.second_lba(),
            //     req).value();
            // auto s3 =
            //     MakeReadRequest(&zctrl_, dev3, entry.third_lba(),
            //     req).value();

            co_await zoneRead(s1);
            // co_await (zoneRead(s1) && zoneRead(s2) && zoneRead(s3));

            s1->Clear();
            zctrl_.mRequestContextPool->ReturnRequestContext(s1);
            // s2->Clear();
            // zctrl_.mRequestContextPool->ReturnRequestContext(s2);
            // s3->Clear();
            // zctrl_.mRequestContextPool->ReturnRequestContext(s3);

            co_return handle_request(std::move(req));
        } else {
            log_error("Draining or not enough contexts");
        }

    } else if (req.method() == http::verb::post ||
               req.method() == http::verb::put) {
        // NOTE: Write path: see section 3.3
        auto object_key = req.target();
        auto object_value = req.body();

        if (zctrl_.verbose)
            log_debug("key {}, value {}", req.target(), req.body());

        auto dev_tuple = zctrl_.GetDevTupleForRandomReads(object_key).value();

        // TODO:  populate the map with consistent hashes
        // auto dev_tuple = zctrl_.GetDevTuple(object_key).value();
        auto entry = zctrl_.CreateObject(object_key, dev_tuple).value();

        // if (zctrl_.verbose)
        log_debug("Tuple to write: {} {} {}", entry.first_tgt(),
                  entry.second_tgt(), entry.third_tgt());

        if (!zctrl_.isDraining &&
            zctrl_.mRequestContextPool->availableContexts.size() > 3) {
            // if (!zctrl_.start) {
            //     zctrl_.start = true;
            //     zctrl_.stime = std::chrono::high_resolution_clock::now();
            // }

            if (zctrl_.verbose)
                log_debug("1111");
            if (zctrl_.verbose)
                log_debug("2222");
            // update and broadcast BF
            auto rc2 = zctrl_.UpdateBF(object_key);
            assert(rc2.has_value());

            if (zctrl_.verbose)
                log_debug("3333");
            auto dev1 = zctrl_.GetDevice(entry.first_tgt());
            auto dev2 = zctrl_.GetDevice(entry.second_tgt());
            auto dev3 = zctrl_.GetDevice(entry.third_tgt());

            // auto slot = MakeWriteRequest(
            //     &zctrl_, zctrl_.GetDevice(entry.first_tgt()), req, entry);

            if (zctrl_.verbose)
                log_debug("44444");
            auto s1 = MakeWriteRequest(&zctrl_, dev1, req).value();
            auto s2 = MakeWriteRequest(&zctrl_, dev2, req).value();
            auto s3 = MakeWriteRequest(&zctrl_, dev3, req).value();

            if (zctrl_.verbose)
                log_debug("5555");

            co_await zoneAppend(s1);
            log_debug("s1");
            // co_await zoneAppend(s2);
            // log_debug("s2");
            // co_await zoneAppend(s3);
            // log_debug("s3");

            // co_await (zoneAppend(s1) && zoneAppend(s2) && zoneAppend(s3));

            if (zctrl_.verbose)
                log_debug("6666");
            s1->Clear();
            zctrl_.mRequestContextPool->ReturnRequestContext(s1);
            s2->Clear();
            zctrl_.mRequestContextPool->ReturnRequestContext(s2);
            s3->Clear();
            zctrl_.mRequestContextPool->ReturnRequestContext(s3);

            auto new_entry =
                createMapEntry(std::make_tuple(entry.first_tgt(),
                                               entry.second_tgt(),
                                               entry.third_tgt()),
                               s1->append_lba, 0, 0)
                    // s1->append_lba, s2->append_lba, s3->append_lba)
                    .value();
            // update lba in map
            auto rc = zctrl_.PutObject(object_key, new_entry).value();
            assert(rc == true);
            // if (rc == false)
            //     log_debug("Inserting object {} failed ", object_key);

            // if (zctrl_.verbose)
            //     log_debug("666");
            co_return handle_request(std::move(req));
        }
    } else {
        log_error("Request is not a supported operation\n");
        co_return handle_request(std::move(req));
    }
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
            HttpRequest req;
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
