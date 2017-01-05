/*
*
* $Id: commands.h,v 1.4 2007-04-08 23:09:17+01 taviso Exp $
*
*/

#ifndef _COMMANDS_INC
#define _COMMANDS_INC           /* include guard */

/*lint -esym(534,registercommand) okay to ignore return value */

typedef struct {
    bool(*handler) (globals_t * vars, char **argv, unsigned argc);
    char *command;
    char *shortdoc;
    char *longdoc;
} command_t;


bool registercommand(const char *command, void *handler, list_t * commands,
                     char *shortdoc, char *longdoc);
bool execcommand(globals_t * vars, const char *commandline);

#endif
