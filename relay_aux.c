
/*----------------------------------------------------------------------------+
 |                                                                            |
 |                  HEYU Auxiliary Input Daemon                               |
 |       Copyright 2002,2003,2004-2007 Charles W. Sullivan                    |
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

#ifdef SCO
#define _IBCS2
#endif

#include <stdio.h>
#include <ctype.h>
#include <signal.h>
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#endif
#ifdef LINUX
#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif
#endif
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <time.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include "x10.h"
#include "process.h"

#ifdef pid_t
#define PID_T pid_t
#else
#define PID_T long
#endif

extern int tty,sptty;
extern int verbose;
extern int i_am_relay;
extern void quit(), error();

extern char x10_tty_aux[PATH_LEN + 1];
extern unsigned char alert_ack[]; 
extern CONFIG config;
extern CONFIG *configp;

char spoolfile[PATH_LEN + 1];

extern int is_ring( void );
int relay_ri_control( void );
extern int i_am_aux;

extern int heyu_parent;

static char saved_auxport[PATH_LEN + 1] = "";
static char saved_suffix[PATH_LEN + 1] = "";
static unsigned char saved_auxdev;
static char auxfilename[PATH_LEN + 1];

unsigned int rflastaddr[16];
unsigned int rftransceive[16][7];
unsigned int rfforward[16][7];

/*---------------------------------------------------------------+
 | Set up tables determining whether received standard X10 RF    |
 | signals are to be transceived, forwarded, or ignored.         |
 +---------------------------------------------------------------*/
void configure_rf_tables ( void )
{
   ALIAS         *aliasp;
   int           hcode;
   unsigned int  mask;
   unsigned long optflags;
   int           j, k;

   for ( hcode = 0; hcode < 16; hcode++ ) {
#if 0	   
      rflastaddr[hcode] = 0;
#endif
      rflastaddr[hcode] = 1;
      mask = 1 << hcode;
      for ( k = 0; k < 7; k++ ) {
         if ( configp->transceive & mask )
             rftransceive[hcode][k] = 0xffff;
         else
             rftransceive[hcode][k] = 0;

         if ( configp->rfforward & mask )
             rfforward[hcode][k] = 0xffff;
         else
             rfforward[hcode][k] = 0;
      }
   }

   if ( !(aliasp = configp->aliasp) )
      return;

   j = 0;
   while ( aliasp[j].line_no > 0 ) {
      hcode = hc2code(aliasp[j].housecode);
      optflags = aliasp[j].optflags & (MOPT_TRANSCEIVE | MOPT_RFFORWARD | MOPT_RFIGNORE) ;
      if ( optflags == 0 || aliasp[j].modtype < 0 ) {
         j++;
         continue;
      }
      
      for ( k = 2; k < 6; k++ ) {
         if ( (aliasp[j].flags & (unsigned long)(1 << k)) == 0 )
            continue;

         if ( optflags & MOPT_TRANSCEIVE ) {
            rftransceive[hcode][k] |= aliasp[j].unitbmap;
            rfforward[hcode][k] &= ~aliasp[j].unitbmap;
         }
         else if ( optflags & MOPT_RFFORWARD ) {
            rftransceive[hcode][k] &= ~aliasp[j].unitbmap;
            rfforward[hcode][k] |= aliasp[j].unitbmap;
         }
         else if ( optflags & MOPT_RFIGNORE ) {
            rftransceive[hcode][k] &= ~aliasp[j].unitbmap;
            rfforward[hcode][k] &= ~aliasp[j].unitbmap;
         }
      }
      j++ ;
   }
   return ;
}


/*---------------------------------------------------------------+
 | Close the aux serial port and unlink the port and daemon lock |
 | files.                                                        |
 +---------------------------------------------------------------*/
void aux_shutdown_port ( char *saved_auxport )
{
   char filename[PATH_LEN + 1];
   extern int tty_aux;
   extern int munlock(char *);

   if (tty_aux >= 0 ) {
      close(tty_aux);
      tty_aux = -1;
   }

   if ( saved_auxport[0] != '\0' ) {
      munlock(saved_auxport);
      sprintf(filename, "%s%s", AUXFILE, saved_suffix);
      munlock(filename);
   }
   if ( configp->ttyaux != '\0' ) {
      munlock(configp->ttyaux);
      sprintf(filename, "%s%s", AUXFILE, configp->suffixaux);
      munlock(filename);
   }
   return;
}  

   
/*---------------------------------------------------------------+
 | Aux local setup.                                              |
 +---------------------------------------------------------------*/
void aux_local_setup ( void )
{

   extern int ttylock(), lock_for_write(), munlock();
   extern int aux_baudrate ( void );
   extern int setup_tty_aux ( int, int );
   int configure_rf_receiver(unsigned char);
   extern int tty_aux;
   int msglevel = 0;

   if ( (saved_auxport[0] != '\0') &&
         ((strcmp(saved_auxport, configp->ttyaux) != 0) ||
         (configp->auxdev != saved_auxdev)) ) {
      syslog(LOG_ERR, "Shutting down aux port %s\n", saved_auxport);
      aux_shutdown_port(saved_auxport);
      saved_auxport[0] = '\0';
      saved_auxdev = 0;
      msglevel = 1;
      millisleep(100);
   }

   if ( saved_auxport[0] == '\0' ) {
      sprintf(auxfilename, "%s%s", AUXFILE, configp->suffixaux);
      if ( ttylock(auxfilename) < 0 )  {
         aux_shutdown_port(saved_auxport);
         syslog(LOG_ERR, "Could not set up the heyu_aux lock-\n");
         exit(0);	
      }
      if ( setup_tty_aux(aux_baudrate(), 1) != 0 || tty_aux < 0 ) {
         syslog(LOG_ERR, "Unable to open tty_aux\n");
         aux_shutdown_port(saved_auxport);
         exit(1);
      }
      if ( msglevel > 0 ) {
         syslog(LOG_ERR, "Opening aux port %s\n", configp->ttyaux);
      }
   }

   strncpy2(saved_auxport, configp->ttyaux, PATH_LEN);
   strncpy2(saved_suffix, configp->suffixaux, PATH_LEN);
   saved_auxdev = configp->auxdev;

   configure_rf_tables();

   return;
}

void aux_daemon_quit ( int signo )
{
   syslog(LOG_ERR, "interrupt received.\n");
   aux_shutdown_port(saved_auxport);
   exit(0);
}

/*---------------------------------------------------------------+
 | Start the daemon for the auxilliary input device.             |
 +---------------------------------------------------------------*/  
int c_start_aux ( char *tty_auxname )
{
   long child;
   PID_T pid;
   int in_sync;
   int first_byte;
   char spoolfilename[PATH_LEN + 1];
   char relayfilename[PATH_LEN + 1];

   char writefilename[PATH_LEN + 1];
   struct stat file_buf;
   extern char *argptr;
   extern int ttylock(), lock_for_write(), munlock();
   extern int aux_w800(), aux_rfxcomvl(), aux_mr26a();
   extern int setup_tty_aux( int, int ), setup_sp_tty();
   extern PID_T lockpid( char * );
   PID_T was_locked;
   int is_idle;
   extern int sxread(int, unsigned char *, int, int);

   first_byte = 1;
   in_sync = 0;
   was_locked = (PID_T)0;
   is_idle = 0;

   if ( !configp->ttyaux ) {
      error("Serial port TTY_AUX not specified.");
      exit(1);
   }

   /* set up the file names */
   sprintf(spoolfilename, "%s%s", SPOOLFILE, configp->suffix);
   sprintf(relayfilename, "%s%s", RELAYFILE, configp->suffix);
   sprintf(writefilename, "%s%s", WRITEFILE, configp->suffix);
   sprintf(auxfilename,   "%s%s", AUXFILE, configp->suffixaux);
   spoolfile[0] = '\0';
   strcat(spoolfile, SPOOLDIR);

   if ( spoolfile[strlen(spoolfile) - 1] != '/' )
       strcat(spoolfile, "/");

   if ( stat(spoolfile, &file_buf) < 0 ) {
       char tmpbuf[sizeof(spoolfile) + 100];
       sprintf(tmpbuf, "The directory %s does not exist or is not writable.",
               spoolfile);
       error(tmpbuf);
   }
   strcat(spoolfile, spoolfilename);

   /* is an aux daemon in place ? */
    if ( lockpid(auxfilename) > (PID_T)1)  {
       if ( verbose )
          printf("There was already an aux daemon running (pid = %ld)\n",
		(long)lockpid(auxfilename) );

       return(-1);		/* there was a valid aux relay running */
    }
    else {
        if ( verbose )
               printf("Aux lockfile not found - spawning heyu_aux.\n"); 
 	
	/* Spawn an aux process */
	child = fork();
	if ( child > 0 )  {
	    sleep(3);		/* give child time to set up */
	    return(1);		/* this is parent process */
	}
	if ( child < 0 )  {	     /* This is an error */
	    perror("I could not spawn heyu_aux process");
	    syslog(LOG_DAEMON | LOG_ERR, "I could not spawn heyu_aux process.\n");
	    quit();
	}
    }

    /* from this point out, it should be the child. */

    close(0);
    close(1);
    close(2);

    strcpy(argptr, "heyu_aux");

    pid = setsid();   /* break control terminal affiliation */
    openlog( "heyu_aux", 0, LOG_DAEMON);
    if ( pid == (PID_T)(-1) )  {
	syslog(LOG_ERR, "aux setsid failed--\n");
	quit(1);
    }
    else {
	syslog(LOG_ERR, "aux setting up-\n");
    }


    /* Ok. We're alone now. */

    while ( 1 ) {
       aux_local_setup();

       i_am_relay = 1;	/* set flag to inhibit printing */
       i_am_aux = 1;

       setup_sp_tty();

       putenv("HEYU_PARENT=AUXDEV");
       heyu_parent = D_AUXDEV;

       (void) signal(SIGINT, aux_daemon_quit);
       (void) signal(SIGTERM, aux_daemon_quit);
       (void) signal(SIGHUP, aux_daemon_quit);


       if ( configp->auxdev == DEV_W800RF32 )  {
          /* Process loop for data from the W800RF32 */
          aux_w800();
       }
       else if ( configp->auxdev == DEV_RFXCOM32 ) {
          /* Process loop for data from the RFXCOM in W800 mode */
          aux_w800();
       }
       else if ( configp->auxdev == DEV_RFXCOMVL ) {
          /* Process loop for data from the RFXCOM in variable length mode */
          aux_rfxcomvl();
       }
       else {
          /* Process loop for data from the MR26A */
          aux_mr26a();
       }
    } /* End restart loop */

    return 0;
}


/*---------------------------------------------------------------------+
 | Verify that the aux daemon is running by checking for its lock      |
 | file.                                                               |
 +---------------------------------------------------------------------*/
int check_for_aux ( void )
{
   struct stat statbuf;
   int retcode;
   char lockpath[PATH_LEN + 1];

   if ( !configp->ttyaux[0] )
      return -1;

   sprintf(lockpath, "%s/LCK..%s%s", LOCKDIR, AUXFILE, configp->suffixaux);

   retcode = stat(lockpath, &statbuf);

   return retcode;
}

void cleanup_aux ( void )
{
   aux_shutdown_port(saved_auxport);
   return;
}
