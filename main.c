/*
*
* $Author: taviso $
* $Revision: 1.1 $
*
*/

#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif

#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stddef.h>
#include <getopt.h>
#include <string.h>

#include "scanmem.h"

/* TODO: also search registers */

int main(int argc, char **argv)
{
    pid_t target = 0;
    int optindex, continuous = 0;
    unsigned count = 0, value = 0, num = 0, to = 0;
    region_t *regions = NULL;
    intptr_t *matches = NULL;
    struct option longopts[] = {
        { "pid",         1, NULL, 'p' },  /* the target program */
        { "value",       1, NULL, 'V' },  /* value of search */
        { "to",          1, NULL, 't' },  /* what to change value to */
        { "version",     0, NULL, 'v' },  /* print version */
        { "help",        0, NULL, 'h' },  /* print help summary */
        { "width",       1, NULL, 'w' },  /* width in bytes */
        { "continuous",  1, NULL, 'c' },  /* continually inject to value every second */
        { NULL,          0, NULL,  0  },
    };
        
    /* process command line */
    while (true) {
        switch (getopt_long(argc, argv, "p:iV:t:hv", longopts, &optindex)) {
            case 'p': /* pid */
                target = (pid_t) strtoul(optarg, NULL, 0);
                break;
            case 'V': /* initial value to search for */
                value = strtoul(optarg, NULL, 0);
                break;
            case 't': /* value to set when value has been uniquely identified */
                to = strtoul(optarg, NULL, 0);
                break;
            case 'c': /* continuous, use once per second to wait */
                continuous++;
                break;
            case 'v':
                fprintf(stderr, "scanmem %s\n", VERSIONSTRING);
                return 0;
                break;
            case 'h': 
                fprintf(stderr, "scanmem %s, a ptrace() toy.\n"
                                "\tby Tavis Ormandy, taviso@sdf.lonestar.org\n\n"
                                "\t--pid n (required)\n"
                                "\t\tset the target program pid to n\n"
                                "\t--to n (default: 0)\n"
                                "\t\tset to this value when the location has been isolated.\n"
                                "\t--value n (default: 0)\n"
                                "\t\tthe value of the variable to search for\n"
                                "\t--continuous (default: off)\n"
                                "\t\tonce found, continually inject new value every second.\n"
                                "\t\tuse once per second to wait.\n"
                                "\t--width n (default: 4)\n"
                                "\t\tthe sizeof the variable to find\n", VERSIONSTRING);
                return 0;
                break;
            case -1:
                goto done;
            default:
                fprintf(stderr, "error: an error occurred while processing arguments.\n");
                break;
        }
    }
    
done:
    
    /* check if pid was specified */
    if (target == 0) {
        fprintf(stderr, "error: you must specify a pid.\n");
        return 1;
    }
    
    /* we have the information we require, now halt this process and attach */
    if (attach(target) == -1) {
        fprintf(stderr, "error: sorry, there was a problem attaching to the target.\n");
        goto error;
    }
    
    /* now process is halted, and we are in control, get a list of addresses */
    if (readmaps(target, &regions, &count) == -1) {
        fprintf(stderr, "error: sorry, there was a problem getting a list of regions to search.\n");
        goto error;
    }
    
    /* okay, now continue and let him run */
    if (detach(target) == -1) {
        fprintf(stderr, "error: failed to detach from target\n");
    }    
    
    while (true) {
        char *line = NULL;
        size_t len = 0;
            
        fprintf(stderr, "Please enter current value, or \"help\" for other commands.\n");
            
        while (true) {
            fprintf(stderr, "[%u]> ", value);
            fflush(stderr);
                
            if (getline(&line, &len, stdin) == -1) {
                fprintf(stderr, "exit\n");
                free(line);
                goto error;
             }
            
             if (strncmp(line, "help", 4) == 0) {
                 fprintf(stderr,
                     "\tlist\t- print all known matches.\n"
                     "\tset all\t- set all matches to %u.\n"
                     "\tset n\t- set match n to %u.\n"
                     "\tto n\t- change to value to n.\n"
                     "\tcont\t- toggle continuous mode.\n"
                     "\texit\t- exit scanmem.\n"
                     "\tabout\t- about scanmem.\n"
                     "\thelp\t- this screen.\n", to, to);
                 continue;
             } else if (strncmp(line, "list", 4) == 0) {
                 unsigned p;
                    
                 for (p = 0; p < num; p++) {
                     fprintf(stderr, "[%02u] %#010x\n", p, matches[p]);
                 }
                 
                 continue;
             } else if (strncmp(line, "set ", 4) == 0) {
                 if (strncmp(line + 4, "all", 3) == 0) {
                     unsigned p;
                        
                      for (p = 0; p < num; p++) {
                          fprintf(stderr, "+ %#010x -> %u\n", matches[p], to);
                          setaddr(target, matches[p], to);
                      }
                 } else {
                     unsigned p;
                     if ((p = strtoul(line + 4, NULL, 0)) <= num) {
                         fprintf(stderr, "+ %#010x -> %u\n", matches[p], to);
                         setaddr(target, matches[p], to);
                     }
                 }
                
                continue;
               } else if (strncmp(line, "to ", 3) == 0) {
                   to = strtoul(line + 3, NULL, 0);
                   continue;
               } else if (strncmp(line, "cont", 4) == 0) {
                   if (continuous) continuous = 0;
                   else continuous++;
                   continue;
               } else if (strncmp(line, "exit", 4) == 0) {
                   free(line);
                   goto error;
               } else if (strncmp(line, "about", 5) == 0) {
                   fprintf(stderr, "scanmem, a ptrace() toy by Tavis Ormandy.\n");
                   continue;
               }
                
               break;

            }
            
				/* if we dont want the same value, reset it */
            if (*line != '\n' && *line != '\0') {
                char *end;
            
                value = strtoul(line, &end, 0);
                
                /* check if there was anything valid */
                if (end == line) {
                    continue;
                }
        }   
            
        free(line);
		  /* stop the target machine to initiate search */
        if (attach(target) == -1) {
            fprintf(stderr, "error: sorry, there was a problem attaching to the target.\n");
            goto error;
        }
           
         /* search for matches */
        candidates(&matches, &num, regions, count, target, value, true);
        
        fprintf(stderr, "info: we currently have %d matches.\n", num);
        
        if (num == 1) {
            fprintf(stderr, "info: found, setting *%#010x to %u...\n", *matches, to);
            
            if (continuous) {
                fprintf(stderr, "info: setting value every %u seconds...\n", continuous);
            }
            
            while (true) {
                setaddr(target, *matches, to);
                
                if (continuous == 0) {
                    break;
                }
                
                if (detach(target) == -1) {
                    fprintf(stderr, "error: failed to detach from target\n");
                } 
                
                sleep(continuous);
                
                /* stop the target machine to initiate search */
                if (attach(target) == -1) {
                    fprintf(stderr, "error: sorry, there was a problem attaching to the target.\n");
                    goto error;
                }
                fprintf(stderr, "info: setting *%#010x to %u...\n", *matches, to);
            }
            
            if (detach(target) == -1) {
                fprintf(stderr, "error: failed to detach from target\n");
            } 
            
            break;
        }
        
        if (detach(target) == -1) {
            fprintf(stderr, "error: failed to detach from target\n");
        } 
        
        fprintf(stderr, "info: detached from target.\n");
    }  
    
    free(regions);
    free(matches);
    return 0;
    
error:
    free(regions);
    free(matches);
    (void) detach(target);
    
    return 1;
}
