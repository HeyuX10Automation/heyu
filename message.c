/*
 * Copyright 1986 by Larry Campbell, 73 Concord Street, Maynard MA 01754 USA
 * (maynard!campbell).
 *
 * John Chmielewski (tesla!jlc until 9/1/86, then rogue!jlc) assisted
 * by doing the System V port and adding some nice features.  Thanks!
 */
/*
 * updated by Daniel Suthers, dbs@tanj.com 1996 -- 2000 
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

#include <stdio.h>
#include "process.h"
#include "x10.h"

char *E_2MANY = EM_2MANY;
char *E_INVCN = EM_INVCN;
char *E_WNA = EM_WNA;
char *E_NMA = EM_NMA;
char *E_NOCMD = EM_NOCMD;

void exit();

extern CONFIG *configp;

int usage(s)
char *s;
{

    char RCSID[]= "@(#) $Id: message.c,v 1.12 1999/12/26 20:37:17 dbs Exp dbs $\n";

/*
 * Don't combine the two calls to fputs or my compiler will
 * gag with "token too long"
 */

    display(RCSID);

    if( s != NULL )
    (void) fprintf(stderr, "Command error: %s\n", s);
    (void) fputs("Usage:\n\
 heyu help                   Display new commands in heyu ALPHA\n\
 heyu date                    Return date in date(1) input format\n", stderr);
    
    (void) fputs("\
 heyu erase                   Zero CM11a EEPROM, erasing all events and macros\n\
 heyu help                    Print this message and exit\n\
 heyu info                    Display current CM11a registers and clock\n\
 heyu monitor                 Monitor X10 activity (end with <BREAK>)\n", stderr);
    
    (void) fputs("\
 heyu preset ann vv           Set unit to level vv for next dim/bright\n\
 heyu reset  [housecode]      Reset interface to 'A' or specified housecode\n\
 heyu setclock                Set CM11a clock to system clock (per schedule)\n\
 heyu status ann[,nn...]      Return status of smart modules (rr501...)\n\
 heyu stop                    Stop the current relay daemon\n", stderr);
    
    (void) fputs("\
 heyu turn ann[,nn...] on|off|dim|bright [vv]\n\
                              Change state of housecode a, unit nn by vv\n\
 heyu upload [check|croncheck|status|cronstatus]\n\
                              Upload schedule to CM11a or check schedule file\n\
                                or display status of uploaded schedule\n\
 heyu utility <option>        (Enter 'heyu utility' to see options)\n\
 heyu version                 Display the Heyu version and exit\n\
", stderr);

    (void) fputs("Verbose mode is enabled if the first parameter is -v\n",
            stderr);
    exit(1);
}

void error(s)
char *s;
{
    extern void quit();
    extern int munlock();
    extern int port_locked;

    (void) fprintf(stderr, "HEYU: %s\n", s);
    if( port_locked == 1 ) {
        char writefilename[PATH_LEN + 1];

        sprintf(writefilename, "%s%s", WRITEFILE, configp->suffix);
        munlock(writefilename);
    }

    quit();
}
