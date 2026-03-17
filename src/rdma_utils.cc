#include "include/rdma_utils.h"
#include "include/configuration.h"
#include "include/rdma_common.h"
#include "include/types.h"
#include "include/zstore_controller.h"

ObjectKeyHash to_key_hash(const char *buffer)
{
    ObjectKeyHash result;

    // for (size_t i = 0; i < 32; ++i) {
    //     std::memcpy(&result[i], buffer + i * 4, sizeof(uint32_t));
    // }

    return result;
}

// RDMA client
void write_remote(struct RdmaClient &client, struct rdma_cm_id *id,
                  uint32_t len)
{
    struct ibv_send_wr wr, *bad_wr = NULL;
    struct ibv_sge sge;

    memset(&wr, 0, sizeof(wr));

    wr.wr_id = (uintptr_t)id;
    wr.opcode = IBV_WR_RDMA_WRITE_WITH_IMM;
    wr.send_flags = IBV_SEND_SIGNALED;
    wr.imm_data = htonl(len);
    wr.wr.rdma.remote_addr = client.ctx.peer_addr;
    wr.wr.rdma.rkey = client.ctx.peer_rkey;

    if (len) {
        wr.sg_list = &sge;
        wr.num_sge = 1;

        sge.addr = (uintptr_t)client.ctx.buffer;
        sge.length = len;
        sge.lkey = client.ctx.buffer_mr->lkey;
    }

    TEST_NZ(ibv_post_send(id->qp, &wr, &bad_wr));
}

void post_receive(struct RdmaClient &client, struct rdma_cm_id *id)
{
    struct ibv_recv_wr wr, *bad_wr = NULL;
    struct ibv_sge sge;

    memset(&wr, 0, sizeof(wr));

    wr.wr_id = (uintptr_t)id;
    wr.sg_list = &sge;
    wr.num_sge = 1;

    sge.addr = (uintptr_t)client.ctx.msg;
    auto message = (struct message *)client.ctx.msg;
    sge.length = sizeof(message);
    sge.lkey = client.ctx.msg_mr->lkey;

    TEST_NZ(ibv_post_recv(id->qp, &wr, &bad_wr));
}

//  void send_next_chunk(struct rdma_cm_id *id) {
//   struct client_context *ctx = (struct client_context *)id->context;
//
//   ssize_t size = 0;
//
//   size = read(ctx->fd, ctx->buffer, BUFFER_SIZE);
//
//   if (size == -1)
//     rc_die("read() failed\n");
//
//   write_remote(id, size);
// }

void send_next_hash(struct RdmaClient &client, struct rdma_cm_id *id)
{
    ssize_t size = 0;

    strcpy(client.ctx.buffer, "1234567890abcdef");
    size = sizeof("1234567890abcdef");

    if (size == -1)
        rc_die("read() failed\n");

    write_remote(client, id, size);
}

void send_server_name(struct RdmaClient &client, struct rdma_cm_id *id)
{
    strcpy(client.ctx.buffer, client.ctx.server_name);

    write_remote(client, id, strlen(client.ctx.server_name) + 1);
}

void on_pre_conn(struct RdmaClient &client, struct rdma_cm_id *id)
{
    posix_memalign((void **)&client.ctx.buffer, sysconf(_SC_PAGESIZE),
                   RDMA_BUFFER_SIZE);
    TEST_Z(client.ctx.buffer_mr =
               ibv_reg_mr(rc_get_pd(), client.ctx.buffer, RDMA_BUFFER_SIZE, 0));

    posix_memalign((void **)&client.ctx.msg, sysconf(_SC_PAGESIZE),
                   sizeof(*client.ctx.msg));
    TEST_Z(client.ctx.msg_mr =
               ibv_reg_mr(rc_get_pd(), client.ctx.msg, sizeof(*client.ctx.msg),
                          IBV_ACCESS_LOCAL_WRITE));

    post_receive(client, id);
}

void on_completion(struct RdmaClient &client, struct ibv_wc *wc)
{
    struct rdma_cm_id *id = (struct rdma_cm_id *)(uintptr_t)(wc->wr_id);
    if (wc->opcode & IBV_WC_RECV) {
        if (client.ctx.msg->id == MSG_MR) {
            client.ctx.peer_addr = client.ctx.msg->data.mr.addr;
            client.ctx.peer_rkey = client.ctx.msg->data.mr.rkey;

            printf("received MR, sending file name\n");
            send_server_name(client, id);
        } else if (client.ctx.msg->id == MSG_READY) {
            // printf("received READY, sending chunk\n");
            // send_next_hash(client, id);

            printf("XXXXX received READY, sending chunk\n");

            ZstoreController *ctrl = client.ctrl;
            if (!ctrl->mBroadcast.empty()) {
                printf("received READY, sending chunk\n");
                ctrl->mBroadcast.visit_all([&](auto &x) {
                    send_next_hash(client, id);
                    ctrl->mBroadcast.erase(x);
                });
            }

        } else if (client.ctx.msg->id == MSG_DONE) {
            printf("received DONE, disconnecting\n");
            rc_disconnect(id);
            return;
        }

        post_receive(client, id);
    }
}

// RDMA server
void send_message(struct rdma_cm_id *id)
{
    struct conn_context *ctx = (struct conn_context *)id->context;

    struct ibv_send_wr wr, *bad_wr = NULL;
    struct ibv_sge sge;

    memset(&wr, 0, sizeof(wr));

    wr.wr_id = (uintptr_t)id;
    wr.opcode = IBV_WR_SEND;
    wr.sg_list = &sge;
    wr.num_sge = 1;
    wr.send_flags = IBV_SEND_SIGNALED;

    sge.addr = (uintptr_t)ctx->msg;
    sge.length = sizeof(*ctx->msg);
    sge.lkey = ctx->msg_mr->lkey;

    TEST_NZ(ibv_post_send(id->qp, &wr, &bad_wr));
}

void post_receive(struct rdma_cm_id *id)
{
    struct ibv_recv_wr wr, *bad_wr = NULL;

    memset(&wr, 0, sizeof(wr));

    wr.wr_id = (uintptr_t)id;
    wr.sg_list = NULL;
    wr.num_sge = 0;

    TEST_NZ(ibv_post_recv(id->qp, &wr, &bad_wr));
}

void on_pre_conn(ZstoreController &ctrl, struct rdma_cm_id *id)
{
    struct conn_context *ctx =
        (struct conn_context *)malloc(sizeof(struct conn_context));
    ctx->ctrl = &ctrl;

    id->context = ctx;

    ctx->server_name[0] = '\0'; // take this to mean we don't have the file name

    posix_memalign((void **)&ctx->buffer, sysconf(_SC_PAGESIZE),
                   RDMA_BUFFER_SIZE);
    TEST_Z(ctx->buffer_mr =
               ibv_reg_mr(rc_get_pd(), ctx->buffer, RDMA_BUFFER_SIZE,
                          IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE));

    posix_memalign((void **)&ctx->msg, sysconf(_SC_PAGESIZE),
                   sizeof(*ctx->msg));
    TEST_Z(ctx->msg_mr =
               ibv_reg_mr(rc_get_pd(), ctx->msg, sizeof(*ctx->msg), 0));

    post_receive(id);
}

void on_connection(struct rdma_cm_id *id)
{
    struct conn_context *ctx = (struct conn_context *)id->context;

    ctx->msg->id = MSG_MR;
    ctx->msg->data.mr.addr = (uintptr_t)ctx->buffer_mr->addr;
    ctx->msg->data.mr.rkey = ctx->buffer_mr->rkey;

    send_message(id);
}

void on_completion(struct ibv_wc *wc)
{
    struct rdma_cm_id *id = (struct rdma_cm_id *)(uintptr_t)wc->wr_id;
    struct conn_context *ctx = (struct conn_context *)id->context;

    if (wc->opcode == IBV_WC_RECV_RDMA_WITH_IMM) {
        uint32_t size = ntohl(wc->imm_data);

        if (size == 0) {
            ctx->msg->id = MSG_DONE;
            send_message(id);

            // don't need post_receive() since we're done with this connection

        } else if (ctx->server_name[0]) {
            printf("received %i bytes.\n", size);

            // TODO
            auto ctrl = ctx->ctrl;
            if (ctrl->SearchInflight(to_key_hash(ctx->buffer))) {
                printf("found %s in inflight\n", ctx->buffer);
            } else {
                printf("did not find %s in inflight\n", ctx->buffer);
            }
            printf("Got something from zctrl: %lu\n", ctrl->mTotalCounts);

            char hash[16];
            strcpy(hash, ctx->buffer);
            printf("opening file %s\n", ctx->buffer);
            printf("opening hash %s\n", hash);

            // if (ret != size)
            //   rc_die("write() failed");

            post_receive(id);

            ctx->msg->id = MSG_READY;
            send_message(id);

        } else {
            // size = (size > MAX_FILE_NAME) ? MAX_FILE_NAME : size;
            memcpy(ctx->server_name, ctx->buffer, 16);
            ctx->server_name[size - 1] = '\0';

            // ctx->fd = open(ctx->file_name, O_WRONLY | O_CREAT | O_EXCL,
            //                S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

            // if (ctx->fd == -1)
            //   rc_die("open() failed");

            post_receive(id);

            ctx->msg->id = MSG_READY;
            send_message(id);
        }
    }
}

void on_disconnect(struct rdma_cm_id *id)
{
    struct conn_context *ctx = (struct conn_context *)id->context;

    // close(ctx->fd);

    ibv_dereg_mr(ctx->buffer_mr);
    ibv_dereg_mr(ctx->msg_mr);

    free(ctx->buffer);
    free(ctx->msg);

    printf("finished transferring %s\n", ctx->server_name);

    free(ctx);
}
