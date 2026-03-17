#include <iostream>

#define ASIO_STANDALONE
#include <boost/asio.hpp>
#include <boost/asio/ts/buffer.hpp>
#include <boost/asio/ts/internet.hpp>

#include <chrono>

namespace asio = boost::asio;

void read_some_data(std::string &str, asio::ip::tcp::socket &socket,
                    std::vector<uint8_t> &vBuffer)
{
    socket.async_read_some(asio::buffer(vBuffer.data(), vBuffer.size()),
                           [&](boost::system::error_code err, size_t len) {
                               if (!err) {
                                   printf("%llu bytes received\n\n", len);

                                   for (auto buff : vBuffer) {
                                       str.push_back(buff);
                                   }

                                   read_some_data(str, socket, vBuffer);
                               }

                               if (err == asio::error::eof) {
                                   printf("EOF reached\n\n");
                               }
                           });
}

int main()
{

    boost::system::error_code err;
    asio::io_context ctx;
    asio::ip::tcp::endpoint endpoint(asio::ip::make_address("127.0.0.1", err),
                                     2000);
    asio::ip::tcp::socket socket(ctx);

    socket.connect(endpoint, err);

    if (err) {
        printf("Error connecting socket: %s", err.message().c_str());
    }

    if (!socket.is_open()) {
        printf("Error - socket didn't open, exiting program\n");
        return -1;
    }

    /////////////////////////
    std::string request = "GET /index.html HTTP/1.1\r\n"
                          "Host: david-barr.co.uk\r\n"
                          "Connection: close\r\n\r\n";

    socket.write_some(asio::buffer(request.data(), request.size()), err);

    std::string fullContentReceived;

    std::vector<uint8_t> vBuffer(16 * 1024);

    read_some_data(fullContentReceived, socket, vBuffer);

    while (true) {
        auto p = ctx.poll();

        if (p == asio::error::eof) {
            break;
        }
    }

    socket.close();

    ctx.stop();

    printf("%s", fullContentReceived.c_str());

    return 0;
}
