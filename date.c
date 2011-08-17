
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

 /* Changes for the CM11A made by Daniel Suthers, dbs@tanj.com */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <time.h>
#include "x10.h"
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef SYSV
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#endif

#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#ifdef BEFORE
#ifdef HAVE_SYS_FILSYS_H
#include <sys/filsys.h>
#endif
#endif
#include "x10.h"


extern void error();
extern int invalidate_for_cm10a();

extern int
 Idays, Ijday, Ihours, Iminutes, Iseconds;

/* ARGSUSED */

int c_date( int argc, char *argv[] )
{
    int  today;
    int  expire;
    struct tm *tp;
    char RCSID[]= "@(#) $Id: date.c,v 1.8 2000/01/02 23:00:32 dbs Exp dbs $\n";
    extern int usage(), get_status(), dowX2U();
    extern struct tm *cm11a_to_legal();
    time_t now;

    if ( invalidate_for_cm10a() != 0 )
       return 1;

    display(RCSID);
    if (argc != 2)
	usage(E_2MANY);
    time(&now);
    tp = localtime(&now);

    if( get_status() < 1 )
        error(" No reponse from CM11A.  Program exiting");

    /* Translate CM11a clock data to Legal Time via */
    /* info in X10 Record File (if it exists).      */
    tp = cm11a_to_legal( &Idays, &Ijday, &Ihours,
	      &Iminutes, &Iseconds, &expire );
    expire = expire;  /* Keep compiler from complaining */
    
    today = dowX2U(Idays);
#ifndef POSIX
    while (tp->tm_wday % 7 != today)
	tp->tm_wday++, tp->tm_mday++;
#endif

#ifdef VENIX
    (void) printf("%2d%02d%02d%02d%02d\n",
	     tp->tm_year % 100, tp->tm_mon + 1, tp->tm_mday, Ihours, Iminutes);
#else
    (void) printf("%02d%02d%02d%02d%02d\n",
	     tp->tm_mon + 1, tp->tm_mday, Ihours, Iminutes, tp->tm_year % 100);
#endif
    return(0);
}
