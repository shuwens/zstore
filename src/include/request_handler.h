#pragma once
#include "CivetServer.h"
#include "global.h"
// #include "helper.h"
#include "utils.hpp"
// #include "zstore.h"
#include <cstring>
#include <unistd.h>

#define EVP_MAX_MD_SIZE 64 /* SHA512 */

#define PORT "8081"
#define EXAMPLE_URI "/example"
#define EXIT_URI "/exit"
typedef mg_connection Zstore_Connection;

#include <fstream>
#include <iostream>
#include <list>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include <CivetServer.h>
// #include <civetweb.h>
// #include <curlpp/Easy.hpp>

// #include "../s3/aws_s3.h"
// #include "../s3/multidict.h"
#include "global.h"
#include "utils.hpp"

// int obj_cmp(const void *arg1, const void *arg2)
// {
//     const struct object *o1 = arg1, *o2 = arg2;
//     return strcmp(o1->name, o2->name);
// }

void handler(int sig)
{
    mg_stop(g_ctx);
    printf("stopped\n");
    exit(0);
}

void parse_uri(const char *uri, char *bkt, char *key)
{
    char c;
    *bkt = *key = 0;

    if (*uri == '/')
        uri++;

    while ((c = *uri++) != '/' && c != 0)
        *bkt++ = c;
    *bkt = 0;

    if (c == 0)
        return;

    while ((c = *uri++) != 0)
        *key++ = c;
    *key = 0;
}

char *timestamp(char *buf, size_t len)
{
    time_t t = time(NULL);
    struct tm *tm = gmtime(&t);
    strftime(buf, len, "%FT%T.000Z", tm);
    return buf;
}

class ZstoreHandler : public CivetHandler
{
  public:
    bool handleGet(CivetServer *server, struct mg_connection *conn)
    {
        log_info("Recv GET");
        const struct mg_request_info *req = mg_get_request_info(conn);

        char bucket[128], key[128];
        const char *query = req->query_string;
        parse_uri(req->local_uri, bucket, key);
        log_info("Recv GET: bucket {}, key {}", bucket, key);

        return true;
    }

    bool handlePut(CivetServer *server, struct mg_connection *conn)
    {
        log_info("Recv PUT");
        const struct mg_request_info *req = mg_get_request_info(conn);

        char bucket[128], key[128];
        const char *query = req->query_string;
        parse_uri(req->local_uri, bucket, key);
        log_info("Recv PUT: bucket {}, key {}", bucket, key);
        // gZstoreController->Read(4096, 1 * blockSize, gBuckets[i].buffer,
        //                         nullptr, nullptr);
        return true;
    }

    bool handleDelete(CivetServer *server, struct mg_connection *conn)
    {
        log_info("Recv DELETE");
        const struct mg_request_info *req = mg_get_request_info(conn);

        char bucket[128], key[128];
        const char *query = req->query_string;
        parse_uri(req->local_uri, bucket, key);
        log_info("Recv DELETE: bucket {}, key {}", bucket, key);

        return true;
    }
};
