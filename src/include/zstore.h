#pragma once
#include "CivetServer.h"
#include "global.h"
#include <CivetServer.h>
#include <unistd.h>

#define EVP_MAX_MD_SIZE 64 /* SHA512 */

#define PORT "8081"
#define EXAMPLE_URI "/example"
#define EXIT_URI "/exit"
typedef mg_connection Zstore_Connection;

volatile bool exitNow = false;

// class Zstore : public CivetHandler
// {
//   private:
//     std::string name;
//     int verbose;
//
//   public:
//     Zstore(const std::string &name) : name(name){};
//     ~Zstore(){};
//
//     void SetVerbosity(int v) { verbose = v; }
//     // std::list<AWS_S3_Bucket> buckets;
// };

// https://github.com/civetweb/civetweb/blob/master/examples/embedded_cpp/embedded_cpp.cpp

// const char *options[] = {
//     // "listening_ports", port_str,
//     "listening_ports", "2000", "tcp_nodelay", "1",
//     //"num_threads", "1",
//     "enable_keep_alive", "yes",
//     //"max_request_size", "65536",
//     0};
