// https://stackoverflow.com/questions/12474196/how-does-zookeeper-call-complete-function-in-asynchronous-apis
#include <cstring>
#include <iostream>
#include <zookeeper/zookeeper.h>

int main()
{
    // zoo_set_debug_level(ZOO_LOG_LEVEL_DEBUG);

    // const char *host = "127.0.0.1:3000";
    //
    // zhandle_t *zh;
    // clientid_t myid;
    // zh = zookeeper_init(host, NULL, 5000, &myid, NULL, 0);
    //
    // struct Stat stat;
    // const char *line = "/test";
    // const char *ptr = "hello, world";
    // int ret = zoo_set2(zh, line, ptr, std::strlen(ptr), -1, &stat);
    // printf("zoo_set2: %s\n", zerror(ret));
    return 0;
}
