#include "include/configuration.h"
#include "include/types.h"
#include "include/zone.h"
#include <spdk/likely.h>
#include <spdk/nvme_zns.h>

void Device::Init(struct spdk_nvme_ctrlr *ctrlr, int nsid, u32 zone_id)
{
    mZoneId = zone_id;
    mController = ctrlr;
    mNamespace = spdk_nvme_ctrlr_get_ns(ctrlr, nsid);

    mZoneSize = spdk_nvme_zns_ns_get_zone_size_sectors(mNamespace);
    mNumZones = spdk_nvme_zns_ns_get_num_zones(mNamespace);
    if (mZoneSize == 2ull * 1024 * 256) {
        mZoneCapacity = 1077 * 256; // hard-coded here since it is ZN540; update
                                    // this for emulated SSDs
    } else {
        mZoneCapacity = mZoneSize;
    }

    if (spdk_nvme_ns_get_md_size(mNamespace) == 0) {
        log_info(
            "ns: {}, Zone size: {}, zone cap: {}, num of zones: {}, md size 0",
            nsid, mZoneSize, mZoneCapacity, mNumZones);
        Configuration::SetDeviceSupportMetadata(false);
    } else {
        log_info(
            "Getting ns: {}, Zone size: {}, zone cap: {}, num of zones: {}",
            nsid, mZoneSize, mZoneCapacity, mNumZones);
    }
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

void Device::InitZones(uint32_t numNeededZones, uint32_t numReservedZones)
{
    if (numNeededZones + numReservedZones > mNumZones) {
        printf(
            "Warning! The real storage space is not sufficient for the setting,"
            "%u %u %u\n",
            numNeededZones, numReservedZones, mNumZones);
    }
    mNumZones = std::min(mNumZones, numNeededZones + numReservedZones);
    mZones = new Zone[mNumZones];
    for (u32 i = 0; i < mNumZones; ++i) {
        mZones[i].Init(this, i * mZoneSize, mZoneCapacity, mZoneSize);
        mAvailableZones.insert(&mZones[i]);
    }
}

bool Device::HasAvailableZone() { return !mAvailableZones.empty(); }

Zone *Device::OpenZone()
{
    assert(!mAvailableZones.empty());
    auto it = mAvailableZones.begin();
    Zone *zone = *it;
    mAvailableZones.erase(it);

    return zone;
}

Zone *Device::OpenZoneBySlba(uint64_t slba)
{
    uint32_t zid = slba / mZoneSize;
    Zone *zone = &mZones[zid];

    assert(mAvailableZones.find(zone) != mAvailableZones.end());
    mAvailableZones.erase(zone);

    return zone;
}

void Device::ReturnZone(Zone *zone) { mAvailableZones.insert(zone); }

void Device::SetDeviceTransportAddress(const char *addr)
{
    memcpy(mTransportAddress, addr, SPDK_NVMF_TRADDR_MAX_LEN + 1);
}

char *Device::GetDeviceTransportAddress() const
{
    return (char *)mTransportAddress;
}

void Device::AddAvailableZone(Zone *zone) { mAvailableZones.insert(zone); }

uint64_t Device::GetZoneCapacity() { return mZoneCapacity; }

uint64_t Device::GetZoneSize() { return mZoneSize; }

uint32_t Device::GetNumZones() { return mNumZones; }

void Device::EraseWholeDevice()
{
    bool done = false;
    auto resetComplete = [](void *arg, const struct spdk_nvme_cpl *completion) {
        bool *done = (bool *)arg;
        *done = true;
    };

    spdk_nvme_zns_reset_zone(mNamespace, GetIoQueue(0), 0, true, resetComplete,
                             &done);

    while (!done) {
        spdk_nvme_qpair_process_completions(GetIoQueue(0), 0);
    }
}

void Device::ResetZone(Zone *zone, void *ctx)
{
    log_debug("This is currently unimplemented ");
}

void Device::FinishZone(Zone *zone, void *ctx)
{
    log_debug("This is currently unimplemented ");

    // RequestContext *slot = (RequestContext *)ctx;
    // slot->ioContext.cb = finishComplete;
    // slot->ioContext.ctx = ctx;
    // slot->ioContext.offset = zone->GetSlba();
    // slot->ioContext.flags = 0;
    //
    // if (Configuration::GetBypassDevice()) {
    //     debug_e("Bypass finish zone");
    //     slot->Queue();
    //     return;
    // }
    //
    // if (Configuration::GetEventFrameworkEnabled()) {
    //     issueIo2(zoneFinish2, slot);
    // } else {
    //     issueIo(zoneFinish, slot);
    // }
}

// static void finishComplete(void *arg, const struct spdk_nvme_cpl *completion)
// {
//     RequestContext *slot = (RequestContext *)arg;
//     if (spdk_nvme_cpl_is_error(completion)) {
//         fprintf(stderr, "I/O error status: %s\n",
//                 spdk_nvme_cpl_get_status_string(&completion->status));
//         fprintf(stderr, "Finish I/O failed, aborting run\n");
//         exit(1);
//     }
//     debug_warn("finish complete %d slot %p\n", slot->successBytes, slot);
//     assert(slot->status == FINISH_REAPING);
//     slot->Queue();
// };

void Device::FinishCurrentZone(void *ctx) {}

void Device::OpenNextZone() { mZoneId++; }

void b();
void Device::GetZoneHeaders(std::map<uint64_t, uint8_t *> &zones)
{
    // Inspired by SPDK/nvme/identify.c
    // SimpleSZD: szd.c
    bool done = false;
    auto complete = [](void *arg, const struct spdk_nvme_cpl *completion) {
        bool *done = (bool *)arg;
        *done = true;
    };

    int rc = 0;

    log_debug("111");
    // Setup state variables
    size_t report_bufsize = spdk_nvme_ns_get_max_io_xfer_size(mNamespace);
    uint8_t *report_buf = (uint8_t *)calloc(1, report_bufsize);
    uint64_t reported_zones = 0;
    uint32_t nr_zones = spdk_nvme_zns_ns_get_num_zones(mNamespace);
    // uint64_t zones_to_report = (eslba - slba) / info.zone_size;
    uint64_t zones_to_report = nr_zones;
    struct spdk_nvme_zns_zone_report *zns_report;

    log_debug("222");
    // Setup logical variables
    const struct spdk_nvme_ns_data *nsdata = spdk_nvme_ns_get_data(mNamespace);
    const struct spdk_nvme_zns_ns_data *nsdata_zns =
        spdk_nvme_zns_ns_get_data(mNamespace);
    uint64_t zone_report_size = sizeof(struct spdk_nvme_zns_zone_report);
    uint64_t zone_descriptor_size = sizeof(struct spdk_nvme_zns_zone_desc);
    uint64_t zns_descriptor_size =
        nsdata_zns->lbafe[nsdata->flbas.format].zdes * 64;
    uint64_t max_zones_per_buf =
        zns_descriptor_size
            ? (report_bufsize - zone_report_size) /
                  (zone_descriptor_size + zns_descriptor_size)
            : (report_bufsize - zone_report_size) / zone_descriptor_size;

    log_debug("333");
    // Get zone heads iteratively
    do {
        log_debug("xxxx, report buf size {}, actual size {}", report_bufsize,
                  sizeof(report_buf));
        memset(report_buf, 0, report_bufsize);
        // Get as much as we can from SPDK
        rc = spdk_nvme_zns_report_zones(
            mNamespace, GetIoQueue(0), report_buf, report_bufsize, 0,
            SPDK_NVME_ZRA_LIST_ALL, true, complete, &done);

        if (spdk_unlikely(rc != 0)) {
            free(report_buf);
            // return SZD_SC_SPDK_ERROR_REPORT_ZONES;
        }
        // Busy wait for the head.
        while (!done) {
            spdk_nvme_qpair_process_completions(GetIoQueue(0), 0);
        }

        log_debug("111");
        // retrieve nr_zones
        zns_report = (struct spdk_nvme_zns_zone_report *)report_buf;
        uint64_t nr_zones = zns_report->nr_zones;
        if (nr_zones > max_zones_per_buf || nr_zones == 0) {
            free(report_buf);
            // return SZD_SC_SPDK_ERROR_REPORT_ZONES;
        }

        // Retrieve write heads from zone information.
        for (uint64_t i = 0; i < nr_zones && reported_zones <= zones_to_report;
             i++) {
            log_debug("zone {}: start", i);
            struct spdk_nvme_zns_zone_desc *desc = &zns_report->descs[i];
            mWriteHead[i] = desc->wp;
            // if (spdk_unlikely(write_head[reported_zones] < slba)) {
            //     free(report_buf);
            //     return SZD_SC_SPDK_ERROR_REPORT_ZONES;
            // }
            // if (write_head[reported_zones] > slba + desc->zcap) {
            //     write_head[reported_zones] = slba + info.zone_size;
            // }
            // // progress
            // slba += info.zone_size;
            reported_zones++;
        }
    } while (reported_zones < zones_to_report);
    free(report_buf);
    // return SZD_SC_SUCCESS;
}

void b();
void Device::ReadZoneHeaders(std::map<uint64_t, uint8_t *> &zones)
{
    // Inspired by SPDK/nvme/identify.c
    // SimpleSZD: szd.c
    bool done = false;
    auto complete = [](void *arg, const struct spdk_nvme_cpl *completion) {
        bool *done = (bool *)arg;
        *done = true;
    };

    // TODO: some checks on devices
    // DeviceInfo info = GetIoQueue(0)->man->info;
    // if (spdk_unlikely(slba < info.min_lba || slba >= info.max_lba ||
    //                   eslba < info.min_lba || eslba >= info.max_lba ||
    //                   slba > eslba || slba % info.zone_size != 0 ||
    //                   eslba % info.zone_size != 0)) {
    //     return SZD_SC_SPDK_ERROR_REPORT_ZONES;
    // }

    // Read zone report
    uint32_t nr_zones = spdk_nvme_zns_ns_get_num_zones(mNamespace);
    struct spdk_nvme_zns_zone_report *report;
    uint32_t report_bytes =
        sizeof(report->descs[0]) * nr_zones + sizeof(*report);
    log_debug("number of zones {}, report size {}", nr_zones, report_bytes);
    report = (struct spdk_nvme_zns_zone_report *)calloc(1, report_bytes);
    spdk_nvme_zns_report_zones(mNamespace, GetIoQueue(0), report, report_bytes,
                               0, SPDK_NVME_ZRA_LIST_ALL, false, complete,
                               &done);
    while (!done) {
        spdk_nvme_qpair_process_completions(GetIoQueue(0), 0);
    }

    for (uint32_t i = 0; i < report->nr_zones; ++i) {
        log_debug("zone {}: start", i);
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
            // if (mCurrentWriteZone == 0) {
            //     log_debug("Current write zone {}, current read zone {}", i,
            //               i - 1);
            //     mCurrentWriteZone = i;
            //     mCurrentReadZone = i - 1;
            // }
            // if (mCurrentReadZone == -1)
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
        log_debug("zone {}: wp {}, zslba {}", i, wp, zslba);
    }

    free(report);
}
