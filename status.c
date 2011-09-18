/*
 * Copyright 1996, 1997, 1998, 1999 by Daniel B. Suthers,
 * Pleasanton Ca. 94588 USA
 * E-MAIL dbs@tanj.com
 *
 * You may freely copy, use, and distribute this software,
 * in whole or in part, subject to the following restrictions:
 *
 *  1)  You may not charge money for it.
 *  2)  You may not remove or alter this copyright notice.
 *  3)  You may not claim you wrote it.
 *  4)  If you make improvements (or other changes), you are requested
 *      to send them to me, so there's a focal point for distributing
 *      improved versions.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <ctype.h>
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "x10.h"

extern int tty;
extern int sptty;
extern int i_am_relay, i_am_state, verbose;
extern unsigned char alert_ack[3];


int Irev, Idays, Ijday, Ihours, Iminutes, Iseconds;
int Istatdim, Istatmon, Iaddmon;
unsigned char Ihcode;
unsigned int Ibattery; /* Raw battery status */


/* buf should be a 16 byte character array. It will be read into. */
int get_status()
{
    register int n;
    unsigned int batlife;
    extern int timeout;
    static int tries = 0;
    int cnt;
    unsigned char buf[20];
    unsigned char powerfail[16];
    char *bits2day();

    extern int xwrite(), exread(), check4poll(), xread(), ri_control();

    /* check the status of the interface.  See section 9 of the
       protocol doc.
     */
    timeout = TIMEOUT;
    for ( n = 0; n < 15; n++ )
        powerfail[n] = 0xa5;
    powerfail[n] = '\0';

    buf[0] = 0x8b;	/* get status command */

    (void) xwrite(tty, buf, 1);  /* STATUS REQUEST command */

    n = exread(sptty, buf, 14, 3);   

    if ( n != 14 ) {
	if ( buf[0] == 0x5a || n == 0 || (strncmp((char *)buf, (char *)powerfail,n) == 0 )) {
	    if ( n > 0 && (strncmp((char *)buf, (char *)powerfail,n) == 0) ) {
	        /* power failed, wait */
		if( verbose )
		    fprintf( stderr, "Power fail detected, wait 4 seconds.\n");
	        sleep(4);
	    }
	    if ( tries++ < 2) {
		if ( verbose )
		    fprintf( stderr, "re checking for poll \n");
		check4poll(0,2);
		if ( verbose )
		    fprintf( stderr, "re entering get status \n");
		return(get_status());
	    }
	}
	else {
	    if ( n == 13 ) {
		return(get_status());
            }
	    else {
	        if ( tries++ < 4 )
		    return(get_status());
            }
	}

        fprintf(stderr,"Invalid status response (was %d bytes instead of 14)\n", n);
	for ( cnt = 0; cnt < n; cnt++)
	    fprintf(stderr," byte %d was 0x%02x\n", cnt, buf[cnt]);
        return(-1);
    }

    /* No check sum after status command */

    /* Send a RI control command.  In the event that the last three bytes */
    /* of the status string happen to match our checksum 0x5A relay alert */
    /* sequence, this will have the effect of cancelling the alert.       */

    /* if ( memcmp(buf + 11, alert_ack, 3) == 0 ) */
    ri_control();

    /* After a cold restart, i.e., if the CM11A has been unplugged
     * for some period of time with no battery, the returned value
     * of the battery timer will be 0xffff.  After resetting, the timer
     * will increment by 1 each time the clock rolls over to the
     * next whole minute of operation disconnected from the AC line.
     */
    
    batlife = ( (buf[1] << 8) | (unsigned int) buf[0] );
    Ibattery = batlife;

    if ( batlife > 2400 && batlife != 0xffff )  {	    
        fprintf( stderr,
        "Battery has been used  %d minutes.  It should be replaced soon.\n", batlife);
    }
    if ( verbose ) {
        if ( batlife == 0xffff ) 
            fprintf(stderr, 
	        "The battery usage is unknown.\n");
	else if ( batlife > 59 ) 
	    fprintf(stderr,
	        "The battery has been used for %d hour%sand %d minute%s\n",
		batlife / 60, (batlife/60) == 1 ? " " : "s ",
		batlife % 60, (batlife % 60)  == 1 ? "." : "s.");
	else
        fprintf(stderr, "The battery has been used for %d minute%s\n",
	    batlife, batlife == 1 ? "." : "s.");
    }

    Irev = (buf[7] & 0x0Fu);
    Iseconds = (buf[2] & 0xffu);
    Iminutes = (buf[3] & 0xffu) % 60;
    Ihours =  ((buf[4] & 0xffu) * 2) + ((buf[3] & 0xffu) / 60);
    Ijday = buf[5] + (((buf[6] & 0x080u) >> 7) * 256) ;
    Idays = buf[6] & 0x07fu;
    Ihcode = ((buf[7] & 0xF0u ) >> 4);


    /* check the clock against current time. */
    /* Btye 4 has the hours divided in half, 3 has 2 hours worth of minutes, */
    /* 2 has seconds */
    if ( verbose ) {
       fprintf (stderr, "%02d:%02d:%02d is the interface time\n",
            Ihours, Iminutes, Iseconds);
       fprintf (stderr, "%03d is the julian date\n",
	    (int)buf[5] + ((((int)buf[6] & 0x080) >> 7) * 256) );
       fprintf (stderr, "%s (%2x) is the interface day of the week\n",
	    bits2day(buf[6] & 0x07f), buf[6] & 0x07f );
    }

    Iaddmon = buf[8] + (buf[9] << 8);
    Istatmon = buf[10] + (buf[11] << 8);
    Istatdim = buf[12] + (buf[13] << 8);

    return(1);
}
