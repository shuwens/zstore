#pragma once

#include "boost_utils.h"
#include "common.h"
// #include "configuration.h"
#include "global.h"
#include "object.h"
#include "tinyxml2.h"
#include "zstore_controller.h"
#include <boost/asio/any_completion_handler.hpp>
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/async_result.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/experimental/parallel_group.hpp>
#include <chrono>
// #include <functional>
// #include <regex>
#include <utility>

using namespace boost::asio::experimental::awaitable_operators;
namespace http = boost::beast::http; // from <boost/beast/http.hpp>

using HttpMsg = http::message_generator;
using tcp = asio::ip::tcp; // from <boost/asio/ip/tcp.hpp>
using tcp_stream = typename boost::beast::tcp_stream::rebind_executor<
    asio::use_awaitable_t<>::executor_with_default<asio::any_io_executor>>::
    other;

void async_sleep_impl(
    asio::any_completion_handler<void(boost::system::error_code)> handler,
    asio::any_io_executor ex, std::chrono::nanoseconds duration);

template <typename CompletionToken>
inline auto async_sleep(asio::any_io_executor ex,
                        std::chrono::nanoseconds duration,
                        CompletionToken &&token);

// TODO: use max keys
void create_s3_list_objects_response(
    tinyxml2::XMLDocument &doc, std::string bucket_name, int max_keys,
    std::vector<ObjectKeyHash> object_key_hashes);

std::pair<std::string, std::string> parse_url(const std::string &str);

// This function implements the core logic of async
auto awaitable_on_request(HttpRequest req,
                          ZstoreController &zctrl_) -> asio::awaitable<HttpMsg>;

// Handles an HTTP server connection
asio::awaitable<void> do_session(tcp_stream stream, ZstoreController &zctrl);

// Accepts incoming connections and launches the sessions
asio::awaitable<void> do_listen(tcp::endpoint endpoint,
                                ZstoreController &zctrl);
