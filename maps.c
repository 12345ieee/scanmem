/*
*
* $Author: taviso $
* $Revision: 1.5 $
*
*/

#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif

#include <stdio.h>
#include <sys/types.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <alloca.h>

#include "scanmem.h"

int readmaps(pid_t target, list_t *regions)
{
    FILE *maps;
    char name[22], *line = NULL;
    size_t len = 0;

    /* construct the maps filename */
    snprintf(name, sizeof(name), "/proc/%u/maps", target);

    /* attempt to open the maps file */
    if ((maps = fopen(name, "r")) == NULL) {
        fprintf(stderr, "error: failed to open maps file %s.\n", name);
        return -1;
    }

    eprintf("info: maps file located at %s opened.\n", name);

    /* read every line of the maps file */
    while (getline(&line, &len, maps) != -1) {
        intptr_t start, end;
        region_t *map = NULL;
        char read, write, exec, cow, *pathname;

        if ((pathname = calloc(len, 1)) == NULL) {
            fprintf(stderr, "error: failed to allocate %u bytes for pathname.\n", len);
            goto error;
        }
        
        /* parse each line */
        if (sscanf(line, "%x-%x %c%c%c%c %*x %*s %*u %s", &start, &end, &read,
                &write, &exec, &cow, pathname) >= 6) {
            
				/* must have permissions to read and write */
            if (write == 'w' && read == 'r') {

	         	 /* allocate a new region structure */
                if ((map = calloc(1, sizeof(region_t))) == NULL) {
                   fprintf(stderr, "error: failed to allocate memory for region.\n");
                   free(pathname);
                   goto error;
                }

                /* initialise this region */
                map->perms |= (MAP_RD | MAP_WR);    
                map->start = start;
                map->size = end - start;

            	 /* setup other permissions */
                if (exec == 'x') map->perms |= MAP_EX;
                if (cow == 's') map->perms |= MAP_SH;
                if (cow == 'p') map->perms |= MAP_PR;
            
            	 /* save pathname */
                if (*pathname) {
                    if ((map->pathname = realloc(pathname, strlen(pathname) + 1)) == NULL) {
                        fprintf(stderr, "error: failed to allocate memory.\n");
                        goto error;
                    }
                } else {
                    map->pathname = NULL;
                }
                
					 /* okay, add this guy to our list */
                if (l_append(regions, NULL, map) == -1) {
                    fprintf(stderr, "error: sorry, failed to add region to list.\n");
                    goto error;
                }           
            } else {
                free(pathname);
            }
        } else {
            free(pathname);
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
