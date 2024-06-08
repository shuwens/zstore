#include <map>

// #include <stdio.h>
// #include <string.h>
#include <civetweb.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
// #include <string.h>
// #include <signal.h>
#include <mutex>

#include "item.h"

class Backend
{
  private:
    std::map<std::string, object> obj_table;
    // avl_tree_t *obj_table;
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
