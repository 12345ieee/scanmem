/*
*
* $Author: taviso $
* $Revision: 1.10 $
*
*/

#ifndef _SCANMEM_INC
#define _SCANMEM_INC            /* include guard */

#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>
#include "list.h"

#define DEBUG

#ifndef VERSIONSTRING
# define VERSIONSTRING "(unknown)"
#endif

#ifdef DEBUG
# define eprintf(x, y...) fprintf(stderr, x, ## y)
#else
# define eprintf(x, y...)
#endif

#ifdef _lint
# define snprintf(a, b, c...) sprintf(a, ## c)
#endif

#ifndef MIN
# define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

#define MAP_RD (1<<0)
#define MAP_WR (1<<1)
#define MAP_EX (1<<2)
#define MAP_SH (1<<3)
#define MAP_PR (1<<4)

/* a region obtained from /proc/pid/maps, these are searched for matches */
typedef struct {
    void *start;                /* start address */
    size_t size;                /* size */
    char *pathname;             /* first 1024 characters */
    unsigned char perms;        /* map permissions */
} region_t;

/* each dword read from the target is checked if they're numeric value matches in multiple types */
typedef union {
    uint8_t u8;
    uint16_t u16;
    uint32_t u32;
} types_t;

typedef struct __attribute__ ((packed)) {
    types_t value;
    struct __attribute__ ((packed)) {
        unsigned seen:1;
        unsigned u8:1;
        unsigned u16:1;
        unsigned u32:1;
    } flags;
} value_t;

/* this structure represents one known match, its address and type. */
typedef struct __attribute__ ((packed)) {
    void *address;              /* address of variable */
    region_t *region;           /* region it belongs to */
    value_t lvalue;             /* last seen value */
} match_t;


#define TYPESTR(s) ((s.flags.u32) ? "uint32" : \
        (s.flags.u16) ? "uint16" : \
                (s.flags.u8) ? "uint8" : \
			       	"no match")

#define TYPEVAL(s) ((s.flags.u32) ? s.value.u32 : \
        (s.flags.u16) ? s.value.u16 : \
            (s.flags.u8) ? s.value.u8 : \
                    0U)

#define TYPEFMT(s) ((s.flags.u32) ? "%u" : \
        (s.flags.u16) ? "%hu" : \
                (s.flags.u8) ? "%hhu" : \
                    "%u")

int readmaps(pid_t target, list_t * regions);
int detach(pid_t target);
int setaddr(pid_t target, void *addr, value_t * to);

typedef enum { MATCHEXACT, MATCHINCREMENT, MATCHDECREMENT,
        MATCHEQUAL } matchtype_t;

int checkmatches(list_t * matches, pid_t target, value_t value,
                 matchtype_t type);
int candidates(list_t * matches, const list_t * regions, pid_t target,
               value_t value);
int snapshot(list_t * matches, const list_t * regions, pid_t target);

typedef enum {
    COMMAND_ERROR = -1,
    COMMAND_EXACT,
    COMMAND_INCREMENT,
    COMMAND_DECREMENT,
    COMMAND_EQUAL,
    COMMAND_CONTINUOUS,
    COMMAND_SET,
    COMMAND_EXIT,
    COMMAND_HELP,
    COMMAND_VERSION,
    COMMAND_DELETE,
    COMMAND_LIST,
    COMMAND_PID,
    COMMAND_LISTREGIONS,
    COMMAND_DELREGIONS,
    COMMAND_RESET,
    COMMAND_SNAPSHOT
} selection_t;

selection_t getcommand(unsigned value, unsigned *operand);

void printinthelp(void);
void printversion(void);

#endif
