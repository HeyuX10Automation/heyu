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

#include <ctype.h>
#include <stdio.h>
#include <time.h>
#include "x10.h"
#include "process.h"

extern unsigned int x2unitmap();

extern int Irev, Idays, Ihours, Iminutes, Ijday, Iseconds;
extern int Istatdim, Istatmon, Iaddmon;
extern unsigned int Ibattery;
extern unsigned char Ihcode;

extern void error();
extern char *heyu_tzname[];
extern struct tm *cm11a_to_legal();
extern CONFIG config;
extern CONFIG *configp;

extern char *wday_name[7];
extern char *month_name[12];
extern char heyu_config[PATH_LEN + 1];

/* ARGSUSED */

int c_info( int argc, char *argv[] )
{
    extern int usage(), get_status();

    struct tm *tms;
    int expire;
    char error_msg[127];


    if (argc != 2)
	usage(E_2MANY);

    if ( configp->device_type & DEV_DUMMY ) {
       printf("Heyu version %s\n", VERSION);
       printf("Configuration at %s\n", pathspec(heyu_config));
       printf("Dummy powerline interface (TTY %s)\n", configp->tty);
       if ( configp->ttyaux[0] )
          printf("Auxiliary RF interface on %s\n", configp->ttyaux);
       if ( configp->ttyrfxmit[0] ) {
          printf("%sMHz RFXmitter on %s\n",
            ((configp->rfxmit_freq == MHZ310) ? "310" : "433"), configp->ttyrfxmit);
       }
       return 0;
    }
    
    if ( configp->device_type & DEV_CM10A ) {
       printf("Heyu version %s\n", VERSION);
       printf("Configuration at %s\n", pathspec(heyu_config));
       printf("Powerline interface CM10A on %s\n", configp->tty);
       if ( configp->ttyaux[0] )
          printf("Auxiliary RF interface on %s\n", configp->ttyaux);
       if ( configp->ttyrfxmit[0] ) {
          printf("%sMHz RFXmitter on %s\n",
            ((configp->rfxmit_freq == MHZ310) ? "310" : "433"), configp->ttyrfxmit);
       }
       return 0;
    }
    
    if ( get_status() < 0 ) {
	 sprintf(error_msg, 
	    "No response from the CM11A on %s\nProgram exiting.",
	        configp->tty);
         error(error_msg);
    }

    printf("Heyu version %s\n", VERSION);
    printf("Configuration at %s\n", pathspec(heyu_config));
    printf("Powerline interface on %s\n", configp->tty);
    if ( configp->ttyaux[0] )
       printf("Auxiliary RF interface on %s\n", configp->ttyaux);
    if ( configp->ttyrfxmit[0] ) {
       printf("%sMHz RFXmitter on %s\n",
         ((configp->rfxmit_freq == MHZ310) ? "310" : "433"), configp->ttyrfxmit);
    }
    printf("Firmware revision Level = %d\n", Irev );

    /* Display CM11a battery timer */
    if ( Ibattery == 0xFFFF ) 
       (void) printf("Interface battery usage = Unknown\n");
    else
       (void) printf("Interface battery usage = %d:%02d  (hh:mm)\n", 
                                          Ibattery / 60, Ibattery % 60);

    if ((Ihours + Iminutes + Idays + Ijday) != 0 )  {
	(void) printf("Raw interface clock: %s, Day %03d, %02d:%02d:%02d\n",
		      bmap2ascdow(Idays), Ijday, Ihours, Iminutes, Iseconds);

        tms = cm11a_to_legal(&Idays, &Ijday, &Ihours, &Iminutes, &Iseconds, &expire);

        if ( expire != BAD_RECORD_FILE ) {
           (void) printf("(--> Civil Time: %s %02d %s %d   %02d:%02d:%02d %s)\n",
               bmap2ascdow(Idays), tms->tm_mday, month_name[tms->tm_mon], tms->tm_year + 1900,
               tms->tm_hour, tms->tm_min, tms->tm_sec, heyu_tzname[tms->tm_isdst]);
        }
        
        display_status_message( expire );
    }
    else
	(void) printf("Interface clock not yet set\n");

    (void) printf("Housecode = %c\n", code2hc(Ihcode));
    
    (void) printf("0 = off, 1 = on,               unit  16.......8...4..1\n");

    (void) printf("Last addressed device =       0x%04x (%s)\n", Iaddmon,
            bmap2rasc(Iaddmon, "01") );
    (void) printf("Status of monitored devices = 0x%04x (%s)\n", Istatmon,
            bmap2rasc(Istatmon, "01") );
    (void) printf("Status of dimmed devices =    0x%04x (%s)\n", Istatdim,
            bmap2rasc(Istatdim, "01") );
    return(0);
}

void wait_next_tick (void )
{
   time_t first, now;
   int j;

   time(&first);
   
   for ( j = 0; j < 25; j++ ) {
      time(&now);
      if ( now > first )
         break;
      millisleep(50);
   }
   return;
}   
   
      
	
int c_readclock( int argc, char *argv[] )
{
    extern int usage(), get_status();
    extern char *bits2day(), *b2s();

    time_t    now;			      
    struct tm *tms;
    int expire;
    char error_msg[127];


    if (argc != 2)
	usage(E_2MANY);

    if ( configp->device_type & DEV_CM10A ) {
       printf("CM10A interface has no clock.\n");
       return 1;
    }

    wait_next_tick();

    if ( get_status() < 0 ) {
	 sprintf(error_msg, 
	    "No response from the CM11A on %s\nProgram exiting.",
	        configp->tty);
         error(error_msg);
    }

    if ((Ihours + Iminutes + Idays + Ijday) != 0 )  {

        tms = cm11a_to_legal(&Idays, &Ijday, &Ihours, &Iminutes, &Iseconds, &expire);

        if ( expire != BAD_RECORD_FILE ) {
           (void) printf("CM11A Time : %s %02d %s %d %02d:%02d:%02d %s\n",
               bmap2ascdow(Idays), tms->tm_mday, month_name[tms->tm_mon], tms->tm_year + 1900, 
               tms->tm_hour, tms->tm_min, tms->tm_sec, heyu_tzname[tms->tm_isdst]);
	   time(&now);
	   tms = localtime(&now);
	   (void) printf("System Time: %s %02d %s %d %02d:%02d:%02d %s\n",
	       bmap2ascdow(1 << tms->tm_wday), tms->tm_mday, month_name[tms->tm_mon], tms->tm_year + 1900, 
               tms->tm_hour, tms->tm_min, tms->tm_sec, heyu_tzname[tms->tm_isdst]);
        }
	else {
	   (void) fprintf(stderr, "File x10record has been corrupted.\n");
	   return 1;
	}
    }
    else
	(void) printf("Interface clock not yet set\n");

    return 0;
}
