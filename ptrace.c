/*
*
* $Author: taviso $
* $Revision: 1.10 $
*/

#include <sys/ptrace.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <assert.h>
#include <errno.h>
#include <math.h>

#ifdef __GNUC__
# define EXPECT(x,y) __builtin_expect(x, y)
#else
# define EXPECT(x,y) x
#endif

#include "scanmem.h"

static int attach(pid_t target);
static bool peekdata(pid_t pid, void *addr, value_t * result);
static bool valuecmp(value_t * required, value_t * candidate, matchtype_t type);

/* ptrace peek buffer */
static struct {
    union {
        signed long peeks[2];   /* read from ptrace() */
        unsigned char bytes[sizeof(long) * 2];  /* used to retrieve unaligned hits */
    } cache;
    unsigned size;              /* number of entries (in longs) */
    void *base;                 /* base address of cached region */
    pid_t pid;                  /* what pid this applies to */
} peekbuf;


int attach(pid_t target)
{
    int status;

    /* attach, to the target application, which should cause a SIGSTOP */
    if (ptrace(PTRACE_ATTACH, target, NULL, NULL) == -1) {
        fprintf(stderr, "error: failed to attach to %d.\n", target);
        return -1;
    }

    /* wait for the SIGSTOP to take place. */
    if (waitpid(target, &status, 0) == -1 || !WIFSTOPPED(status)) {     /*lint !e10 */
        fprintf(stderr,
                "error: there was an error waiting for the target to stop.\n");
        return -1;
    }

    /* flush the peek buffer */
    memset(&peekbuf, 0x00, sizeof(peekbuf));
    
    /* everything looks okay */
    return 0;

}

int detach(pid_t target)
{
    return ptrace(PTRACE_DETACH, target, NULL, 0);
}

/* cache overlapping ptrace reads to improve performance */
bool peekdata(pid_t pid, void *addr, value_t * result)
{

    assert(peekbuf.size < 3);
    assert(result != NULL);

    /* check if we have a cache hit */
    if (pid == peekbuf.pid && addr >= peekbuf.base
        && (unsigned) (addr + sizeof(unsigned) - peekbuf.base) <=
        sizeof(unsigned) * peekbuf.size) {

        result->value.u32 =
            *(unsigned *) &peekbuf.cache.bytes[addr - peekbuf.base];
        return true;
    } else if (pid == peekbuf.pid && addr >= peekbuf.base
               && (unsigned) (addr - peekbuf.base) <
               sizeof(unsigned) * peekbuf.size) {

        assert(peekbuf.size != 0);

        /* partial hit, remove old entry */

        if (EXPECT(peekbuf.size == 2, true)) {
            peekbuf.cache.peeks[0] = peekbuf.cache.peeks[1];
            peekbuf.size = 1;
            peekbuf.base += sizeof(unsigned);
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
               peekbuf.base + sizeof(unsigned) * peekbuf.size, NULL);

    /* check if ptrace() succeeded */
    if (peekbuf.cache.peeks[peekbuf.size] == -1L && errno != 0) {
        return false;
    }

    /* record new entry in cache */
    peekbuf.size++;

    /* return result to caller */
    result->value.u32 = *(unsigned *) &peekbuf.cache.bytes[addr - peekbuf.base];

    /* success */
    return true;
}

int snapshot(list_t * matches, const list_t * regions, pid_t target)
{
    unsigned regnum = 0;
    element_t *n = regions->head;
    region_t *r;

    /* stop and attach to the target */
    if (attach(target) == -1)
        return -1;

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
        fprintf(stderr, "info: %02u/%02u saving %p - %p.", ++regnum,
                regions->size, r->start, r->start + r->size);
        fflush(stderr);         /*lint !e534 */

        /* for every offset, check if we have a match */
        for (offset = 0; offset < r->size; offset++) {
            value_t check;
            match_t *match;

            memset(&check, 0x0, sizeof(check));

            /* if a region cant be read, skip it */
            if (peekdata(target, r->start + offset, &check) == false) {
                break;
            }

            /* save this new location */
            if ((match = calloc(1, sizeof(match_t))) == NULL) {
                fprintf(stderr, "error: memory allocation failed.\n");
                (void) detach(target);
                return -1;
            }

            match->address = r->start + offset;
            match->region = r;
            match->lvalue = check;

            if (EXPECT(l_append(matches, NULL, match) == -1, false)) {
                fprintf(stderr, "error: unable to add match to list.\n");
                (void) detach(target);
                return -1;
            }

            /* print a simple progress meter. */
            if (EXPECT(offset % ((r->size - (r->size % 10)) / 10) == 10, false)) {
                fprintf(stderr, ".");
                fflush(stderr); /*lint !e534 */
            }
        }

        n = n->next;
        fprintf(stderr, "ok\n");
    }

    eprintf("info: we currently have %d matches.\n", matches->size);

    /* okay, detach */
    return detach(target);
}

int checkmatches(list_t * matches, pid_t target, value_t value,
                 matchtype_t type)
{
    element_t *n, *p;

    assert(matches != NULL);
    assert(matches->size);

    p = NULL;
    n = matches->head;

    /* stop and attach to the target */
    if (attach(target) == -1)
        return -1;

    /* shouldnt happen */
    if (matches->size == 0) {
        fprintf(stderr, "warn: cant check non-existant matches.\n");
        return -1;
    }

    while (n) {
        match_t *match = n->data;
        value_t check;

        /* read value from this address */
        if (peekdata(target, match->address, &check) == false) {

            /* assume this was in unmapped region, so remove */
            fprintf(stderr, "info: peek *%p failed, probably unmapped.\n",
                    match->address);
            l_remove(matches, p, NULL);

            /* confirm this isnt the head element */
            n = p ? p->next : matches->head;
            continue;
        }

        memcpy(&check.flags, &match->lvalue.flags, sizeof(check.flags));

        if (type != MATCHEXACT) {
            memcpy(&value, &match->lvalue, sizeof(value));
        }

        if (valuecmp(&value, &check, type)) {
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

int candidates(list_t * matches, const list_t * regions, pid_t target,
               value_t value)
{
    unsigned regnum = 0;
    element_t *n = regions->head;
    region_t *r;

    /* stop and attach to the target */
    if (attach(target) == -1)
        return -1;

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
        fprintf(stderr, "info: %02u/%02u searching %p - %p.", ++regnum,
                regions->size, r->start, r->start + r->size);
        fflush(stderr);         /*lint !e534 */

        /* for every offset, check if we have a match */
        for (offset = 0; offset < r->size; offset++) {
            value_t check;

            memset(&check, 0x00, sizeof(check));

            /* if a region cant be read, skip it */
            if (peekdata(target, r->start + offset, &check) == false) {
                break;
            }

            /* check if we have a match */
            if (EXPECT(valuecmp(&value, &check, MATCHEXACT), false)) {
                match_t *match;

                /* save this new location */
                if ((match = calloc(1, sizeof(match_t))) == NULL) {
                    fprintf(stderr, "error: memory allocation failed.\n");
                    (void) detach(target);
                    return -1;
                }

                match->address = r->start + offset;
                match->region = r;
                match->lvalue = check;

                if (EXPECT(l_append(matches, NULL, match) == -1, false)) {
                    fprintf(stderr, "error: unable to add match to list.\n");
                    (void) detach(target);
                    return -1;
                }
            }

            /* print a simple progress meter. */
            if (EXPECT(offset % ((r->size - (r->size % 10)) / 10) == 10, false)) {
                fprintf(stderr, ".");
                fflush(stderr); /*lint !e534 */
            }
        }

        n = n->next;
        fprintf(stderr, "ok\n");
    }

    eprintf("info: we currently have %d matches.\n", matches->size);

    /* okay, detach */
    return detach(target);
}

int setaddr(pid_t target, void *addr, value_t * to)
{
    value_t saved;

    memset(&saved, 0x00, sizeof(saved));

    if (attach(target) == -1) {
        return -1;
    }

    if (peekdata(target, addr, &saved) == false) {
        return -1;
    }

    if (to->flags.u32)
        saved.value.u32 = to->value.u32;
    else if (to->flags.u16)
        saved.value.u16 = to->value.u16;
    else if (to->flags.u8)
        saved.value.u8 = to->value.u8;
    else {
        fprintf(stderr, "error: could not determine type to poke.\n");
        return -1;
    }

    if (ptrace(PTRACE_POKEDATA, target, addr, saved.value.u32) == -1) {
        return -1;
    }

    return detach(target);
}

bool valuecmp(value_t * required, value_t * candidate, matchtype_t type)
{
    bool ret = false, seen = false;

    assert(required != NULL);
    assert(candidate != NULL);

    seen = candidate->flags.seen & required->flags.seen;


    switch (type) {
    case MATCHEXACT:
    case MATCHEQUAL:
        if (!seen
            || (required->flags.u8
                && (candidate->flags.u8 || !candidate->flags.seen)))
            ret |= candidate->flags.u8 =
                (candidate->value.u8 == required->value.u8);
        if (!seen
            || (required->flags.u16
                && (candidate->flags.u16 || !candidate->flags.seen)))
            ret |= candidate->flags.u16 =
                (candidate->value.u16 == required->value.u16);
        if (!seen
            || (required->flags.u32
                && (candidate->flags.u32 || !candidate->flags.seen)))
            ret |= candidate->flags.u32 =
                (candidate->value.u32 == required->value.u32);
        break;
    case MATCHINCREMENT:
        if (!seen
            || (required->flags.u8
                && (candidate->flags.u8 || !candidate->flags.seen)))
            ret |= candidate->flags.u8 =
                (candidate->value.u8 > required->value.u8);
        if (!seen
            || (required->flags.u16
                && (candidate->flags.u16 || !candidate->flags.seen)))
            ret |= candidate->flags.u16 =
                (candidate->value.u16 > required->value.u16);
        if (!seen
            || (required->flags.u32
                && (candidate->flags.u32 || !candidate->flags.seen)))
            ret |= candidate->flags.u32 =
                (candidate->value.u32 > required->value.u32);
        break;
    case MATCHDECREMENT:
        if (!seen
            || (required->flags.u8
                && (candidate->flags.u8 || !candidate->flags.seen)))
            ret |= candidate->flags.u8 =
                (candidate->value.u8 < required->value.u8);
        if (!seen
            || (required->flags.u16
                && (candidate->flags.u16 || !candidate->flags.seen)))
            ret |= candidate->flags.u16 =
                (candidate->value.u16 < required->value.u16);
        if (!seen
            || (required->flags.u32
                && (candidate->flags.u32 || !candidate->flags.seen)))
            ret |= candidate->flags.u32 =
                (candidate->value.u32 < required->value.u32);
        break;
    }

    candidate->flags.seen = 1;

    return ret;
}
