/*
*
* $Author: taviso $
* $Revision: 1.6 $
*
*/

#ifndef _SCANMEM_INC
#define _SCANMEM_INC /* include guard */

#include <stdint.h>
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

#define MAP_RD (1<<0)
#define MAP_WR (1<<1)
#define MAP_EX (1<<2)
#define MAP_SH (1<<3)
#define MAP_PR (1<<4)

typedef struct {
    intptr_t start;         /* start address */
    size_t size;            /* size */
    char *pathname;         /* first 1024 characters */
    unsigned char perms;    /* map permissions */
} region_t;

typedef struct {
    intptr_t address;      /* address of variable */
    region_t *region;      /* region it belongs to */
    unsigned lvalue;       /* last seen of variable */
#if 0
    unsigned short reg;    /* TODO: bitmask of register */
#endif    
}match_t;

int readmaps(pid_t target, list_t *regions);

int detach(pid_t target);
int setaddr(pid_t target, intptr_t addr, unsigned to);

typedef enum { MATCHEXACT, MATCHINCREMENT, MATCHDECREMENT } matchtype_t;

int candidates(list_t *matches, const list_t *regions, pid_t target, unsigned value, unsigned width,
               matchtype_t type);

typedef enum {
   COMMAND_ERROR = -1,
   COMMAND_EXACT,
   COMMAND_INCREMENT,
   COMMAND_DECREMENT,
   COMMAND_CONTINUOUS,
   COMMAND_SET,
   COMMAND_EXIT,
   COMMAND_HELP,
   COMMAND_VERSION,
   COMMAND_DELETE,
   COMMAND_LIST,
   COMMAND_WIDTH,
   COMMAND_PID,
   COMMAND_LISTREGIONS,
   COMMAND_DELREGIONS,
   COMMAND_RESET
} selection_t;

selection_t getcommand(unsigned value, unsigned *operand);

void printinthelp(void);
void printversion(void);
#endif
