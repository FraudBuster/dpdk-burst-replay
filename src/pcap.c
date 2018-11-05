/*
  Copyright 2018 Jonathan Ribas, FraudBuster. All rights reserved.

  Redistribution and use in source and binary forms, with or without modification,
  are permitted provided that the following conditions are met:

  1. Redistributions of source code must retain the above copyright notice,
  this list of conditions and the following disclaimer.

  2. Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation and/or
  other materials provided with the distribution.

  3. Neither the name of the copyright holder nor the names of its contributors
  may be used to endorse or promote products derived from this software without
  specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <strings.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <rte_malloc.h>
#include <rte_mbuf.h>

#include "main.h"

#define MAX_PKT_SZ (1024*64) /* 64ko */

/*
  PCAP file header
*/
#define PCAP_MAGIC (0xa1b2c3d4)
#define PCAP_MAJOR_VERSION (2)
#define PCAP_MINOR_VERSION (4)
#define PCAP_SNAPLEN (262144)
#define PCAP_NETWORK (1) /* ethernet layer */
typedef struct pcap_hdr_s {
    uint32_t magic_number;   /* magic number */
    uint16_t version_major;  /* major version number */
    uint16_t version_minor;  /* minor version number */
    int32_t  thiszone;       /* GMT to local correction */
    uint32_t sigfigs;        /* accuracy of timestamps */
    uint32_t snaplen;        /* max length of captured packets, in octets */
    uint32_t network;        /* data link type */
} __attribute__((__packed__)) pcap_hdr_t;

typedef struct pcaprec_hdr_s {
        uint32_t ts_sec;         /* timestamp seconds */
        uint32_t ts_usec;        /* timestamp microseconds */
        uint32_t incl_len;       /* number of octets of packet saved in file */
        uint32_t orig_len;       /* actual length of packet */
} __attribute__((__packed__)) pcaprec_hdr_t;

int check_pcap_hdr(const int fd)
{
    pcap_hdr_t  pcap_h;
    size_t      nb_read;

    nb_read = read(fd, &pcap_h, sizeof(pcap_h));
    if (nb_read != sizeof(pcap_h))
        return (EIO);
    if (pcap_h.magic_number != PCAP_MAGIC ||
        pcap_h.version_major != PCAP_MAJOR_VERSION ||
        pcap_h.version_minor != PCAP_MINOR_VERSION) {
        printf("%s: check failed. magic (0x%.8x), major: %u, minor: %u\n",
               __FUNCTION__, pcap_h.magic_number,
               pcap_h.version_major, pcap_h.version_minor);
        return (EPROTO);
    }
    return (0);
}

int add_pkt_to_cache(const struct dpdk_ctx* dpdk, const int cache_index,
                     const unsigned char* pkt_buf, const size_t pkt_sz,
                     const unsigned int cpt, const int nbruns)
{
    struct rte_mbuf* m;

    if (!dpdk || !pkt_buf)
        return (EINVAL);

    m = rte_pktmbuf_alloc(dpdk->pktmbuf_pool);
    if (!m) {
        printf("\n%s rte_pktmbuf_alloc failed. exiting.\n", __FUNCTION__);
        return (ENOMEM);
    }
    rte_memcpy((char*)m->buf_addr, pkt_buf, pkt_sz);
    m->data_off = 0;
    m->data_len = m->pkt_len = pkt_sz;
    m->nb_segs = 1;
    m->next = NULL;

    /* set the refcnt to the wanted number of runs, avoiding to free
       mbuf struct on first tx burst */
    rte_mbuf_refcnt_set(m, nbruns);

    /* check that the crafted packet is valid */
    rte_mbuf_sanity_check(m, 1);

    /* assign new cached pkt to list */
    dpdk->pcap_caches[cache_index].mbufs[cpt] = m;
    return (0);
}

int preload_pcap(const struct cmd_opts* opts, struct pcap_ctx* pcap)
{
    unsigned char   pkt_buf[MAX_PKT_SZ];
    pcaprec_hdr_t   pcap_rechdr;
    struct stat     s;
    unsigned int    cpt;
    size_t          nb_read;
    long int        total_read;
    float           percent;
    int             ret;

    if (!opts || !pcap)
        return (EINVAL);

    /* open wanted file */
    pcap->fd = open(opts->trace, O_RDONLY);
    if (pcap->fd < 0) {
        printf("open of %s failed: %s\n", opts->trace, strerror(errno));
        return (errno);
    }

    /* check pcap header */
    ret = check_pcap_hdr(pcap->fd);
    if (ret)
        goto preload_pcapErrorInit;

    /* get file informations */
    ret = stat(opts->trace, &s);
    if (ret)
        goto preload_pcapErrorInit;
    s.st_size -= sizeof(pcap_hdr_t);
    printf("preloading %s file (of size: %li bytes)\n", opts->trace, s.st_size);
    pcap->cap_sz = s.st_size;

    /* loop on file to read all saved packets */
    for (total_read = 0, cpt = 0; ; cpt++) {
        /* get packet pcap header */
        nb_read = read(pcap->fd, &pcap_rechdr, sizeof(pcap_rechdr));
        if (!nb_read) /* EOF :) */
            break;
        else if (nb_read == (unsigned long)(-1)) {
            printf("\n%s: read failed (%s)\n", __FUNCTION__, strerror(errno));
            ret = errno;
            goto preload_pcapError;
        } else if (nb_read != sizeof(pcap_rechdr)) {
            printf("\nread pkt hdr misssize: %lu / %lu\n",
                   nb_read, sizeof(pcap_rechdr));
            ret = EIO;
            goto preload_pcapError;
        }
        total_read += nb_read;

#ifdef DEBUG
        if (pcap_rechdr.incl_len != pcap_rechdr.orig_len)
            printf("\npkt %i size: %u/%u\n", cpt,
                   pcap_rechdr.incl_len, pcap_rechdr.orig_len);
#endif /* DEBUG */

        /* update max pkt size (to be able to calculate the needed memory) */
        if (pcap_rechdr.incl_len > pcap->max_pkt_sz)
            pcap->max_pkt_sz = pcap_rechdr.incl_len;

        /* get packet */
        nb_read = read(pcap->fd, pkt_buf, pcap_rechdr.incl_len);
        if (nb_read == (unsigned long)(-1)) {
            printf("\n%s: read failed (%s)\n", __FUNCTION__, strerror(errno));
            ret = errno;
            goto preload_pcapError;
        } else if (nb_read != pcap_rechdr.incl_len) {
            printf("\nread pkt %i payload misssize: %u / %u\n", cpt,
                   (unsigned int)nb_read, pcap_rechdr.incl_len);
            ret = EIO;
            goto preload_pcapError;
        }
        total_read += nb_read;

        /* calcul & print progression every 1024 pkts */
        if ((cpt % 1024) == 0) {
            percent = 100 * (float)total_read / (float)s.st_size;
            printf("\rfile read at %02.2f%%", percent);
        }
    }

preload_pcapError:
    percent = 100 * (float)total_read / (float)s.st_size;
    printf("%sfile read at %02.2f%%\n", (ret ? "\n" : "\r"), percent);
    printf("read %u pkts (for a total of %li bytes). max paket length = %u bytes.\n",
           cpt, total_read, pcap->max_pkt_sz);
preload_pcapErrorInit:
    if (ret) {
        close(pcap->fd);
        pcap->fd = 0;
    } else
        pcap->nb_pkts = cpt;
    return (ret);
}

int load_pcap(const struct cmd_opts* opts, struct pcap_ctx* pcap,
              const struct cpus_bindings* cpus, struct dpdk_ctx* dpdk)
{
    pcaprec_hdr_t   pcap_rechdr;
    unsigned char   pkt_buf[MAX_PKT_SZ];
    unsigned int    cpt = 0;
    size_t          nb_read;
    long int        total_read = 0;
    float           percent;
    unsigned int    i;
    int             ret;

    if (!opts || !pcap || !cpus || !dpdk)
        return (EINVAL);

    /* alloc needed pkt caches and bzero them */
    dpdk->pcap_caches = malloc(sizeof(*(dpdk->pcap_caches)) * (cpus->nb_needed_cpus));
    if (!dpdk->pcap_caches) {
        printf("malloc of pcap_caches failed.\n");
        return (ENOMEM);
    }
    bzero(dpdk->pcap_caches, sizeof(*(dpdk->pcap_caches)) * (cpus->nb_needed_cpus));
    for (i = 0; i < cpus->nb_needed_cpus; i++) {
        dpdk->pcap_caches[i].mbufs = malloc(sizeof(*(dpdk->pcap_caches[i].mbufs)) *
                                            pcap->nb_pkts);
        if (dpdk->pcap_caches[i].mbufs == NULL) {
            fprintf(stderr, "%s: malloc of mbufs failed.\n", __FUNCTION__);
            return (ENOMEM);
        }
        bzero(dpdk->pcap_caches[i].mbufs,
              sizeof(*(dpdk->pcap_caches[i].mbufs)) * pcap->nb_pkts);
    }

    /* seek again to the beginning */
    if (lseek(pcap->fd, 0, SEEK_SET) == (off_t)(-1)) {
        printf("%s: lseek failed (%s)\n", __FUNCTION__, strerror(errno));
        ret = errno;
        goto load_pcapError;
    }
    ret = check_pcap_hdr(pcap->fd);
    if (ret)
        goto load_pcapError;

    printf("-> Will cache %i pkts.\n", pcap->nb_pkts);
    for (; cpt < pcap->nb_pkts; cpt++) {
        /* get packet pcap header */
        nb_read = read(pcap->fd, &pcap_rechdr, sizeof(pcap_rechdr));
        if (!nb_read) /* EOF :) */
            break;
        else if (nb_read == (unsigned long)(-1)) {
            printf("\n%s: read failed (%s)\n", __FUNCTION__, strerror(errno));
            ret = errno;
            goto load_pcapError;
        } else if (nb_read != sizeof(pcap_rechdr)) {
            printf("\nread pkt hdr misssize: %u / %lu\n",
                   (unsigned int)nb_read, sizeof(pcap_rechdr));
            ret = EIO;
            goto load_pcapError;
        }
        total_read += nb_read;

        /* get packet */
        nb_read = read(pcap->fd, pkt_buf, pcap_rechdr.incl_len);
        if (nb_read == (unsigned long)(-1)) {
            printf("\n%s: read failed (%s)\n", __FUNCTION__, strerror(errno));
            ret = errno;
            goto load_pcapError;
        } else if (nb_read != pcap_rechdr.incl_len) {
            printf("\nread pkt %u payload misssize: %u / %u\n", cpt,
                   (unsigned int)nb_read, pcap_rechdr.incl_len);
            ret = EIO;
            goto load_pcapError;
        }
        total_read += nb_read;

        /* add packet to caches */
        for (i = 0; i < cpus->nb_needed_cpus; i++) {
            ret = add_pkt_to_cache(dpdk, i, pkt_buf, nb_read, cpt, opts->nbruns);
            if (ret) {
                fprintf(stderr, "\nadd_pkt_to_cache failed on pkt.\n");
                goto load_pcapError;
            }
        }

        /* calcul & print progression every 1024 pkts */
        if ((cpt % 1024) == 0) {
            percent = 100 * cpt / pcap->nb_pkts;
            printf("\rfile read at %02.2f%%", percent);
        }
    }

load_pcapError:
    percent = 100 * cpt / pcap->nb_pkts;
    printf("%sfile read at %02.2f%%\n", (ret ? "\n" : "\r"), percent);
    printf("read %u pkts (for a total of %li bytes).\n", cpt, total_read);
    dpdk->pcap_sz = total_read;
    close(pcap->fd);
    pcap->fd = 0;
    return (ret);
}

void clean_pcap_ctx(struct pcap_ctx* pcap)
{
    if (!pcap)
        return ;

    if (pcap->fd) {
        close(pcap->fd);
        pcap->fd = 0;
    }
    return ;
}
