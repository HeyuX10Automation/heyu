
/*----------------------------------------------------------------------------+
 |                                                                            |
 |              RFXSensor and RFXMeter Support for HEYU                       |
 |                Copyright 2008 Charles W. Sullivan                          |
 |                      All Rights Reserved                                   |
 |                                                                            |
 |                                                                            |
 | This software is licensed free of charge for non-commercial distribution   |
 | and for personal and internal business use only.  Inclusion of this        |
 | software or any part thereof in a commercial product is prohibited         |
 | without the prior written permission of the author.  You may copy, use,    |
 | and distribute this software subject to the following restrictions:        |
 |                                                                            |
 |  1)	You may not charge money for it.                                      |
 |  2)	You may not remove or alter this license, copyright notice, or the    |
 |      included disclaimers.                                                 |
 |  3)	You may not claim you wrote it.                                       |
 |  4)	If you make improvements (or other changes), you are requested        |
 |	to send them to the Heyu maintainer so there's a focal point for      |
 |      distributing improved versions.                                       |
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
 | Disclaimers:                                                               |
 | THERE IS NO ASSURANCE THAT THIS SOFTWARE IS FREE OF DEFECTS AND IT MUST    |
 | NOT BE USED IN ANY SITUATION WHERE THERE IS ANY CHANCE THAT ITS            |
 | PERFORMANCE OR FAILURE TO PERFORM AS EXPECTED COULD RESULT IN LOSS OF      |
 | LIFE, INJURY TO PERSONS OR PROPERTY, FINANCIAL LOSS, OR LEGAL LIABILITY.   |
 |                                                                            |
 | TO THE EXTENT ALLOWED BY APPLICABLE LAW, THIS SOFTWARE IS PROVIDED "AS IS",|
 | WITH NO EXPRESS OR IMPLIED WARRANTY, INCLUDING, BUT NOT LIMITED TO, THE    |
 | IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.|
 |                                                                            |
 | IN NO EVENT UNLESS REQUIRED BY APPLICABLE LAW WILL THE AUTHOR BE LIABLE    |
 | FOR DAMAGES, INCLUDING ANY GENERAL, SPECIAL, INCIDENTAL OR CONSEQUENTIAL   |
 | DAMAGES ARISING OUT OF THE USE OR INABILITY TO USE THIS SOFTWARE EVEN IF   |
 | THE AUTHOR HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.            |
 |                                                                            |
 +----------------------------------------------------------------------------*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#endif
#include <ctype.h>
#if defined(SYSV) || defined(FREEBSD) || defined(OPENBSD)
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif

#include "x10.h"
#include "process.h"
#include "x10state.h"
#include "rfxcom.h"

extern int sptty;
extern int i_am_state, i_am_aux, i_am_relay, i_am_rfxmeter;
extern char *funclabel[];
extern void create_flagslist ( unsigned char, unsigned long, char * );

extern CONFIG config;
extern CONFIG *configp;

extern struct x10global_st x10global;
extern x10hcode_t *x10state;

extern unsigned int modmask[NumModMasks][16];
extern int lock_for_write(), munlock();
extern void update_activity_timeout ( ALIAS *, int );

/*---------------------------------------------------------------------+
 | Send the RFXSensor initialization type message to the spool file.   |
 +---------------------------------------------------------------------*/
int send_rfxtype_message ( char rfxtype, unsigned char code )
{
   extern int sptty;
   char writefilename[PATH_LEN + 1];

   int ignoret;

   static unsigned char template[15] = {
      0xff,0xff,0xff,3,ST_COMMAND,ST_SOURCE,D_AUXRCV,
      0xff, 0xff, 0xff, 4, ST_COMMAND, ST_RFXTYPE, 0, 0
   };

   sprintf(writefilename, "%s%s", WRITEFILE, configp->suffix);

   if ( lock_for_write() < 0 ) {
      syslog(LOG_ERR, "aux unable to lock writefile\n");
      return 1;
   }

   template[13] = (rfxtype == 'X') ? (unsigned char)'1' : (unsigned char)rfxtype;
   template[14] = code;

   ignoret = write(sptty, (char *)template, sizeof(template));

   munlock(writefilename);

   return 0;
}

/*---------------------------------------------------------------------+
 | Translate the RFXSensor initialization type message.                |
 +---------------------------------------------------------------------*/
char *translate_rfxtype_message( unsigned char *buf )
{
   static char outbuf[80];

#ifdef HASRFXS
   sprintf(outbuf, "RFXSensorXmitter : Type %c, version %d, sample_mode %s",
     (char)buf[2], buf[3] & 0x7fu, (buf[3] & 0x80u ? "slow" : "fast"));
#else
   sprintf(outbuf, "RFXSensorXmitter : Heyu support not configured.");
#endif

   return outbuf;
}


/*---------------------------------------------------------------------+
 | Send the RFXSensor initialization serial message to the spool file. |
 +---------------------------------------------------------------------*/
int send_rfxsensor_ident ( unsigned short sensor, unsigned char *serial )
{
   extern int sptty;

   int ignoret;

   static unsigned char template[14] = {
      0xff, 0xff, 0xff, 10, ST_COMMAND, ST_RFXSERIAL,
      0, 0,  0, 0, 0, 0, 0, 0
   };

   template[6] = (sensor >> 8) & 0xffu;
   template[7] = sensor & 0xffu;  
   memcpy (template + 8, serial, 6);
   ignoret = write(sptty, (char *)template, sizeof(template));

   return 0;
}

/*---------------------------------------------------------------------+
 |  | Translate the RFXSensor initialization serial message.           |
 +---------------------------------------------------------------------*/
char *translate_rfxsensor_ident ( unsigned char *buf )
{
   static char outbuf[80];
#ifdef HASRFXS
   unsigned char addr;
   unsigned char chip;
   unsigned long serial = 0;
   int j;

   addr = buf[2];
   chip = buf[3];

   for ( j = 0; j < 6; j++ ) {
      serial |= (unsigned long)(buf[j + 4] << (8 * j));
   }

   sprintf(outbuf, "RFXSensorInit    : ID 0x%02X, chip %s, serial_no %lu",
      addr, ((chip == 0x26) ? "DS2438" : (chip == 0x28) ? "DS18B20" : "???"), serial );
#else
   sprintf(outbuf, "RFXSensorInit    : Heyu support not configured.");
#endif /* HASRFXS */

   return outbuf;
}


#ifdef HASRFXS
/*---------------------------------------------------------------------+
 | Convert RFXSensor temperature short word to scaled double           |
 | temperature.                                                        |
 +---------------------------------------------------------------------*/
void rfxdata2temp ( unsigned short tdata, double *tempc, double *temperature)
{
   tdata &= 0xffe0u;
   if ( tdata & 0x8000u ) {
      /* Negative temperature */
      tdata = (~tdata + 1) & 0xffffu;
      *tempc = -(double)tdata / 256.0;
   }
   else {
      *tempc = (double)tdata / 256.0;
   }

   switch ( configp->rfx_tscale ) {
      case 'F' :
         /* Fahrenheit */
         *temperature = 1.8 * (*tempc) + 32.0;
         break;
      case 'R' :
         /* Rankine */
         *temperature = 1.8 * (*tempc) + 32.0 + 459.67;
         break;
      case 'K' :
         /* Kelvin */
         *temperature = *tempc + 273.15;
         break;
      default :
         /* Celsius */
         *temperature = *tempc;
         break;
   }
   *temperature += configp->rfx_toffset;

   return;
}

/*---------------------------------------------------------------------+
 | Convert RFXSensor a/d voltage to scaled double voltage.             |
 +---------------------------------------------------------------------*/
void rfxdata2volt ( unsigned short vdata, double *vad, double *voltage )
{
   *vad = 0.010 * (double)((vdata & 0xffe0u) >> 5);
   *voltage = (*vad * configp->rfx_vadscale) + configp->rfx_vadoffset;
   return;
}

/*---------------------------------------------------------------------+
 | Convert RFXSensor a/d voltage hdata to double relative humidity.    |
 | The temperature and supply voltage are required for the calculation.|
 +---------------------------------------------------------------------*/
void rfxdata2humi ( unsigned short hdata, unsigned short vdata,
                       unsigned short tdata, double *vad, double *rhum )
{
   double tempc, supvolt;

   tdata &= 0xffe0u;
   if ( tdata & 0x8000u ) {
      /* Negative temperature */
      tdata = (~tdata + 1) & 0xffffu;
      tempc = -(double)tdata / 256.0;
   }
   else {
      tempc = (double)tdata / 256.0;
   }

   supvolt = 0.010 * (double)((vdata & 0xffe0u) >> 5);
   *vad = 0.010 * (double)((hdata & 0xffe0u) >> 5);
   *rhum = (((*vad/supvolt) - 0.16) / 0.0062)/(1.0546 - 0.00216 * tempc);
   return;
}

/*---------------------------------------------------------------------+
 | Convert RFXSensor a/d voltage hdata to scaled double barometric     |
 | pressure value.  The supply voltage is required for the calculation.|
 +---------------------------------------------------------------------*/
void rfxdata2press ( unsigned short hdata, unsigned short vdata,
                                            double *vad, double *bpr )
{
   double supvolt;

   supvolt = 0.010 * (double)((vdata & 0xffe0u) >> 5);
   *vad = 0.010 * (double)((hdata & 0xffe0u) >> 5);
   *bpr = ((*vad/supvolt) + 0.095) / 0.0009;
   *bpr *= configp->rfx_bpscale;
   *bpr += configp->rfx_bpoffset;
   return;
}

/*---------------------------------------------------------------------+
 | Convert RFXSensor a/d voltage hdata to potentiometer percent value. |
 | The supply voltage is required for the calculation.                 |
 +---------------------------------------------------------------------*/
void rfxdata2pot ( unsigned short hdata, unsigned short vdata, double *vad, double *pot )
{
   double supvolt;

   supvolt = 0.010 * (double)((vdata & 0xffe0u) >> 5);
   *vad = 0.010 * (double)((hdata & 0xffe0u) >> 5);
   *pot = 100.0 * (*vad/supvolt);
   return;
}

#endif /* HASRFXS */

/*---------------------------------------------------------------------+
 | Retrieve raw RFXSensor data                                         |
 +---------------------------------------------------------------------*/
int raw_rfxsensor_data ( ALIAS *aliasp, int index, unsigned short *tdata,
                           unsigned short *vdata, unsigned short *hdata )
{
#ifdef HASRFXS
   unsigned char ident, /* delta, */base, offset /*, offbase */;

   if ( aliasp[index].vtype != RF_XSENSOR ) {
      return 1;
   } 

   ident = aliasp[index].ident[0];
   base = ident / 0x20;
   offset = 0x04 * ((ident % 0x20) / 0x04);

   *tdata = x10global.rfxdata[base][offset + RFX_T];
   *hdata = x10global.rfxdata[base][offset + RFX_H];
   *vdata = x10global.rfxdata[base][offset + RFX_S];
#else
   *tdata = *hdata = *vdata = 0;  /* Keep some compilers happy */
#endif /* HASRFXS */

   return 0;
}

  
/*---------------------------------------------------------------------+
 | Decode RFXSensor data for transmitter mapped to ALIAS               |
 +---------------------------------------------------------------------*/
int decode_rfxsensor_data ( ALIAS *aliasp, int index, unsigned int *inmap,
      unsigned int *outmap, double *tempp, double *vsupp, double *vadp, double *var2p )
{
#ifdef HASRFXS
   unsigned short tdata, hdata, vdata;
   unsigned char ident, /* delta, */ base, offset, /* offbase, */flags, tflag, vflag, hflag;
   unsigned char hcode, ucode;
   double tempc, voltage;

   *inmap = *outmap = 0;

   if ( aliasp[index].vtype != RF_XSENSOR ) {
      return 1;
   }

   hcode = hc2code(aliasp[index].housecode);
   ucode = single_bmap_unit(aliasp[index].unitbmap);

   ident = aliasp[index].ident[0];
   base = ident / 0x20;
   offset = 0x04 * ((ident % 0x20) / 0x04);

   flags = (unsigned char)(x10global.rfxflags[base] >> offset) & 0x0fu;
   tflag = (flags >> RFX_T) & 0x01;
   hflag = (flags >> RFX_H) & 0x01;
   vflag = (flags >> RFX_S) & 0x01;

   tdata = x10global.rfxdata[base][offset + RFX_T];
   hdata = x10global.rfxdata[base][offset + RFX_H];
   vdata = x10global.rfxdata[base][offset + RFX_S];


   *inmap |= RFXO_T;
   if ( aliasp[index].optflags & MOPT_RFXVS )
      *inmap |= (RFXO_S | RFXO_LO);

   if ( tflag ) {
      rfxdata2temp(tdata, &tempc, tempp);
      *outmap |= RFXO_T;
   }

   if ( hflag ) {
      rfxdata2volt(hdata, vadp, var2p);
      *outmap |= RFXO_VI;
   }

   if ( vflag ) {
      rfxdata2volt(vdata, vsupp, &voltage);
      *outmap |= RFXO_S;
   }

   if ( aliasp[index].optflags & MOPT_RFXRH ) {
      *inmap |= (RFXO_H | RFXO_VI);
      if ( tflag && vflag && hflag ) {
         rfxdata2humi(hdata, vdata, tdata, vadp, var2p);
         *outmap |= RFXO_H;
      }
   }
   else if ( aliasp[index].optflags & MOPT_RFXBP ) {
      *inmap |= (RFXO_B | RFXO_VI);
      if ( vflag && hflag ) {
         rfxdata2press(hdata, vdata, vadp, var2p);
         *outmap |= RFXO_B;
      }
   }
   else if ( aliasp[index].optflags & MOPT_RFXVAD ) {
      *inmap |= (RFXO_V | RFXO_VI);
      if ( hflag ) {
         rfxdata2volt(hdata, vadp, var2p);
         *outmap |= RFXO_V;
      }
   }
   else if ( aliasp[index].optflags & MOPT_RFXPOT ) {
      *inmap |= (RFXO_P | RFXO_VI);
      if ( hflag && vflag ) {
         rfxdata2pot(hdata, vdata, vadp, var2p);
         *outmap |= RFXO_P;
      }
   }
   else if ( aliasp[index].optflags & MOPT_RFXT2 ) {
      *inmap |= RFXO_T2;
      if ( hflag ) {
         rfxdata2temp(hdata, &tempc, var2p);
         *outmap |= RFXO_T2;
      }
   }

   if ( x10state[hcode].rfxlobat & (1 << ucode) )
      *outmap |= (*inmap & RFXO_LO);

#else
   *inmap = *outmap = 0;
   *tempp = *vsupp = *vadp = *var2p = 0.0;
#endif /* HASRFXS */      
   return 0;
}

#if (defined(HASRFXS) || defined(HASRFXM))
/*---------------------------------------------------------------------+
 | Display a RFXSensor or RFXMeter data value stored in the x10state   |
 | structure.                                                          |
 +---------------------------------------------------------------------*/
int c_rfxcmds ( int argc, char *argv[] )
{

   ALIAS          *aliasp;
   unsigned char  hcode, ucode, offset, type;
   unsigned int   inmap, outmap, fmap;
   double         temp, vsup, vad, var2;
   unsigned long  aflags, optflag;
   unsigned long  rfxmeter;
   char           hc;
   unsigned int   bitmap;
   int            j, unit, index, panelid;
   char           *display, *sp;
   unsigned short data, tdata = 0, vdata = 0, hdata = 0;
   char           obuf[60];
#ifdef HASRFXM
   int            retcode;
#endif

   int read_x10state_file ( void );
   int powerpanel_query ( unsigned char, unsigned long * );

   static struct {
      char *rfxcmd;
      char *display;
      unsigned char offset;
      unsigned char panelok;
      unsigned int fmap;
      unsigned char type;
      unsigned long optflag;
   } cmdlist[] = {
      {"rfxtemp",    "Temp",    0, 0, RFXO_T,  RF_XSENSOR, 0 },
      {"rfxrh",      "RH",      1, 0, RFXO_H,  RF_XSENSOR, 0 },
      {"rfxbp",      "BP",      1, 0, RFXO_B,  RF_XSENSOR, 0 },
      {"rfxpot",     "Pot",     1, 0, RFXO_P,  RF_XSENSOR, 0 },
      {"rfxvs",      "Vs",      2, 0, RFXO_S,  RF_XSENSOR, 0 },
      {"rfxvad",     "Vad",     1, 0, RFXO_V,  RF_XSENSOR, 0 },
      {"rfxvadi",    "Vadi",    1, 0, RFXO_VI, RF_XSENSOR, 0 },
      {"rfxtemp2",   "Temp2",   1, 0, RFXO_T2, RF_XSENSOR, 0 },
      {"rfxlobat",   "LoBat",   2, 0, RFXO_LO, RF_XSENSOR, 0 },
      {"rfxpower",   "Power",   0, 0, 0,       RF_XMETER,  MOPT_RFXPOWER },
      {"rfxwater",   "Water",   0, 0, 0,       RF_XMETER,  MOPT_RFXWATER },
      {"rfxgas",     "Gas",     0, 0, 0,       RF_XMETER,  MOPT_RFXGAS   },
      {"rfxpulse",   "Pulse",   0, 0, 0,       RF_XMETER,  MOPT_RFXPULSE },
      {"rfxcount",   "Counter", 0, 0, 0,       RF_XMETER,  MOPT_RFXCOUNT },
      {"rfxpanel",   "Panel",   0, 1, 0,       RF_XMETER,  MOPT_RFXPOWER },
      {NULL,         NULL,      0, 0, 0,       0,          0}
   };

   if ( argc < 3 && (strcmp(argv[1], "rfxpanel") != 0) ) {
      fprintf(stderr, "Usage: %s %s Hu\n", argv[0], argv[1]);
      return 1;
   }

   if ( check_for_engine() != 0 ) {
      fprintf(stderr, "State engine is not running.\n");
      return 1;
   }
   if ( read_x10state_file() != 0 ) {
      fprintf(stderr, "Unable to read state file.\n");
      return 1;
   }

   if ( (aliasp = configp->aliasp) == NULL )
      return 1;

   j = 0;
   while ( cmdlist[j].rfxcmd != NULL ) {
      if ( strcmp(argv[1], cmdlist[j].rfxcmd) == 0 )
         break;
      j++;
   }
   if ( cmdlist[j].rfxcmd == NULL ) {
      fprintf(stderr, "Invalid command '%s'\n", argv[1]);
      return 1;
   }
   fmap = cmdlist[j].fmap;
   display = cmdlist[j].display;
   offset = cmdlist[j].offset;
   type = cmdlist[j].type;
   optflag = cmdlist[j].optflag;

   if ( cmdlist[j].panelok ) {
      if ( argc > 2 ) {
         panelid = (int)strtol(argv[2], &sp, 10);
      }
      else {
         panelid = 0;
         sp = " ";
      }
#ifdef HASRFXM
      if ( strchr(" \t\r\n", *sp) == NULL || panelid < 0 || panelid >= 0xff ) {
         fprintf(stderr, "Invalid power panel number '%s'\n", argv[2]);
         return 1;
      }
      if ( (retcode = powerpanel_query((unsigned char)panelid, &rfxmeter)) == 0 ) {
         printf(FMT_RFXPOWER"\n", (double)(rfxmeter) * configp->rfx_powerscale);
      }
      else if ( retcode == 1 ) {
         fprintf(stderr, "Power panel %d does not exist.\n", panelid);
         return 1;      
      }
      else if ( retcode == 2 ) {
         fprintf(stderr, "Power panel %d data not ready.\n", panelid);
         return 1;      
      }
#endif /* HASRFXM */
      return 0;
   }
       
   aflags = parse_addr(argv[2], &hc, &bitmap);
   if ( !(aflags & A_VALID) || aflags & (A_DUMMY | A_PLUS | A_MINUS) || bitmap == 0 ) {
      fprintf(stderr, "Invalid Hu address '%s'\n", argv[2]);
      return 1;
   }
   if ( aflags & A_MULT ) {
      fprintf(stderr, "Only a single unit address is valid.\n");
      return 1;
   }
   hcode = hc2code(hc);
   ucode = single_bmap_unit(bitmap);
   unit  = code2unit(ucode);

   if ( (index = alias_lookup_index(hc, bitmap, type)) < 0 ) {
      fprintf(stderr, "Address is not configured as an %s\n",
         ((type == RF_XSENSOR) ? "RFXSensor" : "RFXMeter") );
      return 1;
   }


   if ( type == RF_XMETER ) {
      if ( (optflag & aliasp[index].optflags) == 0 ) {
         fprintf(stderr, "Address is not configured as an RFX%s meter.\n", display);
         return 1;
      }
      if ( (rfxmeter = x10state[hcode].rfxmeter[ucode]) == 0 ) {
         printf("Not ready\n");
         return 1;
      }

      if ( optflag & MOPT_RFXPOWER ) {
         printf(FMT_RFXPOWER"\n", (double)(rfxmeter >> 8) * configp->rfx_powerscale);
      }
      else if ( (optflag & MOPT_RFXWATER) && *(configp->rfx_waterunits) ) {
         printf(FMT_RFXWATER"\n", (double)(rfxmeter >> 8) * configp->rfx_waterscale);
      }
      else if ( (optflag & MOPT_RFXGAS) && *(configp->rfx_gasunits) ) {
         printf(FMT_RFXGAS"\n", (double)(rfxmeter >> 8) * configp->rfx_gasscale);
      }
      else if ( (optflag & MOPT_RFXPULSE) && *(configp->rfx_pulseunits) ) {
         printf(FMT_RFXPULSE"\n", (double)(rfxmeter >> 8) * configp->rfx_pulsescale);
      }
      else if ( optflag & MOPT_RFXCOUNT ) {
         printf("%ld\n", (rfxmeter >> 8));
      }
      else {
         printf("%ld\n", (rfxmeter >> 8));
      }
      return 0;
   }

   /* Remainder is for RFXSensors */

   if ( argc > 3 && toupper((int)((unsigned char)(*argv[3]))) == 'R' ) {
      /* Just display raw stored data and exit */
      raw_rfxsensor_data(aliasp,index, &tdata, &vdata, &hdata);
      data = (offset == 0) ? tdata :
             (offset == 1) ? hdata :
             (offset == 2) ? vdata : 0xffff ;
      printf("0x%04x\n", data); 
      return 0;
   }

   decode_rfxsensor_data(aliasp, index, &inmap, &outmap, &temp, &vsup, &vad, &var2);

   if ( !(inmap & fmap) ) {
      fprintf(stderr, "%s is not supported by this sensor.\n", display);
      return 1;
   }
   if ( fmap != RFXO_LO && !(outmap & fmap) ) {
      printf("Data not ready\n");
      return 1;
   }

   switch ( fmap ) {
      case RFXO_T :
         sprintf(obuf, FMT_RFXT, temp);
         printf("%s\n", obuf);
         break;
      case RFXO_H :
         sprintf(obuf, FMT_RFXRH, var2);
         printf("%s\n", obuf);
         break;
      case RFXO_B :
         sprintf(obuf, FMT_RFXBP, var2);
         printf("%s\n", obuf);
         break;
      case RFXO_V :
         sprintf(obuf, FMT_RFXVAD, var2);
         printf("%s\n", obuf);
         break;
      case RFXO_P :
         printf("%.2f\n", var2);
         break;
      case RFXO_S :
         printf("%.2f\n", vsup);
         break;
      case RFXO_T2 :
         sprintf(obuf, FMT_RFXT, var2);
         printf("%s\n", obuf);
         break;
      case RFXO_LO :
         printf("%d\n", ((outmap & RFXO_LO) ? 1 : 0) );
         break;
      case RFXO_VI :
         printf("%.2f\n", vad);
         break;
      default :
         fprintf(stderr, "Internal error c_rfxcmds()\n");
         return 1;
   }
            
   return 0;
}
#endif /* HASRFXS || HASRFXM */
  
#ifdef HASRFXM
/*---------------------------------------------------------------------+
 | Display stored data for all RFXMeters                               |
 +---------------------------------------------------------------------*/
int show_rfxmeters ( void )
{
   ALIAS         *aliasp;
   char          hc;
   int           unit, index, count = 0, maxlabel = 0;
   unsigned char hcode, ucode;
   unsigned long rfxmeter;
   int           read_x10state_file ( void );

   if ( !(aliasp = configp->aliasp) ) 
      return 0;

   /* Get maximum lengths of module name and alias label */
   index = 0;
   while ( aliasp[index].line_no > 0 ) {
      if ( aliasp[index].vtype == RF_XMETER ) {
         count++;
         maxlabel = max(maxlabel, (int)strlen(aliasp[index].label));
      }
      index++;
   }

   if ( count == 0 )
      return 0;

   index = 0;
   while ( aliasp[index].line_no > 0 ) {
      if ( aliasp[index].vtype != RF_XMETER ) {
         index++;
         continue;
      }
      hc = aliasp[index].housecode;
      hcode = hc2code(hc);
      ucode = single_bmap_unit(aliasp[index].unitbmap);
      unit = code2unit(ucode);

      printf("%c%-2d %-*s", hc, unit, maxlabel, aliasp[index].label);

      rfxmeter = x10state[hcode].rfxmeter[ucode];

      if ( aliasp[index].optflags & MOPT_RFXPOWER ) {
         if ( rfxmeter != 0 ) {
            printf("  Power "FMT_RFXPOWER" %s, Counter %ld\n", 
               (double)(rfxmeter >> 8) * configp->rfx_powerscale,
               configp->rfx_powerunits, (rfxmeter >> 8));
         }
         else {
            printf("  Power ---- , Counter ----\n");
         }
      }
      else if ( aliasp[index].optflags & MOPT_RFXWATER ) {
         if ( rfxmeter != 0 ) {
            if ( *(configp->rfx_waterunits) ) {
               printf("  Water "FMT_RFXWATER" %s, Counter %ld\n", 
                  (double)(rfxmeter >> 8) * configp->rfx_waterscale,
                  configp->rfx_waterunits, (rfxmeter >> 8));
            }
            else {
               printf("  Water ---- , Counter %ld\n", (rfxmeter >> 8));
            }
         }
         else {
            printf("  Water ---- , Counter ----\n");
         }
      }
      else if ( aliasp[index].optflags & MOPT_RFXGAS ) {
         if ( rfxmeter != 0 ) {
            if ( *(configp->rfx_gasunits) ) {
               printf("  Gas "FMT_RFXGAS" %s, Counter %ld\n", 
                  (double)(rfxmeter >> 8) * configp->rfx_gasscale,
                  configp->rfx_gasunits, (rfxmeter >> 8));
            }
            else {
               printf("  Gas ---- , Counter %ld\n", (rfxmeter >> 8));
            }
         }
         else {
            printf("  Gas ---- , Counter ----\n");
         }
      }
      else if ( aliasp[index].optflags & MOPT_RFXPULSE ) {
         if ( rfxmeter != 0 ) {
            if ( *(configp->rfx_pulseunits) ) {
               printf("  Pulse "FMT_RFXPULSE" %s, Counter %ld\n", 
                  (double)(rfxmeter >> 8) * configp->rfx_pulsescale,
                  configp->rfx_pulseunits, (rfxmeter >> 8));
            }
            else {
               printf("  Pulse ---- , Counter %ld\n", (rfxmeter >> 8));
            }
         }
         else {
            printf("  Pulse ---- , Counter ----\n");
         }
      }
      else if ( aliasp[index].optflags & MOPT_RFXCOUNT ) {
         if ( rfxmeter != 0 ) {
            printf("  Counter %ld\n", (rfxmeter >> 8));
         }
         else {
            printf("  Counter ----\n");
         }
      }
      index++;
   }
   return 0;
}

#endif /* HASRFXM */
 
#ifdef HASRFXS
/*---------------------------------------------------------------------+
 | Display stored data for all RFXSensors                              |
 +---------------------------------------------------------------------*/
int show_rfxsensors ( void )
{
   ALIAS         *aliasp;
   char          hc;
   int           unit, index, count = 0, maxlabel = 0;
   unsigned int  inmap, outmap;
   double        temp, vsup, vad, var2;
   char          valbuf[80];
   int read_x10state_file ( void );

   if ( !(aliasp = configp->aliasp) ) 
      return 0;

   /* Get maximum lengths of module name and alias label */
   index = 0;
   while ( aliasp[index].line_no > 0 ) {
      if ( aliasp[index].vtype == RF_XSENSOR ) {
         count++;
         maxlabel = max(maxlabel, (int)strlen(aliasp[index].label));
      }
      index++;
   }

   if ( count == 0 )
      return 0;

   index = 0;
   while ( aliasp[index].line_no > 0 ) {
      if ( aliasp[index].vtype != RF_XSENSOR ) {
         index++;
         continue;
      }
      unit = code2unit(single_bmap_unit(aliasp[index].unitbmap));
      hc = aliasp[index].housecode;

      printf("%c%-2d %-*s", hc, unit, maxlabel, aliasp[index].label);

      decode_rfxsensor_data(aliasp, index, &inmap, &outmap, &temp, &vsup, &vad, &var2);

      if ( outmap & RFXO_T ) {
         sprintf(valbuf, FMT_RFXT, temp);
         printf("  Temp %s%c", valbuf, configp->rfx_tscale);
      }
      else {
         printf("  Temp ----%c", configp->rfx_tscale);
      }

      if ( outmap & RFXO_S ) {
         printf("  Vs %.2fV", vsup);
      }
      else if ( inmap & RFXO_S ) {
         printf("  Vs ----V");
      }
      else {
         printf("\n");
         index++;
         continue;
      }

      if ( outmap & RFXO_H ) {
         printf("  RH %.2f%%", var2);
      }
      else if ( inmap & RFXO_H ) {
         printf("  RH ----%%");
      }
      else if ( outmap & RFXO_B ) {
         sprintf(valbuf, FMT_RFXBP, var2);
         printf("  BP %s %s", valbuf, configp->rfx_bpunits);
      }
      else if ( inmap & RFXO_B) {
         printf("  BP ---- %s", configp->rfx_bpunits);
      }
      else if ( outmap & RFXO_V ) {
         sprintf(valbuf, FMT_RFXVAD, var2);      
         printf("  Vad %s %s", valbuf, configp->rfx_vadunits);
      }
      else if ( inmap & RFXO_V ) {
         printf("  Vad ---- %s", configp->rfx_vadunits);
      }
      else if ( outmap & RFXO_P ) {
         printf("  Pot %.2f%%", var2);
      }
      else if ( inmap & RFXO_P ) {
         printf("  Pot ---- %%");
      }
      else if ( outmap & RFXO_T2 ) {
         sprintf(valbuf, FMT_RFXT, var2);
         printf("  Temp2 %s%c", valbuf, configp->rfx_tscale);
      }
      else if ( inmap & RFXO_T2 ) {
         printf("  Temp2 ----%c", configp->rfx_tscale);
      }

      printf("\n");

      index++;
   }

   return 0;
}
 
 
/*----------------------------------------------------------------------------+
 | Update the x10state structure per the contents of the argument 'buf'       |
 | for RFXSensor modules.  'buf' contains 6 bytes.  The first is the          |
 | standard hcode|ucode byte, the second is the data 0x00-0xff, the third is  |
 | the virtual type, the fourth is the module ID byte, the fifth and sixth    |
 | are the high and low significant raw data bytes.                           |
 |                                                                            |
 | Only modules of type RF_XSENSOR will be updated.                           |
 |                                                                            |
 | The received signal and state are tested to see if any of the conditions   |
 | exist for triggering the launching of an external script, and if so, the   |
 | index of the launcher is passed back through argument 'launchp',           | 
 | otherwise -1 is passed back.                                               |
 +----------------------------------------------------------------------------*/
int x10state_update_rfxsensor ( unsigned char *buf, int len, int *launchp )
{
   unsigned char  hcode, func, xfunc, ucode, vdata, vtype, hibyte, lobyte;
   unsigned char  actfunc, genfunc, xactfunc;
   unsigned int   bitmap, trigaddr, mask, active, trigactive;
   unsigned int   changestate, startupstate;
   unsigned long  vflags;
   unsigned int   bmaplaunch, launched;
   unsigned long  afuncmap, gfuncmap, xfuncmap;
//   unsigned short ident;
   unsigned long  ident;
   struct xlate_vdata_st xlate_vdata;
   int            j, index, trig, base, offset /*, divisor */;
   char           hc;
   LAUNCHER       *launcherp;
   ALIAS          *aliasp;
   extern unsigned int signal_source;
   unsigned short rfxdata;

   launcherp = configp->launcherp;

   *launchp = -1;

   aliasp = config.aliasp;

   genfunc = 0;
   gfuncmap = 0;
   func = xfunc = VdataFunc;
   trig = VdataTrig;
   vflags = 0;

   hcode  = (buf[0] & 0xf0u) >> 4;
   ucode  = buf[0] & 0x0fu;
   vdata  = buf[1];
   vtype  = buf[2];
   ident  = buf[3] | (buf[4] << 8);
   hibyte = buf[5];
   lobyte = buf[6];

   bitmap = 1 << ucode;
   hc = code2hc(hcode);

   /* Run the decoding function for the module type, if any */
   if ( (index = alias_rev_index(hc, bitmap, vtype, ident)) >= 0 &&
        aliasp[index].modtype >= 0 && aliasp[index].xlate_func != NULL ) {
      xlate_vdata.vdata = vdata;
      xlate_vdata.hibyte = hibyte;
      xlate_vdata.lobyte = lobyte;
      xlate_vdata.hcode = hcode;
      xlate_vdata.ucode = ucode;
      xlate_vdata.ident = ident;
      xlate_vdata.nident = aliasp[index].nident;
      xlate_vdata.identp = aliasp[index].ident;
      xlate_vdata.optflags = aliasp[index].optflags;
      /* Tamper flag is sticky */
      xlate_vdata.vflags = x10state[hcode].vflags[ucode] & SEC_TAMPER;
      if ( aliasp[index].xlate_func(&xlate_vdata) != 0 )
         return 1;
      func = xlate_vdata.func;
      vflags = xlate_vdata.vflags;
      trig = xlate_vdata.trig;
   }
   
   x10state[hcode].vaddress = bitmap;
   x10state[hcode].lastcmd = func;
   x10state[hcode].lastunit = code2unit(ucode);
   x10state[hcode].vident[ucode] = ident;
   x10state[hcode].vflags[ucode] = vflags;
   x10state[hcode].timestamp[ucode] = time(NULL);
//   x10state[hcode].state[ValidState] |= (1 << ucode);
//   x10state[hcode].state[ChgState] = 0;
   x10global.lasthc = hcode;
   x10global.lastaddr = 0;
   changestate = 0;

   if ( vflags & SEC_LOBAT ) {
      x10state[hcode].state[LoBatState] |= (1 << ucode);
   }
   else {
      x10state[hcode].state[LoBatState] &= ~(1 << ucode);
   }

   if ( vdata != x10state[hcode].dimlevel[ucode] ) {
//      x10state[hcode].state[ChgState] |= bitmap;
      changestate |= bitmap;
      x10state[hcode].dimlevel[ucode] = vdata;
   }

   base = ident / 0x20;
   offset = ident % 0x20;
   rfxdata = (hibyte << 8) | lobyte;

   if ( (lobyte & 0x10u) == 0 ) {
      if ( x10global.rfxdata[base][offset] != rfxdata ) {
//         x10state[hcode].state[ChgState] |= (1 << ucode);
         changestate |= (1 << ucode);
         x10global.rfxdata[base][offset] = rfxdata;
      }
      else {
//         x10state[hcode].state[ChgState] &= ~(1 << ucode);
         changestate = 0;
      }
      x10global.rfxflags[base] |= (1 << offset);
      if ( (offset % 4) == 2 ) {
         x10state[hcode].rfxlobat &= ~(1 << ucode);
      }
   }
   else if ( (offset % 4) == 2 && hibyte == 0x02 ) {
      x10state[hcode].rfxlobat |= (1 << ucode);
   }

   actfunc = 0;
   afuncmap = 0;
   xactfunc = func;
   xfuncmap = (1 << trig);
   trigaddr = bitmap;

   mask = modmask[VdataMask][hcode];
   active = bitmap & mask;

   startupstate = ~x10state[hcode].state[ValidState] & active;
   x10state[hcode].state[ValidState] |= active;

   update_activity_timeout(aliasp, index);
   update_activity_states(hcode, active, S_ACTIVE);


//   x10state[hcode].state[ModChgState] = (x10state[hcode].state[ModChgState] & ~active) | changestate;
   x10state[hcode].state[ModChgState] = changestate;

   x10state[hcode].state[ChgState] = x10state[hcode].state[ModChgState] |
      (x10state[hcode].state[ActiveChgState] & active) |
      (startupstate & ~modmask[PhysMask][hcode]);

   changestate = x10state[hcode].state[ChgState];

   if ( i_am_state )
      write_x10state_file();

   /* Heyuhelper, if applicable */
   if ( i_am_state && signal_source & RCVI && configp->script_mode & HEYU_HELPER ) {
      launch_heyuhelper(hcode, trigaddr, func);
      return 0;
   }

   bmaplaunch = 0;
   launched = 0;
   j = 0;
   while ( launcherp && launcherp[j].line_no > 0 ) {

      if ( launcherp[j].type != L_NORMAL ||
           launcherp[j].hcode != hcode ||
           is_unmatched_flags(&launcherp[j]) ) {
         j++;
	 continue;
      }

      if ( launcherp[j].xfuncmap & xfuncmap &&
           launcherp[j].source & signal_source ) {
         trigactive = trigaddr & (mask | launcherp[j].signal);
         if ( (bmaplaunch = (trigactive & launcherp[j].bmaptrig) & 
                   (changestate | ~launcherp[j].chgtrig)) ||
                   (launcherp[j].unitstar && !trigaddr)) {
            *launchp = j;
            launcherp[j].matched = YES;
            launcherp[j].actfunc = xactfunc;
            launcherp[j].genfunc = 0;
	    launcherp[j].xfunc = xactfunc;
	    launcherp[j].level = vdata;
            launcherp[j].bmaplaunch = bmaplaunch;
            launcherp[j].actsource = signal_source;
            launched |= bmaplaunch;
            if ( launcherp[j].scanmode & FM_BREAK )
               break;
         }
      }
      j++;
   }

   x10state[hcode].launched = launched;

   return 0;
}
#endif /* HASRFXS */

#ifdef HASRFXM
/*----------------------------------------------------------------------------+
 | Form the unsigned long RFXMeter word for storage and further processing    |
 | from the last 4 bytes of the data received by the RFXCOMVL receiver        |
 | (which have a somewhat unusual arrangement).                               |
 | The "parity" nybble is replaced by 0x1 for use as a flag.                  |
 | The pulse count will be the return value >> 8                              |
 +----------------------------------------------------------------------------*/
unsigned long rfxmeter_word( unsigned char byte0, unsigned char byte1,
                             unsigned char byte2, unsigned char byte3 )
{
   return (byte2 << 24) | (byte0 << 16) | (byte1 << 8) | (byte3 & 0xf0) | 1;
}

/*----------------------------------------------------------------------------+
 | Update the x10state structure per the contents of the argument 'buf'       |
 | for RFXMeter modules.  'buf' contains 8 bytes.  The first is the           |
 | standard hcode|ucode byte, the second is the data 0x00-0xff, the third is  |
 | the virtual type, the fourth is the module ID byte, the fifth through      |
 | eighth are the high through low significant raw data bytes.                |
 |                                                                            |
 | Only modules of type RF_XMETER will be updated.                            |
 |                                                                            |
 | The received signal and state are tested to see if any of the conditions   |
 | exist for triggering the launching of an external script, and if so, the   |
 | index of the launcher is passed back through argument 'launchp',           | 
 | otherwise -1 is passed back.                                               |
 | Buffer reference: vtype, seq, id_high, id_low, nbytes, bytes 1-N
 +----------------------------------------------------------------------------*/
int x10state_update_rfxmeter ( unsigned char addr, unsigned char *buf, unsigned char *sunchanged, int *launchp )
{
   unsigned char  hcode, func, xfunc, ucode, vdata, /*ident,*/ vtype, hibyte, lobyte;
//   unsigned short ident;
   unsigned long  ident;
   unsigned char  actfunc, genfunc, xactfunc;
   unsigned int   bitmap, trigaddr, mask, active, trigactive, vflags;
   unsigned int   changestate, startupstate;
   unsigned int   bmaplaunch, launched;
   unsigned long  afuncmap, gfuncmap, xfuncmap;
   struct xlate_vdata_st xlate_vdata;
   int            j, index, trig;
   char           hc;
   LAUNCHER       *launcherp;
   ALIAS          *aliasp;
   extern unsigned int signal_source;
   unsigned long  rfxmeter;
   unsigned char  rfxcode;
   unsigned char  *vdatap;

   launcherp = configp->launcherp;

   *launchp = -1;
   *sunchanged = 0;

   aliasp = config.aliasp;

   genfunc = 0;
   gfuncmap = 0;
   func = xfunc = VdataFunc;
   trig = VdataTrig;
   vflags = 0;

   hcode  = (addr & 0xf0u) >> 4;
   ucode  = addr & 0x0fu;
   vtype  = buf[0];
//   ident  = (unsigned short)buf[3];
   ident = buf[3];
   vdatap = buf + 7;
   vdata  = 0;
   
   hibyte = buf[5];
   lobyte = buf[6];
   
   bitmap = 1 << ucode;
   hc = code2hc(hcode);

   /* Run the decoding function for the module type, if any */
   if ( (index = alias_rev_index(hc, bitmap, vtype, ident)) >= 0 &&
        aliasp[index].modtype >= 0 && aliasp[index].xlate_func != NULL ) {
      xlate_vdata.vdata = vdata;
      xlate_vdata.hibyte = hibyte;
      xlate_vdata.lobyte = lobyte;
      xlate_vdata.hcode = hcode;
      xlate_vdata.ucode = ucode;
      xlate_vdata.ident = ident;
      xlate_vdata.nident = aliasp[index].nident;
      xlate_vdata.identp = aliasp[index].ident;
      xlate_vdata.optflags = aliasp[index].optflags;
      /* Tamper flag is sticky */
      xlate_vdata.vflags = x10state[hcode].vflags[ucode] & SEC_TAMPER;
      if ( aliasp[index].xlate_func(&xlate_vdata) != 0 )
         return 1;

      func = xlate_vdata.func;
      vflags = xlate_vdata.vflags;
      trig = xlate_vdata.trig;
   }
   
   x10state[hcode].vaddress = bitmap;
   x10state[hcode].lastcmd = func;
   x10state[hcode].lastunit = code2unit(ucode);
   x10state[hcode].vident[ucode] = ident;
   x10state[hcode].vflags[ucode] = vflags;
   x10state[hcode].timestamp[ucode] = time(NULL);
//   x10state[hcode].state[ValidState] |= (1 << ucode);
//   x10state[hcode].state[ChgState] = 0;
   x10global.lasthc = hcode;
   x10global.lastaddr = 0;
   changestate = 0;

   if ( vflags & SEC_LOBAT ) {
      x10state[hcode].state[LoBatState] |= (1 << ucode);
   }
   else {
      x10state[hcode].state[LoBatState] &= ~(1 << ucode);
   }

   rfxmeter = rfxmeter_word(vdatap[0], vdatap[1], vdatap[2], vdatap[3]);

   rfxcode = rfxmeter & 0xf0;

   /* Replace the superfluous parity field (lowest nybble) by 1 as a flag */
   rfxmeter = (rfxmeter & 0xfffffff0) | 0x01;

   if ( rfxcode == 0 ) {
      if ( rfxmeter != x10state[hcode].rfxmeter[ucode] ) {
//         x10state[hcode].state[ChgState] |= bitmap;
         changestate |= bitmap;
      }
      else {
//         x10state[hcode].state[ChgState] &= ~bitmap;
         changestate &= ~bitmap;
      }

      if ( (rfxmeter & 0xFFFFFF00) < (x10state[hcode].rfxmeter[ucode] & 0xFFFFFF00) ) {
         x10state[hcode].vflags[ucode] |= RFX_ROLLOVER;
//         changestate |= bitmap;
      }
      x10state[hcode].rfxmeter[ucode] = rfxmeter;
   }
   else {
      return 0;
   }

   actfunc = 0;
   afuncmap = 0;
   xactfunc = func;
   xfuncmap = (1 << trig);
   trigaddr = bitmap;

   mask = modmask[VdataMask][hcode];
   active = bitmap & mask;

   startupstate = ~x10state[hcode].state[ValidState] & active;
   x10state[hcode].state[ValidState] |= active;

   update_activity_timeout(aliasp, index);
   update_activity_states(hcode, active, S_ACTIVE);
 
//   x10state[hcode].state[ModChgState] = (x10state[hcode].state[ModChgState] & ~active) | changestate;  
   x10state[hcode].state[ModChgState] = changestate;
  
   x10state[hcode].state[ChgState] = x10state[hcode].state[ModChgState] |
      (x10state[hcode].state[ActiveChgState] & active) |
      (startupstate & ~modmask[PhysMask][hcode]);

   changestate = x10state[hcode].state[ChgState];

   *sunchanged = (changestate & active) ? 0 : 1;

   if ( i_am_state )
      write_x10state_file();

   /* Heyuhelper, if applicable */
   if ( i_am_state && signal_source & RCVI && configp->script_mode & HEYU_HELPER ) {
      launch_heyuhelper(hcode, trigaddr, func);
      return 0;
   }

   bmaplaunch = 0;
   launched = 0;
   j = 0;
   while ( launcherp && launcherp[j].line_no > 0 ) {
      if ( launcherp[j].type != L_NORMAL ||
           launcherp[j].hcode != hcode ||
           is_unmatched_flags(&launcherp[j]) ||
           (launcherp[j].vflags & x10state[hcode].vflags[ucode]) != launcherp[j].vflags ||
	   (launcherp[j].notvflags & ~x10state[hcode].vflags[ucode]) != launcherp[j].notvflags ) {
         j++;
	 continue;
      }

      if ( launcherp[j].xfuncmap & xfuncmap &&
           launcherp[j].source & signal_source ) {
         trigactive = trigaddr & (mask | launcherp[j].signal);
         if ( (bmaplaunch = (trigactive & launcherp[j].bmaptrig) &
                   (changestate | ~launcherp[j].chgtrig)) ||
                   (launcherp[j].unitstar && !trigaddr)) {
            *launchp = j;
            launcherp[j].matched = YES;
            launcherp[j].actfunc = xactfunc;
            launcherp[j].genfunc = 0;
	    launcherp[j].xfunc = xactfunc;
	    launcherp[j].level = vdata;
            launcherp[j].bmaplaunch = bmaplaunch;
            launcherp[j].actsource = signal_source;
            launched |= bmaplaunch;
            if ( launcherp[j].scanmode & FM_BREAK )
               break;
         }
      }
      j++;
   }

   x10state[hcode].launched = launched;

   return 0;
}

/*----------------------------------------------------------------------------+
 | Translate RFXMeter messages for codes other than 0                         |
 | Reference: RFXCOM RFreceiver program.                                      |
 +----------------------------------------------------------------------------*/
char *translate_rfxmeter_code ( unsigned short vident, unsigned long rfxdata )
{ 
   unsigned char code, subcode, intv;

   static char outbuf[80];
   unsigned char recbuf[6];
   double calib, dmeasured;
   unsigned long measured;

   /* Reconstruct original bytes (excluding ident bytes) */
   recbuf[0] = recbuf[1] = 0;
   recbuf[2] = (rfxdata & 0xFF0000) >> 16;
   recbuf[3] = (rfxdata & 0xFF00) >> 8;
   recbuf[4] = (rfxdata & 0xFF000000) >> 24;
   recbuf[5] = rfxdata & 0xFF;

   code = recbuf[5] & 0xF0;
   outbuf[0] = '\0';

   switch ( code ) {
      case 0x00 :
         sprintf(outbuf, "RFXMeter ID %02X, Counter %ld", vident, (rfxdata >> 8));
         break;
      case 0x10 :
         intv = recbuf[2];
         sprintf(outbuf, "Interval %s ", 
            ((intv == 0x01) ? "30 sec"     :
             (intv == 0x02) ? "1 min"      :
             (intv == 0x04) ? "6 min"      :
             (intv == 0x08) ? "12 min"     :
             (intv == 0x10) ? "15 min"     :
             (intv == 0x20) ? "30 min"     :
             (intv == 0x40) ? "45 min"     :
             (intv == 0x80) ? "60 min"     : "???"));
         break;
      case 0x20 :
         switch ( recbuf[4] & 0xC0 ) {
            case 0x00 :
               sprintf(outbuf, "Input-0 ");
               break;
            case 0x40 :
               sprintf(outbuf, "Input-1 ");
               break;
            case 0x80 :
               sprintf(outbuf, "Input-2 ");
               break;
            default :
               sprintf(outbuf, "Unknown input ");
               break;
         }
         dmeasured = (double)(((recbuf[4] & 0x3F) << 16) | (recbuf[2] << 8) | recbuf[3]) / 1000.;
         sprintf(outbuf + strlen(outbuf), "Calibration: %.3f msec", dmeasured);
         if ( dmeasured != 0 ) {
#if 0
            calib = 1.0 / ( (16. * dmeasured) / (3600000. / 100.) );
            sprintf(outbuf + strlen(outbuf), "RFXPower= %.3f kW ", calib);
#endif /* Old RFXPower module */

            calib = 1.0 / ( (16. * dmeasured) / (3600000. / 62.5) );
            sprintf(outbuf + strlen(outbuf), ", RFXPwr= %.3f kW", calib);
#if 0
            calib *= 1.917;
            sprintf(outbuf + strlen(outbuf), "|%.3f kW", calib);
#endif /* Old RFXPOWER module */

         }
         break;
      case 0x30 :
         sprintf(outbuf, "New address ID %02X set", vident);
         break;
      case 0x40 :
         subcode = recbuf[4] & 0xC0;
         sprintf(outbuf, "Push MODE button to skip resetting %s counter to 0",
            ((subcode == 0x00) ? "Input-0" :
             (subcode == 0x40) ? "Input-1" :
             (subcode == 0x80) ? "Input-2" : "Error"));
         break;
      case 0x50 :
         measured = ((recbuf[2] >> 4) * 100000) + ((recbuf[2] & 0xF) * 10000) +
            ((recbuf[3] >> 4) * 1000) + ((recbuf[3] & 0xF) * 100) + 
            ((recbuf[4] >> 4) * 10) + (recbuf[4] & 0xF);
         sprintf(outbuf, "Push MODE button to increment 1st digit of counter %05ld", measured);
         break;
      case 0x60 :
         measured = ((recbuf[2] >> 4) * 100000) + ((recbuf[2] & 0xF) * 10000) +
            ((recbuf[3] >> 4) * 1000) + ((recbuf[3] & 0xF) * 100) + 
            ((recbuf[4] >> 4) * 10) + (recbuf[4] & 0xF);
         sprintf(outbuf, "Push MODE button to increment 2nd digit of counter %05ld", measured);
         break;
      case 0x70 :
         measured = ((recbuf[2] >> 4) * 100000) + ((recbuf[2] & 0xF) * 10000) +
            ((recbuf[3] >> 4) * 1000) + ((recbuf[3] & 0xF) * 100) + 
            ((recbuf[4] >> 4) * 10) + (recbuf[4] & 0xF);
         sprintf(outbuf, "Push MODE button to increment 3rd digit of counter %05ld", measured);
         break;
      case 0x80 :
         measured = ((recbuf[2] >> 4) * 100000) + ((recbuf[2] & 0xF) * 10000) +
            ((recbuf[3] >> 4) * 1000) + ((recbuf[3] & 0xF) * 100) + 
            ((recbuf[4] >> 4) * 10) + (recbuf[4] & 0xF);
         sprintf(outbuf, "Push MODE button to increment 4th digit of counter %05ld", measured);
         break;
      case 0x90 :
         measured = ((recbuf[2] >> 4) * 100000) + ((recbuf[2] & 0xF) * 10000) +
            ((recbuf[3] >> 4) * 1000) + ((recbuf[3] & 0xF) * 100) + 
            ((recbuf[4] >> 4) * 10) + (recbuf[4] & 0xF);
         sprintf(outbuf, "Push MODE button to increment 5th digit of counter %05ld", measured);
         break;
      case 0xA0 :
         measured = ((recbuf[2] >> 4) * 100000) + ((recbuf[2] & 0xF) * 10000) +
            ((recbuf[3] >> 4) * 1000) + ((recbuf[3] & 0xF) * 100) + 
            ((recbuf[4] >> 4) * 10) + (recbuf[4] & 0xF);
         sprintf(outbuf, "Push MODE button to increment 6th digit of counter %05ld", measured);
         break;
      case 0xB0 :
         subcode = recbuf[4];
         sprintf(outbuf, "Counter for %s has been reset to zero",
            ((subcode == 0x00) ? "Input-0" :
             (subcode == 0x40) ? "Input-1" :
             (subcode == 0x80) ? "Input-2" : "???"));           
         break;
      case 0xC0 :
         sprintf(outbuf, "Push MODE button to skip SET INTERVAL RATE");
         break;
      case 0xD0 :
         subcode = recbuf[4] & 0xC0;
         sprintf(outbuf, "Push MODE button to skip CALIBRATION of %s",
            ((subcode == 0x00) ? "Input-0" :
             (subcode == 0x40) ? "Input-1" :
             (subcode == 0x80) ? "Input-2" : "???"));           
         break;
      case 0xE0 :
         sprintf(outbuf, "Push MODE button to skip SET ADDRESS");
         break;
      case 0xF0 :
         sprintf(outbuf, "%s ",
            ((recbuf[2] < 0x40) ? "RFXPower " :
             (recbuf[2] < 0x80) ? "RFXWater " :
             (recbuf[2] < 0xC0) ? "RFXGas "   : "RFXMeter "));
         sprintf(outbuf + strlen(outbuf), "firmware 0x%02X,  ", recbuf[2]);
         intv = recbuf[3];
         sprintf(outbuf + strlen(outbuf), "Interval %s ", 
            ((intv == 0x01) ? "30 sec"     :
             (intv == 0x02) ? "1 min"      :
             (intv == 0x04) ? "6 min"      :
             (intv == 0x08) ? "12 min"     :
             (intv == 0x10) ? "15 min"     :
             (intv == 0x20) ? "30 min"     :
             (intv == 0x40) ? "45 min"     :
             (intv == 0x80) ? "60 min"     : "???"));
         break;
      default :
         sprintf(outbuf, "Invalid RFXMeter message code %02X", code);
         break;
   }

   return outbuf;
}

#define MAX_PANEL_PHASES  3

static struct phaselist_st {
   unsigned char  panel;
   unsigned char  size;
   unsigned char  ident;
   unsigned char  hcode;
   unsigned char  ucode;
} phaselist[80 /* MAX_RFX_SENSORS */];
static int nphases;

int npowerpanels;

/*----------------------------------------------------------------------------+
 |  Comparison function for qsort().                                          |
 +----------------------------------------------------------------------------*/
int cmp_phase_ids( const void *phase1, const void *phase2 )
{
   struct phaselist_st *one = (struct phaselist_st *)phase1;
   struct phaselist_st *two = (struct phaselist_st *)phase2;

   return (two->ident < one->ident) ? -1 :
          (two->ident > one->ident) ?  1 : 0;
}

/*----------------------------------------------------------------------------+
 | Create a table of RFXPower addresses grouped in "power panels" of two or   |
 | three phases of a multiphase AC power system.                              |
 +----------------------------------------------------------------------------*/
void create_rfxpower_panels ( void )
{   
   ALIAS         *aliasp;
   int           j, k;
   unsigned char identprev, size, panelid;

   npowerpanels = 0;
   nphases = 0;

   if ( (aliasp = configp->aliasp) == NULL ) {
      return;
   }

   j = 0;
   nphases = 1;
   while ( aliasp[j].line_no > 0 && nphases < (int)(sizeof(phaselist)/sizeof(struct phaselist_st)) ) {
      if ( aliasp[j].optflags & MOPT_RFXPOWER ) {
         phaselist[nphases].panel = 0xff;
         phaselist[nphases].size = 1;
         phaselist[nphases].hcode = hc2code(aliasp[j].housecode);
         phaselist[nphases].ucode = single_bmap_unit(aliasp[j].unitbmap);
         phaselist[nphases++].ident = aliasp[j].ident[0];
      }
      j++;
   }

   if ( nphases == 1 ) {
      nphases = 0;
      return;
   }

   /* Sort phaselist in descending order of idents */
   qsort(phaselist + 1, nphases - 1, sizeof(struct phaselist_st), cmp_phase_ids);

   phaselist[0].ident = phaselist[1].ident;
   phaselist[0].size  = phaselist[1].size;

   /* Create groups of phases which have consecutive IDs */
   identprev = phaselist[nphases - 1].ident;
   j = nphases - 1;
   while ( j > 0 ) {
      k = j - 1;
      while ( k >= 0 ) {
         if ( phaselist[k].ident == (identprev + 1) && phaselist[j].size < MAX_PANEL_PHASES) {
            ++phaselist[j].size;
            identprev = phaselist[k].ident;
            k--;
         }
         else {
            identprev = phaselist[k].ident;
            j = k;
            break;
         }
      }
   }

   nphases--;
   memmove(phaselist, phaselist + 1, nphases * sizeof(struct phaselist_st) );

   for ( j = 0; j < nphases; j++ ) {
      if ( (size = phaselist[j].size) > 1 ) {
         phaselist[j - size + 1].size = size;
         phaselist[j].size = 1;
      }
   }


   /* Assign an ID to each panel */
   panelid = 0;
   npowerpanels = 0;
   for ( j = nphases - 1; j >= 0; j-- ) {
      if ( phaselist[j].size > 1 )
         phaselist[j].panel = panelid++;
   }
   npowerpanels = panelid;

   return;
}

/*----------------------------------------------------------------------------+
 | Pass back the power panel ID and total power usage for RFXPower sensors    |
 | in multiphase AC power systems when the argument ident corresponds to the  |
 | highest ident in a panel group and return 0.  Otherwise return 1.          |
 +----------------------------------------------------------------------------*/
int powerpanel_total ( unsigned char ident, int *panelid, unsigned long *rfxpower )
{
   int           j, k;
   unsigned long rfxmeter;
    
   for ( j = 0; j < nphases; j++ ) {
      if ( phaselist[j].ident == ident ) {
         if ( phaselist[j].size < 2 )
            return 1;
         *panelid = phaselist[j].panel;
         *rfxpower = 0;
         for ( k = 0; k < phaselist[j].size; k++ ) {
            rfxmeter = x10state[phaselist[j + k].hcode].rfxmeter[phaselist[j + k].ucode];
            if ( rfxmeter & 0x01 )
               *rfxpower += (rfxmeter >> 8);
            else
               return 2;
         }
         break;
      }
   }
   return 0;
}

/*----------------------------------------------------------------------------+
 | Pass back the total multiphase power usage for power panel 'panelid'.      |
 | Return 1 if valid panelid does not exist, or 0 otherwise.                  |
 +----------------------------------------------------------------------------*/
int powerpanel_query ( unsigned char panelid, unsigned long *rfxpower ) 
{
   int j, k;
   unsigned long rfxmeter;

   *rfxpower = 0;

   if ( nphases == 0 || panelid == 0xff ) {
      return 1;
   }

   for ( j = 0; j < nphases; j++ ) {
      if ( phaselist[j].panel == panelid ) {
         for ( k = 0; k < phaselist[j].size; k++ ) {
            rfxmeter = x10state[phaselist[j + k].hcode].rfxmeter[phaselist[j + k].ucode];
            if ( rfxmeter & 0x01 )
               *rfxpower += (rfxmeter >> 8);
            else 
               return 2;
         }
         return 0;
      }
   }
   return 1;
} 

#endif /* HASRFXM */

/*------------------------------------------------------------------------+
 | Interpret Virtual data string, update the state, and test whether      |
 | any launch condition is satisfied.                                     |
 | buffer references:                                                     |
 |  ST_COMMAND, ST_LONGVDATA, vtype, seq, vidhi, vidlo, nbytes, bytes 1-N |
 +------------------------------------------------------------------------*/
char *translate_rfxmeter ( unsigned char *buf, unsigned char *sunchanged, int *launchp )
{
   static char outbuf[160];
#ifdef HASRFXM
   char flagslist[80];
   ALIAS *aliasp;
   unsigned char func, *vdatap, vtype, seq;
   unsigned short vident;

   static char *typename[] = {"Std", "Ent", "Sec", "RFXSensor", "RFXMeter", "?", "??", "???", "????", "Digimax", "Noise", "Noise2"};
   char hc;
   int  j, k, found, index = -1, panelid;
   unsigned char hcode, ucode, addr, rfxcode, unit;
   unsigned int bitmap, vflags = 0;
   unsigned char *inbuf;
   unsigned long counter, longvdata, rfxpower;
   double rfxmeterdbl;
   char *roll;
   extern int x10state_update_rfxmeter ( unsigned char, unsigned char *, unsigned char *, int * );
   extern unsigned long rfxmeter_word ( unsigned char, unsigned char,
           unsigned char, unsigned char );
   char *translate_rfxmeter_code ( unsigned short, unsigned long );
   int powerpanel_total ( unsigned char, int *, unsigned long * );
   extern int rfxmeter_checksum ( unsigned char * );

   *launchp = -1;
   *sunchanged = 0;
   flagslist[0] = '\0';
   *outbuf = '\0';

   aliasp = config.aliasp;

   inbuf  = buf + 7; /* Original data including two ID bytes */
   vtype  = buf[2];
   seq    = buf[3];
   vident = (buf[4] << 8) | buf[5];
   vdatap = buf + 9;
   func   = VdataFunc;

   x10global.longvdata = 0;
   x10global.lastvtype = vtype;

   if ( vtype == RF_XMETER ) {
      /* Re-verify RFXMeter checksum in case of spoolfile corruption */
      if ( rfxmeter_checksum(inbuf) != 0 ) {
         sprintf(outbuf, "            Error : Corrupted RFXM buffer: 0x%02X%02X%02X%02X%02X%02X",
           inbuf[0], inbuf[1], inbuf[2], inbuf[3], inbuf[4], inbuf[5]);
         *sunchanged = 0;
         *launchp = -1;
         return outbuf;
      };

      if ( i_am_rfxmeter ) {
         longvdata = rfxmeter_word(vdatap[0], vdatap[1], vdatap[2], vdatap[3]);
         return translate_rfxmeter_code ( vident, longvdata );
      }
         
      hcode = ucode = 0;
      found = 0;
      j = 0;
      /* Look up the alias, if any */
      while ( !found && aliasp && aliasp[j].line_no > 0 ) {
         if ( aliasp[j].vtype == vtype && 
              (bitmap = aliasp[j].unitbmap) > 0 &&
              (ucode = single_bmap_unit(bitmap)) != 0xff ) {
            for ( k = 0; k < aliasp[j].nident; k++ ) {
               if ( aliasp[j].ident[k] == vident ) {
                  index = j;
                  found = 1;
                  break;
               }
            }
         }
         j++;
      }

      if ( !found || !aliasp ) {
         sprintf(outbuf, "func %12s : Type %s ID 0x%02X Data 0x%02X%02X%02X%02X",
           "RFdata", typename[vtype], vident, vdatap[0], vdatap[1], vdatap[2], vdatap[3]);
         return outbuf;
      }

      x10global.longvdata = rfxmeter_word(vdatap[0], vdatap[1], vdatap[2], vdatap[3]);

      hc = aliasp[index].housecode;
      hcode = hc2code(hc);
      unit = code2unit(ucode);

      /* Add address and update the state */
      addr = (hcode & 0x0fu) << 4 | (ucode & 0x0fu);
      if ( x10state_update_rfxmeter(addr, buf + 2, sunchanged, launchp) != 0 )
         return "";

      func = x10state[hcode].lastcmd;
      vflags = x10state[hcode].vflags[ucode];
      create_flagslist(vtype, vflags, flagslist);

      roll = (vflags & RFX_ROLLOVER) ? "rollover " : "";

      rfxcode = vdatap[3] >> 4;

      if ( rfxcode == 0 ) {
         counter = x10global.longvdata >> 8;
         if ( func == RFXPowerFunc && *(configp->rfx_powerunits) ) {
            rfxmeterdbl = (double)counter * configp->rfx_powerscale;
            sprintf(outbuf, "func %12s : hu %c%-2d Meter "FMT_RFXPOWER" %s %s(%s)",
               funclabel[func], hc, unit, rfxmeterdbl, configp->rfx_powerunits, roll, aliasp[index].label);
            if ( powerpanel_total(vident, &panelid, &rfxpower) == 0 ) {
               rfxmeterdbl = (double)rfxpower * configp->rfx_powerscale;
               sprintf(outbuf + strlen(outbuf),
                 "\nPanel %d total = "FMT_RFXPOWER" %s", panelid, rfxmeterdbl, configp->rfx_powerunits);
            }
         }
         else if ( func == RFXWaterFunc && *(configp->rfx_waterunits) ) {
            rfxmeterdbl = (double)counter * configp->rfx_waterscale;
            sprintf(outbuf, "func %12s : hu %c%-2d Meter "FMT_RFXWATER" %s %s(%s)",
               funclabel[func], hc, unit, rfxmeterdbl, configp->rfx_waterunits, roll, aliasp[index].label);
         }
         else if ( func == RFXGasFunc && *(configp->rfx_gasunits) ) {
            rfxmeterdbl = (double)counter * configp->rfx_gasscale;
            sprintf(outbuf, "func %12s : hu %c%-2d Meter "FMT_RFXGAS" %s %s(%s)",
               funclabel[func], hc, unit, rfxmeterdbl, configp->rfx_gasunits, roll, aliasp[index].label);
         }
         else if ( func == RFXPulseFunc && *(configp->rfx_pulseunits) ) {
            rfxmeterdbl = (double)counter * configp->rfx_pulsescale;
            sprintf(outbuf, "func %12s : hu %c%-2d Meter "FMT_RFXPULSE" %s %s(%s)",
               funclabel[func], hc, unit, rfxmeterdbl, configp->rfx_pulseunits, roll, aliasp[index].label);
         }
         else {
            sprintf(outbuf, "func %12s : hu %c%-2d Counter %ld %s(%s)",
               funclabel[func], hc, unit, counter, roll, aliasp[index].label);
         }
      }
      else if ( configp->rfx_inline == YES ) {
         longvdata = rfxmeter_word(vdatap[0], vdatap[1], vdatap[2], vdatap[3]);
         sprintf(outbuf, "func %12s : hu %c%-2d Code: 0x%02X %s",
           funclabel[func], hc, unit, rfxcode, translate_rfxmeter_code(vident, longvdata));
      }
      else {
         sprintf(outbuf, "func %12s : hu %c%-2d Code: 0x%02X (%s)",
            funclabel[func], hc, unit, rfxcode, aliasp[index].label);
      }
   }
   else {
      sprintf(outbuf, "func %12s : Type 0x%02x Data (hex) %02x %02x %02x %02x",
         "RFdata", vtype, vdatap[0], vdatap[1], vdatap[2], vdatap[3]);
   }
#endif /* HASRFXM */ 

   return outbuf;
}

struct x10list_st {
   unsigned char hcode;
   unsigned int  bitmap;
};


/*----------------------------------------------------------------------------+
 | Compress X10 list by combining bitmaps for the same housecodes, keeping    |
 | housecodes in the same order as they appear in the original list.          |
 +----------------------------------------------------------------------------*/
int compress_x10list ( struct x10list_st *x10list, int *listsize )
{
#ifdef HASKAKU
   struct x10list_st duplist[256];
   int j, k, newsize;
   unsigned char hcode;

   memcpy(duplist, x10list, (*listsize) * sizeof(struct x10list_st));

   newsize = 0;
   for ( j = 0; j < (*listsize); j++ ) {
      if ( !duplist[j].bitmap )
         continue;
      x10list[newsize].hcode = hcode = duplist[j].hcode;
      x10list[newsize].bitmap = duplist[j].bitmap;
      for ( k = j + 1; k < (*listsize); k++ ) {
         if ( duplist[k].hcode == hcode ) {
            x10list[newsize].bitmap |= duplist[k].bitmap;
            duplist[k].bitmap = 0;
         }
      }
      newsize++;
   }
   *listsize = newsize;
#endif /* HASKAKU */

   return 0;
}


/*----------------------------------------------------------------------------+
 | Translate KaKu RF data                                                     |
 | Buffer reference: ST_COMMAND, ST_VARIABLE_LEN, plus:                       |
 |  vtype, buflen,  buffer                                                    |
 |   [2]     [3]     [4+]                                                     |
 +----------------------------------------------------------------------------*/
char *translate_kaku ( unsigned char *xbuf, unsigned char *sunchanged, int *launchp )
{

#ifdef HASKAKU
   static char   outbuf[2048];
   extern unsigned int signal_source;
   extern unsigned int kmodmask[NumKmodMasks][16];
   extern void get_states ( unsigned char, unsigned int *, unsigned int * );
   extern unsigned int get_dimstate ( unsigned char );
   extern void save_dimlevel ( unsigned char, unsigned int );
   extern void restore_dimlevel ( unsigned char, unsigned int );
   extern void set_dimlevel ( int, unsigned char, unsigned int );
   extern void set_on_level ( unsigned char, unsigned int );
   extern char *datstrf ( void );
   extern char *lookup_label ( char, unsigned int );

   struct x10list_st x10list[256];

   ALIAS         *aliasp;
   LAUNCHER      *launcherp;
   unsigned long kaddr;
   unsigned char *buf;
   unsigned char keynum;
   char          hc = 'Z';
   unsigned char hcode, ucode, trig;
   unsigned int  bitmap, afuncmap;
   unsigned long kfuncmap;
   unsigned int  onstate, dimstate, active, mask, resumask, trigaddr, trigactive;
   unsigned int  changestate, startupstate;
   unsigned int  launched, bmaplaunch;
   char          *ulabel;
   char          uval[4];
   unsigned char cmd, cmd2;
   unsigned char level = 0, breaker;
   int           j, k, n, nx10, unit;
   int           index;
   int           nbits, func, actfunc, kactfunc;
   char          funcparmstr[16];
   char          *sublabel;

   /* Kaku functions 0x00 to 0x11 */
   static int    ksfunc[] = {KakuOffFunc, KakuOnFunc, KakuGrpOffFunc, KakuGrpOnFunc};
//   static int    kstrig[] = {KakuOffTrig, KakuOnTrig, KakuGrpOffTrig, KakuGrpOnTrig};
   static int    kpfunc[] = {KakuPreFunc, KakuUnkPreFunc, KakuGrpPreFunc, KakuUnkPreFunc};
//   static int    kptrig[] = {KakuPreTrig, KakuUnkPreTrig, KakuGrpPreTrig, KakuUnkPreTrig};

   *sunchanged = 0;
   *launchp = -1;
   *outbuf = '\0';
   breaker = 0;
   changestate = 0;


   nbits = xbuf[4] & 0x7F;
   buf = xbuf + 5;
   sublabel = (nbits == 34) ? "KAKU_S" :
              (nbits == 36) ? "KAKU_D" : "KAKU_?";

   kaddr = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | (buf[3] & 0xc0);
   kaddr = kaddr >> 6;

   cmd = (buf[3] & 0x30) >> 4;
   keynum = (buf[3] & 0x0f);

   index = 0;
   *outbuf = '\0';

   aliasp = config.aliasp;
   
   /* Create a list of Hu mapped to this signal */

   nx10 = 0;
   if ( cmd & 0x02 ) {
      /* Group command */
      while ( aliasp && aliasp[index].line_no > 0 ) {
         if ( aliasp[index].vtype == RF_KAKU ) {
            for ( j = 0; j < aliasp[index].nident; j++ ) {
               if ( aliasp[index].ident[j] == kaddr  &&
                    aliasp[index].kaku_grpmap[j] & (1 << keynum) ) {
                  x10list[nx10].hcode = hc2code(aliasp[index].housecode);
                  x10list[nx10++].bitmap = aliasp[index].unitbmap;                  
               }
            }
         }
         index++;
      }
   }
   else {
      /* Unit key command */
      while ( aliasp && aliasp[index].line_no > 0 ) {
         if ( aliasp[index].vtype == RF_KAKU ) {
            for ( j = 0; j < aliasp[index].nident; j++ ) {
               if ( aliasp[index].ident[j] == kaddr  &&
                    aliasp[index].kaku_keymap[j] & (1 << keynum) ) {
                  x10list[nx10].hcode = hc2code(aliasp[index].housecode);
                  x10list[nx10++].bitmap = aliasp[index].unitbmap;                  
               }
            }
         }
         index++;
      }
   }


   if ( !nx10 ) {
      /* Display "Key" for kOn/kOff or "Grp" for kGrpOn/kGrpOff */
      if ( cmd & 0x02 ) {
         /* Group command */
         ulabel = "Grp";
         sprintf(uval, "%c", 'A' + keynum);
      }
      else {
         /* Unit command */
         ulabel = "Key";
         sprintf(uval, "%d", keynum + 1);
      }

      if ( nbits == 34 ) {
         cmd2 = (buf[4] & 0xc0) >> 6;
         func = ksfunc[cmd];
         sprintf(outbuf, "%s rcva func %12s : Type KAKU_S ID 0x%07lx Cmd %s %s %s [Cmd2 0x%02x]\n",
           datstrf(), "RFdata", kaddr, funclabel[func], ulabel, uval, cmd2);
      }
      else if ( nbits == 38 ) {
         level = (buf[4] & 0xf0) >> 4;
         cmd2 = (buf[4] & 0x0c) >> 2;
         func = kpfunc[cmd];
         sprintf(outbuf, "%s rcva func %12s : Type KAKU_P ID 0x%07lx Cmd %s %s %s Level %d [Cmd2 0x%02x]\n",
           datstrf(), "RFdata", kaddr, funclabel[func], ulabel, uval, level, cmd2);
      }
      else {
         sprintf(outbuf, "%s rcva Unknown KAKU\n", datstrf());
      }
      return outbuf;
   }

   /* Built a combined HU bitmap for each housecode */
   compress_x10list(x10list, &nx10);

   actfunc  = func = (nbits == 34) ? ksfunc[cmd] : kpfunc[cmd];

   for ( n = 0; n < nx10; n++ ) {
      hcode = x10list[n].hcode;
      bitmap = x10list[n].bitmap;
      hc = code2hc(hcode);

      switch ( func ) {
         case KakuOnFunc :
            mask = kmodmask[KonMask][hcode];
            trig = KakuOnTrig;
            level = 15;
            sprintf(funcparmstr, "key %d", keynum + 1);
            break;
         case KakuOffFunc :
            mask = kmodmask[KoffMask][hcode];
            trig = KakuOffTrig;
            sprintf(funcparmstr, "key %d", keynum + 1);
            level = 0;
            break;
         case KakuGrpOnFunc :
            mask = kmodmask[KGonMask][hcode];
            trig = KakuGrpOnTrig;
            sprintf(funcparmstr, "grp %c", keynum + 'A');
            level = 15;
            break;
         case KakuGrpOffFunc :
            mask = kmodmask[KGoffMask][hcode];
            trig = KakuGrpOffTrig;
            sprintf(funcparmstr, "grp %c", keynum + 'A');
            level = 0;
            break;
         case KakuPreFunc :
            mask = kmodmask[KpreMask][hcode];
            level = (buf[4] & 0xf0) >> 4;
            trig = KakuPreTrig;
            sprintf(funcparmstr, "key %d level %d", keynum + 1, level);
            break;
         case KakuGrpPreFunc :
            mask = kmodmask[KGpreMask][hcode];
            level = (buf[4] & 0xf0) >> 4;
            trig = KakuGrpPreTrig;
            sprintf(funcparmstr, "grp %c level %d", keynum + 'A', level);
            break;
         default :
            mask = 0;
            trig = KakuUnkPreTrig;
            sprintf(funcparmstr, "key %d", keynum + 1);
            level = 0;
            break;
      }

      active = 0;

      switch ( func ) {
         case KakuOffFunc :
         case KakuGrpOffFunc :
            active = bitmap & mask;
            onstate = x10state[hcode].state[OnState];
            save_dimlevel(hcode, active & kmodmask[KresumeMask][hcode]);
            set_dimlevel(0, hcode, active);
            get_states(hcode, &onstate, &dimstate);
//            x10state[hcode].state[ChgState] = active &
            changestate = active &
              ((x10state[hcode].state[OnState] ^ onstate) |
               (x10state[hcode].state[DimState] ^ dimstate));
            x10state[hcode].state[OnState]  = onstate;
            x10state[hcode].state[DimState] = dimstate;
            x10state[hcode].state[LightsOnState] &= ~active;
            break;

         case KakuOnFunc :
         case KakuGrpOnFunc :
            active = bitmap & mask;
            onstate = x10state[hcode].state[OnState];
            resumask = kmodmask[KresumeMask][hcode];
            /* Resume-enabled units will go to saved brightness */
            restore_dimlevel(hcode, active & resumask);
            /* Modules which go to 'On' brightness */
            set_on_level(hcode, active & ~resumask & ~onstate);
            save_dimlevel(hcode, active & resumask);
            get_states(hcode, &onstate, &dimstate);
//            x10state[hcode].state[ChgState] =  /* active & */
            changestate = active &
              ((x10state[hcode].state[OnState] ^ onstate) |
               (x10state[hcode].state[DimState] ^ dimstate));

            x10state[hcode].state[OnState] = onstate;
            x10state[hcode].state[DimState] = dimstate;
            x10state[hcode].state[LightsOnState] &= ~active;
            break;

         case KakuPreFunc :
         case KakuGrpPreFunc :
            active = bitmap & mask;
            onstate = x10state[hcode].state[OnState];
            set_dimlevel(level, hcode, active);
            get_states(hcode, &onstate, &dimstate);
//            x10state[hcode].state[ChgState] = active &
            changestate = active &
              ((x10state[hcode].state[OnState] ^ onstate) |
               (x10state[hcode].state[DimState] ^ dimstate));
            x10state[hcode].state[OnState]  = onstate;
            x10state[hcode].state[DimState] = dimstate;
            x10state[hcode].state[LightsOnState] &= ~active;
            break;

         default :
            active = 0;
            break;
      }

      ucode = 0xff;

      startupstate = ~x10state[hcode].state[ValidState] & active;
      x10state[hcode].state[ValidState] |= active;

//      x10state[hcode].state[ModChgState] = (x10state[hcode].state[ModChgState] & ~active) | changestate;
      x10state[hcode].state[ModChgState] = changestate;

      x10state[hcode].state[ChgState] = x10state[hcode].state[ModChgState] |
        (x10state[hcode].state[ActiveChgState] & active) |
        (startupstate & ~kmodmask[KPhysMask][hcode]);

      changestate = x10state[hcode].state[ChgState];

      for ( k = 0; k < 16; k++ ) {
         if ( bitmap & (1 << k) ) {
            ucode = k;
            x10state[hcode].vident[ucode] = kaddr;
            x10state[hcode].vflags[ucode] = 0;
            x10state[hcode].timestamp[ucode] = time(NULL);
            x10state[hcode].state[ValidState] |= (1 << ucode);
         }
      }

      unit = code2unit(ucode);
      x10state[hcode].vaddress = bitmap;
      x10state[hcode].lastcmd = func;
      x10state[hcode].lastunit = unit;
      x10global.lasthc = hcode;
      x10global.lastaddr = 0;
      
      actfunc = 0;
      afuncmap = 0;
      kactfunc = func;
      kfuncmap = (1 << trig);
      trigaddr = active;

      launcherp = config.launcherp;

      bmaplaunch = launched = 0;
      j = 0;
      while ( launcherp && !breaker && launcherp[j].line_no > 0 ) {
         if ( launcherp[j].type != L_NORMAL ||
              launcherp[j].hcode != hcode ||
              is_unmatched_flags(&launcherp[j]) ||
              (launcherp[j].vflags & x10state[hcode].vflags[ucode]) != launcherp[j].vflags ||
	      (launcherp[j].notvflags & ~x10state[hcode].vflags[ucode]) != launcherp[j].notvflags ) {
            j++;
	    continue;
         }

         if ( launcherp[j].kfuncmap & kfuncmap &&
              launcherp[j].source & signal_source ) {
            trigactive = trigaddr & (mask | launcherp[j].signal);
            if ( (bmaplaunch = (trigactive & launcherp[j].bmaptrig) & 
//                   (x10state[hcode].state[ChgState] | ~launcherp[j].chgtrig)) ||
                   (changestate | ~launcherp[j].chgtrig)) ||
                   (launcherp[j].unitstar && !trigaddr)) {
               launcherp[j].matched = YES;
               *launchp = j;
               launched |= bmaplaunch;
               launcherp[j].actfunc = kactfunc;
               launcherp[j].genfunc = 0;
	       launcherp[j].xfunc = kactfunc;
	       launcherp[j].level = x10state[hcode].dimlevel[ucode];
               launcherp[j].bmaplaunch = bmaplaunch;
               launcherp[j].actsource = signal_source;
               if ( launcherp[j].scanmode & FM_BREAK ) {
                  breaker = 1;
                  break;
               }
            }
         }
         j++;
      }

      x10state[hcode].launched = launched;

      for ( ucode = 0; ucode < 16; ucode++ ) {
         if ( bitmap & (1 << ucode) ) {
            unit = code2unit(ucode);
            sprintf(outbuf + strlen(outbuf), "%s rcva      %12s : hu %c%d (%s)\n",
             datstrf(), "kAddress", hc, unit, lookup_label(hc, (1 << ucode)) );
         }
      } 
   }
   
   sprintf(outbuf + strlen(outbuf), "%s rcva func %12s : %s\n",
      datstrf(), funclabel[func], funcparmstr);
   
   if ( i_am_state )
      write_x10state_file();

   return outbuf;

#else
   return "";

#endif  /* HASKAKU */

  
}

/*----------------------------------------------------------------------------+
 | Translate Visonic RF data                                                  |
 | Buffer reference: ST_COMMAND, ST_VARIABLE_LEN, plus:                       |
 |  vtype, buflen,  buffer                                                    |
 |   [2]     [3]     [4+]                                                     |
 +----------------------------------------------------------------------------*/
char *translate_visonic ( unsigned char *xbuf, unsigned char *sunchanged, int *launchp )
{
   static char   outbuf[120];
   unsigned int  ident;
   unsigned char data;
   unsigned char *buf;

   extern char *datstrf ( void );

   buf = xbuf + 5;
   ident = (buf[4] << 16) | (buf[1] << 8) | buf[0];
   data = buf[2];

   sprintf(outbuf, "%s rcva func %12s : Type VISONIC ID 0x%06x Data 0x%02x\n",
      datstrf(), "RFdata", ident, data);

   *sunchanged = 0;
   *launchp = -1;

   return outbuf;
}

   

