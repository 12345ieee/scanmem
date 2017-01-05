/*
*
* $Author: taviso $
* $Revision: 1.1 $
*/

#include <sys/ptrace.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "scanmem.h"

int attach(pid_t target)
{
    int status;
    
    /* attach, to the target application, which should cause a SIGSTOP */
    if (ptrace(PTRACE_ATTACH, target, NULL, NULL) == -1) {
        fprintf(stderr, "error: failed to attach to %d.\n", target);
        return -1;
    }
    
    /* wait for the SIGSTOP to take place. */
    if (waitpid(target, &status, 0) == -1 || !WIFSTOPPED(status)) {
        fprintf(stderr, "error: there was an error waiting for the target to stop.\n");
        return -1; 
    }
    
    /* everything looks okay */
    return 0;

}

int detach(pid_t target)
{
    return ptrace(PTRACE_DETACH, target, NULL, 0);
}

int candidates(intptr_t **matches, unsigned *num, region_t *regions, unsigned count, 
    pid_t target, unsigned value, bool print)
{ 
    /* do we already have a list of locations to check? */
    if (*num != 0 && *matches != NULL) {
        unsigned i;
    
        for (i = 0; i < *num; i++) {
            if (ptrace(PTRACE_PEEKDATA, target, (*matches)[i], NULL) == value) {
                /* still a candidate */
                continue;
            } else {
                memmove(&(*matches)[i], &(*matches)[i+1], (--(*num) - i) * sizeof(intptr_t));
                if ((*matches = realloc(*matches, *num * sizeof(intptr_t))) == NULL) {
                    fprintf(stderr, "error: there was a problem allocating memory.\n");
                    return -1;
                }
            }
        }
    } else {
        unsigned j, k;
        
        /* for every memory region */
        for (j = 0; j < count; j++) {
            
            /* for every aligned word */
            for (k = regions[j].region; k < regions[j].region + regions[j].size; k++) {

                if (ptrace(PTRACE_PEEKDATA, target, k, NULL) == value) {
                    if ((*matches = realloc(*matches, ++(*num) * sizeof(intptr_t))) == NULL) {
                        fprintf(stderr, "error: there was a problem allocating memory.\n");
                        return -1;
                    }
                    
                    /* save this new location */
                    (*matches)[*num - 1] = k;
                }
            }
        }
    }
    
    return 0;
}
                
int setaddr(pid_t target, intptr_t addr, unsigned to)
{
    return ptrace(PTRACE_POKEDATA, target, addr, to);
}
