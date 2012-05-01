/*
 * Copyright 1986 by Larry Campbell, 73 Concord Street, Maynard MA 01754 USA
 * (maynard!campbell).
 *
 * John Chmielewski (tesla!jlc until 9/1/86, then rogue!jlc) assisted
 * by doing the System V port and adding some nice features.  Thanks!
 *
 * Charles Sullivan (cwsulliv01@heyu.org) added the function
 * c_set_status_bits().
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
#include <stdio.h>
#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#endif
#include "x10.h"
#include "process.h"

extern int tty, sptty;
extern int verbose, i_am_relay;
extern int invalidate_for_cm10a();

/* ARGSUSED */

void c_reset( int argc, char *argv[] )
{
    int hletter;
    int oldcode;
    extern int x10_housecode;
    extern int c_setclock(), usage();

    if ( invalidate_for_cm10a() != 0 )
       return;

    if (argc > 3)
	usage(E_WNA);

    oldcode = x10_housecode;	/* this is the default housecode */

    if (argc == 3) {
	hletter = argv[2][0];
	x10_housecode = hc2code(hletter);
    }
    c_setclock(2, argv);
    x10_housecode = oldcode;
}

/*---------------------------------------------------------+
 | Set Status Bits - Send the CM11A a status update        |
 | in the lowest four bits of the status update block,     |
 | which are the following (defined in x10.h):             |
 |   MONITORED_STATUS_CLEAR                                |
 |   RESET_BATTERY_TIMER                                   |
 |   PURGE_DELAYED_MACROS                                  |
 |   RESERVED_STATUS_BIT                                   |
 |                                                         |
 | The status update block which includes these bits also  |
 | includes the CM11A clock settings.  To avoid changing   |
 | the clock settings, we first request the status from    |
 | the CM11A, and then immediately write back the same     |
 | clock information along with one (or more) of the above |
 | bits set. (Added by CWS)                                |
 +---------------------------------------------------------*/

int c_set_status_bits ( unsigned char bitmap )
{

    extern int Idays, Ijday, Ihours, Iminutes, Iseconds;
    extern unsigned char Ihcode;

    unsigned char data[9];
    unsigned char buf[3];
    char msgbuf[1000];
    unsigned int n;
    extern int xwrite(), exread(), check4poll(), get_status();
    
    if ( invalidate_for_cm10a() != 0 )
       return 1;

    if ( get_status() < 0 ) {
       fprintf(stderr, "Unable to read current CM11A status.\n");
       return -1;
    }

    data[0] = 0x9b;		/* CM11A timer download code */
    data[1] = Iseconds ;
    data[2] = Iminutes + ((Ihours %2) * 60 ) ;    /* minutes 0 - 119 */
    data[3] = Ihours / 2 ;                       /* hour / 2         0 - 11 */
    data[4] = Ijday  % 256 ;                     /* mantisa of julian date */
    data[5] = ((Ijday / 256 ) << 7);             /* radix of julian date */
    data[5] |= (Idays & 0x7F);           /* bits 0-6 =  single bit mask day */
    			                         /* of week ( smtwtfs ) */
    data[6] = ( Ihcode << 4);
    data[6] |= (bitmap & 0x0F); 
    
    if ( verbose ) {
        sprintf(msgbuf, "would send %0x %0x %0x %0x %0x %0x %0x\n",
	        data[0], data[1], data[2], data[3], data[4], data[5],
		data[6]);
	if ( i_am_relay == 1 )
	    syslog(LOG_ERR, "%s", msgbuf);
	else
	    fprintf(stderr, "%s", msgbuf);
    }
    (void) xwrite(tty, (char *) data, 7);
    n = exread(sptty, buf, 1, 1);
    
    check4poll(0,1);		/* zero means to discard data */
    get_status();

    millisleep(SETCLOCK_DELAY);
	
    return 0;
}

