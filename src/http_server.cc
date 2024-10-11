#include "include/http_server.h"

// Start the asynchronous operation
void session::run()
{
    // We need to be executing within a strand to perform async operations
    // on the I/O objects in this session. Although not strictly necessary
    // for single-threaded contexts, this example code is written to be
    // thread-safe by default.
    net::dispatch(
        stream_.get_executor(),
        beast::bind_front_handler(&session::do_request, shared_from_this()));
}

void session::do_request()
{
    // Make the request empty before reading,
    // otherwise the operation behavior is undefined.
    req_ = {};

    // Set the timeout.
    stream_.expires_after(std::chrono::seconds(30));

    // Read a request
    http::async_read(
        stream_, buffer_, req_,
        beast::bind_front_handler(&session::on_request, shared_from_this()));
}

void session::on_request(beast::error_code ec, std::size_t bytes_transferred)
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

        auto entry = zctrl_.FindObject(object_key).value();

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
                                   beast::bind_front_handler(&session::on_write,
                                                             shared_from_this(),
                                                             keep_alive));
            };

            auto slot =
                MakeReadRequest(&zctrl_, entry.first_lba(), req_, closure_);
            assert(slot.has_value());
            {
                std::unique_lock lock(zctrl_.GetRequestQueueMutex());
                zctrl_.EnqueueRead(slot.value());
            }
        }
    } else if (req_.method() == http::verb::put) {
        // NOTE: Write path: see section 3.3

        auto object_key = req_.target();
        auto object_value = req_.body();

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

            auto self(shared_from_this());
            auto closure_ = [this, self](HttpRequest req_, MapEntry entry) {
                auto object_key = req_.target();
                // update lba in map
                auto rc = zctrl_.PutObject(object_key, entry);
                assert(rc.has_value());

                // update and broadcast BF
                rc = zctrl_.UpdateBF(object_key);
                assert(rc.has_value());

                // send ack back to client
                http::message_generator msg =
                    handle_request(std::move(req_), zctrl_);
                bool keep_alive = msg.keep_alive();
                beast::async_write(stream_, std::move(msg),
                                   beast::bind_front_handler(&session::on_write,
                                                             shared_from_this(),
                                                             keep_alive));
            };

            auto slot =
                MakeWriteRequest(&zctrl_, req_, entry.value(), closure_);
            assert(slot.has_value());
            {
                std::unique_lock lock(zctrl_.GetRequestQueueMutex());
                zctrl_.EnqueueWrite(slot.value());
            }
        }
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
