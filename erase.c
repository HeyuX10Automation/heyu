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
#include "x10.h"
#include <string.h>

extern int tty;
extern int sptty;
extern int verbose;
extern int invalidate_for_cm10a();


/*
 * The erase function will completely erase the EEPROM by calling eraseall.
 *
 * The qerase function erases the headers of the EEPROM in the CM11.
 * It forces the erase by ignoring the response of the CM11A.  This can get
 * you out of a lockup.
 *
 * It does so  by overwriting the EEPROM with no macros and no events
 * In essence, the following byte string should be sent
 * FB 00 00 00 02 ff ff ff ff ff ff ff ff ff ff ff ff ff ff
 * FB = load macros
 *    00 00 = address to load at
 *          00 02 = address 2 = start of macro initiators
 *                ff ff  end of macro initiators
 *                      ff ff ff ff ff ff ff ff ff ff ff ff   filler
 *
 * It's important to note that the eeprom macro data is NOT zeroed.  This
 * leaves a possibility that a new timer/macro will put a partial record
 * at the address of the old one.
 *
 */
int c_erase(argc, argv)
int argc;
char *argv[];
{
    char RCSID[]= "@(#) $Id: erase.c,v 1.9 1999/12/26 20:37:17 dbs Exp dbs $\n";
    int n;
    extern int eraseall();

   if ( invalidate_for_cm10a() != 0 )
      return 1;
   
    display(RCSID);

    fputs("Erasing all blocks of data on the eeprom\n", stdout);
    n=eraseall();
    fputc('\n', stdout);
    return(n);

}

/* This is the old erase routine.  It's been altered slightly to force its
 * way through a lockup.  It does not falter if the CM11A does not return
 * a valid checksum.
 * 
 * It does the commit even though the CM11 does not give any valid response.
 */
int c_qerase(argc, argv)
int argc;
char *argv[];

{
    register int n;
    int j, statusflag;
    int timeout;
    unsigned char sum;
    unsigned char buf[22];
    unsigned char checksum();
    extern int eraseall(), xwrite(), exread(), check4poll();
    extern void remove_record_file();
    extern int c_setclock();

    if ( invalidate_for_cm10a() != 0 )
       return 1;

    timeout=10;
    statusflag = 0;

    if( verbose)
       fputs("Erasing just the header block of data on the eeprom\n", stdout);

    for ( j = 0; j < 19; j++ )
       buf[j] = 0xff;

    buf[0] = 0xfb; /* I want to write data */
    buf[1] = 0x00; /* starting address to write (hi-lo) in this case Zero */
    buf[2] = 0x00;
                   /* data to store in eeprom starts here */
    buf[3] = 0x00; /* Start of the dummy macro init table (hi-lo) = 3 */
    buf[4] = 0x03;

#if 0
    		   /* start of timer initiator table */
    buf[5] = 0xff; /* flag to indicate end of timer init table */
    buf[6] = 0xff; /* start of bogus macro initiator table */
    buf[7] = 0xff; /* filler.  Content of data packets must be 16 bytes */
    buf[8] = 0xff;
    buf[9] = 0xff;
    buf[10] = 0xff;
    buf[11] = 0xff;
    buf[12] = 0xff;
    buf[13] = 0xff;
    buf[14] = 0xff;
    buf[15] = 0xff;
    buf[16] = 0xff;
    buf[17] = 0xff;
    buf[18] = 0xff;
    buf[19] = 0;
#endif

    if( verbose )
    {
	fprintf(stderr, "Sending erase string\n" ); 
    }
    (void) xwrite(tty, (char *) buf, 19);

    /* get a check sum in reply */
    /* The check sum does not include the 'fb'
     * It starts at the 2nd byte (buf[1])
     */
    sum=checksum(buf+1,18) ;

    if( verbose )
	fprintf( stdout, "Expected checksum = %0x\n", sum);
    n = exread(sptty, buf, 1, timeout);
    if( verbose )
	fprintf(stdout, "CM11 reports a checksum of %0x\n", buf[0]);

    if ( verbose && sum != buf[0] )
	fprintf(stderr, "Failure sending command header\n");

    /* Write the 00 regardless.  The CM11 may be locked up. */
    (void) xwrite(tty, "\00" , 1);	/* WRMI (we really mean it) */

#if 0
    /* Write the 00 regardless.  The CM11 may be locked up. */
    /* if( sum == buf[0])  */ /* old code */
    if( 1 == 1 )
    {
	(void) xwrite(tty, "\00" , 1);	/* WRMI (we really mean it) */
    }
    else
    {
	fprintf(stderr, "Failure sending command header\n");
	return(-1);
    }
#endif

    buf[0] = 0;
    n = exread(sptty, buf, 1, timeout);
    if( n == 1 )
        if(buf[0] != 0x55 )
        {
            fprintf(stderr, "Ack after execution = %0x, It should be 0x55)\n",
                    buf[0]);
            n = 0;
        }
    if(n != 1)
    {
        fprintf(stderr,
                "Interface not ready after excuting function (buf= %0x)\n",
                buf[0]);
        fprintf(stderr, "N = %0x)\n", n);
        return(-1);
    }


    if(statusflag == 1)
    {
	for( n = 0; n < 2; n++)
	    check4poll(1,1);
    }
    else
	check4poll(0,0);

    /* 
     * Delete the X10 Record File and reset the cm11a clock.
     * (In the absence of record file, the clock is set to  
     * system standard time.)
     *
     */
    
    remove_record_file();
    (void) c_setclock(1, NULL); 

    if( verbose )
	fprintf(stdout, "All ok with Erase.\n");
    return(0);
}
