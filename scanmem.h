/*
*
* $Id: scanmem.h,v 1.15 2007-04-08 23:09:18+01 taviso Exp $
*
*/

#ifndef _SCANMEM_INC
#define _SCANMEM_INC            /* include guard */

#include <stdint.h>
#include <sys/types.h>          /*lint !e537 */

#include "list.h"
#include "value.h"

/* list of functions where i dont want to be warned about ignored return code */
/*lint -esym(534,detach,printversion,strftime,fflush) */

#ifndef VERSIONSTRING
# define VERSIONSTRING "(unknown)"
#endif

#ifndef NDEBUG
# define eprintf(x, y...) fprintf(stderr, x, ## y)
#else
# define eprintf(x, y...)
#endif

#ifdef _lint
/*lint -save -e652 -e683 -e547 */
# define snprintf(a, b, c...) (((void) b), sprintf(a, ## c))
# define strtoll(a,b,c) ((long long) strtol(a,b,c))
# define WIFSTOPPED
# define sighandler_t _sigfunc_t
/*lint -restore */
/*lint -save -esym(526,getline,strdupa,strdup,strndupa,strtoll) */
ssize_t getline(char **lineptr, size_t * n, FILE * stream);
char *strndupa(const char *s, size_t n);
char *strdupa(const char *s);
char *strdup(const char *s);
/*lint -restore */
#endif
#ifdef __CSURF__
# define waitpid(x,y,z) ((*(y)=0),-rand())
# define WIFSTOPPED(x) (rand())
# define ptrace(w,x,y,z) ((errno=rand()),(ptrace(w,x,y,z)))
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
    char *start;                /* start address */
    size_t size;                /* size */
    char *pathname;             /* associated file */
    unsigned char perms;        /* map permissions */
} region_t;

/* global settings */
typedef struct {
    unsigned exit:1;
    pid_t target;
    list_t *matches;
    list_t *regions;
    list_t *commands;
} globals_t;

/* this structure represents one known match, its address and type. */
typedef struct {
    void *address;              /* address of variable */
    region_t *region;           /* region it belongs to */
    value_t lvalue;             /* last seen value */
} match_t;


/* global settings */
extern globals_t globals;

bool readmaps(pid_t target, list_t * regions);
bool detach(pid_t target);
bool setaddr(pid_t target, void *addr, const value_t * to);
bool checkmatches(list_t * matches, pid_t target, value_t value,
                  matchtype_t type);
bool candidates(list_t * matches, const list_t * regions, pid_t target,
                value_t value);
bool snapshot(list_t * matches, const list_t * regions, pid_t target);
bool peekdata(pid_t pid, void *addr, value_t * result);
bool attach(pid_t target);
bool getcommand(globals_t * vars, char **line);
int printversion(FILE * fp);
#endif
