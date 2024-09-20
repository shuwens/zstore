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

// NOTE: this is a copy paste of Peter's kv store which uses civetweb
/*
int XXXXdoRequest(struct mg_connection *conn)
{
    log_info("Recv request: info");
    log_debug("Recv request: debug");
    int verbose = 1;

    const struct mg_request_info *req = mg_get_request_info(conn);

    char bucket[128], key[128];
    const char *query = req->query_string;
    parse_uri(req->local_uri, bucket, key);

    log_info("Recv request: bucket {}, key {}", bucket, key);
    log_debug("Recv request: bucket {}, key {}", bucket, key);

    if (strcmp(req->request_method, "PUT") == 0 && strlen(key) == 0) {
        //  create bucket
mg_send_http_error(conn, 204, "No Content");
return 204;
}
if (strcmp(req->request_method, "PUT") == 0) {
    if (req->content_length > 0) {
        if (verbose)
            printf("PUT %s\n", key);
        struct object *o =
            (struct object *)malloc(sizeof(*o) + req->content_length);
        o->name = key;
        o->data = malloc(req->content_length);
        o->len = req->content_length;
        mg_printf(conn, "HTTP/1.1 100 Continue\r\n\r\n");

        mg_read(conn, o->data, req->content_length);

        std::lock_guard<std::mutex> lock(obj_table_mutex);
        auto it = obj_table.find(key);
        if (it != obj_table.end()) {
            free(it->second.data);
            it->second = *o;
        } else {
            obj_table[key] = *o;
        }

        mg_printf(conn, "HTTP/1.1 204 No Content\r\n"
                        "Cache-Control: no-cache\r\n"
                        "Connection: keep-alive\r\n\r\n");
        return 204;
    } else {
        mg_send_http_error(conn, 400, "Bad Request");
        return 400;
    }
}
if (strcmp(req->request_method, "GET") == 0 && strlen(key) == 0 &&
    query != NULL && strcmp(query, "location") == 0) {
    const char *msg = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                      "<LocationConstraint "
                      "xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\">"
                      "here</LocationConstraint>";
    size_t len = strlen(msg);
    mg_send_http_ok(conn, "application/xml", len);
    mg_write(conn, msg, len);
    return 200;
}

if (strcmp(req->request_method, "GET") == 0 && strlen(key) == 0) {
    const char *fmt =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n"
        "<ListBucketResult "
        "xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\">"
        "<Name>%s</Name><Prefix></Prefix><Marker></Marker><MaxKeys>1000</"
        "MaxKeys>"
        "<Delimiter>/</Delimiter><IsTruncated>false</IsTruncated>";

    char *msg = (char *)malloc(300 * 1024), *ptr = msg;
    ptr += sprintf(msg, fmt, bucket);

    char date[64];
    timestamp(date, sizeof(date));
    char *objfmt =
        "<Contents><Key>%s</Key>"                              // key
"<LastModified>%s</LastModified>"                          // timestamp
    "<ETag>&quot;%p&quot;</ETag>"                          // etag (%p)
    "<Size>%d</Size><StorageClass>STANDARD</StorageClass>" // size
    "<Owner><ID>user</ID><DisplayName>user</DisplayName></Owner>"
    "<Type>Normal</Type></Contents>";

std::lock_guard<std::mutex> lock(obj_table_mutex);
int count = 0;
for (const auto &pair : obj_table) {
    if (count++ >= 1000)
        break;
    struct object o = pair.second;
    ptr += sprintf(ptr, objfmt, o.name.c_str(), date, o.data, o.len);
}

ptr += sprintf(ptr, "%s", "<Marker></Marker></ListBucketResult>");

size_t len = strlen(msg);
mg_send_http_ok(conn, "application/xml", len);
mg_write(conn, msg, len);
free(msg);
return 200;
}

if (strcmp(req->request_method, "GET") == 0 && strlen(key) > 0) {
    TMP_OBJECT(o, key);

    std::lock_guard<std::mutex> lock(obj_table_mutex);
    auto it = obj_table.find(key);

    if (it == obj_table.end()) {
        char *fmt = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                    "<Error><Code>NoSuchKey</Code>"
                    "<BucketName>%s</BucketName>"
                    "<RequestId>tx00-00-00-default</RequestId>"
                    "<HostId>00-default-default</HostId></Error>";
        char msg[strlen(fmt) + strlen(bucket) + 10];
        sprintf(msg, fmt, bucket);

        int len = strlen(msg);
        mg_printf(conn,
                  "HTTP/1.1 404 Not Found\r\n"
                  "Content-Type: application/xml\r\n"
                  "Connection: keep-alive\r\n"
                  "Content-Length: %d\r\n\r\n",
                  len);
        mg_write(conn, msg, len);

        if (verbose)
            printf("GET %s = 404\n", key);

        return 404;
    }

    struct object *obj = &(it->second);

    const char *range = NULL;
    for (int i = 0; i < req->num_headers; i++)
        if (!strcmp(req->http_headers[i].name, "Range")) {
            range = req->http_headers[i].value;
            break;
        }

    if (range != NULL) {
        off_t start, end, len;
        sscanf(range, "bytes=%ld-%ld", &start, &end);
        len = end + 1 - start;

        if (end >= obj->len) {
            mg_send_http_error(conn, 416, "Requested Range Not Satisfiable");
            return 416;
        }

        char range_str[128], len_str[32];
        sprintf(range_str, "bytes %ld-%ld/%ld", start, end, (long)obj->len);
        sprintf(len_str, "%ld", len);

        mg_printf(conn,
                  "HTTP/1.1 206 Partial Content\r\n"
                  "Content-Type: application/octet-stream\r\n"
                  "Cache-Control: no-cache\r\n"
                  "Connection: keep-alive\r\n"
                  "Content-Range: %s\r\n"
                  "Content-Length: %s\r\n"
                  "Accept-Ranges: bytes\r\n\r\n",
                  range_str, len_str);
        mg_write(conn, (char *)obj->data + start, len);

        return 206;

    } else {
        mg_send_http_ok(conn, "application/octet-stream", obj->len);
        mg_write(conn, obj->data, obj->len);
        return 200;
    }
}

if (strcmp(req->request_method, "DELETE") == 0) {
    TMP_OBJECT(o, key);
    struct object *obj = NULL;

    std::lock_guard<std::mutex> lock(obj_table_mutex);
    auto it = obj_table.find(key);
    if (it != obj_table.end()) {
        obj = &(it->second);
        obj_table.erase(it);
    }

    if (obj != NULL) {
        free(obj->data);
        mg_send_http_error(conn, 204, "No Content");
        if (verbose)
            printf("DELETE %s = 204\n", key);
        return 204;
    } else {
        mg_send_http_error(conn, 404, "%s", "Not Found");
        if (verbose)
            printf("DELETE %s = 404\n", key);
        return 404

            ;
    }
}

mg_send_http_error(conn, 405, "%s", "Method Not Allowed");
return 405;
}
*/
