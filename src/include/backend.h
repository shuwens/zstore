#pragma once

#include <map>

#include <civetweb.h>
#include <map>
#include <mutex>
#include <pthread.h>
#include <stdlib.h>
#include <string>
#include <unistd.h>

#include <CivetServer.h>
// #include <civetweb.h>
// #include <curlpp/Easy.hpp>

#include "../s3/aws_s3.h"
#include "../s3/multidict.h"

#include "item.h"

class Backend
{
  private:
    std::map<std::string, object> obj_table;
    std::mutex obj_table_mutex;
    int verbose;

  public:
    Backend(const Backend &) = delete;
    Backend(Backend &&) = delete;
    Backend &operator=(const Backend &) = delete;
    Backend &operator=(Backend &&) = delete;
    Backend(const std::string &name);
    ~Backend();
    int begin_request(struct mg_connection *conn);
};
