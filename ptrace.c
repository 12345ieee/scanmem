/*
*
* $Id: ptrace.c,v 1.16 2007-04-08 23:09:18+01 taviso Exp $
*/

#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <stdbool.h>

#ifdef __GNUC__
# define EXPECT(x,y) __builtin_expect(x, y)
#else
# define EXPECT(x,y) x
#endif

#include "value.h"
#include "scanmem.h"

/* ptrace peek buffer, used by peekdata() */
static struct {
    union {
        signed long peeks[2];   /* read from ptrace() */
        unsigned char bytes[sizeof(long) * 2];  /* used to retrieve unaligned hits */
    } cache;
    unsigned size;              /* number of entries (in longs) */
    char *base;                 /* base address of cached region */
    pid_t pid;                  /* what pid this applies to */
} peekbuf;


bool attach(pid_t target)
{
    int status;

    /* attach, to the target application, which should cause a SIGSTOP */
    if (ptrace(PTRACE_ATTACH, target, NULL, NULL) == -1L) {
        fprintf(stderr, "error: failed to attach to %d, %s\n", target,
                strerror(errno));
        return false;
    }

    /* wait for the SIGSTOP to take place. */
    if (waitpid(target, &status, 0) == -1 || !WIFSTOPPED(status)) {
        fprintf(stderr,
                "error: there was an error waiting for the target to stop.\n");
        fprintf(stdout, "info: %s\n", strerror(errno));
        return false;
    }

    /* flush the peek buffer */
    memset(&peekbuf, 0x00, sizeof(peekbuf));

    /* everything looks okay */
    return true;

}

bool detach(pid_t target)
{
    return ptrace(PTRACE_DETACH, target, NULL, 0) == 0;
}

/*
 * peekdata - caches overlapping ptrace reads to improve performance.
 * 
 * This routine could just call ptrace(PEEKDATA), but using a cache reduces
 * the number of peeks required by 70% when reading large chunks of
 * consecutive addreses.
 */

bool peekdata(pid_t pid, void *addr, value_t * result)
{
    char *reqaddr = addr;

    assert(peekbuf.size < 3);
    assert(result != NULL);

    memset(result, 0x00, sizeof(value_t));

    valnowidth(result);

    /* check if we have a cache hit */
    if (pid == peekbuf.pid && reqaddr >= peekbuf.base
        && (unsigned) (reqaddr + sizeof(long) - peekbuf.base) <=
        sizeof(long) * peekbuf.size) {

        result->value.tslong = *(long *) &peekbuf.cache.bytes[reqaddr - peekbuf.base];  /*lint !e826 */
        return true;
    } else if (pid == peekbuf.pid && reqaddr >= peekbuf.base
               && (unsigned) (reqaddr - peekbuf.base) <
               sizeof(long) * peekbuf.size) {

        assert(peekbuf.size != 0);

        /* partial hit, we have some of the data but not all, so remove old entry */

        if (EXPECT(peekbuf.size == 2, true)) {
            peekbuf.cache.peeks[0] = peekbuf.cache.peeks[1];
            peekbuf.size = 1;
            peekbuf.base += sizeof(long);
        }
    } else {

        /* cache miss, invalidate cache */
        peekbuf.pid = pid;
        peekbuf.size = 0;
        peekbuf.base = addr;
    }

    /* we need a ptrace() to complete request */
    errno = 0;

    peekbuf.cache.peeks[peekbuf.size] =
        ptrace(PTRACE_PEEKDATA, pid,
               peekbuf.base + sizeof(long) * peekbuf.size, NULL);

    /* check if ptrace() succeeded */
    if (EXPECT(peekbuf.cache.peeks[peekbuf.size] == -1L && errno != 0, false)) {
        /* i wont print a message here, would be very noisy if a region is unmapped */
        return false;
    }

    /* record new entry in cache */
    peekbuf.size++;

    /* return result to caller */
    result->value.tslong = *(long *) &peekbuf.cache.bytes[reqaddr - peekbuf.base];      /*lint !e826 */

    /* success */
    return true;
}

bool snapshot(list_t * matches, const list_t * regions, pid_t target)
{
    unsigned regnum = 0;
    element_t *n = regions->head;
    region_t *r;

    /* stop and attach to the target */
    if (attach(target) == false)
        return false;

    /* make sure we have some regions to search */
    if (regions->size == 0) {
        fprintf(stderr,
                "warn: no regions defined, perhaps you deleted them all?\n");
        fprintf(stderr,
                "info: use the \"reset\" command to refresh regions.\n");
        return detach(target);
    }

    /* first time, we have to check every memory region */
    while (n) {
        unsigned offset;

        r = n->data;

        /* print a progress meter so user knows we havnt crashed */
        fprintf(stderr, "info: %02u/%02u saving %10p - %10p.", ++regnum,
                regions->size, r->start, r->start + r->size);
        fflush(stderr);

        /* for every offset, check if we have a match */
        for (offset = 0; offset < r->size; offset++) {
            value_t check;
            match_t *match;

            /* if a region cant be read, skip it */
            if (EXPECT
                (peekdata(target, r->start + offset, &check) == false, false)) {
                break;
            }

            /* save this new location */
            if ((match = calloc(1, sizeof(match_t))) == NULL) {
                fprintf(stderr, "error: memory allocation failed.\n");
                (void) detach(target);
                return false;
            }

            match->address = r->start + offset;
            match->region = r;
            match->lvalue = check;

            if (EXPECT(l_append(matches, NULL, match) == -1, false)) {
                fprintf(stderr, "error: unable to add match to list.\n");
                (void) detach(target);
                free(match);
                return false;
            }

            /* print a simple progress meter. */
            if (EXPECT(offset % ((r->size - (r->size % 10)) / 10) == 10, false)) {
                fprintf(stderr, ".");
                fflush(stderr);
            }
        }

        n = n->next;
        fprintf(stderr, "ok\n");
    }

    eprintf("info: we currently have %d matches.\n", matches->size);

    /* okay, detach */
    return detach(target);
}

bool checkmatches(list_t * matches, pid_t target, value_t value,
                  matchtype_t type)
{
    element_t *n, *p;

    assert(matches != NULL);
    assert(matches->size);

    p = NULL;
    n = matches->head;

    /* stop and attach to the target */
    if (attach(target) == false)
        return false;

    /* shouldnt happen */
    if (matches->size == 0) {
        fprintf(stderr, "warn: cant check non-existant matches.\n");
        return false;
    }

    while (n) {
        match_t *match = n->data;
        value_t check;

        /* read value from this address */
        if (EXPECT(peekdata(target, match->address, &check) == false, false)) {

            /* assume this was in unmapped region, so remove */
            l_remove(matches, p, NULL);

            /* confirm this isnt the head element */
            n = p ? p->next : matches->head;
            continue;
        }

        truncval(&check, &match->lvalue);

        /* XXX: this sucks. */
        if (type != MATCHEXACT) {
            valcpy(&value, &match->lvalue);
        }

        if (EXPECT(valuecmp(&check, type, &value, &check), false)) {
            /* still a candidate */
            match->lvalue = check;
            p = n;
            n = n->next;
        } else {
            /* no match */
            l_remove(matches, p, NULL);

            /* confirm this isnt the head element */
            n = p ? p->next : matches->head;
        }
    }

    eprintf("info: we currently have %d matches.\n", matches->size);

    /* okay, detach */
    return detach(target);
}

bool candidates(list_t * matches, const list_t * regions, pid_t target,
                value_t value)
{
    unsigned regnum = 0;
    element_t *n = regions->head;
    region_t *r;

    /* stop and attach to the target */
    if (attach(target) == false)
        return false;

    /* make sure we have some regions to search */
    if (regions->size == 0) {
        fprintf(stderr,
                "warn: no regions defined, perhaps you deleted them all?\n");
        fprintf(stderr,
                "info: use the \"reset\" command to refresh regions.\n");
        return detach(target);
    }

    /* first time, we have to check every memory region */
    while (n) {
        unsigned offset;

        r = n->data;

        /* print a progress meter so user knows we havnt crashed */
        fprintf(stderr, "info: %02u/%02u searching %10p - %10p.", ++regnum,
                regions->size, r->start, r->start + r->size);
        fflush(stderr);

        /* for every offset, check if we have a match */
        for (offset = 0; offset < r->size; offset++) {
            value_t check;

            /* if a region cant be read, skip it */
            if (peekdata(target, r->start + offset, &check) == false) {
                break;
            }

            /* check if we have a match */
            if (EXPECT(valuecmp(&value, MATCHEQUAL, &check, &check), false)) {
                match_t *match;

                /* save this new location */
                if ((match = calloc(1, sizeof(match_t))) == NULL) {
                    fprintf(stderr, "error: memory allocation failed.\n");
                    (void) detach(target);
                    return false;
                }

                match->address = r->start + offset;
                match->region = r;
                match->lvalue = check;

                if (EXPECT(l_append(matches, NULL, match) == -1, false)) {
                    fprintf(stderr, "error: unable to add match to list.\n");
                    (void) detach(target);
                    free(match);
                    return false;
                }
            }

            /* print a simple progress meter. */
            if (EXPECT(offset % ((r->size - (r->size % 10)) / 10) == 10, false)) {
                fprintf(stderr, ".");
                fflush(stderr);
            }
        }

        n = n->next;
        fprintf(stderr, "ok\n");
    }

    eprintf("info: we currently have %d matches.\n", matches->size);

    /* okay, detach */
    return detach(target);
}

bool setaddr(pid_t target, void *addr, const value_t * to)
{
    value_t saved;

    if (attach(target) == false) {
        return false;
    }

    if (peekdata(target, addr, &saved) == false) {
        fprintf(stderr, "error: couldnt access the target address %10p\n",
                addr);
        return false;
    }

    /* XXX: oh god */
    if (to->flags.wlong)
        saved.value.tulong = to->value.tulong;
    else if (to->flags.wint)
        saved.value.tuint = to->value.tuint;
    else if (to->flags.wshort)
        saved.value.tushort = to->value.tushort;
    else if (to->flags.wchar)
        saved.value.tuchar = to->value.tuchar;
    else if (to->flags.tfloat)
        saved.value.tfloat = to->value.tslong;
    else {
        fprintf(stderr, "error: could not determine type to poke.\n");
        return false;
    }

    if (ptrace(PTRACE_POKEDATA, target, addr, saved.value.tulong) == -1L) {
        return false;
    }

    return detach(target);
}
