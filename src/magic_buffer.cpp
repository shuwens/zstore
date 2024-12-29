#include "include/magic_buffer.hpp"
#include "include/error.hpp"

#include <sys/mman.h>
#include <sys/shm.h>

#include <cstddef>

namespace kym
{
namespace ringbuffer
{

StatusOr<void *> GetMagicBuffer(size_t buf_size)
{
    // TODO(fischi) Check that buf_size is aligned to page size

    // (fischi) We reserve virtual memory two times the size of the buffer. This
    // mapping is not actually used, but we make sure that we have enough
    // reserved space. I stole this hack from
    // https://github.com/smcho-kr/magic-ring-buffer . This might actually be
    // racy, but I think this should be fine
    void *buf_addr = mmap(NULL, 2 * buf_size, PROT_READ | PROT_WRITE,
                          MAP_ANONYMOUS | MAP_SHARED, -1, 0);
    if (buf_addr == MAP_FAILED) {
        return Status(StatusCode::Unknown, "error resevering space");
    }

    // allocate shared memory segment that needs to be mapped twice
    int shm_id = shmget(IPC_PRIVATE, buf_size, IPC_CREAT | 0700);
    if (shm_id < 0) {
        munmap(buf_addr, 2 * buf_size);
        return Status(StatusCode::Unknown,
                      "error " + std::to_string(shm_id) +
                          " allocating Sytem V shared memory segment");
    }
    // We actually don't need this mapping
    munmap(buf_addr, 2 * buf_size);

    // attach shared memory to first buffer segment
    if (shmat(shm_id, buf_addr, 0) != buf_addr) {
        shmctl(shm_id, IPC_RMID, NULL);
        return Status(StatusCode::Unknown,
                      "error " + std::to_string(errno) +
                          " mapping shared memory to first half of buffer");
    }
    // attach shared memory to second buffer segment
    void *sec_addr = (void *)((size_t)buf_addr + buf_size);
    if (shmat(shm_id, sec_addr, 0) != sec_addr) {
        shmdt(buf_addr);
        shmctl(shm_id, IPC_RMID, NULL);
        return Status(StatusCode::Unknown,
                      "error " + std::to_string(errno) +
                          " mapping shared memory to second half of buffer");
    }

    // frees shared memory as soon as process exits.
    if (shmctl(shm_id, IPC_RMID, NULL) < 0) {
        shmdt(buf_addr);
        shmdt((char *)buf_addr + buf_size);
        return Status(StatusCode::Unknown,
                      "error " + std::to_string(errno) +
                          " marking segement to be destroy. Possible shared "
                          "memory leak!");
    }

    return buf_addr;
}

Status FreeMagicBuffer(void *buf_addr, size_t buf_size)
{
    int ret = shmdt(buf_addr);
    if (ret) {
        return Status(StatusCode::Internal,
                      "error " + std::to_string(errno) +
                          " umapping first half of buffer");
    }
    ret = shmdt((char *)buf_addr + buf_size);
    if (ret) {
        return Status(StatusCode::Internal,
                      "error " + std::to_string(errno) +
                          " umapping second half of buffer");
    }
    return Status();
}

} // namespace ringbuffer
} // namespace kym
