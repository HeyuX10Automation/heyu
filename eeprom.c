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
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "eeprom.h"
#include "process.h"

extern int tty;
extern int sptty;
extern int display();
extern int send_buffer ( unsigned char *, int, int, int, int );

/*-----------------------------------------------------------------+
 | This function erases the eeprom.                                |
 |  0 = 0                                                          |
 |  1 = 3 (start of macros                                         |
 |  2 = 0xff (end of timer initiators)                             |
 |  3 = 0xff (end of macro initiators)                             |
 |                                                                 |
 | It does it by writing the value 0xff to all memory positions    |
 | past the 3rd.  We start by writing bytes 3 through 18 to 0xff,  |
 | and then byte 0 to 0 and 1 to 3.                                |
 |                                                                 |
 | We want to avoid having a timer trigger to an invalid position. |
 | We want all memory to be initialized just in case.              |
 +-----------------------------------------------------------------*/

int eraseall ( void )
{
    unsigned char block[20];
    int           blockno;
    int           x;
    extern void   error();
    extern void   remove_record_file();
    extern int    c_setclock();


    block[0] = 0;
    block[1] = 3;

    for ( x = 2; x < 16; x++ )
        block[x] = (unsigned char) 0xff;

    /* Write the first block */
    if ( sendpacket(0, block) < 0 )  {
        error("Erase failed to write the first block\n");
    }
    fputc( '.', stdout);
    fflush(stdout);

    /* fill the block */
    for(x=0; x < 16; x++)
	block[x] = (unsigned char) 0xff;

    /* For each 16 byte block... */
    for ( blockno = 1; blockno < (PROMSIZE / 16); blockno++ )  {
	if ( sendpacket((blockno * 16)  , block) < 0 )  {
	    char tmpstr[100];
	    sprintf( tmpstr, "Erase failed to write block %d\n", blockno);
	    error(tmpstr);
	}
	fputc( '.', stdout);
	fflush(stdout);
    }

    /* 
     * Delete the X10 Record File and reset the cm11a clock.
     * (In the absence of record file, the clock is set to  
     * system standard time.)
     *
     */
    
    remove_record_file();
    (void) c_setclock(1, NULL); 

    return 0;
}


/*-----------------------------------------------------------------+
 | sendpacket sends the packet from a 19 byte string and handles   |
 | locking and handshake.                                          |
 |    loc = eeprom address (0 relative )                           |
 |    dat = 16 btyes of data.                                      |
 |    Return 0 on success, -1 on error.                            |
 +-----------------------------------------------------------------*/
int sendpacket ( int loc, unsigned char *dat)
{
    unsigned char buf[23];
    extern int    xwrite();
    int           rtn;
    unsigned char sum;
    int           timeout;
    extern int    xwrite(), exread(), check4poll();
    extern unsigned char checksum();


    /* timeout = 10; */
    timeout = 3;

    buf[0] = (char) 0xfb;	/* write to eeprom command */
    buf[1] = loc / 256;         /* hi byte of eeprom address */
    buf[2] = loc % 256;         /* low byte of eeprom address */
    memcpy(&buf[3], dat, 16);

    rtn = xwrite(tty, buf, 19);
    if ( rtn < 0 ) 
        return rtn;

    /* the checksum covers the data... Not the leading 0xfb */
    sum = checksum(buf + 1, 18) ;

    /* read back the check sum */
    rtn = exread(sptty, buf, 1, timeout);
    if ( rtn < 0 ) 
        return rtn;

    if ( sum != buf[0] )  {
	fprintf(stderr, "Checksum failure sending eeprom command\n");
	fprintf( stderr, "Expected %0x, got %0x\n", sum, buf[0]);
	return -2;
    }

    /* check sums match */
    rtn= xwrite(tty, "\00" , 1);  /* WRMI (we really mean it) */
    if ( rtn < 0 ) 
        return(rtn);

    buf[0] = 0;
    rtn = exread(sptty, buf, 1, timeout);
    if ( rtn == 1 ) {
        if ( buf[0] != 0x55 ) {
            fprintf(stderr,
               "Ack after execution = 0x%02x, It should be 0x55)\n", buf[0]);
            rtn = 0;
        }
    }

    if ( rtn != 1 )  {
       if ( rtn == 0 && sum == 0x5a )  /* Workaround for 0x5a checksum problem */
          return 0;
       else {
          fprintf(stderr,
           "Interface not ready after sending data for location  %0x)\n", loc);
          fprintf(stderr, "rtn = %0x\n", rtn);
          return -1;
       }
    }

    return 0;

}


/*-----------------------------------------------------------------+
 | Upload the 1024 byte memory image 'prommap' to the CM11A EEPROM.|
 | Start by nulling out the header block.  Then upload backwards   |
 | so everything will be in place when the new header block is     |
 | uploaded.                                                       |
 +-----------------------------------------------------------------*/
void upload_eeprom_image_old ( unsigned char *prommap )
{
    unsigned char emptyprom[32];
    extern int verbose;
    extern void error();
    int x;

    /* Zero out the initiators to null the header block */

    emptyprom[0] = (unsigned char)0;
    emptyprom[1] = (unsigned char)3;
    for ( x = 2; x < 16; x++)
	emptyprom[x] = (unsigned char)0xff;


    /* erase the old eeprom header information */
    if ( sendpacket(0 , emptyprom) < 0 ) {
        (void)error("load_macro() failed to erase initiator block");  
        /* error() exits */
    }

    /* Copy the data from the array 'prommap' to the CM11A */
    /* Load backwards 
     * This is so the timers at the end of the eeprom are there when
     * the initiators are loaded at the start of the eeprom
     */

    fprintf(stdout, "Uploading %d block memory image to interface.\n", 
    		PROMSIZE / 16 );

    for ( x = 0; x < (PROMSIZE/16); x++ )
        fputc( '.', stdout);
    fputc( '\r', stdout);
    fflush(stdout);

    for ( x = (PROMSIZE / 16) - 1; x >= 0; x-- ) {
        if (verbose)
            fprintf( stderr, "Loading %d\n", x * 16 );

        if ( sendpacket((x * 16), prommap + (x*16)) < 0 ) {
            char tmpstr[100];
            sprintf( tmpstr, "Upload failed to write block %d\n", x);
		error(tmpstr);
        }
        fputc( '#', stdout);
        fflush(stdout);
    }
    fputc('\n', stdout);

    return;
}

/*-----------------------------------------------------------------+
 | Upload the 1024 byte memory image 'prommap' to the CM11A EEPROM.|
 | Start by nulling out the header block.  Then upload backwards   |
 | so everything will be in place when the new header block is     |
 | uploaded.                                                       |
 +-----------------------------------------------------------------*/
void upload_eeprom_image ( unsigned char *prommap )
{
    extern int    verbose;
    unsigned char buffer[32];
    unsigned int  loc;
    int           j, k, block, chkoff, result, timeout = 3;

    /* Offset checksum from beginning of buffer - the initial */
    /* 0xfb is not included in the checksum.                  */
    chkoff = 1;
    
    /* Zero out the initiators to null the header block */

    buffer[0] = 0xfb;
    buffer[1] = 0x00;
    buffer[2] = 0x03;
    for ( j = 3; j < 19; j++)
	buffer[j] = 0xff;

    /* erase the old eeprom header information */
    if ( send_buffer(buffer, 19, chkoff, timeout, NO_SYNC) != 0 ) {
       fprintf(stderr, "upload_eeprom_image() failed to erase initiator block.\n");
       exit(1);
    }

    /* Copy the data from the array 'prommap' to the CM11A */
    /* Load backwards 
     * This is so the timers at the end of the eeprom are there when
     * the initiators are loaded at the start of the eeprom
     */

    fprintf(stdout, "Uploading %d block memory image to interface.\n", 
    		PROMSIZE / 16 );

    for ( j = 0; j < (PROMSIZE/16); j++ )
        fputc( '.', stdout);
    fputc( '\r', stdout);
    fflush(stdout);

    for ( block = (PROMSIZE / 16) - 1; block >= 0; block-- ) {
        loc = block * 16;

        buffer[0] = 0xfb;
        buffer[1] = loc / 256;
        buffer[2] = loc % 256;
        memcpy(&buffer[3], prommap + loc, 16);

        result = 0;  /* Keep compiler happy */
        for ( k = 0; k < 3; k++ ) {
           if (verbose)
               fprintf(stderr, "Uploading block %d to address 0x%03x\n", block, loc );

           if ( (result = send_buffer(buffer, 19, chkoff, timeout, NO_SYNC)) == 0 ) 
              break;
           sleep(2);
        }
        if ( result != 0 ) {
           fprintf(stderr, "Upload failed to write block %d to address 0x%03x\n", block, loc);
           exit(1);
        }

        fputc( '#', stdout);
        fflush(stdout);
    }
    fputc('\n', stdout);

    return;
}

