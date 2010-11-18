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


int sendpacket();
    /* sendpacket sends the packet from a 19 byte string and handles
    Arg1 = eeprom address
    arg2 = 16 byte eeprom data array
    Return 0 on success, -1 on error
    Locks before it starts to write
    locking and handshake. 
    */
int eraseall();
    /*  eraseall uses sendpacket to fill eeprom with zeroed macros,
    timers and triggers.
    */

int validate_sched(FILE *sch_file);
    /* Validates that the config file is OK. 
    Builds a map of the eeprom in memory.
    All dates are validated,
    */


/* Prom is 1024 bytes of memory.  This is gathered from usenet and from 
 * observations:  The CM11A crashes when I exceed 1024 :-) 
 */

# define PROMSIZE 1024 


#if 0
struct timerinit {
	unsigned int unused : 1;
	unsigned int dow : 7 ;
	unsigned int start_jul;
	unsigned int stop_jul;
	unsigned int start_time : 4 ; /* start minute / 120,   */
	unsigned int stop_time : 4 ; /* stop minute / 120,   */
	unsigned int hi_startjul : 1; /* high bit of start_jul */
	unsigned int start_mins : 7 ; /* start minute % 120 */
	unsigned int hi_stopjul : 1; /* high bit of stop_jul */
	unsigned int stop_mins : 7 ; /* stop minute % 120 */
	unsigned int hi_start_nybble : 4;
	unsigned int hi_stop_nybble : 4;
	unsigned int low_start_macro;
	unsigned int low_stop_macro;
	};

struct macroinit {
	unsigned int dev : 4;
	unsigned int hc : 4;
	unsigned int onoff : 1;
	unsigned int unused : 3;
	unsigned int macro_pointer : 12;  /* address the macro is written to */
	unsigned int macro_num;  /* number of the macro in the array */
	int macro_label;	/* label in the config file */
	} ;

struct basicmacrodata {	/* simple one */
	unsigned char delay;
	unsigned char size;	/* number of elements */
	int memsize;		/* amount of prom memory taken by the macro */
	struct macrolinkl *lnk;
	unsigned char fc : 4;
	unsigned char hc : 4;
	unsigned int device_bm ;
	unsigned int terminator;
	} ;

struct macrolinkl {
        struct macrolinkl *lnk;
	unsigned int hc : 4;
	unsigned int fc : 4;
	unsigned int device_bm :16;	/* device bitmap */
	unsigned int dim_value : 5;
	} ;
#endif
