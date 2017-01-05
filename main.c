/*
*
* $Author: taviso $
* $Revision: 1.7 $
*
*/

#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif

#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stddef.h>
#include <getopt.h>
#include <string.h>
#include <signal.h>
#include <limits.h>
#include <assert.h>

#include "scanmem.h"
#include "list.h"

pid_t target; /* used on signal to detach from target */

int main(int argc, char **argv)
{
    int optindex, ret = 0;
    unsigned continuous = 0, width = sizeof(unsigned) * CHAR_BIT, max = 0xffff;
    list_t *regions = NULL, *matches = NULL;
    element_t *n = NULL;
    struct option longopts[] = {
        { "pid",         1, NULL, 'p' },  /* target pid */
        { "version",     0, NULL, 'v' },  /* print version */
        { "help",        0, NULL, 'h' },  /* print help summary */
        { NULL,          0, NULL,  0  },
    };

    (void) max; /* plan to implement max matches at some point, to prevent memory exhaustion */
    
    /* process command line */
    while (true) {
        switch (getopt_long(argc, argv, ":vhw", longopts, &optindex)) {
            case 'p':
                target = (pid_t) strtoul(optarg, NULL, 0);
                break;
            case 'v':
                printversion();
                return 0;
                break;
            case 'h':
                printhelp();
                return 0;
                break;
            case -1:
                goto done;
            default:
                fprintf(stderr, "error: an error occurred while processing arguments.\n");
                break;
        }
    }

done:

    /* check if pid was specified */
    if (target) {
        eprintf("info: attaching to pid %u.\n", target);
    } else {
        fprintf(stderr, "error: you must specify a pid, use --help for assistance.\n");
        return 1;
    }

    /* before attaching to target, install signal handler to detach on error */
    signal(SIGHUP, sighandler);
    signal(SIGINT, sighandler);
    signal(SIGQUIT, sighandler);
    signal(SIGSEGV, sighandler);
    signal(SIGABRT, sighandler);
    signal(SIGILL, sighandler);

    /* create a new linked list of regions */
    if ((regions = l_init()) == NULL) {
        fprintf(stderr, "error: sorry, there was a memory allocation error.\n");
        ret++;
        goto end;
    }
    
    /* get list of regions */
    if (readmaps(target, regions) == -1) {
        fprintf(stderr, "error: sorry, there was a problem getting a list of regions to search.\n");
        ret++;
        goto end;
    }

    /* create a list to store matches */
    if ((matches = l_init()) == NULL) {
        fprintf(stderr, "error: sorry, there was a memory allocation error.\n");
        ret++;
        goto end;
    }
    
    eprintf("info: %u suitable regions found.\n", regions->size);
    
    fprintf(stderr, "Please enter current value, or \"help\" for other commands.\n");
       
    /* main loop, read input and process commands */
    while (true) {
        unsigned operand;
      
        switch (getcommand(matches->size, &operand)) {
            case COMMAND_EXACT:
                /* user has specified an exact value of the variable to find */
                if (candidates(matches, regions, target, operand, width, MATCHEXACT) == -1) {
                    fprintf(stderr, "error: failed to search target address space.\n");
                    ret++;
                    goto end;
                }
                
                /* check if we now know the only possible candidate */
                if (matches->size == 1) {
                    fprintf(stderr, "info: match identified, use \"set\" to modify value.\n");
                    fprintf(stderr, "info: enter \"help\" for other commands.\n");
                }
                break;
            case COMMAND_INCREMENT:
                /* check if user has indicated that the variable is now higher than last time seen */
                if (candidates(matches, regions, target, 0x00, width, MATCHINCREMENT) == -1) {
                    fprintf(stderr, "error: failed to search target address space.\n");
                    ret++;
                    goto end;
                }
                
                if (matches->size == 1) {
                    fprintf(stderr, "info: match identified, use \"set\" to modify value.\n");
                    fprintf(stderr, "info: enter \"help\" for other commands.\n");
                }
                break;
            case COMMAND_DECREMENT:
                /* the user indicated the value is now lower than last time seen */
                if (candidates(matches, regions, target, 0x00, width, MATCHDECREMENT) == -1) {
                    fprintf(stderr, "error: failed to search target address space.\n");
                    ret++;
                    goto end;
                }
                
                if (matches->size == 1) {
                    fprintf(stderr, "info: match identified, use \"set\" to modify value.\n");
                    fprintf(stderr, "info: enter \"help\" for other commands.\n");
                }
                break;
            case COMMAND_CONTINUOUS:
                /* the set command should continually inject the specified value */
                if ((continuous = operand)) {
                    fprintf(stderr, "info: use \"set\" to start injecting value every %u seconds.\n", operand);
                } else {
                    /* zero operand indicated disable continuous mode */
                    fprintf(stderr, "info: continuous mode disabled.\n");
                }
                break;
            case COMMAND_SET:
                if (continuous) {
                    fprintf(stderr, "info: setting value every %u seconds until interrupted...\n", continuous);
                }
                
					 /* set every value in match list to operand */
                while (true) {
                    element_t *n = matches->head;
                      
                    while (n) {
                        match_t *match = n->data;
                            
                        fprintf(stderr, "info: setting *%#010x to %u...\n", match->address, operand);
                            
                        if (setaddr(target, match->address, operand) == -1) {
                            fprintf(stderr, "error: failed to set a value.\n");
                            ret++;
                            goto end;
                        }
                          
                        n = n->next;
                    }

                    if (continuous) {
                        sleep(continuous);
                    } else {
                        break;
                    }
                        
                }
                break;
            case COMMAND_LIST: {
                    unsigned i = 0;
                    element_t *n = matches->head;
                    
                    /* list all known matches */
                    while (n) {
                        match_t *match = n->data;
                        fprintf(stderr, "[%02u] %#010x {%10u} (%s)\n", i++, 
                            match->address, match->lvalue,
                            match->region->pathname ? match->region->pathname :
                                "unassociated, typically .bss");
                        n = n->next;
                    }
                }
                break;
            case COMMAND_DELETE: {
                    unsigned i;
                    element_t *n = matches->head;
                    
                    /* delete the nth match from the matches list */
                    if (operand < matches->size) {
                        for (i = 0; n && i < operand - 1; i++, n = n->next)
                            ;
                        l_remove(matches, n, NULL);
                    } else {
                        fprintf(stderr, "warn: you attempted to delete a non-existant match `%u`.\n", operand);
                        fprintf(stderr, "info: use \"list\" to list matches, or \"help\" for other commands.\n");
                    }
                }
                break;
            case COMMAND_PID:
                /* print the pid of the target program */
                fprintf(stderr, "info: target pid is %u.\n", target);
                break;
            case COMMAND_EXIT:
                /* exit now */
                goto end;
                break;
            case COMMAND_WIDTH:
                /* change width of target */
                if ((operand > CHAR_BIT * sizeof(unsigned) || operand % CHAR_BIT != 0) && operand != 0) {
                    fprintf(stderr, "info: %u is invalid, width must be multiple of %d and <= %d.\n",
                        operand, CHAR_BIT, sizeof(unsigned) * CHAR_BIT);
                } else if (operand != 0) {
                    if (matches->size) {
                        fprintf(stderr, "warn: you cannot change the width when you have matches.\n");
                        fprintf(stderr, "info: use \"reset\" to remove matches, or \"help\" for other commands.\n");
                    } else {
                        width = operand;
                    }
                }
                
                fprintf(stderr, "info: width is currently set to %u.\n", width);
                break;
            case COMMAND_LISTREGIONS: {
                    unsigned i = 0;
                    element_t *n = regions->head;
                    
                    /* print a list of regions that are searched */
                    while (n) {
                        region_t *region = n->data;
                        fprintf(stderr, "[%02u] %#010x, %u bytes, %c%c%c, %s\n", i++, 
                            region->start, region->size, region->perms & MAP_RD ? 'r' : '-',
                            region->perms & MAP_WR ? 'w' : '-', region->perms & MAP_EX ? 'x' : '-',
                            region->pathname ? region->pathname :
                                "unassociated");
                        n = n->next;
                    }
                }
                break;
            case COMMAND_DELREGIONS: {
                    unsigned i;
                    element_t *n = regions->head, *t = matches->head, *p = NULL;
                    
                    /* delete the nth region */
                    if (operand < regions->size) {
                        
                        /* traverse list to element */
                        for (i = 0; n && i < operand - 1; i++, n = n->next)
                            ; 
                        
                        /* check for any affected matches before removing it */
                        while (t) {
                            match_t *match = t->data;
                            region_t *s;
                            
                            /* determine the correct pointer we're supposed to be checking */
                            if (n) {
                                assert(n->next);
                                s = n->next->data;
                            } else {
                                /* head of list */
                                s = regions->head->data;
                            }
                            
                            /* check if this one should go */
                            if (match->region == s) {
                                /* remove this match */
                                l_remove(matches, p, NULL);
                                
                                /* move to next element */
                                t = p ? p->next : matches->head;
                            } else {
                                p = t;
                                t = t->next;
                            }
                        }
                        l_remove(regions, n, NULL);
                    } else {
                        fprintf(stderr, "warn: you attempted to delete a non-existant region `%u`.\n", operand);
                        fprintf(stderr, "info: use \"lregions\" to list regions, or \"help\" for other commands.\n");
                    }
                }
                break;
            case COMMAND_RESET:
                /* forget all matches */
                l_destroy(matches);
                
                if ((matches = l_init()) == NULL) {
                    fprintf(stderr, "error: sorry, there was a memory allocation error.\n");
                    ret++;
                    goto end;
                }
                
                /* refresh list of regions */
                l_destroy(regions);
                /* create a new linked list of regions */
                if ((regions = l_init()) == NULL || readmaps(target, regions) == -1) {
                    fprintf(stderr, "error: sorry, there was a problem getting a list of regions to search.\n");
                    ret++;
                    goto end;
                }
                break;
            case COMMAND_VERSION:
                printversion();
                break;
            case COMMAND_HELP:
                printinthelp();
                break;
            case COMMAND_ERROR:
            default:
                fprintf(stderr, "Please enter current value, or \"help\" for other commands.\n");
                break;
        }
    }

end:
    /* first free all the pathnames from region list */
    if (regions) {
        n = regions->head;
    }
    
    while (n) {
        region_t *r = n->data;
        free(r->pathname);
        n = n->next;
    }
    
    /* now free allocated memory used */
    if (regions) {
        l_destroy(regions);
    }
    
    if (matches) {
        l_destroy(matches);
    }
    
    /* attempt to detach just in case */
    (void) detach(target);
    
    return ret;
}

void sighandler(int n)
{
    (void) n;
    
    if (target) {
        (void) detach(target);
    }
    
    exit(1);
}

void printhelp(void)
{
    printversion();
 
    fprintf(stderr, "\n--pid pid (required)\n"
        "\tset the target process pid.\n"
        "--help\n"
        "\tprint this message.\n"
        "--version\n"
        "\tprint version message.\n");
    return;
}

void printversion(void)
{
    fprintf(stderr, "scanmem %s - Tavis Ormandy <taviso@sdf.lonestar.org>\n", VERSIONSTRING);
    return;
}
