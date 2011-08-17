/*
 * Changes for use with CM11A copyright 1996, 1997 - 2003 Daniel B. Suthers,
 * Pleasanton Ca, 94588 USA
 * E-mail dbs@tanj.com
 *
 */

/*
 * Copyright 1986 by Larry Campbell, 73 Concord Street, Maynard MA 01754 USA
 * (maynard!campbell).
 *
 * John Chmielewski (tesla!jlc until 9/1/86, then rogue!jlc) assisted
 * by doing the System V port and adding some nice features.  Thanks!
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
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <signal.h>
#include <setjmp.h>
#if    (defined(SCO) || defined (SOLARIS) || defined (ATTSVR4) || defined(OPENBSD) || defined(NETBSD))
#include <errno.h>
#else
#include <sys/errno.h>
#endif
#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include "x10.h"
#if (defined(LINUX) || defined(SOLARIS) || defined(FREEBSD) || defined(DARWIN) || defined(SYSV) || defined(OPENBSD) || defined(NETBSD))
#ifdef HAVE_STRING_H
#include <string.h>    /* char *strerror(); */
#endif
#endif

#include <time.h>

#include "process.h"
#include "local.h"
 
extern int verbose;
extern int i_am_relay, i_am_aux;
extern int tty;

unsigned alarm();
void sigtimer( int );
#ifdef HAS_ITIMER
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
struct itimerval iold, icurrent;
#endif
int xread ( int, unsigned char *, int, int );

/*
 * xread(fd, buf, count, timeout)
 *
 *	Timed read.  Works just like read(2) but gives up after
 *	timeout seconds, returning whatever's been read so far.
 */
/*  NOTE:  The CM11A will pop out a poll message every second when it wants
 *  to send an update.  Alarm(1) will go off when the next second starts.
 *  This may be any amount of time up to 1 second.  This makes a normal
 *  alarm(1) unsuitable.  Thus the use of setitimer.  Other OSes will have
 *  similar capabilities that assure a 1 second alarm is actually one second.
 */

jmp_buf jb;


int xread(fd, buf, count, timeout)
int fd, count, timeout;
unsigned char *buf;
{
    static int total;	/* so setjump won't clobber it */
    extern int i_am_relay;
    int counter;
    unsigned char *cbuf;
    char RCSID[]= "@(#) $Id: xread.c,v 1.10 2003/03/17 01:40:32 dbs Exp dbs $\n";

    display(RCSID);

    if (verbose && (! i_am_relay) ) {
    fprintf(stderr, "xread() called, count=%d, timeout = %d\n", count, timeout);
    fflush(stderr);
    }

    total = 0;

    if (setjmp(jb))
    {
	if (verbose && (! i_am_relay) ) {
	    fprintf(stderr, "xread() returning %d after timeout\n", total);
	    fflush(stderr);
	}
	(void) alarm(0);
	(void) signal(SIGALRM, SIG_IGN);
	return (total);
    }

    /* The compiler would complain that 
     *  "argument `buf' and count might be clobbered by `longjmp'"
     * This is bacause we change them after the setjmp.
     * To prevent this, we use copies of the arguements.
     */
    cbuf = buf;
    counter = count;

    (void)alarm(0);
    (void) signal(SIGALRM, sigtimer);
#ifdef HAS_ITIMER
    icurrent.it_interval.tv_sec = 0;
    icurrent.it_interval.tv_usec = 0;
    icurrent.it_value.tv_sec = timeout;
    icurrent.it_value.tv_usec = 30;
    if (setitimer(ITIMER_REAL, &icurrent, &iold) == -1)
	if( !i_am_relay )
	    perror ("setitimer");
#else
    (void) alarm((unsigned) timeout);
#endif


    errno = 0;
    while (counter--) {			/* loop till all characters are read */
	int i;
	if ((i = read(fd, (char *)cbuf, 1)) < 1) {
	    if (i == 0) {
		/* loop around on EOF */
		counter++;	/* reverse the decrement in the while loop. */
		if( errno == EINTR)  /* alarm was triggered */
		{
		    break;
		}
		continue;
	    }
	    if( (i < 0) && (i_am_relay != 1) )
	        perror("read");
	    else
	       if( (i < 0) && (i_am_relay == 1) )
	       {
	           syslog(LOG_ERR,"Relay Xread read error");
	           syslog(LOG_ERR, "%s", strerror(errno));
	       }
	    (void) alarm(0);
	    (void) signal(SIGALRM, SIG_IGN);
	    if (verbose) {
	        fprintf(stderr, "xread() returning %d bytes after error\n", total);
		fflush(stderr);
	    }
	    return (total);
	}
	cbuf++;
	total++;
    }
    (void) alarm(0);
    (void) signal(SIGALRM, SIG_IGN);
    if (verbose && (i_am_relay != 1) ) {
        if ( total == 0 )
           fprintf(stderr, "xread() returning 0 bytes\n");
        else
           fprintf(stderr, "xread() returning %d byte(s). The first is 0x%02x\n", total, buf[0] );
	fflush(stderr);
    }	
    return (total);
}

void sigtimer( int signo )
{
    if (verbose && (! i_am_relay)  ){
        fprintf(stderr, "Alarm - timeout\n");
        fflush(stderr);
    }
    (void) signal(SIGALRM, sigtimer);
    errno = EINTR;
    return;
    /* the jmp temporarily disabled (version 1.34)
            ... It was causing problems when triggered under linux 2.2.20 
            using USB.  The second trap would not behave properly.
            I don't know why.
    */

    /* longjmp(jb, 1); */
}



/* expect xread sends a code to the spool file that specifys how many
 * incoming characters are to be discarded.
 * The code is 3 0xff followed by (the number of bytes to expect +127)
 */
int exread(fd, buf, count, timeout)
int fd;
char *buf;
int count;
int timeout;
{
    unsigned char lbuf[160];
    extern int sptty;

    lbuf[1] = lbuf[2] = lbuf[0] = 0xff;
    lbuf[3] = count+127;
    if ( (!i_am_relay || i_am_aux) && write(sptty, (char *)lbuf , 4) != 4 )
    {
       /* if(  i_am_relay )
       {
	   syslog (LOG_ERR, "exread(): write failed\n");
       }
       else */

       if ( !i_am_relay && !i_am_aux )
       {
	   fprintf (stderr, "exread(): write failed\n");
       }
   }


    return(xread( fd, (unsigned char *)buf, count, timeout));
}




int is_blocking ( int tty )
{
    int flags;

    flags = fcntl(tty, F_GETFL);

    if ( flags & O_NONBLOCK )
       return 0;
 
    return 1;
}

int file_flags ( void ) 
{
   return fcntl(tty, F_GETFL);
}   

int setblocking ( int fd, int code )
{
   int flags;

   flags = fcntl(fd, F_GETFL);
   if ( code == 0 )
      flags |= O_NONBLOCK;
   else
      flags &= ~O_NONBLOCK;
   fcntl(fd, F_SETFL, flags);

   return flags;
}

void sxread_isr_1( int signo )
{
   return;
}


#if defined(HASSIGACT)
/*--------------------------------------------------------+
 |  An interruptable read() like xread(), but from a      |
 |  serial port file descriptor.  Needs sigaction()       |
 +--------------------------------------------------------*/
int sxread ( int fd, unsigned char *buffer, int size, int timeout )
{
   extern void cleanup_files();
   extern void cleanup_aux(void);
   int count = 0, nread = 0, passes = 0;
   struct sigaction act;

   if ( fd == TTY_DUMMY ) {
      sleep(timeout);
      return 0;
   }

   errno = 0;
   setblocking(fd, 1);

   sigaction(SIGALRM, NULL, &act);

   act.sa_handler = sxread_isr_1;
   act.sa_flags &= ~SA_RESTART;

   sigaction(SIGALRM, &act, NULL);

   alarm(timeout);
   while ( nread < size && passes++ < 100 ) {
      count = read(fd, (char *)(buffer + nread), size - nread);
      if ( count < 0 )
         break;
      nread += count;
   }
   alarm(0);

   act.sa_handler = SIG_IGN;
   act.sa_flags |= SA_RESTART;
   sigaction(SIGALRM, &act, NULL);

   if ( count >= 0 || (errno == EINTR) )
      return nread;
   
   syslog(LOG_ERR, "sxread() failed, error = %s\n", strerror(errno));

   if ( i_am_aux )
      cleanup_aux();

   cleanup_files();

   exit(1);
   
   return -1;
}


#elif defined(HASSIGINT)
/*--------------------------------------------------------+
 |  An interruptable read() like xread(), but from a      |
 |  serial port file descriptor.  Needs siginterrupt()    |
 +--------------------------------------------------------*/
int sxread ( int fd, unsigned char *buffer, int size, int timeout )
{
   extern void cleanup_files();
   extern void cleanup_aux(void);
   int count = 0, nread = 0, passes = 0;

   if ( fd == TTY_DUMMY ) {
      sleep(timeout);
      return 0;
   }

   errno = 0;
   setblocking(fd, 1);
   siginterrupt(SIGALRM, 1);
   signal(SIGALRM, sxread_isr_1);
   alarm(timeout);
   while ( size > nread && passes++ < 100 ) {
      count = read(fd, (char *)(buffer + nread), size - nread);
      if ( count < 0 )
         break;
      nread += count;
   }
   alarm(0);
   siginterrupt(SIGALRM, 0);

   if ( count >= 0 || (count < 0 && errno == EINTR) )
      return nread;
   
   syslog(LOG_ERR, "sxread() failed, error = %s\n", strerror(errno));

   if ( i_am_aux )
      cleanup_aux();

   cleanup_files();

   exit(1);
   
   return -1;
}


#elif defined(SIGKLUGE)

static int s_fd;
void sxread_isr_2( int signo ) 
{
   setblocking(s_fd, 0);
   return;
}

/*--------------------------------------------------------+
 |  An interruptable read() like xread(), but from a      |
 |  serial port file descriptor.  This one uses a kluge   |
 |  whereby the port is changed from blocking to non-     |
 |  blocking after the alarm timeout.                     |
 +--------------------------------------------------------*/
int sxread ( int fd, unsigned char *buffer, int size, int timeout )
{
   extern void cleanup_files();
   extern void cleanup_aux(void);
   int count = 0, nread = 0, passes = 0;

   if ( fd == TTY_DUMMY ) {
      sleep(timeout);
      return 0;
   }

   s_fd = fd;
   alarm(0);
   signal(SIGALRM, sxread_isr_2);
   alarm(timeout);
   while ( size > nread && passes++ < 100 ) {
      errno = 0;   
      setblocking(fd, 1);
      count = read(fd, (char *)(buffer + nread), size - nread);
      if ( count < 0 ) {
         if ( errno == EINTR || errno == EAGAIN ) {
            break;
         }
         else {
            continue;
         }
      }
      nread += count;
   }
   alarm(0);
   setblocking(fd, 1);

   if ( count >= 0 || (errno == EINTR || errno == EAGAIN) )
      return nread;

   syslog(LOG_ERR, "sxread() failed, error = %s\n", strerror(errno));

   if ( i_am_aux )
      cleanup_aux();

   cleanup_files();

   exit(1);
   
   return -1;
}

#else
/*--------------------------------------------------------+
 |  This version of sxread polls until the characters are |
 |  read or timeout.  Timeout is not very accurate - can  |
 |  take twice as long as specified with older kernels.   |
 +--------------------------------------------------------*/
int sxread ( int fd, unsigned char *buffer, int size, int timeout )
{
   extern void cleanup_files();
   extern void cleanup_aux();
   int count = 0, nread = 0, passes = 0;

   if ( fd == TTY_DUMMY ) {
      sleep(timeout);
      return 0;
   }

   setblocking(fd, 0);
   passes = 100 * timeout;
   while ( size > nread && --passes > 0 ) {
      errno = 0;
      count = read(fd, (char *)(buffer + nread), size - nread);
      if ( count <= 0 ) {
         millisleep(10);
         continue;
      }
      nread += count;
   }
   setblocking(fd, 1);

   if ( count >= 0 || errno == EAGAIN )
      return nread;

   syslog(LOG_ERR, "sxread() failed, error = %s\n", strerror(errno));

   if ( i_am_aux )
      cleanup_aux();

   cleanup_files();

   exit(1);

   return -1;
}

#endif


#if 0
int timeout_flag = 0;

void xread_sigtimer ( int signo )
{
   timeout_flag = 1;
   errno = EINTR;
   return;
}

/* 
 * An experimental version of xread() 
 */
int xread ( int fd, unsigned char *buffer, int count, int timeout )
{
   int  nread, ntot = 0;

   if ( fd == TTY_DUMMY ) {
      sleep(timeout);
      return 0;
   }

   if ( verbose && (! i_am_relay) ) {
      fprintf(stderr, "xread() called, count=%d, timeout = %d\n", count, timeout);
      fflush(stderr);
   }

   timeout_flag = 0;

   (void)alarm(0);
   (void) signal(SIGALRM, xread_sigtimer);
#ifdef HAS_ITIMER
   icurrent.it_interval.tv_sec = 0;
   icurrent.it_interval.tv_usec = 0;
   icurrent.it_value.tv_sec = timeout;
   icurrent.it_value.tv_usec = 30;
   if ( setitimer(ITIMER_REAL, &icurrent, &iold) == -1 ) {
      if( !i_am_relay )
          perror ("setitimer");
   }
#else
    (void) alarm((unsigned) timeout);
#endif

   errno = 0;
   while ( ntot < count && timeout_flag == 0 ) {
      nread = read(fd, (char *)(buffer + ntot), count - ntot);
      if ( nread < 0 )  {
         syslog(LOG_ERR, "xread: read() error %s\n", strerror(errno));
         if ( verbose && !i_am_relay ) {
	    fprintf(stderr, "xread() returning %d bytes after error\n", ntot);
            fflush(stderr);
	 }
         alarm(0);
         signal(SIGALRM, SIG_IGN);
         return ntot;
      }
      ntot += nread;
      
      if ( nread == 0 )
         millisleep(10);
   }

   if ( verbose && !i_am_relay ) {
      if ( ntot == 0 )
         fprintf(stderr, "xread() returning 0 bytes\n");
      else
         fprintf(stderr, "xread() returning %d byte(s). The first is 0x%02x\n", ntot, buffer[0]);
      fflush(stderr);
   }
   alarm(0);
   signal(SIGALRM, SIG_IGN);
	
   return ntot;
}
#endif


#if 0
/*
 * This is an alternate experimental version of xread which does not use
 * a signal for timeout.
 */
int xread ( int fd, unsigned char *buffer, int count, int timeout )
{
   int  nread, ntot = 0;
   long maxpasses, passes = 0;
   time_t tquit;

   if ( fd == TTY_DUMMY ) {
      sleep(timeout);
      return 0;
   }

   if ( verbose && (! i_am_relay) ) {
      fprintf(stderr, "xread() called, count=%d, timeout = %d\n", count, timeout);
      fflush(stderr);
   }

   maxpasses = timeout * 100;                     /* at nominal 10 msec/pass */

   /* System clock is checked since the timing resolution with older kernels */
   /* is such that the timout interval could otherwise be twice as long as   */
   /* specified */

   tquit = time(NULL) + (time_t)(timeout + 1);

   errno = 0;
   while ( ntot < count && passes < maxpasses && time(NULL) < tquit ) {
      nread = read(fd, (char *)(buffer + ntot), count - ntot);
      if ( nread < 0 )  {
         syslog(LOG_ERR, "xread: read() error %s\n", strerror(errno));
         if ( verbose ) {
	    fprintf(stderr, "xread() returning %d bytes after error\n", ntot);
            fflush(stderr);
	 }
         return ntot;
      }
      ntot += nread;
      passes++;
      if ( nread == 0 )
         millisleep(10);
   }

   if ( verbose && (i_am_relay != 1) ) {
      if ( ntot == 0 )
         fprintf(stderr, "xread() returning 0 bytes\n");
      else
         fprintf(stderr, "xread() returning %d byte(s). The first is 0x%02x\n", ntot, buffer[0]);
      fflush(stderr);
   }
	
   return ntot;
}

#endif











