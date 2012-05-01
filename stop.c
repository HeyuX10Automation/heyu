/*
 * Copyright 1996, 1997, 1998, 1999 by Daniel B. Suthers,
 * Pleasanton Ca. 94588 USA
 * E-MAIL dbs@tanj.com
 *
 */

/*
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <ctype.h>
#if 0
#include "x10.h"
#include "process.h"
#endif
#include <signal.h>
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "x10.h"
#include "process.h"

#ifdef pid_t
#define PID_T pid_t
#else
#define PID_T long
#endif

extern int verbose;
extern CONFIG config;
extern CONFIG *configp;


/* This function looks up the currently running relay and aux
 * processes and kills them.
 */
int c_stop(argc, argv)
int argc;
char *argv[];
{
    PID_T pid;
    extern PID_T lockpid( char * );
    extern int unlock_state_engine();
    extern void quit();
#ifdef __linux__    
    FILE * pidfile;
    char buf[80];
    char procname[80];
    char *ignoretp;
#endif
    char relayfilename[PATH_LEN + 1];
    char auxfilename[PATH_LEN + 1];

    if ( configp->ttyaux[0] != '\0' ) {
       sprintf(auxfilename, "%s%s", AUXFILE, configp->suffixaux);
       pid = lockpid(auxfilename);
       if ( pid > 0 ) {
          if ( verbose ) {
             printf("killing %ld\n", (long)pid);
          }
          if ( kill(pid, SIGTERM) != 0 ) {
             fprintf(stderr, "I could not kill %s (pid = %ld)\n",
	         auxfilename, (long)pid );
          }
       }
       if ( pid == 0 ) {
          if( verbose )
	     fprintf(stderr, "stop.c: heyu_aux pid not available\n");
       }
    }
    

    sprintf(relayfilename, "%s%s", RELAYFILE, configp->suffix);
    pid = lockpid(relayfilename);
    if ( pid == 0 ) {
	if ( verbose )
	    printf("stop.c: heyu_relay pid not available\n");
	return(1);
    }
    if ( pid < 0 )
        return(-1);
    
/* Now this is really linux dependent.  It relies on having the 
 * /proc filesystem in order to verify that the process is really
 * the right one.
 */
#ifdef __linux__
    sprintf(procname, "/proc/%ld/cmdline", (long)pid);
    if( (pidfile = fopen( procname,"r")) == NULL )
    {
	if( verbose )
	    printf("proc file not openable for %ld\n", (long)pid);
        quit(0);
    }
    buf[0] = '\0';
    ignoretp = fgets(buf,79,  pidfile);
    {
	buf[79] = '\0';
        if( strstr(buf, "heyu_relay") == NULL )
	{
	    if( verbose )
		printf("proc file for %ld did not contain \"heyu_relay\"\n", (long)pid);
	    quit(0);
	}
    }
#endif

    if( verbose )
        printf("killing %ld\n", (long)pid);

    unlock_state_engine();

    if ( kill(pid, SIGTERM) != 0 ) {
        fprintf(stderr, "I could not kill %s (pid = %ld)\n",
           relayfilename, (long)pid);
        quit();
    }

#if 0    
    quit();
#endif    
    return(0);
} 
