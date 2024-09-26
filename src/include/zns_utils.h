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

static void check_io(void *args)
{
    ZstoreController *zctrlr = (ZstoreController *)args;
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
            // usage(argv[0]);
            return 1;
        case 'L':
            rc = spdk_log_set_flag(optarg);
            if (rc < 0) {
                log_error("unknown flag");
                // usage(argv[0]);
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
                // usage(argv[0]);
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
