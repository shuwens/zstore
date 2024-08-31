#include "include/request_handler.h"
#include "include/zns_device.h"
#include "include/zns_utils.h"
#include "include/zstore_controller.h"
#include "spdk/env.h"
#include "src/include/utils.hpp"

#include "src/zstore_controller.cc"

// NOTE currently we are using spdk threading. We might want to move to
// event/reactor/poller framework.
// https://spdk.io/doc/event.html

// a simple test program to ZapRAID
uint64_t gSize = 64 * 1024 * 1024 / Configuration::GetBlockSize();
uint64_t gRequestSize = 4096;
bool gSequential = false;
bool gSkewed = false;
uint32_t gNumBuffers = 1024 * 128;
bool gCrash = false;
std::string gAccessTrace = "";

uint32_t gTestGc = 0; // 0: fill
bool gTestMode = false;
bool gUseLbaLock = false;
bool gHybridSize = false;
// 150GiB WSS for GC test
uint64_t gWss = 150ull * 1024 * 1024 * 1024 / Configuration::GetBlockSize();
uint64_t gTrafficSize = 1ull * 1024 * 1024 * 1024 * 1024;

uint32_t gChunkSize = 4096 * 4;
uint32_t gWriteSizeUnit = 16 * 4096; // 256KiB still stuck, use 64KiB and try

std::string gTraceFile = "";
struct timeval tv1;

uint8_t *buffer_pool;

uint32_t qDepth = 1;
bool gVerify = false;
uint8_t *bitmap = nullptr;
uint32_t gNumOpenSegments = 1;
uint64_t gL2PTableSize = 0;
std::map<uint32_t, uint32_t> latCnt;

ZstoreController *gZstoreController;

// Threading model is the following:
// * we need one thread per ns/ns worker/SSD
// * we need one thread which handles civetweb tasks
//
int main(int argc, char **argv)
{
    int rc;

    setbuf(stderr, nullptr);
    uint32_t blockSize = Configuration::GetBlockSize();

    Configuration::SetStorageSpaceInBytes(gSize * blockSize);

    Configuration::SetL2PTableSizeInBytes(gL2PTableSize);

    {
        Configuration::SetNumOpenSegments(gNumOpenSegments);
        uint32_t *groupSizes =
            new uint32_t[Configuration::GetNumOpenSegments()];
        uint32_t *unitSizes = new uint32_t[Configuration::GetNumOpenSegments()];
        int *paritySizes = new int[Configuration::GetNumOpenSegments()];
        int *dataSizes = new int[Configuration::GetNumOpenSegments()];
        for (int i = 0; i < Configuration::GetNumOpenSegments(); ++i) {
            groupSizes[i] = 256;
            unitSizes[i] = gChunkSize;
        }
        // special. 8+8+16+16
        if (gNumOpenSegments == 4 && gChunkSize == 16384) {
            groupSizes[1] = groupSizes[2] = groupSizes[3] = 1;
            unitSizes[0] = unitSizes[1] = 8192;
        }

        // special. 8+8+8+16
        if (gNumOpenSegments == 4 && gChunkSize == 4096 * 5) {
            groupSizes[1] = groupSizes[2] = groupSizes[3] = 1;
            unitSizes[0] = unitSizes[1] = unitSizes[2] = 8192;
            unitSizes[3] = 16384;
        }

        // special. 8+16+16+16
        if (gNumOpenSegments == 4 && gChunkSize == 4096 * 6) {
            groupSizes[1] = groupSizes[2] = groupSizes[3] = 1;
            unitSizes[0] = 8192;
            unitSizes[1] = unitSizes[2] = unitSizes[3] = 16384;
        }

        // special. 8*4+16*2
        if (gNumOpenSegments == 6 && gChunkSize == 4096 * 6) {
            groupSizes[1] = groupSizes[2] = groupSizes[3] = groupSizes[4] =
                groupSizes[5] = 1;
            unitSizes[0] = unitSizes[1] = unitSizes[2] = unitSizes[3] = 8192;
            unitSizes[4] = unitSizes[5] = 16384;
        }
        for (int i = 0; i < Configuration::GetNumOpenSegments(); ++i) {
            paritySizes[i] = unitSizes[i];
            dataSizes[i] = 3 * unitSizes[i];
        }
        Configuration::SetStripeGroupSizes(groupSizes);
        Configuration::SetStripeUnitSizes(unitSizes);
        Configuration::SetStripeParitySizes(paritySizes);
        Configuration::SetStripeDataSizes(dataSizes);
    }

    {
        struct spdk_env_opts opts;
        spdk_env_opts_init(&opts);
        opts.core_mask = "0x1fc";
        if (spdk_env_init(&opts) < 0) {
            fprintf(stderr, "Unable to initialize SPDK env.\n");
            exit(-1);
        }

        int ret = spdk_thread_lib_init(nullptr, 0);
        if (ret < 0) {
            fprintf(stderr, "Unable to initialize SPDK thread lib.\n");
            exit(-1);
        }
    }

    log_error("here ");

    gZstoreController = new ZstoreController;
    gZstoreController->Init(false);

    struct worker_thread *worker, *main_worker;
    unsigned main_core;
    char task_pool_name[30];
    uint32_t task_count = 0;
    // struct spdk_env_opts opts;

    // rc = parse_args(argc, argv);
    // if (rc != 0) {
    //     return rc;
    // }

    // spdk_env_opts_init(&opts);
    // opts.name = "arb";
    // opts.mem_size = g_dpdk_mem;
    // opts.hugepage_single_segments = g_dpdk_mem_single_seg;
    // opts.core_mask = g_zstore.core_mask;
    // if (spdk_env_init(&opts) < 0) {
    //     return 1;
    // }

    g_zstore.tsc_rate = spdk_get_ticks_hz();

    snprintf(task_pool_name, sizeof(task_pool_name), "task_pool_%d", getpid());

    /*
     * The task_count will be dynamically calculated based on the
     * number of attached active namespaces, queue depth and number
     * of cores (workers) involved in the IO perations.
     */
    task_count = g_zstore.num_namespaces > g_zstore.num_workers
                     ? g_zstore.num_namespaces
                     : g_zstore.num_workers;
    task_count *= g_zstore.queue_depth;

    task_pool =
        spdk_mempool_create(task_pool_name, task_count, sizeof(struct arb_task),
                            0, SPDK_ENV_SOCKET_ID_ANY);
    if (task_pool == NULL) {
        log_error("could not initialize task pool");
        rc = 1;
        zstore_cleanup(task_count);
        return rc;
    }

    print_configuration(argv[0]);

    log_info("Initialization complete. Launching workers.");
    // https://github.com/fallfish/zapraid/blob/main/zapraid_prototype/src/raid_controller.cc

    /* Launch all of the secondary workers */
    main_core = spdk_env_get_current_core();
    log_info("main core is: {}", main_core);
    main_worker = NULL;
    int used_core = 0;
    TAILQ_FOREACH(worker, &g_workers, link)
    {
        if (worker->lcore != main_core) {
            if (used_core < worker->lcore)
                used_core = worker->lcore;
            log_info("launch work fn on core: {}", worker->lcore);
            spdk_env_thread_launch_pinned(worker->lcore, work_fn, worker);
        } else {
            if (used_core < worker->lcore)
                used_core = worker->lcore;
            assert(main_worker == NULL);
            main_worker = worker;
        }
    }

    log_debug("1");
    // rc = spdk_env_thread_launch_pinned(used_core + 1, http_server_fn, NULL);
    log_debug("launch civetweb {}", rc);

    log_debug("1");
    assert(main_worker != NULL);
    rc = work_fn(main_worker);

    log_debug("1");
    spdk_env_thread_wait_all();

    print_stats();

    zstore_cleanup(task_count);
    return rc;
}
