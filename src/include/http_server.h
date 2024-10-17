#pragma once
#include "boost_utils.h"
#include "common.h"
#include "object.h"
#include "utils.h"
#include <boost/asio.hpp>
#include <boost/beast/http/message.hpp>
#include <cassert>

namespace asio = boost::asio;

// Handles an HTTP server connection
class session : public std::enable_shared_from_this<session>
{
    beast::tcp_stream stream_;
    beast::flat_buffer buffer_;
    HttpRequest req_;
    ZstoreController &zctrl_;

  public:
    // Take ownership of the stream
    session(tcp::socket &&socket, ZstoreController &zctrl)
        : stream_(std::move(socket)), zctrl_(zctrl)
    {
    }

    // Start the asynchronous operation
    void run()
    {
        // We need to be executing within a strand to perform async operations
        // on the I/O objects in this session. Although not strictly necessary
        // for single-threaded contexts, this example code is written to be
        // thread-safe by default.
        net::dispatch(stream_.get_executor(),
                      beast::bind_front_handler(&session::do_request,
                                                shared_from_this()));
    }

    void do_request()
    {
        // Make the request empty before reading,
        // otherwise the operation behavior is undefined.
        req_ = {};

        // Set the timeout.
        stream_.expires_after(std::chrono::seconds(30));

        // Read a request
        http::async_read(stream_, buffer_, req_,
                         beast::bind_front_handler(&session::on_request,
                                                   shared_from_this()));
    }

    void on_request(beast::error_code ec, std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        // This means they closed the connection
        if (ec == http::error::end_of_stream)
            return do_close();

        if (ec)
            return fail(ec, "read");

        if (req_.method() == http::verb::get) {
            // NOTE: READ path: see section 3.4
            auto object_key = req_.target();

            if (zctrl_.SearchBF(object_key).value()) {
                log_info("Object {} is recently modified", object_key);
                log_error("Unimplemented!!!");
            }

            MapEntry entry;
            auto rc = zctrl_.GetObject(object_key, entry).value();
            assert(rc == true);

            if (!zctrl_.isDraining &&
                zctrl_.mRequestContextPool->availableContexts.size() > 0) {
                if (!zctrl_.start) {
                    zctrl_.start = true;
                    zctrl_.stime = std::chrono::high_resolution_clock::now();
                }

                auto self(shared_from_this());
                auto closure_ = [this, self](HttpRequest req_) {
                    http::message_generator msg =
                        handle_request(std::move(req_));
                    bool keep_alive = msg.keep_alive();
                    beast::async_write(stream_, std::move(msg),
                                       beast::bind_front_handler(
                                           &session::on_write,
                                           shared_from_this(), keep_alive));
                };

                // log_debug("entry first tgt {} ", entry.first_tgt());
                auto dev = zctrl_.GetDevice(entry.first_tgt());
                // log_debug("111 ");

                auto slot = MakeReadRequest(&zctrl_, dev, entry.first_lba(),
                                            req_, closure_);
                assert(slot.has_value());
                {
                    std::unique_lock lock(zctrl_.GetRequestQueueMutex());
                    zctrl_.EnqueueRead(slot.value());
                }
            }
        } else if (req_.method() == http::verb::post ||
                   req_.method() == http::verb::put) {
            if (zctrl_.verbose)
                log_debug("11111");
            // NOTE: Write path: see section 3.3

            auto object_key = req_.target();
            auto object_value = req_.body();
            // log_debug("key {}, value {}", req_.target(), req_.body());

            // TODO: populate the map with consistent hashes
            auto dev_tuple = zctrl_.GetDevTuple(object_key).value();
            auto entry = zctrl_.CreateObject(object_key, dev_tuple);
            assert(entry.has_value());

            if (!zctrl_.isDraining &&
                zctrl_.mRequestContextPool->availableContexts.size() > 0) {
                if (!zctrl_.start) {
                    zctrl_.start = true;
                    zctrl_.stime = std::chrono::high_resolution_clock::now();
                }
                // if (zctrl_.verbose)
                // log_debug("222");

                auto closure_ = [this, self = shared_from_this()](
                                    HttpRequest req_, MapEntry entry) {
                    auto object_key = req_.target();

                    // update lba in map
                    auto rc = zctrl_.PutObject(object_key, entry).value();
                    // FIXME:we need to handle the failure
                    // assert(rc == true);
                    // if (rc == false)
                    //     log_debug("Inserting object {} failed", object_key);

                    // update and broadcast BF
                    auto rc2 = zctrl_.UpdateBF(object_key);
                    assert(rc2.has_value());

                    // send ack back to client
                    http::message_generator msg =
                        handle_request(std::move(req_));
                    bool keep_alive = msg.keep_alive();
                    beast::async_write(stream_, std::move(msg),
                                       beast::bind_front_handler(
                                           &session::on_write,
                                           shared_from_this(), keep_alive));
                };

                auto slot = MakeWriteRequest(
                    &zctrl_, zctrl_.GetDevice(entry.value().first_tgt()), req_,
                    entry.value(), closure_);

                assert(slot.has_value());
                {
                    std::unique_lock lock(zctrl_.GetRequestQueueMutex());
                    zctrl_.EnqueueWrite(slot.value());
                }
                if (zctrl_.verbose)
                    log_debug("666");
            }
        } else {
            log_error("Request is not a supported operation\n");
        }
    }

    void on_write(bool keep_alive, beast::error_code ec,
                  std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        if (ec)
            return fail(ec, "write");

        if (!keep_alive) {
            // This means we should close the connection, usually because
            // the response indicated the "Connection: close" semantic.
            return do_close();
        }

        // Read another request
        do_request();
    }

    void do_close()
    {
        // Send a TCP shutdown
        beast::error_code ec;
        stream_.socket().shutdown(tcp::socket::shutdown_send, ec);

        // At this point the connection is closed gracefully
    }
};

//------------------------------------------------------------------------------

// Accepts incoming connections and launches the sessions
class listener : public std::enable_shared_from_this<listener>
{
    net::io_context &ioc_;
    tcp::acceptor acceptor_;
    ZstoreController &zctrl_;

  public:
    listener(net::io_context &ioc, tcp::endpoint endpoint,
             ZstoreController &zctrl)
        : ioc_(ioc), acceptor_(net::make_strand(ioc)), zctrl_(zctrl)
    {
        beast::error_code ec;

        // Open the acceptor
        acceptor_.open(endpoint.protocol(), ec);
        if (ec) {
            fail(ec, "open");
            return;
        }

        // Allow address reuse
        acceptor_.set_option(net::socket_base::reuse_address(true), ec);
        if (ec) {
            fail(ec, "set_option");
            return;
        }

        // Bind to the server address
        acceptor_.bind(endpoint, ec);
        if (ec) {
            fail(ec, "bind");
            return;
        }

        // Start listening for connections
        acceptor_.listen(net::socket_base::max_listen_connections, ec);
        if (ec) {
            fail(ec, "listen");
            return;
        }
    }

    // Start accepting incoming connections
    void run() { do_accept(); }

  private:
    void do_accept()
    {
        // The new connection gets its own strand
        acceptor_.async_accept(net::make_strand(ioc_),
                               beast::bind_front_handler(&listener::on_accept,
                                                         shared_from_this()));
    }

    void on_accept(beast::error_code ec, tcp::socket socket)
    {
        if (ec) {
            fail(ec, "accept");
            return; // To avoid infinite loop
        } else {
            // Create the session and run it
            std::make_shared<session>(std::move(socket), zctrl_)->run();
        }

        // Accept another connection
        do_accept();
    }
};
