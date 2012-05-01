/*
 * Changes for use with the CM11A are
 * Copyright 1996, 1997, 1998, 1999 by Daniel B. Suthers,
 * Pleasanton Ca. 94588 USA
 * E-MAIL dbs@tanj.com
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

#ifdef        SCO
#define       _IBCS2
#endif


#include <stdio.h>
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
/* msf - added for glibc/rh 5.0 */
#ifdef HAVE_PTY_H
#include <pty.h>
#endif

#ifdef HAVE_FEATURE_RFXLAN
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#endif

#include "x10.h"
#include "process.h"

#ifdef pid_t
#define PID_T pid_t
#else
#define PID_T long
#endif

void exit();
char *make_lock_name();
PID_T lockpid(char *);

extern int verbose, i_am_relay;
extern void error();
extern CONFIG config;
extern CONFIG *configp;
extern int sptty;

int ttylock();
int lock_device();

int tty_aux = -1;		/* Real tty */


#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif

#if !defined(HAVE_STRUCT_TERMIOS) && !defined(HAVE_STRUCT_TERMIO)
# include <sgtty.h>
struct sgttyb oldsb, newsb;
void hangup();
#else
# ifndef HAVE_STRUCT_TERMIOS
#  ifdef HAVE_TERMIO_H
#   include <termio.h>
#  endif
#  ifdef HAVE_SYS_TERMIO_H
#   include <sys/termio.h>
#  endif
struct termio oldsb, newsb;
# else
#  ifdef HAVE_TERMIOS_H
#   include <termios.h>
#  endif
#  ifdef HAVE_SYS_TERMIOS_H
#   include <sys/termios.h>
#  endif
struct termios oldsb, newsb;
# endif
# ifndef NCC
#  define NCC NCCS
# endif
#endif

/*---------------------------------------------------------------+
 | Aux port baud rate.                                           |
 +---------------------------------------------------------------*/
int aux_baudrate ( void )
{
    return (configp->auxdev == DEV_W800RF32)    ? B4800  :
           (configp->auxdev == DEV_MR26A)       ? B9600  :
           (configp->rfxcom_hibaud == YES)      ? B38400 : B4800;
}

/*---------------------------------------------------------------+
 | Alternate baud rate for RFXCOM receiver to that specified     |
 | in the config file.  I.e., if RFXCOM_HIBAUD YES is specified  |
 | in the config file, return the baud rate for NO, and vice-    |
 | versa.                                                        |
 +---------------------------------------------------------------*/
int rfxcom_alt_baudrate ( void )
{
   return  (configp->rfxcom_hibaud == YES) ? B4800 : B38400;
}


int setup_tty_aux( int auxbaud, int lockflag )
{
    int flags;

    if ( !configp->ttyaux[0] ) {
       syslog(LOG_ERR, "No TTY_AUX specified in config file");
       return 1;
    }

    if ( lockflag ) {
	if ( ttylock(configp->ttyaux) < 0 ) {
	    syslog(LOG_ERR, "Other process is using the tty_aux port.");
            return 1;
	}
    }


#ifdef DEBUG
    syslog(LOG_ERR, "Opening tty_aux line.");
#endif

#ifdef HAVE_FEATURE_RFXLAN
#if defined(HAVE_NETDB_H) && defined(HAVE_MEMSET) && defined(HAVE_SOCKET)
    if ( configp->auxhost[0] && configp->auxport[0] ) {
       struct addrinfo hints;
       struct addrinfo *result, *rp;
       int sfd, s;

       memset(&hints, 0, sizeof(struct addrinfo));
       hints.ai_family = AF_UNSPEC;	/* Allow IPv4 or IPv6 */
       hints.ai_socktype = SOCK_STREAM; /* Stream socket */
       hints.ai_flags = 0;
       hints.ai_protocol = 0;		/* Any protocol */

       /* Obtain address(es) matching host/port */

       s = getaddrinfo(configp->auxhost, configp->auxport, &hints, &result);
       if (s != 0) {
          syslog(LOG_ERR, "getaddrinfo: %s\n", gai_strerror(s));
       } else {
          /*
	   * getaddrinfo() returns a list of address structures.
	   * Try each address until we successfully connect(2).
	   * If socket(2) (or connect(2)) fails, we (close the socket
	   * and) try the next address.
	   */
          for (rp = result; rp != NULL; rp = rp->ai_next) {
	     sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
	     if (sfd == -1)
	        continue;

             if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1)
	        break;			/* Success */

	     close(sfd);
          }

          freeaddrinfo(result);		/* No longer needed */

          if (rp == NULL) {		/* No address succeeded */
	     syslog(LOG_ERR, "Could not connect\n");
          } else {
             tty_aux = sfd;
             return 0;
	  }
       }
    }
#else
#error "No system support required for RFXLAN feature, please disable."
#endif
#endif

#ifdef O_NONBLOCK
    /* Open with non-blocking I/O, we'll fix after we set CLOCAL */
    tty_aux = open(configp->ttyaux, O_RDWR|O_NONBLOCK);
#else
    tty_aux = open(configp->ttyaux, O_RDWR);
#endif

    if ( tty_aux < 0 ) {
        syslog(LOG_ERR, "Can't open tty_aux line.");
        return 1;
    }
#ifdef DEBUG
    else {
	syslog(LOG_ERR, "tty_aux line opened.");
    }
#endif


#if !defined(HAVE_STRUCT_TERMIOS) && !defined(HAVE_STRUCT_TERMIO)
    /* Old-style BSD/v7 sgtty calls */
    (void) ioctl(tty_aux, TIOCFLUSH, (struct sgttyb *) NULL);
    (void) ioctl(tty_aux, TIOCGETP, &oldsb);
    newsb = oldsb;
    newsb.sg_flags |= RAW;
    newsb.sg_flags &= ~(ECHO | EVENP | ODDP);
    hangup();
    newsb.sg_ispeed = newsb.sg_ospeed = auxbaud;	/* raise DTR & set speed */
    (void) ioctl(tty_aux, TIOCSETN, &newsb);
#elif !defined(HAVE_STRUCT_TERMIOS)
    /* SVr2-style termio */
    if (ioctl(tty_aux, TCGETA, &oldsb) < 0) {
    	syslog(LOG_ERR,"ioctl get");
	exit(1);
    }
    newsb = oldsb;
    newsb.c_lflag = 0;
    newsb.c_oflag = 0;
    newsb.c_iflag = IGNBRK | IGNPAR;
    newsb.c_cflag = (CLOCAL | auxbaud | CS8 | CREAD);
    newsb.c_cc[VMIN] = 1;
    newsb.c_cc[VTIME] = 0;
    newsb.c_cc[VINTR] = 0;
    newsb.c_cc[VQUIT] = 0;
#ifdef VSWTCH
    newsb.c_cc[VSWTCH] = 0;
#endif
    newsb.c_cc[VTIME] = 0;
    if (ioctl(tty_aux, TCSETAF, &newsb) < 0) {
    	syslog(LOG_ERR,"aux ioctl set");
	exit(1);
    }
#else
    {
	/* POSIX-style termios */
	int s;
	s = tcgetattr(tty_aux, &oldsb);
	if (s < 0) {
		syslog(LOG_ERR,"ttopen tcgetattr");
		exit(1);
	}

	newsb = oldsb;

	/* newsb.c_iflag = BRKINT|(oldsb.c_iflag & (IXON|IXANY|IXOFF)); */
	newsb.c_iflag = IGNPAR;
	newsb.c_oflag = 0;
	newsb.c_lflag = 0;
	newsb.c_cflag = (CLOCAL | CS8 | CREAD);
	for (s = 0; s < NCC; s++)
		newsb.c_cc[s] = 0;
	newsb.c_cc[VMIN] = 1;
	newsb.c_cc[VTIME] = 0;
#ifdef BEFORE
#ifdef	VSWTCH
	newsb.c_cc[VSWTCH] = 0;
#endif
	newsb.c_cc[VSUSP] = 0;
	newsb.c_cc[VSTART] = 0;
	newsb.c_cc[VSTOP] = 0;
#endif

	/* POSIX sets speed separately */
	cfsetispeed(&newsb, auxbaud);
	cfsetospeed(&newsb, auxbaud);
	tcsetattr(tty_aux, TCSADRAIN, &newsb);
    }
#endif

#ifdef O_NONBLOCK
    /* Now that we have set CLOCAL on the port, we can use blocking I/O */
    flags = fcntl(tty_aux, F_GETFL);
    fcntl(tty_aux, F_SETFL, flags & ~O_NONBLOCK);
#endif

    return(0);
}

void restore_tty_aux()
{
#if !defined(HAVE_STRUCT_TERMIOS) && !defined(HAVE_STRUCT_TERMIO)
    hangup();
    (void) ioctl(tty_aux, TIOCSETN, &oldsb);
#elif !defined(HAVE_STRUCT_TERMIOS)
    (void) ioctl(tty_aux, TCSETAF, &oldsb);
#else
    tcsetattr(tty_aux, TCSADRAIN, &oldsb);
#endif
}

#if !defined(HAVE_STRUCT_TERMIOS) && !defined(HAVE_STRUCT_TERMIO)
void hangup_aux()
{
    newsb.sg_ispeed = newsb.sg_ospeed = B0;	/* drop DTR */
    (void) ioctl(tty_aux, TIOCSETN, &newsb);
    sleep(SMALLPAUSE);
}

#endif

void quit_aux()
{
    if (tty_aux == -1)
	exit(1);
    restore_tty_aux();
    exit(1);
}


/* set up the real and spool psuedo tty file descriptor.
 */
int setup_sp_tty_aux()
{

    /* extern char spoolfile[PATH_MAX]; */
    extern char spoolfile[PATH_LEN + 1];

    if ( tty_aux < 0 ) setup_tty_aux(aux_baudrate(), 0);
    if ( tty_aux < 0 ) {
	error("Can't open tty_aux line");
	syslog(LOG_DAEMON | LOG_ERR, "Can't open tty_aux line\n");
    }

    sptty = open(spoolfile, O_RDWR|O_APPEND,0x644);/* open the spool file */
    if ( sptty < 0 ) {
	char tmpbuf[sizeof(spoolfile) + 100];
	perror("Can't open spool file");
	sprintf(tmpbuf, "Can't open spool file %s", spoolfile);
	error(tmpbuf);
	strcat(tmpbuf, "\n");
	syslog(LOG_DAEMON | LOG_ERR, "Can't open spool file %s", spoolfile);

    }
    lseek(sptty, 0, SEEK_END);			/* seek to end of file */

    return(0);
}

int config_tty_dtr_rts ( int tty, unsigned char bits )
{
   int status, retcode = 0;

   if ( (retcode = ioctl(tty, TIOCMGET, &status)) < 0 )
      return retcode;

   status &= ~(TIOCM_DTR | TIOCM_RTS);
   if ( bits & RAISE_DTR )  status |= TIOCM_DTR;
   if ( bits & RAISE_RTS )  status |= TIOCM_RTS;

   retcode |= ioctl(tty, TIOCMSET, &status);

   return retcode;
}
