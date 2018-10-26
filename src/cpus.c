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
#include <numa.h>
#include <unistd.h>

#include "config.h"
#include "main.h"

static int find_cpus_to_use(const struct cmd_opts* opts, struct cpus_bindings* cpus)
{
    unsigned int        i;
    unsigned int        cpu_cpt;

    if (!opts || !cpus)
        return (EINVAL);

    cpus->numacores = 1;
    cpus->numacore = opts->numacore;
    cpus->cpus_to_use = (void*)malloc(sizeof(*(cpus->cpus_to_use)) * (cpus->nb_needed_cpus + 1));
    if (cpus->cpus_to_use == NULL) {
        printf("%s: malloc failed.\n", __FUNCTION__);
        return (ENOMEM);
    }
    printf("CPU cores to use:");
    for (i = 0, cpu_cpt = 0; i < cpus->nb_available_cpus; i++) {
        /* be sure that we get cores on the wanted numa */
        if (cpus->numacore == numa_node_of_cpu(i)) {
            cpus->cpus_to_use[cpu_cpt++] = i;
            printf(" %i", i);
            if (cpu_cpt == cpus->nb_needed_cpus + 1) /* +1 to keep the first as fake master */
                break;
        } else cpus->numacores = 2;
    }
    putchar('\n');
    if (cpu_cpt < cpus->nb_needed_cpus + 1) {
        printf("Wanted %i threads on numa %i, but found only %i CPUs.\n",
               cpus->nb_needed_cpus + 1, cpus->numacore, cpu_cpt);
        free(cpus->cpus_to_use);
        return (ENODEV);
    }
    return (0);
}

static uint64_t generate_mask(const struct cpus_bindings* cpus, uint8_t number)
{
    int      i;
    uint64_t coremask;

    if (!cpus)
        return (EINVAL);

    if ((0 == number) || ( 64 < number)) /* out of bounds */
        return (0);

    /* generate coremask */
    for (coremask = 0, i = 0; i < number; i++)
        coremask |= (uint64_t)(1 << cpus->cpus_to_use[i]);
    printf("%s for %u cores -> 0x%lx\n", __FUNCTION__, number, coremask);
    return (coremask);
}

int init_cpus(const struct cmd_opts* opts, struct cpus_bindings* cpus)
{
    int ret;
    int i;

    if (!opts || !cpus)
        return (EINVAL);

    /* get the number of available cpus */
    cpus->nb_available_cpus = (int)sysconf(_SC_NPROCESSORS_ONLN);
#ifdef DEBUG
    printf("available cpus: %i\n", cpus->nb_available_cpus);
#endif /* DEBUG */

    /* calculate the number of needed cpu cores */
    for (i = 0; opts->pcicards[i]; i++);
    cpus->nb_needed_cpus = i;
    printf("-> Needed cpus: %i\n", cpus->nb_needed_cpus);

    /* lookup on cores ID to use */
    ret = find_cpus_to_use(opts, cpus);
    if (ret)
        return (ret);

    /* generate coremask of selected cpu cores for dpdk init */
    /* NOTES: get an extra one to not use the 0/master one. TODO: do better :) */
    cpus->coremask = generate_mask(cpus, cpus->nb_needed_cpus + 1);
    if (!cpus->coremask)
        return (EINVAL);
    return (0);
}
