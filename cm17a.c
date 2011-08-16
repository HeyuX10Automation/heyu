/*----------------------------------------------------------------------------+
 |                                                                            |
 |                 HEYU CM17A "Firecracker" Support                           |
 |             Copyright 2005, 2006 Charles W. Sullivan                       |
 |                      All Rights Reserved                                   |
 |                                                                            |
 |                                                                            |
 | This software is licensed free of charge for non-commercial distribution   |
 | and for personal and internal business use only.  Inclusion of this        |
 | software or any part thereof in a commercial product is prohibited         |
 | without the prior written permission of the author.  You may copy, use,    |
 | and distribute this software subject to the following restrictions:        |
 |                                                                            |
 |  1)	You may not charge money for it.                                      |
 |  2)	You may not remove or alter this license, copyright notice, or the    |
 |      included disclaimers.                                                 |
 |  3)	You may not claim you wrote it.                                       |
 |  4)	If you make improvements (or other changes), you are requested        |
 |	to send them to the Heyu maintainer so there's a focal point for      |
 |      distributing improved versions.                                       |
 |                                                                            |
 | As used herein, HEYU is a trademark of Daniel B. Suthers.                  | 
 | X10, CM11A, and ActiveHome are trademarks of X-10 (USA) Inc.               |
 | The author is not affiliated with either entity.                           |
 |                                                                            |
 | Charles W. Sullivan                                                        |
 | Co-author and Maintainer                                                   |
 | Greensboro, North Carolina                                                 |
 | Email ID: cwsulliv01                                                       |
 | Email domain: -at- heyu -dot- org                                          |
 |                                                                            |
 | Disclaimers:                                                               |
 | THERE IS NO ASSURANCE THAT THIS SOFTWARE IS FREE OF DEFECTS AND IT MUST    |
 | NOT BE USED IN ANY SITUATION WHERE THERE IS ANY CHANCE THAT ITS            |
 | PERFORMANCE OR FAILURE TO PERFORM AS EXPECTED COULD RESULT IN LOSS OF      |
 | LIFE, INJURY TO PERSONS OR PROPERTY, FINANCIAL LOSS, OR LEGAL LIABILITY.   |
 |                                                                            |
 | TO THE EXTENT ALLOWED BY APPLICABLE LAW, THIS SOFTWARE IS PROVIDED "AS IS",|
 | WITH NO EXPRESS OR IMPLIED WARRANTY, INCLUDING, BUT NOT LIMITED TO, THE    |
 | IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.|
 |                                                                            |
 | IN NO EVENT UNLESS REQUIRED BY APPLICABLE LAW WILL THE AUTHOR BE LIABLE    |
 | FOR DAMAGES, INCLUDING ANY GENERAL, SPECIAL, INCIDENTAL OR CONSEQUENTIAL   |
 | DAMAGES ARISING OUT OF THE USE OR INABILITY TO USE THIS SOFTWARE EVEN IF   |
 | THE AUTHOR HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.            |
 |                                                                            |
 +----------------------------------------------------------------------------*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_FEATURE_CM17A  /* Compile only if configured for CM17A */

#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <ctype.h>
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#include "x10.h"
#include "process.h"

#ifdef        SCO
#define _SVID3 /* required for correct termio handling */
#undef  _IBCS2 /* conflicts with SVID3  */
#endif

#ifdef TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <signal.h>

#ifdef HAVE_ASM_IOCTLS_H
#include <asm/ioctls.h>
#endif
#ifdef HAVE_LINUX_SERIAL_REG_H
#include <linux/serial_reg.h>
#endif
#ifdef HAVE_LINUX_SERIAL_H
#include <linux/serial.h>
#endif
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#endif
#ifdef HAVE_SYS_TERMIOS_H
#include <sys/termios.h>
#endif
#ifdef HAVE_SYS_TERMIO_H
#include <sys/termio.h>
#endif
#ifdef HAVE_TERMIOS_H
#include <termios.h>
#endif
#ifdef HAVE_TERMIO_H
#include <termio.h>
#endif

#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif


extern int verbose;
extern int sptty;
extern int tty;
extern int ttylock(), munlock();
extern int i_am_relay;

extern CONFIG config;
extern CONFIG *configp;

#define FC_STANDBY (TIOCM_RTS | TIOCM_DTR)
#define FC_RESET   ~FC_STANDBY
#define FC_ONE     TIOCM_RTS
#define FC_ZERO    TIOCM_DTR

#define HC_MASK      0xF000
#define FUNC_MASK    0x00B8
#define UNIT_MASK    0x0458
#define NOSW_MASK    0x0003
#define OLD_MASK     0x0800
#define NOTX10_MASK  ~(HC_MASK|FUNC_MASK|UNIT_MASK|NOSW_MASK|OLD_MASK)

/* The CM17A will normally repeat its RF code    */
/* transmission 5 times (configp->def_rf_bursts)   */
/* at intervals of about 110 milliseconds        */
/* (configp->rf_burst_spacing).                    */
/* By resetting the unit in one of the intervals */
/* between transmissions the number of bursts    */
/* can be set from 1 through 5.                  */
/* It appears there is an effective window of    */
/* about 40-50  milliseconds at the end of an    */
/* interval when the reset will cut off          */
/* subsequent bursts without interfering with    */
/* the current burst.                            */
/* Time the reset at OFFSET milliseconds before  */
/* the start of the next interval.               */

#define OFFSET        10   

/* On many systems the nanosleep() function will  */
/* have a minimum resolution of 10 milliseconds   */
/* and a minimum time of 20 milliseconds.         */
/* The tweak (configp->rf_timer_tweak) attempts to  */
/* compensate for this to keep within the         */
/* effective window.                              */

/* RF Codes for X10 Housecodes (A-P) */
unsigned int hc2rfcode[16] = {
   0x6000, 0x7000, 0x4000, 0x5000,
   0x8000, 0x9000, 0xA000, 0xB000,
   0xE000, 0xF000, 0xC000, 0xD000,
   0x0000, 0x1000, 0x2000, 0x3000,
};   

/* Housecode for RF code (upper nybble) */
static char *rf2hc = "MNOPCDABEFGHKLIJ";

/* RF Codes for X10 _encoded_ unit codes */
unsigned int rf_unit_code[16] = {
   0x0440, 0x0040, 0x0008, 0x0408,
   0x0448, 0x0048, 0x0000, 0x0400,
   0x0450, 0x0050, 0x0018, 0x0418,
   0x0458, 0x0058, 0x0010, 0x0410,
};	

/* RF Function names for monitor */
char *rf_func_name[] = {
   "fAllOff", "fLightsOn", "fOn", "fOff",
   "fDim", "fBright", "fLightsOff",
   "fdimbo", "freset", "fArb"
};

/* RF Function Codes:               */
/* AllOff, LightsOn, On, Off,       */
/* Dim,    Bright, LightsOff        */
unsigned int rf_func_code[] = {
   0x0080, 0x0090, 0x0000, 0x0020,
   0x0098, 0x0088, 0x00a0,
   0x0000, 0x0000, 0x0000
};

/* Undocumented "No Switch" bits for TM751 and RR501    */
/* Transceivers.  If set, the built-in appliance module */
/* relay will not respond to the On, Off, and AllOff RF */
/* signals, and will just quietly transceive them.      */
unsigned int rf_nosw_code[] = {
   0x0001, 0x0000, 0x0002, 0x0002, 
   0x0000, 0x0000, 0x0000, 0x0000,
   0x0000, 0x0000,
};
	  
static unsigned long cm17a_loopcount;

static volatile unsigned long countdown; 	
static volatile int dummy = 1;
/*----------------------------------------------------------------------------+
 | Microsecond timer using timing loop.                                       |
 +----------------------------------------------------------------------------*/
static void looptimer ( void )
{
   countdown = cm17a_loopcount;

   while (dummy && countdown--);
   return;
}

/*----------------------------------------------------------------------------+
 | Microsecond timer using standard timing functions.  These don't have       |
 | sufficient resolution for fast RF mode in most Unix-like kernels.          |
 +----------------------------------------------------------------------------*/
static void stdtimer ( void )
{
   long microsec = configp->cm17a_bit_delay;

#ifdef HAVE_NSLEEP
   struct timestruc_t tspec;

   tspec.tv_sec = microsec / 1000000L;
   tspec.tv_nsec = 1000L * (microsec % 1000000L);

   while ( nsleep( &tspec, &tspec ) == -1 );
   return;
#else
#ifndef HAVE_NANOSLEEP
   struct timeval tspec;

   tspec.tv_sec = microsec / 1000000;
   tspec.tv_usec = microsec % 1000000;
   while ( usleep(tspec.tv_usec) == -1 );
#else
   struct timespec tspec;

   tspec.tv_sec = microsec / 1000000L;
   tspec.tv_nsec = 1000L * (microsec % 1000000L);

   while ( nanosleep( &tspec, &tspec ) == -1 );
#endif /* HAVE_NANOSLEEP */
   return;
#endif
}  


/*----------------------------------------------------------------------------+
 | Delay after normal CM17A command.                                          |
 +----------------------------------------------------------------------------*/
void rf_post_delay(void)
{
   millisleep(configp->rf_post_delay); 
   return;
}   

/*----------------------------------------------------------------------------+
 | Delay after 'farb' CM17A command                                           |
 +----------------------------------------------------------------------------*/
void rf_farb_delay(void)
{
   millisleep(configp->rf_farb_delay);
   return;
}   

/*----------------------------------------------------------------------------+
 | Delay after 'farw' CM17A command                                           |
 +----------------------------------------------------------------------------*/
void rf_farw_delay(void)
{
   millisleep(configp->rf_farw_delay);
   return;
}   

/*----------------------------------------------------------------------------+
 | Delay after 'flux' CM17A command                                           |
 +----------------------------------------------------------------------------*/
void rf_flux_delay( long msec )
{
   millisleep(msec);
   return;
}   

/*----------------------------------------------------------------------------+
 | Reset CM17A to the power-up state - with no log message.                   |
 +----------------------------------------------------------------------------*/
int reset_cm17a_quiet ( void ) 
{
   int status, retcode;

   retcode = ioctl(tty, TIOCMGET, &status);
   status &= FC_RESET;
   retcode |= ioctl(tty, TIOCMSET, &status);
   millisleep(10);

   retcode = ioctl(tty, TIOCMGET, &status);
   status = (status & FC_RESET) | FC_STANDBY;
   retcode |= ioctl(tty, TIOCMSET, &status);
   millisleep(500);

   return retcode;
}

/*----------------------------------------------------------------------------+
 | Reset CM17A to the power-up state.                                         |
 +----------------------------------------------------------------------------*/
int reset_cm17a( void ) 
{
   send_x10state_command(ST_RESETRF, 0);

   return reset_cm17a_quiet();
}

/*----------------------------------------------------------------------------+
 | Actuate the CM17A by appropriately toggling the RTS and DTR lines.         |
 +----------------------------------------------------------------------------*/
int write_cm17a( unsigned int rfword, int bursts, unsigned char rfmode )
{
   unsigned char buffer[16];
   unsigned char data, mask;
   int           status, signal;
   int           i, j, k, retcode, groups, presleep;
   void          (*delay)();

   if ( rfmode == RF_FAST && configp->timer_loopcount > 0 ) {
      cm17a_loopcount =
         configp->cm17a_bit_delay * (configp->timer_loopcount / 1000000L);
      delay = looptimer;
   }
   else {
      delay = stdtimer;
   }

   if ( rfmode == RF_FAST ) {
      groups = (bursts - 1) / (int)configp->def_rf_bursts + 1;
      bursts = (bursts - 1) % (int)configp->def_rf_bursts + 1; 
      presleep = (configp->rf_burst_spacing * configp->def_rf_bursts)
                   - OFFSET - configp->rf_timer_tweak;
   }
   else {
      groups = 1;
      bursts = min(bursts, (int)configp->def_rf_bursts);
      presleep = 0;
   }

   buffer[0] = 0xD5;
   buffer[1] = 0xAA;
   buffer[2] = (rfword & 0xff00) >> 8;
   buffer[3] = rfword & 0xff;
   buffer[4] = 0xAD;

   retcode = ioctl(tty, TIOCMGET, &status);
   status |= FC_STANDBY;
   retcode |= ioctl(tty, TIOCMSET, &status);
   delay();
   
   for ( i = 0; i < groups; i++ ) {
      if ( i > 0 )
         millisleep( presleep );
      for ( j = 0; j < 5; j++ ) {
         data = buffer[j];
         mask = 0x80;
         for ( k = 0; k < 8; k++ ) {
            delay(); /* Put delay at beginning of loop */
            signal = (data & mask) ? FC_ONE : FC_ZERO ;
#if 0
            status &= FC_RESET;
	    status |= signal;
#endif
            status = (status & FC_RESET) | signal;
	    retcode |= ioctl(tty, TIOCMSET, &status);
	    delay();
#if 0
	    status |= FC_STANDBY;
#endif
            status = (status & FC_RESET) | FC_STANDBY;
	    retcode |= ioctl(tty, TIOCMSET, &status);
	    mask = mask >> 1;
         }
      }
   }

   /* Timed reset here limits the number of RF bursts */
   if ( bursts > 0 ) {
      millisleep((configp->rf_burst_spacing * bursts)
           - OFFSET - configp->rf_timer_tweak);
      status &= FC_RESET;
      retcode |= ioctl(tty, TIOCMSET, &status);
      millisleep(10);
      status |= FC_STANDBY;
      retcode |= ioctl(tty, TIOCMSET, &status);
      millisleep(10);
   }
      
   return retcode;
}   
   
/*----------------------------------------------------------------------------+
 |
 +----------------------------------------------------------------------------*/
void xlate_rf( unsigned char type, char **fname, unsigned int rfword,
		     char *hcp, int *unitp, unsigned char *nosw )
{
   int j;
   
   *fname = rf_func_name[type];
   if ( type == 9 )
      return;
   
   *hcp = rf2hc[(rfword & HC_MASK) >> 12];
   *nosw = rfword & NOSW_MASK;

   rfword &= UNIT_MASK;
   *unitp = 0;
   for ( j = 0; j < 16; j++ ) {
      if ( rfword == rf_unit_code[j] ) {
         *unitp = code2unit(j);
	 break;
      }
   }
   return;
}   

/*----------------------------------------------------------------------------+
 | Display the CM17A command in the monitor/logfile                           |
 |                                                                            |
 | *** This really doesn't work very well, especially with multiple RF        |
 | commands.  The uncertain delay between transmission of the RF and the      |
 | reporting of the received power line signal by the CM11A often results in  |
 | the RF signals not properly interleaved with the resulting power line      |
 | signals.  It can be corrected only by setting an unreasonably long         |
 | post-delay.                                                                |
 +----------------------------------------------------------------------------*/
int display_rf_xmit ( unsigned char type, unsigned int rfword, int bursts )
{
   extern int sptty;

   int ignoret;

   static unsigned char template[10] = {
      0xff,0xff,0xff,6,ST_COMMAND,ST_XMITRF,0,0,0,0};

   if ( configp->disp_rf_xmit == NO )
      return 0;

   template[6] = type;
   template[7] = (rfword & 0xFF00) >> 8;
   template[8] = rfword & 0xFF;
   template[9] = (unsigned char)bursts;

   ignoret = write(sptty, template, 10);

   return 0;
}

   
#else  /* Stubs */
void xlate_rf( unsigned char type, char **fname, unsigned int rfword,
		     char *hcp, int *unitp, unsigned char *nosw ) {}
#endif  /* End of HAVE_FEATURE_CM17A code */



