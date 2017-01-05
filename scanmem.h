/*
*
* $Author: taviso $
* $Revision: 1.4 $
*
*/

#ifndef _SCANMEM_INC
#define _SCANMEM_INC /* include guard */

#include <stddef.h>
#include <stdbool.h>
#include <unistd.h>

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
    size_t size;                /* size */
    char *pathname;         /* first 1024 characters */
    unsigned char perms;    /* map permissions */
} __attribute__((__packed__)) region_t;

typedef struct {
    intptr_t address;       /* address of variable */
    region_t *region;       /* region it belongs to */
    unsigned lvalue;            /* last seen of variable */
    unsigned short reg;    /* bitmask of register */
} __attribute__((__packed__)) match_t;

int readmaps(pid_t target, list_t *regions);

int attach(pid_t target);
int detach(pid_t target);
int setaddr(pid_t target, intptr_t addr, unsigned to);

typedef enum { MATCHEXACT, MATCHINCREMENT, MATCHDECREMENT } matchtype_t;

int candidates(list_t *matches, list_t *regions, pid_t target, unsigned value, unsigned width,
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
   COMMAND_MEMORY,
   COMMAND_LISTREGIONS,
   COMMAND_DELREGIONS,
   COMMAND_RESET,
} selection_t;

selection_t getcommand(unsigned value, unsigned *operand);
    
void sighandler(int n);
void printhelp(void);
void printversion(void);
void printinthelp(void);
#endif
