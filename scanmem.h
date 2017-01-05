/*
*
* $Author: taviso $
* $Revision: 1.3 $
*
*/

#include <stddef.h>
#include <stdbool.h>

#define VERSIONSTRING "v0.02"

typedef struct regions {
    intptr_t region;
    size_t size;
} region_t;

int readmaps(pid_t target, region_t **regions, unsigned *count);
int attach(pid_t target);
int detach(pid_t target);
int candidates(intptr_t **matches, unsigned *num, region_t *regions, unsigned count,
    pid_t target, unsigned value);
int setaddr(pid_t target, intptr_t addr, unsigned to);
void sighandler(int n);
