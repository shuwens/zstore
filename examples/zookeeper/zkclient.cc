#include <errno.h>
#include <stdio.h>
#include <zookeeper/zookeeper.h>
using namespace std;

// Keeping track of the connection state
static int connected = 0;
static int expired = 0;

// *zkHandler handles the connection with Zookeeper
static zhandle_t *zkHandler;

// watcher function would process events
void watcher(zhandle_t *zkH, int type, int state, const char *path,
             void *watcherCtx)
{
    if (type == ZOO_SESSION_EVENT) {

        // state refers to states of zookeeper connection.
        // To keep it simple, we would demonstrate these 3:
        // ZOO_EXPIRED_SESSION_STATE, ZOO_CONNECTED_STATE,
        // ZOO_NOTCONNECTED_STATE If you are using ACL, you should be aware of
        // an authentication failure state - ZOO_AUTH_FAILED_STATE
        if (state == ZOO_CONNECTED_STATE) {
            connected = 1;
        } else if (state == ZOO_NOTCONNECTED_STATE) {
            connected = 0;
        } else if (state == ZOO_EXPIRED_SESSION_STATE) {
            expired = 1;
            connected = 0;
            zookeeper_close(zkH);
        }
    }
}

int main()
{
    zoo_set_debug_level(ZOO_LOG_LEVEL_DEBUG);

    // zookeeper_init returns the handler upon a successful connection, null
    // otherwise
    zkHandler = zookeeper_init("localhost:2181", watcher, 10000, 0, 0, 0);

    if (!zkHandler) {
        return errno;
    } else {
        printf("Connection established with Zookeeper. \n");
    }

    // Close Zookeeper connection
    zookeeper_close(zkHandler);

    return 0;
}
