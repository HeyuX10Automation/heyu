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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <ctype.h>

#ifdef        SCO
#define _SVID3 /* required for correct termio handling */
#undef  _IBCS2 /* conflicts with SVID3  */
#endif

#include <time.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

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

#ifdef HAVE_STRING_H
#include <string.h>
#endif

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



