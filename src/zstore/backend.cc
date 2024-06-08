#include <civetweb.h>
#include <map>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <signal.h>

#include "../include/backend.h"
#include "../include/helper.h"
#include "../include/item.h"

int Backend::begin_request(struct mg_connection *conn)
{
    const struct mg_request_info *req = mg_get_request_info(conn);

    char bucket[128], key[128];
    const char *query = req->query_string;
    parse_uri(req->local_uri, bucket, key);

    if (strcmp(req->request_method, "PUT") == 0 && strlen(key) == 0) {
        /* create bucket */
        mg_send_http_error(conn, 204, "No Content");
        return 204;
    }
    if (strcmp(req->request_method, "PUT") == 0) {
        if (req->content_length > 0) {
            if (verbose)
                printf("PUT %s\n", key);
            struct object *o =
                malloc(sizeof(*o) + strlen(key) + 1 + req->content_length);
            strcpy(o->name, key);
            o->data = &(o->name[strlen(key) + 1]);
            o->len = req->content_length;
            mg_printf(conn, "HTTP/1.1 100 Continue\r\n\r\n");

            mg_read(conn, o->data, req->content_length);

            obj_table_mutex.lock();
            avl_node_t *node = avl_search(obj_table, o);
            if (node != NULL) {
                avl_node_t *node = avl_search(obj_table, o);
                free(node->item);
                node->item = o;
            } else
                avl_insert(obj_table, o);
            obj_table_mutex.unlock();
            // obj_table.insert_or_assign(key, o);

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

    /* list bucket.
     * - 1000 keys per reply * 300 bytes/key = 300KB
     * TODO: doesn't handle continuation, only sends 1000 entries
     */
    if (strcmp(req->request_method, "GET") == 0 && strlen(key) == 0) {
        const char *fmt =
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n"
            "<ListBucketResult "
            "xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\">"
            "<Name>%s</Name><Prefix></Prefix><Marker></Marker><MaxKeys>1000</"
            "MaxKeys>"
            "<Delimiter>/</Delimiter><IsTruncated>false</IsTruncated>";

        char *msg = malloc(300 * 1024), *ptr = msg;
        ptr += sprintf(msg, fmt, bucket);

        char date[64];
        timestamp(date, sizeof(date));
        char *objfmt =
            "<Contents><Key>%s</Key>"         /* key */
            "<LastModified>%s</LastModified>" /* timestamp */
            "<ETag>&quot;%p&quot;</ETag>"     /* etag (%p) */
            "<Size>%d</Size><StorageClass>STANDARD</StorageClass>" /* size */
            "<Owner><ID>user</ID><DisplayName>user</DisplayName></Owner>"
            "<Type>Normal</Type></Contents>";

        obj_table_mutex.lock();
        int n = avl_count(obj_table);
        for (int i = 0; i < n && i < 1000; i++) {
            char etag[32];
            avl_node_t *node = avl_at(obj_table, i);
            struct object *o = node->item;
            ptr += sprintf(ptr, objfmt, o->name, date, o, o->len);
        }
        obj_table_mutex.unlock();

        ptr += sprintf(ptr, "%s", "<Marker></Marker></ListBucketResult>");

        size_t len = strlen(msg);
        mg_send_http_ok(conn, "application/xml", len);
        mg_write(conn, msg, len);
        free(msg);
        return 200;
    }

    if (strcmp(req->request_method, "GET") == 0 && strlen(key) > 0) {
        TMP_OBJECT(o, key);

        obj_table_mutex.lock();
        avl_node_t *node = avl_search(obj_table, o);
        obj_table_mutex.unlock();

        if (node == NULL) {
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

        /* is it a range request?
        */
        const char *range = NULL;
        for (int i = 0; i < req->num_headers; i++)
            if (!strcmp(req->http_headers[i].name, "Range")) {
                range = req->http_headers[i].value;
                break;
            }

        struct object *obj = node->item;

        if (range != NULL) {
            off_t start, end, len;
            sscanf(range, "bytes=%ld-%ld", &start, &end);
            len = end + 1 - start;

            if (end >= obj->len) {
                mg_send_http_error(conn, 416,
                        "Requested Range Not Satisfiable");
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
                    "Content-Range: bytes %ld-%ld/%ld\r\n"
                    "Content-Length: %ld\r\n"
                    "Accept-Ranges: bytes\r\n\r\n",
                    start, end, (long)obj->len, len);
            mg_write(conn, obj->data + start, len);

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

        obj_table_mutex.lock();
        avl_node_t *node = avl_search(obj_table, o);
        if (node != NULL)
            obj = avl_delete_node(obj_table, node);
        obj_table_mutex.unlock();

        if (obj != NULL) {
            free(obj);
            mg_send_http_error(conn, 204, "No Content");
            // mg_send_http_ok(conn, "application/octet-stream", 0);
            if (verbose)
                printf("DELETE %s = 204\n", key);
            return 204;
        } else {
            mg_send_http_error(conn, 404, "%s", "Not Found");
            if (verbose)
                printf("DELETE %s = 404\n", key);
            return 404;
        }
    }

    mg_send_http_error(conn, 405, "%s", "Method Not Allowed");
    return 405;
}
