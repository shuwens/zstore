#pragma once
#include "boost_utils.h"
#include "src/include/utils.hpp"
#include <boost/asio.hpp>

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

        // MAGIC

        if (req_.method() == http::verb::get) {
            async_do_get(req_, asio::deferred);
        } else if (req_.method() == http::verb::put) {
            async_do_put(req_, asio::deferred);
        } else {
            log_error("Request is not a supported operation\n");
            //           req_.method());
        }

        // Send the response
        send_response(handle_request(*doc_root_, std::move(req_), zctrl_));
    }

    // using Packet = std::string;
    // void writeMessage(Packet msg,
    //                   std::move_only_function<void(Packet)> wroteMessage)
    // {
    //     std::thread([=, f = std::move(wroteMessage)]() mutable {
    //         // std::this_thread::sleep_for(1s);
    //         std::move(f)(msg);
    //     }).detach();
    // }

    void send_response(http::message_generator &&msg)
    {
        bool keep_alive = msg.keep_alive();

        // Write the response
        beast::async_write(stream_, std::move(msg),
                           beast::bind_front_handler(&session::on_write,
                                                     shared_from_this(),
                                                     keep_alive));
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
