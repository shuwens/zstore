#pragma once

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

/*
void start() { stime = std::chrono::high_resolution_clock::now(); }
            u64 end()
            {
                auto etime = std::chrono::high_resolution_clock::now();
                auto dur =
                    std::chrono::duration_cast<std::chrono::microseconds>(
                        etime - stime);
                return dur.count();
            }
*/
