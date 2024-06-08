#include <fmt/core.h>
#include <szd/datastructures/szd_buffer.hpp>
#include <szd/datastructures/szd_circular_log.hpp>
#include <szd/datastructures/szd_fragmented_log.hpp>
#include <szd/datastructures/szd_log.hpp>
#include <szd/datastructures/szd_once_log.hpp>
#include <szd/szd_channel.hpp>
#include <szd/szd_channel_factory.hpp>
#include <szd/szd_device.hpp>

int main(int argc, char **argv)
{
    // Setup SZD
    SZD::SZDDevice dev("ResetPerfTest");
    dev.Init();
    // Probe devices
    std::string dev_picked;
    std::vector<SZD::DeviceOpenInfo> dev_avail;
    dev.Probe(dev_avail);
    for (auto it = dev_avail.begin(); it != dev_avail.end(); it++) {
        fmt::print("Dev: {} {}", it->is_zns, it->traddr);
    }
}
