/*
*
* $Author: taviso $
* $Revision: 1.11 $
*
*/

#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include <signal.h>
#include <limits.h>
#include <assert.h>
#include <math.h>

#include <readline/readline.h>

#include "scanmem.h"

static void printhelp(void);
static void sighandler(int n);

static pid_t target;            /* used on signal to detach from target */

int main(int argc, char **argv)
{
    int optindex, ret = 0;
    unsigned continuous = 0, max = 0xffff;
    list_t *regions = NULL, *matches = NULL;
    element_t *n = NULL;
    struct option longopts[] = {
        {"pid", 1, NULL, 'p'},  /* target pid */
        {"version", 0, NULL, 'v'},      /* print version */
        {"help", 0, NULL, 'h'}, /* print help summary */
        {NULL, 0, NULL, 0},
    };

    /* disable readline completion for now, this will work at some point */
    rl_bind_key('\t', rl_insert);

    (void) max;                 /* plan to implement max matches at some point, to prevent memory exhaustion */

    /* process command line */
    while (true) {
        switch (getopt_long(argc, argv, ":vhw", longopts, &optindex)) {
        case 'p':
            target = (pid_t) strtoul(optarg, NULL, 0);
            break;
        case 'v':
            printversion();
            return 0;
        case 'h':
            printhelp();
            return 0;
        case -1:
            goto done;
        default:
            fprintf(stderr,
                    "error: an error occurred while processing arguments.\n");
            break;
        }
    }

  done:

    /* check if pid was specified */
    if (target) {
        eprintf("info: attaching to pid %u.\n", target);
    } else {
        fprintf(stderr,
                "error: you must specify a pid, use --help for assistance.\n");
        return 1;
    }

    /* before attaching to target, install signal handler to detach on error */

    /*lint -save -e534 */
    signal(SIGHUP, sighandler);
    signal(SIGINT, sighandler);
    signal(SIGQUIT, sighandler);
    signal(SIGSEGV, sighandler);
    signal(SIGABRT, sighandler);
    signal(SIGILL, sighandler); /*lint -restore */

    /* create a new linked list of regions */
    if ((regions = l_init()) == NULL) {
        fprintf(stderr, "error: sorry, there was a memory allocation error.\n");
        ret++;
        goto end;
    }

    /* get list of regions */
    if (readmaps(target, regions) == -1) {
        fprintf(stderr,
                "error: sorry, there was a problem getting a list of regions to search.\n");
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

    fprintf(stderr,
            "Please enter current value, or \"help\" for other commands.\n");

    /* main loop, read input and process commands */
    while (true) {
        value_t val;
        unsigned operand;

        memset(&val, 0x00, sizeof(val));

        switch (getcommand(matches->size, &operand)) {
        case COMMAND_EXACT:

            /* setup value */
            val.value.u32 = operand;
            val.flags.u32 = 1;
            val.flags.seen = 1;
            val.flags.u8 = (operand == val.value.u8);
            val.flags.u16 = (operand == val.value.u16);

            /* user has specified an exact value of the variable to find */
            if (matches->size) {
                /* already know some matches */
                if (checkmatches(matches, target, val, MATCHEXACT) == -1) {
                    fprintf(stderr,
                            "error: failed to search target address space.\n");
                    ret++;
                    goto end;
                }
            } else {
                /* initial search */
                if (candidates(matches, regions, target, val) == -1) {
                    fprintf(stderr,
                            "error: failed to search target address space.\n");
                    ret++;
                    goto end;
                }
            }
            /* check if we now know the only possible candidate */
            if (matches->size == 1) {
                fprintf(stderr,
                        "info: match identified, use \"set\" to modify value.\n");
                fprintf(stderr, "info: enter \"help\" for other commands.\n");
            }
            break;
        case COMMAND_INCREMENT:
            /* check if user has indicated that the variable is now higher than last time seen */
            if (matches->size) {
                if (checkmatches(matches, target, val, MATCHINCREMENT) == -1) {
                    fprintf(stderr,
                            "error: failed to search target address space.\n");
                    ret++;
                    goto end;
                }
            } else {
                fprintf(stderr,
                        "error: cannot use that search without matches\n");
            }

            if (matches->size == 1) {
                fprintf(stderr,
                        "info: match identified, use \"set\" to modify value.\n");
                fprintf(stderr, "info: enter \"help\" for other commands.\n");
            }
            break;
        case COMMAND_DECREMENT:
            /* the user indicated the value is now lower than last time seen */
            if (matches->size) {
                if (checkmatches(matches, target, val, MATCHDECREMENT) == -1) {
                    fprintf(stderr,
                            "error: failed to search target address space.\n");
                    ret++;
                    goto end;
                }
            } else {
                fprintf(stderr,
                        "error: cannot use that search without matches\n");
            }

            if (matches->size == 1) {
                fprintf(stderr,
                        "info: match identified, use \"set\" to modify value.\n");
                fprintf(stderr, "info: enter \"help\" for other commands.\n");
            }
            break;
        case COMMAND_EQUAL:
            val.value.u32 = operand;
            /* the last seen value is still there */
            if (matches->size) {
                if (checkmatches(matches, target, val, MATCHEQUAL) == -1) {
                    fprintf(stderr,
                            "error: failed to search target address space.\n");
                    ret++;
                    goto end;
                }
            } else {
                fprintf(stderr,
                        "error: cannot use that search without matches\n");
            }

            if (matches->size == 1) {
                fprintf(stderr,
                        "info: match identified, use \"set\" to modify value.\n");
                fprintf(stderr, "info: enter \"help\" for other commands.\n");
            }
            break;

        case COMMAND_CONTINUOUS:
            /* the set command should continually inject the specified value */
            if ((continuous = operand)) {
                fprintf(stderr,
                        "info: use \"set\" to start injecting value every %u seconds.\n",
                        operand);
            } else {
                /* zero operand indicated disable continuous mode */
                fprintf(stderr, "info: continuous mode disabled.\n");
            }
            break;
        case COMMAND_SET:
            if (continuous) {
                fprintf(stderr,
                        "info: setting value every %u seconds until interrupted...\n",
                        continuous);
            }

            val.value.u32 = operand;

            /* set every value in match list to operand */
            while (true) {
                element_t *np = matches->head;

                while (np) {
                    match_t *match = np->data;

                    memcpy(&val.flags, &match->lvalue.flags, sizeof(val.flags));

                    fprintf(stderr, "info: setting *%p to %u...\n",
                            match->address, operand);

                    if (setaddr(target, match->address, &val) == -1) {
                        fprintf(stderr, "error: failed to set a value.\n");
                        ret++;
                        goto end;
                    }

                    np = np->next;
                }

                if (continuous) {
                    sleep(continuous);  /*lint !e534 */
                } else {
                    break;
                }

            }
            break;
        case COMMAND_LIST:{
                unsigned i = 0;
                element_t *np = matches->head;

                /* list all known matches */
                while (np) {
                    char v[32];
                    match_t *match = np->data;

                    snprintf(v, 32, TYPEFMT(match->lvalue),
                             TYPEVAL(match->lvalue));

                    fprintf(stderr, "[%02u] %p {%s, %s} (%s)\n",
                            i++, match->address, TYPESTR(match->lvalue), v,
                            match->region->pathname ? match->region->
                            pathname : "unassociated, typically .bss");
                    np = np->next;
                }
            }
            break;
        case COMMAND_DELETE:{
                unsigned i;
                element_t *np = matches->head;

                /* delete the nth match from the matches list */
                if (operand < matches->size) {
                    for (i = 0; np && i < operand - 1; i++, np = np->next);
                    l_remove(matches, np, NULL);
                } else {
                    fprintf(stderr,
                            "warn: you attempted to delete a non-existant match `%u`.\n",
                            operand);
                    fprintf(stderr,
                            "info: use \"list\" to list matches, or \"help\" for other commands.\n");
                }
            }
            break;
        case COMMAND_PID:
            if (operand) {
                target = operand;
            } else {
                /* print the pid of the target program */
                fprintf(stderr, "info: target pid is %u.\n", target);
                break;
            }
            /*lint -fallthrough */
        case COMMAND_RESET:
            /* forget all matches */
            l_destroy(matches);

            if ((matches = l_init()) == NULL) {
                fprintf(stderr,
                        "error: sorry, there was a memory allocation error.\n");
                ret++;
                goto end;
            }

            /* refresh list of regions */
            l_destroy(regions);
            /* create a new linked list of regions */
            if ((regions = l_init()) == NULL || readmaps(target, regions) == -1) {
                fprintf(stderr,
                        "error: sorry, there was a problem getting a list of regions to search.\n");
                ret++;
                goto end;
            }
            break;
        case COMMAND_EXIT:
            /* exit now */
            goto end;
        case COMMAND_LISTREGIONS:{
                unsigned i = 0;
                element_t *np = regions->head;

                /* print a list of regions that are searched */
                while (np) {
                    region_t *region = np->data;
                    fprintf(stderr, "[%02u] %p, %u bytes, %c%c%c, %s\n", i++,
                            region->start, region->size,
                            region->perms & MAP_RD ? 'r' : '-',
                            region->perms & MAP_WR ? 'w' : '-',
                            region->perms & MAP_EX ? 'x' : '-',
                            region->pathname ? region->
                            pathname : "unassociated");
                    np = np->next;
                }
            }
            break;
        case COMMAND_DELREGIONS:{
                unsigned i;
                element_t *np = regions->head, *t = matches->head, *p = NULL;

                /* delete the nth region */
                if (operand < regions->size) {

                    /* traverse list to element */
                    for (i = 0; np && i < operand - 1; i++, np = np->next);

                    /* check for any affected matches before removing it */
                    while (t) {
                        match_t *match = t->data;
                        region_t *s;

                        /* determine the correct pointer we're supposed to be checking */
                        if (np) {
                            assert(np->next);
                            s = np->next->data;
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
                    l_remove(regions, np, NULL);
                } else {
                    fprintf(stderr,
                            "warn: you attempted to delete a non-existant region `%u`.\n",
                            operand);
                    fprintf(stderr,
                            "info: use \"lregions\" to list regions, or \"help\" for other commands.\n");
                }
            }
            break;
        case COMMAND_SNAPSHOT:
            /* already know some matches */
            if (snapshot(matches, regions, target) == -1) {
                fprintf(stderr,
                        "error: failed to save target address space.\n");
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
            fprintf(stderr,
                    "Please enter current value, or \"help\" for other commands.\n");
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
            "--version\n" "\tprint version message.\n");
    return;
}

void printversion(void)
{
    fprintf(stderr, "scanmem %s - Tavis Ormandy <taviso@sdf.lonestar.org>\n",
            VERSIONSTRING);
    return;
}
