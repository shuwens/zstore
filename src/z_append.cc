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

struct LatencyBucket {
    struct timeval s, e;
    uint8_t *buffer;
    bool done;
    void print()
    {
        double elapsed =
            e.tv_sec - s.tv_sec + e.tv_usec / 1000000. - s.tv_usec / 1000000.;
        printf("%f\n", elapsed);
    }
};

void readCallback(void *arg)
{
    LatencyBucket *b = (LatencyBucket *)arg;
    b->done = true;
}

void setdone(void *arg)
{
    bool *done = (bool *)arg;
    *done = true;
}

void validate()
{
    uint8_t *readValidateBuffer = buffer_pool;
    char buffer[Configuration::GetBlockSize()];
    char tmp[Configuration::GetBlockSize()];
    printf("Validating...\n");
    struct timeval s, e;
    gettimeofday(&s, NULL);
    //  Configuration::SetEnableDegradedRead(true);
    for (uint64_t i = 0; i < gSize; ++i) {
        LatencyBucket b;
        gettimeofday(&b.s, NULL);
        bool done;
        done = false;
        gZstoreController->Read(
            i * Configuration::GetBlockSize(), Configuration::GetBlockSize(),
            readValidateBuffer +
                i % gNumBuffers * Configuration::GetBlockSize(),
            setdone, &done);
        while (!done) {
            std::this_thread::yield();
        }
        sprintf(buffer, "temp%lu", i * 7);
        if (strcmp(buffer,
                   (char *)readValidateBuffer +
                       i % gNumBuffers * Configuration::GetBlockSize()) != 0) {
            printf("Mismatch %lu\n", i);
            assert(0);
            break;
        }
    }
    printf("Read finish\n");
    //  gZstoreController->Drain();
    gettimeofday(&e, NULL);
    double mb = (double)gSize * Configuration::GetBlockSize() / 1024 / 1024;
    double elapsed =
        e.tv_sec - s.tv_sec + e.tv_usec / 1000000. - s.tv_usec / 1000000.;
    printf("Throughput: %.2fMiB/s\n", mb / elapsed);
    printf("Validate successful!\n");
}

LatencyBucket *gBuckets;
static void latency_puncher(void *arg)
{
    LatencyBucket *b = (LatencyBucket *)arg;
    gettimeofday(&(b->e), NULL);
    //  delete b->buffer;
    //  b->print();
}

static void write_complete(void *arg)
{
    std::atomic<int> *ongoing = (std::atomic<int> *)arg;
    (*ongoing)--;
}

// uint64_t gLba = 0;
//
// static void write_complete_no_atomic(void *arg)
//{
//   uint64_t *finished = (uint64_t*)arg;
//   (*finished)++;
////  fprintf(stderr, "no atomic\n");
////
////  gZstoreController->Write(gLba * 4096, 4096,
////      gBuckets[0].buffer,
////      write_complete_no_atomic, arg);
////  gLba += 1;
//}

struct trace_ctx {
    //  std::atomic<int> ongoing;
    //  uint64_t started;
    //  uint64_t finished;
    std::atomic<uint64_t> started;
    std::atomic<uint64_t> finished;
    std::mutex mtx;
    std::map<uint64_t, int> lbaLock;
    std::map<uint64_t, int> lbaReadWrite;
}; // gTraceCtx;

struct trace_input_ctx {
    struct timeval tv;
    struct trace_ctx *ctx = nullptr;
    uint64_t lba;  // in 4KiB
    uint32_t size; // in bytes
};

static void write_complete_trace(void *arg)
{
    struct trace_input_ctx *ctx = (struct trace_input_ctx *)arg;
    struct timeval s, e;
    gettimeofday(&e, 0);
    if (gUseLbaLock) {
        ctx->ctx->mtx.lock();
        for (uint64_t dlba = 0; dlba < ctx->size / 4096; dlba++) {
            ctx->ctx->lbaLock[ctx->lba + dlba]--;
            if (ctx->ctx->lbaLock[ctx->lba + dlba] == 0) {
                ctx->ctx->lbaLock.erase(ctx->lba + dlba);
                ctx->ctx->lbaReadWrite.erase(ctx->lba + dlba);
            }
        }
        ctx->ctx->mtx.unlock();
    }
    ctx->ctx->finished++;
    uint64_t timeInUs =
        (e.tv_sec - ctx->tv.tv_sec) * 1000000 + e.tv_usec - ctx->tv.tv_usec;
    latCnt[timeInUs]++;
    delete ctx;
}

// lba in 4KiB
// size in bytes
struct trace_input_ctx *trace_ctx_add_lba(struct trace_ctx *tctx, uint64_t lba,
                                          uint32_t size, bool isWrite)
{
    struct trace_input_ctx *ctx = new struct trace_input_ctx;
    // assignment
    ctx->ctx = tctx;
    ctx->lba = lba;
    ctx->size = size;
    gettimeofday(&ctx->tv, 0);

    // add lba
    if (gUseLbaLock) {
        ctx->ctx->mtx.lock();
        for (uint64_t dlba = 0; dlba < size / 4096; dlba++) {
            ctx->ctx->lbaLock[lba + dlba]++;
            ctx->ctx->lbaReadWrite[lba + dlba] = isWrite;
        }
        ctx->ctx->mtx.unlock();
    }
    return ctx;
}

// Threading model is the following:
// * we need one thread per ns/ns worker/SSD
// * we need one thread which handles civetweb tasks
//
int main(int argc, char **argv)
{

    // Bootstrap zstore
    // NOTE: we switch between zones and keep track of it with a file
    std::ifstream inputFile("../current_zone");
    if (inputFile.is_open()) {
        inputFile >> g_current_zone;
        inputFile.close();
    }
    log_info("Zstore start with current zone: {}", g_current_zone);

    setbuf(stderr, nullptr);
    uint32_t blockSize = Configuration::GetBlockSize();
    Configuration::SetStorageSpaceInBytes(gSize * blockSize);
    Configuration::SetL2PTableSizeInBytes(gL2PTableSize);

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

    gZstoreController = new ZstoreController;
    gZstoreController->Init(false);

    gBuckets = new LatencyBucket[gSize];

    // for write verification
    // if (gVerify) {
    //     gNumBuffers = gWss;
    //     bitmap = new uint8_t[gNumBuffers];
    //     memset(bitmap, 0, gNumBuffers);
    // }
    buffer_pool = new uint8_t[gNumBuffers * blockSize];
    //  buffer_pool = (uint8_t*)spdk_zmalloc(
    //      gNumBuffers * blockSize, 4096, NULL, SPDK_ENV_SOCKET_ID_ANY,
    //      SPDK_MALLOC_DMA);
    if (buffer_pool == nullptr) {
        printf("Failed to allocate buffer pool\n");
        return -1;
    }

    // if (!gVerify) {
    //     // by default, 512MiB data
    //     for (uint64_t i = 0; i < (uint64_t)gNumBuffers * blockSize; ++i) {
    //         if (i % (blockSize * gNumBuffers / 4) == 0) {
    //             printf("Init %lu\n", i);
    //         }
    //         if (i % 4096 == 0)
    //             buffer_pool[i] = rand() % 256;
    //     }
    // }

    gettimeofday(&tv1, NULL);
    uint64_t writtenBytes = 0;
    printf("Start writing...\n");

    struct timeval s, e;
    gettimeofday(&s, NULL);
    uint64_t totalSize = 0;
    uint64_t totalRequest = 0;
    log_debug("for loop...");
    for (uint64_t i = 0; i < gSize; i += 1) {
        if (verbose)
            log_debug("{}-th write...", i);
        gBuckets[i].buffer = buffer_pool + i % gNumBuffers * blockSize;
        sprintf((char *)gBuckets[i].buffer, "temp%lu", i * 7);
        gettimeofday(&gBuckets[i].s, NULL);
        gZstoreController->Read(i * blockSize, 1 * blockSize,
                                gBuckets[i].buffer, nullptr, nullptr);
        totalSize += 4096;
        totalRequest += 1;
    }
    gZstoreController->Drain();

    // Make a new segment of 100 MiB on purpose (for the crash recovery
    // exps)
    // uint64_t numSegs = gSize / (gZstoreController->GetDataRegionSize() * 3);
    // uint64_t toFill = 0;
    // if (gSize % (gZstoreController->GetDataRegionSize() * 3) > 100 * 256) {
    //     toFill = (numSegs + 1) * (gZstoreController->GetDataRegionSize() * 3)
    //     +
    //              100 * 256 - gSize;
    // } else {
    //     toFill = gSize % (gZstoreController->GetDataRegionSize() * 3);
    // }

    // for (uint64_t i = 0; i < toFill; i += 1) {
    //     gBuckets[i].buffer = buffer_pool + i % gNumBuffers * blockSize;
    //     sprintf((char *)gBuckets[i].buffer, "temp%lu", i * 7);
    //     gettimeofday(&gBuckets[i].s, NULL);
    //     gZstoreController->Read(i * blockSize, 1 * blockSize,
    //                             gBuckets[i].buffer, nullptr, nullptr);
    //
    //     totalSize += 4096;
    // }
    // log_debug("3...");
    // gZstoreController->Drain();

    log_debug("4...");
    gettimeofday(&e, NULL);
    double mb = totalSize / 1024 / 1024;
    double elapsed =
        e.tv_sec - s.tv_sec + e.tv_usec / 1000000. - s.tv_usec / 1000000.;
    printf("Total: %.2f MiB, Throughput: %.2f MiB/s\n", mb, mb / elapsed);

    double total = totalSize / 4096;
    printf("Total: %.2f IOs, Throughput: %.2f IOPS\n", total, total / elapsed);
    log_info("Total: {} IOs, Throughput: {} IOPS", totalRequest,
             totalRequest / elapsed);

    validate();
    delete gZstoreController;
}
