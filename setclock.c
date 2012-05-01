/*
 * Copyright 1996, 1997, 1998, 1999, 2002 by Daniel B. Suthers,
 * Pleasanton Ca. 94588 USA
 * E-MAIL dbs@tanj.com
 *
 */

/*
 * Original code written for the cp290 series by Larry Campbell,
 * 73 Concord Street, Maynard MA 01754 USA. (maynard!campbell)
 */

/* 
 * Code modified for Heyu ver 2 by Charles Sullivan 
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
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#include <time.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include "x10.h"
#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#include <ctype.h>
#include "process.h"

extern int tty;
extern int sptty;

extern char *wday_name[7];
extern char *month_name[12];

extern int i_am_relay;

extern int x10_housecode;
extern int verbose;
extern unsigned int jul();
extern CONFIG config;
extern CONFIG *configp;

/* ARGSUSED */

int c_setclock ( int argc, char *argv[] )
{
    unsigned char data[9];
    unsigned char buf[3];
    char msgbuf[1000];
    unsigned int n;
    unsigned int clear;
    struct tm *tp;
    time_t dtime;

    extern int usage(), xwrite(), xread(), exread(), sxread(), check4poll(), get_status();
    extern void wait_next_tick();
    extern struct tm *legal_to_cm11a();

    int  code;
    unsigned char cksum;

    int  ignoret;

    if ( invalidate_for_cm10a() != 0 )
       return -1;
   
    /* Argc == 1 means use system time.
     * Argc == 2 means parse a date from the input or reset the clock.
     */
    if ( argc < 1 || argc > 3 ) {
	if ( argc != 7  )
           usage(E_2MANY);
    }
    clear = 0;

    if ( i_am_relay != 1 )
       wait_next_tick();  /* System clock rolls over to next second */
    
    dtime = time(NULL);
    tp = localtime(&dtime); 

    if ( argc == 2 ) {
	if ( strcmp( "reset", argv[1]) == 0 ) {
	    clear = 1;
	}
    }
    if ( argc == 3 ) {
    	/* must be date information.  Load it in the structure
    	 * and then remake the structure with the right time.
         * (In Heyu 1, entered data was taken to be Standard Time
         * but is now taken to be Legal Time.)
         */
	if ( sscanf(argv[2], "%2d%2d%2d%2d%2d", &tp->tm_mon, &tp->tm_mday,
	             &tp->tm_hour, &tp->tm_min, &tp->tm_sec) != 5 ) 	{
	    printf(" Bad argument. Expected %s %s MMDDhhmmss\n", argv[0], argv[1]);
	    return(-1);
	}
	tp->tm_mon -= 1; /* tm month is 0 relative */
        tp->tm_isdst = -1;

	dtime = mktime(tp);	/* reset day of week, julian */

	if ( dtime < 1 ) {
	    printf("An error was encountered parsing the input %s\n", argv[2]);
	    return(-1);
	}
    }

    /* 
     * Translate Legal Time into CM11a clock time per the 
     * X10 Record File, if it exists, or to Standard Time
     * if it doesn't.
     */
    tp = legal_to_cm11a( &dtime );


    data[0] = 0x9b;		/* CM11A timer download code */
    data[1] = tp->tm_sec ;
    data[2] = tp->tm_min + (((tp->tm_hour) %2) * 60 ) ;  /* minutes 0 - 119 */
    data[3] = tp->tm_hour / 2 ;			/* hour / 2         0 - 11 */
    data[4] = tp->tm_yday  % 256 ;		/* mantisa of julian date */
    data[5] = ((tp->tm_yday / 256 ) << 7);	/* radix of julian date */
    data[5] |= (0x01 << (tp->tm_wday)); /* bits 0-6 =  single bit mask day */
    					 /* of week ( smtwtfs ) */
    data[6] = (x10_housecode << 4);
    data[6] |= clear;

    if ( verbose )  {
        sprintf(msgbuf, "would send %02x %02x %02x %02x %02x %02x %02x\n",
	        data[0], data[1], data[2], data[3], data[4], data[5],
		data[6]);
	if ( i_am_relay == 1 )
	    syslog(LOG_ERR, "%s", msgbuf);
	else
	    fprintf(stderr, "%s", msgbuf);
    }

    /* Reset the timed macro execution timestamp */
    set_macro_timestamp(0);

    cksum = checksum(data + 1, 6);
    code = 0;

    if ( i_am_relay ) {
       ignoret = write(tty, (char *) data, 7);
       n = sxread(tty, buf, 1, 1);
    }
    else {
       xwrite(tty, (char *) data, 7);
       n = exread(sptty, buf, 1, 1);
    }

    if ( n != 1 )
       code |= 0x10;
    else if ( buf[0] != cksum )
       code |= 0x01;
    
    if ( i_am_relay ) {
       ignoret = write(tty, "\0", 1);   /* WRMI */
       n = sxread(tty, buf, 1, 1);
    }
    else {
       xwrite(tty, "\0", 1);   /* WRMI */
       n = exread(sptty, buf, 1, 1);
    }

    if ( n != 1 )
       code |= 0x20;
    else if ( buf[0] != 0x55 )
       code |= 0x02;


    if ( argc == 2 || argc == 3 )  {
	check4poll(0,1);		/* zero means to discard data */

	get_status();

	if ( clear == 0) {
	    if ( i_am_relay != 1 )
	       fprintf(stderr, "CM11A clock set to %s, Day %03d, %02d:%02d:%02d (Standard Time)\n",
		      wday_name[tp->tm_wday], tp->tm_yday, tp->tm_hour, tp->tm_min, tp->tm_sec);
	}
	else {
	    if ( i_am_relay != 1 )
	       printf("CM11A house code set to %c\n", code2hc( x10_housecode));
	}
    }

    return(0);
}

/* Set clock without regard to the x10record file */
int c_setclockraw ( int argc, char *argv[] )
{
    unsigned char data[9];
    unsigned char buf[3];
    char msgbuf[1000];
    unsigned int n;
    unsigned int clear;
    struct tm *tp;
    time_t dtime;
    int was_tm_isdst;
    extern int usage(), xwrite(), xread(), exread(), sxread(), check4poll(), get_status();
    extern void wait_next_tick();

    int  code, nwrite;
    unsigned char cksum;

   if ( invalidate_for_cm10a() != 0 )
      return 1;
   
    /* Argc == 1 means use system time.
     * Argc == 2 means parse a date from the input or reset the clock.
     */
    if ( argc < 1 || argc > 3 ) {
	if ( argc != 7  )
	   usage(E_2MANY);
    }
    clear = 0;
    
    if ( i_am_relay != 1 )
       wait_next_tick();  /* System clock rolls over to next second */

    dtime = time(NULL);
    tp = localtime(&dtime); 

    if ( argc == 2 )  {
	if( strcmp( "reset", argv[1]) == 0 ) {
	    clear = 1;
	}
    }
    if ( argc == 3) {
    	/* must be date information.  Load it in the structure
    	 * and then remake the structure with the right time.
    	 */
	if ( sscanf(argv[2], "%2d%2d%2d%2d%2d", &tp->tm_mon, &tp->tm_mday,
	             &tp->tm_hour, &tp->tm_min, &tp->tm_sec) != 5)  {
	    printf(" Bad argument. Expected %s %s MMDDhhmmss\n", argv[0], argv[1]);
	    return(-1);
	}
	tp->tm_mon -= 1; /* tm month is 0 relative */
	was_tm_isdst = tp->tm_isdst;		/* save the current DST flag */
	dtime = mktime(tp);	/* reset day of week, julian */
	if ( dtime < 1 ) {
	    printf("An error was encountered parsing the input %s\n", argv[2]);
	    return(-1);
	}

	/* we want absolute time, but mktime will reset the hour as necessary,
	 * to match daylight savings.
	 * we undo that here.
	 */
	if ( tp->tm_isdst != was_tm_isdst ) {
	    if ( was_tm_isdst == 0 ) {
	        dtime -= 3600;	/* subtract an hour*/
	    }
	    else {
	        dtime += 3600;	/* add an hour*/
	    }
	    tp = localtime(&dtime);
	}


    }

    data[0] = 0x9b;		/* CM11A timer download code */
    data[1] = tp->tm_sec ;
    data[2] = tp->tm_min + (((tp->tm_hour) %2) * 60 ) ;  /* minutes 0 - 119 */
    data[3] = tp->tm_hour / 2 ;			/* hour / 2         0 - 11 */
    data[4] = tp->tm_yday  % 256 ;		/* mantisa of julian date */
    data[5] = ((tp->tm_yday / 256 ) << 7);	/* radix of julian date */
    data[5] |= (0x01 << (tp->tm_wday)); /* bits 0-6 =  single bit mask day */
    					 /* of week ( smtwtfs ) */
    data[6] = (x10_housecode << 4);
    data[6] |= clear;

    if ( verbose )  {
        sprintf(msgbuf, "would send %02x %02x %02x %02x %02x %02x %02x\n",
	        data[0], data[1], data[2], data[3], data[4], data[5],
		data[6]);
	if ( i_am_relay == 1 )
	    syslog(LOG_ERR, "%s", msgbuf);
	else
	    fprintf(stderr, "%s", msgbuf);
    }

    cksum = checksum(data + 1, 6);
    code = 0;

    nwrite = xwrite(tty, (char *) data, 7);
    if ( verbose & !i_am_relay )
       fprintf(stderr, "%d bytes written.\n", nwrite);
 
    if ( i_am_relay )
       n = sxread(tty, buf, 1, 1);
    else
       n = exread(sptty, buf, 1, 1);

    if ( n != 1 )
       code |= 0x10;
    else if ( buf[0] != cksum )
       code |= 0x01;
    
    xwrite(tty, "\0", 1);   /* WRMI */
    if ( i_am_relay ) 
       n = sxread(tty, buf, 1, 1);
    else
       n = exread(sptty, buf, 1, 1);

    if ( n != 1 )
       code |= 0x20;
    else if ( buf[0] != 0x55 )
       code |= 0x02;

    if ( argc == 2 || argc == 3 ) {
	check4poll(0,1);		/* zero means to discard data */
	get_status();
	if( clear == 0)  {
	    if ( i_am_relay != 1 )
               fprintf(stderr, "CM11A clock set to %s, %d:%02d\n",
		      wday_name[tp->tm_wday], tp->tm_hour, tp->tm_min);
	}
	else {
	    if ( i_am_relay != 1 )
	       (void) printf("CM11A house code set to %c\n", code2hc( x10_housecode));
	}
    }

    return(0);
}


/* Stop power-fail update polling without        */
/* resetting clock. (For clock testing purposes) */

int c_stop_cm11a_poll ( int argc, char *argv[] )
{	
   unsigned char data[3];
   extern int    xwrite(), check4poll();

   int ignoret;
   
   data[0] = 0x9b;

   if ( i_am_relay ) {
      ignoret = write(tty, (char *)data, 1);
   }
   else {
      xwrite(tty, (char *)data, 1);
   }

   if ( !i_am_relay )
      check4poll(0,1);   /* zero means to discard data */

   /* Allow CM11A to recover */
   millisleep(SETCLOCK_DELAY);

   return 0;
}
