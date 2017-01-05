/*
*
* $Id: handlers.c,v 1.6 2007-04-08 23:09:17+01 taviso Exp $
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
#include <assert.h>
#include <setjmp.h>
#include <alloca.h>
#include <strings.h>            /*lint -esym(526,strcasecmp) */
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <sys/time.h>

#include <readline/readline.h>

#include "scanmem.h"
#include "commands.h"
#include "handlers.h"
#include "interrupt.h"

#define USEPARAMS() ((void) vars, (void) argv, (void) argc)     /* macro to hide gcc unused warnings */

/*lint -esym(818, vars, argv) dont want want to declare these as const */

/*
 * This file defines all the command handlers used, each one is registered using
 * registercommand(), and when a matching command is entered, the commandline is
 * tokenized and parsed into an argv/argc.
 * 
 * argv[0] will contain the command entered, so one handler can handle multiple
 * commands by checking whats in there, but you still need seperate documentation
 * for each command when you register it.
 *
 * Most commands will also need some documentation, see handlers.h for the format.
 *
 * Commands are allowed to read and modify settings in the vars structure.
 *
 */

#define calloca(x,y) (memset(alloca((x) * (y)), 0x00, (x) * (y)))

bool handler__set(globals_t * vars, char **argv, unsigned argc)
{
    unsigned block, seconds = 1;
    char *delay = NULL;
    bool cont = false;
    struct setting {
        char *matchids;
        char *value;
        unsigned seconds;
    } *settings = NULL;

    assert(argc != 0);
    assert(argv != NULL);
    assert(vars != NULL);

    if (argc < 2) {
        fprintf(stderr,
                "error: expected an argument, type `help set` for details.\n");
        return false;
    }

    /* check if there are any matches */
    if (vars->matches->size == 0) {
        fprintf(stderr, "error: no matches are known yet.\n");
        return false;
    }

    /* --- parse arguments into settings structs --- */

    settings = calloca(argc - 1, sizeof(struct setting));

    /* parse every block into a settings struct */
    for (block = 0; block < argc - 1; block++) {

        /* first seperate the block into matches and value, which are separated by '=' */
        if ((settings[block].value = strchr(argv[block + 1], '=')) == NULL) {

            /* no '=' found, whole string must be the value */
            settings[block].value = argv[block + 1];
        } else {
            /* there is a '=', value+1 points to value string. */

            /* use strndupa() to copy the matchids into a new buffer */
            settings[block].matchids =
                strndupa(argv[block + 1],
                         (size_t) (settings[block].value++ - argv[block + 1]));
        }

        /* value points to the value string, possibly with a delay suffix */

        /* matchids points to the match-ids (possibly multiple) or NULL */

        /* now check for a delay suffix (meaning continuous mode), eg 0xff/10 */
        if ((delay = strchr(settings[block].value, '/')) != NULL) {
            char *end = NULL;

            /* parse delay count */
            settings[block].seconds = strtoul(delay + 1, &end, 10);

            if (*(delay + 1) == '\0') {
                /* empty delay count, eg: 12=32/ */
                fprintf(stderr,
                        "error: you specified an empty delay count, `%s`, see `help set`.\n",
                        settings[block].value);
                return false;
            } else if (*end != '\0') {
                /* parse failed before end, probably trailing garbage, eg 34=9/16foo */
                fprintf(stderr,
                        "error: trailing garbage after delay count, `%s`.\n",
                        settings[block].value);
                return false;
            } else if (settings[block].seconds == 0) {
                /* 10=24/0 disables continous mode */
                fprintf(stderr,
                        "info: you specified a zero delay, disabling continuous mode.\n");
            } else {
                /* valid delay count seen and understood */
                fprintf(stderr,
                        "info: setting %s every %u seconds until interrupted...\n",
                        settings[block].matchids ? settings[block].
                        matchids : "all", settings[block].seconds);

                /* continuous mode on */
                cont = true;
            }

            /* remove any delay suffix from the value */
            settings[block].value =
                strndupa(settings[block].value,
                         (size_t) (delay - settings[block].value));
        }                       /* if (strchr('/')) */
    }                           /* for(block...) */

    /* --- setup a longjmp to handle interrupt if in continuous mode --- */
    if (cont) {
        if (INTERRUPTABLE()) {
            /* control returns here when interrupted */
            (void) detach(vars->target);
            ENDINTERRUPTABLE();
            return true;
        }
    }

    /* --- execute the parsed setting structs --- */

    while (true) {
        unsigned i;
        value_t val;
        char *end = NULL;
        element_t *np = vars->matches->head;

        /* for every settings struct */
        for (block = 0; block < argc - 1; block++) {

            /* reset linked list ptr */
            np = vars->matches->head;

            /* check if this block has anything to do this iteration */
            if (seconds != 1) {
                /* not the first iteration (all blocks get executed first iteration) */

                /* if settings.seconds is zero, then this block is only executed once */
                /* if seconds % settings.seconds is zero, then this block must be executed */
                if (settings[block].seconds == 0
                    || (seconds % settings[block].seconds) != 0)
                    continue;
            }

            /* convert value */
            strtoval(settings[block].value, &end, 0x00, &val);

            /* check that converted successfully */
            if (*end != '\0') {
                fprintf(stderr, "error: could not parse value `%s`\n",
                        settings[block].value);
                ENDINTERRUPTABLE();
                return false;
            }

            /* check if specific match(s) were specified */
            if (settings[block].matchids != NULL) {
                char *id, *lmatches = NULL;
                unsigned num = 0;

                /* create local copy of the matchids for strtok() to modify */
                lmatches = strdupa(settings[block].matchids);

                /* now seperate each match, spearated by commas */
                while ((id = strtok(lmatches, ",")) != NULL) {
                    match_t *match;

                    /* set to NULL for strtok() */
                    lmatches = NULL;

                    /* parse this id */
                    num = strtoul(id, &end, 0x00);

                    /* check that succeeded */
                    if (*id == '\0' || *end != '\0') {
                        fprintf(stderr,
                                "error: could not parse match id `%s`\n", id);
                        ENDINTERRUPTABLE();
                        return false;
                    }

                    /* check this is a valid match-id */
                    if (num < vars->matches->size) {
                        value_t v;

                        /*lint -e722 semi-colon intended, skip to correct node */
                        for (i = 0, np = vars->matches->head; i < num;
                             i++, np = np->next);

                        match = np->data;

                        fprintf(stderr, "info: setting *%p to %#x...\n",
                                match->address, val.value.tuint);

                        /* copy val onto v */
                        valcpy(&v, &val);

                        /* XXX: valcmp? make sure the sizes match */
                        truncval(&v, &match->lvalue);

                        /* set the value specified */
                        if (setaddr(vars->target, match->address, &v) == false) {
                            fprintf(stderr, "error: failed to set a value.\n");
                            ENDINTERRUPTABLE();
                            return false;
                        }

                    } else {
                        /* match-id > than number of matches */
                        fprintf(stderr,
                                "error: found an invalid match-id `%s`\n", id);
                        ENDINTERRUPTABLE();
                        return false;
                    }           /* if (num < matches->size) else ... */
                }               /* while(strtok) */
            } else {

                /* user wants to set all matches */
                while (np) {
                    match_t *match = np->data;

                    /* XXX: as above : make sure the sizes match */
                    truncval(&val, &match->lvalue);

                    fprintf(stderr, "info: setting *%p to %#x...\n",
                            match->address, val.value.tuint);


                    if (setaddr(vars->target, match->address, &val) == false) {
                        fprintf(stderr, "error: failed to set a value.\n");
                        ENDINTERRUPTABLE();
                        return false;
                    }

                    np = np->next;
                }
            }                   /* if (matchid != NULL) else ... */
        }                       /* for(block) */

        if (cont) {
            (void) sleep(1);
        } else {
            break;
        }

        seconds++;
    }                           /* while(true) */

    ENDINTERRUPTABLE();
    return true;

}

/*XXX: remove handler__cont() in next version */
bool handler__cont(globals_t * vars, char **argv, unsigned argc)
{
    USEPARAMS();

    fprintf(stderr,
            "warn: the `cont` command has been deprecated, use `set` instead.\n");
    fprintf(stdout,
            "info: type `help set` to see the new syntax and examples.\n");
    return false;
}

/*XXX: add flags legend to list longdoc */
/* XXX: add yesno command to check if matches > 099999 */
/* example: [012] 0xffffff, csLfznu, 120, /lib/libc.so */

bool handler__list(globals_t * vars, char **argv, unsigned argc)
{
    unsigned i = 0;

    USEPARAMS();

    element_t *np = vars->matches->head;

    /* list all known matches */
    while (np) {
        char v[32];
        match_t *match = np->data;

        if (valtostr(&match->lvalue, v, sizeof(v)) != true) {
            strncpy(v, "unknown", sizeof(v));
        }

        fprintf(stderr, "[%2u] %10p, %s, %s\n", i++, match->address, v,
                match->region->pathname ? match->region->
                pathname : "unassociated, typically .bss");
        np = np->next;
    }

    return true;
}

/* XXX: handle multiple deletes, eg delete !1 2 3 4 5 6
   rememvber to check the id-s work correctly, and deleteing one doesnt fux it up.
*/
bool handler__delete(globals_t * vars, char **argv, unsigned argc)
{
    unsigned i, id;
    element_t *np = vars->matches->head;
    char *end = NULL;

    if (argc != 2) {
        fprintf(stderr,
                "error: was expecting one argument, see `help delete`.\n");
        return false;
    }

    /* parse argument */
    id = strtoul(argv[1], &end, 0x00);

    /* check that strtoul() worked */
    if (argv[1][0] == '\0' || *end != '\0') {
        fprintf(stderr, "error: sorry, couldnt parse `%s`, try `help delete`\n",
                argv[1]);
        return false;
    }

    /* check this is a valid match-id */
    if (id >= vars->matches->size) {
        fprintf(stderr, "warn: you specified a non-existant match `%u`.\n", id);
        fprintf(stderr,
                "info: use \"list\" to list matches, or \"help\" for other commands.\n");
        return false;
    }

    /*lint -e722 skip to the correct node, semi-colon intended */
    for (i = 0; np && i < id - 1; i++, np = np->next);
    l_remove(vars->matches, np, NULL);

    return true;
}

bool handler__reset(globals_t * vars, char **argv, unsigned argc)
{

    USEPARAMS();

    l_destroy(vars->matches);

    if ((vars->matches = l_init()) == NULL) {
        fprintf(stderr, "error: sorry, there was a memory allocation error.\n");
        return false;
    }

    /* refresh list of regions */
    l_destroy(vars->regions);

    /* create a new linked list of regions */
    if ((vars->regions = l_init()) == NULL) {
        fprintf(stderr,
                "error: sorry, there was a problem allocating memory.\n");
        return false;
    }

    /* read in maps if a pid is known */
    if (vars->target && readmaps(vars->target, vars->regions) != true) {
        fprintf(stderr,
                "error: sorry, there was a problem getting a list of regions to search.\n");
        fprintf(stderr,
                "warn: the pid may be invalid, or you dont have permission.\n");
        vars->target = 0;
        return false;
    }

    return true;
}

bool handler__pid(globals_t * vars, char **argv, unsigned argc)
{
    char *resetargv[] = { "reset", NULL };
    char *end = NULL;

    if (argc == 2) {
        vars->target = (pid_t) strtoul(argv[1], &end, 0x00);

        if (vars->target == 0) {
            fprintf(stderr, "error: `%s` does not look like a valid pid.\n",
                    argv[1]);
            return false;
        }
    } else if (vars->target) {
        /* print the pid of the target program */
        fprintf(stderr, "info: target pid is %u.\n", vars->target);
        return true;
    } else {
        fprintf(stderr, "info: no target is currently set.\n");
        return false;
    }

    return handler__reset(vars, resetargv, 1);
}

bool handler__snapshot(globals_t * vars, char **argv, unsigned argc)
{
    USEPARAMS();

    /* check that a pid has been specified */
    if (vars->target == 0) {
        fprintf(stderr, "error: no target set, type `help pid`.\n");
        return false;
    }

    /* remove any existing matches */
    l_destroy(vars->matches);

    /* allocate new matches list */
    if ((vars->matches = l_init()) == NULL) {
        fprintf(stderr, "error: sorry, there was a memory allocation error.\n");
        return false;
    }

    if (snapshot(vars->matches, vars->regions, vars->target) != true) {
        fprintf(stderr, "error: failed to save target address space.\n");
        return false;
    }

    return true;
}

/* XXXX: REALL Y NOT READY */
bool handler__dregion(globals_t * vars, char **argv, unsigned argc)
{
    unsigned i, id;
    char *end = NULL;
    element_t *np = vars->regions->head, *t = vars->matches->head, *p = NULL;

    USEPARAMS();

    if (argc < 2) {
        fprintf(stderr,
                "error: expected at least one argument, see `help dregion`.\n");
        return false;
    }

    id = strtoul(argv[1], &end, 0x00);

    if (*end != '\0') {
        fprintf(stderr, "error: could not parse argument %s.\n", argv[1]);
        return false;
    }

    /* check that there is a process known */
    if (vars->target == 0) {
        fprintf(stderr, "error: no target specified, see `help pid`\n");
        return false;
    }

    /* delete the nth region */
    if (id < vars->regions->size) {

        /*lint !e722 traverse list to element, semi-colon intended */
        for (i = 0; np && i < id - 1; i++, np = np->next);

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
                s = vars->regions->head->data;
            }

            /* check if this one should go */
            if (match->region == s) {
                /* remove this match */
                l_remove(vars->matches, p, NULL);

                /* move to next element */
                t = p ? p->next : vars->matches->head;
            } else {
                p = t;
                t = t->next;
            }
        }

        l_remove(vars->regions, np, NULL);
    } else {
        fprintf(stderr,
                "warn: you attempted to delete a non-existant region `%u`.\n",
                id);
        fprintf(stderr,
                "info: use \"lregions\" to list regions, or \"help\" for other commands.\n");
        return false;
    }

    return true;
}

bool handler__lregions(globals_t * vars, char **argv, unsigned argc)
{
    unsigned i = 0;
    element_t *np = vars->regions->head;

    USEPARAMS();

    if (vars->target == 0) {
        fprintf(stderr,
                "error: no target has been specified, see `help pid`.\n");
        return false;
    }

    /* print a list of regions that are searched */
    while (np) {
        region_t *region = np->data;

        fprintf(stderr, "[%2u] %10p, %7u bytes, %c%c%c, %s\n",
                i++, region->start, region->size,
                region->perms & MAP_RD ? 'r' : '-',
                region->perms & MAP_WR ? 'w' : '-',
                region->perms & MAP_EX ? 'x' : '-',
                region->pathname ? region->pathname : "unassociated");
        np = np->next;
    }

    return true;
}

bool handler__decinc(globals_t * vars, char **argv, unsigned argc)
{
    value_t val;
    matchtype_t m;

    USEPARAMS();

    memset(&val, 0x00, sizeof(val));

    switch (argv[0][0]) {
    case '=':
        m = MATCHEQUAL;
        break;
    case '<':
        m = MATCHLESSTHAN;
        break;
    case '>':
        m = MATCHGREATERTHAN;
        break;
    default:
        fprintf(stderr,
                "error: unrecogised match type seen at decinc handler.\n");
        return false;
    }

    /* the last seen value is still there */
    if (vars->matches->size) {
        if (checkmatches(vars->matches, vars->target, val, m) == false) {
            fprintf(stderr, "error: failed to search target address space.\n");
            return false;
        }
    } else {
        fprintf(stderr, "error: cannot use that search without matches\n");
        return false;
    }

    if (vars->matches->size == 1) {
        fprintf(stderr,
                "info: match identified, use \"set\" to modify value.\n");
        fprintf(stderr, "info: enter \"help\" for other commands.\n");
    }

    return true;
}

bool handler__version(globals_t * vars, char **argv, unsigned argc)
{
    USEPARAMS();

    printversion(stdout);
    return true;
}

bool handler__default(globals_t * vars, char **argv, unsigned argc)
{
    char *end = NULL;
    value_t val;

    USEPARAMS();

    /* attempt to parse command as a number */
    strtoval(argv[0], &end, 0x00, &val);

    /* check if that worked */
    if (*end != '\0') {
        fprintf(stderr,
                "error: unable to parse command `%s`, gave up at `%s`\n",
                argv[0], end);
        return false;
    }

    /* need a pid for the rest of this to work */
    if (vars->target == 0) {
        return false;
    }

    /* user has specified an exact value of the variable to find */
    if (vars->matches->size) {
        /* already know some matches */
        if (checkmatches(vars->matches, vars->target, val, MATCHEXACT) != true) {
            fprintf(stderr, "error: failed to search target address space.\n");
            return false;
        }
    } else {
        /* initial search */
        if (candidates(vars->matches, vars->regions, vars->target, val) != true) {
            fprintf(stderr, "error: failed to search target address space.\n");
            return false;
        }
    }

    /* check if we now know the only possible candidate */
    if (vars->matches->size == 1) {
        fprintf(stderr,
                "info: match identified, use \"set\" to modify value.\n");
        fprintf(stderr, "info: enter \"help\" for other commands.\n");
    }

    return true;
}

bool handler__exit(globals_t * vars, char **argv, unsigned argc)
{
    USEPARAMS();

    vars->exit = 1;
    return true;
}

#define DOC_COLUMN 11           /* which column descriptions start on with help command */

bool handler__help(globals_t * vars, char **argv, unsigned argc)
{
    list_t *commands = vars->commands;
    element_t *np = NULL;
    command_t *def = NULL;
    assert(commands != NULL);
    assert(argc >= 1);

    np = commands->head;

    /* print version information for generic help */
    if (argv[1] == NULL)
        printversion(stdout);

    /* traverse the commands list, printing out the relevant documentation */
    while (np) {
        command_t *command = np->data;

        /* remember the default command */
        if (command->command == NULL)
            def = command;

        /* just `help` with no argument */
        if (argv[1] == NULL) {
            int width;

            /* NULL shortdoc means dont print in help listing */
            if (command->shortdoc == NULL) {
                np = np->next;
                continue;
            }

            /* print out command name */
            if ((width =
                 fprintf(stdout, "%s",
                         command->command ? command->command : "default")) <
                0) {
                /* hmm? */
                np = np->next;
                continue;
            }

            /* print out the shortdoc description */
            fprintf(stdout, "%*s%s\n", DOC_COLUMN - width, "",
                    command->shortdoc);

            /* detailed information requested about specific command */
        } else if (command->command
                   && strcasecmp(argv[1], command->command) == 0) {
            fprintf(stdout, "%s\n",
                    command->longdoc ? command->
                    longdoc : "missing documentation");
            return true;
        }

        np = np->next;
    }

    if (argc > 1) {
        fprintf(stderr, "error: unknown command `%s`\n", argv[1]);
        return false;
    } else if (def) {
        fprintf(stdout, "\n%s\n", def->longdoc ? def->longdoc : "");
    }

    return true;
}

bool handler__eof(globals_t * vars, char **argv, unsigned argc)
{
    fprintf(stdout, "exit\n");
    return handler__exit(vars, argv, argc);
}

/* XXX: handle !ls style escapes */
bool handler__shell(globals_t * vars, char **argv, unsigned argc)
{
    size_t len = argc;
    unsigned i;
    char *command;

    USEPARAMS();

    if (argc < 2) {
        fprintf(stderr,
                "error: shell command requires an argument, see `help shell`.\n");
        return false;
    }

    /* convert arg vector into single string, first calculate length */
    for (i = 1; i < argc; i++)
        len += strlen(argv[i]);

    /* allocate space */
    command = calloca(len, 1);

    /* concatenate strings */
    for (i = 1; i < argc; i++) {
        strcat(command, argv[i]);
        strcat(command, " ");
    }

    /* finally execute command */
    if (system(command) == -1) {
        fprintf(stderr, "error: system() failed, command was not executed.\n");
        return false;
    }

    return true;
}

bool handler__watch(globals_t * vars, char **argv, unsigned argc)
{
    value_t o, n;
    unsigned i, id;
    element_t *np = vars->matches->head;
    match_t *match;
    char *end = NULL, buf[64], timestamp[64];
    time_t t;

    if (argc != 2) {
        fprintf(stderr,
                "error: was expecting one argument, see `help watch`.\n");
        return false;
    }

    /* parse argument */
    id = strtoul(argv[1], &end, 0x00);

    /* check that strtoul() worked */
    if (argv[1][0] == '\0' || *end != '\0') {
        fprintf(stderr, "error: sorry, couldnt parse `%s`, try `help watch`\n",
                argv[1]);
        return false;
    }

    /* check this is a valid match-id */
    if (id >= vars->matches->size) {
        fprintf(stderr, "error: you specified a non-existant match `%u`.\n",
                id);
        fprintf(stderr,
                "info: use \"list\" to list matches, or \"help\" for other commands.\n");
        return false;
    }

    /*lint -e722 skip to the correct node, semi-colon intended */
    for (i = 0; np && i < id; i++, np = np->next);

    if (np == NULL) {
        fprintf(stderr, "error: couldnt locate match `%u`.\n", id);
        return false;
    }

    match = np->data;

    valcpy(&o, &match->lvalue);
    valcpy(&n, &o);

    if (valtostr(&o, buf, sizeof(buf)) == false) {
        strncpy(buf, "unknown", sizeof(buf));
    }

    if (INTERRUPTABLE()) {
        (void) detach(vars->target);
        ENDINTERRUPTABLE();
        return true;
    }

    /* every entry is timestamped */
    t = time(NULL);
    strftime(timestamp, sizeof(timestamp), "[%T]", localtime(&t));

    fprintf(stdout,
            "info: %s monitoring %10p for changes until interrupted...\n",
            timestamp, match->address);

    while (true) {

        if (attach(vars->target) == false)
            return false;

        if (peekdata(vars->target, match->address, &n) == false)
            return false;

        detach(vars->target);

        truncval(&n, &match->lvalue);

        /* check if the new value is different */
        if (valuecmp(&o, MATCHNOTEQUAL, &n, NULL)) {

            valcpy(&o, &n);
            truncval(&o, &match->lvalue);

            if (valtostr(&o, buf, sizeof(buf)) == false) {
                strncpy(buf, "unknown", sizeof(buf));
            }

            /* fetch new timestamp */
            t = time(NULL);
            strftime(timestamp, sizeof(timestamp), "[%T]", localtime(&t));

            fprintf(stdout, "info: %s %10p -> %s\n", timestamp, match->address,
                    buf);
        }

        (void) sleep(1);
    }
}
