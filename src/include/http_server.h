#pragma once
#include "common.h"
#include "global.h"
#include "utils.h"
#include <boost/asio.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/version.hpp>
#include <boost/config.hpp>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

class ZstoreController;

namespace asio = boost::asio;
namespace beast = boost::beast;   // from <boost/beast.hpp>
namespace http = beast::http;     // from <boost/beast/http.hpp>
namespace net = boost::asio;      // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp; // from <boost/asio/ip/tcp.hpp>

// Return a response for the given request.
//
// The concrete type of the response message (which depends on the
// request), is type-erased in message_generator.
template <class Body, class Allocator>
http::message_generator handle_request(
    // beast::string_view doc_root,
    http::request<Body, http::basic_fields<Allocator>> &&req,
    ZstoreController &zctrl)
{
    // log_debug("strt handler request ");
    auto const dummy = [&req](beast::string_view target) {
        http::response<http::string_body> res{http::status::ok, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.body() = "The resource was dummy.";
        res.prepare_payload();
        return res;
    };

    // Returns a bad request response
    auto const bad_request = [&req](beast::string_view why) {
        http::response<http::string_body> res{http::status::bad_request,
                                              req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.body() = std::string(why);
        res.prepare_payload();
        return res;
    };

    // Returns a not found response
    auto const not_found = [&req](beast::string_view target) {
        http::response<http::string_body> res{http::status::not_found,
                                              req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.body() =
            "The resource '" + std::string(target) + "' was not found.";
        res.prepare_payload();
        return res;
    };

    // Returns a server error response
    auto const server_error = [&req](beast::string_view what) {
        http::response<http::string_body> res{
            http::status::internal_server_error, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.body() = "An error occurred: '" + std::string(what) + "'";
        res.prepare_payload();
        return res;
    };

    // Make sure we can handle the method
    if (req.method() != http::verb::get && req.method() != http::verb::head)
        return bad_request("Unknown HTTP-method");

    // Request path must be absolute and not contain "..".
    if (req.target().empty() || req.target()[0] != '/' ||
        req.target().find("..") != beast::string_view::npos)
        return bad_request("Illegal request-target");

    // Build the path to the requested file
    // std::string path = path_cat(doc_root, req.target());
    // if (req.target().back() == '/')
    //     path.append("index.html");

    // Attempt to open the file
    beast::error_code ec;
    http::file_body::value_type body;
    // body.open(path.c_str(), beast::file_mode::scan, ec);

    // Handle the case where the file doesn't exist
    // if (ec == beast::errc::no_such_file_or_directory) {

    // log_debug("dummy write return");
    return dummy(req.target());
    // }
    // Handle an unknown error
    // if (ec)
    //     return server_error(ec.message());

    // Cache the size since we need it after the move
    // auto const size = body.size();

    // Respond to HEAD request
    // if (req.method() == http::verb::head) {
    //     http::response<http::empty_body> res{http::status::ok,
    //     req.version()}; res.set(http::field::server,
    //     BOOST_BEAST_VERSION_STRING);
    //     // res.set(http::field::content_type, mime_type(path));
    //     res.content_length(size);
    //     res.keep_alive(req.keep_alive());
    //     return res;
    // }

    // Respond to GET request
    // http::response<http::file_body> res{
    //     std::piecewise_construct, std::make_tuple(std::move(body)),
    //     std::make_tuple(http::status::ok, req.version())};
    // res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    // res.set(http::field::content_type, mime_type(path));
    // res.content_length(size);
    // res.keep_alive(req.keep_alive());
    // return res;
}

//------------------------------------------------------------------------------

// Report a failure
void fail(beast::error_code ec, char const *what)
{
    std::cerr << what << ": " << ec.message() << "\n";
}

// Handles an HTTP server connection
class session : public std::enable_shared_from_this<session>
{
    beast::tcp_stream stream_;
    beast::flat_buffer buffer_;
    // std::shared_ptr<std::string const> doc_root_;
    http::request<http::string_body> req_;
    ZstoreController &zctrl_;

  public:
    // NOTE the following functions are important ones where our HTTP server
    // (boost beast) interact with the SPDK infrastructure. Each of these
    // do_zstore_X function is async, and also
    // https://www.boost.org/doc/libs/1_86_0/doc/html/boost_asio/reference/asynchronous_operations.html
    // https://www.boost.org/doc/libs/1_86_0/libs/beast/doc/html/beast/ref/boost__beast__async_base.html

    // Take ownership of the stream
    session(tcp::socket &&socket,
            // std::shared_ptr<std::string const> const &doc_root,
            ZstoreController &zctrl)
        : stream_(std::move(socket)), // doc_root_(doc_root),
          zctrl_(zctrl)
    {
    }

    // Start the asynchronous operation
    void run();

    void do_request();
    void on_request(beast::error_code ec, std::size_t bytes_transferred);

    void on_write(bool keep_alive, beast::error_code ec,
                  std::size_t bytes_transferred);

    void do_close();
};

//------------------------------------------------------------------------------

// Accepts incoming connections and launches the sessions
class listener : public std::enable_shared_from_this<listener>
{
    net::io_context &ioc_;
    tcp::acceptor acceptor_;
    // std::shared_ptr<std::string const> doc_root_;
    ZstoreController &zctrl_;

  public:
    listener(net::io_context &ioc, tcp::endpoint endpoint,
             // std::shared_ptr<std::string const> const &doc_root,
             ZstoreController &zctrl)
        : ioc_(ioc), acceptor_(net::make_strand(ioc)),
          // doc_root_(doc_root),
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
            std::make_shared<session>(std::move(socket), zctrl_)->run();
        }

        // Accept another connection
        do_accept();
    }
};
