/*
 * Changes for use with CM11A copyright 1996, 1997 Daniel B. Suthers,
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

/*
 * X-10 CM11 Computer Interface Definitions
 */

/***********************************************************************/
/* You probably only need to hack this section to reconfigure for      */
/* your system                                                         */
/***********************************************************************/

#ifndef _x10_header
#define _x10_header

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* Some lints don't know about the void type */
#ifndef HAVE_VOID
#define void int
#endif

#ifdef  SCO
#define _IBCS2
#endif  

#if 0
#ifndef XDIR
#define XDIR "."		/* directory containing X10 files */
#endif
#endif

/* directory for spool file */
#ifndef SPOOLDIR
#define SPOOLDIR "/var/tmp/heyu"
#endif

/* directory used for locks, specified in the Filesystem Hierarchy Standard */
#ifndef LOCKDIR
#define LOCKDIR "/var/lock"
#endif 

/* Base directory under user $HOME */
#define HOMEBASEDIR           ".heyu/" 

/* system-wide Heyu base directory */
#ifndef SYSBASEDIR
#define SYSBASEDIR "/etc/heyu"
#endif

#define SPOOLFILE  "heyu.out"
#define RELAYFILE  "heyu.relay"
#define WRITEFILE  "heyu.write"
#define AUXFILE    "heyu.aux"
#define LOGFILE    "heyu.log"

#if 0
#define IDFILE "/x10id"		/* description file for X10 modules */
#define ALIASFILE "/.x10config"	/* description file for X10 modules */
#endif

#ifdef  MINIEXCH		/* if talking through a DEC Mini-Exchange */
#define MINIXPORT 3		/* port number X10 gizmo is plugged in to */
#endif

#ifdef  VENIX
#define SMALLPAUSE -10		/* 1/6th of a second sleep(3) (VENIX only) */
#else
#define SMALLPAUSE 1
#endif

#define TIMEOUT 10		/* seconds to wait for data */
#if 0
#define DTIMEOUT 15		/* timeout for dim and diagnostic commands */
#endif

/***********************************************************************/
/*                  End of configuration section                       */
/***********************************************************************/

/* Bitmaps for bits 0-3 of the CM11a send status block - (per CWS) */
#define  RESERVED_STATUS_BIT      8
#define  PURGE_DELAYED_MACROS     4
#define  RESET_BATTERY_TIMER      2
#define  MONITORED_STATUS_CLEAR   1

#if 0
#define SYNCN 16		/* number of FF chars to send before packet */

#define CHKSUM(buf) chksum(buf, sizeof(buf))
#endif

#define DIM 0x4
#define BRIGHT 0x5
#define EXTCODE 0x7
#define EXTEND 0xC
#define PRESET1 0xA
#define PRESET2 0xB

#if 0
/* Event item as stored in event file */

struct evitem
    {
    unsigned e_num;
    unsigned char e_buf[8];
    };

#define EVSIZE sizeof(struct evitem)

#define EVENTS "events"		/* event data keyword */
#define ETOTAL 128		/* total number of events */
#define ESIZE  8		/* size of event data field */
#define EVCMD  12		/* size of event command */

/* Data item as stored in data file */

struct ditem
    {
    unsigned d_num;
    unsigned char d_buf[2];
    };

#define DISIZE 6	/* sizeof not used as it includes holes */

#define DATA   "data"		/* id data keyword */
#define DTOTAL 256		/* total number of id's */
#define DSIZE  2		/* size of id data field */
#define DICMD  6		/* size of data command */

/* description field structure */

#define DLENGTH 40		/* length of the description field */

struct id
    {
    char describe[DLENGTH];
    };

/* Command codes */

#define SETHCODE	0	/* load house code */
#define DIRCMD		1	/* direct command */
#define SETCLK		2	/* set clock */
#define DATALOAD	3	/* timer/graphics data download */
#define GETINFO		4	/* get house code and clock */
#define GETEVENTS	5	/* get timer events */
#define GETDATA		6	/* get graphics data */
#define DIAGNOSE	7	/* run diagnostic */

#define XMTSYNC		16	/* transmitted sync length */
#define RCVSYNC		6	/* received sync length */

/* House code magic numbers */

#define HC_A	06
#define HC_B	016
#define HC_C	02
#define HC_D	012
#define HC_E	01
#define HC_F	011
#define HC_G	05
#define HC_H	015
#define HC_I	07
#define HC_J	017
#define HC_K	03
#define HC_L	013
#define HC_M	0
#define HC_N	010
#define HC_O	04
#define HC_P	014

struct hstruct
    {
    unsigned char h_code;
    char h_letter;
    };

struct nstruct
    {
    char *n_name;
    char n_code;
    };
#endif

/* Message definitions */

#define EM_2MANY	"Too many command line arguments"
#define EM_INVCN	"Invalid command name"
#define EM_WNA		"Wrong number of arguments"
#define EM_NMA		"Need more command line arguments"
#define EM_NOCMD	"No command argument specified"

/* External Variables */

extern char *E_2MANY, *E_INVCN, *E_WNA, *E_NMA, *E_NOCMD;
extern void display();
#endif	/* _x10_header */
