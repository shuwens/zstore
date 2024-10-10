#pragma once
#include "boost_utils.h"
#include "common.h"
#include <boost/asio.hpp>
#include <boost/beast/http/message.hpp>

namespace asio = boost::asio;

// Handles an HTTP server connection
class session : public std::enable_shared_from_this<session>
{
    beast::tcp_stream stream_;
    beast::flat_buffer buffer_;
    std::shared_ptr<std::string const> doc_root_;
    http::request<http::string_body> req_;
    ZstoreController &zctrl_;

  public:
    // NOTE the following functions are important ones where our HTTP server
    // (boost beast) interact with the SPDK infrastructure. Each of these
    // do_zstore_X function is async, and also
    // https://www.boost.org/doc/libs/1_86_0/doc/html/boost_asio/reference/asynchronous_operations.html
    // https://www.boost.org/doc/libs/1_86_0/libs/beast/doc/html/beast/ref/boost__beast__async_base.html

    template <class CompletionToken>
    auto async_do_get(http::request<http::string_body> req_,
                      CompletionToken &&token)
    {
        log_debug("start async do get ");
        auto init = [](auto completion_handler,
                       http::request<http::string_body> req_) {
            log_debug("completion of async do get ");
            // writeMessage(std::move(msg), std::move(completion_handler));
            // initiate the operation and cause completion_handler to be
            // invoked with the result
        };

        return async_initiate<CompletionToken,
                              void(http::request<http::string_body>,
                                   ZstoreObject)>(init, token, std::move(req_));
    }

    template <class CompletionToken>
    auto async_do_put(http::request<http::string_body> req_,
                      CompletionToken &&token)
    {
        log_debug("strt async do put ");

        auto init = [](auto completion_handler,
                       http::request<http::string_body> req_) {
            log_debug("completion async do put ");
            // writeMessage(std::move(msg), std::move(completion_handler));
            // initiate the operation and cause completion_handler to be
            // invoked with the result
        };

        return async_initiate<CompletionToken,
                              void(http::request<http::string_body>,
                                   ZstoreObject)>(init, token, std::move(req_));
    }

    // Take ownership of the stream
    session(tcp::socket &&socket,
            std::shared_ptr<std::string const> const &doc_root,
            ZstoreController &zctrl)
        : stream_(std::move(socket)), doc_root_(doc_root), zctrl_(zctrl)
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
            // log_debug("REQUEST: {}", req_.target());
            if (!zctrl_.isDraining &&
                zctrl_.mRequestContextPool->availableContexts.size() > 0) {
                if (!zctrl_.start) {
                    zctrl_.start = true;
                    zctrl_.stime = std::chrono::high_resolution_clock::now();
                }

                auto self(shared_from_this());
                auto closure_ = [this,
                                 self](http::request<http::string_body> req_) {
                    http::message_generator msg =
                        handle_request(std::move(req_), zctrl_);
                    bool keep_alive = msg.keep_alive();
                    beast::async_write(stream_, std::move(msg),
                                       beast::bind_front_handler(
                                           &session::on_write,
                                           shared_from_this(), keep_alive));
                };

                RequestContext *slot =
                    zctrl_.mRequestContextPool->GetRequestContext(true);
                slot->ctrl = &zctrl_;
                assert(slot->ctrl == &zctrl_);

                auto ioCtx = slot->ioContext;
                // FIXME hardcode
                // int size_in_ios = 212860928;
                int io_size_blocks = 1;
                // auto offset_in_ios = rand_r(&seed) % size_in_ios;
                // auto offset_in_ios = 1;
                ioCtx.ns = zctrl_.GetDevice()->GetNamespace();
                ioCtx.qpair = zctrl_.GetIoQpair();
                ioCtx.data = slot->dataBuffer;

                // lookup
                auto entry = zctrl_.find_object(req_.target());
                ioCtx.offset = Configuration::GetZslba() + entry.value().second;

                // ioCtx.offset = Configuration::GetZslba() +
                //                zctrl_.GetDevice()->mTotalCounts;

                ioCtx.size = io_size_blocks;
                ioCtx.cb = complete;
                ioCtx.ctx = slot;
                ioCtx.flags = 0;
                slot->ioContext = ioCtx;
                slot->request = req_;
                slot->request = std::move(req_);
                // slot->keep_alive = keep_alive;
                // slot->doc_root = doc_root_;
                // slot->session_ = this;
                slot->fn = closure_;

                assert(slot->ioContext.cb != nullptr);
                assert(slot->ctrl != nullptr);
                {
                    std::unique_lock lock(zctrl_.GetRequestQueueMutex());
                    zctrl_.EnqueueRead(slot);
                }
            }
        } else if (req_.method() == http::verb::put) {
            async_do_put(req_, asio::deferred);
        } else {
            log_error("Request is not a supported operation\n");
            //           req_.method());
        }
    }

    // void send_response(http::message_generator &&msg)
    // {
    //     // log_error("Send_response.");
    //     bool keep_alive = msg.keep_alive();
    //
    //     // Write the response
    //     // FIXME
    //     // log_error("async write.");
    //     beast::async_write(stream_, std::move(msg),
    //                        beast::bind_front_handler(&session::on_write,
    //                                                  shared_from_this(),
    //                                                  keep_alive));
    // }

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
    std::shared_ptr<std::string const> doc_root_;
    ZstoreController &zctrl_;

  public:
    listener(net::io_context &ioc, tcp::endpoint endpoint,
             std::shared_ptr<std::string const> const &doc_root,
             ZstoreController &zctrl)
        : ioc_(ioc), acceptor_(net::make_strand(ioc)), doc_root_(doc_root),
          zctrl_(zctrl)
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
            std::make_shared<session>(std::move(socket), doc_root_, zctrl_)
                ->run();
        }

        // Accept another connection
        do_accept();
    }
};
