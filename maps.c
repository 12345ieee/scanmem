/*
*
* $Author: taviso $
* $Revision: 1.2 $
*
*/

#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "scanmem.h"

int readmaps(pid_t target, region_t **regions, unsigned *count)
{
    FILE *maps;
    char name[22], *line = NULL;
    size_t len = 0;

    /* initialise number of regions found to zero */
    *count = 0;
    *regions = NULL;

     /* construct the maps filename */
    snprintf(name, sizeof(name), "/proc/%u/maps", target);

    /* attempt to open the maps file */
    if ((maps = fopen(name, "r")) == NULL) {
        fprintf(stderr, "error: failed to open maps file %s.\n", name);
        return -1;
    }

    fprintf(stderr, "info: maps file located at %s opened.\n", name);

    /* read every line of the maps file */
    while (getline(&line, &len, maps) != -1) {
        ptrdiff_t start, end;
        char read, write, execute;

        /* parse each line */
        if (sscanf(line, "%x-%x %c%c%c%*c %*x", &start, &end, &read, &write, &execute) == 5) {

            /* must have permissions to read and write */
            if (write == 'w' && read == 'r') {

                /* okay, add this guy to our array */
                if ((*regions = realloc(*regions, ++(*count) * sizeof(region_t))) == NULL) {
                    goto error;
                }

                (*regions)[*count - 1].region = start;
                (*regions)[*count - 1].size = end - start;
            }
        }
    }

    /* release memory allocated by getline() */
    free(line);
    fclose(maps);

    return 0;

error:
    free(line);
    fclose(maps);

    return -1;
}
