/*-
 *   BSD LICENSE
 *
 *   Copyright (c) Intel Corporation.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#pragma once
#include "spdk/endian.h"
#include "spdk/env.h"
#include "spdk/log.h"
#include "spdk/nvme.h"
#include "spdk/nvme_intel.h"
#include "spdk/nvme_ocssd.h"
#include "spdk/nvme_spec.h"
#include "spdk/nvme_zns.h"
#include "spdk/nvmf_spec.h"
#include "spdk/pci_ids.h"
#include "spdk/stdinc.h"
#include "spdk/string.h"
#include "spdk/util.h"
#include "spdk/uuid.h"
#include "spdk/vmd.h"
#include "utils.hpp"

#define MAX_DISCOVERY_LOG_ENTRIES ((uint64_t)1000)

#define NUM_CHUNK_INFO_ENTRIES 8
#define MAX_ZONE_DESC_ENTRIES 8

static int outstanding_commands;

struct feature {
    uint32_t result;
    bool valid;
};

static struct feature features[256] = {};

static struct spdk_nvme_error_information_entry error_page[256];

static struct spdk_nvme_health_information_page health_page;

static struct spdk_nvme_firmware_page firmware_page;

static struct spdk_nvme_ana_page *g_ana_log_page;

static size_t g_ana_log_page_size;

static struct spdk_nvme_cmds_and_effect_log_page cmd_effects_log_page;

static struct spdk_nvme_intel_smart_information_page intel_smart_page;

static struct spdk_nvme_intel_temperature_page intel_temperature_page;

static struct spdk_nvme_intel_marketing_description_page intel_md_page;

static struct spdk_nvmf_discovery_log_page *g_discovery_page;
static size_t g_discovery_page_size;
static uint64_t g_discovery_page_numrec;

static struct spdk_ocssd_geometry_data geometry_data;

static struct spdk_ocssd_chunk_information_entry
    g_ocssd_chunk_info_page[NUM_CHUNK_INFO_ENTRIES];

static struct spdk_nvme_zns_zone_report *g_zone_report;
static size_t g_zone_report_size;
static uint64_t g_nr_zones_requested;

static bool g_hex_dump = false;

static int g_shm_id = -1;

static int g_dpdk_mem = 0;

static bool g_dpdk_mem_single_seg = false;

static int g_master_core = 0;

static char g_core_mask[16] = "0x1";

static struct spdk_nvme_transport_id g_trid;

static int g_controllers_found = 0;

static bool g_vmd = false;

static void hex_dump(const void *data, size_t size)
{
    size_t offset = 0, i;
    const uint8_t *bytes = (const uint8_t *)data;

    while (size) {
        printf("%08zX:", offset);

        for (i = 0; i < 16; i++) {
            if (i == 8) {
                printf("-");
            } else {
                printf(" ");
            }

            if (i < size) {
                printf("%02X", bytes[offset + i]);
            } else {
                printf("  ");
            }
        }

        printf("  ");

        for (i = 0; i < 16; i++) {
            if (i < size) {
                if (bytes[offset + i] > 0x20 && bytes[offset + i] < 0x7F) {
                    printf("%c", bytes[offset + i]);
                } else {
                    printf(".");
                }
            }
        }

        printf("\n");

        offset += 16;
        if (size > 16) {
            size -= 16;
        } else {
            break;
        }
    }
}

// static void get_feature_completion(void *cb_arg,
//                                    const struct spdk_nvme_cpl *cpl)
// {
//     struct feature *feature = cb_arg;
//     int fid = feature - features;
//
//     if (spdk_nvme_cpl_is_error(cpl)) {
//         printf("get_feature(0x%02X) failed\n", fid);
//     } else {
//         feature->result = cpl->cdw0;
//         feature->valid = true;
//     }
//     outstanding_commands--;
// }

static void get_log_page_completion(void *cb_arg,
                                    const struct spdk_nvme_cpl *cpl)
{
    if (spdk_nvme_cpl_is_error(cpl)) {
        printf("get log page failed\n");
    }
    outstanding_commands--;
}

static void get_ocssd_geometry_completion(void *cb_arg,
                                          const struct spdk_nvme_cpl *cpl)
{
    if (spdk_nvme_cpl_is_error(cpl)) {
        printf("get ocssd geometry failed\n");
    }
    outstanding_commands--;
}

static void get_zns_zone_report_completion(void *cb_arg,
                                           const struct spdk_nvme_cpl *cpl)
{
    if (spdk_nvme_cpl_is_error(cpl)) {
        printf("get zns zone report failed\n");
    }

    /*
     * Since we requested a partial report, verify that the firmware returned
     * the correct number of zones.
     */
    if (g_zone_report->nr_zones != g_nr_zones_requested) {
        printf("Invalid number of zones returned: %" PRIu64
               " (expected: %" PRIu64 ")\n",
               g_zone_report->nr_zones, g_nr_zones_requested);
        exit(1);
    }
    outstanding_commands--;
}

// static int get_feature(struct spdk_nvme_ctrlr *ctrlr, uint8_t fid)
// {
//     struct spdk_nvme_cmd cmd = {};
//     struct feature *feature = &features[fid];
//
//     feature->valid = false;
//
//     cmd.opc = SPDK_NVME_OPC_GET_FEATURES;
//     cmd.cdw10_bits.get_features.fid = fid;
//
//     return spdk_nvme_ctrlr_cmd_admin_raw(ctrlr, &cmd, NULL, 0,
//                                          get_feature_completion, feature);
// }

// static void get_features(struct spdk_nvme_ctrlr *ctrlr)
// {
//     size_t i;
//
//     uint8_t features_to_get[] = {
//         SPDK_NVME_FEAT_ARBITRATION, SPDK_NVME_FEAT_POWER_MANAGEMENT,
//         SPDK_NVME_FEAT_TEMPERATURE_THRESHOLD, SPDK_NVME_FEAT_ERROR_RECOVERY,
//         SPDK_NVME_FEAT_NUMBER_OF_QUEUES,      SPDK_OCSSD_FEAT_MEDIA_FEEDBACK,
//     };
//
//     /* Submit several GET FEATURES commands and wait for them to complete */
//     outstanding_commands = 0;
//     for (i = 0; i < SPDK_COUNTOF(features_to_get); i++) {
//         if (!spdk_nvme_ctrlr_is_ocssd_supported(ctrlr) &&
//             features_to_get[i] == SPDK_OCSSD_FEAT_MEDIA_FEEDBACK) {
//             continue;
//         }
//         if (get_feature(ctrlr, features_to_get[i]) == 0) {
//             outstanding_commands++;
//         } else {
//             printf("get_feature(0x%02X) failed to submit command\n",
//                    features_to_get[i]);
//         }
//     }
//
//     while (outstanding_commands) {
//         spdk_nvme_ctrlr_process_admin_completions(ctrlr);
//     }
// }

static int get_error_log_page(struct spdk_nvme_ctrlr *ctrlr)
{
    const struct spdk_nvme_ctrlr_data *cdata;

    cdata = spdk_nvme_ctrlr_get_data(ctrlr);

    if (spdk_nvme_ctrlr_cmd_get_log_page(
            ctrlr, SPDK_NVME_LOG_ERROR, SPDK_NVME_GLOBAL_NS_TAG, error_page,
            sizeof(*error_page) * (cdata->elpe + 1), 0, get_log_page_completion,
            NULL)) {
        printf("spdk_nvme_ctrlr_cmd_get_log_page() failed\n");
        exit(1);
    }

    return 0;
}

static int get_health_log_page(struct spdk_nvme_ctrlr *ctrlr)
{
    if (spdk_nvme_ctrlr_cmd_get_log_page(
            ctrlr, SPDK_NVME_LOG_HEALTH_INFORMATION, SPDK_NVME_GLOBAL_NS_TAG,
            &health_page, sizeof(health_page), 0, get_log_page_completion,
            NULL)) {
        printf("spdk_nvme_ctrlr_cmd_get_log_page() failed\n");
        exit(1);
    }

    return 0;
}

static int get_firmware_log_page(struct spdk_nvme_ctrlr *ctrlr)
{
    if (spdk_nvme_ctrlr_cmd_get_log_page(ctrlr, SPDK_NVME_LOG_FIRMWARE_SLOT,
                                         SPDK_NVME_GLOBAL_NS_TAG,
                                         &firmware_page, sizeof(firmware_page),
                                         0, get_log_page_completion, NULL)) {
        printf("spdk_nvme_ctrlr_cmd_get_log_page() failed\n");
        exit(1);
    }

    return 0;
}

static int get_ana_log_page(struct spdk_nvme_ctrlr *ctrlr)
{
    if (spdk_nvme_ctrlr_cmd_get_log_page(
            ctrlr, SPDK_NVME_LOG_ASYMMETRIC_NAMESPACE_ACCESS,
            SPDK_NVME_GLOBAL_NS_TAG, g_ana_log_page, g_ana_log_page_size, 0,
            get_log_page_completion, NULL)) {
        printf("spdk_nvme_ctrlr_cmd_get_log_page() failed\n");
        exit(1);
    }

    return 0;
}

static int get_cmd_effects_log_page(struct spdk_nvme_ctrlr *ctrlr)
{
    if (spdk_nvme_ctrlr_cmd_get_log_page(
            ctrlr, SPDK_NVME_LOG_COMMAND_EFFECTS_LOG, SPDK_NVME_GLOBAL_NS_TAG,
            &cmd_effects_log_page, sizeof(cmd_effects_log_page), 0,
            get_log_page_completion, NULL)) {
        printf("spdk_nvme_ctrlr_cmd_get_log_page() failed\n");
        exit(1);
    }

    return 0;
}

static int get_intel_smart_log_page(struct spdk_nvme_ctrlr *ctrlr)
{
    if (spdk_nvme_ctrlr_cmd_get_log_page(
            ctrlr, SPDK_NVME_INTEL_LOG_SMART, SPDK_NVME_GLOBAL_NS_TAG,
            &intel_smart_page, sizeof(intel_smart_page), 0,
            get_log_page_completion, NULL)) {
        printf("spdk_nvme_ctrlr_cmd_get_log_page() failed\n");
        exit(1);
    }

    return 0;
}

static int get_intel_temperature_log_page(struct spdk_nvme_ctrlr *ctrlr)
{
    if (spdk_nvme_ctrlr_cmd_get_log_page(
            ctrlr, SPDK_NVME_INTEL_LOG_TEMPERATURE, SPDK_NVME_GLOBAL_NS_TAG,
            &intel_temperature_page, sizeof(intel_temperature_page), 0,
            get_log_page_completion, NULL)) {
        printf("spdk_nvme_ctrlr_cmd_get_log_page() failed\n");
        exit(1);
    }
    return 0;
}

static int get_intel_md_log_page(struct spdk_nvme_ctrlr *ctrlr)
{
    if (spdk_nvme_ctrlr_cmd_get_log_page(
            ctrlr, SPDK_NVME_INTEL_MARKETING_DESCRIPTION,
            SPDK_NVME_GLOBAL_NS_TAG, &intel_md_page, sizeof(intel_md_page), 0,
            get_log_page_completion, NULL)) {
        printf("spdk_nvme_ctrlr_cmd_get_log_page() failed\n");
        exit(1);
    }
    return 0;
}

// static void
// get_discovery_log_page_header_completion(void *cb_arg,
//                                          const struct spdk_nvme_cpl *cpl)
// {
//     struct spdk_nvmf_discovery_log_page *new_discovery_page;
//     struct spdk_nvme_ctrlr *ctrlr = cb_arg;
//     uint16_t recfmt;
//     uint64_t remaining;
//     uint64_t offset;
//
//     outstanding_commands--;
//     if (spdk_nvme_cpl_is_error(cpl)) {
//         /* Return without printing anything - this may not be a discovery
//          * controller */
//         free(g_discovery_page);
//         g_discovery_page = NULL;
//         return;
//     }
//
//     /* Got the first 4K of the discovery log page */
//     recfmt = from_le16(&g_discovery_page->recfmt);
//     if (recfmt != 0) {
//         printf("Unrecognized discovery log record format %" PRIu16 "\n",
//                recfmt);
//         return;
//     }
//
//     g_discovery_page_numrec = from_le64(&g_discovery_page->numrec);
//
//     /* Pick an arbitrary limit to avoid ridiculously large buffer size. */
//     if (g_discovery_page_numrec > MAX_DISCOVERY_LOG_ENTRIES) {
//         printf("Discovery log has %" PRIu64 " entries - limiting to %" PRIu64
//                ".\n",
//                g_discovery_page_numrec, MAX_DISCOVERY_LOG_ENTRIES);
//         g_discovery_page_numrec = MAX_DISCOVERY_LOG_ENTRIES;
//     }
//
//     /*
//      * Now that we now how many entries should be in the log page, we can
//      * allocate the full log page buffer.
//      */
//     g_discovery_page_size += g_discovery_page_numrec *
//                              sizeof(struct
//                              spdk_nvmf_discovery_log_page_entry);
//     new_discovery_page = realloc(g_discovery_page, g_discovery_page_size);
//     if (new_discovery_page == NULL) {
//         free(g_discovery_page);
//         printf("Discovery page allocation failed!\n");
//         return;
//     }
//
//     g_discovery_page = new_discovery_page;
//
//     /* Retrieve the rest of the discovery log page */
//     offset = offsetof(struct spdk_nvmf_discovery_log_page, entries);
//     remaining = g_discovery_page_size - offset;
//     while (remaining) {
//         uint32_t size;
//
//         /* Retrieve up to 4 KB at a time */
//         size = spdk_min(remaining, 4096);
//
//         if (spdk_nvme_ctrlr_cmd_get_log_page(ctrlr, SPDK_NVME_LOG_DISCOVERY,
//         0,
//                                              (char *)g_discovery_page +
//                                              offset, size, offset,
//                                              get_log_page_completion, NULL))
//                                              {
//             printf("spdk_nvme_ctrlr_cmd_get_log_page() failed\n");
//             exit(1);
//         }
//
//         offset += size;
//         remaining -= size;
//         outstanding_commands++;
//     }
// }

// static int get_discovery_log_page(struct spdk_nvme_ctrlr *ctrlr)
// {
//     g_discovery_page_size = sizeof(*g_discovery_page);
//     g_discovery_page = calloc(1, g_discovery_page_size);
//     if (g_discovery_page == NULL) {
//         printf("Discovery log page allocation failed!\n");
//         exit(1);
//     }
//
//     if (spdk_nvme_ctrlr_cmd_get_log_page(
//             ctrlr, SPDK_NVME_LOG_DISCOVERY, 0, g_discovery_page,
//             g_discovery_page_size, 0,
//             get_discovery_log_page_header_completion, ctrlr)) {
//         printf("spdk_nvme_ctrlr_cmd_get_log_page() failed\n");
//         exit(1);
//     }
//
//     return 0;
// }

// static void get_log_pages(struct spdk_nvme_ctrlr *ctrlr)
// {
//     const struct spdk_nvme_ctrlr_data *cdata;
//     outstanding_commands = 0;
//     bool is_discovery = spdk_nvme_ctrlr_is_discovery(ctrlr);
//
//     cdata = spdk_nvme_ctrlr_get_data(ctrlr);
//
//     if (!is_discovery) {
//         /*
//          * Only attempt to retrieve the following log pages
//          * when the NVM subsystem that's being targeted is
//          * NOT the Discovery Controller which only fields
//          * a Discovery Log Page.
//          */
//         if (get_error_log_page(ctrlr) == 0) {
//             outstanding_commands++;
//         } else {
//             printf("Get Error Log Page failed\n");
//         }
//
//         if (get_health_log_page(ctrlr) == 0) {
//             outstanding_commands++;
//         } else {
//             printf("Get Log Page (SMART/health) failed\n");
//         }
//
//         if (get_firmware_log_page(ctrlr) == 0) {
//             outstanding_commands++;
//         } else {
//             printf("Get Log Page (Firmware Slot Information) failed\n");
//         }
//     }
//
//     if (spdk_nvme_ctrlr_is_log_page_supported(
//             ctrlr, SPDK_NVME_LOG_ASYMMETRIC_NAMESPACE_ACCESS)) {
//         /* We always set RGO (Return Groups Only) to 0 in this tool, an ANA
//          * group descriptor is returned only if that ANA group contains
//          * namespaces that are attached to the controller processing the
//          * command, and namespaces attached to the controller shall be
//          members
//          * of an ANA group. Hence the following size should be enough.
//          */
//         g_ana_log_page_size =
//             sizeof(struct spdk_nvme_ana_page) +
//             cdata->nanagrpid * sizeof(struct spdk_nvme_ana_group_descriptor)
//             + cdata->nn * sizeof(uint32_t);
//         g_ana_log_page = calloc(1, g_ana_log_page_size);
//         if (g_ana_log_page == NULL) {
//             exit(1);
//         }
//         if (get_ana_log_page(ctrlr) == 0) {
//             outstanding_commands++;
//         } else {
//             printf("Get Log Page (Asymmetric Namespace Access) failed\n");
//         }
//     }
//     if (cdata->lpa.celp) {
//         if (get_cmd_effects_log_page(ctrlr) == 0) {
//             outstanding_commands++;
//         } else {
//             printf("Get Log Page (Commands Supported and Effects) failed\n");
//         }
//     }
//
//     if (cdata->vid == SPDK_PCI_VID_INTEL) {
//         if (spdk_nvme_ctrlr_is_log_page_supported(ctrlr,
//                                                   SPDK_NVME_INTEL_LOG_SMART))
//                                                   {
//             if (get_intel_smart_log_page(ctrlr) == 0) {
//                 outstanding_commands++;
//             } else {
//                 printf("Get Log Page (Intel SMART/health) failed\n");
//             }
//         }
//         if (spdk_nvme_ctrlr_is_log_page_supported(
//                 ctrlr, SPDK_NVME_INTEL_LOG_TEMPERATURE)) {
//             if (get_intel_temperature_log_page(ctrlr) == 0) {
//                 outstanding_commands++;
//             } else {
//                 printf("Get Log Page (Intel temperature) failed\n");
//             }
//         }
//         if (spdk_nvme_ctrlr_is_log_page_supported(
//                 ctrlr, SPDK_NVME_INTEL_MARKETING_DESCRIPTION)) {
//             if (get_intel_md_log_page(ctrlr) == 0) {
//                 outstanding_commands++;
//             } else {
//                 printf("Get Log Page (Intel Marketing Description)
//                 failed\n");
//             }
//         }
//     }
//
//     if (is_discovery && (get_discovery_log_page(ctrlr) == 0)) {
//         outstanding_commands++;
//     }
//
//     while (outstanding_commands) {
//         spdk_nvme_ctrlr_process_admin_completions(ctrlr);
//     }
// }

static int get_ocssd_chunk_info_log_page(struct spdk_nvme_ns *ns)
{
    struct spdk_nvme_ctrlr *ctrlr = spdk_nvme_ns_get_ctrlr(ns);
    int nsid = spdk_nvme_ns_get_id(ns);
    outstanding_commands = 0;

    if (spdk_nvme_ctrlr_cmd_get_log_page(ctrlr, SPDK_OCSSD_LOG_CHUNK_INFO, nsid,
                                         &g_ocssd_chunk_info_page,
                                         sizeof(g_ocssd_chunk_info_page), 0,
                                         get_log_page_completion, NULL) == 0) {
        outstanding_commands++;
    } else {
        printf("get_ocssd_chunk_info_log_page() failed\n");
        return -1;
    }

    while (outstanding_commands) {
        spdk_nvme_ctrlr_process_admin_completions(ctrlr);
    }

    return 0;
}

static void get_ocssd_geometry(struct spdk_nvme_ns *ns,
                               struct spdk_ocssd_geometry_data *geometry_data)
{
    struct spdk_nvme_ctrlr *ctrlr = spdk_nvme_ns_get_ctrlr(ns);
    int nsid = spdk_nvme_ns_get_id(ns);
    outstanding_commands = 0;

    if (spdk_nvme_ocssd_ctrlr_cmd_geometry(
            ctrlr, nsid, geometry_data, sizeof(*geometry_data),
            get_ocssd_geometry_completion, NULL)) {
        printf("Get OpenChannel SSD geometry failed\n");
        exit(1);
    } else {
        outstanding_commands++;
    }

    while (outstanding_commands) {
        spdk_nvme_ctrlr_process_admin_completions(ctrlr);
    }
}

static void get_zns_zone_report(struct spdk_nvme_ns *ns,
                                struct spdk_nvme_qpair *qpair)
{
    g_nr_zones_requested = spdk_nvme_zns_ns_get_num_zones(ns);
    /*
     * Rather than getting the whole zone report, which could contain thousands
     * of zones, get maximum MAX_ZONE_DESC_ENTRIES, so that we don't flood
     * stdout.
     */
    g_nr_zones_requested =
        spdk_min(g_nr_zones_requested, MAX_ZONE_DESC_ENTRIES);
    outstanding_commands = 0;

    g_zone_report_size =
        sizeof(struct spdk_nvme_zns_zone_report) +
        g_nr_zones_requested * sizeof(struct spdk_nvme_zns_zone_desc);
    g_zone_report =
        (struct spdk_nvme_zns_zone_report *)calloc(1, g_zone_report_size);
    if (g_zone_report == NULL) {
        printf("Zone report allocation failed!\n");
        exit(1);
    }

    log_info("debug: zone report size {}", g_zone_report->nr_zones);
    if (spdk_nvme_zns_report_zones(ns, qpair, g_zone_report, g_zone_report_size,
                                   0, SPDK_NVME_ZRA_LIST_ALL, true,
                                   get_zns_zone_report_completion, NULL)) {
        printf("spdk_nvme_zns_report_zones() failed\n");
        exit(1);
    } else {
        outstanding_commands++;
    }

    while (outstanding_commands) {
        spdk_nvme_qpair_process_completions(qpair, 0);
    }
}

static void print_hex_be(const void *v, size_t size)
{
    const uint8_t *buf = (const uint8_t *)v;
    while (size--) {
        printf("%02X", *buf++);
    }
}

static void print_uint128_hex(uint64_t *v)
{
    unsigned long long lo = v[0], hi = v[1];
    if (hi) {
        printf("0x%llX%016llX", hi, lo);
    } else {
        printf("0x%llX", lo);
    }
}

static void print_uint128_dec(uint64_t *v)
{
    unsigned long long lo = v[0], hi = v[1];
    if (hi) {
        /* can't handle large (>64-bit) decimal values for now, so fall back to
         * hex */
        print_uint128_hex(v);
    } else {
        printf("%llu", (unsigned long long)lo);
    }
}

/* The len should be <= 8. */
static void print_uint_var_dec(uint8_t *array, unsigned int len)
{
    uint64_t result = 0;
    int i = len;

    while (i > 0) {
        result += (uint64_t)array[i - 1] << (8 * (i - 1));
        i--;
    }
    printf("%lu", result);
}

/* Print ASCII string as defined by the NVMe spec */
// static void print_ascii_string(const void *buf, size_t size)
// {
//     const uint8_t *str = buf;
//
//     /* Trim trailing spaces */
//     while (size > 0 && str[size - 1] == ' ') {
//         size--;
//     }
//
//     while (size--) {
//         if (*str >= 0x20 && *str <= 0x7E) {
//             printf("%c", *str);
//         } else {
//             printf(".");
//         }
//         str++;
//     }
// }

static void print_zns_zone_report(void)
{
    uint64_t i;

    printf("NVMe ZNS Zone Report Glance\n");
    printf("===========================\n");

    log_info("debug: nr zones {}", g_zone_report->nr_zones);
    for (i = 0; i < g_zone_report->nr_zones; i++) {
        struct spdk_nvme_zns_zone_desc *desc = &g_zone_report->descs[i];
        printf("Zone: %" PRIu64 " ZSLBA: 0x%016" PRIx64 " ZCAP: 0x%016" PRIx64
               " WP: 0x%016" PRIx64 " ZS: %x ZT: %x ZA: %x\n",
               i, desc->zslba, desc->zcap, desc->wp, desc->zs, desc->zt,
               desc->za.raw);
    }
    free(g_zone_report);
    g_zone_report = NULL;
}

static void print_zns_current_zone_report(uint64_t current_zone)
{

    printf("NVMe ZNS Zone Report Glance\n");
    printf("===========================\n");

    // log_info("debug: nr zones {}", g_zone_report->nr_zones);
    log_info("debug: current zone {}", current_zone);
    struct spdk_nvme_zns_zone_desc *desc = &g_zone_report->descs[current_zone];
    printf("Zone: %" PRIu64 " ZSLBA: 0x%016" PRIx64 " ZCAP: 0x%016" PRIx64
           " WP: 0x%016" PRIx64 " ZS: %x ZT: %x ZA: %x\n",
           current_zone, desc->zslba, desc->zcap, desc->wp, desc->zs, desc->zt,
           desc->za.raw);
    free(g_zone_report);
    g_zone_report = NULL;
}

static void print_zns_ns_data(const struct spdk_nvme_zns_ns_data *nsdata_zns)
{
    printf("ZNS Specific Namespace Data\n");
    printf("===========================\n");
    printf("Variable Zone Capacity:                %s\n",
           nsdata_zns->zoc.variable_zone_capacity ? "Yes" : "No");
    printf("Zone Active Excursions:                %s\n",
           nsdata_zns->zoc.zone_active_excursions ? "Yes" : "No");
    printf("Read Across Zone Boundaries:           %s\n",
           nsdata_zns->ozcs.read_across_zone_boundaries ? "Yes" : "No");
    if (nsdata_zns->mar == 0xffffffff) {
        printf("Max Active Resources:                  No Limit\n");
    } else {
        printf("Max Active Resources:                  %" PRIu32 "\n",
               nsdata_zns->mar + 1);
    }
    if (nsdata_zns->mor == 0xffffffff) {
        printf("Max Open Resources:                    No Limit\n");
    } else {
        printf("Max Open Resources:                    %" PRIu32 "\n",
               nsdata_zns->mor + 1);
    }
    if (nsdata_zns->rrl == 0) {
        printf("Reset Recommended Limit:               Not Reported\n");
    } else {
        printf("Reset Recommended Limit:               %" PRIu32 "\n",
               nsdata_zns->rrl);
    }
    if (nsdata_zns->frl == 0) {
        printf("Finish Recommended Limit:              Not Reported\n");
    } else {
        printf("Finish Recommended Limit:              %" PRIu32 "\n",
               nsdata_zns->frl);
    }
    printf("\n");
}

static void print_namespace(struct spdk_nvme_ctrlr *ctrlr,
                            struct spdk_nvme_ns *ns, uint64_t current_zone)
{
    const struct spdk_nvme_ctrlr_data *cdata;
    const struct spdk_nvme_ns_data *nsdata;
    const struct spdk_nvme_zns_ns_data *nsdata_zns;
    const struct spdk_uuid *uuid;
    uint32_t i;
    uint32_t flags;
    char uuid_str[SPDK_UUID_STRING_LEN];
    uint32_t blocksize;

    cdata = spdk_nvme_ctrlr_get_data(ctrlr);
    nsdata = spdk_nvme_ns_get_data(ns);
    nsdata_zns = spdk_nvme_zns_ns_get_data(ns);
    flags = spdk_nvme_ns_get_flags(ns);

    printf("Namespace ID:%d\n", spdk_nvme_ns_get_id(ns));

    if (g_hex_dump) {
        hex_dump(nsdata, sizeof(*nsdata));
        printf("\n");
    }

    /* This function is only called for active namespaces. */
    assert(spdk_nvme_ns_is_active(ns));

    printf("Deallocate:                            %s\n",
           (flags & SPDK_NVME_NS_DEALLOCATE_SUPPORTED) ? "Supported"
                                                       : "Not Supported");
    printf("Deallocated/Unwritten Error:           %s\n",
           nsdata->nsfeat.dealloc_or_unwritten_error ? "Supported"
                                                     : "Not Supported");
    printf("Deallocated Read Value:                %s\n",
           nsdata->dlfeat.bits.read_value == SPDK_NVME_DEALLOC_READ_00
               ? "All 0x00"
           : nsdata->dlfeat.bits.read_value == SPDK_NVME_DEALLOC_READ_FF
               ? "All 0xFF"
               : "Unknown");
    printf("Deallocate in Write Zeroes:            %s\n",
           nsdata->dlfeat.bits.write_zero_deallocate ? "Supported"
                                                     : "Not Supported");
    printf("Deallocated Guard Field:               %s\n",
           nsdata->dlfeat.bits.guard_value ? "CRC for Read Value" : "0xFFFF");
    printf("Flush:                                 %s\n",
           (flags & SPDK_NVME_NS_FLUSH_SUPPORTED) ? "Supported"
                                                  : "Not Supported");
    printf("Reservation:                           %s\n",
           (flags & SPDK_NVME_NS_RESERVATION_SUPPORTED) ? "Supported"
                                                        : "Not Supported");
    if (flags & SPDK_NVME_NS_DPS_PI_SUPPORTED) {
        printf("End-to-End Data Protection:            Supported\n");
        printf("Protection Type:                       Type%d\n",
               nsdata->dps.pit);
        printf("Protection Information Transferred as: %s\n",
               nsdata->dps.md_start ? "First 8 Bytes" : "Last 8 Bytes");
    }
    if (nsdata->lbaf[nsdata->flbas.format].ms > 0) {
        printf("Metadata Transferred as:               %s\n",
               nsdata->flbas.extended ? "Extended Data LBA"
                                      : "Separate Metadata Buffer");
    }
    printf("Namespace Sharing Capabilities:        %s\n",
           nsdata->nmic.can_share ? "Multiple Controllers" : "Private");
    blocksize = 1 << nsdata->lbaf[nsdata->flbas.format].lbads;
    printf("Size (in LBAs):                        %lld (%lldGiB)\n",
           (long long)nsdata->nsze,
           (long long)nsdata->nsze * blocksize / 1024 / 1024 / 1024);
    printf("Capacity (in LBAs):                    %lld (%lldGiB)\n",
           (long long)nsdata->ncap,
           (long long)nsdata->ncap * blocksize / 1024 / 1024 / 1024);
    printf("Utilization (in LBAs):                 %lld (%lldGiB)\n",
           (long long)nsdata->nuse,
           (long long)nsdata->nuse * blocksize / 1024 / 1024 / 1024);
    if (nsdata->noiob) {
        printf("Optimal I/O Boundary:                  %u blocks\n",
               nsdata->noiob);
    }
    if (!spdk_mem_all_zero(nsdata->nguid, sizeof(nsdata->nguid))) {
        printf("NGUID:                                 ");
        print_hex_be(nsdata->nguid, sizeof(nsdata->nguid));
        printf("\n");
    }
    if (!spdk_mem_all_zero(&nsdata->eui64, sizeof(nsdata->eui64))) {
        printf("EUI64:                                 ");
        print_hex_be(&nsdata->eui64, sizeof(nsdata->eui64));
        printf("\n");
    }
    uuid = spdk_nvme_ns_get_uuid(ns);
    if (uuid) {
        spdk_uuid_fmt_lower(uuid_str, sizeof(uuid_str), uuid);
        printf("UUID:                                  %s\n", uuid_str);
    }
    printf("Thin Provisioning:                     %s\n",
           nsdata->nsfeat.thin_prov ? "Supported" : "Not Supported");
    printf("Per-NS Atomic Units:                   %s\n",
           nsdata->nsfeat.ns_atomic_write_unit ? "Yes" : "No");
    if (nsdata->nsfeat.ns_atomic_write_unit) {
        if (nsdata->nawun) {
            printf("  Atomic Write Unit (Normal):          %d\n",
                   nsdata->nawun + 1);
        }

        if (nsdata->nawupf) {
            printf("  Atomic Write Unit (PFail):           %d\n",
                   nsdata->nawupf + 1);
        }

        if (nsdata->nacwu) {
            printf("  Atomic Compare & Write Unit:         %d\n",
                   nsdata->nacwu + 1);
        }

        printf("  Atomic Boundary Size (Normal):       %d\n", nsdata->nabsn);
        printf("  Atomic Boundary Size (PFail):        %d\n", nsdata->nabspf);
        printf("  Atomic Boundary Offset:              %d\n", nsdata->nabo);
    }

    printf("NGUID/EUI64 Never Reused:              %s\n",
           nsdata->nsfeat.guid_never_reused ? "Yes" : "No");

    if (cdata->cmic.ana_reporting) {
        printf("ANA group ID:                          %u\n", nsdata->anagrpid);
    }

    printf("Number of LBA Formats:                 %d\n", nsdata->nlbaf + 1);
    printf("Current LBA Format:                    LBA Format #%02d\n",
           nsdata->flbas.format);
    for (i = 0; i <= nsdata->nlbaf; i++) {
        printf("LBA Format #%02d: Data Size: %5d  Metadata Size: %5d\n", i,
               1 << nsdata->lbaf[i].lbads, nsdata->lbaf[i].ms);
        if (spdk_nvme_ns_get_csi(ns) == SPDK_NVME_CSI_ZNS) {
            printf("LBA Format Extension #%02d: Zone Size (in LBAs): 0x%" PRIx64
                   " Zone Descriptor Extension Size: %d bytes\n",
                   i, nsdata_zns->lbafe[i].zsze,
                   nsdata_zns->lbafe[i].zdes << 6);
        }
    }
    printf("\n");

    if (spdk_nvme_ctrlr_is_ocssd_supported(ctrlr)) {
        get_ocssd_geometry(ns, &geometry_data);
        // print_ocssd_geometry(&geometry_data);
        get_ocssd_chunk_info_log_page(ns);
        // print_ocssd_chunk_info(g_ocssd_chunk_info_page,
        // NUM_CHUNK_INFO_ENTRIES);
    } else if (spdk_nvme_ns_get_csi(ns) == SPDK_NVME_CSI_ZNS) {
        struct spdk_nvme_qpair *qpair =
            spdk_nvme_ctrlr_alloc_io_qpair(ctrlr, NULL, 0);
        if (qpair == NULL) {
            printf("ERROR: spdk_nvme_ctrlr_alloc_io_qpair() failed\n");
            exit(1);
        }
        print_zns_ns_data(nsdata_zns);
        get_zns_zone_report(ns, qpair);

        print_zns_zone_report();
        // print_zns_current_zone_report(current_zone);

        spdk_nvme_ctrlr_free_io_qpair(qpair);
    }
}
