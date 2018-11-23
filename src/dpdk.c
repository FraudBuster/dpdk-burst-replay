/*
  SPDX-License-Identifier: BSD-3-Clause
  Copyright 2018 Jonathan Ribas, FraudBuster. All rights reserved.
*/

#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

/* DPDK includes */
#include <rte_version.h>
#include <rte_ethdev.h>

#include "main.h"

static struct rte_eth_conf ethconf = {
#ifdef RTE_VER_YEAR
    /* version  > to 2.2.0, last one with old major.minor.patch system */
    .link_speeds = ETH_LINK_SPEED_AUTONEG,
#else
    /* compatibility with older version */
    .link_speed = 0,        // autonegociated speed link
    .link_duplex = 0,       // autonegociated link mode
#endif
    .rxmode = {
        // Multi queue packet routing mode. We wont use DPDK RSS scaling for now,
        // we will use our own ashkey
        .mq_mode = ETH_MQ_RX_NONE,
        // Default maximum frame length. Whenever this is > ETHER_MAX_LEN,
        // jumbo_frame has to be set to 1
        .max_rx_pkt_len = 9000,
        .split_hdr_size = 0,        // Disable header split
        .header_split = 0,          // Disable header split
        .hw_ip_checksum = 0,        // Disable ip checksum
        .hw_vlan_filter = 0,        // Disable vlan filtering
        .jumbo_frame = 1,           // Enable Jumbo frame
        .hw_strip_crc = 0,          // Disable hardware CRC stripping
    },

    .txmode = {
        .mq_mode = ETH_MQ_TX_NONE,  // Multi queue packet routing mode. We wont use
        // DPDK RSS scaling for now, we will use our own ashkey
    },

    .fdir_conf = {
        .mode = RTE_FDIR_MODE_NONE, // Disable flow director support
    },

    .intr_conf = {
        .lsc = 0,                   // Disable lsc interrupts
    },
};

static struct rte_eth_txconf const txconf = {
    .tx_thresh = {
        .pthresh = TX_PTHRESH,
        .hthresh = TX_HTHRESH,
        .wthresh = TX_WTHRESH,
    },
    .tx_free_thresh = 32,
};

void* myrealloc(void* ptr, size_t new_size)
{
    void* res = realloc(ptr, new_size);
    if (!res && ptr)
        free(ptr);
    return (res);
}

char** fill_eal_args(const struct cmd_opts* opts, const struct cpus_bindings* cpus,
                     const struct dpdk_ctx* dpdk, int* eal_args_ac)
{
    char    buf_coremask[30];
    char    socket_mem_arg[32];
    char**  eal_args;
    int     i, cpt, mempool;

    if (!opts || !cpus || !dpdk)
        return (NULL);

    mempool = dpdk->pool_sz * 1024;
    /* Set EAL init parameters */
    snprintf(buf_coremask, 20, "0x%lx", cpus->coremask);
    if (cpus->numacores == 1)
        snprintf(socket_mem_arg, sizeof(socket_mem_arg), "--socket-mem=%i", mempool);
    else if (cpus->numacore == 0)
        snprintf(socket_mem_arg, sizeof(socket_mem_arg), "--socket-mem=%i,0", mempool);
    else
        snprintf(socket_mem_arg, sizeof(socket_mem_arg), "--socket-mem=0,%i", mempool);
    char *pre_eal_args[] = {
        "./dpdk-replay",
        "-c", strdup(buf_coremask),
        "-n", "1", /* NUM MEM CHANNELS */
        "--proc-type", "auto",
        "--file-prefix", "dpdkreplay_",
        strdup(socket_mem_arg),
        NULL
    };
    /* fill pci whitelist args */
    eal_args = malloc(sizeof(*eal_args) * sizeof(pre_eal_args));
    if (!eal_args)
        return (NULL);
    memcpy(eal_args, (char**)pre_eal_args, sizeof(pre_eal_args));
    cpt = sizeof(pre_eal_args) / sizeof(*pre_eal_args);
    for (i = 0; opts->pcicards[i]; i++) {
        eal_args = myrealloc(eal_args, sizeof(char*) * (cpt + 2));
        if (!eal_args)
            return (NULL);
        eal_args[cpt - 1] = "--pci-whitelist"; /* overwrite "NULL" */
        eal_args[cpt] = opts->pcicards[i];
        eal_args[cpt + 1] = NULL;
        cpt += 2;
    }
    *eal_args_ac = cpt - 1;
    return (eal_args);
}

int dpdk_init_port(const struct cpus_bindings* cpus, int port)
{
    int                 ret, i;
#ifdef DEBUG
    struct rte_eth_link eth_link;
#endif /* DEBUG */

    if (!cpus)
        return (EINVAL);

    /* Configure for each port (ethernet device), the number of rx queues & tx queues */
    if (rte_eth_dev_configure(port,
                              0, /* nb rx queue */
                              NB_TX_QUEUES, /* nb tx queue */
                              &ethconf) < 0) {
        fprintf(stderr, "DPDK: RTE ETH Ethernet device configuration failed\n");
        return (-1);
    }

    /* Then allocate and set up the transmit queues for this Ethernet device  */
    for (i = 0; i < NB_TX_QUEUES; i++) {
        ret = rte_eth_tx_queue_setup(port,
                                     i,
                                     TX_QUEUE_SIZE,
                                     cpus->numacore,
                                     &txconf);
        if (ret < 0) {
            fprintf(stderr, "DPDK: RTE ETH Ethernet device tx queue %i setup failed: %s",
                    i, strerror(-ret));
            return (ret);
        }
    }

    /* Start the ethernet device */
    if (rte_eth_dev_start(port) < 0) {
        fprintf(stderr, "DPDK: RTE ETH Ethernet device start failed\n");
        return (-1);
    }

#ifdef DEBUG
    /* Get link status and display it. */
    rte_eth_link_get(port, &eth_link);
    if (eth_link.link_status) {
        printf(" Link up - speed %u Mbps - %s\n",
               eth_link.link_speed,
               (eth_link.link_duplex == ETH_LINK_FULL_DUPLEX) ?
               "full-duplex" : "half-duplex\n");
    } else {
        printf("Link down\n");
    }
#endif /* DEBUG */
    return (0);
}

int init_dpdk_eal_mempool(const struct cmd_opts* opts,
                          const struct cpus_bindings* cpus,
                          struct dpdk_ctx* dpdk)
{
    char**          eal_args;
    int             eal_args_ac = 0;
    unsigned int    nb_ports;
    int             ret;

    if (!opts || !cpus || !dpdk)
        return (EINVAL);

    /* craft an eal arg list */
    eal_args = fill_eal_args(opts, cpus, dpdk, &eal_args_ac);
    if (!eal_args) {
        printf("%s: fill_eal_args failed.\n", __FUNCTION__);
        return (1);
    }

#ifdef DEBUG
    puts("EAL ARGS:");
    for (int i = 0; eal_args[i]; i++)
        printf("eal_args[%i] = %s\n", i, eal_args[i]);
#endif /* DEBUG */

    /* DPDK RTE EAL INIT */
    ret = rte_eal_init(eal_args_ac, eal_args);
    free(eal_args);
    if (ret < 0) {
        printf("%s: rte_eal_init failed (%d)\n", __FUNCTION__, ret);
        return (ret);
    }

    /* check that dpdk see enough usable cores */
    if (rte_lcore_count() != cpus->nb_needed_cpus + 1) {
        printf("%s error: not enough rte_lcore founds\n", __FUNCTION__);
        return (1);
    }

    /* check that dpdk detects all wanted/needed NIC ports */
    nb_ports = rte_eth_dev_count();
    if (nb_ports != cpus->nb_needed_cpus) {
        printf("%s error: wanted %u NIC ports, found %u\n", __FUNCTION__,
               cpus->nb_needed_cpus, nb_ports);
        return (1);
    }

    printf("-> Create mempool of %u mbufs of %u octs.\n",
           dpdk->nb_mbuf, dpdk->mbuf_sz);
    dpdk->pktmbuf_pool = rte_mempool_create("dpdk_replay_mempool",
                                            dpdk->nb_mbuf,
                                            dpdk->mbuf_sz,
                                            MBUF_CACHE_SZ,
                                            sizeof(struct rte_pktmbuf_pool_private),
                                            rte_pktmbuf_pool_init, NULL,
                                            rte_pktmbuf_init, NULL,
                                            cpus->numacore,
                                            0);
    if (dpdk->pktmbuf_pool == NULL) {
        fprintf(stderr, "DPDK: RTE Mempool creation failed\n");
        return (1);
    }
    return (0);
}

int init_dpdk_ports(struct cpus_bindings* cpus)
{
    int i;
    int numa;

    if (!cpus)
        return (EINVAL);

    for (i = 0; (unsigned)i < cpus->nb_needed_cpus; i++) {
        /* if the port ID isn't on the good numacore, exit */
        numa = rte_eth_dev_socket_id(i);
        if (numa != cpus->numacore) {
            fprintf(stderr, "port %i is not on the good numa id (%i).\n", i, numa);
            return (1);
        }
        /* init ports */
        if (dpdk_init_port(cpus, i))
            return (1);
        printf("-> NIC port %i ready.\n", i);
    }
    return (0);
}

double timespec_diff_to_double(const struct timespec start, const struct timespec end)
{
    struct timespec diff;
    double duration;

    diff.tv_sec = end.tv_sec - start.tv_sec;
    if (end.tv_nsec > start.tv_nsec)
        diff.tv_nsec = end.tv_nsec - start.tv_nsec;
    else {
        diff.tv_nsec = end.tv_nsec - start.tv_nsec + 1000000000;
        diff.tv_sec--;
    }
    duration = diff.tv_sec + ((double)diff.tv_nsec / 1000000000);
    return (duration);
}

int tx_thread(void* thread_ctx)
{
    struct thread_ctx*  ctx;
    struct rte_mbuf**   mbuf;
    struct timespec     start, end;
    unsigned int        tx_queue;
    int                 ret, thread_id, index, i, run_cpt, retry_tx;
    int                 nb_sent, to_sent, total_to_sent, total_sent;
    int                 nb_drop;

    if (!thread_ctx)
        return (EINVAL);

    /* retrieve thread context */
    ctx = (struct thread_ctx*)thread_ctx;
    thread_id = ctx->tx_port_id;
    mbuf = ctx->pcap_cache->mbufs;
    printf("Starting thread %i.\n", thread_id);

    /* init semaphore to wait to start the burst */
    ret = sem_wait(ctx->sem);
    if (ret) {
        fprintf(stderr, "sem_wait failed on thread %i: %s\n",
                thread_id, strerror(ret));
        return (ret);
    }

    /* get the start time */
    ret = clock_gettime(CLOCK_MONOTONIC, &start);
    if (ret) {
        fprintf(stderr, "clock_gettime failed on start for thread %i: %s\n",
                thread_id, strerror(errno));
        return (errno);
    }

    /* iterate on each wanted runs */
    for (run_cpt = ctx->nbruns, tx_queue = ctx->total_drop = ctx->total_drop_sz = 0;
         run_cpt;
         ctx->total_drop += nb_drop, run_cpt--) {
        /* iterate on pkts for every batch of BURST_SZ number of packets */
        for (total_to_sent = ctx->nb_pkt, nb_drop = 0, to_sent = min(BURST_SZ, total_to_sent);
             to_sent;
             total_to_sent -= to_sent, to_sent = min(BURST_SZ, total_to_sent)) {
            /* calculate the mbuf index for the current batch */
            index = ctx->nb_pkt - total_to_sent;

            /* send the burst batch, and retry NB_RETRY_TX times if we */
            /* didn't success to sent all the wanted batch */
            for (total_sent = 0, retry_tx = NB_RETRY_TX;
                 total_sent < to_sent && retry_tx;
                 total_sent += nb_sent, retry_tx--) {
                nb_sent = rte_eth_tx_burst(ctx->tx_port_id,
                                           (tx_queue++ % NB_TX_QUEUES),
                                           &(mbuf[index + total_sent]),
                                           to_sent - total_sent);
                if (retry_tx != NB_RETRY_TX &&
                    tx_queue % NB_TX_QUEUES == 0)
                    usleep(100);
            }
            /* free unseccessfully sent  */
            if (unlikely(!retry_tx))
                for (i = total_sent; i < to_sent; i++) {
                    nb_drop++;
                    ctx->total_drop_sz += mbuf[index + i]->pkt_len;
                    rte_pktmbuf_free(mbuf[index + i]);
                }
        }
#ifdef DEBUG
        if (unlikely(nb_drop))
            printf("[thread %i]: on loop %i: sent %i pkts (%i were dropped).\n",
                   thread_id, ctx->nbruns - run_cpt, ctx->nb_pkt, nb_drop);
#endif /* DEBUG */
    }

    /* get the ends time and calculate the duration */
    ret = clock_gettime(CLOCK_MONOTONIC, &end);
    if (ret) {
        fprintf(stderr, "clock_gettime failed on finish for thread %i: %s\n",
                thread_id, strerror(errno));
        return (errno);
    }
    ctx->duration = timespec_diff_to_double(start, end);
    printf("Exiting thread %i properly.\n", thread_id);
    return (0);
}

int process_result_stats(const struct cpus_bindings* cpus,
                         const struct dpdk_ctx* dpdk,
                         const struct cmd_opts* opts,
                         const struct thread_ctx* ctx)
{
    double              pps, bitrate;
    double              total_pps, total_bitrate;
    unsigned long int   total_pkt_sent, total_pkt_sent_sz;
    unsigned int        i, total_drop, total_pkt;

    if (!cpus || !dpdk || !opts || !ctx)
        return (EINVAL);

    total_pps = total_bitrate = 0;
    total_drop = 0;
    puts("\nRESULTS :");
    for (i = 0; i < cpus->nb_needed_cpus; i++) {
        total_pkt_sent = (ctx[i].nb_pkt * opts->nbruns) - ctx[i].total_drop;
        total_pkt_sent_sz = (dpdk->pcap_sz * opts->nbruns) - ctx[i].total_drop_sz;
        pps = total_pkt_sent / ctx[i].duration;
        bitrate = total_pkt_sent_sz / ctx[i].duration
            * 8 /* Bytes to bits */
            / 1024 /* bits to Kbits */
            / 1024 /* Kbits to Mbits */
            / 1024; /* Mbits to Gbits */
        total_bitrate += bitrate;
        total_pps += pps;
        total_drop += ctx[i].total_drop;
        printf("[thread %02u]: %f Gbit/s, %f pps on %f sec (%u pkts dropped)\n",
               i, bitrate, pps, ctx[i].duration, ctx[i].total_drop);
    }
    puts("-----");
    printf("TOTAL        : %.3f Gbit/s. %.3f pps.\n", total_bitrate, total_pps);
    total_pkt = ctx[0].nb_pkt * opts->nbruns * cpus->nb_needed_cpus;
    printf("Total dropped: %u/%u packets (%f%%)\n", total_drop, total_pkt,
           (double)(total_drop * 100) / (double)(total_pkt));
    return (0);
}

int start_tx_threads(const struct cmd_opts* opts,
                     const struct cpus_bindings* cpus,
                     const struct dpdk_ctx* dpdk,
                     const struct pcap_ctx* pcap)
{
    struct thread_ctx* ctx = NULL;
    sem_t sem;
    unsigned int i;
    int ret;

    /* init semaphore for synchronous threads startup */
    if (sem_init(&sem, 0, 0)) {
        fprintf(stderr, "sem_init failed: %s\n", strerror(errno));
        return (errno);
    }

    /* create threads contexts */
    ctx = malloc(sizeof(*ctx) * cpus->nb_needed_cpus);
    if (!ctx)
        return (ENOMEM);
    bzero(ctx, sizeof(*ctx) * cpus->nb_needed_cpus);
    for (i = 0; i < cpus->nb_needed_cpus; i++) {
        ctx[i].sem = &sem;
        ctx[i].tx_port_id = i;
        ctx[i].nbruns = opts->nbruns;
        ctx[i].pcap_cache = &(dpdk->pcap_caches[i]);
        ctx[i].nb_pkt = pcap->nb_pkts;
        ctx[i].nb_tx_queues = NB_TX_QUEUES;
    }

    /* launch threads, which will wait on the semaphore to start */
    for (i = 0; i < cpus->nb_needed_cpus; i++) {
        ret = rte_eal_remote_launch(tx_thread, &(ctx[i]),
                                    cpus->cpus_to_use[i + 1]); /* skip fake master core */
        if (ret) {
            fprintf(stderr, "rte_eal_remote_launch failed: %s\n", strerror(ret));
            free(ctx);
            return (ret);
        }
    }

    /* wait for ENTER and starts threads */
    puts("Threads are ready to be launched, please press ENTER to start sending packets.");
    for (ret = getchar(); ret != '\n'; ret = getchar()) ;
    for (i = 0; i < cpus->nb_needed_cpus; i++) {
        ret = sem_post(&sem);
        if (ret) {
            fprintf(stderr, "sem_post failed: %s\n", strerror(errno));
            free(ctx);
            return (errno);
        }
    }

    /* wait all threads */
    rte_eal_mp_wait_lcore();

    /* get results */
    ret = process_result_stats(cpus, dpdk, opts, ctx);
    free(ctx);
    return (ret);
}

void dpdk_cleanup(struct dpdk_ctx* dpdk, struct cpus_bindings* cpus)
{
    unsigned int i;

    /* free caches */
    if (dpdk->pcap_caches) {
        for (i = 0; i < cpus->nb_needed_cpus; i++)
            free(dpdk->pcap_caches[i].mbufs);
        free(dpdk->pcap_caches);
    }

    /* close ethernet devices */
    for (i = 0; i < cpus->nb_needed_cpus; i++)
        rte_eth_dev_close(i);

    /* free mempool */
    if (dpdk->pktmbuf_pool)
        rte_mempool_free(dpdk->pktmbuf_pool);
    return ;
}
