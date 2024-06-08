#include <iostream>
#include <CivetServer.h>
#include "../include/webServer.h"
#include "../include/startRouteRequestHandler.h"

using namespace std;


const char *options[] = {
    // "listening_ports", port_str,
    "listening_ports", "2000",
    "tcp_nodelay", "1",
    //"num_threads", "1",
    "enable_keep_alive", "yes",
    //"max_request_size", "65536",
    0};

// Starts the local web server to allow for communcation between the robot and external API.
struct mg_context* startWebServer() {
    struct mg_context* server;

    mg_init_library(0);

    server = mg_start(NULL, 0, options);

    // Start route handlers.
    // mg_set_request_handler(server, "/start", startRouteRequestHandler, nullptr);

    cout << "Started web server." << endl;

    return server;
}
