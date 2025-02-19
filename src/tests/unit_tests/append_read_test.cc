#include "unit_tests.h"

int main(int argc, char **argv)
{
    setup_test(2, 1, "write_after_read_test");
    boost::asio::io_context ioc;

    HttpRequest dummy;
    auto dev = gZstore->GetDevice("Zstore2Dev1");

    auto write = MakeWriteRequest(gZstore, dev, dummy).value();
    const char *message = "42";
    const size_t msg_len = strlen(message);
    memcpy(write->dataBuffer, message, msg_len);
    write->dataBuffer[msg_len] = '\0'; // null-terminate the string
    printf("[WRITE] Data: %s\n", (char *)write->dataBuffer);

    boost::asio::co_spawn(
        ioc,
        [&]() -> boost::asio::awaitable<void> { co_await zoneAppend(write); },
        boost::asio::detached);
    ioc.run();

    log_info("write->append_lba: {}", write->append_lba);
    auto read = MakeReadRequest(gZstore, dev, write->append_lba).value();

    boost::asio::co_spawn(
        ioc, [&]() -> boost::asio::awaitable<void> { co_await zoneRead(read); },
        boost::asio::detached);
    ioc.run();

    read->dataBuffer[msg_len - 1] = '\0';
    printf("[READ] Data: %s\n", (char *)read->dataBuffer);

    //     task->buf =
    //         (char *)spdk_dma_zmalloc(g_arbitration.io_size_bytes, 4096,
    //         NULL);
    // / if (!task->buf)
    // {
    //         spdk_mempool_put(task_pool, task);
    //         fprintf(stderr, "task->buf spdk_dma_zmalloc failed\n");
    //         exit(1);
    //     }
    //     task->ns_ctx = ns_ctx;
    //     const uint64_t zone_dist = 0x80000; // zone size
    //     const int current_zone = 0;
    //     auto zslba = zone_dist * current_zone;
    //     rc = spdk_nvme_zns_zone_append(entry->nvme.ns, ns_ctx->qpair,
    //     task->buf,
    //                                    zslba, entry->io_size_blocks,
    //                                    io_complete, task, 0);
    //     starting_lba = completion->cdw0;
    //     printf("[WRITE] Starting LBA: 0x%" PRIx64 "\n", starting_lba);
    //     printf("[WRITE] Starting LBA: %lu \n", starting_lba);

    spdk_env_thread_wait_all();
    gZstore->zstore_cleanup();
    return 0;
}
