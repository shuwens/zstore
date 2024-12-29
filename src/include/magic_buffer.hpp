/*
 *
 *  magic_buffer.hpp
 *
 *  Functions to register and free so called "magic" buffers, which is a
 * physical memory region that is mapped twice into the virtual memory space to
 * allow wrapping around the end of a ring buffer using RDMA.
 *
 *
 */
#pragma once

#include "error.hpp"
#include <cstddef>

namespace kym
{
namespace ringbuffer
{
/*
 * Get a new "magic" circular buffer allocated using shared memory.
 */
StatusOr<void *> GetMagicBuffer(size_t buf_size);
/*
 * Frees a "magic" circular buffer allocated using shared memory.
 */
Status FreeMagicBuffer(void *buf, size_t buf_size);

} // namespace ringbuffer
} // namespace kym
