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

void b();
void Device::ReadZoneHeaders(std::map<uint64_t, uint8_t *> &zones)
{
    log_debug("111");
    bool done = false;
    auto complete = [](void *arg, const struct spdk_nvme_cpl *completion) {
        bool *done = (bool *)arg;
        *done = true;
    };

    // Read zone report
    uint32_t nr_zones = spdk_nvme_zns_ns_get_num_zones(mNamespace);
    struct spdk_nvme_zns_zone_report *report;
    uint32_t report_bytes =
        sizeof(report->descs[0]) * nr_zones + sizeof(*report);
    report = (struct spdk_nvme_zns_zone_report *)calloc(1, report_bytes);
    spdk_nvme_zns_report_zones(mNamespace, GetIoQueue(0), report, report_bytes,
                               0, SPDK_NVME_ZRA_LIST_ALL, false, complete,
                               &done);
    log_debug("222");
    while (!done) {
        log_debug("qpair completion");
        spdk_nvme_qpair_process_completions(GetIoQueue(0), 0);
    }

    log_debug("111");
    for (uint32_t i = 0; i < report->nr_zones; ++i) {
        log_debug("iiii {}", i);
        struct spdk_nvme_zns_zone_desc *zdesc = &(report->descs[i]);
        uint64_t wp = ~0ull;
        uint64_t zslba = zdesc->zslba;

        if (zdesc->zs == SPDK_NVME_ZONE_STATE_FULL ||
            zdesc->zs == SPDK_NVME_ZONE_STATE_IOPEN ||
            zdesc->zs == SPDK_NVME_ZONE_STATE_EOPEN) {
            if (zdesc->wp != zslba) {
                wp = zdesc->wp;
            }
        }

        if (wp == ~0ull) {
            continue;
        }

        uint8_t *buffer =
            (uint8_t *)spdk_zmalloc(Configuration::GetBlockSize(), 4096, NULL,
                                    SPDK_ENV_SOCKET_ID_ANY, SPDK_MALLOC_DMA);
        done = false;
        spdk_nvme_ns_cmd_read(mNamespace, GetIoQueue(0), buffer, zslba, 1,
                              complete, &done, 0);
        while (!done) {
            spdk_nvme_qpair_process_completions(GetIoQueue(0), 0);
        }

        zones[wp] = buffer;
    }

    free(report);
}
