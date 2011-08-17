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


/* This module is to be called by the first process to run under HEYU.
 * It:
 * 1) Locks the TTY port by putting it's pid in LOCKDIR/LCK..ttyX
 * 2) Validates any existing HEYU locks in LOCKDIR/LCK..heyu.relay.ttyX
 *    and sets a lock in LOCKDIR/LCK..heyu.relay.ttyX  with it's PID if none exists.
 * 3) Starts reading from the TTY port associated with the CM11A
 *    and writing the raw bytes to SPOOLDIR/heyu.out.ttyX
 *    The heyu.out.ttyX file will be deleted if it exists and created new.
 * 4) Upon SIGHUP signal will truncate the .in file.... someday, but not yet
 * 5) Upon SIGTERM or SIGINT will...
 *    Close the tty port
 *    unlink the TTY lock 
 *    unlink the heyu.relay.ttyX lock
 *    unlink the heyu.out.ttyX file
 *    unlink the x10_tty file
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
#include <errno.h>

#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#endif
#ifdef LINUX
#include <sys/resource.h>
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

extern int tty;
extern int verbose;
extern int i_am_relay;
extern void quit(), error();

extern unsigned char alert_ack[]; 
extern CONFIG config;
extern CONFIG *configp;

char spoolfile[PATH_LEN + 1];

static char saved_auxport[PATH_LEN + 1] = "";
static char saved_suffixaux[PATH_LEN + 1] = "";

static char restartfile[PATH_LEN + 1];

int interrupted = 0;
void alarmist(int signo);
void flag_intr();
void cleanup_files();
extern int is_ring( void );
extern int heyu_parent;
int relay_ri_control( void );
int port_status_init ( int, char, char );

extern struct opt_st *optptr;

#if 0
int seconds;
int check_flag;
int port_health_check( int );
int relay_ping( int, int );
#endif


/* tty should be the device that we are going to use.  It should be a fully
 * qualified path name (/dev/tty2), but may be just the device (tty2)
 */

int start_relay ( char *tty_name )
{
   unsigned char ibuff[80];
   long child;
   PID_T pid;
   int outfd;
   int j, count, expected;
   int powerfail, in_sync;
   int count_5a;
   int first_byte;
   char argv[2][5];
   char spoolfilename[PATH_LEN + 1];
   char relayfilename[PATH_LEN + 1];
   char writefilename[PATH_LEN + 1];
   struct stat file_buf;
   extern char *argptr;
   extern int ttylock(), c_setclock(), lock_for_write(), munlock();
   extern int c_stop_cm11a_poll(int, char **);
   extern int setup_tty(), setup_sp_tty(), port_locked;
   extern int write_restart_error( char * );
   extern PID_T lockpid( char * );
   time_t pfail_time, starttime, boottime = 0;
   unsigned char bootflag;
   unsigned char alert_count;
   extern int relay_powerfail_script(void);
   PID_T was_locked;
   char saved_auxport[PATH_LEN + 1] = "";

   int is_idle;
   int is_err = 0;
   extern int check_for_engine();
   extern int sxread(int, unsigned char *, int, int);
   int restart_aux( char * );

   int ignoret;

   first_byte = 1;
   in_sync = 0;
   alert_count = 0;
   was_locked = (PID_T)0;
   is_idle = 0;

   strcpy(saved_auxport, configp->ttyaux);
   strcpy(saved_suffixaux, configp->suffixaux);

   /* set up the spool file name */
   sprintf(spoolfilename, "%s%s", SPOOLFILE, configp->suffix);
   sprintf(relayfilename, "%s%s", RELAYFILE, configp->suffix);
   sprintf(writefilename, "%s%s", WRITEFILE, configp->suffix);
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

   /* is a relay in place ? */
    if ( lockpid(relayfilename) > (PID_T)1)  {
       if ( stat(spoolfile, &file_buf) < 0 )  {
	   char tmpbuf[sizeof(spoolfile) + 100];
	   sprintf(tmpbuf, "The file %s does not exist or is not writable.",
		   spoolfile);
	   error(tmpbuf);
       }
       if ( verbose )
          printf("There was already a relay running (pid = %ld)\n",
		(long)lockpid(relayfilename) );

       return(-1);		/* there was a valid relay running */
    }
    else {
        if ( verbose )
               printf("Relay lockfile not found - spawning heyu_relay.\n");  	
	/* we will spawn a relay process */
	child = fork();
	if ( child > 0 )  {
            if ( heyu_parent == D_CMDLINE )
               printf("starting heyu_relay\n");
	    sleep(3);		/* give child time to set up */
	    return(1);		/* this is parent process */
	}
	if ( child < 0 )  {	     /* This is an error */
	    perror("I could not spawn heyu_relay process");
	    syslog(LOG_DAEMON | LOG_ERR, "I could not spawn heyu_relay process.\n");
	    quit();
	}
    }

    /* from this point out, it should be the child. */

    close(0);
    close(1);
    close(2);
    strcpy(argptr, "heyu_relay");
    pid = setsid();   /* break control terminal affiliation */
    openlog( "heyu_relay", 0, LOG_DAEMON);
    if ( pid == (PID_T)(-1) )  {
	syslog(LOG_ERR, "relay setsid failed--\n");
	quit(1);
    }
    else {
	syslog(LOG_ERR, "relay setting up-\n");
        boottime = time(NULL);
    }


    /* Ok. We're alone now. */

    strncpy2(restartfile, pathspec(RESTART_RELAY_FILE), PATH_LEN);
    unlink(restartfile);

    if ( ttylock(relayfilename) < 0 )  {
	syslog(LOG_ERR, "Could not set up the heyu relay lock-");
        exit(0);	/* a competing process must have started up
	                 * in the lastfew milliseconds
			 */
    }

    setup_tty(1);	/* open the real tty */
    i_am_relay = 1;	/* set flag so calling function will clean up. */

    port_status_init(tty, DTR_INIT, RTS_INIT);

    unlink(spoolfile);
#ifdef REVERT_PERMS
    outfd=open(spoolfile, O_WRONLY|O_CREAT|O_EXCL|O_APPEND, 0777);
#else
    outfd=open(spoolfile, O_WRONLY|O_CREAT|O_EXCL|O_APPEND, 0666);
#endif
    setup_sp_tty();   /* Per DBS 3/27/2006 */

    if ( outfd < 0 )  {
        syslog(LOG_ERR, "Trouble creating spoolfile (%s)", spoolfile);
	quit();
    }
#ifdef REVERT_PERMS
    chmod(spoolfile, 0777);
#endif

    (void) signal(SIGINT, flag_intr);
    (void) signal(SIGTERM, flag_intr);
    (void) signal(SIGHUP, flag_intr);

    /* certain codes come out 1 second apart.  These are the 5a and a5
     * codes.  They indicate the CM11A wants a response, ie a polling 
     * sequence indicator.
     * In order to handle powerfails, we have to timestamp each a5 character
     * as it comes in.  Three 0xa5 characters in a row, 1 second apart
     * would indicate a power fail condition that needs a reply.
     * If the very first byte received is a5 or 5a, it's a condition
     * that needs a reply.
     * As an alternative, a leading byte that's more than 1 second from the
     * previous one __may__ be a polling sequence.
     * Adding a counter to make sure it was a standalone byte may help when
     * something like a checkum just happens to equal 0xa5.
     */
    powerfail = 0;	/* increment this each time a 0xa5 is seen */
    strncpy2(argv[0], " ", sizeof(argv[0]) - 1); /* set a vector that can be used by c_setclock() */
    strncpy2(argv[1], " ", sizeof(argv[1]) - 1); /* set a vector that can be used by c_setclock() */
    count_5a = 0;
    pfail_time = time(NULL);

    in_sync = 1;

    if ( configp->ring_ctrl == DISABLE ) 
        relay_ri_control();
    
    while ( 1 )  {
	alarm(0);  /* just in case I ever forget */

        /* Check if restart needed */
        if ( is_idle && stat(restartfile, &file_buf) == 0 && is_err == 0 ) {
           if ( reread_config() != 0 ) {
              is_err = 1;
              syslog(LOG_ERR, "relay reconfiguration failed!\n");
              write_restart_error(restartfile);
           }
           else {
              send_x10state_command(ST_RESTART, 0);
              syslog(LOG_ERR, "relay reconfiguration-\n");
              strcpy(saved_auxport, configp->ttyaux);
              strcpy(saved_suffixaux, configp->suffixaux);
              unlink(restartfile);
           }
        }

        /* Check the spool file to make sure it has not exceeded limits */
        stat(spoolfile, &file_buf);

        if ( ((unsigned long)file_buf.st_size > configp->spool_max && is_idle) ||
              file_buf.st_size > SPOOLFILE_ABSMAX )  {
            send_x10state_command(ST_REWIND, 0);
            sleep(2);
            ignoret = ftruncate(outfd, (off_t)0);
            lseek(outfd, (off_t)0, SEEK_END);
        }
#if 0
        else if ( configp->lockup_check > 0 ) {
           seconds = time(NULL) % 60;
           if ( seconds > 30 && check_flag == 0 ) {
              check_flag = 1;
              if ( (configp->lockup_check & CHECK_PORT) && (fstat(tty, &file_buf) < 0) ) {
                 send_x10state_command(ST_LOCKUP, CHECK_PORT);
              }
              else if ( (configp->lockup_check & CHECK_CM11) && (relay_ping(tty, 1) != 0) ) {
                 send_x10state_command(ST_LOCKUP, CHECK_CM11);
              }
           }
           else if ( seconds <= 30 ) {
              check_flag = 0;
           }
        }
#endif

        starttime = time(NULL);

        count = sxread(tty, ibuff, 1, 5);

        if ( count <= 0 ) {
            is_idle = 1;
            continue;
        }
        is_idle = 0;


	if ( (time(NULL) - starttime) > 5 )  {
	    /* we must be in sync if it's been a while since the first byte */
	    in_sync = 1;
	}
	if ( (time(NULL) - starttime) > 2 )  {
            /* Cancel the checksum 0x5A alert if any of the alert_ack bytes */
            /* or the 0x5A is overdue.                                      */
            alert_count = 0;
	}

        /* Check for the three special alert bytes in a row which */
        /* indicate that the next 0x5A is a checksum rather than  */
        /* an incoming X10 signal.                                */
        if ( ibuff[0] == alert_ack[0] )
           alert_count = 1;
        else if ( ibuff[0] == alert_ack[1] && alert_count == 1 )
           alert_count = 2;
        else if ( ibuff[0] == alert_ack[2] && alert_count == 2 )
           alert_count = 3;
        else if ( ibuff[0] == 0x5A && alert_count == 3 ) {
            /* This is the 0x5a checksum */
            ignoret = write(outfd, ibuff, 1);
            alert_count = 0;
            continue;
        }           
        else 
           alert_count = 0;
    

	if ( ibuff[0] != 0x5a )  {
	    /* just write any unknown data */
	    ignoret = write(outfd,ibuff, 1);
        }
#if 0
	if ( ibuff[0] == 0x5a && (count_5a == 0 || in_sync == 1 ) )  {
	    /* write the first 0x5a that's seen */
	    ignoret = write(outfd,ibuff, 1);

	    /* if we've reached this point and the RI is still asserted, it
	     * should be time for sending 0xc3, NO?
	     * let's try that next time that I'm deep into the code.
	     */
	}
#endif
	if ( ibuff[0] == 0x5a && count == 1)  {

            /* CM11A has a character to read */
	    if ( ++count_5a > 1 || (count_5a == 1 && in_sync == 1) )  {
                /* After a (configurable) short delay, tell the CM11A to send it */
                millisleep(configp->cm11a_query_delay);
		ibuff[1] = 0xC3;

		/* if( lock_for_write() == 0 ) */
		{
		    port_locked = 1;
		    ignoret = write(tty,ibuff + 1, 1);
		    munlock(writefilename);
                    port_locked = 0;
		}

		/* read the number of bytes to read. If it's greater than
		 * the size of the buffer, let the outer loop copy it 
		 * to the spoolfile. (out of sync... Noise, etc )
		 * If it's 1 byte, there's the chance that it's a special
		 * Like power fail or hail request.
		 */

	         count = sxread(tty, ibuff + 1, 1, 5); 	/* Get number of bytes which follow */
	         				      
	         if ( count == 1 ) {
                     /* so far so good */

		     expected = ibuff[1];

                     if ( expected == 0x5a ) {
                        /* CM11A quirk - sometimes send 0xC3, get nothing */
                        /* back until another 0x5A one second later.      */
			count_5a = 1;
			in_sync = 0;
                        continue;
                     }

		     if ( expected > 20 )  {
                         ignoret = write(outfd,ibuff, 2);
                         /* Too many. We must not be synced. */
		         in_sync = 0;
		         continue; /* go to outer while to grab this */
		     }

                     count = sxread(tty, ibuff + 2, expected, 5);

		     if ( count != expected )  {
		         /* This should be too few. so we aren't in sync yet. */
                         ibuff[1] = count;
		         ignoret = write(outfd,ibuff, count + 2);
		         in_sync = 0;
		         continue; /* go to outer while to grab this */
		     }
		     if ( count == expected )  {
			count_5a = 0;
			in_sync = 1;
			ignoret = write(outfd,ibuff, count + 2);
                     }

	         }
	         else  {
	             /* we did not get any response, so let the outer loop handle it. */
	             continue;	
	         }
	    }
	}
	else  {
	    count_5a = 0;
	}

	if ( ibuff[0] == 0xa5 && count == 1 )  {
            /* CM11A reports a power fail */
	    if ( powerfail == 0 )  {
                /* set timestamp for the first poll */
	        pfail_time = time(NULL);
	    }
	    if ( (first_byte == 1) || (powerfail++ >= 2) )  {
		if ( (powerfail >= 3) && 
		    ((pfail_time != 0) && ((time(NULL) - pfail_time) > 2)) )  {
		    /* 3 bytes of 'a5' in a row over a period of 3 seconds
			  means a power failure*/

                    /* Set lock file if not already set */
                    if ( (was_locked = lockpid(writefilename)) == (PID_T)0 ) {                    
                        if ( lock_for_write() < 0 )
                            error("Program exiting.\n");
                    }
                    port_locked = 1;

		    powerfail = 0;

                    bootflag = (time(NULL) - boottime) > ATSTART_DELAY ? 
                        R_NOTATSTART : R_ATSTART ;

                    if ( configp->device_type & DEV_CM10A ) {
                       c_cm10a_init(1, (char **)argv);
                    }
		    else if ( configp->pfail_update == NO ) {
		       c_stop_cm11a_poll(1, (char **)argv);
                    }
                    else {
		       c_setclock(1, (char **)argv);
                    }

                    if ( configp->ring_ctrl == DISABLE )
                       relay_ri_control();

		    pfail_time = 0;
		    in_sync = 1;

                    /* Remove the lock file if we locked it, */
                    /* otherwise leave it.                   */
                    if ( was_locked == (PID_T)0 ) {
                       munlock(writefilename);
                       port_locked = 0;
                    }

                    /* Launch a powerfail script directly from the relay */
                    /* (Don't use for Heyu commands.)                    */
                    relay_powerfail_script();

                    /* Notify state engine of the powerfail and let it */
                    /* launch a powerfail script.                      */
                    if ( bootflag & R_ATSTART && check_for_engine() != 0 ) {
                       /* Give the engine a little more time for startup */
                       for ( j = 0; j < 20; j++ ) {
                          millisleep(100);
                          if ( check_for_engine() == 0 )
                             break;
                       }
                    }
                    send_x10state_command(ST_PFAIL, bootflag);
		}
	    }
	}
	else  {
	    powerfail = 0;
	    pfail_time = 0;
	}
	first_byte = 0;
	ibuff[0] = '\0';

    }  /* End while() loop */

    /* return(0); */
} 

void alarmist(int signo)
{
   return;
}


void cleanup_files ( void )
{
    extern char spoolfile[];
    char buffer[PATH_LEN + 1];

    sprintf(buffer, "%s/LCK..%s%s", LOCKDIR, RELAYFILE, configp->suffix);
    unlink(buffer);
    sprintf(buffer, "%s/LCK..%s%s", LOCKDIR, WRITEFILE, configp->suffix);
    unlink(buffer);
    sprintf(buffer, "%s/LCK..%s%s", LOCKDIR, STATE_LOCKFILE, configp->suffix);
    unlink(buffer);
    sprintf(buffer, "%s/LCK.%s", LOCKDIR, configp->suffix);
    unlink(buffer);
    unlink(spoolfile);
    unlink(restartfile);
    return;
}


void flag_intr( int signo )
{
    extern int munlock();
    char buffer[PATH_LEN + 1];
    PID_T pid;
    PID_T lockpid();

    interrupted = 1;
    (void) signal(SIGTERM, flag_intr);
    syslog(LOG_ERR, "interrupt received\n");

    if ( configp->ttyaux[0] ) {
       sprintf(buffer, "%s%s", AUXFILE, configp->suffixaux);
       if ( (pid = lockpid(buffer)) > 0 ) {
          kill(pid, SIGTERM);
       }
       munlock(buffer);
       munlock(configp->ttyaux);
    }
    else if ( *saved_auxport ) {
       sprintf(buffer, "%s%s", AUXFILE, saved_suffixaux);
       if ( (pid = lockpid(buffer)) > 0 ) {
          kill(pid, SIGTERM);
       }
       munlock(buffer);
       munlock(saved_auxport);
    }

    sprintf(buffer, "%s%s", STATE_LOCKFILE, configp->suffix);
    if ( (pid = lockpid(buffer)) > 0 ) {
       kill(pid, SIGTERM);
       munlock(buffer);
    }

    sprintf(buffer, "%s%s", RELAYFILE, configp->suffix);
    munlock(buffer);
    munlock(configp->tty);
    sprintf(buffer, "%s%s", WRITEFILE, configp->suffix);
    munlock(buffer);

    unlink(spoolfile);
    unlink(restartfile);

    exit(0);
}



/*---------------------------------------------------------------+
 |  Quick check to verify the aux port is not in use by an       |
 |  earlier invocation of Heyu.  Return 0 if OK, 1 otherwise.    |
 +---------------------------------------------------------------*/  
int quick_ports_check ( void )
{
   struct stat statbuf;
   char lockpath[PATH_LEN + 1];

   if ( !configp->ttyaux[0] )
      return 0;

   sprintf(lockpath, "%s/LCK.%s", LOCKDIR, configp->suffixaux);
   if ( stat(lockpath, &statbuf) != 0 )
      return 0;

   sprintf(lockpath, "%s/LCK.%s", LOCKDIR, configp->suffix);
   if ( stat(lockpath, &statbuf) != 0 )
      return 1;

   return 0;
}
 
