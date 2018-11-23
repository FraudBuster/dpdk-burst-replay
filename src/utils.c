/*
  SPDX-License-Identifier: BSD-3-Clause
  Copyright 2018 Jonathan Ribas, FraudBuster. All rights reserved.
*/

#define _GNU_SOURCE
#include <stdio.h>

char*   nb_oct_to_human_str(float size)
{
    char*           disp_unit[] = { "o", "Mo", "Mo", "Go", "To" };
    int             i;
    char*           buf = NULL;
        
    for (i = 0; i < 5; i++, size /= 1024)
        if (size / 1024 < 1) break;
    if (asprintf(&buf, "%.3f %s", size, disp_unit[i]) == -1) {
        fprintf(stderr, "%s: asprintf failed.\n", __FUNCTION__);
        return (NULL);
    }
    return (buf);
}

unsigned int get_next_power_of_2(const unsigned int nb)
{
    unsigned int i;

    for (i = 0; (unsigned int)((1 << i)) < nb; i++) ;
    return (1 << i);
}
