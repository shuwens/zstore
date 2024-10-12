#pragma once
#include "zstore_controller.h"
#include <boost/asio/dispatch.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/config.hpp>
#include <cstdlib>
#include <iostream>
#include <string>

namespace beast = boost::beast;
namespace http = beast::http;
using tcp = boost::asio::ip::tcp;

// Return a response for the given request.
//
// The concrete type of the response message (which depends on the
// request), is type-erased in message_generator.
template <class Body, class Allocator>
http::message_generator
handle_request(http::request<Body, http::basic_fields<Allocator>> &&req)
{
    // if (ctrl->verbose)
    //     log_debug("1111");
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
    // if (req.method() != http::verb::get && req.method() != http::verb::head)
    //     return bad_request("Unknown HTTP-method");

    // Request path must be absolute and not contain "..".
    // if (req.target().empty() || req.target()[0] != '/' ||
    //     req.target().find("..") != beast::string_view::npos)
    //     return bad_request("Illegal request-target");

    // Attempt to open the file
    // beast::error_code ec;
    // body.open(path.c_str(), beast::file_mode::scan, ec);

    // Handle the case where the file doesn't exist
    // if (ec == beast::errc::no_such_file_or_directory)
    //     return not_found(req.target());

    // Handle an unknown error
    // if (ec)
    //     return server_error(ec.message());

    // Cache the size since we need it after the move
    auto const size = req.body().size();

    // Respond to HEAD request
    if (req.method() == http::verb::head) {
        http::response<http::empty_body> res{http::status::ok, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "text/html");
        res.content_length(size);
        res.keep_alive(req.keep_alive());
        return res;
    } else if (req.method() == http::verb::get) {
        // Respond to GET request
        http::response<http::string_body> res{http::status::ok, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "object");
        res.content_length(size);
        res.keep_alive(req.keep_alive());
        res.body() = req.body();
        return res;
    } else if (req.method() == http::verb::put) {
        // if (ctrl->verbose)
        //     log_debug("dw1111");
        // Respond to PUT request
        http::response<http::string_body> res{http::status::ok, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "object");
        res.content_length(size);
        res.keep_alive(req.keep_alive());
        res.body() = "Object is written";
        return res;
    } else if (req.method() == http::verb::post) {
        // if (ctrl->verbose)
        //     log_debug("dw1111");
        // Respond to PUT request
        http::response<http::string_body> res{http::status::ok, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "object");
        res.content_length(size);
        res.keep_alive(req.keep_alive());
        res.body() = "Object is written";
        return res;
    } else {
        return bad_request("Unknown HTTP-method");
    }
}

// Report a failure
void fail(beast::error_code ec, char const *what)
{
    std::cerr << what << ": " << ec.message() << "\n";
}
