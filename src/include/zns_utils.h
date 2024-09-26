#pragma once
#include "spdk/env.h"
#include "spdk/log.h"
#include "spdk/nvme.h"
#include "spdk/nvme_spec.h"
#include "spdk/nvme_zns.h"
#include "spdk/nvmf_spec.h"
#include "spdk/string.h"
#include "src/include/utils.hpp"
#include "src/include/zstore_controller.h"

static void task_complete(struct arb_task *task);

static void io_complete(void *ctx, const struct spdk_nvme_cpl *completion);

static __thread unsigned int seed = 0;

// static void submit_single_io(void *args)
// {
//     ZstoreController *zctrlr = (ZstoreController *)args;
//     int rc;
//
//     auto worker = zctrlr->GetWorker();
//     struct ns_entry *entry = worker->ns_ctx->entry;
//
//     RequestContext *slot =
//         zctrlr->mRequestContextPool->GetRequestContext(false);
//     auto ioCtx = slot->ioContext;
//
//     ioCtx.ns = entry->nvme.ns;
//     ioCtx.qpair = worker->ns_ctx->qpair;
//     ioCtx.data = slot->dataBuffer;
//     // ioCtx.offset = offset_in_ios * entry->io_size_blocks;
//     ioCtx.offset = 0;
//     ioCtx.size = entry->io_size_blocks;
//     // ioCtx.cb = io_complete;
//     // ioCtx.ctx = task;
//     ioCtx.cb = complete;
//     bool *done = nullptr;
//     ioCtx.ctx = done;
//     ioCtx.flags = 0;
//
//     // task->ns_ctx = zctrlr->mWorker->ns_ctx;
//     slot->ctrl = zctrlr;
//     assert(slot->ctrl == zctrlr);
//
//     // if (g_arbitration.is_random) {
//     //     offset_in_ios = rand_r(&seed) % entry->size_in_ios;
//     // } else {
//     //     offset_in_ios = worker->ns_ctx->offset_in_ios++;
//     //     if (worker->ns_ctx->offset_in_ios == entry->size_in_ios) {
//     //         worker->ns_ctx->offset_in_ios = 0;
//     //     }
//     // }
//
//     // log_debug("Before READ {}", zctrlr->GetTaskPoolSize());
//     // log_debug("Before READ {}", offset_in_ios * entry->io_size_blocks);
//
//     rc = spdk_nvme_ns_cmd_read(ioCtx.ns, ioCtx.qpair, ioCtx.data,
//     ioCtx.offset,
//                                ioCtx.size, ioCtx.cb, ioCtx.ctx, ioCtx.flags);
//
//     if (rc != 0) {
//         log_error("starting I/O failed");
//     } else {
//         worker->ns_ctx->current_queue_depth++;
//     }
// }

// static void task_complete(struct arb_task *task)
// {
//     ZstoreController *zctrlr = (ZstoreController *)task->zctrlr;
//     std::lock_guard<std::mutex> lock(zctrlr->mTaskPoolMutex); // Lock the
//     mutex auto worker = zctrlr->GetWorker(); auto taskpool =
//     zctrlr->GetTaskPool();
//
//     assert(zctrlr != nullptr);
//     assert(worker != nullptr);
//     assert(worker->ns_ctx != nullptr);
//     // zctrlr->mWorker->ns_ctx = task->ns_ctx;
//     worker->ns_ctx->current_queue_depth--;
//     worker->ns_ctx->io_completed++;
//     // worker->ns_ctx->etime = std::chrono::high_resolution_clock::now();
//     // worker->ns_ctx->etimes.push_back(worker->ns_ctx->etime);
//
//     // log_debug("Before returning task {}", zctrlr->GetTaskPoolSize());
//     spdk_dma_free(task->buf);
//     spdk_mempool_put(taskpool, task);
//
//     // log_debug("After returning task {}", zctrlr->GetTaskPoolSize());
//
//     /*
//      * is_draining indicates when time has expired for the test run
//      * and we are just waiting for the previously submitted I/O
//      * to complete.  In this case, do not submit a new I/O to replace
//      * the one just completed.
//      */
//     // if (!worker->ns_ctx->is_draining) {
//     // log_info("IO count {}", zctrlr->mWorker->ns_ctx->io_completed);
//     //     submit_single_io(zctrlr);
//     // }
// }

// static void io_complete(void *ctx, const struct spdk_nvme_cpl *completion)
// {
//     task_complete((struct arb_task *)ctx);
//
//     // ZstoreController *zctrlr = (ZstoreController *)args;
//     // task_complete((struct arb_task *)ctx);
// }

static void check_io(void *args)
{
    ZstoreController *zctrlr = (ZstoreController *)args;
    // auto worker = zctrlr->GetWorker();
    spdk_nvme_qpair_process_completions(zctrlr->GetIoQpair(), 0);
}

// static void submit_io(void *args, int queue_depth)
// {
//     ZstoreController *zctrlr = (ZstoreController *)args;
//     // zctrlr->CheckTaskPool("submit IO");
//     // while (queue_depth-- > 0) {
//     while (queue_depth-- > 0) {
//         submit_single_io(zctrlr);
//     }
// }

// static void drain_io(void *args)
// {
//     ZstoreController *zctrlr = (ZstoreController *)args;
//     auto worker = zctrlr->GetWorker();
//     worker->ns_ctx->is_draining = true;
//     // while (zctrlr->mWorker->ns_ctx->current_queue_depth > 0) {
//     //     check_io(zctrlr);
//     // }
// }

static auto quit(void *args) { exit(0); }

// static void print_performance(void *args)
// {
//     ZstoreController *zctrlr = (ZstoreController *)args;
//     float io_per_second, sent_all_io_in_secs;
//     // auto worker = zctrlr->GetWorker();
//     io_per_second =
//         (float)worker->ns_ctx->io_completed / g_arbitration.time_in_sec;
//     sent_all_io_in_secs = g_arbitration.io_count / io_per_second;
//     // printf("%-43.43s core %u: %8.2f IO/s %8.2f secs/%d ios\n",
//     //        ns_ctx->entry->name, worker->lcore, io_per_second,
//     //        sent_all_io_in_secs, g_arbitration.io_count);
//     log_info("{} IO/s {} secs/{} ios", io_per_second, sent_all_io_in_secs,
//              g_arbitration.io_count);
//     //     }
//     // }
//     log_info("========================================================");
// }

static void print_stats(void *args)
{
    ZstoreController *zctrlr = (ZstoreController *)args;
    // print_performance(zctrlr);
}

// static int work_fn(void *args)
// {
//     ZstoreController *zctrlr = (ZstoreController *)args;
//     uint64_t tsc_end;
//
//     auto worker = zctrlr->GetWorker();
//     // gZstoreController->CheckTaskPool("work fn start");
//     log_info("Starting thread on core {}", worker->lcore);
//
//     tsc_end =
//         spdk_get_ticks() + g_arbitration.time_in_sec *
//         g_arbitration.tsc_rate;
//     // printf("tick %s, time in sec %s, tsc rate %s", spdk_get_ticks(),
//     //        g_arbitration.time_in_sec, g_arbitration.tsc_rate);
//
//     /* Submit initial I/O for each namespace. */
//     // TAILQ_FOREACH(ns_ctx, &worker->ns_ctx, link)
//     // {
//     log_info("1111");
//     submit_io(zctrlr, g_arbitration.queue_depth);
//     // }
//
//     log_info("222");
//     while (1) {
//         /*
//          * Check for completed I/O for each controller. A new
//          * I/O will be submitted in the io_complete callback
//          * to replace each I/O that is completed.
//          */
//         // TAILQ_FOREACH(ns_ctx, &worker->ns_ctx, link) {
//         // check_io(zctrlr->mWorker->ns_ctx);
//         // }
//
//         if (spdk_get_ticks() > tsc_end) {
//             break;
//         }
//     }
//
//     // TAILQ_FOREACH(ns_ctx, &worker->ns_ctx, link)
//     // {
//     log_debug("drain io");
//     drain_io(zctrlr);
//     log_debug("clean up ns worker");
//     zctrlr->cleanup_ns_worker_ctx();
//
//     // std::vector<uint64_t> deltas1;
//     // for (int i = 0; i < worker->ns_ctx->stimes.size(); i++) {
//     //     deltas1.push_back(
//     //         std::chrono::duration_cast<std::chrono::microseconds>(
//     //             worker->ns_ctx->etimes[i] - worker->ns_ctx->stimes[i])
//     //             .count());
//     // }
//     // auto sum1 = std::accumulate(deltas1.begin(), deltas1.end(), 0.0);
//     // auto mean1 = sum1 / deltas1.size();
//     // auto sq_sum1 = std::inner_product(deltas1.begin(), deltas1.end(),
//     //                                   deltas1.begin(), 0.0);
//     // auto stdev1 = std::sqrt(sq_sum1 / deltas1.size() - mean1 * mean1);
//     // log_info("qd: {}, mean {}, std {}", worker->ns_ctx->io_completed,
//     mean1,
//     //          stdev1);
//     //
//     // // clearnup
//     // deltas1.clear();
//     // worker->ns_ctx->etimes.clear();
//     // worker->ns_ctx->stimes.clear();
//     // }
//
//     log_debug("end work fn");
//     print_stats(zctrlr);
//     return 0;
// }

static void usage(char *program_name)
{
    log_info("{} options", program_name);
    log_info("\t[-d DPDK huge memory size in MB]\n");
    log_info("\t[-q io depth]\n");
    printf("\t[-o io size in bytes]\n");
    printf("\t[-w io pattern type, must be one of\n");
    printf("\t\t(read, write, randread, randwrite, rw, randrw)]\n");
    printf("\t[-M rwmixread (100 for reads, 0 for writes)]\n");
#ifdef DEBUG
    printf("\t[-L enable debug logging]\n");
#else
    printf("\t[-L enable debug logging (flag disabled, must reconfigure with "
           "--enable-debug)]\n");
#endif
    spdk_log_usage(stdout, "\t\t-L");
    printf("\t[-l enable latency tracking, default: disabled]\n");
    printf("\t\t(0 - disabled; 1 - enabled)\n");
    printf("\t[-t time in seconds]\n");
    printf("\t[-c core mask for I/O submission/completion.]\n");
    printf("\t\t(default: 0xf - 4 cores)]\n");
    printf("\t[-m max completions per poll]\n");
    printf("\t\t(default: 0 - unlimited)\n");
    printf("\t[-a arbitration mechanism, must be one of below]\n");
    printf("\t\t(0, 1, 2)]\n");
    printf("\t\t(0: default round robin mechanism)]\n");
    printf("\t\t(1: weighted round robin mechanism)]\n");
    printf("\t\t(2: vendor specific mechanism)]\n");
    printf("\t[-b enable arbitration user configuration, default: "
           "disabled]\n");
    printf("\t\t(0 - disabled; 1 - enabled)\n");
    printf("\t[-n subjected IOs for performance comparison]\n");
    printf("\t[-i shared memory group ID]\n");
    printf("\t[-r remote NVMe over Fabrics target address]\n");
    printf("\t[-g use single file descriptor for DPDK memory segments]\n");
}

static void print_configuration(char *program_name)
{
    log_info("{} run with configuration:", program_name);
    printf("%s -q %d -s %d -w %s -M %d -l %d -t %d -c %s -m %d -a %d -b %d -n "
           "%d -i %d\n",
           program_name, g_arbitration.queue_depth, g_arbitration.io_size_bytes,
           g_arbitration.workload_type, g_arbitration.rw_percentage,
           g_arbitration.latency_tracking_enable, g_arbitration.time_in_sec,
           g_arbitration.core_mask, g_arbitration.max_completions,
           g_arbitration.arbitration_mechanism,
           g_arbitration.arbitration_config, g_arbitration.io_count,
           g_arbitration.shm_id);
}

static int parse_args(int argc, char **argv)
{
    const char *workload_type = NULL;
    int op = 0;
    bool mix_specified = false;
    int rc;
    long int val;

    spdk_nvme_trid_populate_transport(&g_trid, SPDK_NVME_TRANSPORT_PCIE);
    snprintf(g_trid.subnqn, sizeof(g_trid.subnqn), "%s",
             SPDK_NVMF_DISCOVERY_NQN);

    while ((op = getopt(argc, argv, "a:b:c:d:ghi:l:m:n:o:q:r:t:w:M:L:")) !=
           -1) {
        switch (op) {
        case 'c':
            g_arbitration.core_mask = optarg;
            break;
        case 'd':
            g_dpdk_mem = spdk_strtol(optarg, 10);
            if (g_dpdk_mem < 0) {
                log_error("Invalid DPDK memory size");
                return g_dpdk_mem;
            }
            break;
        case 'w':
            g_arbitration.workload_type = optarg;
            break;
        case 'r':
            if (spdk_nvme_transport_id_parse(&g_trid, optarg) != 0) {
                log_error("Error parsing transport address");
                return 1;
            }
            break;
        case 'g':
            g_dpdk_mem_single_seg = true;
            break;
        case 'h':
        case '?':
            usage(argv[0]);
            return 1;
        case 'L':
            rc = spdk_log_set_flag(optarg);
            if (rc < 0) {
                log_error("unknown flag");
                usage(argv[0]);
                exit(EXIT_FAILURE);
            }
#ifdef DEBUG
            spdk_log_set_print_level(SPDK_LOG_DEBUG);
#endif
            break;
        default:
            val = spdk_strtol(optarg, 10);
            if (val < 0) {
                log_error("Converting a string to integer failed");
                return val;
            }
            switch (op) {
            case 'i':
                g_arbitration.shm_id = val;
                break;
            case 'l':
                g_arbitration.latency_tracking_enable = val;
                break;
            case 'm':
                g_arbitration.max_completions = val;
                break;
            case 'q':
                g_arbitration.queue_depth = val;
                break;
            case 'o':
                g_arbitration.io_size_bytes = val;
                break;
            case 't':
                g_arbitration.time_in_sec = val;
                break;
            case 'M':
                g_arbitration.rw_percentage = val;
                mix_specified = true;
                break;
            case 'a':
                g_arbitration.arbitration_mechanism = val;
                break;
            case 'b':
                g_arbitration.arbitration_config = val;
                break;
            case 'n':
                g_arbitration.io_count = val;
                break;
            default:
                usage(argv[0]);
                return -EINVAL;
            }
        }
    }

    workload_type = g_arbitration.workload_type;

    if (strcmp(workload_type, "read") && strcmp(workload_type, "write") &&
        strcmp(workload_type, "randread") &&
        strcmp(workload_type, "randwrite") && strcmp(workload_type, "rw") &&
        strcmp(workload_type, "randrw")) {
        log_error("io pattern type must be one of"
                  "(read, write, randread, randwrite, rw, randrw)");
        return 1;
    }

    if (!strcmp(workload_type, "read") || !strcmp(workload_type, "randread")) {
        g_arbitration.rw_percentage = 100;
    }

    if (!strcmp(workload_type, "write") ||
        !strcmp(workload_type, "randwrite")) {
        g_arbitration.rw_percentage = 0;
    }

    if (!strcmp(workload_type, "read") || !strcmp(workload_type, "randread") ||
        !strcmp(workload_type, "write") ||
        !strcmp(workload_type, "randwrite")) {
        if (mix_specified) {
            log_error("Ignoring -M option... Please use -M option"
                      " only when using rw or randrw.");
        }
    }

    if (!strcmp(workload_type, "rw") || !strcmp(workload_type, "randrw")) {
        if (g_arbitration.rw_percentage < 0 ||
            g_arbitration.rw_percentage > 100) {
            log_error("-M must be specified to value from 0 to 100 "
                      "for rw or randrw.");
            return 1;
        }
    }

    if (!strcmp(workload_type, "read") || !strcmp(workload_type, "write") ||
        !strcmp(workload_type, "rw")) {
        g_arbitration.is_random = 0;
    } else {
        log_info("workload is random");
        g_arbitration.is_random = 1;
    }

    if (g_arbitration.latency_tracking_enable != 0 &&
        g_arbitration.latency_tracking_enable != 1) {
        log_error("-l must be specified to value 0 or 1.");
        return 1;
    }

    switch (g_arbitration.arbitration_mechanism) {
    case SPDK_NVME_CC_AMS_RR:
    case SPDK_NVME_CC_AMS_WRR:
    case SPDK_NVME_CC_AMS_VS:
        break;
    default:
        log_error("-a must be specified to value 0, 1, or 7.");
        return 1;
    }

    if (g_arbitration.arbitration_config != 0 &&
        g_arbitration.arbitration_config != 1) {
        log_error("-b must be specified to value 0 or 1.");
        return 1;
    } else if (g_arbitration.arbitration_config == 1 &&
               g_arbitration.arbitration_mechanism != SPDK_NVME_CC_AMS_WRR) {
        log_error("-a must be specified to 1 (WRR) together.");
        return 1;
    }

    return 0;
}
