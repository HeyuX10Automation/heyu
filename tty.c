/*
 * Changes for use with the CM11A are
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

/*
 * Copyright 1986 by Larry Campbell, 73 Concord Street, Maynard MA 01754 USA
 * (maynard!campbell).  You may freely copy, use, and distribute this software
 * subject to the following restrictions:
 *
 *  1)	You may not charge money for it.
 *  2)	You may not remove or alter this copyright notice.
 *  3)	You may not claim you wrote it.
 *  4)	If you make improvements (or other changes), you are requested
 *	to send them to me, so there's a focal point for distributing
 *	improved versions.
 *
 * John Chmielewski (tesla!jlc until 9/1/86, then rogue!jlc) assisted
 * by doing the System V port and adding some nice features.  Thanks!
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

#include "x10.h"
#include "process.h"

#ifdef pid_t
#define PID_T pid_t
#else
#define PID_T long
#endif

extern CONFIG config;
extern CONFIG *configp;

void exit();
char *make_lock_name();
PID_T lockpid(char *);

extern int verbose, i_am_relay, i_am_aux;
extern void error();
extern CONFIG config;

/* static char dev_string[PATH_MAX]; */
static char dev_string[PATH_LEN + 1];

int ttylock();
int lock_device();

int tty = TTY_CLOSED;		/* Real tty */
int sptty = -1;	/* Spool */

#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif

#if !defined(HAVE_STRUCT_TERMIOS) && !defined(HAVE_STRUCT_TERMIO)
#  include <sgtty.h>
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

int setup_tty ( int lockflag )
{
    int flags;

    if ( !configp->tty[0] )
    	error("no TTY specified in configfile");

    if ( lockflag ) {
	if ( ttylock(configp->tty) < 0 ) {
	    syslog(LOG_ERR, "Other proccess is using the real tty port.");
	    error("Other process is using tty port.");
	}
    }

    if ( configp->device_type & DEV_DUMMY ) {
       tty = TTY_DUMMY;
       return 0;
    }

#ifdef DEBUG
    syslog(LOG_ERR, "Opening tty line.");
#endif

#ifdef O_NONBLOCK
    /* Open with non-blocking I/O, we'll fix after we set CLOCAL */
    tty = open(configp->tty, O_RDWR|O_NONBLOCK);
#else
    tty = open(configp->tty, O_RDWR);
#endif

    if ( tty < 0 ) {
        syslog(LOG_ERR, "Can't open tty line.");
	error("Can't open tty line.  Check the permissions.");
    }
#ifdef DEBUG
    else {
	syslog(LOG_ERR, "Tty line opened.");
    }
#endif


#if !defined(HAVE_STRUCT_TERMIOS) && !defined(HAVE_STRUCT_TERMIO)
    /* Old-style BSD/v7 sgtty calls */
    (void) ioctl(tty, TIOCFLUSH, (struct sgttyb *) NULL);
    (void) ioctl(tty, TIOCGETP, &oldsb);
    newsb = oldsb;
    newsb.sg_flags |= RAW;
    newsb.sg_flags &= ~(ECHO | EVENP | ODDP);
    hangup();
    newsb.sg_ispeed = newsb.sg_ospeed = B4800;	/* raise DTR & set speed */
    (void) ioctl(tty, TIOCSETN, &newsb);
#elif !defined(HAVE_STRUCT_TERMIOS)
    /* SVr2-style termio */
    if ( ioctl(tty, TCGETA, &oldsb) < 0 ) {
    	syslog(LOG_ERR,"ioctl get");
	exit(1);
    }
    newsb = oldsb;
    newsb.c_lflag = 0;
    newsb.c_oflag = 0;
    newsb.c_iflag = IGNBRK | IGNPAR;
    newsb.c_cflag = (CLOCAL | B4800 | CS8 | CREAD);
    newsb.c_cc[VMIN] = 1;
    newsb.c_cc[VTIME] = 0;
    newsb.c_cc[VINTR] = 0;
    newsb.c_cc[VQUIT] = 0;
#ifdef VSWTCH
    newsb.c_cc[VSWTCH] = 0;
#endif
    newsb.c_cc[VTIME] = 0;
    if (ioctl(tty, TCSETAF, &newsb) < 0) {
    	syslog(LOG_ERR,"ioctl set");
	exit(1);
    }
#else
    {
	/* POSIX-style termios */
	int s;
	s = tcgetattr(tty, &oldsb);
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

	/* POSIX sets speed seperately */
	cfsetispeed(&newsb, B4800);
	cfsetospeed(&newsb, B4800);
	tcsetattr(tty, TCSADRAIN, &newsb);
    }
#endif

#ifdef O_NONBLOCK
    /* Now that we have set CLOCAL on the port, we can use blocking I/O */
    flags = fcntl(tty, F_GETFL);
    fcntl(tty, F_SETFL, flags & ~O_NONBLOCK);
#endif

    return(0);
}

void restore_tty()
{
   if ( tty == TTY_DUMMY )
      return;

#if !defined(HAVE_STRUCT_TERMIOS) && !defined(HAVE_STRUCT_TERMIO)
    hangup();
    (void) ioctl(tty, TIOCSETN, &oldsb);
#elif !defined(HAVE_STRUCT_TERMIOS)
    (void) ioctl(tty, TCSETAF, &oldsb);
#else
    tcsetattr(tty, TCSADRAIN, &oldsb);
#endif
}

#if !defined(HAVE_STRUCT_TERMIOS) && !defined(HAVE_STRUCT_TERMIO)
void hangup()
{
    newsb.sg_ispeed = newsb.sg_ospeed = B0;	/* drop DTR */
    (void) ioctl(tty, TIOCSETN, &newsb);
    sleep(SMALLPAUSE);
}

#endif

void quit()
{
    if ( tty < 0 )
	exit(1);
    restore_tty();
    exit(1);
}

/* ttylock locks the tty device in a UUCP compatible style.
 * If the process is not valid, it's ok to remove the lock
 * The ttydev arguement should be the device (tty2) or a fully qualified
 * path name (/dev/tty2).
 * It's OK to sleep a moment when we get see a valid lock, just in case it
 * will go away quickly
 *
 */

int ttylock ( char *ttydev )
{
    char *devstr;
    int rtn;

    rtn = -1;
    devstr = make_lock_name(ttydev);

    if( verbose )
        printf("Trying to lock (%s)\n", devstr);
    if( lockpid(ttydev) == 0 )
	rtn = lock_device(devstr);
    if( (verbose) && ( rtn == 0 ) )
            printf("%s is locked\n", devstr);
    else if(  rtn != 0  )
    	    printf("Unable to lock %s\n", devstr);

    return(rtn);
}


/* munlock should be called when it's time to close the TTY.
 * The ttydev param should be a fully qualified pathname for the lock file
 */
int munlock ( char *ttydev )
{
    char *devstr;

    if ( lockpid(ttydev) == (PID_T)getpid() ) {
	devstr = make_lock_name(ttydev);
        if ( verbose && !i_am_relay && !i_am_aux )
	   fprintf(stderr, "munlock: Unlink file '%s'\n", devstr);
	return( unlink(devstr) );
    }
    return(0);
}

/* This function writes the pid into the lock file.  It uses the ASCII mode.
 * It will overwrite the existing file.
 */
int lock_device ( char *ttydev )
{
    FILE *f;
    char err_string[128];

    if ( (f = fopen(ttydev, "w")) != NULL ) {
        fprintf(f, "    %d\n", (int)getpid() );
#ifdef REVERT_PERMS
	chmod(ttydev, 0777);
#endif
    }
    else {
	if ( verbose )
	    syslog(LOG_ERR,"Unable to create the lock file:");
	syslog(LOG_DAEMON | LOG_ERR, "Unable to create the lock file.");
        /* error quits the program */
        sprintf(err_string, "Unable to create the lock file %s.\n", ttydev);
        error(err_string);
    }
    fclose(f);
    return(0);
}

/* set up the real and spool psuedo tty file descriptor.
 */
int setup_sp_tty( void )
{
    /* extern char spoolfile[PATH_MAX]; */
    extern char spoolfile[PATH_LEN + 1];

    if ( tty == TTY_CLOSED )
        setup_tty(0);
    if ( tty == TTY_CLOSED ) {
	error("Can't open tty line");
	syslog(LOG_DAEMON | LOG_ERR, "Can't open tty line.");
    }

#ifdef REVERT_PERMS
    sptty = open(spoolfile, O_RDWR|O_APPEND,0x644);/* open the spool file */
#else
    sptty = open(spoolfile, O_RDWR|O_APPEND);/* open the spool file */
#endif
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

/* concatenate the lock directory path with the name to be locked. 
 * Start with a device or other string.  Example: /dev/tty2
 * End result is a string such as "/usr/spool/uucp/LCK..tty2"
 */
char *make_lock_name ( char *ttydev )
{
    char        *devstr;
    int         x, ngrps;
#ifdef GETGROUPS_T
    GETGROUPS_T grps[30];
#else
    gid_t       grps[30];
#endif
    char        *ptr;
    struct stat stat_buf;

    /* strip the leading path name */
    ptr = rindex(ttydev, '/');

    devstr = dev_string;
    
    if ( ptr == (char *)NULL ) {
        ptr = ttydev;
    }
    else {
        ptr++; 		/* move past the slash */
    }

    /* Check to see that the Lock Directory is valid */
    
    strncpy2(dev_string, LOCKDIR, sizeof(dev_string) - 1);
    
    if (dev_string[strlen(dev_string) - 1] != '/') {
        strncat(dev_string, "/", sizeof(dev_string) - 1 - strlen(dev_string));
    }

    /* check that the lock directory exists */
    if ( (stat(dev_string, &stat_buf) != 0 ) || ((stat_buf.st_mode & S_IFDIR) == 0) ) {
	syslog(LOG_DAEMON | LOG_ERR, "The lock directory %s does not exist.", LOCKDIR);
	fprintf(stderr, "The lock directory %s does not exist.\n", LOCKDIR);
	quit();
    }
    
    /* check that we can write to the lock directory. Either u+w, g+w  or writable 
       by other and different group*/
    
    if ( !((stat_buf.st_uid == geteuid()) && ((stat_buf.st_mode & S_IWUSR)!=0) ) ) {
        ngrps = 30;
	if ( (stat_buf.st_mode & S_IWGRP) != 0) {
	    ngrps = getgroups(30, grps);
	    for ( x = 0; x < ngrps; x++ ) {
		if ( stat_buf.st_gid == grps[x] ) {
		   break;
		}
	    }
	}
	else {
           x = 30;                /* magic number */
	}
	
	if( (x == 30 || x == ngrps) && (stat_buf.st_mode & S_IWOTH) )
	    x--;
	
	if( x == 30 || x == ngrps ) {
	    syslog(LOG_DAEMON | LOG_ERR, "The lock directory %s is not writable.", LOCKDIR);
	    fprintf(stderr, "The lock directory %s is not writable.\n", LOCKDIR);
	    quit();
        }
    }
    strncat(dev_string, "LCK..", sizeof(dev_string) - 1 - strlen(dev_string));
    strncat(dev_string, ptr, sizeof(dev_string) - 1 - strlen(dev_string));

    return(devstr);
}



/* lockpid returns 0 if the file is not locked or PID is invalid,
 *         returns the PID if the file is locked
 *         returns -1 on error
 */
PID_T lockpid ( char *ttydev )
{
    FILE *input;
    /* char bufr[PATH_MAX]; */
    char bufr[PATH_LEN + 1];
    char *devstr;
    struct stat lockstat;
    int infd;
    long pid_no;

    char *ignorep;
    int  ignoret;

    devstr = make_lock_name(ttydev);

    if ( verbose && !i_am_relay )
       fprintf(stderr, "lockpid: Checking for file '%s'\n", devstr);

    errno = 0;
    if ( stat(devstr, &lockstat) >= 0 ) {
        /* a lock file exists */
	input = fopen(devstr, "r");
	if( input == NULL ) {
	   syslog(LOG_DAEMON | LOG_ERR, "Problem opening the lock file.");
           if ( !i_am_relay )
	      fprintf(stderr, "Problem opening the lock file.\n");
	   quit();
	}
	ignorep = fgets(bufr,80,input);	/* read the pid info from the file */
	if ( strncmp("    ", bufr, 4) == 0 ) {
	    /* Oh! ascii info */
	    sscanf(bufr, " %ld", &pid_no);
	    fclose(input);
	}
	else {
           /* Ahhh.  Binary */
	   fclose(input);
	   infd = open(devstr, O_RDONLY);
	   if ( infd < 0  ) {
	       syslog(LOG_DAEMON | LOG_ERR, "Problem opening the lock file.");
               if ( !i_am_relay )
                  fprintf(stderr, "Problem opening the lock file.\n");
	       quit();
	   }
	   ignoret = read(infd,&pid_no,4);
	   close(infd);
	}
	/* does the process exist? */
        
        errno = 0;
        getpriority(PRIO_PROCESS, (PID_T)pid_no);	/* a harmless check for a pid */
        if ( errno == ESRCH )
            return(0);			/* no pid exists */
        else {
	    /*  locked by other process.  Please try again. */
	    return((PID_T)pid_no);
        }
    }
    else {
	/* could not stat the file.  Why? */
        if ( errno == ENOENT ) {
            /* Cool!  no lock file. */
            return(0);
        }
	else {
	    if ( !i_am_relay ) {
	       fprintf(stderr, "stat pathspec '%s': Error = %s\n",
		  devstr, strerror(errno));
	    }  
	    perror("Lock file not accessable:");
	    syslog(LOG_DAEMON | LOG_ERR, "Lock file not accessable.");
	    quit(); 
	}
    }

    return(0);
}

int lock_for_write()
{
    int max;
    char writefilename[PATH_LEN + 1];

    sprintf(writefilename, "%s%s", WRITEFILE, configp->suffix);
    max = 0;
    while( (lockpid(writefilename) > 1) && (++max <= configp->lock_timeout) )
         sleep(1);

    if ( ttylock(writefilename) < 0 ) {
        if ( !i_am_relay && !i_am_aux )
           fprintf(stderr, "Could not set up the heyu.write lock\n");
        syslog(LOG_ERR, "Could not set up the heyu.write lock-");
        return(-1);
    }

    return(0);
}

/*-----------------------------------------------------+
 | Return 1 if serial RI line is asserted, 0 if not,   |
 | or -1 if modem control lines are unsupported by the |
 | serial port hardware.                               |
 +-----------------------------------------------------*/
int is_ring ( void )
{
   int lines = 0;
   if ( tty == TTY_DUMMY )
      return -1;
   if ( ioctl(tty, TIOCMGET, &lines) < 0 )
      return -1;
   if ( lines & TIOCM_RI )
      return 1;
   return 0;
}

/*----------------------------------------------------+
 | Return 1 if modem control lines are supported by   |
 | the serial port hardware, otherwise 0.             |
 +----------------------------------------------------*/
int is_modem_support ( void )
{
   return ( is_ring() >= 0 ) ? 1 : 0;
}

/*----------------------------------------------------+
 | Display state of modem control lines.              |
 +----------------------------------------------------*/
int c_show_modem_lines ( int argc, char *argv[] )
{
  int  lines = 0;
  char *set = "SET";
  char *clr = "CLR";

  if ( ioctl(tty, TIOCMGET, &lines) < 0 ) {
     fprintf(stderr, "Modem control not supported.\n");
     return 1;
  }
   
  printf("RI  <  %s\n", ((lines & TIOCM_RI ) ? set : clr));
  printf("DCD <  %s\n", ((lines & TIOCM_CD ) ? set : clr));
  printf("CTS <  %s\n", ((lines & TIOCM_CTS) ? set : clr));
  printf("DSR <  %s\n", ((lines & TIOCM_DSR) ? set : clr));
  printf("RTS >  %s\n", ((lines & TIOCM_RTS) ? set : clr));
  printf("DTR >  %s\n", ((lines & TIOCM_DTR) ? set : clr));
 
  return 0;
}


/*----------------------------------------------------+
 | Verify functioning of serial port status lines RI, |
 | CD, DSR, CTS.  Jumper required between DTR (DB-9,  |
 | pin 4) and input line(s) to be tested:             |
 |   RI  - pin 9                                      |
 |   CD  - pin 1                                      |
 |   DSR - pin 6                                      |
 |   CTS - pin 8                                      |
 | If functional, the pin being tested will agree     |
 | with the DTR pin as either SET or clr.             |
 +----------------------------------------------------*/
int c_port_line_test ( int argc, char *argv[] )
{

   int j, k, drive;
   int status, saved;

   static struct {
     char *name;
     int  pin;
     int  tiocm;
   } inpin[4] = {
     { "RI ", 9, TIOCM_RI },
     { "CD ", 1, TIOCM_CD },
     { "DSR", 6, TIOCM_DSR },
     { "CTS", 8, TIOCM_CTS },
   }, outpin[2] = {
     { "DTR", 4, TIOCM_DTR },
     { "RTS", 7, TIOCM_RTS },
   };

   enum {DTR, RTS};

   drive = DTR;

   if ( ioctl(tty, TIOCMGET, &status) < 0 ) {
      fprintf(stderr,
	 "Status lines are not supported by the serial port hardware.\n");
      return 1;
   }

   saved = status;

   printf("Jumpered pin %2d   to", outpin[drive].pin);
   for ( k = 0; k < 4; k++ ) 
      printf("   %2d", inpin[k].pin);
   printf("\n");

   printf("Status Line: %-3s  => ", outpin[drive].name);
   for ( k = 0; k < 4; k++ )
      printf("  %-3s", inpin[k].name);
   printf("\n             ---       ---  ---  ---  ---\n");


   for ( j = 0; j < 2; j++ ) {
      status &= ~(outpin[drive].tiocm);
      ioctl(tty, TIOCMSET, &status);
      millisleep(100);

      ioctl(tty, TIOCMGET, &status);

      printf("             %3s  =>  ",
	 ((status & outpin[drive].tiocm) ? "SET" : "clr"));
      for ( k = 0; k < 4; k++ ) {
         printf(" %3s ",
            ((status & inpin[k].tiocm) ? "SET" : "clr"));
      }
      printf("\n");

      status |= outpin[drive].tiocm;
      ioctl(tty, TIOCMSET, &status);
      millisleep(100);

      ioctl(tty, TIOCMGET, &status);

      printf("             %3s  =>  ",
	 ((status & outpin[drive].tiocm) ? "SET" : "clr"));
      for ( k = 0; k < 4; k++ ) {
         printf(" %3s ",
            ((status & inpin[k].tiocm) ? "SET" : "clr"));
      }
      printf("\n");
   }
   printf("\n");

   ioctl(tty, TIOCMSET, &saved);
   
   return 0;
}

int port_health_check ( int tty )
{
   int status;

   if ( configp->check_RI_line == NO )
      return 0;

   if ( ioctl(tty, TIOCMGET, &status) < 0 ) {
      return -1;
   }

   return 1;
}


int rts_pulser ( int msec_on, int msec_off, int repeats )
{
   int j, status;

   if ( ioctl(tty, TIOCMGET, &status) < 0 ) {
      fprintf(stderr,
         "Status lines are not supported by the serial port hardware.\n");
      return 1;
   }
   if ( status & TIOCM_RTS ) {
      status &= ~TIOCM_RTS;
      ioctl(tty, TIOCMSET, &status);
      millisleep(500);
   }

   for ( j = 0; j < repeats; j++ ) {
      ioctl(tty, TIOCMGET, &status);
      status |= TIOCM_RTS;
      ioctl(tty, TIOCMSET, &status);
      millisleep(msec_on);

      ioctl(tty, TIOCMGET, &status);
      status &= ~TIOCM_RTS;
      ioctl(tty, TIOCMSET, &status);
      millisleep(msec_off);
   }
   return 0;
}

int c_rts_pulser ( int argc, char *argv[] )
{
   int  msec_on, msec_off, repeats;
   char *sp;

   if ( argc != 5 ) {
      fprintf(stderr, "Usage: %s %s msec_on msec_off repeats.\n", argv[0], argv[1]);
      return 1;
   }

   if ( (msec_on = (int)strtol(argv[2], &sp, 10)) < 1 || !strchr(" \t\r\n", *sp) ) {
      fprintf(stderr, "Invalid msec_on time '%s'\n", argv[2]);
      return 1;
   }
   if ( (msec_off = (int)strtol(argv[3], &sp, 10)) < 1 || !strchr(" \t\r\n", *sp) ) {
      fprintf(stderr, "Invalid msec_of time '%s'\n", argv[3]);
      return 1;
   }
   if ( (repeats = (int)strtol(argv[4], &sp, 10)) < 0 || !strchr(" \t\r\n", *sp) ) {
      fprintf(stderr, "Invalid repeats '%s'\n", argv[4]);
      return 1;
   }

   return rts_pulser(msec_on, msec_off, repeats);
}

/*----------------------------------------------------+
 | Silently set initial values of DTR and RTS for     |
 | a serial port which supports status lines.         |
 +----------------------------------------------------*/

int port_status_init ( int tty, char dtr, char rts )
{
   int status;

   if ( ioctl(tty, TIOCMGET, &status) < 0 )
      return 0;

   if ( dtr == HIGH_STATE )
      status |= TIOCM_DTR;
   else
      status &= ~TIOCM_DTR;

   if ( rts == HIGH_STATE )
      status |= TIOCM_RTS;
   else
      status &= ~TIOCM_RTS;

   ioctl(tty, TIOCMSET, &status);

   return 0;
}

