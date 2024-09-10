#pragma once
#include "CivetServer.h"
#include "global.h"
#include <CivetServer.h>
#include <iostream>
#include <unistd.h>
#include <unordered_map>

#define EVP_MAX_MD_SIZE 64 /* SHA512 */

#define PORT "8081"
#define EXAMPLE_URI "/example"
#define EXIT_URI "/exit"
typedef mg_connection Zstore_Connection;

/* Exit flag for main loop */
volatile bool exitNow = false;

const char *options[] = {
    // "listening_ports", port_str,
    "listening_ports", "2000", "tcp_nodelay", "1",
    //"num_threads", "1",
    "enable_keep_alive", "yes",
    //"max_request_size", "65536",
    0};

/**
 * @class Zstore
 * @brief Represents a Zstore object.
 *
 * The Zstore class is a subclass of CivetHandler and provides functionality for
 * managing Zstore objects. It allows setting the verbosity level and provides a
 * destructor.
 */
class Zstore : public CivetHandler
{
  private:
    std::string name;
    int verbose;

    // std::unordered_map<std::string, ObjectMetadata>
    //     metadata_index; // Hash Table
    // Alternatively, use a B+ Tree for range queries
    // BPlusTree<std::string, ObjectMetadata> metadata_index;

    // Functions for SSD I/O operations
    // void writeMetadata(const ObjectMetadata &metadata, uint64_t &offset);
    // void readMetadata(uint64_t offset, ObjectMetadata &metadata);
    void writeData(const std::string &data, uint64_t &offset);
    void readData(uint64_t offset, std::string &data);

  public:
    void putObject(const std::string &key, const std::string &data);
    std::string getObject(const std::string &key);
    void deleteObject(const std::string &key);

    /**
     * @brief Constructs a Zstore object with the specified name.
     *
     * @param name The name of the Zstore object.
     */
    Zstore(const std::string &name) : name(name){};

    /**
     * @brief Destroys the Zstore object.
     */
    ~Zstore(){};

    /**
     * @brief Sets the verbosity level of the Zstore object.
     *
     * @param v The verbosity level to set.
     */
    void SetVerbosity(int v) { verbose = v; }

    // std::list<AWS_S3_Bucket> buckets;
};

// https://github.com/civetweb/civetweb/blob/master/examples/embedded_cpp/embedded_cpp.cpp
/**
 * Starts the web server on port 2000.
 *
 * @return The CivetServer object representing the started web server.
 */
CivetServer startWebServer(CivetHandler h)
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
    // ZstoreHandler h;
    server.addHandler("", h);

    while (!exitNow) {
        sleep(1);
    }

    return server;
    // printf("Bye!\n");
    // mg_exit_library();
}
