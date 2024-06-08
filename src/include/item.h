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

#include "../s3/aws_s3.h"
#include "../s3/multidict.h"

#define TMP_OBJECT(var, key)                                                   \
    struct object var;                                                         \
    var.name = key;

struct object {
    int len;
    void *data;       /* points to after 'name' */
    std::string name; /* null terminated */
};

/* ----------- */

static struct mg_context *g_ctx; /* Set by start_civetweb() */
