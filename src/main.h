/*
  SPDX-License-Identifier: BSD-3-Clause
  Copyright 2018 Jonathan Ribas, FraudBuster. All rights reserved.
*/

#ifndef __COMMON_H__
#define __COMMON_H__

#include <stdint.h>
#include <semaphore.h>

#define MBUF_CACHE_SZ   32
#define TX_QUEUE_SIZE   4096
#define NB_TX_QUEUES    64 /* ^2 needed to make fast modulos % */
#define BURST_SZ        128
#define NB_RETRY_TX     (NB_TX_QUEUES * 2)

#define TX_PTHRESH 36 // Default value of TX prefetch threshold register.
#define TX_HTHRESH 0  // Default value of TX host threshold register.
#define TX_WTHRESH 0  // Default value of TX write-back threshold register.

#ifndef min
#define min(x, y) (x < y ? x : y)
#endif /* min */
#ifndef max
#define max(x, y) (x > y ? x : y)
#endif /* max */

/* struct to store the command line args */
struct cmd_opts {
    char**          pcicards;
    int             nb_pcicards;
    int             numacore;
    int             nbruns;
    unsigned int    maxbitrate;
    int             wait;
    char*           trace;
    int             increase_hugepages_nb;
};

/* struct to store the cpus context */
struct                  cpus_bindings {
    int                 numacores; /* nb of numacores of the system */
    int                 numacore; /* wanted numacore to run */
    unsigned int        nb_available_cpus;
    unsigned int        nb_needed_cpus;
    unsigned int*       cpus_to_use;
    char*               prefix;
    char*               suffix;
    uint64_t            coremask;
};

/* struct corresponding to a cache for one NIC port */
struct                  pcap_cache {
    struct rte_mbuf**   mbufs;
};

/* struct to store dpdk context */
struct                  dpdk_ctx {
    unsigned long       nb_mbuf; /* number of needed mbuf (see main.c) */
    unsigned long       mbuf_sz; /* wanted/needed size for the mbuf (see main.c) */
    unsigned long       pool_sz; /* mempool wanted/needed size (see main.c) */
    struct rte_mempool* pktmbuf_pool;

    /* pcap file caches */
    long int            pcap_sz; /* size of the capture */
    struct pcap_cache*  pcap_caches; /* tab of caches, one per NIC port */
};

/* struct to store threads context */
struct                  thread_ctx {
    sem_t*              sem;
    pthread_t           thread;
    int                 tx_port_id; /* assigned tx port id */
    int                 nbruns;
    unsigned int        nb_pkt;
    int                 nb_tx_queues;
    /* results */
    double              duration;
    unsigned int        total_drop;
    unsigned int        total_drop_sz;
    struct pcap_cache*  pcap_cache;
};

struct                  pcap_ctx {
    int                 fd;
    unsigned int        nb_pkts;
    unsigned int        max_pkt_sz;
    size_t              cap_sz;
};

/*
  FUNC PROTOTYPES
*/

/* CPUS.C */
int             init_cpus(const struct cmd_opts* opts, struct cpus_bindings* cpus);

/* DPDK.C */
int             init_dpdk_eal_mempool(const struct cmd_opts* opts,
                                      const struct cpus_bindings* cpus,
                                      struct dpdk_ctx* dpdk);
int             init_dpdk_ports(struct cpus_bindings* cpus);
void*           myrealloc(void* ptr, size_t new_size);
int             start_tx_threads(const struct cmd_opts* opts,
                                 const struct cpus_bindings* cpus,
                                 const struct dpdk_ctx* dpdk,
                                 const struct pcap_ctx *pcap);
void            dpdk_cleanup(struct dpdk_ctx* dpdk, struct cpus_bindings* cpus);

/* PCAP.C */
int             preload_pcap(const struct cmd_opts* opts, struct pcap_ctx* pcap);
int             load_pcap(const struct cmd_opts* opts, struct pcap_ctx* pcap,
                          const struct cpus_bindings* cpus, struct dpdk_ctx* dpdk);
void            clean_pcap_ctx(struct pcap_ctx* pcap);

/* UTILS.C */
char*           nb_oct_to_human_str(float size);
unsigned int    get_next_power_of_2(const unsigned int nb);

#endif /* __COMMON_H__ */
