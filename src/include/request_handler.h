// Simple example program on how to use Embedded C++ interface.

#include "global.h"
#include "helper.h"
// #include "../include/item.h"
#include "utils.hpp"
#include "zstore.h"

#include "CivetServer.h"
#include <cstring>
#include <unistd.h>
// #include "../include/backend.h"

class ZstoreHandler : public CivetHandler
{
  public:
    bool handleGet(CivetServer *server, struct mg_connection *conn)
    {
        const struct mg_request_info *req = mg_get_request_info(conn);

        char bucket[128], key[128];
        const char *query = req->query_string;
        parse_uri(req->local_uri, bucket, key);
        log_info("Recv GET: bucket {}, key {}", bucket, key);

        return true;
    }

    bool handlePut(CivetServer *server, struct mg_connection *conn)
    {
        const struct mg_request_info *req = mg_get_request_info(conn);

        char bucket[128], key[128];
        const char *query = req->query_string;
        parse_uri(req->local_uri, bucket, key);
        log_info("Recv PUT: bucket {}, key {}", bucket, key);

        return true;
    }

    bool handleDelete(CivetServer *server, struct mg_connection *conn)
    {
        const struct mg_request_info *req = mg_get_request_info(conn);

        char bucket[128], key[128];
        const char *query = req->query_string;
        parse_uri(req->local_uri, bucket, key);
        log_info("Recv DELETE: bucket {}, key {}", bucket, key);

        return true;
    }
};