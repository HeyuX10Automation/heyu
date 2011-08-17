/*----------------------------------------------------------------------------+
 |                                                                            |
 |              HEYU CM10 "IBM Home Director HD16" Support                    |
 |               Copyright 2006 Charles W. Sullivan                           |
 |                                                                            |
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
 +----------------------------------------------------------------------------*/

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

#include <time.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <signal.h>

#ifdef LINUX
#include <asm/ioctls.h>
#   ifdef OLDLINUX
#include <linux/serial_reg.h>
#   endif
#include <linux/serial.h>
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#endif
#else
#    if (defined(POSIX) || defined(FREEBSD) || defined(OPENBSD))
#include <sys/termios.h>
#    else
#         ifdef SCO
#include <sys/termio.h>
#         else
#              ifdef DARWIN
#ifdef HAVE_TERMIOS_H
#include <termios.h>
#endif
#              else
#ifdef HAVE_TERMIO_H
#include <termio.h>
#endif
#              endif
#         endif
#    endif
#endif

#if (defined(OSF) || defined(DARWIN) || defined(NETBSD))
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#endif

#ifdef HASSELECT
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#endif


extern int verbose;
extern int sptty;
extern int tty;
extern int ttylock(), munlock();
extern int i_am_relay;
extern int xwrite(), exread();
extern int check4poll();

extern CONFIG config;
extern CONFIG *configp;

/* On CM10A versions having a 6-pin RJ11 connector and cable,        */
/* if the serial port RTS line is turned off for more than about 20  */
/* milliseconds, then turned on again, a CM10A will write a bunch    */
/* of characters to the serial port.  This is what I'm seeing with   */
/* the unit I have.  Their interpretation is currently unknown.      */

unsigned char cm10a_standard_response[29] =
    {0x00,0x78,0x1e,0x00,0x00,0x80,0x78,0x00,
     0xf0,0xf8,0x00,0x78,0xfe,0x80,0x80,0x00,
     0x78,0x00,0x00,0x78,0x00,0x00,0x78,0x1e,
     0x80,0x78,0x1e,0x78,0x1e};

/* Example CM10 macro from protocol, equivalent to:   */
/*   trigger C1 off mac1                              */
/*   macro mac1 0 dim A1-3 11; on A4                  */
/*   trigger C2 on  mac2                              */
/*   macro mac2 0 on A1; dim A2,3 6; off A4           */

static unsigned char example[] =
  {0xfb, 0x26,0x0a,0x04,0x66,0x2e,0x04,0x0b,0x02,0x6a,0x02,
         0x26,0x8c,0x02,0x66,0x02,0x03,0x6e,0x42,0x06,0x02,0x6a,0x03,};


/*----------------------------------------------------------------------------+
 | Turn the RTS serial line off.                                              |
 +----------------------------------------------------------------------------*/
int turn_rts_off( void ) 
{
   int status, retcode;

   retcode = ioctl(tty, TIOCMGET, &status);
   status &= ~TIOCM_RTS;
   retcode |= ioctl(tty, TIOCMSET, &status);

   return retcode;
}

int c_turn_rts_off( int argc, char *argv[] ) 
{
   if ( turn_rts_off() != 0 ) {
      fprintf(stderr, "Unable to turn RTS line Off.\n");
      return 1;
   }
   return 0;
}


/*----------------------------------------------------------------------------+
 | Turn the RTS serial line on.                                               |
 +----------------------------------------------------------------------------*/
int turn_rts_on( void ) 
{
   int status, retcode;

   retcode = ioctl(tty, TIOCMGET, &status);
   status |= TIOCM_RTS;
   retcode |= ioctl(tty, TIOCMSET, &status);

   return retcode;
}

int c_turn_rts_on( int argc, char *argv[] ) 
{
   if ( turn_rts_on() != 0 ) {
      fprintf(stderr, "Unable to turn RTS line On.\n");
      return 1;
   }
   return 0;
}


/*----------------------------------------------------------------------------+
 | Toggle the RTS serial line off, then back on                               |
 +----------------------------------------------------------------------------*/
int toggle_rts( void ) 
{
   int retcode;

   retcode = turn_rts_off();

   millisleep(100);

   retcode |= turn_rts_on();

   return retcode;
}

/*----------------------------------------------------------------------------+
 | Ask the CM10A to identify itself.                                          |
 +----------------------------------------------------------------------------*/
int c_cm10a_ident ( int argc, char *argv[] )	
{
    unsigned char buf[50];
    unsigned char *bp;
    int      j, count, nread, left;
    extern void millisleep();
    
    if ( (toggle_rts()) != 0 ) {
       fprintf(stderr, "Unable to toggle RTS line.\n");
       return 1;
    }

    bp = buf;
    nread = 0; left = 30;
    for ( j = 0; j < 3; j++ ) {
       count = exread(sptty, bp, left, 1);
       nread += count;
       if ( nread == 29 ) {
          if ( memcmp(buf, cm10a_standard_response, 29) == 0 ) {
             printf("CM10 is connected.\n");
	     check4poll(0,1);
             return 0;
          }
          else {
             fprintf(stderr, "Non-standard CM10A response.\n");
	     check4poll(0,1);
             return 1;
          }
       }
       bp += count;
       left -= count;
       millisleep(10);
   }
 
   if ( nread == 0 ) {    
     fprintf(stderr, "No response from CM10A\n");
   }
   else {
     fprintf(stderr, "Invalid response, %d bytes returned.\n", nread);
   }

   return 1;
}

/*----------------------------------------------------------------------------+
 | For versions of the CM10A with a 6-pin RJ11 connector, the factory         |
 | CM10A cable has a jumper between DB-9 pins 4 (DTR) and 6 (DSR),            |
 | neither of which are connected to the interface.  These function verify    |
 | that the cable is connected at the PC end at least.                        |
 +----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------+
 | This function just reads the serial status, normally assuming that the DTR |
 | line is active.  It returns 0 if both the DTR and DSR lines are active;    |
 | 1 if the DTR is active and the DSR is not; -1 if the DTR is not.           | 
 +----------------------------------------------------------------------------*/
int cm10a_cable_check ( void )
{
   int status;
    
   ioctl(tty, TIOCMGET, &status);

   if ( !(status & TIOCM_DTR) )
      return -1;

   if ( !(status & TIOCM_DSR) )
      return 1;

   return 0;
}

/*----------------------------------------------------------------------------+
 | This function toggles the DTS line.                                        |
 | It returns 0 if the DSR activity follows the DTR activity.                 |
 | Otherwise it returns 1 if DSR remains inactive when the DTR is toggled or  |
 | -1 if the DSR remains active regardless.                                   |
 +----------------------------------------------------------------------------*/
int cm10a_cable_check_full ( void )
{
   int status, savestatus;

   ioctl(tty, TIOCMGET, &status);
   savestatus = status;

   status &= ~TIOCM_DTR;
   ioctl(tty, TIOCMSET, &status);
   millisleep(10);
   ioctl(tty, TIOCMGET, &status);
   if ( status & TIOCM_DSR ) {
      ioctl(tty, TIOCMSET, &savestatus);
      return -1;
   }

   status |= TIOCM_DTR;
   ioctl(tty, TIOCMSET, &status);
   millisleep(10);
   ioctl(tty, TIOCMGET, &status);
   if ( !(status & TIOCM_DSR) ) {
      ioctl(tty, TIOCMSET, &savestatus);
      return 1;
   }

   ioctl(tty, TIOCMSET, &savestatus);

   return 0;
}   


/*----------------------------------------------------------------------------+
 |  Initialize CM10A interface by uploading a dummy macro block.              |
 +----------------------------------------------------------------------------*/
int c_cm10a_init ( int argc, char *argv[] )
{
    unsigned char macrodata[50];
    unsigned char buf[3];
    unsigned int j, n, code;
    unsigned char cksum;
    extern int usage(), xwrite(), xread(), exread(), check4poll();

    int ignoret;

    if ( !(configp->device_type & DEV_CM10A) && i_am_relay != 1 ) {
       fprintf(stderr,
	 "Heyu not configured for CM10A - see man page heyu(1)\n");
       return 1;
    }

    if( argc > 2  )
       usage(E_2MANY);

#if 0  
    if ( cm10a_cable_check() == 1 ) {
       if ( i_am_relay != 1 ) 
          fprintf(stderr, "CM10A is not connected.\n");
       return 1;
    }
#endif

    macrodata[0] = 0xfb;          /* CM10A init code */
    for ( j = 1; j < 43; j++ ) {
       macrodata[j] = 0;
    }

    example[0] = example[0];  /* Keep compiler happy */

    /* For testing, use example CM10 macro data from protocol */
#if 0
    memcpy(macrodata, example, sizeof(example));
#endif


    cksum = checksum(macrodata + 1, 42);
    
    code = 0;

#if 0
    xwrite(tty, (char *) macrodata, 43);
    if ( i_am_relay )
       n = xread(tty, buf, 1, 1);
    else
       n = exread(sptty, buf, 1, 1);

    if ( n != 1 )
       code |= 0x10;
    else if ( buf[0] != cksum )
       code |= 0x01;
    
    xwrite(tty, "\0", 1);   /* WRMI */
    if ( i_am_relay ) 
       n = xread(tty, buf, 1, 1);
    else
       n = exread(sptty, buf, 1, 1);
#endif

    if ( i_am_relay ) {
       ignoret = write(tty, (char *) macrodata, 43);
       n = xread(tty, buf, 1, 2);
    }
    else {
       xwrite(tty, (char *) macrodata, 43);
       n = exread(sptty, buf, 1, 1);
    }

    if ( n != 1 )
       code |= 0x10;
    else if ( buf[0] != cksum )
       code |= 0x01;
    
    if ( i_am_relay ) {
       ignoret = write(tty, "\0", 1);   /* WRMI */
       n = xread(tty, buf, 1, 2);
    }
    else {
       xwrite(tty, "\0", 1);   /* WRMI */
       n = exread(sptty, buf, 1, 1);
    }

    if ( n != 1 )
       code |= 0x20;
    else if ( buf[0] != 0x55 )
       code |= 0x02;

    if ( argc == 2 )    
       check4poll(0,1);		/* zero means to discard data */

    if ( i_am_relay != 1 )
       (void) printf("CM10A initialized.\n");

    if ( i_am_relay )
       return code;

    return 0;
}


int c_test_serial_port ( int argc, char *argv[] )
{
   int  j, k, nlist, retcode;
   int  all_lines, status, savestatus, result1, result2;
   int  delay = 10;

   static struct tiocm_list {
      int line;
      char *name;
   } list[] = {
     {TIOCM_LE,  "LE" },
     {TIOCM_DTR, "DTR"},
     {TIOCM_RTS, "RTS"},
     {TIOCM_ST,  "ST" },
     {TIOCM_SR,  "SR" },
     {TIOCM_CTS, "CTS"},
     {TIOCM_CD,  "CD" },
     {TIOCM_RI,  "RI" },
     {TIOCM_DSR, "DSR"},
   };
   nlist = (sizeof(list)/sizeof(struct tiocm_list));

   all_lines = 0;
   for ( j = 0; j < nlist; j++ )
      all_lines |= list[j].line;

   ioctl(tty, TIOCMGET, &status);
   savestatus = status;

   if ( status & TIOCM_CTS ) {
      printf("CTS initially On\n");
   }
   else {
      printf("CTS initially Off\n");
   }

   status |= TIOCM_CTS;
   retcode = ioctl(tty, TIOCMBIS, &status);
   millisleep(delay);
   status |= TIOCM_CTS;
   retcode |= ioctl(tty, TIOCMSET, &status);
   millisleep(delay);
   retcode |= ioctl(tty, TIOCMGET, &status);

   if ( status & TIOCM_CTS ) {
      printf("CTS finally On\n");
   }
   else {
      printf("CTS finally Off\n");
   }

   printf("retcode = %x\n", retcode);

   /* Test individual lines for toggleability */
   for ( j = 0; j < nlist; j++ ) {
      retcode = 0;
      status &= ~list[j].line;
      retcode |= ioctl(tty, TIOCMSET, &status);
      if ( delay )
         millisleep(delay);
      retcode |= ioctl(tty, TIOCMGET, &status);
      result1 = status & list[j].line;
      status |= list[j].line;
      retcode |= ioctl(tty, TIOCMSET, &status);
      if ( delay )
         millisleep(delay);
      retcode |= ioctl(tty, TIOCMGET, &status);
      result2 = status & list[j].line;
      if ( retcode != 0 )
         printf("Bad retcode\n");
      if ( result2 != result1 ) 
         printf("%-3s can be controlled by PC.\n", list[j].name);
   }

   /* Test for linked lines */
   for ( j = 0; j < nlist; j++ ) {
      retcode = 0;
      status &= ~all_lines;
      retcode |= ioctl(tty, TIOCMSET, &status);
      if ( delay )
         millisleep(delay);
      retcode |= ioctl(tty, TIOCMGET, &status);
      result1 = status & all_lines;
      status &= ~all_lines;
      status |= list[j].line;
      retcode |= ioctl(tty, TIOCMSET, &status);
      if ( delay )
         millisleep(delay);
      retcode |= ioctl(tty, TIOCMGET, &status);
      result2 = (status ^ result1) & ~list[j].line & all_lines;
      if ( retcode != 0 )
         printf("Bad retcode\n");
      for ( k = 0; k < nlist; k++ ) {
         if ( result2 & list[k].line )
            printf("%-3s is linked to %-3s\n", list[k].name, list[j].name);
      }
   }
   for ( j = 0; j < nlist; j++ ) {
      retcode = 0;
      status |= all_lines;
      retcode |= ioctl(tty, TIOCMSET, &status);
      if ( delay )
         millisleep(delay);
      retcode |= ioctl(tty, TIOCMGET, &status);
      result1 = status & all_lines;
      status |= all_lines;
      status &= ~list[j].line;
      retcode |= ioctl(tty, TIOCMSET, &status);
      if ( delay )
         millisleep(delay);
      retcode |= ioctl(tty, TIOCMGET, &status);
      result2 = (status ^ result1) & ~list[j].line & all_lines;
      if ( retcode != 0 )
         printf("Bad retcode\n");
      for ( k = 0; k < nlist; k++ ) {
         if ( result2 & list[k].line )
            printf("%-3s is revlinked to %-3s\n", list[k].name, list[j].name);
      }
   }

   ioctl(tty, TIOCMSET, &savestatus);

   return 0;
}
   

   

   
