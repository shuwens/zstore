#include "include/device.h"

void Device::Init(struct spdk_nvme_ctrlr *ctrlr, int nsid)
{
    mController = ctrlr;
    mNamespace = spdk_nvme_ctrlr_get_ns(ctrlr, nsid);
    // if (spdk_nvme_ns_get_md_size(mNamespace) == 0) {
    //     Configuration::SetDeviceSupportMetadata(false);
    // }

    mZoneSize = spdk_nvme_zns_ns_get_zone_size_sectors(mNamespace);
    mNumZones = spdk_nvme_zns_ns_get_num_zones(mNamespace);
    if (mZoneSize == 2ull * 1024 * 256) {
        mZoneCapacity = 1077 * 256; // hard-coded here since it is ZN540; update
                                    // this for emulated SSDs
    } else {
        mZoneCapacity = mZoneSize;
    }
    log_info("Getting ns: {}, Zone size: {}, zone cap: {}, num of zones: {}",
             nsid, mZoneSize, mZoneCapacity, mNumZones);

    struct spdk_nvme_io_qpair_opts opts;
    spdk_nvme_ctrlr_get_default_io_qpair_opts(mController, &opts, sizeof(opts));
    enum spdk_nvme_qprio qprio = SPDK_NVME_QPRIO_URGENT;
    opts.qprio = qprio;

    opts.delay_cmd_submit = true;
    opts.create_only = true;

    mIoQueues = new struct spdk_nvme_qpair *[Configuration::GetNumIoThreads()];
    for (int i = 0; i < Configuration::GetNumIoThreads(); ++i) {
        mIoQueues[i] =
            spdk_nvme_ctrlr_alloc_io_qpair(ctrlr, &opts, sizeof(opts));
        assert(mIoQueues[i]);
    }

    // mQpair = spdk_nvme_ctrlr_alloc_io_qpair(ctrlr, &opts, sizeof(opts));
    // assert(mQpair);

    mReadCounts.clear();
    mTotalReadCounts = 0;
}

void Device::ConnectIoPairs()
{
    for (int i = 0; i < Configuration::GetNumIoThreads(); ++i) {
        if (spdk_nvme_ctrlr_connect_io_qpair(mController, mIoQueues[i]) < 0) {
            printf("Connect ctrl failed!\n");
        }
    }
}

void Device::InitZones(uint32_t numNeededZones, uint32_t numReservedZones) {}

void Device::EraseWholeDevice() {}

void Device::SetDeviceTransportAddress(const char *addr)
{
    memcpy(mTransportAddress, addr, SPDK_NVMF_TRADDR_MAX_LEN + 1);
}

char *Device::GetDeviceTransportAddress() const
{
    return (char *)mTransportAddress;
}
