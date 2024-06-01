#include <gtest/gtest.h>
#include "zns_device.h"

// Test fixture for ZNSDevice tests
class ZNSDeviceTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Set up the ZNSDevice object with a mock device URI and namespace ID
        std::string uri = "mock_device_uri";
        u32 nsid = 1;
        znsDevice = new ZNSDevice(uri, nsid);
    }

    void TearDown() override
    {
        // Clean up the ZNSDevice object
        delete znsDevice;
    }

    ZNSDevice* znsDevice;
};

// Test case for appending data to the ZNS device
TEST_F(ZNSDeviceTest, AppendData)
{
    // Create a buffer and fill it with test data
    size_t bufferSize = 4096;
    ZNSDevice::DeviceBuf buffer(*znsDevice, bufferSize);
    memset(buffer.buf, 0xAA, bufferSize);

    // Append the buffer to the ZNS device
    u64 lba = 0;
    znsDevice->append(lba, bufferSize, buffer.buf);

    // TODO: Add assertions to verify the append operation
}

// Test case for reading data from the ZNS device
TEST_F(ZNSDeviceTest, ReadData)
{
    // Create a buffer to store the read data
    size_t bufferSize = 4096;
    ZNSDevice::DeviceBuf buffer(*znsDevice, bufferSize);

    // Read data from the ZNS device
    u64 lba = 0;
    znsDevice->read(lba, bufferSize, buffer.buf);

    // TODO: Add assertions to verify the read operation
}

// Test case for finishing a zone on the ZNS device
TEST_F(ZNSDeviceTest, FinishZone)
{
    // Finish a zone on the ZNS device
    u64 slba = 0;
    znsDevice->finish_zone(slba);

    // TODO: Add assertions to verify the finish zone operation
}

// TODO: Add more test cases for other methods in the ZNSDevice class

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}