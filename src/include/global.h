#pragma once

#include <map>
#include <mutex>
#include <string>

// These data struct are not supposed to be global like this, but this is the
// simple way to do it. So sue me.

#define TMP_OBJECT(var, key)                                                   \
    struct object var;                                                         \
    var.name = key;

struct object {
    int len;
    void *data;       /* points to after 'name' */
    std::string name; /* null terminated */
};

std::map<std::string, object> obj_table;
std::mutex obj_table_mutex;
