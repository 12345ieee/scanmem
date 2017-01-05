/*
*
* $Author: taviso $
* $Revision: 1.5 $
*/

#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include <readline/readline.h>
#include <readline/history.h>

#include "list.h"
#include "scanmem.h"

selection_t getcommand(unsigned info, unsigned *operand)
{
    static selection_t last = COMMAND_ERROR;
    char *command = NULL, *end = NULL;

    while (true) {
        unsigned i;
        size_t len;
        char prompt[64];
        
        snprintf(prompt, sizeof(prompt), "%u> ", info);

        if ((command = readline(prompt)) == NULL) {
            /* EOF */
            free(command);
            fprintf(stderr, "exit\n");
            return last = COMMAND_EXIT;
        }
        
        len = strlen(command);
        
        /* repeat last command */
        if (*command == '\n' || *command == '\0') {
            free(command);
            return last;
        }
        
        /* record this line to readline history */
        add_history(command);
        
        /* attempt to parse command as number */
        i = strtoul(command, &end, 0);
        
        /* the whole command is a valid number */
        if (*end == '\n' || *end == '\0') {
            *operand = i;
            last = COMMAND_EXACT;
        } else if (strncmp(command, ">", 1) == 0) {
            last = COMMAND_INCREMENT;
        } else if (strncmp(command, "<", 1) == 0) {
            last = COMMAND_DECREMENT;
        } else if (strncmp(command, "cont", 4) == 0) {
            /* check for seconds argument */
            if (len > 5) { 
                *operand = strtoul(command + 5, NULL, 0);
            } else {
                *operand = 0;
            }
            last = COMMAND_CONTINUOUS;
        } else if (strncmp(command, "set", 3) == 0) {
            if (len > 4) {
                *operand = strtoul(command + 4, NULL, 0);
            } else {
                *operand = ~0;
            }
            last = COMMAND_SET;
        } else if (strncmp(command, "exit", 4) == 0) {
            last = COMMAND_EXIT;
        } else if (strncmp(command, "version", 7) == 0) {
            last = COMMAND_VERSION;
        } else if (strncmp(command, "help", 4) == 0) {
            last = COMMAND_HELP;
        } else if (strncmp(command, "list", 4) == 0) {
            last = COMMAND_LIST;
        } else if (strncmp(command, "lregions", 8) == 0) {
            last = COMMAND_LISTREGIONS;
        } else if (strncmp(command, "dregion", 7) == 0) {
            if (len > 8) {
                *operand = strtoul(command + 7, NULL, 0);
            } else {
                *operand = ~0;
            }
            last = COMMAND_DELREGIONS;
        } else if (strncmp(command, "delete", 6) == 0) {
            if (len > 8) {
                
                *operand = strtoul(command + 7, NULL, 0);
                last = COMMAND_DELETE;
            } else {
                fprintf(stderr, "warn: operand required for delete command, use \"list\".\n");
                last = COMMAND_ERROR;
            }
        } else if (strncmp(command, "pid", 3) == 0) {
            if (len > 4) {
                *operand = strtoul(command + 4, NULL, 0);
            } else {
                *operand = 0;
            }
            last = COMMAND_PID;
        } else if (strncmp(command, "reset", 5) == 0) {
            last = COMMAND_RESET;
        } else if (strncmp(command, "width", 5) == 0) {
            if (len > 6) {
                *operand = strtoul(command + 6, NULL, 0);
            } else {
                *operand = 0;
            }
            last = COMMAND_WIDTH;
        } else {
            fprintf(stderr, "warn: unable to parse command `%s`.\n", command);
            continue;
        }
        
        free(command);
        return last;
    }
}

void printinthelp(void)
{
    printversion();
    
    fprintf(stderr,
        "\n"
        "n\t- (where n is any number), scan for variables with this value.\n"
        ">\t- match all variables that have increased since last scan.\n"
        "<\t- match all variables that have decreased since last scan.\n"
        "cont n\t- inject value continuously every n seconds, use 0 to disable.\n"
        "set n\t- set all known matches to n (default 0).\n"
        "list\t- list all known matches.\n"
        "delete n- delete nth known match (as printed by \"list\").\n"
        "pid\t- print the pid of the target.\n"
        "width n\t- change the width of the target variable.\n"
        "lregions- list all the regions known about.\n"
        "dregion\t- delete the nth region.\n"
        "reset\t- forget all matches.\n"
        "version\t- print the current version.\n"
        "help\t- print this screen.\n"
        "exit\t- exit scanmem.\n"
     );
     return;
}
