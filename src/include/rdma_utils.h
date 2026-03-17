#pragma once
#include <cstdint>
#include <fcntl.h>
#include <libgen.h>
#include <sys/stat.h>

class ZstoreController;

// RDMA client
struct client_context {
    char *buffer;
    struct ibv_mr *buffer_mr;

    struct message *msg;
    struct ibv_mr *msg_mr;

    uint64_t peer_addr;
    uint32_t peer_rkey;

    // int fd;
    const char *server_name;
    ZstoreController *ctrl;
};

// RDMA server
#define MAX_SERVER_NAME 16

struct conn_context {
    char *buffer;
    struct ibv_mr *buffer_mr;

    struct message *msg;
    struct ibv_mr *msg_mr;

    // int fd;
    char server_name[MAX_SERVER_NAME];
    ZstoreController *ctrl;
};

void write_remote(struct RdmaClient &client, struct rdma_cm_id *id,
                  uint32_t len);
void post_receive(struct RdmaClient &client, struct rdma_cm_id *id);
void send_next_hash(struct RdmaClient &client, struct rdma_cm_id *id);
void send_server_name(struct RdmaClient &client, struct rdma_cm_id *id);
void on_pre_conn(struct RdmaClient &client, struct rdma_cm_id *id);
void on_completion(struct RdmaClient &client, struct ibv_wc *wc);

// RDMA server
void send_message(struct rdma_cm_id *id);
void post_receive(struct rdma_cm_id *id);

void on_pre_conn(ZstoreController &ctrl, struct rdma_cm_id *id);
void on_connection(struct rdma_cm_id *id);
void on_completion(struct ibv_wc *wc);
void on_disconnect(struct rdma_cm_id *id);
