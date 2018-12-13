/*
  SPDX-License-Identifier: BSD-3-Clause
  Copyright 2018 Jonathan Ribas, FraudBuster. All rights reserved.
*/

#include <strings.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>

#include <rte_ethdev.h>

#include "main.h"

void usage(void)
{
    puts("dpdk-replay [options] pcap_file port1[,portx...]\n"
         "pcap_file: the file to send through the DPDK ports.\n"
         "port1[,portx...] : specify the list of ports to be used (pci addresses).\n"
         "Options:\n"
         "--numacore numacore : use cores only if it fits the wanted numa.\n"
         "--nbruns X : set the wanted number of replay (1 by default). Set to 0 to infinite mode.\n"
         /* TODO: */
         /* "[--maxbitrate bitrate]|[--normalspeed] : bitrate not to be exceeded (default: no limit) in ko/s.\n" */
         /* "  specify --normalspeed to replay the trace with the good timings." */
        );
    return ;
}

#ifdef DEBUG
void print_opts(const struct cmd_opts* opts)
{
    int i;

    if (!opts)
        return ;
    puts("--");
    printf("numacore: %i\n", (int)(opts->numacore));
    printf("nb runs: %u\n", opts->nbruns);
    /* if (opts->maxbitrate) */
    /*     printf("MAX BITRATE: %u\n", opts->maxbitrate); */
    /* else */
    /*     puts("MAX BITRATE: FULL SPEED"); */
    printf("trace: %s\n", opts->trace);
    printf("pci nic ports:");
    for (i = 0; opts->pcicards[i]; i++)
        printf(" %s", opts->pcicards[i]);
    puts("\n--");
    return ;
}
#endif /* DEBUG */

char** str_to_pcicards_list(struct cmd_opts* opts, char* pcis)
{
    char** list = NULL;
    int i;

    if (!pcis || !opts)
        return (NULL);

    for (i = 1; ; i++) {
        list = realloc(list, sizeof(*list) * (i + 1));
        if (!list)
            return (NULL);
        list[i - 1] = pcis;
        list[i] = NULL;
        while (*pcis != '\0' && *pcis != ',')
            pcis++;
        if (*pcis == '\0')
            break;
        else { /* , */
            *pcis = '\0';
            pcis++;
        }
    }
    opts->nb_pcicards = i;
    return (list);
}

int parse_options(const int ac, char** av, struct cmd_opts* opts)
{
    int i;

    if (!av || !opts)
        return (EINVAL);

    /* if no trace or no pcicard is specified */
    if (ac < 3)
        return (ENOENT);

    for (i = 1; i < ac - 2; i++) {
        /* --numacore numacore */
        if (!strcmp(av[i], "--numacore")) {
            int nc;

            /* if no numa core is specified */
            if (i + 1 >= ac - 2)
                return (ENOENT);

            nc = atoi(av[i + 1]);
            if (nc < 0 || nc > 127)
                return (ENOENT);
            opts->numacore = (char)nc;
            i++;
            continue;
        }

        /* --nbruns nbruns */
        if (!strcmp(av[i], "--nbruns")) {
            /* if no nb runs is specified */
            if (i + 1 >= ac - 2)
                return (ENOENT);

            opts->nbruns = atoi(av[i + 1]);
            if (opts->nbruns < 0)
                return (EPROTO);
            i++;
            continue;
        }
        break;
    }
    if (i + 2 > ac)
        return (EPROTO);
    opts->trace = av[i];
    opts->pcicards = str_to_pcicards_list(opts, av[i + 1]);
    return (0);
}

int check_needed_memory(const struct cmd_opts* opts, const struct pcap_ctx* pcap,
                        struct dpdk_ctx* dpdk)
{
    float           needed_mem;
    char*           hsize;

    if (!opts || !pcap || !dpdk)
        return (EINVAL);

    /* # CALCULATE THE NEEDED SIZE FOR MBUF STRUCTS */
    dpdk->mbuf_sz = sizeof(struct rte_mbuf) + pcap->max_pkt_sz;
    dpdk->mbuf_sz += (dpdk->mbuf_sz % (sizeof(int)));
#ifdef DEBUG
    puts("Needed paket allocation size = "
         "(size of MBUF) + (size of biggest pcap packet), "
         "rounded up to the next multiple of an integer.");
    printf("(%lu + %u) + ((%lu + %u) %% %lu) = %lu\n",
           sizeof(struct rte_mbuf), pcap->max_pkt_sz,
           sizeof(struct rte_mbuf), pcap->max_pkt_sz,
           sizeof(int), dpdk->mbuf_sz);
#endif /* DEBUG */
    printf("-> Needed MBUF size: %lu\n", dpdk->mbuf_sz);

    /* # CALCULATE THE NEEDED NUMBER OF MBUFS */
    /* For number of pkts to be allocated on the mempool, DPDK says: */
    /* The optimum size (in terms of memory usage) for a mempool is when n is a
       power of two minus one: n = (2^q - 1).  */
#ifdef DEBUG
    puts("Needed number of MBUFS: next power of two minus one of "
         "(nb pkts * nb ports)");
#endif /* DEBUG */
    dpdk->nb_mbuf = get_next_power_of_2(pcap->nb_pkts * opts->nb_pcicards) - 1;
    printf("-> Needed number of MBUFS: %lu\n", dpdk->nb_mbuf);

    /* # CALCULATE THE TOTAL NEEDED MEMORY SIZE  */
    needed_mem = dpdk->mbuf_sz * dpdk->nb_mbuf;
#ifdef DEBUG
    puts("Needed memory = (needed mbuf size) * (number of needed mbuf).");
    printf("%lu * %lu = %.0f bytes\n", dpdk->mbuf_sz, dpdk->nb_mbuf, needed_mem);
#endif /* DEBUG */
    hsize = nb_oct_to_human_str(needed_mem);
    if (!hsize)
        return (-1);
    printf("-> Needed Memory = %s\n", hsize);
    free(hsize);

    /* # CALCULATE THE NEEDED NUMBER OF GIGABYTE HUGEPAGES */
    if (fmod(needed_mem,((double)(1024*1024*1024))))
        dpdk->pool_sz = needed_mem / (float)(1024*1024*1024) + 1;
    else
        dpdk->pool_sz = needed_mem / (1024*1024*1024);
    printf("-> Needed Hugepages of 1 Go = %lu\n", dpdk->pool_sz);
    return (0);
}

int main(const int ac, char** av)
{
    struct cmd_opts         opts;
    struct cpus_bindings    cpus;
    struct dpdk_ctx         dpdk;
    struct pcap_ctx         pcap;
    int                     ret;

    /* set default opts */
    bzero(&cpus, sizeof(cpus));
    bzero(&opts, sizeof(opts));
    bzero(&dpdk, sizeof(dpdk));
    bzero(&pcap, sizeof(pcap));
    opts.nbruns = 1;

    /* parse cmdline options */
    ret = parse_options(ac, av, &opts);
    if (ret) {
        usage();
        return (1);
    }
#ifdef DEBUG
    print_opts(&opts);
#endif /* DEBUG */

    /*
      pre parse the pcap file to get needed informations:
      . number of packets
      . biggest packet size
    */
    ret = preload_pcap(&opts, &pcap);
    if (ret)
        goto mainExit;

    /* calculate needed memory to allocate for mempool */
    ret = check_needed_memory(&opts, &pcap, &dpdk);
    if (ret)
        goto mainExit;

    /*
      check that we have enough cpus, find the ones to use and calculate
       corresponding coremask
    */
    ret = init_cpus(&opts, &cpus);
    if (ret)
        goto mainExit;

    /* init dpdk eal and mempool */
    ret = init_dpdk_eal_mempool(&opts, &cpus, &dpdk);
    if (ret)
        goto mainExit;

    /* cache pcap file into mempool */
    ret = load_pcap(&opts, &pcap, &cpus, &dpdk);
    if (ret)
        goto mainExit;

    /* init dpdk ports to send pkts */
    ret = init_dpdk_ports(&cpus);
    if (ret)
        goto mainExit;

    /* start tx threads and wait to start to send pkts */
    ret = start_tx_threads(&opts, &cpus, &dpdk, &pcap);
    if (ret)
        goto mainExit;

mainExit:
    /* cleanup */
    clean_pcap_ctx(&pcap);
    dpdk_cleanup(&dpdk, &cpus);
    if (cpus.cpus_to_use)
        free(cpus.cpus_to_use);
    return (ret);
}
