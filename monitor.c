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


#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <setjmp.h>
#include <errno.h>
#include <syslog.h>
#if defined(SYSV) || defined(FREEBSD) || defined(OPENBSD)
#include <string.h>
#else
#include <strings.h>
#endif

#include <time.h>

#include "x10.h"
#include "process.h"


#ifdef __GLIBC__
/* msf - added for glibc/rh 5.0 */
#include <sys/types.h>
#endif

extern int timeout;
extern int sptty;
extern int i_am_monitor, i_am_state, i_am_relay, i_am_rfxmeter;
extern int heyu_parent;
extern void fix_tznames ( void );
extern void x10state_init_all ();
extern int read_x10state_file ();
extern CONFIG config;
extern CONFIG *configp;
extern FILE *fdsout, *fdserr;
extern FILE *fprfxo, *fprfxe;
extern char *datstrf(void);
extern int  clear_data_storage( void );

void iquit();
jmp_buf mjb;

void iquit( int signo )
{
    longjmp(mjb, 1);
}

void engine_quit ( int signo )
{
   char buffer[PATH_LEN + 1];
   signal(SIGTERM, engine_quit);
   syslog(LOG_ERR, "interrupt received\n");
   write_x10state_file();
   sprintf(buffer, "%s/LCK..%s%s", LOCKDIR, STATE_LOCKFILE, configp->suffix);
   unlink(buffer);
   exit(0);
}

int c_monitor( int argc, char *argv[] )
{
    off_t f_offset;
    int check4poll();
    struct stat stat_buf;
    char spoolfile[100];
    time_t time_now, time_prev;

#ifdef HASSELECT
    extern int tty;
    int newfd;
    int rtn;
    fd_set rfds;
    struct timeval tv;
    if ( tty == TTY_DUMMY )
       newfd = tty;
    else
       newfd=dup(tty);
#endif

    sprintf( spoolfile, "%s/%s%s", SPOOLDIR, SPOOLFILE, configp->suffix);
    (void) signal(SIGCHLD, iquit);
    (void) signal(SIGINT, iquit);
    if (setjmp(mjb))
	return(0);

    i_am_monitor = 1;

    if ( read_x10state_file() != 0 )
       x10state_init_all();

    if ( argc == 3 && strcmp(argv[2], "rfxmeter") == 0 ) {
       /* Disable display except for RFXMeter setup file pointers */
       i_am_monitor = 0;
       i_am_rfxmeter = 1;
       fdsout = fopen("/dev/null", "w");
       fdserr = fopen("/dev/null", "w");
       fprfxo = stdout;
       fprfxe = stderr;
    }
    else {
       fprintf(stdout, "%s Monitor started\n", datstrf());
       fflush(stdout);
    }
     
    f_offset = lseek(sptty, 0, SEEK_CUR);  /* find current position */

    time_now = time_prev = time(NULL);


    while (1)  {
	if( f_offset == lseek(sptty, 0, SEEK_END) )  {  /* find end of file */
	    if( stat( spoolfile, &stat_buf ) < 0)
	        return(0); 
#ifndef HASSELECT
	    /* this imposes a 1 second delay between the start of new output*/
	    /* It keeps the disk from being thrashed. */
	    sleep(1);
#else
            if ( tty != TTY_DUMMY ) {
	       FD_CLR(newfd, &rfds);
	       FD_SET(newfd, &rfds);
            }
	    tv.tv_sec = 0;	/* This does it even better */
	    tv.tv_usec = configp->engine_poll;
	    if( (rtn = select(1 ,NULL, NULL, NULL, &tv)) < 0 )
		perror("select failed\n");
#endif
	}
	else {
	    if ( fstat( sptty, &stat_buf ) < 0 )
	         return(0);
	    lseek(sptty, f_offset, SEEK_SET);
	    check4poll(1,1);
	    f_offset = lseek(sptty, 0, SEEK_CUR);  /* find current position */
	}

        /* Activate countdown timers */
        if ( (time_now = time(NULL)) != time_prev ) {
           second_tick(time_now, (long)(time_now - time_prev));
           time_prev = time_now;       
        }
    }
    /* return(0); */
}

int c_engine( int argc, char *argv[] )
{
    off_t f_offset;
    int check4poll();
    struct stat stat_buf;
    char spoolfile[100];
    time_t time_now, time_prev;
    long   tmin_now, tmin_prev, thour_now, thour_prev;
    mode_t oldumask;
    extern FILE *fdsout;
    extern FILE *fdserr;
    
#ifdef HASSELECT
    extern int tty;
    int newfd;
    int rtn;
    fd_set rfds;
    struct timeval tv;
    if ( tty == TTY_DUMMY )
       newfd = tty;
    else
       newfd=dup(tty);
#endif

    sprintf( spoolfile, "%s/%s%s", SPOOLDIR, SPOOLFILE, configp->suffix);

    (void) signal(SIGINT, engine_quit /*iquit*/);
    (void) signal(SIGTERM, engine_quit);

    if (setjmp(mjb))
	return(0);

    i_am_state = 1;
    i_am_monitor = 1;
    heyu_parent = D_CMDLINE;

    openlog( "heyu_engine", 0, LOG_DAEMON);
    syslog(LOG_ERR, "engine setting up-\n");

    if ( read_x10state_file() != 0 )
       x10state_init_all();

    time_now = time_prev = time(NULL);
    tmin_now = tmin_prev = (long)time_now / 60L;
    thour_now = thour_prev = (long)time_now / 3600;

    engine_local_setup(E_START);
    fix_tznames();

    f_offset = lseek(sptty, 0, SEEK_CUR);  /* find current position */

    oldumask = umask(configp->log_umask);
    fdsout = freopen(configp->logfile, "a", stdout);
    fdserr = freopen(configp->logfile, "a", stderr);
    umask(oldumask);

    fprintf(fdsout, "%s Engine started\n", datstrf());
    fflush(fdsout);

    while (1) {
	if ( f_offset == lseek(sptty, 0, SEEK_END) )  {  /* find end of file */
	    if( stat( spoolfile, &stat_buf ) < 0)
	       engine_quit(SIGTERM) /* return(0) */; 
#ifndef HASSELECT
	    /* this imposes a 1 second delay between the start of new output*/
	    /* It keeps the disk from being thrashed. */
	    sleep(1);
#else
            if ( tty != TTY_DUMMY ) {
	       FD_CLR(newfd, &rfds);
	       FD_SET(newfd, &rfds);
            }
	    tv.tv_sec = 0;	/* This does it even better */
	    tv.tv_usec = configp->engine_poll;
	    if( (rtn = select(1 ,NULL, NULL, NULL, &tv)) < 0 )
		perror("select failed\n");
#endif /* HASSELECT */
	}
	else {
	    if ( fstat( sptty, &stat_buf ) < 0)
	         return(0);
	    lseek(sptty, f_offset, SEEK_SET);
	    check4poll(1,1);
	    f_offset = lseek(sptty, 0, SEEK_CUR);  /* find current position */
	}

        if ( (time_now = time(NULL)) != time_prev ) {
           if ( is_tomorrow(time_now) ) {
              /* Local Midnight. STD Time */
              midnight_tick(time_now);
              set_global_tomorrow(time_now);
           }
           if ( (thour_now = (long)time_now / 3600L) != thour_prev ) {
              /* Once every hour */
              hour_tick(time_now);
              thour_prev = thour_now;
           } 
           if ( (tmin_now = (long)time_now / 60L) != tmin_prev ) {
              /* Once every minute */
              minute_tick(time_now);
              tmin_prev = tmin_now;
           }
           /* Once every second */
           /* Activate countdown timers */
           second_tick(time_now, (long)(time_now - time_prev));
           time_prev = time_now;       
        }
    }
    /* return(0); */
}

