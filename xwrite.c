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
#include <ctype.h>

#ifdef        SCO
#define _SVID3 /* required for correct termio handling */
#undef  _IBCS2 /* conflicts with SVID3  */
#endif

#include <time.h>
#include <unistd.h>

#ifdef LINUX
#include <asm/ioctls.h>
#   ifdef OLDLINUX
#include <linux/serial_reg.h>
#   endif
#include <linux/serial.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <syslog.h>
#else
#    if (defined(POSIX) || defined(FREEBSD) || defined(OPENBSD))
#include <sys/termios.h>
#    else
#         ifdef SCO
#include <sys/termio.h>
#         else
#              ifdef DARWIN
#include <termios.h>
#              else
#include <termio.h>
#              endif
#         endif
#    endif
#endif

#if (defined(OSF) || defined(DARWIN))
#include <sys/ioctl.h>
#endif

#include <string.h>

#include "x10.h"
#include "process.h"

extern int verbose;
extern int sptty;
extern int ttylock(), munlock();
extern int i_am_relay;


/* This process echoes all strings to the spool file as well as
 * sending them to the CM11A's tty port
 */
int xwrite( int tty, char *buf, int size)
{
    static unsigned char template[4] = {0xff, 0xff, 0xff, 0x00};
    unsigned char        outbuf[160];
    int                  max = 0;

    int ignoret;

    template[3] = size;
    memcpy(outbuf, template, 4);
    memcpy(outbuf + 4, buf, size);

    ignoret = write(sptty, (char *)outbuf, size + 4);

    /* Command to state engine is written to sptty only */
    if ( buf[0] == ST_COMMAND && size == 3 )
       return 3;
    
    if ( verbose && !i_am_relay )
       fprintf(stdout, "xwrite() called, count=%d\n", size );

    max = write( tty, buf, size);

    return(max);
}



