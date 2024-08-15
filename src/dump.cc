static void __m_append_complete(void *ctx, const struct spdk_nvme_cpl *cpl)
{
    struct ns_worker_ctx *ns_ctx = static_cast<struct ns_worker_ctx *>(ctx);

    if (spdk_nvme_cpl_is_error(cpl)) {
        spdk_nvme_qpair_print_completion(ns_ctx->qpair,
                                         (struct spdk_nvme_cpl *)cpl);
        log_error("Completion failed {}",
                  spdk_nvme_cpl_get_status_string(&cpl->status));
        return;
    } else {
        ns_ctx->etime = std::chrono::high_resolution_clock::now();
        ns_ctx->etimes.push_back(ns_ctx->etime);
    }
    if (ns_ctx->current_lba == 0) {
        log_info("setting current lba value: {}", cpl->cdw0);
        ns_ctx->current_lba = cpl->cdw0;
    }
}

static void __m_complete(void *ctx, const struct spdk_nvme_cpl *cpl)
{
    struct ns_worker_ctx *ns_ctx = static_cast<struct ns_worker_ctx *>(ctx);

    if (spdk_nvme_cpl_is_error(cpl)) {
        spdk_nvme_qpair_print_completion(ns_ctx->qpair,
                                         (struct spdk_nvme_cpl *)cpl);
        log_error("Completion failed {}",
                  spdk_nvme_cpl_get_status_string(&cpl->status));
        return;
    } else {
        ns_ctx->etime = std::chrono::high_resolution_clock::now();
        ns_ctx->etimes.push_back(ns_ctx->etime);
    }
}

int measure_read(struct ns_worker_ctx *ns_ctx, uint64_t slba, void *buffer,
                 uint64_t size)
{
    ERROR_ON_NULL(ns_ctx->qpair, 1);
    ERROR_ON_NULL(buffer, 1);
    int rc = 0;

    int lbas = (size + ns_ctx->info.lba_size - 1) / ns_ctx->info.lba_size;
    int lbas_processed = 0;
    int step_size = (ns_ctx->info.mdts / ns_ctx->info.lba_size);
    int current_step_size = step_size;
    int slba_start = slba;
    if (ns_ctx->verbose)
        log_info("\nmeasure_read: lbas {}, step size {}, slba start {} \n",
                 lbas, current_step_size, slba_start);

    while (lbas_processed < lbas) {
        if ((slba + lbas_processed + step_size) / ns_ctx->info.zone_size >
            (slba + lbas_processed) / ns_ctx->info.zone_size) {
            current_step_size =
                ((slba + lbas_processed + step_size) / ns_ctx->info.zone_size) *
                    ns_ctx->info.zone_size -
                lbas_processed - slba;
        } else {
            current_step_size = step_size;
        }
        current_step_size = lbas - lbas_processed > current_step_size
                                ? current_step_size
                                : lbas - lbas_processed;
        if (ns_ctx->verbose)
            log_info("{} step {}  \n", slba_start, current_step_size);
        if (ns_ctx->verbose)
            log_info(
                "cmd_read: slba_start {}, current step size {}, queued {} ",
                slba_start, current_step_size, ns_ctx->current_queue_depth);
        ns_ctx->stime = std::chrono::high_resolution_clock::now();
        ns_ctx->stimes.push_back(ns_ctx->stime);
        rc = spdk_nvme_ns_cmd_read(ns_ctx->entry->nvme.ns, ns_ctx->qpair,
                                   (char *)buffer +
                                       lbas_processed * ns_ctx->info.lba_size,
                                   slba_start,        /* LBA start */
                                   current_step_size, /* number of LBAs */
                                   __m_complete, ns_ctx, 0);
        if (rc != 0) {
            // log_error("cmd read error: {}", rc);
            // if (rc == -ENOMEM) {
            //     spdk_nvme_qpair_process_completions(ctx->qpair, 0);
            //     rc = 0;
            // } else
            return 1;
        }

        lbas_processed += current_step_size;
        slba_start = slba + lbas_processed;

        // while (ns_ctx->current_queue_depth) {
        //     ret = spdk_nvme_qpair_process_completions(ns_ctx->qpair, 0);
        //     ns_ctx->num_queued -= ret;
        // }
    }

    return rc;
}

int measure_append(struct ns_worker_ctx *ns_ctx, uint64_t slba, void *buffer,
                   uint64_t size)
{
    if (ns_ctx->verbose)
        log_info("\n\nmeasure_append start: slba {}, size {}\n", slba, size);
    ERROR_ON_NULL(ns_ctx->qpair, 1);
    ERROR_ON_NULL(buffer, 1);

    int rc = 0;

    int lbas = (size + ns_ctx->info.lba_size - 1) / ns_ctx->info.lba_size;
    int lbas_processed = 0;
    int step_size = (ns_ctx->info.zasl / ns_ctx->info.lba_size);
    int current_step_size = step_size;
    int slba_start = (slba / ns_ctx->info.zone_size) * ns_ctx->info.zone_size;
    if (ns_ctx->verbose)
        log_info("measure_append: lbas {}, step size {}, slba start {} ", lbas,
                 current_step_size, slba_start);

    while (lbas_processed < lbas) {
        // Completion completion = {.done = false, .err = 0};
        if ((slba + lbas_processed + step_size) / ns_ctx->info.zone_size >
            (slba + lbas_processed) / ns_ctx->info.zone_size) {
            current_step_size =
                ((slba + lbas_processed + step_size) / ns_ctx->info.zone_size) *
                    ns_ctx->info.zone_size -
                lbas_processed - slba;
        } else {
            current_step_size = step_size;
        }
        current_step_size = lbas - lbas_processed > current_step_size
                                ? current_step_size
                                : lbas - lbas_processed;
        if (ns_ctx->verbose)
            log_info(
                "zone_append: slba start {}, current step size {}, queued {}",
                slba_start, current_step_size, ns_ctx->current_queue_depth);
        ns_ctx->stime = std::chrono::high_resolution_clock::now();
        ns_ctx->stimes.push_back(ns_ctx->stime);
        rc = spdk_nvme_zns_zone_append(
            ns_ctx->entry->nvme.ns, ns_ctx->qpair,
            (char *)buffer + lbas_processed * ns_ctx->info.lba_size,
            slba_start,        /* LBA start */
            current_step_size, /* number of LBAs */
            __m_append_complete, ns_ctx, 0);
        if (rc != 0) {
            log_error("zone append error: {}", rc);
            // if (rc == -ENOMEM) {
            //     spdk_nvme_qpair_process_completions(ctx->qpair, 0);
            //     rc = 0;
            // } else
            break;
        }

        lbas_processed += current_step_size;
        slba_start = ((slba + lbas_processed) / ns_ctx->info.zone_size) *
                     ns_ctx->info.zone_size;
    }
    // while (ns_ctx->num_queued) {
    //     // log_debug("GOOD: qpair process completion: queued {}, qd {}",
    //     //           ns_ctx->num_queued, ns_ctx->qd);
    //     ret = spdk_nvme_qpair_process_completions(ns_ctx->qpair, 0);
    //     ns_ctx->num_queued -= ret;
    // }
    return rc;
}

/*
// FIXME:
// DONE: add queue size as qpair options
// DONE: detect current lbas to use for reads
// TODO:
// something smart about open/close zones etc
static void zns_measure(void *arg)
{
    log_info("Fn: zns_measure \n");
    struct ZstoreContext *ctx = static_cast<struct ZstoreContext *>(arg);
    struct spdk_nvme_io_qpair_opts qpair_opts = {};

    // std::vector<int> qds{2, 4, 8, 16, 32, 64};

    // std::vector<int> qds{2}; // 143, 137
    // std::vector<int> qds{4}; //
    // std::vector<int> qds{8}; // 495
    // std::vector<int> qds{16}; //
    std::vector<int> qds{32}; //
    // std::vector<int> qds{64}; // 2345, 4340

    for (auto qd : qds) {
        log_info("\nStarting measurment with queue depth {}, append times {}\n",
                 qd, append_times);
        ctx->qd = qd;
        zns_dev_init(ctx, "192.168.1.121", "4420", "192.168.1.121", "5520");

        // default
        // qpair_opts.io_queue_size = 128;
        // qpair_opts.io_queue_requests = 512;
        qpair_opts.io_queue_size = 64;
        qpair_opts.io_queue_requests = 64 * 4;

        zstore_qpair_setup(ctx, qpair_opts);
        zstore_init(ctx);

        z_get_device_info(&ctx->m1, ctx->verbose);
        z_get_device_info(&ctx->m2, ctx->verbose);
        ctx->m1.zstore_open = true;
        ctx->m2.zstore_open = true;

        ctx->m1.qd = qd;
        ctx->m2.qd = qd;

        // working
        int rc = 0;
        int rc1 = 0;
        int rc2 = 0;

        log_info("writing with z_append:");
        std::vector<void *> wbufs;
        for (int i = 0; i < append_times; i++) {
            char **wbuf = (char **)calloc(1, sizeof(char **));
            rc = write_zstore_pattern(wbuf, &ctx->m1, ctx->m2.info.lba_size,
                                      "testing", value + i);
            assert(rc == 0);
            wbufs.push_back(*wbuf);
        }

        for (int i = 0; i < append_times; i++) {
            // do multple append qd times
            // APPEND
            rc1 = measure_append(&ctx->m1, ctx->m1.zslba, wbufs[i],
                                 ctx->m1.info.lba_size);
            rc2 = measure_append(&ctx->m2, ctx->m2.zslba, wbufs[i],
                                 ctx->m2.info.lba_size);
            assert(rc1 == 0 && rc2 == 0);
            // printf("%d-th round: %d-th is %s\n", i, j, wbufs[i * qd +
            // j]);
        }
        while (ctx->m1.num_queued || ctx->m2.num_queued) {
            log_debug("Reached here to process outstanding requests");
            // log_debug("qpair queued: m1 {}, m2 {}", ctx->m1.num_queued,
            //           ctx->m2.num_queued);
            spdk_nvme_qpair_process_completions(ctx->m1.qpair, 0);
            spdk_nvme_qpair_process_completions(ctx->m2.qpair, 0);
        }
        log_debug("write is all done ");

        std::vector<u64> deltas1;
        std::vector<u64> deltas2;
        for (int i = 0; i < append_times; i++) {
            deltas1.push_back(
                std::chrono::duration_cast<std::chrono::microseconds>(
                    ctx->m1.etimes[i] - ctx->m1.stimes[i])
                    .count());
            deltas2.push_back(
                std::chrono::duration_cast<std::chrono::microseconds>(
                    ctx->m2.etimes[i] - ctx->m2.stimes[i])
                    .count());
        }
        auto sum1 = std::accumulate(deltas1.begin(), deltas1.end(), 0.0);
        auto sum2 = std::accumulate(deltas2.begin(), deltas2.end(), 0.0);
        auto mean1 = sum1 / deltas1.size();
        auto mean2 = sum2 / deltas2.size();
        auto sq_sum1 = std::inner_product(deltas1.begin(), deltas1.end(),
                                          deltas1.begin(), 0.0);
        auto sq_sum2 = std::inner_product(deltas2.begin(), deltas2.end(),
                                          deltas2.begin(), 0.0);
        auto stdev1 = std::sqrt(sq_sum1 / deltas1.size() - mean1 * mean1);
        auto stdev2 = std::sqrt(sq_sum2 / deltas2.size() - mean2 * mean2);
        log_info("WRITES-1 qd {}, append: mean {} us, std {}", ctx->qd, mean1,
                 stdev1);
        log_info("WRITES-2 qd {}, append: mean {} us, std {}", ctx->qd, mean2,
                 stdev2);

        // clearnup
        deltas1.clear();
        deltas2.clear();
        ctx->m1.stimes.clear();
        ctx->m1.etimes.clear();
        ctx->m2.stimes.clear();
        ctx->m2.etimes.clear();

        log_info("current lba for read: device 1 {}, device 2 {}",
                 ctx->m1.current_lba, ctx->m2.current_lba);
        log_info("\nread with z_append:");

        std::vector<int> vec;
        vec.reserve(append_times);
        for (int i = 0; i < append_times; i++) {
            vec.push_back(i);
        }

        auto rng = std::default_random_engine{};
        std::shuffle(std::begin(vec), std::end(vec), rng);

        char *rbuf1 =
            (char *)z_calloc(&ctx->m1, ctx->m1.info.lba_size, sizeof(char *));
        char *rbuf2 =
            (char *)z_calloc(&ctx->m2, ctx->m2.info.lba_size, sizeof(char *));
        for (const auto &i : vec) {
            rc = measure_read(&ctx->m1, ctx->m1.current_lba + i, rbuf1,
                              ctx->m1.info.lba_size);
            assert(rc == 0);
            rc = measure_read(&ctx->m2, ctx->m2.current_lba + i, rbuf2,
                              ctx->m2.info.lba_size);
            assert(rc == 0);
            // printf("%d-th round: %d-th is %s, %s\n", i, j, rbuf1, rbuf2);

            // rc = measure_read(&ctx->m1, ctx->m1.current_lba + i, rbuf1,
            // 4096); assert(rc == 0); rc = measure_read(&ctx->m2,
            // ctx->m2.current_lba + i, rbuf2, 4096); assert(rc == 0);

            // printf("m1: %s, m2: %s\n", rbuf1, rbuf2);
        }
        while (ctx->m1.num_queued || ctx->m2.num_queued) {
            // log_debug("qpair queued: m1 {}, m2 {}",
            // ctx->m1.num_queued,
            //           ctx->m2.num_queued);

            spdk_nvme_qpair_process_completions(ctx->m1.qpair, 0);
            spdk_nvme_qpair_process_completions(ctx->m2.qpair, 0);
        }
        log_debug("m1: {}, {}", ctx->m1.stimes.size(), ctx->m1.etimes.size());
        log_debug("m2: {}, {}", ctx->m2.stimes.size(), ctx->m2.etimes.size());
        for (int i = 0; i < append_times; i++) {
            deltas1.push_back(
                std::chrono::duration_cast<std::chrono::microseconds>(
                    ctx->m1.etimes[i] - ctx->m1.stimes[i])
                    .count());
            deltas2.push_back(
                std::chrono::duration_cast<std::chrono::microseconds>(
                    ctx->m2.etimes[i] - ctx->m2.stimes[i])
                    .count());
        }

        log_debug("deltas: m1 {}, m2 {}", deltas1.size(), deltas2.size());
        sum1 = std::accumulate(deltas1.begin(), deltas1.end(), 0.0);
        sum2 = std::accumulate(deltas2.begin(), deltas2.end(), 0.0);
        mean1 = sum1 / deltas1.size();
        mean2 = sum2 / deltas2.size();
        sq_sum1 = std::inner_product(deltas1.begin(), deltas1.end(),
                                     deltas1.begin(), 0.0);
        sq_sum2 = std::inner_product(deltas2.begin(), deltas2.end(),
                                     deltas2.begin(), 0.0);
        stdev1 = std::sqrt(sq_sum1 / deltas1.size() - mean1 * mean1);
        stdev2 = std::sqrt(sq_sum2 / deltas2.size() - mean2 * mean2);
        log_info("READ-1 qd {}, append: mean {} us, std {}", ctx->qd, mean1,
                 stdev1);
        log_info("READ-2 qd {}, append: mean {} us, std {}", ctx->qd, mean2,
                 stdev2);

        deltas1.clear();
        deltas2.clear();
        // log_info("qd {}, read: mean {} us, std {}", ctx->qd, mean,
        // stdev);

        zstore_qpair_teardown(ctx);
    }

    log_info("Test start finish");
}

*/

/*
    if (ctx->m2.ctrlr == NULL && ctx->verbose) {
        fprintf(stderr,
                "spdk_nvme_connect() failed for transport address '%s'\n",
                ctx->m2.g_trid.traddr);
        spdk_app_stop(-1);
        // pthread_kill(g_fuzz_td, SIGSEGV);
        // return NULL;
        // return rc;
    }

    // SPDK_NOTICELOG("Successfully started the application\n");
    // SPDK_NOTICELOG("Initializing NVMe controller\n");

    if (spdk_nvme_zns_ctrlr_get_data(ctx->m2.ctrlr) && ctx->verbose) {
        printf("ZNS Specific Controller Data\n");
        printf("============================\n");
        printf("Zone Append Size Limit:      %u\n",
               spdk_nvme_zns_ctrlr_get_data(ctx->m2.ctrlr)->zasl);
        printf("\n");
        printf("\n");

        printf("Active Namespaces\n");
        printf("=================\n");
        // for (nsid = spdk_nvme_ctrlr_get_first_active_ns(ctx->ctrlr); nsid !=
        // 0;
        //      nsid = spdk_nvme_ctrlr_get_next_active_ns(ctx->ctrlr, nsid)) {
        //     print_namespace(ctx->ctrlr,
        //                     spdk_nvme_ctrlr_get_ns(ctx->ctrlr, nsid));
        // }
    }
    // ctx->ns = spdk_nvme_ctrlr_get_ns(ctx->ctrlr, 1);

    // NOTE: must find zns ns
    // take any ZNS namespace, we do not care which.
    for (int nsid = spdk_nvme_ctrlr_get_first_active_ns(ctx->m1.ctrlr);
         nsid != 0;
         nsid = spdk_nvme_ctrlr_get_next_active_ns(ctx->m1.ctrlr, nsid)) {

        struct spdk_nvme_ns *ns = spdk_nvme_ctrlr_get_ns(ctx->m1.ctrlr, nsid);
        if (ns == NULL) {
            continue;
        }
        if (spdk_nvme_ns_get_csi(ns) != SPDK_NVME_CSI_ZNS) {
            continue;
        }

        if (ctx->m1.ns == NULL) {
            log_info("Found namespace {}, connect to device manger m1", nsid);
            ctx->m1.ns = ns;
        } else if (ctx->m2.ns == NULL) {
            log_info("Found namespace {}, connect to device manger m2", nsid);
            ctx->m2.ns = ns;

        } else
            break;

        if (ctx->verbose)
            print_namespace(ctx->m1.ctrlr,
                            spdk_nvme_ctrlr_get_ns(ctx->m1.ctrlr, nsid),
                            ctx->current_zone);
    }

    if (ctx->m1.ns == NULL) {
        SPDK_ERRLOG("Could not get NVMe namespace\n");
        spdk_app_stop(-1);
        return;
    }
    */