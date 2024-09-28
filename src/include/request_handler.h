#pragma once
#include "CivetServer.h"
#include "global.h"
#include "utils.hpp"
#include "zstore_controller.h"
#include <cstring>
#include <unistd.h>

#define PORT "8081"
#define EXAMPLE_URI "/example"
#define EXIT_URI "/exit"

volatile bool exitNow = false;

const uint64_t zone_dist = 0x80000; // zone size
const int current_zone = 0;
// const int current_zone = 30;

auto zslba = zone_dist * current_zone;

class ZstoreController;
struct RequestContext;

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

        log_info("Recv GET with no key: bucket {}, key {}", bucket, key);

        auto ctrl = gZstoreController;

        // FIXME we assume the object is located, and turn into a read

        if (!ctrl->isDraining &&
            ctrl->mRequestContextPool->availableContexts.size() > 0) {
            if (!ctrl->start) {
                ctrl->start = true;
                ctrl->stime = std::chrono::high_resolution_clock::now();
            }

            RequestContext *slot =
                ctrl->mRequestContextPool->GetRequestContext(true);
            slot->ctrl = ctrl;
            assert(slot->ctrl == ctrl);

            auto ioCtx = slot->ioContext;
            // FIXME hardcode
            int size_in_ios = 212860928;
            int io_size_blocks = 1;
            // auto offset_in_ios = rand_r(&seed) % size_in_ios;
            auto offset_in_ios = 1;

            ioCtx.ns = ctrl->GetDevice()->GetNamespace();
            ioCtx.qpair = ctrl->GetIoQpair();
            ioCtx.data = slot->dataBuffer;
            ioCtx.offset = zslba + ctrl->GetDevice()->mTotalCounts;
            ioCtx.size = io_size_blocks;
            ioCtx.cb = complete;
            ioCtx.ctx = slot;
            ioCtx.flags = 0;
            slot->ioContext = ioCtx;

            assert(slot->ioContext.cb != nullptr);
            assert(slot->ctrl != nullptr);
            ctrl->EnqueueRead(slot);
            // busy = true;
        }

        const char *msg = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                          "<LocationConstraint "
                          "xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\">"
                          "here</LocationConstraint>";
        size_t len = strlen(msg);
        mg_send_http_ok(conn, "application/xml", len);
        mg_write(conn, msg, len);
        return 200;

        // return true;
    }

    bool handlePut(CivetServer *server, struct mg_connection *conn)
    {
        const struct mg_request_info *req = mg_get_request_info(conn);

        char bucket[128], key[128];
        const char *query = req->query_string;
        parse_uri(req->local_uri, bucket, key);
        log_info("Recv PUT: bucket {}, key {}", bucket, key);

        if (strcmp(req->request_method, "PUT") == 0 && strlen(key) == 0) {
            log_info("Recv PUT-1-: bucket {}, key {}", bucket, key);

            /* create bucket */
            mg_send_http_error(conn, 204, "No Content");
            return 204;
        }
        if (strcmp(req->request_method, "PUT") == 0) {
            log_info("Recv PUT-2-: bucket {}, key {}", bucket, key);

            if (req->content_length > 0) {
                // if (verbose)
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

        return true;
    }

    bool handleDelete(CivetServer *server, struct mg_connection *conn)
    {
        const struct mg_request_info *req = mg_get_request_info(conn);

        char bucket[128], key[128];
        const char *query = req->query_string;
        parse_uri(req->local_uri, bucket, key);
        log_info("Recv DELETE: bucket {}, key {}", bucket, key);

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
                // if (verbose)
                printf("DELETE %s = 204\n", key);
                return 204;
            } else {
                mg_send_http_error(conn, 404, "%s", "Not Found");
                // if (verbose)
                printf("DELETE %s = 404\n", key);
                return 404;
            }
        }

        mg_send_http_error(conn, 405, "%s", "Method Not Allowed");
        return 405;
        return true;
    }

    bool handlePost(CivetServer *server, struct mg_connection *conn)
    {
        /* Handler may access the request info using mg_get_request_info */
        const struct mg_request_info *req_info = mg_get_request_info(conn);
        long long rlen, wlen;
        long long nlen = 0;
        long long tlen = req_info->content_length;
        char buf[1024];

        mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: "
                        "text/html\r\nConnection: close\r\n\r\n");

        mg_printf(conn, "<html><body>\n");
        mg_printf(conn, "<h2>This is the Foo POST handler!!!</h2>\n");
        mg_printf(conn, "<p>The request was:<br><pre>%s %s HTTP/%s</pre></p>\n",
                  req_info->request_method, req_info->request_uri,
                  req_info->http_version);
        mg_printf(conn, "<p>Content Length: %li</p>\n", (long)tlen);
        mg_printf(conn, "<pre>\n");

        while (nlen < tlen) {
            rlen = tlen - nlen;
            if (rlen > sizeof(buf)) {
                rlen = sizeof(buf);
            }
            rlen = mg_read(conn, buf, (size_t)rlen);
            if (rlen <= 0) {
                break;
            }
            wlen = mg_write(conn, buf, (size_t)rlen);
            if (wlen != rlen) {
                break;
            }
            nlen += wlen;
        }

        mg_printf(conn, "\n</pre>\n");
        mg_printf(conn, "</body></html>\n");

        return true;
    }
};
