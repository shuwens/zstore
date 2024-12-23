#include <unistd.h>
#define THREADED 1

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>
#include <zookeeper/zookeeper.h>

std::string zookeeper_host = "localhost:2181";
std::string election_path = "/election";
std::string current_node;
std::string predecessor_node;

zhandle_t *zh;

void watcher_callback(zhandle_t *zzh, int type, int state, const char *path,
                      void *watcherCtx);

void check_leadership()
{
    struct String_vector children;
    int rc = zoo_get_children(zh, election_path.c_str(), 0, &children);
    if (rc != ZOK) {
        std::cerr << "Error fetching children: " << rc << std::endl;
        return;
    }

    // Sort children to identify order
    std::vector<std::string> sorted_nodes;
    for (int i = 0; i < children.count; ++i) {
        sorted_nodes.push_back(children.data[i]);
    }
    std::sort(sorted_nodes.begin(), sorted_nodes.end());

    // Find the position of the current node
    auto it = std::find(sorted_nodes.begin(), sorted_nodes.end(),
                        current_node.substr(election_path.length() + 1));
    if (it == sorted_nodes.begin()) {
        std::cout << "This node is the leader." << std::endl;
    } else {
        // Watch the predecessor node
        auto predecessor_it = std::prev(it);
        predecessor_node = election_path + "/" + *predecessor_it;
        zoo_exists(zh, predecessor_node.c_str(), 1, nullptr); // Set a watch
        std::cout << "Watching predecessor node: " << predecessor_node
                  << std::endl;
    }
}

void watcher_callback(zhandle_t *zzh, int type, int state, const char *path,
                      void *watcherCtx)
{
    if (type == ZOO_DELETED_EVENT && path == predecessor_node) {
        std::cout << "Predecessor node deleted, re-evaluating leadership."
                  << std::endl;
        check_leadership();
    }
}

void start_zookeeper_connection()
{
    zh = zookeeper_init(zookeeper_host.c_str(), watcher_callback, 30000,
                        nullptr, nullptr, 0);
    if (!zh) {
        std::cerr << "Error connecting to ZooKeeper server." << std::endl;
        exit(EXIT_FAILURE);
    }
    std::cout << "Connected to ZooKeeper server." << std::endl;
}

void create_ephemeral_sequential_node()
{
    char buffer[512];
    int buffer_len = sizeof(buffer);
    int rc = zoo_create(zh, (election_path + "/node").c_str(), nullptr, -1,
                        &ZOO_OPEN_ACL_UNSAFE, ZOO_EPHEMERAL | ZOO_SEQUENCE,
                        buffer, buffer_len);
    if (rc != ZOK) {
        std::cerr << "Error creating node: " << rc << std::endl;
        exit(EXIT_FAILURE);
    }
    current_node = buffer;
    std::cout << "Created node: " << current_node << std::endl;
    check_leadership();
}

int main()
{
    start_zookeeper_connection();
    create_ephemeral_sequential_node();

    // Keep the application running to maintain the session and handle events
    while (true) {
        sleep(1);
    }

    return 0;
}
