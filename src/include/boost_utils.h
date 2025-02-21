#pragma once

#include "types.h"
#include <boost/asio/dispatch.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/config.hpp>

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
    // Returns a bad request response
    auto const bad_request = [&req](beast::string_view why) {
        HttpResponse res{http::status::bad_request, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());

        res.body() = std::string(why);
        res.prepare_payload();
        return res;
    };

    // Returns a server error response
    auto const server_error = [&req](beast::string_view what) {
        HttpResponse res{http::status::internal_server_error, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.body() = "An error occurred: '" + std::string(what) + "'";
        res.prepare_payload();
        return res;
    };

    // Respond to HEAD request
    if (req.method() == http::verb::head) {
        http::response<http::empty_body> res{http::status::ok, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.prepare_payload();
        return res;
    } else if (req.method() == http::verb::get) {

        // Respond to GET request
        HttpResponse res{http::status::ok, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "object");
        res.keep_alive(req.keep_alive());
        // we are reusing the request body as the response body as ZStore has
        // done the swaping
        res.body() = req.body();
        res.prepare_payload();
        return res;
    } else if (req.method() == http::verb::put) {
        // Respond to PUT request
        HttpResponse res{http::status::ok, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "object");
        res.keep_alive(req.keep_alive());
        // PUT request is used to create the object
        res.body() = "PUT Object is done";
        res.prepare_payload();
        return res;
    } else if (req.method() == http::verb::post) {
        // Respond to POST request
        HttpResponse res{http::status::ok, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "object");
        res.keep_alive(req.keep_alive());
        // POST request is used to update the object
        res.body() = "POST Object is done";
        res.prepare_payload();
        return res;
    } else if (req.method() == http::verb::delete_) {
        // Respond to POST request
        HttpResponse res{http::status::ok, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "object");
        res.keep_alive(req.keep_alive());
        // POST request is used to update the object
        res.body() = "Delete Object is done";
        res.prepare_payload();
        return res;
    } else {
        return bad_request("Unknown HTTP-method");
    }
}

// A separate handler for not found requests
template <class Body, class Allocator>
http::message_generator handle_not_found_request(
    http::request<Body, http::basic_fields<Allocator>> &&req)
{
    // Returns a not found response
    auto const not_found = [&req](beast::string_view target) {
        HttpResponse res{http::status::not_found, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.body() =
            "The resource '" + std::string(target) + "' was not found.";
        res.prepare_payload();
        return res;
    };
    return not_found(req.target());
}