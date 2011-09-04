
/*----------------------------------------------------------------------------+
 |                                                                            |
 |                RF Auxiliary Input Support for HEYU                         |
 |              Copyright 2006, 2008 Charles W. Sullivan                      |
 |                                                                            |
 |                                                                            |
 | As used herein, HEYU is a trademark of Daniel B. Suthers.                  | 
 | X10, CM11A, and ActiveHome are trademarks of X-10 (USA) Inc.               |
 | The author is not affiliated with either entity.                           |
 |                                                                            |
 | Charles W. Sullivan                                                        |
 | Co-author and Maintainer                                                   |
 | Greensboro, North Carolina                                                 |
 | Email ID: cwsulliv01                                                       |
 | Email domain: -at- heyu -dot- org                                          |
 |                                                                            |
 +----------------------------------------------------------------------------*/

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
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <signal.h>
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_SYS_ERRNO_H
#include <sys/errno.h>
#endif
#include <ctype.h>

#include "x10.h"
#include "process.h"
#include "oregon.h"

#ifdef pid_t
#define PID_T pid_t
#else
#define PID_T long
#endif

#define W800_LEN 4
#define PACKET_LEN  4
#define BSIZE (100 * PACKET_LEN)


#define HC_MASK      0xF000
#define FUNC_MASK    0x00B8
#define UNIT_MASK    0x0458
#define NOSW_MASK    0x0003
#define OFF_MASK     0x0020
#define OLD_MASK     0x0800
#define NEW_MASK     0x0040
#define STD_MASK     (HC_MASK | FUNC_MASK | UNIT_MASK)
#define NOTX10_MASK  ~(HC_MASK|FUNC_MASK|UNIT_MASK|OLD_MASK)

extern int tty_aux;
extern int sptty;
extern int tty;
extern int verbose;
extern int heyu_parent;
extern int sxread();
extern int i_am_aux, i_am_relay;
extern PID_T lockpid();
extern int lock_for_write(), munlock();
extern int is_sec_ignored ( unsigned int );
extern void send_showbuffer ( unsigned char *, unsigned char );

extern CONFIG config, *configp;

extern unsigned int rflastaddr[16];
extern unsigned int rftransceive[16][7];
extern unsigned int rfforward[16][7];


static unsigned char rf2hcode[16] = {
   0, 8, 4, 12, 2, 10, 6, 14, 1, 9, 5, 13, 3, 11, 7, 15
};

#if 0
/* RF Codes for X10 Housecodes (A-P) */
static unsigned int hc2rfcode[16] = {
   0x6000, 0x7000, 0x4000, 0x5000,
   0x8000, 0x9000, 0xA000, 0xB000,
   0xE000, 0xF000, 0xC000, 0xD000,
   0x0000, 0x1000, 0x2000, 0x3000,
};   

/* Housecode for RF code (upper nybble) */
static char *rf2hc = "MNOPCDABEFGHKLIJ";
#endif

/* RF Codes for X10 _encoded_ unit codes */
static unsigned int rf_unit_code[16] = {
   0x0440, 0x0040, 0x0008, 0x0408,
   0x0448, 0x0048, 0x0000, 0x0400,
   0x0450, 0x0050, 0x0018, 0x0418,
   0x0458, 0x0058, 0x0010, 0x0410,
};      

/* RF Function Codes:               */
/* AllOff, LightsOn, On, Off,       */
/* Dim,    Bright, LightsOff        */
static unsigned int rf_func_code[] = {
   0x0080, 0x0090, 0x0000, 0x0020,
   0x0098, 0x0088, 0x00a0,
};


char *funclist[7] =
   {"alloff", "lightson", "on", "off", "dim", "bright", "lightsoff"};

#if 0
struct rfx_disable_st {
   unsigned long bitmap;
   unsigned short code;
   char     *label;
   int      minlabel;
#endif
struct rfx_disable_st rfx_disable[] = {
  {RFXCOM_ARCTECH,   0xF02D, "ARCTECH",   3},
  {RFXCOM_OREGON,    0xF043, "OREGON",    3},
  {RFXCOM_ATIWONDER, 0xF044, "ATIWONDER", 3},
  {RFXCOM_X10,       0xF02F, "X10",       3},
  {RFXCOM_KOPPLA,    0xF02E, "KOPPLA",    3},
  {RFXCOM_HE_UK,     0xF028, "HE_UK",     4},
  {RFXCOM_HE_EU,     0xF047, "HE_EU",     4},
  {RFXCOM_VISONIC,   0xF045, "VISONIC",   3},
};
int nrfxdisable = sizeof(rfx_disable) / sizeof(struct rfx_disable_st);


/*-------------------------------------------------------------+
 | Check list of valid 4-byte codes other than Standard,       |
 | Entertainment, and Security codes, e.g., Pan/Tilt           |
 | Return type code if valid, otherwise RF_NOISEW800.          |
 | (*** Future ***)                                            |
 +-------------------------------------------------------------*/
int is_valid_w800_code( long code )
{
      
   return RF_NOISEW800;
}

/*-------------------------------------------------------------+
 | Get the firmware versions of master and slave RFXCOM        |
 | receiver.  (The query code is normally 0xF020)              |
 +-------------------------------------------------------------*/
int rfxcom_version ( unsigned short code, unsigned char *master, unsigned char *slave )
{
   unsigned char inbuf[16];
   unsigned char outbuf[4];
   int nread;

   int ignoret;

   outbuf[0] = (code >> 8) & 0xffu;
   outbuf[1] = code & 0xffu;

   *master = *slave = 0;
   ignoret = write(tty_aux, (char *)outbuf, 2);
   nread = sxread(tty_aux, inbuf, 4, 1);
   if ( nread == 2 ) {
      if ( inbuf[0] == 'M' )
         *master = inbuf[1];
      else if ( inbuf[0] == 'S' )
         *slave = inbuf[1];
   }
   else if ( nread == 4 ) {
      if ( inbuf[0] == 'M' )
         *master = inbuf[1];
      else if ( inbuf[0] == 'S' )
         *slave = inbuf[1];

      if ( inbuf[2] == 'M' )
         *master = inbuf[3];
      else if ( inbuf[2] == 'S' )
         *slave = inbuf[3];
   }
   else if ( nread == 1 ) {
      syslog(LOG_ERR, "RFXCOM version query returned only a single byte, 0x%02x\n", inbuf[0]);
      return 1;
   }
   else if ( nread == 3 ) {
      syslog(LOG_ERR, "RFXCOM version query returned 3 bytes, 0x%02x 0x%02x 0x%02x\n", 
        inbuf[0], inbuf[1], inbuf[2]);
      return 1;
   }
   else {
      syslog(LOG_ERR, "RFXCOM version query returned nothing.\n");
      return 1;
   }
 
   return 0;
}

/*-------------------------------------------------------------+
 | Configure an RFXCOM mode. When sent the 2-byte configure    |
 | code, e.g., 0xF029 for W800 emulation mode, it should       |
 | respond with the ack byte, e.g., 0x29.                      |
 +-------------------------------------------------------------*/
unsigned char configure_rfxcom ( unsigned short code )
{
   unsigned char inbuf[4];
   unsigned char outbuf[4];

   int ignoret;

   outbuf[0] = (code >> 8) & 0xffu;
   outbuf[1] = code & 0xffu;

   ignoret = write(tty_aux, (char *)outbuf, 2);

   if ( sxread(tty_aux, inbuf, 1, 1) <= 0 ) 
      return 0xffu;

   return inbuf[0];
}

/*-------------------------------------------------------------+
 | Configure the RF receiver if required.                      |
 +-------------------------------------------------------------*/
int configure_rf_receiver ( unsigned char auxdev )
{
   unsigned char  inbuf[8], mver = 0xff, sver = 0xff;
   unsigned short rtn, crtn;
   int            is_config;
   int            j;
   char           *receiver;
   extern int     config_tty_dtr_rts ( int, unsigned char );

   /* Nothing required for W800RF32 or MR26A */
   if ( auxdev == DEV_W800RF32 || auxdev == DEV_MR26A )
      return 0;

   /* Configuration below is for RFXCOM receivers */

   /* Empty the serial port buffers */
   while ( sxread(tty_aux, inbuf, 1, 1) > 0 );

   /* Get the RFXCOM firmware versions */
   rfxcom_version(0xF020, &mver, &sver);

   crtn = 0;
   if ( auxdev == DEV_RFXCOMVL ) {
      /* Variable length mode */
      is_config = ((crtn = configure_rfxcom(0xF02C)) == 0x2Cu);
      if ( !is_config ) {
         syslog(LOG_ERR, "RFXCOM F02C, ack 0x%02x\n", crtn);
      }
   }
   else if ( auxdev == DEV_RFXCOM32 ) {
      /* 32 bit (W800) mode */
      is_config = ((crtn = configure_rfxcom(0xF029)) == 0x29u);
      if ( !is_config ) {
         syslog(LOG_ERR, "RFXCOM F029, ack 0x%02x\n", crtn);
      }
   }
   else {
      is_config = 0;
   }
   
   /* Enable all possible receiving modes */
   if ((rtn = configure_rfxcom(0xF02A)) != crtn ) 
      syslog(LOG_ERR, "RFXCOM F02A, ack 0x%02x\n", rtn);

#if 0
   /* Disable ARC-Tech RF unless enabled */
   if ( (configp->rfxcom_enable & RFXCOM_ARCTECH) == 0 ) {
      if ((rtn = configure_rfxcom(0xF02D)) != crtn )
         syslog(LOG_ERR, "RFXCOM F02D, ack 0x%02x\n", rtn);
   }

   /* Disable Oregon RF unless enabled */
   if ( (configp->rfxcom_enable & RFXCOM_OREGON) == 0 ) {
      if ((rtn = configure_rfxcom(0xF043)) != crtn )
         syslog(LOG_ERR, "RFXCOM F043, ack 0x%02x\n", rtn);
   }

   /* Disable ATI Remote Wonder RF unless enabled */
   if ( (configp->rfxcom_enable & RFXCOM_ATIWONDER) == 0 ) {
      if ( (rtn = configure_rfxcom(0xF044)) != crtn )
         syslog(LOG_ERR, "RFXCOM F044, ack 0x%02x\n", rtn);
   }
#endif

#if 0
   /* Enable specific modes (DEPRECATED) */
   for ( j = 0; j < 3; j++ ) {
      if ( !(configp->rfxcom_enable & rfx_disable[j].bitmap) ) {
         if ( (rtn = configure_rfxcom(rfx_disable[j].code)) != crtn)
            syslog(LOG_ERR, "RFXCOM 0x%4X, ack 0x%02x\n", rfx_disable[j].code, rtn);
      }
   }
#endif

   /* Disable specific modes */
   for ( j = 0; j < nrfxdisable /*sizeof(rfx_disable)/sizeof(struct rfx_disable_st)*/; j++ ) {
      if ( configp->rfxcom_disable & rfx_disable[j].bitmap ) {
         if ( (rtn = configure_rfxcom(rfx_disable[j].code)) != crtn)
//            syslog(LOG_ERR, "RFXCOM 0x%4X, ack 0x%02x\n", rfx_disable[j].code, rtn);
            syslog(LOG_ERR, "Disable %s failure: bad ack 0x%02x\n", rfx_disable[j].label, rtn);
      }
   }
      
   receiver = 
     (auxdev == DEV_RFXCOM32)  ? "RFXCOM32" :
     (auxdev == DEV_RFXCOMVL)  ? "RFXCOMVL" : "UNKNOWN";

   if ( is_config ) {
      syslog(LOG_ERR, "RFXCOM version M:0x%02x S:0x%02x\n", mver, sver);
      syslog(LOG_ERR, "%s configured-\n", receiver);
   }
   else {
      syslog(LOG_ERR, "%s not configured\n", receiver);
      return 1;
   }

   return 0;
}






/*-------------------------------------------------------------+
 | Translate Standard X10 RF code into hcode, ucode, and fcode |
 +-------------------------------------------------------------*/
int rf_parms (int rfword, unsigned char *hcode, unsigned char *ucode, unsigned char *fcode)
{
   unsigned int  rem;
   int           j;

   /* Old X10 wireless switch code to new code */
   if ( rfword & OLD_MASK ) {
      rfword = (rfword & ~OLD_MASK) | NEW_MASK;
   }

   /* Standardize */
   rfword &= STD_MASK;

   *hcode = rf2hcode[(rfword & HC_MASK) >> 12];

   rem = rfword & ~HC_MASK & ~OFF_MASK;

   for ( j = 0; j < 16; j++ ) {
      if ( rem == rf_unit_code[j] ) {
         *ucode = j;
         *fcode = (rfword & OFF_MASK) ? 3 : 2;
         return 0;
      }
   }

   *ucode = 0xff;
   rem = rfword & 0xff;
   for ( j = 0; j < 7; j++ ) {
      if ( rem == rf_func_code[j] ) {
         *fcode = j;
         return 0;
      }
   }

   return 1;
}

/*-------------------------------------------------------------+
 | Determine what to do with standard X10 RF signal            |
 +-------------------------------------------------------------*/
int rf_disposition ( unsigned char hcode, unsigned char ucode,
                                          unsigned char fcode )
{
   int          retcode = RF_IGNORE;
   unsigned int bitmap;

   switch ( fcode ) {
      case 2 :
      case 3 :
         bitmap = 1 << ucode;
         rflastaddr[hcode] = bitmap;
      case 4 :
      case 5 :
         bitmap = rflastaddr[hcode];
         if ( rftransceive[hcode][fcode] & bitmap )
            retcode = RF_TRANSCEIVE;
         else if ( rfforward[hcode][fcode] & bitmap )
            retcode = RF_FORWARD;
         else
            retcode = RF_IGNORE;
         break;
      case 0 :
      case 1 :
      case 6 :
      default :
         if ( rftransceive[hcode][fcode] )
            retcode = RF_TRANSCEIVE;
         else if ( rfforward[hcode][fcode] )
            retcode = RF_FORWARD;
         else
            retcode = RF_IGNORE;
         break;
   }

   return retcode;
}


/*----------------------------------------------------------------------------+
 | Display a text message in the heyu monitor/state engine logfile            |
 | Maximum length 80 characters.                                              |
 +----------------------------------------------------------------------------*/
int display_aux_message ( char *message )
{
   extern int    sptty;
   int           size;
   unsigned char buf[88];
   char writefilename[PATH_LEN + 1];

   int   ignoret;

   static unsigned char template[7] = {
     0xff,0xff,0xff,0,ST_COMMAND,ST_MSG,0};

   if ( (size = strlen(message)) > (int)(sizeof(buf) - 8) ) {
      fprintf(stderr,
        "Log message exceeds %d characters\n", (int)(sizeof(buf) - 8));
      return 1;
   }
   
   sprintf(writefilename, "%s%s", WRITEFILE, configp->suffix);

   if ( lock_for_write() < 0 ) {
      syslog(LOG_ERR, "aux unable to lock writefile\n");
      return 1;
   }

   memcpy(buf, template, sizeof(template));
   memcpy(buf + 7, message, size + 1);

   buf[6] = size + 1;
   buf[3] = size + 1 + 3;

   ignoret = write(sptty, buf, size + 8);

   munlock(writefilename);

   return 0;
}

int atp ( int testpoint )
{
   char message[80];

   sprintf(message, "Aux Testpoint %d", testpoint);

   display_aux_message(message);

   return 0;
}


/*----------------------------------------------------------------------------+
 | Forward variable length data from aux to the heyu monitor/state engine     |
 +----------------------------------------------------------------------------*/
int forward_variable_aux_data ( unsigned char vl_type, unsigned char *buff, unsigned char length )
{
   extern int sptty;
   char writefilename[PATH_LEN + 1];
   unsigned char sendbuf[80];

   int ignoret;

   static unsigned char template[15] = {
    0xff,0xff,0xff,3,ST_COMMAND,ST_SOURCE,D_AUXRCV,
    0xff,0xff,0xff,0,ST_COMMAND,ST_VARIABLE_LEN,0,0};

   sprintf(writefilename, "%s%s", WRITEFILE, configp->suffix);

   if ( lock_for_write() < 0 ) {
      syslog(LOG_ERR, "aux unable to lock writefile\n");
      return 1;
   }
   template[10] = length + 4;
   template[13] = vl_type;
   template[14] = length;

   memcpy(sendbuf, template, sizeof(template));
   memcpy(sendbuf + sizeof(template), buff, length);

   ignoret = write(sptty, sendbuf, sizeof(template) + length);

#if 0
   ignoret = write(sptty, template, 15);
   ignoret = write(sptty, buff, length);
#endif

   munlock(writefilename);

   return 0;
}




/*----------------------------------------------------------------------------+
 | Forward std data from aux to the heyu monitor/state engine                 |
 +----------------------------------------------------------------------------*/
int forward_standard_aux_data ( unsigned char byte1, unsigned char byte2 )
{
   extern int sptty;
   char writefilename[PATH_LEN + 1];

   int ignoret;

   static unsigned char template[13] = {
    0xff,0xff,0xff,3,ST_COMMAND,ST_SOURCE,D_AUXRCV,
    0xff,0xff,0xff,2,0,0};

   sprintf(writefilename, "%s%s", WRITEFILE, configp->suffix);

   if ( lock_for_write() < 0 ) {
      syslog(LOG_ERR, "aux unable to lock writefile\n");
      return 1;
   }
   template[11] = byte1;
   template[12] = byte2;

   ignoret = write(sptty, template, 13);

   munlock(writefilename);

   return 0;
}


/*----------------------------------------------------------------------------+
 | Send virtual data from aux to the heyu monitor/state engine                |
 +----------------------------------------------------------------------------*/
int send_virtual_aux_data ( unsigned char address, unsigned char vdata,
     unsigned char vtype, unsigned char vidlo, unsigned char vidhi, unsigned char byte2, unsigned char byte3 )
{
   extern int sptty;
   char writefilename[PATH_LEN + 1];

   int ignoret;

   static unsigned char template[20] = {
    0xff,0xff,0xff,3,ST_COMMAND,ST_SOURCE,D_AUXRCV,
    0xff,0xff,0xff,9,ST_COMMAND,ST_VDATA,0,0,0,0,0,0,0};

   sprintf(writefilename, "%s%s", WRITEFILE, configp->suffix);

   if ( lock_for_write() < 0 ) {
      syslog(LOG_ERR, "aux unable to lock writefile\n");
      return 1;
   }
   template[13] = address;
   template[14] = vdata;
   template[15] = vtype;
   template[16] = vidlo;
   template[17] = vidhi;
   template[18] = byte2;
   template[19] = byte3;

   ignoret = write(sptty, template, 20);

   munlock(writefilename);

   return 0;
}

/*----------------------------------------------------------------------------+
 | Send virtual data from command line to the heyu monitor/state engine       |
 | (Similar to the above, but the file lock is already acquired.)             |
 +----------------------------------------------------------------------------*/
int send_virtual_cmd_data ( unsigned char address, unsigned char vdata,
     unsigned char vtype, unsigned char vidlo, unsigned char vidhi, unsigned char byte2, unsigned char byte3 )
{
   extern int sptty;
   unsigned char source;

   int ignoret;

   source = heyu_parent;

   static unsigned char template[20] = {
    0xff,0xff,0xff,3,ST_COMMAND,ST_SOURCE,0,
    0xff,0xff,0xff,9,ST_COMMAND,ST_VDATA,0,0,0,0,0,0,0};

   template[6]  = source;
   template[13] = address;
   template[14] = vdata;
   template[15] = vtype;
   template[16] = vidlo;
   template[17] = vidhi;
   template[18] = byte2;
   template[19] = byte3;

   ignoret = write(sptty, template, 20);

   return 0;
}

/*----------------------------------------------------------------------------+
 | Send buffer of data from aux to the heyu monitor/state engine              |
 +----------------------------------------------------------------------------*/
int forward_aux_longdata ( unsigned char vtype, unsigned char seq, unsigned char id_high, unsigned char id_low,
    unsigned char nbytes, unsigned char buff[])
{
   extern int sptty;
   char writefilename[PATH_LEN + 1];
   unsigned char sendbuf[80];

   int ignoret;

   static unsigned char template[18] = {
    0xff,0xff,0xff,3,ST_COMMAND,ST_SOURCE,D_AUXRCV,
    0xff,0xff,0xff,0,ST_COMMAND,ST_LONGVDATA,0, 0,0,0,0};

   sprintf(writefilename, "%s%s", WRITEFILE, configp->suffix);

   if ( lock_for_write() < 0 ) {
      syslog(LOG_ERR, "aux unable to lock writefile\n");
      return 1;
   }
 
   template[10] = nbytes + 7;
   template[13] = vtype;
   template[14] = seq;
   template[15] = id_high;
   template[16] = id_low;
   template[17] = nbytes;

   memcpy(sendbuf, template, sizeof(template));
   memcpy(sendbuf + sizeof(template), buff, nbytes);

   ignoret = write(sptty, sendbuf, sizeof(template) + nbytes);

   munlock(writefilename);

   return 0;
}

/*----------------------------------------------------------------------------+
 | Forward general aux data.                                                  |
 | Buffer reference: ST_COMMAND, ST_GENLONG, plus:                            |
 |  vtype, subtype, vidhi, vidlo, seq, buflen, buffer                         |
 |   [2]     [3]     [4]    [5]   [6]    [7]    [8+]                          |
 +----------------------------------------------------------------------------*/
int forward_gen_longdata ( unsigned char vtype, unsigned char subtype, int nseq,
    unsigned char id_high, unsigned char id_low, unsigned char nbytes, unsigned char buff[] )
{
   extern int sptty;
   char writefilename[PATH_LEN + 1];
   unsigned char outbuf[128];
   int  j;

   int ignoret;

   static unsigned char template[19] = {
    0xff,0xff,0xff,3,ST_COMMAND,ST_SOURCE,D_AUXRCV,
    0xff,0xff,0xff,0,ST_COMMAND,ST_GENLONG,0,0,0,0,0,0};

   if ( (sizeof(template) + nbytes) > sizeof(outbuf) ) {
      syslog(LOG_ERR, "forward_gen_longdata(): input buffer too long\n");
      return 1;
   }

   sprintf(writefilename, "%s%s", WRITEFILE, configp->suffix);

   if ( lock_for_write() < 0 ) {
      syslog(LOG_ERR, "aux unable to lock writefile\n");
      return 1;
   }

   template[10] = nbytes + 8;
   template[13] = vtype;
   template[14] = subtype;
   template[15] = id_high;
   template[16] = id_low;
   template[18] = nbytes;
   
   for ( j = 0; j < nseq; j++ ) {
      template[17] = (unsigned char)(j + 1);
      memcpy(outbuf, template, (sizeof(template)));
      memcpy(outbuf + (sizeof(template)), buff, nbytes);
      ignoret = write(sptty, (char *)outbuf, (sizeof(template)) + nbytes);
   }

   munlock(writefilename);

   return 0;
}


/*-------------------------------------------------------------+
 | This function forwards Digimax data to the Engine 3 times,  |
 | with the parity removed and replaced by a sequence number   |
 | 1-3.  (This is so the engine can split the data into three  |
 | separate functions.)                                        |
 +-------------------------------------------------------------*/
int forward_digimax ( unsigned char *buff )
{
   int j;

   for ( j = 0; j < 3; j++ ) {
      forward_aux_longdata(RF_DIGIMAX, j + 1, buff[0], buff[1], 6, buff);
   }
   return 0;
}

/*-------------------------------------------------------------+
 | Forward RF data of type RF_STD (Standard X10 RF remotes)    |
 | to Heyu engine via the spoolfile.                           |
 +-------------------------------------------------------------*/
int forward_std ( unsigned char hcode, unsigned char ucode,
                                            unsigned char fcode )
{
   unsigned char cmdbyte;

   switch ( fcode ) {
      case 2 :
      case 3 :
         forward_standard_aux_data(0x04, (hcode << 4 | ucode));
         forward_standard_aux_data(0x06, (hcode << 4 | fcode));
         break;
      case 4 :
      case 5 :
         cmdbyte = 0x06 | (configp->trans_dim << 3);
         forward_standard_aux_data(cmdbyte, (hcode << 4 | fcode));
         break;
      case 0 :
      case 1 :
      case 6 :
         forward_standard_aux_data(0x06, (hcode << 4 | fcode));
         break;
      default :
         break;
   }
   
   return 0;
}
      
/*-------------------------------------------------------------+
 | Transceive RF data of type RF_STD (Standard X10 RF remotes) |
 | and send to spoolfile and interface per config.             |
 +-------------------------------------------------------------*/
int transceive_std( unsigned char hcode, unsigned char ucode,
                                            unsigned char fcode )
{
   unsigned char cmdbuf[16];
   char writefilename[PATH_LEN + 1];

   if ( configp->device_type & DEV_DUMMY ) {
      display_x10state_message("Unable to transceive to TTY dummy");
      return 1;
   }

   sprintf(writefilename, "%s%s", WRITEFILE, configp->suffix);

   if ( lock_for_write() < 0 ) {
      syslog(LOG_ERR, "aux unable to lock writefile\n");
      return 1;
   }

   switch ( fcode ) {
      case 2 : /* On */
      case 3 : /* Off */
         send_address(hcode, (1 << ucode), NO_SYNC);
         cmdbuf[0] = 0x06;
         cmdbuf[1] = (hcode << 4) | fcode;
         send_command(cmdbuf, 2, 0, NO_SYNC);
         break;
      case 4 : /* Dim */
      case 5 : /* Bright */
         cmdbuf[0] = 0x06 | (configp->trans_dim << 3);
         cmdbuf[1] = (hcode << 4) | fcode;
         send_command(cmdbuf, 2, 0, NO_SYNC);
         break;
      case 0 : /* AllOff */
      case 1 : /* LightsOn */
      case 6 : /* LightsOff */
      default :
         cmdbuf[0] = 0x06;
         cmdbuf[1] = (hcode << 4) | fcode;
         send_command(cmdbuf, 2, 0, NO_SYNC);
         break;
   }

   munlock(writefilename);


   return 0;
}


/*-------------------------------------------------------------+
 | Process RF data of type RF_STD (Standard X10 RF remotes)    |
 | and send to spoolfile and interface per config.             |
 +-------------------------------------------------------------*/
int proc_type_std( unsigned char hcode, unsigned char ucode,
                                            unsigned char fcode )
{
   int dispo;

   dispo = rf_disposition(hcode, ucode, fcode);

   if ( dispo == RF_TRANSCEIVE )
      transceive_std(hcode, ucode, fcode);
   else if ( dispo == RF_FORWARD )
      forward_std(hcode, ucode, fcode);

   return 0;
}

/*-------------------------------------------------------------+
 | RFXMeter 4 bit checksum.  A valid message returns 0x00.     |
 | Force a failure if Heyu is not configured for RFXMeters.    |
 +-------------------------------------------------------------*/
int rfxmeter_checksum ( unsigned char *buf )
{
   unsigned int chksum;

   chksum = buf[0] + (buf[0] >> 4) + buf[1] + (buf[1] >> 4) +
            buf[2] + (buf[2] >> 4) + buf[3] + (buf[3] >> 4) +
            buf[4] + (buf[4] >> 4) + buf[5] + (buf[5] >> 4);

//   chksum &= 0x0fu;
   chksum = (chksum - 0x0fu) & 0x0fu;

#ifdef HAVE_FEATURE_RFXM
   return (int)chksum;
#else
   return (int)0xffu;
#endif
}

/*-------------------------------------------------------------+
 | RFXSensor 4 bit checksum.  A valid message returns 0x0f.    |
 | Force a failure if Heyu is not configured for RFXSensors.   |
 +-------------------------------------------------------------*/
int rfxsensor_checksum ( unsigned char *buf )
{
   unsigned int chksum;

   chksum = buf[0] + (buf[0] >> 4) + buf[1] + (buf[1] >> 4) +
            buf[2] + (buf[2] >> 4) + buf[3] + (buf[3] >> 4);

   chksum = ~chksum & 0x0fu;
//   chksum &= 0x0fu;

#ifdef HAVE_FEATURE_RFXS
   return (int)chksum;
#else
   return (int)0xffu;
#endif
}

/*-------------------------------------------------------------+
 | Parity check for 6-byte X10 Security messages in RFXCOM     |
 | variable length mode.  Return 0 for pass, 1 for fail.       |
 | This is the even parity of byte4 and bit 7 of byte5.        |                                                            |
 |                                                             | 
 | Some RF modules, e.g., the AUX channel in a DS90 Door/      |
 | Window sensors, don't reliably transmit a signal with the   |
 | correct parity bit set, apparently due to a firmware bug.   |
 | The config directive allows this checking to be bypasssed.  |
 +-------------------------------------------------------------*/
int security_checksum ( unsigned char byte4, unsigned char byte5 )
{
   int j, bitsum = 0;

   if ( configp->securid_parity == NO )
      return 0;

   for ( j = 0; j < 8; j++ )
      bitsum += (byte4 >> j) & 0x01;

   return ( (bitsum % 2) &&  (byte5 & 0x80)) ? 0 : 
          (!(bitsum % 2) && !(byte5 & 0x80)) ? 0 : 1;
}

/*-------------------------------------------------------------+
 | Parity check for 6-byte Digimax message in RFXCOM variable  |
 | length mode.  Return 0 for pass, otherwise fail.            |
 +-------------------------------------------------------------*/
unsigned char digimax_checksum ( unsigned char *buf )
{
   unsigned char sum1, sum2;

   sum1 = ~((buf[0] >> 4) + (buf[0] & 0x0fu) +
            (buf[1] >> 4) + (buf[1] & 0x0fu) +
            (buf[2] >> 4) + (buf[2] & 0x0fu)) & 0x0fu;

   sum2 = ~((buf[3] >> 4) + (buf[3] & 0x0fu) +
            (buf[4] >> 4) + (buf[4] & 0x0fu) +
            (buf[5] >> 4) + (buf[5] & 0x0fu)) & 0x0fu;

#ifdef HAVE_FEATURE_DMX
   return  (sum1 << 4) | sum2;
#else
   return 0xffu;
#endif
}


/*-------------------------------------------------------------+
 | Alert to an RF Flood                                        |
 +-------------------------------------------------------------*/
int send_rf_flood_message( unsigned char vflags )
{
   send_virtual_aux_data(0, vflags, RF_FLOOD, 0, 0, 0, 0);

   return 0;
}

int write_restart_error ( char *restartfile )
{
   FILE *fp;
  
   if ( (fp = fopen(restartfile, "w")) == NULL ) {
      syslog(LOG_ERR, "Cannot fopen restartfile\n");
      return 1;
   }

   fprintf(fp, "%s\n", error_message());

   fclose(fp);

   return 0;
}

#define ABSMAX_IN_A_ROW  1000000
   
enum {MinCount, RepCount, MaxCount, FloodRep};

/*-------------------------------------------------------------+
 | Process data read from the W800RF32 on the TTY_AUX serial   |
 | port and send to spoolfile and/or interface per config.     |
 +-------------------------------------------------------------*/
int aux_w800 ( void )
{
   unsigned char  type;
   unsigned char  hcode, ucode, fcode, rfflood, addr;
   unsigned char  buff[8];
   unsigned long  in_a_row, max_in_a_row;
   unsigned int   saddr = 0xffff;
   long           rfword, last_word, this_word;
   int            count = 0;
   int            mincount;
   int            nread = 0;
   int            is_idle = 0;
   int            is_err = 0;
   int            use_old_buffer = 0;
   char           restartfile[PATH_LEN + 1];
   struct stat    statbuf;
   extern void    aux_local_setup ( void );
   int            configure_rf_receiver ( unsigned char );
   int            proc_rfxsensor_init ( int, unsigned char *, int );
   int            send_rfxtype_message ( char, unsigned char );

   static unsigned char jammast[2] = {0xFF,0x0F};
   static unsigned char jamslav[2] = {0x00,0xF0};
   static unsigned char jamstrt[2] = {0x07,0xF8};
   static unsigned char jamend[2]  = {0x1F,0xE0};

   rfword = last_word = -1;
   type = 0;
   nread = 0;
   is_idle = 0;
   in_a_row = 0;
   rfflood = 0;
   max_in_a_row = config.aux_repcounts[MaxCount];
   
   strncpy2(restartfile, pathspec(RESTART_AUX_FILE), PATH_LEN);
   unlink(restartfile);

   openlog( "heyu_aux", 0, LOG_DAEMON);

   configure_rf_receiver(config.auxdev);

   while ( 1 )  {
      /* Check if restart needed */
      if ( is_idle && stat(restartfile, &statbuf) == 0 && is_err == 0 ) {
         if ( reread_config() != 0 /* || configure_rf_receiver(config.auxdev) != 0 */ ) {
            is_err = 1;
            syslog(LOG_ERR, "aux reconfiguration failed!\n");
            write_restart_error(restartfile);
         }
         else {
            syslog(LOG_ERR, "aux reconfiguration-\n");
            unlink(restartfile);
            return 0;
         }
      }

      while ( nread < 4 && sxread(tty_aux, buff + nread, 1, 1) ) {
         nread++;
      }

      if ( nread < 4 ) {
         is_idle = 1;
         nread = 0;
         count = 0;
         in_a_row = 0;
         max_in_a_row = config.aux_repcounts[MaxCount];
         if ( max_in_a_row > 0 && rfflood > 0 )
            send_rf_flood_message(0); /* End of flood */
         rfflood = 0;
         continue;
      }
      is_idle = 0;

      /* Check for a continuous flood of RF inputs without a break */
      if ( max_in_a_row > 0 && ++in_a_row == max_in_a_row ) {
         rfflood = 1;
         send_rf_flood_message(SEC_FLOOD);
         max_in_a_row *= config.aux_repcounts[FloodRep];
         if ( max_in_a_row > ABSMAX_IN_A_ROW ) {
            max_in_a_row = ABSMAX_IN_A_ROW;
            in_a_row = 0;
         }
         continue;
      }

      mincount = config.aux_repcounts[MinCount];

      if ( (buff[0] ^ buff[1]) == 0xf0u && rfxsensor_checksum(buff) == 0 ) {
         /* RFXSensor RF */
         type = RF_XSENSOR;
         this_word = (buff[0] << 16) | (buff[2] << 8) | buff[3];
         mincount = config.aux_mincount_rfx;
      }
      else if ( memcmp(buff, "RF", 2) == 0 && 
                (buff[2] == '3' || buff[2] == '2' || buff[2] == 'X') ) {
         type = RF_XSENSORHDR;
         this_word = (buff[0] << 24) | (buff[1] << 16) | (buff[2] << 8) | buff[3];
         mincount = config.aux_mincount_rfx;
      }
      else if ( (memcmp(buff, jamstrt, 2) == 0 || memcmp(buff, jamend,2) == 0) &&
                (memcmp(buff + 2, jammast, 2) == 0 || memcmp(buff + 2, jamslav, 2) == 0) ) {
         /* RFXCOM jamming signal */
         if ( config.suppress_rfxjam == YES ) {
            if ( config.disp_raw_rf & DISPMODE_RF_NOISE ) 
               send_virtual_aux_data(0, buff[0], RF_NOISEW800, buff[1], 0, buff[2], buff[3]);
         }
         else {
            send_virtual_aux_data(0, buff[2], RF_XJAM32, buff[0], 0, 0, 0);
         }
         nread = 0;
         continue;
      }
      else if ( (buff[2] ^ buff[3]) != 0xffu ) {
         this_word = (buff[0] << 24) | (buff[1] << 16) | (buff[2] << 8) | buff[3];
         if ( (type = is_valid_w800_code(this_word)) == RF_NOISEW800 ) {
            /* Noise */
            if ( config.disp_raw_rf & DISPMODE_RF_NOISE )
               send_virtual_aux_data(0, buff[0], RF_NOISEW800, buff[1], 0, buff[2], buff[3]);
            nread = 0;
            continue;
         }
      }
      else if ( (buff[0] ^ buff[1]) == 0xffu ) {
         /* X10 Standard RF (type 0) or Entertainment RF (type 1) */
         type = (buff[0] & 0x02) ? RF_ENT : RF_STD;
         this_word = (buff[0] << 8 | buff[2]);
      }
      else if ( (buff[0] ^ buff[1]) == 0x0fu ) {
         /* X10 Security RF (type 2) */
         type = RF_SEC;
         saddr = buff[0];
         this_word = (buff[0] << 8 | buff[2]);
      }
      else if ( (buff[0] ^ buff[1]) == 0xfeu ) {
         /* DM10 Motion sensor or GB10 Glass Break sensor */
         type = RF_SEC;
         saddr = buff[0];
         this_word = (buff[0] << 8 | buff[2]);
      }
#if 0
      else if ( (buff[0] ^ buff[1]) == 0xfeu && (buff[2] & 0xf0u) == 0xf0u ) {
         /* GB10 Dusk sensor */
         type = RF_DUSK;
         this_word = (buff[0] << 8 | buff[2]);
      }
      else if ( (buff[0] ^ buff[1]) == 0xfeu && buff[2] == 0xe0u ) {
         /* MD10 Motion sensor */
         type = RF_SECX;
         saddr = buff[0];
         this_word = (buff[0] << 8 | buff[2]);
      }
#endif
      else {
         this_word = (buff[0] << 24) | (buff[1] << 16) | (buff[2] << 8) | buff[3];
         if ( (type = is_valid_w800_code(this_word)) == RF_NOISEW800 ) {
            /* Noise */
            if ( config.disp_raw_rf & DISPMODE_RF_NOISE )
               send_virtual_aux_data(0, buff[0], RF_NOISEW800, buff[1], 0, buff[2], buff[3]);
            nread = 0;
            continue;
         }
      }

      if ( (type == RF_SEC || type == RF_SECX) && is_sec_ignored(saddr) != 0 ) {
         use_old_buffer = 0;
         nread = 0;
         count = 0;
         continue;
      }

      if ( (config.disp_raw_rf & DISPMODE_RF_NORMAL) && !use_old_buffer ) {
         send_virtual_aux_data(0, buff[0], RF_RAWW800, buff[1], 0, buff[2], buff[3]);
      }
      use_old_buffer = 0;

      if ( count == 0 ) {
         rfword = this_word;
         count++;
         nread = 0;
         if ( type == RF_STD ) {
            if ( rf_parms((int)rfword, &hcode, &ucode, &fcode) != 0 ) {
               if ( config.disp_raw_rf & DISPMODE_RF_NOISE )
                  send_virtual_aux_data(0, buff[0], RF_NOISEW800, buff[1], 0, buff[2], buff[3]);
               nread = 0;
               continue;
            }
            if ( count == mincount /*config.aux_repcounts[MinCount]*/ ) {
               proc_type_std(hcode, ucode, fcode);
            }
         }
         else if ( type == RF_SEC || type == RF_ENT || type == RF_XJAM32 ) {
            if ( count == mincount /*config.aux_repcounts[MinCount]*/ ) {
               send_virtual_aux_data(0, buff[2], type, buff[0], 0, 0, 0);
            }
         }
         else if ( type == RF_DUSK || type == RF_SECX ) {
            if ( count == mincount /*config.aux_repcounts[MinCount]*/ ) {
               addr = rf2hcode[buff[0] >> 4] << 4;
//               send_virtual_aux_data(addr, buff[2], type, buff[0], 0, 0, 0);
               send_virtual_aux_data(buff[0], buff[2], type, buff[0], 0, 0, 0);
            }
         }
         else if ( type == RF_XSENSOR ) {
            if ( count == mincount /*config.aux_mincount_rfx*/ ) {
               send_virtual_aux_data(0, 0, type, buff[0], 0, buff[2], buff[3]);
            }
         }
         else if ( type == RF_XSENSORHDR ) {
            if ( count == mincount /*config.aux_mincount_rfx*/ ) {
               send_rfxtype_message((char)buff[2], buff[3]);
            }
         }
         else if ( count == mincount /*config.aux_repcounts[MinCount]*/ ) {
            send_virtual_aux_data(0, buff[0], type, buff[1], 0, buff[2], buff[3]);
         }
      }
      else if ( this_word == rfword ) {
         /* Repeat of previous burst */
         count++;
         nread = 0;
         if ( config.aux_repcounts[RepCount] > mincount &&
              (count % config.aux_repcounts[RepCount]) == mincount &&
              (config.aux_repcounts[MaxCount] == 0 ||
               count < config.aux_repcounts[MaxCount]) ) {
            if ( type == RF_STD ) {
               if ( rf_parms((int)rfword, &hcode, &ucode, &fcode) != 0 ) {
                  if ( config.disp_raw_rf & DISPMODE_RF_NOISE )
                     send_virtual_aux_data(0, buff[0], RF_NOISEW800, buff[1], 0, buff[2], buff[3]);
                  nread = 0;
                  continue;
               }
               proc_type_std(hcode, ucode, fcode);
            }
            else if ( type == RF_SEC || type == RF_ENT || type == RF_XJAM32 ) {
               send_virtual_aux_data(0, buff[2], type, buff[0], 0, 0, 0);
            }
            else if ( type == RF_DUSK || type == RF_SECX ) {
               addr = rf2hcode[buff[0] >> 4] << 4;
//               send_virtual_aux_data(addr, buff[2], type, buff[0], 0, 0, 0);
               send_virtual_aux_data(buff[0], buff[2], type, buff[0], 0, 0, 0);
            }
            else if ( type == RF_XSENSOR ) {
               send_virtual_aux_data(0, 0, type, buff[0], 0, buff[2], buff[3]);
            }
            else if ( type == RF_XSENSORHDR ) {
               send_rfxtype_message((char)buff[2], buff[3]);
            }
            else {
               send_virtual_aux_data(0, buff[0], type, buff[1], 0, buff[2], buff[3]);
            }
         }
      }
      else {
         /* New burst, save buffer */
         use_old_buffer = 1;
         count = 0;
      }
   }
   return 0;
}

#ifdef RFXCOM_DUAL
/*-------------------------------------------------------------+
 | Process data read from the RFXCOM receiver in variable      |
 | length message mode on the TTY_AUX serial port and send to  |
 | spoolfile and/or interface per config.                      |
 |                                                             |
 | Maintain separate queues for signals from Master and Slave  |
 | to avoid count errors when signals are received close       |
 | enough together that they are interleaved.                  |
 +-------------------------------------------------------------*/
int aux_rfxcomvl ( void )
{
   unsigned char  subindx, subtype;
   unsigned char  hcode, ucode, fcode, rfflood, addr, xmtr, rcvr;

   unsigned char  typeq[2], nbitsq[2], trulenq[2];
   unsigned char  *type, *nbits, *trulen;

   unsigned char  xbuffq[2][64];
   unsigned char  *xbuff, *buff;

   unsigned char  lastbuffq[2][64];
   unsigned char  *lastbuff;

   unsigned short sensorq[2];
   unsigned short *sensor;

   unsigned char  sensor_flagq[2];
   unsigned char  *sensor_flag;

   long           rfwordq[2];
   long           *rfword;

   int            countq[2];
   int            *count;

   int            mincountq[2];
   int            *mincount;

   int            repcountq[2];
   int            *repcount;

   int            nseqq[2];
   int            *nseq;

   int            bufflenq[2], lastlenq[2];
   int            *bufflen, *lastlen;

   int            use_old_bufferq[2];
   int            *use_old_buffer;

   unsigned long  saddrq[2];
   unsigned long  *saddr;


   int            nread = 0;
   int            is_idle = 0;
   int            is_err = 0;
   unsigned long  in_a_row, max_in_a_row;
   unsigned char  inbuffer[64];
//   int            inbufflen;

   char           restartfile[PATH_LEN + 1];
   struct stat    statbuf;

   extern void    aux_local_setup ( void );
   int            configure_rf_receiver ( unsigned char );
   int            proc_rfxsensor_init ( int, unsigned char *, int );
   int            send_rfxtype_message ( char, unsigned char );
   int            send_rfxsensor_ident ( unsigned short, unsigned char * );
   int            send_vl_noise ( unsigned char *, unsigned char );

   countq[0] = countq[1] = 0;
   lastlenq[0] = lastlenq[1] = -1;
   typeq[0] = typeq[1] = 0;
   subindx = 0;
   trulenq[0] = trulenq[1] = 0;
   nseqq[0] = nseqq[1] = 0;
   sensorq[0] = sensorq[1] = 0;
   sensor_flagq[0] = sensor_flagq[1] = 0;
   use_old_bufferq[0] = use_old_bufferq[1] = 0;
   saddrq[0] = saddrq[1] = 0;


   nread = 0;
   is_idle = 0;
   in_a_row = 0;
   rfflood = 0;
   max_in_a_row = config.aux_repcounts[MaxCount];
   
   strncpy2(restartfile, pathspec(RESTART_AUX_FILE), PATH_LEN);
   unlink(restartfile);

   openlog( "heyu_aux", 0, LOG_DAEMON);

   configure_rf_receiver(config.auxdev);


   while ( 1 )  {
      /* Check if restart needed */
      if ( is_idle && stat(restartfile, &statbuf) == 0 && is_err == 0 ) {
         if ( reread_config() != 0 /* || configure_rf_receiver(config.auxdev) != 0 */ ) {
            is_err = 1;
            syslog(LOG_ERR, "aux reconfiguration failed!\n");
            write_restart_error(restartfile);
         }
         else {
            syslog(LOG_ERR, "aux reconfiguration-\n");
            unlink(restartfile);
            return 0;
         }
      }

      if ( !use_old_bufferq[0] && !use_old_bufferq[1] && sxread(tty_aux, inbuffer, 1, 1) == 0 ) {
         is_idle = 1;
         nread = 0;
         countq[RFXMASTER] = countq[RFXSLAVE] = 0;
         in_a_row = 0;
         max_in_a_row = config.aux_repcounts[MaxCount];
         if ( max_in_a_row > 0 && rfflood > 0 )
            send_rf_flood_message(0); /* End of flood */
         rfflood = 0;
         inbuffer[0] = 0;
         continue;
      }


      xmtr =  (inbuffer[0] & 0x80u) ? 0xff : 0x00;    /* slave : master */
      rcvr =  (inbuffer[0] & 0x80u) ? RFXSLAVE : RFXMASTER;

      /* Set pointers to appropriate queue */
      xbuff = xbuffq[rcvr];
      buff = xbuff + 1;
      count = &countq[rcvr];
      mincount = &mincountq[rcvr];
      repcount = &repcountq[rcvr];
      bufflen = &bufflenq[rcvr];
      nbits = &nbitsq[rcvr];
      type = &typeq[rcvr];
      sensor_flag = &sensor_flagq[rcvr];
      use_old_buffer = &use_old_bufferq[rcvr];
      saddr = &saddrq[rcvr];
      sensor = &sensorq[rcvr];
      trulen = &trulenq[rcvr];
      nseq = &nseqq[rcvr];
      lastbuff = lastbuffq[rcvr];
      lastlen = &lastlenq[rcvr];
      rfword = &rfwordq[rcvr];

      xbuff[0] = inbuffer[0];
      *nbits = inbuffer[0] & 0x7fu;

      /* Round up to whole bytes */
      *bufflen = (*nbits + 7) / 8;

      while ( nread < *bufflen && sxread(tty_aux, buff + nread, 1, 1) ) {
         nread++;
      }

      if ( nread < *bufflen ) {
         is_idle = 1;
         nread = 0;
         countq[RFXMASTER] = countq[RFXSLAVE] = 0;
         inbuffer[0] = 0;
         in_a_row = 0;
         max_in_a_row = config.aux_repcounts[MaxCount];
         if ( max_in_a_row > 0 && rfflood > 0 )
            send_rf_flood_message(0); /* End of flood */
         rfflood = 0;
         continue;
      }
      is_idle = 0;

      /* Check for a continuous flood of RF inputs without a break */
      if ( max_in_a_row > 0 && ++in_a_row == max_in_a_row ) {
         rfflood = 1;
         send_rf_flood_message(SEC_FLOOD);
         max_in_a_row *= config.aux_repcounts[FloodRep];
         if ( max_in_a_row > ABSMAX_IN_A_ROW ) {
            max_in_a_row = ABSMAX_IN_A_ROW;
            in_a_row = 0;
         }
         continue;
      }

      *mincount = config.aux_repcounts[MinCount];
      *repcount = config.aux_repcounts[RepCount];

      if ( (rcvr == RFXSLAVE  && config.rfx_slave  == VISONIC) ||
           (rcvr == RFXMASTER && config.rfx_master == VISONIC)    ) {
         if ( *nbits == 0x29 && (buff[2] ^ buff[3]) == 0xffu ) {
            *saddr = (buff[4] << 16) | (buff[1] << 8) | buff[0];
            *type = RF_VISONIC;
            *mincount = 1;
         }
         else {
            if ( config.disp_raw_rf & DISPMODE_RF_NOISE )
               forward_variable_aux_data(RF_NOISEVL, xbuff, *bufflen + 1);
            nread = 0;
            continue;
         }
      }
      else if ( *nbits == 0x29 &&
            (buff[0] ^ buff[1]) == 0x0fu &&
            (buff[2] ^ buff[3]) == 0xffu &&
            security_checksum(buff[4], buff[5]) == 0  ) {
         /* Check for jamming signal (obsolete in newer RFXCOM) */
         if ( (buff[0] ^ xmtr) == 0xffu && (buff[2] == 0xf8u || buff[2] == 0xe0u) ) {
            if ( config.suppress_rfxjam == YES ) {
               /* Classify as noise */
               if ( config.disp_raw_rf & DISPMODE_RF_NOISE ) 
                 forward_variable_aux_data(RF_NOISEVL, xbuff, *bufflen + 1);
            }
            else {
               /* RFXCOM jamming signal */
               send_virtual_aux_data(0, buff[2], RF_XJAM, buff[0], 0, 0, 0);
            }
            *sensor_flag = 0;
            nread = 0;
            *use_old_buffer = 0;
            continue;
         }
         else {
            /* X10 Security RF (type 2) */
            *type = RF_SEC16;
            if ( config.disp_raw_rf & DISPMODE_RF_NORMAL ) {
               forward_variable_aux_data(RF_RAWVL, xbuff, *bufflen + 1);
            }
            *saddr = buff[4] << 8 | buff[0];
	    if ( configp->securid_parity == NO ) {
               /* Remove unreliable parity bit */
               buff[5] = 0;
	    }
            *sensor_flag = 0;
         }
      }
      else if ( *nbits == 0x30 &&
               (buff[0] ^ buff[1]) == 0xf0u &&
                rfxmeter_checksum(buff) == 0x00 ) {
         /* RFXMeter */
         *type = RF_XMETER;
         *sensor_flag = 0;
      }
      else if ( *nbits == 0x28 && strncmp((char *)buff, "SEN", 3) == 0 ) {
         /* RFXSensor ID and chip type */
         if ( config.disp_raw_rf & DISPMODE_RF_NORMAL )
            forward_variable_aux_data(RF_RAWVL, xbuff, *bufflen + 1);
         *sensor = buff[3] << 8 | buff[4];
         /* Next packet should be a sensor serial number */
         *sensor_flag = 1;
         nread = 0;
         *use_old_buffer = 0;
         continue;
      }
      else if ( *nbits == 0x30 && *sensor_flag ) {
         /* RFXSensor serial number */
         if ( config.disp_raw_rf & DISPMODE_RF_NORMAL )
            forward_variable_aux_data(RF_RAWVL, xbuff, *bufflen + 1);
         send_rfxsensor_ident(*sensor, buff);
         *sensor_flag = 0;
         nread = 0;
         *use_old_buffer = 0;
         continue;
      }
      else if ( *nbits ==0x2c && digimax_checksum(buff) == 0 ) {
         /* Digimax */
         *type = RF_DIGIMAX;
      }

      else if ( oretype(xbuff, &subindx, &subtype, trulen, saddr, nseq) == 0 ) {
         /* Oregon */
         if ( subtype == OreElec1 ) {
            *type = RF_ELECSAVE;
         }
         else if ( subtype == OreElec2 ) {
            *type = RF_OWL;
         }
         else if ( is_ore_ignored(*saddr) == 0 ) {
            *type = RF_OREGON;
         }
         else {
            *type = RF_OREGON;
            *use_old_buffer = 0;
            *count = 0;
            nread = 0;
            continue;
         }
      }

      else if ( *nbits == 0x21 && (buff[0] ^ buff[1]) == 0xffu && (buff[2] ^ buff[3]) == 0xff ) {
         /* X10 Standard RF from KR15A "Big Red Button" */
         *type = RF_STD;
      }
      else if ( *nbits == 34 || *nbits == 38 ) {
         *type = RF_KAKU;
      }
      else if ( *nbits != 0x20 ) {
//send_x10state_command(ST_TESTPOINT, 205);
         if ( config.disp_raw_rf & DISPMODE_RF_NOISE ) 
            forward_variable_aux_data(RF_NOISEVL, xbuff, *bufflen + 1);
         *sensor_flag = 0;
         nread = 0;
         *use_old_buffer = 0;
         continue;
      }
      else if ( (buff[0] ^ buff[1]) == 0xf0u && rfxsensor_checksum(buff) == 0 ) {
         /* RFXSensor RF */
         *type = RF_XSENSOR;
         *mincount = config.aux_mincount_rfx;
      }
      else if ( memcmp(buff, "RF", 2) == 0 && 
                (buff[2] == '3' || buff[2] == '2' || buff[2] == 'X') ) {
         *type = RF_XSENSORHDR;
         *mincount = config.aux_mincount_rfx;
      }
      else if ( (buff[2] ^ buff[3]) != 0xffu ) {
         if ( (*type = is_valid_w800_code(0)) == RF_NOISEW800 ) {
            /* Noise */
            if ( config.disp_raw_rf & DISPMODE_RF_NOISE )
               forward_variable_aux_data(RF_NOISEVL, xbuff, *bufflen + 1);
            nread = 0;
            continue;
         }
      }
      else if ( (buff[0] ^ buff[1]) == 0xffu ) {
         /* X10 Standard RF (type 0) or Entertainment RF (type 1) */
         *type = (buff[0] & 0x02) ? RF_ENT : RF_STD;
      }
      else if ( (buff[0] ^ buff[1]) == 0x0fu ) {
//send_x10state_command(ST_TESTPOINT, 207);
         /* At least one X10 Security device (SH624) sends only 32 bits */
         /* X10 Security RF */
         *type = RF_SEC;
         *saddr = buff[0];
      }
      else if ( (buff[0] ^ buff[1]) == 0xfeu ) {
         /* DM10 Motion sensor or GB10 Glass Break sensor */
         *type = RF_SEC;
         *saddr = buff[0];
      }
      else {
         if ( (*type = is_valid_w800_code(0)) == RF_NOISEW800 ) {
            /* Noise */
            if ( config.disp_raw_rf & DISPMODE_RF_NOISE )
               forward_variable_aux_data(RF_NOISEVL, xbuff, *bufflen + 1);
            nread = 0;
            continue;
         }
      }

      if ( (*type == RF_OREGON || *type == RF_ELECSAVE || *type == RF_OWL) && is_ore_ignored(*saddr) != 0 ) {
         *use_old_buffer = 1;
         nread = 0;
         continue;
      }

      if ( (*type == RF_SEC16 || *type == RF_SEC || *type == RF_SECX) && is_sec_ignored(*saddr) != 0 ) {
         *use_old_buffer = 0;
         *count = 0;
         nread = 0;
         continue;
      }

      if ( (config.disp_raw_rf & DISPMODE_RF_NORMAL) && !(*use_old_buffer) && *type != RF_SEC16 ) {
            forward_variable_aux_data(RF_RAWVL, xbuff, *bufflen + 1);
      }
      *use_old_buffer = 0;

      if ( *count == 0 ) {
         memcpy(lastbuff, buff, *bufflen);
         *lastlen = *bufflen;
         (*count)++;
         nread = 0;
         if ( *type == RF_STD ) {
            *rfword = (buff[0] << 8 | buff[2]);
            if ( rf_parms((int)(*rfword), &hcode, &ucode, &fcode) != 0 ) {
               if ( config.disp_raw_rf & DISPMODE_RF_NOISE )
                  send_virtual_aux_data(0, buff[0], RF_NOISEW800, buff[1], 0, buff[2], buff[3]);
               nread = 0;
               continue;
            }
            if ( *count == *mincount ) {
               proc_type_std(hcode, ucode, fcode);
            }
         }
         else if ( *type == RF_SEC16 ) {
            if ( *count == *mincount ) {
               send_virtual_aux_data(0, buff[2], RF_SEC, buff[0], buff[4], 0, 0);
            }
         }
         else if ( *type == RF_VISONIC ) {
            if ( *count == *mincount )
               send_virtual_aux_data(0, buff[2], *type, buff[0], buff[1], buff[4], 0);
         }
         else if ( *type == RF_SEC || *type == RF_ENT || *type == RF_XJAM ) {
            if ( *count == *mincount ) {
               send_virtual_aux_data(0, buff[2], *type, buff[0], 0, 0, 0);
            }
         }
         else if ( *type == RF_DUSK || *type == RF_SECX ) {
            if ( *count == *mincount ) {
               addr = rf2hcode[buff[0] >> 4] << 4;
//               send_virtual_aux_data(addr, buff[2], type, buff[0], 0, 0, 0);
               send_virtual_aux_data(buff[0], buff[2], *type, buff[0], 0, 0, 0);
            }
         }
         else if ( *type == RF_OREGON || *type == RF_ELECSAVE || *type == RF_OWL ) {
            if ( *count == *mincount ) {
              forward_gen_longdata (*type, subindx, *nseq, 0, *saddr, *trulen, buff);
            }
         }     
         else if ( *type == RF_KAKU ) {
            if ( *count == *mincount )
               forward_variable_aux_data(RF_KAKU, xbuff, *bufflen + 1);
         }
         else if ( *type == RF_DIGIMAX ) {
            if ( *count == *mincount ) {
               forward_digimax(buff);
            }
         }
         else if ( *type == RF_XSENSOR ) {
            if ( *count == *mincount ) {
               send_virtual_aux_data(0, 0, *type, buff[0], 0, buff[2], buff[3]);
            }
         }
         else if ( *type == RF_XMETER ) {
            if ( *count == *mincount ) {
               forward_aux_longdata(*type, 0, 0, buff[0], 6, buff);
            }
         }
         else if ( *type == RF_XSENSORHDR ) {
            if ( *count == *mincount ) {
               send_rfxtype_message((char)buff[2], buff[3]);
            }
         }
         else if ( *count == *mincount ) {
            send_virtual_aux_data(0, buff[0], *type, buff[1], 0, buff[2], buff[3]);
         }
      }
      else if ((*bufflen == *lastlen ||
              (*type == RF_STD && (*lastlen == 4 || (*lastlen = *bufflen) == 4))
                                  ) && memcmp(buff, lastbuff, *lastlen) == 0 ) {
         /* Repeat of previous burst */
         (*count)++;
         nread = 0;
         if ( *repcount /*config.aux_repcounts[RepCount]*/ > *mincount &&
              (*count % *repcount /*config.aux_repcounts[RepCount]*/) == *mincount &&
              (config.aux_repcounts[MaxCount] == 0 ||
               *count < config.aux_repcounts[MaxCount]) ) {
            if ( *type == RF_STD ) {
               *rfword = (buff[0] << 8 | buff[2]);
               if ( rf_parms((int)(*rfword), &hcode, &ucode, &fcode) != 0 ) {
                  if ( config.disp_raw_rf & DISPMODE_RF_NOISE )
                     send_virtual_aux_data(0, buff[0], RF_NOISEW800, buff[1], 0, buff[2], buff[3]);
                  nread = 0;
                  continue;
               }
               proc_type_std(hcode, ucode, fcode);
            }
            else if ( *type == RF_SEC16 ) {
               send_virtual_aux_data(0, buff[2], RF_SEC, buff[0], buff[4], 0, 0);
            }
            else if ( *type == RF_VISONIC ) {
               send_virtual_aux_data(0, buff[2], *type, buff[0], buff[1], buff[4], 0);
            }

            else if ( *type == RF_SEC || *type == RF_ENT || *type == RF_XJAM ) {
               send_virtual_aux_data(0, buff[2], *type, buff[0], 0, 0, 0);
            }
            else if ( *type == RF_DUSK || *type == RF_SECX ) {
               addr = rf2hcode[buff[0] >> 4] << 4;
//               send_virtual_aux_data(addr, buff[2], *type, buff[0], 0, 0, 0);
               send_virtual_aux_data(buff[0], buff[2], *type, buff[0], 0, 0, 0);
            }
            else if ( *type == RF_OREGON || *type == RF_ELECSAVE || *type == RF_OWL ) {
               forward_gen_longdata (*type, subindx, *nseq, 0, *saddr, *trulen, buff);
            }
            else if ( *type == RF_KAKU ) {
               forward_variable_aux_data(RF_KAKU, xbuff, *bufflen + 1);
            }
            else if ( *type == RF_DIGIMAX ) {
               forward_digimax(buff);
            }
            else if ( *type == RF_XSENSOR ) {
               send_virtual_aux_data(0, 0, *type, buff[0], 0, buff[2], buff[3]);
            }
            else if ( *type == RF_XMETER ) {
               forward_aux_longdata(*type, 0, 0, buff[0], 6, buff);
            }
            else if ( *type == RF_XSENSORHDR ) {
               send_rfxtype_message((char)buff[2], buff[3]);
            }
            else {
               send_virtual_aux_data(0, buff[0], *type, buff[1], 0, buff[2], buff[3]);
            }
         }
      }
      else {
         /* New burst, save buffer */
         *use_old_buffer = 1;
         *count = 0;
      }
   }
   return 0;
}

#else
/*-------------------------------------------------------------+
 | Process data read from the RFXCOM receiver in variable      |
 | length message mode on the TTY_AUX serial port and send to  |
 | spoolfile and/or interface per config.                      |
 +-------------------------------------------------------------*/
int aux_rfxcomvl ( void )
{
   unsigned char  type, subindx, subtype, trulen;
   unsigned char  hcode, ucode, fcode, rfflood, nbits, addr, xmtr, rcvr;
   unsigned char  xbuff[64], *buff;
   unsigned char  lastbuff[64];
   unsigned short sensor = 0;
   unsigned long  saddr;
//   unsigned int   visaddr;
   unsigned char  sensor_flag = 0;
   unsigned long  in_a_row, max_in_a_row;
   long           rfword;
   int            count = 0;
   int            mincount;
   int            nseq;
   int            bufflen, lastlen, nread = 0;
   int            is_idle = 0;
   int            is_err = 0;
   int            use_old_buffer = 0;
   char           restartfile[PATH_LEN + 1];
   struct stat    statbuf;
   extern void    aux_local_setup ( void );
   int            configure_rf_receiver ( unsigned char );
   int            proc_rfxsensor_init ( int, unsigned char *, int );
   int            send_rfxtype_message ( char, unsigned char );
   int            send_rfxsensor_ident ( unsigned short, unsigned char * );
   int            send_vl_noise ( unsigned char *, unsigned char );



   buff = xbuff + 1;

   lastlen = -1;
   type = 0;
   subindx = 0;
   trulen = 0;
   nseq = 0;
   nread = 0;
   is_idle = 0;
   in_a_row = 0;
   rfflood = 0;
   max_in_a_row = config.aux_repcounts[MaxCount];
   
   strncpy2(restartfile, pathspec(RESTART_AUX_FILE), PATH_LEN);
   unlink(restartfile);

   openlog( "heyu_aux", 0, LOG_DAEMON);

   configure_rf_receiver(config.auxdev);


   while ( 1 )  {
      /* Check if restart needed */
      if ( is_idle && stat(restartfile, &statbuf) == 0 && is_err == 0 ) {
         if ( reread_config() != 0 /* || configure_rf_receiver(config.auxdev) != 0 */ ) {
            is_err = 1;
            syslog(LOG_ERR, "aux reconfiguration failed!\n");
            write_restart_error(restartfile);
         }
         else {
            syslog(LOG_ERR, "aux reconfiguration-\n");
            unlink(restartfile);
            return 0;
         }
      }

      if ( !use_old_buffer && sxread(tty_aux, xbuff, 1, 1) == 0 ) {
         is_idle = 1;
         nread = 0;
         count = 0;
         in_a_row = 0;
         max_in_a_row = config.aux_repcounts[MaxCount];
         if ( max_in_a_row > 0 && rfflood > 0 )
            send_rf_flood_message(0); /* End of flood */
         rfflood = 0;
         xbuff[0] = 0;
         continue;
      }


      nbits = xbuff[0] & 0x7fu;
      xmtr =  (xbuff[0] & 0x80u) ? 0xff : 0x00;    /* slave : master */
      rcvr =  (xbuff[0] & 0x80u) ? RFXSLAVE : RFXMASTER;

      /* Round up to whole bytes */
      bufflen = (nbits + 7) / 8;

      while ( nread < bufflen && sxread(tty_aux, buff + nread, 1, 1) ) {
         nread++;
      }

      if ( nread < bufflen ) {
         is_idle = 1;
         nread = 0;
         count = 0;
         xbuff[0] = 0;
         in_a_row = 0;
         max_in_a_row = config.aux_repcounts[MaxCount];
         if ( max_in_a_row > 0 && rfflood > 0 )
            send_rf_flood_message(0); /* End of flood */
         rfflood = 0;
         continue;
      }
      is_idle = 0;

      /* Check for a continuous flood of RF inputs without a break */
      if ( max_in_a_row > 0 && ++in_a_row == max_in_a_row ) {
         rfflood = 1;
         send_rf_flood_message(SEC_FLOOD);
         max_in_a_row *= config.aux_repcounts[FloodRep];
         if ( max_in_a_row > ABSMAX_IN_A_ROW ) {
            max_in_a_row = ABSMAX_IN_A_ROW;
            in_a_row = 0;
         }
         continue;
      }

      mincount = config.aux_repcounts[MinCount];

      if ( (rcvr == RFXSLAVE  && config.rfx_slave  == VISONIC) ||
           (rcvr == RFXMASTER && config.rfx_master == VISONIC)    ) {
         if ( nbits == 0x29 && (buff[2] ^ buff[3]) == 0xffu ) {
            saddr = (buff[4] << 16) | (buff[1] << 8) | buff[0];
            type = RF_VISONIC;
            mincount = 1;
         }
         else {
            if ( config.disp_raw_rf & DISPMODE_RF_NOISE )
               forward_variable_aux_data(RF_NOISEVL, xbuff, bufflen + 1);
            nread = 0;
            continue;
         }
      }
      else if ( nbits == 0x29 &&
            (buff[0] ^ buff[1]) == 0x0fu &&
            (buff[2] ^ buff[3]) == 0xffu &&
            security_checksum(buff[4], buff[5]) == 0  ) {
         /* Check for jamming signal (obsolete in newer RFXCOM) */
         if ( (buff[0] ^ xmtr) == 0xffu && (buff[2] == 0xf8u || buff[2] == 0xe0u) ) {
            if ( config.suppress_rfxjam == YES ) {
               /* Classify as noise */
               if ( config.disp_raw_rf & DISPMODE_RF_NOISE ) 
                 forward_variable_aux_data(RF_NOISEVL, xbuff, bufflen + 1);
            }
            else {
               /* RFXCOM jamming signal */
               send_virtual_aux_data(0, buff[2], RF_XJAM, buff[0], 0, 0, 0);
            }
            sensor_flag = 0;
            nread = 0;
            use_old_buffer = 0;
            continue;
         }
         else {
            /* X10 Security RF (type 2) */
            type = RF_SEC16;
            if ( config.disp_raw_rf & DISPMODE_RF_NORMAL ) {
               forward_variable_aux_data(RF_RAWVL, xbuff, bufflen + 1);
            }
            saddr = buff[4] << 8 | buff[0];
	    if ( configp->securid_parity == NO ) {
               /* Remove unreliable parity bit */
               buff[5] = 0;
	    }
            sensor_flag = 0;
         }
      }
      else if ( nbits == 0x30 &&
               (buff[0] ^ buff[1]) == 0xf0u &&
                rfxmeter_checksum(buff) == 0x00 ) {
         /* RFXMeter */
         type = RF_XMETER;
         sensor_flag = 0;
      }
      else if ( nbits == 0x28 && strncmp((char *)buff, "SEN", 3) == 0 ) {
         /* RFXSensor ID and chip type */
         if ( config.disp_raw_rf & DISPMODE_RF_NORMAL )
            forward_variable_aux_data(RF_RAWVL, xbuff, bufflen + 1);
         sensor = buff[3] << 8 | buff[4];
         /* Next packet should be a sensor serial number */
         sensor_flag = 1;
         nread = 0;
         use_old_buffer = 0;
         continue;
      }
      else if ( nbits == 0x30 && sensor_flag ) {
         /* RFXSensor serial number */
         if ( config.disp_raw_rf & DISPMODE_RF_NORMAL )
            forward_variable_aux_data(RF_RAWVL, xbuff, bufflen + 1);
         send_rfxsensor_ident(sensor, buff);
         sensor_flag = 0;
         nread = 0;
         use_old_buffer = 0;
         continue;
      }
      else if ( nbits ==0x2c && digimax_checksum(buff) == 0 ) {
         /* Digimax */
         type = RF_DIGIMAX;
      }

      else if ( oretype(xbuff, &subindx, &subtype, &trulen, &saddr, &nseq) == 0 ) {
         /* Oregon */
         if ( subtype == OreElec1 ) {
            type = RF_ELECSAVE;
         }
         else if ( subtype == OreElec2 ) {
            type = RF_OWL;
         }
         else if ( is_ore_ignored(saddr) == 0 ) {
            type = RF_OREGON;
         }
         else {
            type = RF_OREGON;
            use_old_buffer = 0;
            count = 0;
            nread = 0;
            continue;
         }
      }

      else if ( nbits == 0x21 && (buff[0] ^ buff[1]) == 0xffu && (buff[2] ^ buff[3]) == 0xff ) {
         /* X10 Standard RF from KR15A "Big Red Button" */
         type = RF_STD;
      }
      else if ( nbits == 34 || nbits == 38 ) {
         type = RF_KAKU;
      }
      else if ( nbits != 0x20 ) {
         if ( config.disp_raw_rf & DISPMODE_RF_NOISE ) 
            forward_variable_aux_data(RF_NOISEVL, xbuff, bufflen + 1);
         sensor_flag = 0;
         nread = 0;
         use_old_buffer = 0;
         continue;
      }
      else if ( (buff[0] ^ buff[1]) == 0xf0u && rfxsensor_checksum(buff) == 0 ) {
         /* RFXSensor RF */
         type = RF_XSENSOR;
         mincount = config.aux_mincount_rfx;
      }
      else if ( memcmp(buff, "RF", 2) == 0 && 
                (buff[2] == '3' || buff[2] == '2' || buff[2] == 'X') ) {
         type = RF_XSENSORHDR;
         mincount = config.aux_mincount_rfx;
      }
      else if ( (buff[2] ^ buff[3]) != 0xffu ) {
         if ( (type = is_valid_w800_code(0 /*this_word*/)) == RF_NOISEW800 ) {
            /* Noise */
            if ( config.disp_raw_rf & DISPMODE_RF_NOISE )
               forward_variable_aux_data(RF_NOISEVL, xbuff, bufflen + 1);
            nread = 0;
            continue;
         }
      }
      else if ( (buff[0] ^ buff[1]) == 0xffu ) {
         /* X10 Standard RF (type 0) or Entertainment RF (type 1) */
         type = (buff[0] & 0x02) ? RF_ENT : RF_STD;
      }
      else if ( (buff[0] ^ buff[1]) == 0x0fu ) {
         /* At least one X10 Security device (SH624) sends only 32 bits */
         /* X10 Security RF */
         type = RF_SEC;
         saddr = buff[0];
      }
      else if ( (buff[0] ^ buff[1]) == 0xfeu ) {
         /* DM10 Motion sensor or GB10 Glass Break sensor */
         type = RF_SEC;
         saddr = buff[0];
      }
#if 0
      else if ( (buff[0] ^ buff[1]) == 0xfeu && (buff[2] & 0xf0u) == 0xf0u ) {
         /* GB10 Dusk sensor */
         type = RF_DUSK;
//         this_word = (buff[0] << 8 | buff[2]);
      }
      else if ( (buff[0] ^ buff[1]) == 0xfeu && buff[2] == 0xe0u ) {
         /* MD10 Motion sensor */
         type = RF_SECX;
         saddr = buff[0];
//         this_word = (buff[0] << 8 | buff[2]);
      }
#endif
      else {
         if ( (type = is_valid_w800_code(0 /*this_word*/)) == RF_NOISEW800 ) {
            /* Noise */
            if ( config.disp_raw_rf & DISPMODE_RF_NOISE )
               forward_variable_aux_data(RF_NOISEVL, xbuff, bufflen + 1);
            nread = 0;
            continue;
         }
      }

      if ( (type == RF_OREGON || type == RF_ELECSAVE || type == RF_OWL) && is_ore_ignored(saddr) != 0 ) {
         use_old_buffer = 1;
         nread = 0;
         continue;
      }

      if ( (type == RF_SEC16 || type == RF_SEC || type == RF_SECX) && is_sec_ignored(saddr) != 0 ) {
         use_old_buffer = 0;
         count = 0;
         nread = 0;
         continue;
      }

      if ( (config.disp_raw_rf & DISPMODE_RF_NORMAL) && !use_old_buffer && type != RF_SEC16 ) {
            forward_variable_aux_data(RF_RAWVL, xbuff, bufflen + 1);
      }
      use_old_buffer = 0;

      if ( count == 0 ) {
         memcpy(lastbuff, buff, bufflen);
         lastlen = bufflen;
         count++;
         nread = 0;
         if ( type == RF_STD ) {
            rfword = (buff[0] << 8 | buff[2]);
            if ( rf_parms((int)rfword, &hcode, &ucode, &fcode) != 0 ) {
               if ( config.disp_raw_rf & DISPMODE_RF_NOISE )
                  send_virtual_aux_data(0, buff[0], RF_NOISEW800, buff[1], 0, buff[2], buff[3]);
               nread = 0;
               continue;
            }
            if ( count == mincount /*config.aux_repcounts[MinCount]*/ ) {
               proc_type_std(hcode, ucode, fcode);
            }
         }
         else if ( type == RF_SEC16 ) {
            if ( count == mincount /*config.aux_repcounts[MinCount]*/ ) {
               send_virtual_aux_data(0, buff[2], RF_SEC, buff[0], buff[4], 0, 0);
            }
         }
         else if ( type == RF_SEC || type == RF_ENT || type == RF_XJAM ) {
            if ( count == mincount /*config.aux_repcounts[MinCount]*/ ) {
               send_virtual_aux_data(0, buff[2], type, buff[0], 0, 0, 0);
            }
         }
         else if ( type == RF_DUSK || type == RF_SECX ) {
            if ( count == mincount /*config.aux_repcounts[MinCount]*/ ) {
               addr = rf2hcode[buff[0] >> 4] << 4;
//               send_virtual_aux_data(addr, buff[2], type, buff[0], 0, 0, 0);
               send_virtual_aux_data(buff[0], buff[2], type, buff[0], 0, 0, 0);
            }
         }
         else if ( type == RF_OREGON || type == RF_ELECSAVE || type == RF_OWL ) {
            if ( count == mincount ) {
              forward_gen_longdata (type, subindx, nseq, 0, (unsigned char)saddr, trulen, buff);
            }
         }     
         else if ( type == RF_KAKU ) {
            if ( count == mincount )
               forward_variable_aux_data(RF_KAKU, xbuff, bufflen + 1);
         }
         else if ( type == RF_VISONIC ) {
            if ( count == mincount )
               send_virtual_aux_data(0, buff[2], type, buff[0], buff[1], buff[4], 0);
         }
         else if ( type == RF_DIGIMAX ) {
            if ( count == mincount ) {
               forward_digimax(buff);
            }
         }
         else if ( type == RF_XSENSOR ) {
            if ( count == mincount /*config.aux_mincount_rfx*/ ) {
               send_virtual_aux_data(0, 0, type, buff[0], 0, buff[2], buff[3]);
            }
         }
         else if ( type == RF_XMETER ) {
            if ( count == mincount ) {
               forward_aux_longdata(type, 0, 0, buff[0], 6, buff);
            }
         }
         else if ( type == RF_XSENSORHDR ) {
            if ( count == mincount /*config.aux_mincount_rfx*/ ) {
               send_rfxtype_message((char)buff[2], buff[3]);
            }
         }
         else if ( count == mincount /*config.aux_repcounts[MinCount]*/ ) {
            send_virtual_aux_data(0, buff[0], type, buff[1], 0, buff[2], buff[3]);
         }
      }
      else if ((bufflen == lastlen ||
                (type == RF_STD && (lastlen == 4 || (lastlen = bufflen) == 4))
                                    ) && memcmp(buff, lastbuff, lastlen) == 0) {
         /* Repeat of previous burst */
         count++;
         nread = 0;
         if ( config.aux_repcounts[RepCount] > mincount &&
              (count % config.aux_repcounts[RepCount]) == mincount &&
              (config.aux_repcounts[MaxCount] == 0 ||
               count < config.aux_repcounts[MaxCount]) ) {
            if ( type == RF_STD ) {
               rfword = (buff[0] << 8 | buff[2]);
               if ( rf_parms((int)rfword, &hcode, &ucode, &fcode) != 0 ) {
                  if ( config.disp_raw_rf & DISPMODE_RF_NOISE )
                     send_virtual_aux_data(0, buff[0], RF_NOISEW800, buff[1], 0, buff[2], buff[3]);
                  nread = 0;
                  continue;
               }
               proc_type_std(hcode, ucode, fcode);
            }
            else if ( type == RF_SEC16 ) {
               send_virtual_aux_data(0, buff[2], RF_SEC, buff[0], buff[4], 0, 0);
            }
            else if ( type == RF_SEC || type == RF_ENT || type == RF_XJAM ) {
               send_virtual_aux_data(0, buff[2], type, buff[0], 0, 0, 0);
            }
            else if ( type == RF_DUSK || type == RF_SECX ) {
               addr = rf2hcode[buff[0] >> 4] << 4;
//               send_virtual_aux_data(addr, buff[2], type, buff[0], 0, 0, 0);
               send_virtual_aux_data(buff[0], buff[2], type, buff[0], 0, 0, 0);
            }
            else if ( type == RF_OREGON || type == RF_ELECSAVE || type == RF_OWL ) {
               forward_gen_longdata (type, subindx, nseq, 0, (unsigned char)saddr, trulen, buff);
            }
            else if ( type == RF_KAKU ) {
               forward_variable_aux_data(RF_KAKU, xbuff, bufflen + 1);
            }
            else if ( type == RF_VISONIC ) {
               send_virtual_aux_data(0, buff[2], type, buff[0], buff[1], buff[4], 0);
            }
            else if ( type == RF_DIGIMAX ) {
               forward_digimax(buff);
            }
            else if ( type == RF_XSENSOR ) {
               send_virtual_aux_data(0, 0, type, buff[0], 0, buff[2], buff[3]);
            }
            else if ( type == RF_XMETER ) {
               forward_aux_longdata(type, 0, 0, buff[0], 6, buff);
            }
            else if ( type == RF_XSENSORHDR ) {
               send_rfxtype_message((char)buff[2], buff[3]);
            }
            else {
               send_virtual_aux_data(0, buff[0], type, buff[1], 0, buff[2], buff[3]);
            }
         }
      }
      else {
         /* New burst, save buffer */
         use_old_buffer = 1;
         count = 0;
      }
   }
   return 0;
}
#endif  /* RFXCOM_DUAL */

/*-------------------------------------------------------------+
 | Process data read from the MR26A on the TTY_AUX serial      |
 | port and send to spoolfile and/or interface per config.     |
 +-------------------------------------------------------------*/
int aux_mr26a ( void )
{
   unsigned char type;
   unsigned char hcode, ucode, fcode, rfflood;
   unsigned char buff[8];
   unsigned long in_a_row, max_in_a_row;
   long          rfword, last_word, this_word;
   int           k;
   int           count = 0;
   int           nread = 0;
   int           is_idle = 0;
   int           is_err = 0;
   int           use_old_buffer = 0;
   char          restartfile[PATH_LEN + 1];
   struct stat   statbuf;
   extern void aux_local_setup(void);

   rfword = last_word = -1;
   type = 0;
   nread = 0;
   is_idle = 0;
   in_a_row = 0;
   rfflood = 0;
   max_in_a_row = config.aux_repcounts[MaxCount];

   strncpy2(restartfile, pathspec(RESTART_AUX_FILE), PATH_LEN);
   unlink(restartfile);
   openlog( "heyu_aux", 0, LOG_DAEMON);

   while ( 1 )  {
      /* Check if restart needed */
      if ( is_idle && stat(restartfile, &statbuf) == 0 && is_err == 0 ) {
         if ( reread_config() != 0 ) {
            is_err = 1;
            syslog(LOG_ERR, "aux reconfiguration failed!\n");
            write_restart_error(restartfile);
         }
         else {
            syslog(LOG_ERR, "aux reconfiguration-\n");
            unlink(restartfile);
            return 0;
         }
      }

      while ( nread < 5 && sxread(tty_aux, buff + nread, 1, 1) ) {
         nread++;
      }

      if ( nread < 5 ) {
         is_idle = 1;
         nread = 0;
         count = 0;
         in_a_row = 0;
         max_in_a_row = config.aux_repcounts[MaxCount];
         if ( rfflood > 0 )
            send_rf_flood_message(0);
         rfflood = 0;
         continue;
      }
      is_idle = 0;

      /* Check for a continuous flood of RF inputs without a break */
      if ( ++in_a_row == max_in_a_row ) {
         rfflood = 1;
         send_rf_flood_message(SEC_FLOOD);
         max_in_a_row *= config.aux_repcounts[FloodRep];
         if ( max_in_a_row > ABSMAX_IN_A_ROW ) {
            max_in_a_row = ABSMAX_IN_A_ROW;
            in_a_row = 0;
         }
         continue;
      }

      /* Check for correct frame characters */
      if ( buff[0] != 0xD5u || buff[1] != 0xAAu || buff[4] != 0xADu ) {
         /* Alignment error - try to realign */
         type = RF_NOISEMR26;
         if ( config.disp_raw_rf & DISPMODE_RF_NOISE && rfflood == 0 )
            send_virtual_aux_data(buff[0], buff[1], RF_NOISEMR26, buff[2], 0, buff[3], buff[4]);
         nread--;
         for ( k = 1; k < 5; k++ ) {
            if ( buff[k] == 0xD5u ) {
               memmove(buff, buff + k, nread);
               break;
            }
            nread--;
         }
         continue;
      }

      /* X10 Standard RF (type 0) or Entertainment RF (type 1) */
      type = (buff[2] & 0x02) ? RF_ENT : RF_STD;
      this_word = (buff[2] << 8 | buff[3]);

      /* Could use a more sophisticated validation step here */
      if ( type == RF_STD && (this_word & NOTX10_MASK) ) {
         if ( config.disp_raw_rf & DISPMODE_RF_NOISE && rfflood == 0 )
            send_virtual_aux_data(buff[0], buff[1], RF_NOISEMR26, buff[2], 0, buff[3], buff[4]);
         nread = 0;
         continue;
      }

      if ( (config.disp_raw_rf & DISPMODE_RF_NORMAL) && !use_old_buffer && rfflood == 0 ) {
         send_virtual_aux_data(buff[0], buff[1], RF_RAWMR26, buff[2], 0, buff[3], buff[4]);
      }
      use_old_buffer = 0;

      if ( count == 0 ) {
         rfword = this_word;
         count++;
         nread = 0;
         if ( type == RF_STD ) {
            rf_parms((int)rfword, &hcode, &ucode, &fcode);
            if ( count == config.aux_repcounts[MinCount] ) {
               proc_type_std(hcode, ucode, fcode);
            }
         }
         else  {
            if ( count == config.aux_repcounts[MinCount] ) {
               send_virtual_aux_data(0, buff[3], type, buff[2], 0, 0, 0);
            }
         }
      }
      else if ( this_word == rfword ) {
         /* Repeat of previous burst */
         count++;
         nread = 0;
         if ( config.aux_repcounts[RepCount] > config.aux_repcounts[MinCount] &&
              (count % config.aux_repcounts[RepCount]) == config.aux_repcounts[MinCount] &&
               count < config.aux_repcounts[MaxCount] ) {
            if ( type == RF_STD ) {
               rf_parms((int)rfword, &hcode, &ucode, &fcode);
               proc_type_std(hcode, ucode, fcode);
            }
            else {
               send_virtual_aux_data(0, buff[3], type, buff[2], 0, 0, 0);
            }
         }
      }
      else {
         /* New burst, save buffer */
         use_old_buffer = 1;
         count = 0;
      }
   }
   return 0;
}

/*-------------------------------------------------------------+
 | Verify a W800RF32 is connected.  When sent the two bytes    |
 | 0xF0, 0x29, it should ack with 0x29.                        |
 +-------------------------------------------------------------*/
int verify_w800 ( void )
{
   unsigned char inbuf[8];
   unsigned char outbuf[4];
   int j, nread = 0;

   int ignoret;

   outbuf[0] = 0xf0;
   outbuf[1] = 0x29;


   /* Empty the serial port buffers */
   while ( sxread(tty_aux, inbuf, 1, 1) > 0 );

   for ( j = 0; j < 3; j++ ) {
      ignoret = write(tty_aux, (char *)outbuf, 2);
      if ( (nread = sxread(tty_aux, inbuf, 1, 1)) > 0 &&
           inbuf[0] == 0x29u ) {
         syslog(LOG_ERR, "W800RF32 detected-\n");
         return 0;
      }
   }

   syslog(LOG_ERR, "W800RF32 not detected-\n");
   return 1;
}


