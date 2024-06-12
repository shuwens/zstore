#include "../include/http_server.h"
#include "../include/global.h"
#include "../include/request_handler.h"
#include <CivetServer.h>

// https://github.com/civetweb/civetweb/blob/master/examples/embedded_cpp/embedded_cpp.cpp

using namespace std;

// Starts the local web server to allow for communcation between the robot and
// external API.
CivetServer startWebServer()
{
    printf("Starting web server with port 2000!\n");
    mg_init_library(0);

    // const char *options[] = {
    //     "document_root", DOCUMENT_ROOT, "listening_ports", PORT, 0};

    const char *options[] = {// "listening_ports", port_str,
                             "listening_ports", "2000", "tcp_nodelay", "1",
                             //"num_threads", "1",
                             "enable_keep_alive", "yes",
                             //"max_request_size", "65536",
                             0};

    std::vector<std::string> cpp_options;
    for (int i = 0; i < (sizeof(options) / sizeof(options[0]) - 1); i++) {
        cpp_options.push_back(options[i]);
    }

    CivetServer server(cpp_options); // <-- C++ style start
    ZstoreHandler h;
    server.addHandler("", h);

    while (!exitNow) {
        sleep(1);
    }

    return server;
    // printf("Bye!\n");
    // mg_exit_library();
}
