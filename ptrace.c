/*
*
* $Author: taviso $
* $Revision: 1.5 $
*/

#include <sys/ptrace.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <limits.h>
#include <stdbool.h>

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

int candidates(list_t *matches, list_t *regions, pid_t target, unsigned value, unsigned width,
               matchtype_t type)
{
    unsigned mask = ~0;
    (void) type;
    
    /* calculate shift for mask */
    width = (sizeof(unsigned) * CHAR_BIT) - width;
    
    /* stop and attach to the target */
    if (attach(target) == -1)
        return -1;
    
    /* do we already have a list of locations to check? */
    if (matches->size) {
        element_t *n, *p;
        
        p = NULL;
        n = matches->head;
        
        while (n) {
            match_t *match = n->data;
            unsigned cval;
            bool t;
            
            cval = ptrace(PTRACE_PEEKDATA, target, match->address, NULL) & (mask >> width);
            
            switch (type) {
                case MATCHEXACT:
                    t = (cval == value);
                    break;
                case MATCHDECREMENT:
                    t = (cval < match->lvalue);
                    break;
                case MATCHINCREMENT:
                    t = (cval > match->lvalue);
                    break;
                default:
                    fprintf(stderr, "error: unrecognised type.\n");
                    return -1;
            }
            
            if (t) {
                /* still a candidate */
                match->lvalue = cval;
                p = n;
                n = n->next;
            } else {
                /* no match */
                l_remove(matches, p, NULL);
                
                /* confirm this isnt the head element */
                n = p ? p->next : matches->head;
            }
        }
    } else if (type == MATCHEXACT) {
        element_t *n = regions->head;
        region_t *r;
        
        /* make sure we have some regions to search */
        if (regions->size == 0) {
            fprintf(stderr, "warn: no regions defined, perhaps you deleted them all?\n");
            fprintf(stderr, "info: use the \"reset\" command to refresh regions.\n");
            return detach(target);
        }
        
        /* first time, we have to check every memory region */
        while (n) {
            unsigned offset;
            
            r = n->data;

            /* this first scan can be very slow on large programs, eg quake3 ;)  */
            /* print a progress meter so user knows we havnt crashed */
            fprintf(stderr, "info: searching %#010x - %#010x.", r->start, r->start + r->size);
            fflush(stderr);
            
            /* for every word */
            for (offset = 0; offset < r->size; offset++) {
                if ((ptrace(PTRACE_PEEKDATA, target, r->start + offset, NULL) & (mask >> width)) == value) {
                    match_t *match;
                    
						  /* save this new location */
                    if ((match = calloc(1, sizeof(match_t))) == NULL) {
                        fprintf(stderr, "error: memory allocation failed.\n");
                        (void) detach(target);
                        return -1;
                    }
                    
                    match->address = r->start + offset;
                    match->region = r;
                    match->lvalue = value;
                    
                    if (l_append(matches, NULL, match) == -1) {
                        fprintf(stderr, "error: unable to add match to list.\n");
                        (void) detach(target);
                        return -1;
                    }
                }
                
                /* print a simple progress meter. */
                if (offset % ((r->size - (r->size % 10)) / 10) == 10) {
                    fprintf(stderr, ".");
                    fflush(stderr);
                }
            }
            n = n->next;
            fprintf(stderr, "ok\n");
        }
    } else {
        fprintf(stderr, "warn: you cannot use that search without any candidates.\n");
    }

    eprintf("info: we currently have %d matches.\n", matches->size);
    
    /* okay, detach */
    return detach(target);
}

int setaddr(pid_t target, intptr_t addr, unsigned to)
{
    if (attach(target) == -1) {
        return -1;
    }
    
    if (ptrace(PTRACE_POKEDATA, target, addr, to) == -1) {
        return -1;
    }
    
    return detach(target);
}
