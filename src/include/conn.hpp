/*
 *
 *  conn.h
 *
 *  Common interfaces for rdma connections
 *
 */
#pragma once

#include <cstdint>
#include <stddef.h>
#include <vector>

#include "error.hpp"

namespace kym
{
namespace connection
{

struct ReceiveRegion {
    uint64_t context;
    void *addr;
    uint32_t length;
    uint32_t lkey;
};

struct SendRegion {
    uint64_t context;
    void *addr;
    uint32_t length;
    uint32_t lkey;
};

/*
 * Interfaces
 */
class Sender
{
  public:
    virtual ~Sender() = default;
    virtual StatusOr<SendRegion> GetMemoryRegion(size_t size) = 0;
    virtual Status Free(SendRegion region) = 0;
    virtual Status Send(SendRegion region) = 0;
    virtual StatusOr<uint64_t> SendAsync(SendRegion region) = 0;
    virtual Status Wait(uint64_t id) = 0;
};

class BatchSender
{
  public:
    virtual ~BatchSender() = default;
    virtual StatusOr<SendRegion> GetMemoryRegion(size_t size) = 0;
    virtual Status Free(SendRegion region) = 0;
    virtual StatusOr<uint64_t> SendAsync(SendRegion region) = 0;
    virtual Status Send(std::vector<SendRegion> regions) = 0;
    virtual StatusOr<uint64_t> SendAsync(std::vector<SendRegion> regions) = 0;
    virtual Status Wait(uint64_t id) = 0;
};

class Receiver
{
  public:
    virtual ~Receiver() = default;
    virtual StatusOr<ReceiveRegion> Receive() = 0;
    virtual Status Free(ReceiveRegion region) = 0;
};

class Connection : public Sender, public Receiver
{
  public:
    virtual ~Connection() = default;
};

} // namespace connection
} // namespace kym
