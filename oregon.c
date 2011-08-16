
/*----------------------------------------------------------------------------+
 |                                                                            |
 |                  Oregon Sensor Support for HEYU                            |
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
#include <time.h>
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif

#include "x10.h"
#include "process.h"
#include "x10state.h"
#include "oregon.h"

extern int sptty;
extern int i_am_state, i_am_aux, i_am_relay, i_am_rfxmeter;
extern char *funclabel[];
extern void create_flagslist ( unsigned char, unsigned long, char * );
extern double celsius2temp ( double, char, double );
extern double temp2celsius ( double, char, double );
extern void intvstrfunc ( char *, long *, time_t, time_t );
extern void update_activity_timeout ( ALIAS *, int );

extern CONFIG config;
extern CONFIG *configp;

extern struct x10global_st x10global;
extern x10hcode_t *x10state;

extern unsigned int modmask[NumModMasks][16];
extern int lock_for_write(), munlock();

extern unsigned int   ore_ignore[];
extern int            ore_ignore_size;

#if 0
int str2hex ( char *str, unsigned char *buf, int *bufsiz )
{
   char minibuf[16];
   char *sp;
   int  j;
   
   if ( (strlen(str) % 2) != 0 ) {
      fprintf(stderr, "Invalid string length %d\n", strlen(str));
      return 1;
   }

   sp = str;

   j = 0;
   while ( *sp != '\0' ) {
      minibuf[0] = *sp++;
      minibuf[1] = *sp++;
      minibuf[2] = '\0';
      buf[j++] = (unsigned char)strtol(minibuf, NULL, 16);
   }
   if ( buf[0] != 8 * (j - 1) ) {
      fprintf(stderr, "Miscount: buf[0] = %d, bufsiz = %d\n", buf[0]/8, j - 1);
      return 1;
   }

   memmove(buf, buf + 1, j - 1);
   *bufsiz = j - 1;

   return 0;
}

unsigned int bcd2dec ( unsigned int bcd, int digits )
{
   unsigned int dec = 0, mult = 1, mask =0x0f;
   int          j;

   for ( j = 0; j < digits; j++ ) {
      dec += mult * ((bcd & mask) >> (4 * j));
      mult *= 10;
      mask <<= 4;
   }
   return dec;
}
#endif


int is_ore_ignored ( unsigned int saddr )
{
#ifdef HASORE
   int j = 0;

   while ( j < ore_ignore_size ) {
      if ( saddr == ore_ignore[j] ) {
         return 1;
      }
      j++;
   }
#endif
   return 0;
}

#ifdef HASORE

char *forecast_txt( int fcast )
{
   static char *text[] = {"", "Sunny ", "PtCloudy ", "Cloudy ", "Rain "};
   return text[fcast & 0x07];
}

unsigned char forecast ( unsigned char rawcode )
{
   return (rawcode == 0x0Cu) ? 1 :
          (rawcode == 0x06u) ? 2 :
          (rawcode == 0x02u) ? 3 :
          (rawcode == 0x03u) ? 4 : 0;
}

int wind_dir ( unsigned char *buf, unsigned char mode )
{
   /* Wind direction as decidegrees (0-3599) */
   if ( mode == 1 ) {
      /* OreWind1 and OreWind2 */
      return ((buf[4] & 0xf0) >> 4) * 225;
   }
   else if ( mode == 2 ) {
      /* OreWind3 */
      return ((buf[5] & 0xf0) >> 4) * 1000 +
             (buf[5] & 0x0f) * 100 +
             ((buf[4] & 0xf0) >> 4) * 10;
   }

   return 0;
}

char *compass_point ( double windir )
{
   return (windir <  11.26) ? "N"   :
          (windir <  33.76) ? "NNE" :
          (windir <  56.26) ? "NE"  :
          (windir <  78.76) ? "ENE" :
          (windir < 101.26) ? "E"   :
          (windir < 123.76) ? "ESE" :
          (windir < 146.26) ? "SE"  :
          (windir < 168.76) ? "SSE" :
          (windir < 191.26) ? "S"   :
          (windir < 213.76) ? "SSW" :
          (windir < 236.26) ? "SW"  :
          (windir < 258.76) ? "WSW" :
          (windir < 281.26) ? "W"   :
          (windir < 303.76) ? "WNW" :
          (windir < 326.26) ? "NW"  :
          (windir < 348.76) ? "NNW" : "N";
}

/*-----------------------------------------------------+
 | Beaufort scale for wind speeds in decimeters/second |
 +-----------------------------------------------------*/
int beaufort_scale ( int dwind )
{
   return (dwind <   2) ?  0 :
          (dwind <  16) ?  1 :
          (dwind <  34) ?  2 :
          (dwind <  55) ?  3 :
          (dwind <  80) ?  4 :
          (dwind < 108) ?  5 :
          (dwind < 139) ?  6 :
          (dwind < 172) ?  7 :
          (dwind < 208) ?  8 :
          (dwind < 254) ?  9 :
          (dwind < 285) ? 10 :
          (dwind < 327) ? 11 : 12;
}
   
  
unsigned char nybble_sum( int nbytes, unsigned char *buf )
{
   int   j;
   unsigned char sum = 0;

   for ( j = 0; j < nbytes; j++ ) {
      sum += (buf[j] & 0x0f) + ((buf[j] >> 4) & 0x0f);
   }
   return sum;
}
      
unsigned char cse ( unsigned char *buf )
{
   short ssum;

   ssum = nybble_sum(7, buf) - buf[7];
   
   return (unsigned char)(ssum & 0xff) - 0x18u;
}   

unsigned char cs7 ( unsigned char *buf )
{
   short ssum;

   ssum = nybble_sum(7, buf) - buf[7];
   
   return (unsigned char)(ssum & 0xff) - 0x0Au;
}   

unsigned char cs8 ( unsigned char *buf )
{
   short ssum;

   ssum = nybble_sum(8, buf) - buf[8];
   
   return (unsigned char)(ssum & 0xff) - 0x0Au;
}

unsigned char cs9 ( unsigned char *buf )
{
   short ssum;

   ssum = nybble_sum(9, buf) - buf[9];
   
   return (unsigned char)(ssum & 0xff) - 0x0Au;
}   

unsigned char cs10 ( unsigned char *buf )
{
   short ssum;

   ssum = nybble_sum(10, buf) - buf[10] ;
   
   return (unsigned char)(ssum & 0xff) - 0x0Au;
}

unsigned char cs11 ( unsigned char *buf )
{
   short ssum;

   ssum = nybble_sum(11, buf) - buf[11] ;
   
   return (unsigned char)(ssum & 0xff) - 0x0Au;
}

unsigned char cs12 ( unsigned char *buf )
{
   short ssum;

   ssum = nybble_sum(10, buf + 1) + (buf[11] & 0x0f);
   ssum -= ((buf[12] & 0x0f) << 4) + ((buf[11] >> 4) & 0x0f);

   return (unsigned char)ssum & 0xff;
}

unsigned char csw ( unsigned char *buf )
/* Note: Same as csWHL() */
{
   short ssum;

   ssum = nybble_sum(6, buf) + (buf[6] & 0x0f)  
            - ((buf[6] >> 4) & 0x0f) - ((buf[7] << 4) & 0xf0);

   return (unsigned char)(ssum & 0xff) - 0x0Au;
}

unsigned char cs2HL ( unsigned char *buf )
{
  short ssum;

  ssum = nybble_sum(8, buf) + (buf[8] & 0x0f) -
        ((buf[8] >> 4) & 0x0f) - ((buf[9] << 4) & 0xf0);

  return (unsigned char)(ssum & 0xff) - 0x0Au;
}

unsigned char csRHL ( unsigned char *buf )
{
  short ssum;

  ssum = nybble_sum(9, buf) + (buf[9] & 0x0f)
         - ((buf[9] >> 4) & 0x0f) - ((buf[10] << 4) & 0xf0);
  
  return (unsigned char)(ssum & 0xff) - 0x0A;
}

unsigned char csWHL ( unsigned char *buf )
/* Note: Same as csw() */
{
   short ssum;

   ssum = nybble_sum(6, buf) + (buf[6] & 0x0f) - 
          ((buf[6] >> 4) & 0x0f) - ((buf[7] << 4) & 0xf0);

   return (unsigned char)(ssum & 0xff) - 0x0Au;
}

unsigned char csWgt1 ( unsigned char *buf )
{
   if ( (buf[0] & 0xf0) != (buf[5] & 0xf0) ||
        (buf[1] & 0x0f) != (buf[6] & 0x0f) ||
        (buf[6] & 0xf0) != 0x10            ||
        (buf[5] & 0x0e) != 0x00               )
      return 1;
   return 0;
}

unsigned char csNull ( unsigned char *buf )
{
  return 0;
}

unsigned char csFail ( unsigned char *buf )
{
  return 1;
}

/* Types
GR101:    GR101
TEMP1:    THR128,THx138
TEMP2:    THN132N,THWR288
TEMP3:    THWR800
TH1:      THGN122N,THGR122NX,THGR228N,THGR268
TH2:      THGR810
TH3:      RTGR328N
TH4:      THGR328N
TH5:      WTGR800
TH6:      THGR918
THB1:     BTHR918
THB2:     BTHR918N,BTHR968
RAIN1:    RGR126,RGR682,RGR918
RAIN2:    PCR800
RAIN3:
WIND1:    WTGR800
WIND2:    WGR800
WIND3:    STR918,WGR918
UV1:      UVR138
UV2:      UVN800
DT1:      RTGR328N
ELEC1:    cent-a-meter
ELEC2:    OWL CM119
WEIGHT1:  BWR101,BWR102
*/

/* Channel modes */
enum {CM0, CM1, CM2, CM3};

/* Battery modes */
enum {BM0, BM1, BM2, BM3, BM4};

static struct oregon_st {
   unsigned char  minbits;
   unsigned char  trulen;
   unsigned char  id0mask;
   unsigned char  id1mask;
   unsigned char  id0;
   unsigned char  id1;
   int            nvar;
   unsigned char  chanmode;
   unsigned char  batmode;
   unsigned char  specmode; /* Special handling */
   unsigned short bpoffset;
   unsigned char  support_mode;  /* 0 = Unsupported; 1 = Normal sensor; 2 = Dummy sensor */
   unsigned char  addr_mask;
   int            addr_byte;
   char           *slabel;
   unsigned char  subtype;
   unsigned char  (*chksumf)(unsigned char *);
} orechk[] = {
   {88,  11, 0xff, 0xff, 0x5a, 0x5d, 3, CM1, BM1, 0, 795, 1, 0xff, 4, "ORE_THB1",  OreTHB1,    cs10  },
   {88,  11, 0xff, 0xff, 0x5a, 0x6d, 3, CM1, BM1, 0, 856, 1, 0xff, 4, "ORE_THB2",  OreTHB2,    cs10  },
   {84,  11, 0xff, 0xff, 0x2a, 0x19, 2, CM0, BM1, 0, 0,   1, 0xff, 4, "ORE_RAIN2", OreRain2,   csRHL },
   {84,  11, 0xff, 0xff, 0x06, 0xe4, 2, CM0, BM1, 0, 0,   1, 0xff, 4, "ORE_RAIN3", OreRain3,   csRHL },
   {80,  10, 0xff, 0xff, 0x2a, 0x1d, 2, CM0, BM1, 0, 0,   1, 0xff, 4, "ORE_RAIN1", OreRain1,   cs2HL },
   {80,  10, 0xff, 0xff, 0x1a, 0x99, 3, CM0, BM1, 1, 0,   1, 0xff, 4, "ORE_WIND1", OreWind1,   cs9   },
   {80,  10, 0xff, 0xff, 0x1a, 0x89, 3, CM0, BM1, 0, 0,   1, 0xff, 4, "ORE_WIND2", OreWind2,   cs9   },
   {80,  10, 0xff, 0xff, 0x3a, 0x0d, 3, CM0, BM2, 0, 0,   1, 0xff, 4, "ORE_WIND3", OreWind3,   cs9   },
   {72,   9, 0xff, 0xff, 0x0a, 0x4d, 1, CM1, BM1, 0, 0,   1, 0xff, 4, "ORE_T1",    OreTemp1,   cs8   },
   {72,   9, 0xff, 0xff, 0x1a, 0x2d, 2, CM1, BM1, 0, 0,   1, 0xff, 4, "ORE_TH1",   OreTH1,     cs8   },
   {72,   9, 0xff, 0xff, 0xfa, 0x28, 2, CM2, BM1, 0, 0,   1, 0xff, 4, "ORE_TH2",   OreTH2,     cs8   },
   {72,   9, 0x0f, 0xff, 0x0a, 0xcc, 2, CM2, BM1, 0, 0,   1, 0xff, 4, "ORE_TH3",   OreTH3,     cs8   },
   {72,   9, 0xff, 0xff, 0xca, 0x2c, 2, CM2, BM1, 0, 0,   1, 0xff, 4, "ORE_TH4",   OreTH4,     cs8   },
   {72,   9, 0xff, 0xff, 0xfa, 0xb8, 2, CM0, BM2, 0, 0,   1, 0xff, 4, "ORE_TH5",   OreTH5,     cs8   },
   {72,   9, 0xff, 0xff, 0x1a, 0x3d, 2, CM1, BM2, 0, 0,   1, 0xff, 4, "ORE_TH6",   OreTH6,     cs8   },
   {64,   8, 0xff, 0xff, 0xda, 0x78, 1, CM0, BM1, 0, 0,   1, 0xff, 4, "ORE_UV2",   OreUV2,     cs7   },
   {60,   8, 0xff, 0xff, 0xea, 0x7c, 1, CM0, BM1, 0, 0,   1, 0xff, 4, "ORE_UV1",   OreUV1,     csWHL },
   {60,   8, 0xff, 0xff, 0xea, 0x4c, 1, CM1, BM1, 0, 0,   1, 0xff, 4, "ORE_T2",    OreTemp2,   csw   },
   {60,   8, 0xff, 0xff, 0xca, 0x48, 1, CM1, BM1, 0, 0,   1, 0xff, 4, "ORE_T3",    OreTemp3,   csw   },
   {56,   7, 0x0f, 0x00, 0x0c, 0x00, 1, CM0, BM0, 0, 0,   1, 0xf0, 2, "ORE_WGT1",  OreWeight1, csWgt1},
   {64,   8, 0xff, 0x80, 0xea, 0x00, 1, CM0, BM3, 0, 0,   1, 0xff, 3, "ELS_ELEC1", OreElec1,   cse   },
   {108, 11, 0xff, 0x00, 0x1a, 0x00, 2, CM3, BM0, 0, 0,   1, 0xff, 3, "OWL_ELEC2", OreElec2,   cs12  },
   {108, 11, 0xff, 0x00, 0x2a, 0x00, 2, CM3, BM0, 0, 0,   1, 0xff, 3, "OWL_ELEC2", OreElec2,   cs12  },
   {108, 11, 0xff, 0x00, 0x3a, 0x00, 2, CM3, BM0, 0, 0,   1, 0xff, 3, "OWL_ELEC2", OreElec2,   cs12  },

   {88,  11, 0xff, 0xff, 0xf0, 0xf0, 1, CM0, BM0, 0, 0,   2, 0xff, 4, "ORE_TEMU",   OreTemu,   csFail}, /* Dummy */
   {88,  11, 0xff, 0xff, 0xf0, 0xf0, 2, CM0, BM0, 0, 0,   2, 0xff, 4, "ORE_THEMU",  OreTHemu,  csFail}, /* Dummy */
   {88,  11, 0xff, 0xff, 0xf0, 0xf0, 3, CM0, BM0, 0, 0,   2, 0xff, 4, "ORE_THBEMU", OreTHBemu, csFail}, /* Dummy */

   /* Unsupported Oregon sensors */
   {96,  12, 0xff, 0xff, 0x8a, 0xec, 2, CM2, BM2, 0, 0,   0, 0xff, 4, "ORE_DT1",   OreDT1,     cs11  },
   {64,   8, 0x0f, 0x00, 0x03, 0x00, 2, CM0, BM0, 0, 0,   0, 0xf0, 2, "ORE_WGT2",  OreWeight2, csNull},    /* OreGR101 */

};
int orechk_size = sizeof(orechk)/sizeof(struct oregon_st);

unsigned char battery_status ( char *batstr, unsigned char *batlvl,
                        unsigned char *buf, unsigned char mode )
{
   unsigned char status;

   *batstr = '\0';
   *batlvl = 0;

   switch ( mode) {
      case BM0 :  /* Battery field absent or unknown */
         status = 0;
         break;
      case BM1 :  /* Has only low battery flag */
         status = (buf[4] & 0x04) ? ORE_LOBAT : 0;
         break;
      case BM2 :  /* Has battery level */
         status = ORE_BATLVL;
         *batlvl = 10 - (buf[4] & 0x0fu);
         sprintf(batstr, "BatLvl %d%% ", *batlvl * 10);
         if ( (*batlvl * 10) <= configp->ore_lobat )
            status |= ORE_LOBAT;
         break;
      case BM3 :  /* Electrisave */
         status = (buf[1] & 0x10) ? ORE_LOBAT : 0;
         break;
      default :
         status = 0;
         break;
   }
   return status;
}


unsigned char channelval( char *chstr, unsigned char *buf, unsigned char mode )
{
   unsigned char chval;

   switch ( mode ) {
      case CM0 :  /* Channel field absent or unknown */
         chval = 1;
         sprintf(chstr, "Ch 1 ");
         break;
      case CM1 :  /* 3-bit channel indicator */
         switch ( buf[2] & 0x70 ) {
            case 0 :
            case 0x10 : chval = 1; sprintf(chstr, "Ch 1 "); break;
            case 0x20 : chval = 2; sprintf(chstr, "Ch 2 "); break;
            case 0x40 : chval = 3; sprintf(chstr, "Ch 3 "); break;
            default :   chval = 0; sprintf(chstr, "Ch ? "); break;
         }
         break;
      case CM2 : /* 4-bit channel value */
         chval = (buf[2] >> 4) & 0x0f;
         sprintf(chstr, "Ch %d ", chval);
         break;
      case CM3: /* Owl CM119 */
         switch ( buf[0] & 0xf0 ) {
            case 0x10 : chval = 1; sprintf(chstr, "Ch 1 "); break;
            case 0x20 : chval = 2; sprintf(chstr, "Ch 2 "); break;
            case 0x30 : chval = 3; sprintf(chstr, "Ch 3 "); break;
            default :   chval = 0; sprintf(chstr, "Ch ? "); break;
         }
         break;
      default :
         chval = 0;
         *chstr = '\0';
         break;
   }
   return chval;
}


#endif /* HASORE */

 
unsigned char oretype ( unsigned char *xbuf, unsigned char *subindx, unsigned char *subtype,
                        unsigned char *trulen, unsigned long *addr, int *nseq )
{
#ifdef HASORE
   int j = 0;

   while ( j < orechk_size ) {
      if ( ((xbuf[0] & 0x7f) >= orechk[j].minbits) &&
           (xbuf[1] & orechk[j].id0mask) == orechk[j].id0 &&
           (xbuf[2] & orechk[j].id1mask) == orechk[j].id1 &&
           orechk[j].chksumf(xbuf + 1) == 0 ) {
         *trulen = orechk[j].trulen;
         *addr = (unsigned short)(xbuf[orechk[j].addr_byte] & orechk[j].addr_mask);
         /* If transmitter and address are shared by two sensors, modify one address */
         if ( orechk[j].specmode == 1 ) {
            *addr = (*addr + 1) & 0x00ff;
         }
         *nseq = (int)orechk[j].nvar;
         *subindx = j;
         *subtype = orechk[j].subtype;
         if ( orechk[j].support_mode || configp->disp_ore_all == YES ) {
            return 0;
         }
         else {
            return 1;
         }
      }
      j++;
   }
   return 1;
#else
   return 1;
#endif /* HASORE */
}


/*----------------------------------------------------------------------------+
 | Translate Oregon RF data                                                   |
 | Buffer reference: ST_COMMAND, ST_GENLONG, plus:                            |
 |  vtype, subindx, vidhi, vidlo, seq, buflen, buffer                         |
 |   [2]     [3]     [4]    [5]   [6]    [7]    [8+]                          |
 +----------------------------------------------------------------------------*/
char *translate_oregon( unsigned char *buf, unsigned char *sunchanged, int *launchp )
{
#ifdef HASORE
   static char    outbuf[160];
   static char    intvstr[32];
   long           intv;
   char           flagslist[80];
   ALIAS          *aliasp;
   LAUNCHER       *launcherp;
   extern unsigned int signal_source;
   unsigned char  func, *vdatap, vtype, subindx, subtype, seq, counter, unchanged;
   int            humid, lasthumid;
   int            deciamps, lastdeciamps;
   int            dspeed, lastdspeed, davspeed, lastdavspeed, direction, lastdirection;
   int            baro, lastbaro;
   unsigned short vident, idmask;
   int            unit, loc, statusoffset;
   int            dtempc, lastdtempc, nvar;
   int            cweight, lastcweight;
   int            buflen;
   unsigned long  longvdata, longvdata2, longvdata3;
   long           delta, tipfactor;
   unsigned long  drain, lastdrain, train, multiplier;
   int            uvfactor, lastuvfactor;

   char           hc;

   unsigned char  actfunc, oactfunc;
   unsigned int   trigaddr, mask, active, trigactive;
   unsigned int   launched, bmaplaunch;
   unsigned long  afuncmap, ofuncmap;

   int            j, k, found, index = -1;
   unsigned char  hcode, ucode, trig = 0;
   unsigned int   bitmap = 0, changestate;
   unsigned long  vflags = 0, vflags_mask = 0;

   static unsigned long prevdata[3], lastchdata[3];
   static unsigned int  startupstate;
   static char          chstr[8];
   static unsigned char channel;
   static unsigned char blevel, wgtnum;
   static unsigned char status;
   static unsigned char batchange;
   unsigned char        batstatus;
   static char          batstr[32];
   static char          pwrstr[32];
   static char          bftstr[16];
   char                 cntrstr[16];

   unsigned long        *prevvalp, *lastchvalp;

   unsigned long        uknbytes;
   unsigned char        fcast;
   char                 *fcast_txt;
   char                 minibuf[32];
   unsigned char        extra;
   char                 rawstring[32];
   char                 flipstr[32];
   char                 *unitstring = "";
   int                  minroll;

   unsigned long        energyhi, energylo, power, lastchpower, chgbits;
   unsigned long long   energyll, lastchll;
   long long            deltall;

   double temp, dbaro, weight, dbldir;
   int  ct1, ct2, ct3;

   launcherp = configp->launcherp;
   aliasp = configp->aliasp;

   *launchp = -1;
   *sunchanged = unchanged = 0;
   flagslist[0] = '\0';
   *outbuf = '\0';
   changestate = 0;


   vtype   = buf[2];
   subindx = buf[3];
   seq     = buf[6];
   buflen  = buf[7];
   vdatap  = buf + 8;
   func    = VdataFunc;

   subtype = orechk[subindx].subtype;

   channel = channelval(chstr, vdatap, orechk[subindx].chanmode);

   if ( configp->oreid_16 == YES )
      vident = (unsigned short)buf[5] | (channel << 8);
   else
      vident = (unsigned short)buf[5];

   x10global.longvdata = 0;
   x10global.lastvtype = vtype;

   hcode = ucode = 0;
   found = 0;
   j = 0;
   /* Look up the alias, if any */
   idmask = configp->oreid_mask;
   while ( !found && aliasp && aliasp[j].line_no > 0 ) {
      if ( aliasp[j].vtype == vtype && aliasp[j].subtype == subtype && 
           (bitmap = aliasp[j].unitbmap) > 0 &&
           (ucode = single_bmap_unit(aliasp[j].unitbmap)) != 0xff ) {
         for ( k = 0; k < aliasp[j].nident; k++ ) {
            if ( (aliasp[j].ident[k] & idmask) == (vident & idmask) ) {
               index = j;
               found = 1;
               break;
            }
         }
      }
      j++;
   }

   if ( !found || !aliasp ) {
      if ( seq == 1 ) {
         sprintf(outbuf, "func %12s : Type %s %sID 0x%02X Data 0x",
            "RFdata", orechk[subindx].slabel, chstr, vident);
         for ( j = 0; j < buflen; j++ ) {
            sprintf(outbuf + strlen(outbuf), "%02x", vdatap[j]);
         }
      }
      return outbuf;
   }

   nvar = orechk[subindx].nvar;

   
   hc = aliasp[index].housecode;
   hcode = hc2code(hc);
   unit = code2unit(ucode);
   
   longvdata2 = longvdata3 = 0;

   /* Whether or not to display channel in output */
   if ( configp->ore_display_chan == NO )
      *chstr = '\0';

   batstatus = battery_status(batstr, &blevel, vdatap, orechk[subindx].batmode);
   if ( configp->ore_display_batlvl == NO )
      *batstr = '\0';

   *rawstring = '\0';
   *flipstr = '\0';
   *bftstr = '\0';
   *cntrstr = '\0';

   /* Save interval since last transmission */
   if ( seq == 1 ) {
      intvstrfunc(intvstr, &intv, time(NULL), x10state[hcode].timestamp[ucode]);
      x10global.interval = intv;
   }

   switch ( subtype ) {
      case OreWind1 :
      case OreWind2 :
      case OreWind3 :
         loc = aliasp[index].storage_index;

         if ( seq == 1 ) {
            /* Wind Average Speed */

            vflags_mask = SEC_LOBAT;
            longvdata = (unsigned long)(channel & 0x0fu) << ORE_CHANSHFT;

            status = ORE_VALID;

            status |= batstatus;

            longvdata |= status | ((unsigned long)blevel << ORE_BATSHFT);

            /* Get previous data */
            for ( j = 0; j < nvar; j++ ) {
               prevdata[j] = x10global.data_storage[loc + j];
               lastchdata[j] = x10global.data_storage[loc + nvar + j];
            }

            batchange = (batstatus ^ (unsigned char)prevdata[0]) & ORE_LOBAT;

            /* Average speed in decimeters/sec */
            davspeed = ((vdatap[8] & 0xf0) >> 4) * 100 + (vdatap[8] & 0x0f) * 10 + ((vdatap[7] & 0xf0) >> 4);
            longvdata |= (unsigned long)davspeed << ORE_DATASHFT;

            if ( configp->ore_display_count == YES )
               sprintf(rawstring, "[%u] ", davspeed);

            if ( configp->ore_display_bft == YES )
               sprintf(bftstr, " %dbft", beaufort_scale(davspeed));  

            lastdavspeed = (lastchdata[seq - 1] & ORE_DATAMSK) >> ORE_DATASHFT;
            delta = abs(davspeed - lastdavspeed);
            unchanged = (delta < configp->ore_chgbits_wavsp) ? 1 : 0;

            func = OreWindAvSpFunc;
            trig = OreWindAvSpTrig;
            vflags |= (status & ORE_LOBAT) ? SEC_LOBAT : 0;

            create_flagslist (vtype, vflags, flagslist);

            sprintf(outbuf, "func %12s : hu %c%-2d %sAvSpeed %.1f%s%s %s%s%s(%s)",
               funclabel[func], hc, unit, chstr, ((double)davspeed / 10.0) * configp->ore_windscale, configp->ore_windunits,
               bftstr, rawstring, batstr, flagslist, aliasp[index].label);

         }
         else if ( seq == 2 ) {
            /* Wind Instantaneous Speed */
            vflags_mask = SEC_LOBAT;
            longvdata = (unsigned long)(channel & 0x0fu) << ORE_CHANSHFT;

            status = ORE_VALID;
            status |= batstatus;

            longvdata |= status | ((unsigned long)blevel << ORE_BATSHFT);

            /* Process wind speeds as decimeters/sec */
            dspeed = (vdatap[7] & 0x0f) * 100 +
                     ((vdatap[6] & 0xf0) >> 4) * 10 + (vdatap[6] & 0x0f);

            if ( configp->ore_display_count == YES )
               sprintf(rawstring, "[%u] ", dspeed);

            if ( configp->ore_display_bft == YES )
               sprintf(bftstr, " %dbft", beaufort_scale(dspeed));  

            longvdata |= (unsigned long)dspeed << ORE_DATASHFT;

            lastdspeed = (lastchdata[seq - 1] & ORE_DATAMSK) >> ORE_DATASHFT;

            delta = abs(dspeed - lastdspeed);
            unchanged = (delta < configp->ore_chgbits_wsp) ? 1 : 0;

            func = OreWindSpFunc;
            trig = OreWindSpTrig;
            vflags |= (status & ORE_LOBAT) ? SEC_LOBAT : 0;

            create_flagslist (vtype, vflags, flagslist);

            sprintf(outbuf, "func %12s : hu %c%-2d %sSpeed %.1f%s%s %s%s%s(%s)",  
               funclabel[func], hc, unit, chstr,
               ((double)dspeed / 10.0) * configp->ore_windscale, configp->ore_windunits,
               bftstr, rawstring, batstr, flagslist, aliasp[index].label);

         }
         else if ( seq == 3 ) {
            /* Process wind direction data as decidegrees (0-3599) */
            vflags_mask = SEC_LOBAT;
            longvdata = (unsigned long)(channel & 0x0fu) << ORE_CHANSHFT;

            status = ORE_VALID;
            status |= batstatus;

            longvdata |= status | ((unsigned long)blevel << ORE_BATSHFT);


            if ( subtype == OreWind3 ) {
               direction = ((vdatap[5] & 0xf0) >> 4) * 1000 +
                           (vdatap[5] & 0x0f) * 100 + ((vdatap[4] & 0xf0) >> 4) * 10;
            }
            else {
               direction = ((vdatap[4] & 0xf0) >> 4) * 225;
            }
            longvdata |= (unsigned long)direction << ORE_DATASHFT;

            lastdirection = (lastchdata[seq - 1] & ORE_DATAMSK) >> ORE_DATASHFT;

            if ( !(lastchdata[seq -1] & ORE_VALID) ) {
               unchanged = 0;
            }
            else if ( subtype == OreWind3 ) {
               delta = (direction - lastdirection);
               if ( delta > 1800 )
                  delta -= 3600;
               else if ( delta < -1800 )
                  delta += 3600;
               unchanged = ((abs(delta) / 10) < configp->ore_chgbits_wdir) ? 1 : 0;
            }
            else {
               delta = ((direction - lastdirection) / 225 + 0x10) % 0x10;
               unchanged = (abs(delta) < configp->ore_chgbits_wdir) ? 1 : 0;
            }
               
            func = OreWindDirFunc;
            trig = OreWindDirTrig;
            vflags |= (status & ORE_LOBAT) ? SEC_LOBAT : 0;

            create_flagslist(vtype, vflags, flagslist);

            direction = (direction + configp->ore_windsensordir + 3600) % 3600;

            dbldir = (double)direction / 10.0;
            *minibuf = '\0';
            if ( configp->ore_winddir_mode & COMPASS_ANGLE )
               sprintf(minibuf + strlen(minibuf), "%.1fdeg ", dbldir);
            if ( configp->ore_winddir_mode & COMPASS_POINTS )
               sprintf(minibuf + strlen(minibuf), "%s ", compass_point(dbldir));

            sprintf(outbuf, "func %12s : hu %c%-2d %sDir %s%s%s(%s)",
               funclabel[func], hc, unit, chstr, minibuf, 
               batstr, flagslist, aliasp[index].label);
         }
         else {
            longvdata = 0;
            break;
         }

         x10global.longvdata = longvdata;

         x10global.data_storage[loc + seq - 1] = longvdata;
         x10state[hcode].longdata = longvdata;

         if ( unchanged == 0 ) {
//            x10state[hcode].state[ChgState] |= bitmap;
            changestate |= bitmap;
            x10global.data_storage[loc + nvar + seq - 1] = longvdata;
         }
         else {
//            x10state[hcode].state[ChgState] &= ~bitmap;
            changestate &= ~bitmap;
         }

         break;

      case OreRain1 :
      case OreRain2 :
      case OreRain3 :
         loc = aliasp[index].storage_index;
         func = aliasp[index].funclist[seq - 1];
         statusoffset = aliasp[index].statusoffset[seq - 1];

         /* Rain data storage. Note that here we are using one extra storage */
         /* location each for the 32-bit rate and total rain. I.e.,          */
         /*  [loc]     => Rate status, channel, and battery                  */
         /*  [loc + 1] => Rate 32-bit value                                  */
         /*  [loc + 2] => Last changed Rate status, channel, and battery     */
         /*  [loc + 3] => Last changed Rate 32-bit value                     */
         /*  [loc + 4] => Total status, channel, and battery                 */
         /*  [loc + 5] => Total 32-bit value                                 */
         /*  [loc + 6] => Last changed total status, channel, and battery    */
         /*  [loc + 7] => Last changed Total 32-bit value                    */

         extra = 0;
         *flipstr = '\0';

         if ( func == OreRainRateFunc ) {
            /* Rainfall Rate */
            vflags_mask = SEC_LOBAT;
            loc = aliasp[index].storage_index;
            status = 0;

            longvdata = (unsigned long)(channel & 0x0fu) << ORE_CHANSHFT;

            status = ORE_VALID | batstatus;
            longvdata |= status | ((unsigned long)blevel << ORE_BATSHFT);

            prevvalp = &x10global.data_storage[loc + statusoffset];
            lastchvalp = &x10global.data_storage[loc + statusoffset + 2];

            if ( seq == 1 )
               batchange = (batstatus ^ (unsigned char)(*prevvalp)) & ORE_LOBAT;

            /* Process rainfall rate */
            drain = 0;
            multiplier = 1;
            if ( subtype == OreRain1 ) {
               drain = ((vdatap[5] & 0xf0) >> 4) * 100 + (vdatap[5] & 0x0f) * 10 +
                       ((vdatap[4] & 0xf0) >> 4) ;
               multiplier = 1000;
               tipfactor = 1;
               unitstring = (*configp->ore_rainrateunits) ? configp->ore_rainrateunits : "mm/hr";
            }
            else if ( subtype == OreRain2 ) {
               drain = ((vdatap[5] & 0xf0) >> 4) * 1000 + (vdatap[5] & 0x0f) * 100 +
                       ((vdatap[4] & 0xf0) >> 4) * 10 +
                       (vdatap[6] & 0x0f);
               tipfactor = 10;
               unitstring = (*configp->ore_rainrateunits) ? configp->ore_rainrateunits : "inches/hr";
            }
            else  {
               drain = ((vdatap[5] & 0xf0) >> 4) * 1000 + (vdatap[5] & 0x0f) * 100 +
                       ((vdatap[4] & 0xf0) >> 4) * 10 + (vdatap[4] & 0x0f);
               tipfactor = 10;
               unitstring = (*configp->ore_rainrateunits) ? configp->ore_rainrateunits : "inches/hr";
            }

            if ( configp->ore_display_count == YES )
               sprintf(rawstring, "[%lu] ", drain);

            drain *= multiplier;

            longvdata2 = drain;

            lastdrain = *(lastchvalp + 1);

            delta = abs(drain - lastdrain) / multiplier;
            unchanged = ((delta/tipfactor) < configp->ore_chgbits_rrate) ? 1 : 0;

            trig = OreRainRateTrig;
            vflags |= (status & ORE_LOBAT) ? SEC_LOBAT : 0;

            create_flagslist (vtype, vflags, flagslist);

            sprintf(outbuf, "func %12s : hu %c%-2d %sRate "FMT_ORERRATE"%s %s%s%s%s(%s)",  
               funclabel[func], hc, unit, chstr, ((double)drain / 1000.0) * configp->ore_rainratescale, unitstring,
               rawstring, flipstr, batstr, flagslist, aliasp[index].label);

         }
         else if ( func == OreRainTotFunc ) {
            /* Process Total Rain data */
            vflags_mask = SEC_LOBAT;
            longvdata = (unsigned long)(channel & 0x0fu) << ORE_CHANSHFT;

            status = ORE_VALID | batstatus;

            longvdata |= status | ((unsigned long)blevel << ORE_BATSHFT);

            prevvalp = &x10global.data_storage[loc + statusoffset];
            lastchvalp = &x10global.data_storage[loc + statusoffset + 2];

            if ( seq == 1 )
               batchange = (batstatus ^ (unsigned char)(*prevvalp)) & ORE_LOBAT;

            if ( subtype == OreRain1 ) {
               train = (vdatap[8] & 0x0f) * 1000 +
                       ((vdatap[7] & 0xf0) >> 4) * 100 + (vdatap[7] & 0x0f) * 10 +
                       ((vdatap[6] & 0xf0) >> 4);
               unitstring = (*configp->ore_raintotunits) ? configp->ore_raintotunits : "mm";
               minroll = 25u;
               multiplier = 1000;
               tipfactor = 1;
            }
            else  {
               train = (vdatap[9] & 0x0f) * 100000 +
                       ((vdatap[8] & 0xf0) >> 4) * 10000 + (vdatap[8] & 0x0f) * 1000 +
                       ((vdatap[7] & 0xf0) >> 4) * 100 + (vdatap[7] & 0x0f) * 10 +
                       ((vdatap[6] & 0xf0) >> 4);
               multiplier = 1;
               tipfactor = 39;
               unitstring = (*configp->ore_raintotunits) ? configp->ore_raintotunits : "inches";
               minroll = 1000u;
            }

            if ( configp->ore_display_count == YES )
               sprintf(rawstring, "[%lu] ", train);

            train *= multiplier;

            longvdata2 = train;

            unchanged = 0;

            /* Determine change and/or rollover here */

            lastdrain = *(lastchvalp + 1);
            delta = abs(train - lastdrain) / multiplier;
            unchanged = ((delta/tipfactor) < configp->ore_chgbits_rtot) ? 1 : 0;

            if ( *(prevvalp + 1) > minroll && train < *(prevvalp + 1) )
               vflags |= RFX_ROLLOVER;

            trig = OreRainTotTrig;
            vflags |= (status & ORE_LOBAT) ? SEC_LOBAT : 0;

            create_flagslist (vtype, vflags, flagslist);

            sprintf(outbuf, "func %12s : hu %c%-2d %sTotRain "FMT_ORERTOT"%s %s%s%s%s(%s)",  
               funclabel[func], hc, unit, chstr, ((double)train / 1000.0) * configp->ore_raintotscale, unitstring,
               rawstring, flipstr, batstr, flagslist, aliasp[index].label);

         }
         else {
            longvdata2 = longvdata = 0;
            break;
         }

         x10global.longvdata = longvdata;
         x10global.longvdata2 = longvdata2;

         *prevvalp = longvdata;
         *(prevvalp + 1) = longvdata2;

         if ( unchanged == 0 ) {
//            x10state[hcode].state[ChgState] |= bitmap;
            changestate |= bitmap;
            *lastchvalp = longvdata;
            *(lastchvalp + 1) = longvdata2;
         }
         else {
//            x10state[hcode].state[ChgState] &= ~bitmap;
            changestate &= ~bitmap;
         }

         break;

#if 0
      case OreRain1 :
      case OreRain2 :
      case OreRain3 :
         loc = aliasp[index].storage_index;

         /* Rain data storage. Note that here we are using one extra storage */
         /* location each for the 32-bit rate and total rain. I.e.,          */
         /*  [loc]     => Rate status, channel, and battery                  */
         /*  [loc + 1] => Rate 32-bit value                                  */
         /*  [loc + 2] => Total status, channel, and battery                 */
         /*  [loc + 3] => Total 32-bit value                                 */
         /*  [loc + 4] => Last changed Rate status, channel, and battery     */
         /*  [loc + 5] => Last changed Rate 32-bit value                     */
         /*  [loc + 6] => Last changed total status, channel, and battery    */
         /*  [loc + 7] => Last changed Total 32-bit value                    */

         extra = 0;
         *flipstr = '\0';

         if ( seq == 1 ) {
            /* Rainfall Rate */
            loc = aliasp[index].storage_index;
            status = 0;
//            /* Save interval since last transmission */
//            intvstrfunc(intvstr, time(NULL), x10state[hcode].timestamp[ucode]);

            longvdata = (unsigned long)(channel & 0x0fu) << ORE_CHANSHFT;

            status = ORE_VALID | batstatus;
            longvdata |= status | ((unsigned long)blevel << ORE_BATSHFT);

            prevvalp = &x10global.data_storage[loc + 2 * (seq-1)];
            lastchvalp = &x10global.data_storage[loc + 2 * (nvar + seq-1)];


            /* Process rainfall rate */
            drain = 0;
            multiplier = 1;
            if ( subtype == OreRain1 ) {
               drain = ((vdatap[5] & 0xf0) >> 4) * 100 + (vdatap[5] & 0x0f) * 10 +
                       ((vdatap[4] & 0xf0) >> 4) ;
               multiplier = 1000;
               tipfactor = 1;
               unitstring = (*configp->ore_rainrateunits) ? configp->ore_rainrateunits : "mm/hr";
//               sprintf(flipstr, "flipcnt %d ", (vdatap[6] & 0x0f));
            }
            else if ( subtype == OreRain2 ) {
               drain = ((vdatap[5] & 0xf0) >> 4) * 1000 + (vdatap[5] & 0x0f) * 100 +
                       ((vdatap[4] & 0xf0) >> 4) * 10 +
                       (vdatap[6] & 0x0f);
               tipfactor = 10;
               unitstring = (*configp->ore_rainrateunits) ? configp->ore_rainrateunits : "inches/hr";
            }
            else  {
               drain = ((vdatap[5] & 0xf0) >> 4) * 1000 + (vdatap[5] & 0x0f) * 100 +
                       ((vdatap[4] & 0xf0) >> 4) * 10 + (vdatap[4] & 0x0f);
               tipfactor = 10;
               unitstring = (*configp->ore_rainrateunits) ? configp->ore_rainrateunits : "inches/hr";
            }

            if ( configp->ore_display_count == YES )
               sprintf(rawstring, "[%lu] ", drain);

            drain *= multiplier;

            longvdata2 = drain;

            lastdrain = *(lastchvalp + 1);

            delta = abs(drain - lastdrain) / multiplier;
            *sunchanged = ((delta/tipfactor) < configp->ore_chgbits_rrate) ? 1 : 0;

            func = OreRainRateFunc;
            trig = OreRainRateTrig;
            vflags |= (status & ORE_LOBAT) ? SEC_LOBAT : 0;

            create_flagslist (vtype, vflags, flagslist);

            sprintf(outbuf, "func %12s : hu %c%-2d %sRate "FMT_ORERRATE"%s %s%s%s%s(%s)",  
               funclabel[func], hc, unit, chstr, ((double)drain / 1000.0) * configp->ore_rainratescale, unitstring,
               rawstring, flipstr, batstr, flagslist, aliasp[index].label);

         }
         else if ( seq == 2 ) {
            /* Process Total Rain data */
            longvdata = (unsigned long)(channel & 0x0fu) << ORE_CHANSHFT;

            status = ORE_VALID | batstatus;

            longvdata |= status | ((unsigned long)blevel << ORE_BATSHFT);

            prevvalp = &x10global.data_storage[loc + 2 * (seq-1)];
            lastchvalp = &x10global.data_storage[loc + 2 * (nvar + seq-1)];

            if ( subtype == OreRain1 ) {
               train = (vdatap[8] & 0x0f) * 1000 +
                       ((vdatap[7] & 0xf0) >> 4) * 100 + (vdatap[7] & 0x0f) * 10 +
                       ((vdatap[6] & 0xf0) >> 4);
               unitstring = (*configp->ore_raintotunits) ? configp->ore_raintotunits : "mm";
               minroll = 25u;
               multiplier = 1000;
               tipfactor = 1;
//               sprintf(flipstr, "flipcnt %d ", (vdatap[6] & 0x0f));
            }
            else  {
               train = (vdatap[9] & 0x0f) * 100000 +
                       ((vdatap[8] & 0xf0) >> 4) * 10000 + (vdatap[8] & 0x0f) * 1000 +
                       ((vdatap[7] & 0xf0) >> 4) * 100 + (vdatap[7] & 0x0f) * 10 +
                       ((vdatap[6] & 0xf0) >> 4);
               multiplier = 1;
               tipfactor = 39;
               unitstring = (*configp->ore_raintotunits) ? configp->ore_raintotunits : "inches";
               minroll = 1000u;
            }

            if ( configp->ore_display_count == YES )
               sprintf(rawstring, "[%lu] ", train);

            train *= multiplier;

            longvdata2 = train;

            *sunchanged = 0;

            /* Determine change and/or rollover here */

            lastdrain = *(lastchvalp + 1);
            delta = abs(train - lastdrain) / multiplier;
            *sunchanged = ((delta/tipfactor) < configp->ore_chgbits_rtot) ? 1 : 0;

            if ( *(prevvalp + 1) > minroll && train < *(prevvalp + 1) )
               vflags |= RFX_ROLLOVER;

            func = OreRainTotFunc;
            trig = OreRainTotTrig;
            vflags |= (status & ORE_LOBAT) ? SEC_LOBAT : 0;

            create_flagslist (vtype, vflags, flagslist);

            sprintf(outbuf, "func %12s : hu %c%-2d %sTotRain "FMT_ORERTOT"%s %s%s%s%s(%s)",  
               funclabel[func], hc, unit, chstr, ((double)train / 1000.0) * configp->ore_raintotscale, unitstring,
               rawstring, flipstr, batstr, flagslist, aliasp[index].label);

         }
         else {
            longvdata2 = longvdata = 0;
            break;
         }

         x10global.longvdata = longvdata;
         x10global.longvdata2 = longvdata2;

         *prevvalp = longvdata;
         *(prevvalp + 1) = longvdata2;

         if ( *sunchanged == 0 ) {
            x10state[hcode].state[ChgState] |= bitmap;
            *lastchvalp = longvdata;
            *(lastchvalp + 1) = longvdata2;
         }
         else {
            x10state[hcode].state[ChgState] &= ~bitmap;
         }

         break;
#endif

      case OreUV1 :
      case OreUV2 :
         /* Process UV sensor data */
         vflags_mask = SEC_LOBAT;
         loc = aliasp[index].storage_index;
         status = 0;

         longvdata = (unsigned long)(channel & 0x0fu) << ORE_CHANSHFT;

         status = ORE_VALID | batstatus;
         longvdata |= status | ((unsigned long)blevel << ORE_BATSHFT);

         prevvalp = &x10global.data_storage[loc];
         lastchvalp = &x10global.data_storage[loc + nvar];

         batchange = (batstatus ^ (unsigned char)(*prevvalp)) & ORE_LOBAT;

         if ( subtype == OreUV1 ) {
            uvfactor = (vdatap[5] & 0x0f) * 10 + (vdatap[4] >> 4);
         }
         else {
            uvfactor = (vdatap[4] >> 4);
         }

         longvdata |= uvfactor << ORE_DATASHFT;

         lastuvfactor = (*lastchvalp & ORE_DATAMSK) >> ORE_DATASHFT;

         delta = abs(uvfactor - lastuvfactor);

         unchanged = (delta < configp->ore_chgbits_uv) ? 1 : 0;

         func = OreUVFunc;
         trig = OreUVTrig;
         vflags |= (status & ORE_LOBAT) ? SEC_LOBAT : 0;

         create_flagslist (vtype, vflags, flagslist);

         sprintf(outbuf, "func %12s : hu %c%-2d %sUV Factor %d%s %s%s(%s)",  
            funclabel[func], hc, unit, chstr, uvfactor, rawstring, batstr,
            flagslist, aliasp[index].label);

         x10global.longvdata = longvdata;

         x10global.data_storage[loc] = longvdata;
         x10state[hcode].longdata = longvdata;

         if ( unchanged == 0 ) {
//            x10state[hcode].state[ChgState] |= bitmap;
            changestate |= bitmap;
            x10global.data_storage[loc + 1] = longvdata;
         }
         else {
//            x10state[hcode].state[ChgState] &= ~bitmap;
            changestate &= ~bitmap;
         }

         break;       
         

      case OreTH1 :
      case OreTH2 :
      case OreTH3 :
      case OreTH4 :
      case OreTH5 :
      case OreTH6 :
      case OreTemp1 :
      case OreTemp2 :
      case OreTemp3 :
      case OreTHB1 :
      case OreTHB2 :
         loc = aliasp[index].storage_index;
         if ( seq == 1 ) {

            /* Process temperature and battery data */
            vflags_mask = SEC_LOBAT | ORE_TMIN | ORE_TMAX;
            for ( j = 0; j < nvar; j++ ) {
               prevdata[j] = x10global.data_storage[loc + j];
               lastchdata[j] = x10global.data_storage[loc + nvar + j];
            }
            longvdata = (unsigned long)(channel & 0x0fu) << ORE_CHANSHFT;

            /* Temperature in units of 0.1 C */
            dtempc = 100 * ((vdatap[5] & 0xf0u) >> 4) +
                            10 * (vdatap[5] & 0x0fu) +
                                ((vdatap[4] & 0xf0u) >> 4);
            if ( subtype == OreTemp2 )
               dtempc += 1000 * (vdatap[6] & 0x01);

            longvdata |= (unsigned long)dtempc << ORE_DATASHFT;
            status = ORE_VALID;

            if ( vdatap[6] & 0x08u ) {
               dtempc = -dtempc;
               status |= ORE_NEGTEMP;
            }

            *batstr = '\0';
            blevel = 0;

            switch ( orechk[subindx].batmode ) {
               case 2 :
                  blevel = 10 - (vdatap[4] & 0x0fu);
                  status |= ORE_BATLVL;
                  if ( (10 * blevel) <= configp->ore_lobat ) {
                     status |= ORE_LOBAT;
                  }
                  if ( configp->ore_display_batlvl == YES )
                     sprintf(batstr, "BatLvl %d%% ", blevel * 10);

                  break;
               case 1 :
                  if ( vdatap[4] & 0x04u ) {
                     status |= ORE_LOBAT;
                  }
                  break;
               default :
                  break;
            }
                     
            longvdata |= status | (blevel << ORE_BATSHFT);

            if ( seq == 1 )
               batchange = (batstatus ^ (unsigned char)prevdata[0]) & ORE_LOBAT;

            lastdtempc = (lastchdata[0] & ORE_DATAMSK) >> ORE_DATASHFT;
            if ( lastchdata[0] & ORE_NEGTEMP ) 
               lastdtempc = -lastdtempc;
            delta = abs(dtempc - lastdtempc);
            unchanged = (delta < (int)configp->ore_chgbits_t) ? 1 : 0;

            func = OreTempFunc;
            trig = OreTempTrig;
            vflags |= (status & ORE_LOBAT) ? SEC_LOBAT : 0;

            if ( (aliasp[index].optflags2 & MOPT2_TMIN) && (dtempc < aliasp[index].tmin) )
               vflags |= ORE_TMIN;
            if ( (aliasp[index].optflags2 & MOPT2_TMAX) && (dtempc > aliasp[index].tmax) )
               vflags |= ORE_TMAX;

            create_flagslist (vtype, vflags, flagslist);

            temp = celsius2temp((double)dtempc / 10.0, configp->ore_tscale, configp->ore_toffset);
            sprintf(outbuf, "func %12s : hu %c%-2d %sTemp "FMT_ORET"%c %s%s(%s)",  
               funclabel[func], hc, unit, chstr, temp, configp->ore_tscale,
               batstr, flagslist, aliasp[index].label);

         }
         else if ( seq == 2 ) {
            /* Process Humidity data */
            vflags_mask = SEC_LOBAT | ORE_RHMIN | ORE_RHMAX;
            longvdata = (unsigned long)(channel & 0x0fu) << ORE_CHANSHFT;
            /* RH in units of 1% */
            humid = 10 * (vdatap[7] & 0x0fu) + ((vdatap[6] & 0xf0u) >> 4);
            longvdata |= (unsigned long)humid << ORE_DATASHFT;
            status |= ORE_VALID;

            longvdata |= status | (blevel << ORE_BATSHFT);

            lasthumid = (lastchdata[seq - 1] & ORE_DATAMSK) >> ORE_DATASHFT;             
            delta = abs(humid - lasthumid);
            unchanged = (delta < (int)configp->ore_chgbits_rh) ? 1 : 0;

            func = OreHumidFunc;
            trig = OreHumidTrig;
            vflags |= (status & ORE_LOBAT) ? SEC_LOBAT : 0;

            if ( (aliasp[index].optflags2 & MOPT2_RHMIN) && (humid < aliasp[index].rhmin) )
               vflags |= ORE_RHMIN;
            if ( (aliasp[index].optflags2 & MOPT2_RHMAX) && (humid > aliasp[index].rhmax) )
               vflags |= ORE_RHMAX;

            create_flagslist (vtype, vflags, flagslist);

            sprintf(outbuf, "func %12s : hu %c%-2d %sRH %d%% %s%s(%s)",
               funclabel[func], hc, unit, chstr, humid,
               batstr, flagslist, aliasp[index].label);

         }
         else if ( seq == 3 ) {
            /* Process Barometric Pressure data */
            vflags_mask = SEC_LOBAT | ORE_BPMIN | ORE_BPMAX;
            longvdata = (unsigned long)(channel & 0x0fu) << ORE_CHANSHFT;
            /* BP in units of 1 hPa */
            baro = (int)vdatap[8] + orechk[subindx].bpoffset;
            longvdata |= (unsigned long)baro << ORE_DATASHFT;
            status |= ORE_VALID;

            longvdata |= status | (blevel << ORE_BATSHFT);

            fcast = 0;            
            if ( subtype == OreTHB1 ) {               
               fcast = forecast(vdatap[9] & 0x0F);
               longvdata |= (fcast << ORE_FCASTSHFT) & ORE_FCAST;
            }
            else if ( subtype == OreTHB2 ) {
               fcast = forecast(vdatap[9] >> 4);
               longvdata |= (fcast << ORE_FCASTSHFT) & ORE_FCAST;
            }
            fcast_txt = (configp->ore_display_fcast == YES) ? forecast_txt((int)fcast) : "";
            
            lastbaro = (lastchdata[seq - 1] & ORE_DATAMSK) >> ORE_DATASHFT;
            delta = abs(baro - lastbaro);
            unchanged = (delta < (int)configp->ore_chgbits_bp) ? 1 : 0;

            func = OreBaroFunc;
            trig = OreBaroTrig;
            vflags |= (status & ORE_LOBAT) ? SEC_LOBAT : 0;

            if ( (aliasp[index].optflags2 & MOPT2_BPMIN) && (baro < aliasp[index].bpmin) )
               vflags |= ORE_BPMIN;
            if ( (aliasp[index].optflags2 & MOPT2_BPMAX) && (baro > aliasp[index].bpmax) )
               vflags |= ORE_BPMAX;

            create_flagslist (vtype, vflags, flagslist);

            dbaro = (double)baro * configp->ore_bpscale + configp->ore_bpoffset;
            sprintf(outbuf, "func %12s : hu %c%-2d %sBP "FMT_OREBP"%s %s%s%s(%s)",
               funclabel[func], hc, unit, chstr, dbaro, configp->ore_bpunits,
               fcast_txt, batstr, flagslist, aliasp[index].label);
            
         }
         else {
            longvdata = 0;
            break;
         }

         x10global.longvdata = longvdata;
         x10global.longvdata2 = longvdata2;

         x10global.data_storage[loc + seq - 1] = longvdata;
         x10state[hcode].longdata = longvdata;

         if ( unchanged == 0 ) {
//            x10state[hcode].state[ChgState] |= bitmap;
            changestate |= bitmap;
            x10global.data_storage[loc + nvar + seq - 1] = longvdata;
         }
         else {
//            x10state[hcode].state[ChgState] &= ~bitmap;
            changestate &= ~bitmap;
         }

         break;

      case OreWeight1 :         

         /* Process weight data */
         vflags_mask = SEC_LOBAT;
         loc = aliasp[index].storage_index;
         for ( j = 0; j < nvar; j++ ) {
            prevdata[j] = x10global.data_storage[loc + j];
            lastchdata[j] = x10global.data_storage[loc + nvar + j];
         }

         longvdata = (unsigned long)(channel & 0x0fu) << ORE_CHANSHFT;

         /* Weight in units of 0.1 kg */
         cweight = 1000 * (vdatap[5] & 0x01u) + 
                    100 * ((vdatap[4] & 0xf0u) >> 4) +
                     10 * (vdatap[4] & 0x0fu) + 
                          ((vdatap[3] & 0xf0u) >> 4);

         /* These two bits cycle through 0-3 for each new weighing */
         wgtnum = (vdatap[2] & 0x30u) >> 3;

         /* Unknown bits in the transmission */
         uknbytes = ((vdatap[2] & ~0x30u) << 8) | (vdatap[3] & 0x0fu);

         longvdata |= (unsigned long)cweight << ORE_DATASHFT;
         status = wgtnum | ORE_VALID;

         *batstr = '\0';
         blevel = 0;

         if ( seq == 1 )
            batchange = (batstatus ^ (unsigned char)prevdata[0]) & ORE_LOBAT;

         longvdata |= status | (blevel << ORE_BATSHFT);

         lastcweight = (lastchdata[0] & ORE_DATAMSK) >> ORE_DATASHFT;
         delta = abs(cweight - lastcweight);
         if ( (delta >= (int)configp->ore_chgbits_wgt) ||
              (wgtnum != (lastchdata[0] & ORE_WGTNUM))    ||
              !(lastchdata[0] & ORE_VALID)  ) {
            unchanged = 0;
         }
         else {
            unchanged = 1;
         }

         func = OreWeightFunc;
         trig = OreWeightTrig;
         vflags |= (status & ORE_LOBAT) ? SEC_LOBAT : 0;

         create_flagslist (vtype, vflags, flagslist);

         weight = (double)cweight / 10.0 * configp->ore_wgtscale;
         sprintf(outbuf, "func %12s : hu %c%-2d %sWgt "FMT_OREWGT"%s %s%s(%s) ub = 0x%04lx",
            funclabel[func], hc, unit, chstr, weight, configp->ore_wgtunits,
            batstr, flagslist, aliasp[index].label, uknbytes);

         x10global.longvdata = longvdata;

         x10global.data_storage[loc + seq - 1] = longvdata;
         x10state[hcode].longdata = longvdata;

         if ( unchanged == 0 ) {
//            x10state[hcode].state[ChgState] |= bitmap;
            changestate |= bitmap;
            x10global.data_storage[loc + nvar + seq - 1] = longvdata;
         }
         else {
//            x10state[hcode].state[ChgState] &= ~bitmap;
            changestate &= ~bitmap;
         }

         break;

      case OreElec1 :  /* Electrisave */
         vflags_mask = SEC_LOBAT;
         loc = aliasp[index].storage_index; 

         /* Process current data */
         for ( j = 0; j < nvar; j++ ) {
            prevdata[j] = x10global.data_storage[loc + j];
            lastchdata[j] = x10global.data_storage[loc + nvar + j];
         }
         longvdata = (unsigned long)(channel & 0x0fu) << ORE_CHANSHFT;
         status = ORE_VALID;

         counter = vdatap[1] & 0x0f;
         uknbytes = vdatap[6] & 0xc0;

         /* Current in deciamps */
         ct1 = vdatap[3] + ((vdatap[4] & 0x03) << 8);                  /* Left socket */
         ct2 = ((vdatap[4] >> 2) & 0x3f) + ((vdatap[5] & 0x0f) << 6);  /* Middle socket */
         ct3 = ((vdatap[5] >> 4) & 0x0f) + ((vdatap[6] & 0x3f) << 4);  /* Right socket */
         deciamps = ct1 + ct2 + ct3;
 
         longvdata |= (unsigned long)deciamps << ORE_DATASHFT;

         /* Battery status */
         status |= (vdatap[1] & 0x10) ? ORE_LOBAT : 0;

         if ( seq == 1 )
            batchange = (batstatus ^ (unsigned char)prevdata[0]) & ORE_LOBAT;

         longvdata |= status;

         lastdeciamps = (lastchdata[0] & ORE_DATAMSK) >> ORE_DATASHFT;
         delta = abs(deciamps - lastdeciamps);
         unchanged = (delta < configp->els_chgbits_curr) ? 1 : 0;

         if ( configp->els_voltage > 0.0 ) {
            sprintf(pwrstr, "[Pwr %.0fVA] ", configp->els_voltage * (double)deciamps / 10.0);
         }
         else {
            *pwrstr = '\0';
         }

         func = ElsCurrFunc;
         trig = ElsCurrTrig;
         vflags |= (status & ORE_LOBAT) ? SEC_LOBAT : 0;

         create_flagslist (vtype, vflags, flagslist);

         sprintf(outbuf, "func %12s : hu %c%-2d %sCurr %.1fA %s%s(%s)",
            funclabel[func], hc, unit, chstr, (double)deciamps/10.0,
            pwrstr, flagslist, aliasp[index].label); 

         x10global.longvdata = longvdata;
         x10global.sigcount = counter;

         x10global.data_storage[loc + seq - 1] = longvdata;
         x10state[hcode].longdata = longvdata;

         if ( unchanged == 0 ) {
//            x10state[hcode].state[ChgState] |= bitmap;
            changestate |= bitmap;
            x10global.data_storage[loc + nvar + seq - 1] = longvdata;
         }
         else {
//            x10state[hcode].state[ChgState] &= ~bitmap;
            changestate &= ~bitmap;
         }

         break;       
         
      case OreElec2 :  /* OWL CM119 */
         loc = aliasp[index].storage_index;
         longvdata = longvdata2 = longvdata3 = 0;

         /* OWL data storage. Note that here we are using one extra storage */
         /* location for the 32-bit power and two extra storage locations   */
         /* for the 64-bit total energy.                                    */
         /*  [loc]     => Power status, channel, and battery                */
         /*  [loc + 1] => Power 32-bit value                                */
         /*  [loc + 2] => Power last changed status, channel, and battery   */
         /*  [loc + 2] => Power last changed 32-bit value                   */

         /*  [loc + 4] => Energy status, channel, and battery               */
         /*  [loc + 5] => Energy (Low) 32-bit value                         */
         /*  [loc + 6] => Energy (High) 32-bit value                        */
         /*  [loc + 7] => Energy last changed status, channel, and battery  */
         /*  [loc + 8] => Energy (Low) last changed 32-bit value            */
         /*  [loc + 9] => Energy (High) last changed 32-bit value           */

         counter = vdatap[1] & 0x0f;
         /* sprintf(cntrstr, "Cntr 0x%x ", counter); */
         *cntrstr = '\0';

         uknbytes = (vdatap[11] << 16) | (vdatap[12] << 8) | vdatap[13];

         func = aliasp[index].funclist[seq - 1];
         statusoffset = aliasp[index].statusoffset[seq - 1];

         if ( func == OwlPowerFunc ) {
            /* Power in kW */
            vflags_mask = SEC_LOBAT;
            loc = aliasp[index].storage_index;
            status = 0;

            longvdata = (unsigned long)(channel & 0x0fu) << ORE_CHANSHFT;

            status = ORE_VALID | batstatus;
            longvdata |= status | ((unsigned long)blevel << ORE_BATSHFT);

            prevvalp = &x10global.data_storage[loc + statusoffset];
            lastchvalp = &x10global.data_storage[loc + statusoffset + 2];

            if ( seq == 1 )
               batchange = (batstatus ^ (unsigned char)(*prevvalp)) & ORE_LOBAT;

            /* Process power */

            power = ((vdatap[5] & 0x0f) << 16) | (vdatap[4] << 8) | vdatap[3];

            if ( configp->owl_display_count == YES )
               sprintf(rawstring, "[%lu] ", power);

            lastchpower = *(lastchvalp + 1);

            chgbits = (unsigned long)((double)configp->owl_chgbits_power /
                (OWLPSC * configp->owl_calib_power * (configp->owl_voltage / OWLVREF)));
            chgbits = max(chgbits, 1UL);

            delta = power - lastchpower;
            unchanged = ( abs(delta) >= chgbits ) ? 0 : 1;

            longvdata2 = power;

            trig = OwlPowerTrig;
            vflags |= (status & ORE_LOBAT) ? SEC_LOBAT : 0;

            create_flagslist (vtype, vflags, flagslist);

            sprintf(outbuf, "func %12s : hu %c%-2d %sPower %.3f%s %s%s%s%s(%s)",  
               funclabel[func], hc, unit, chstr,
               ((double)power / 1000.0 * OWLPSC * configp->owl_calib_power * (configp->owl_voltage / OWLVREF)), "kW",
               rawstring, cntrstr, batstr, flagslist, aliasp[index].label);

            x10global.longvdata = longvdata;
            x10global.longvdata2 = longvdata2;
            x10global.sigcount = counter;            

            *prevvalp = longvdata;
            *(prevvalp + 1) = longvdata2;

            if ( unchanged == 0 ) {
//               x10state[hcode].state[ChgState] |= bitmap;
               changestate |= bitmap;
               *lastchvalp = longvdata;
               *(lastchvalp + 1) = longvdata2;
            }
            else {
//               x10state[hcode].state[ChgState] &= ~bitmap;
               changestate &= ~bitmap;
            }
         }
         else if ( func == OwlEnergyFunc  ) {
            /* Process Total Energy */
            vflags_mask = SEC_LOBAT;
            loc = aliasp[index].storage_index;
            longvdata = (unsigned long)(channel & 0x0fu) << ORE_CHANSHFT;

            status = ORE_VALID | batstatus;

            longvdata |= status | ((unsigned long)blevel << ORE_BATSHFT);

            prevvalp = &x10global.data_storage[loc + statusoffset];
            lastchvalp = &x10global.data_storage[loc + statusoffset + 3];

            if ( seq == 1 )
               batchange = (batstatus ^ (unsigned char)(*prevvalp)) & ORE_LOBAT;

            /* Lower 32 bits */
            energylo = 
               ((unsigned long)(vdatap[9] & 0x0f)  << 28) |
               ((unsigned long)(vdatap[8])  << 20) |
               ((unsigned long)(vdatap[7])  << 12) |
               ((unsigned long)(vdatap[6])  <<  4) |
               ((unsigned long)(vdatap[5])  >>  4)   ;

            /* Upper 12 bits */
            energyhi =
               ((unsigned long)(vdatap[10]) <<  4) |
               ((unsigned long)(vdatap[9])  >>  4)   ;

            longvdata2 = energylo;
            longvdata3 = energyhi;

            energyll = ((unsigned long long)energyhi << 32) | (unsigned long long)energylo;

            if ( configp->owl_display_count == YES )
               sprintf(rawstring, "[%llu] ", energyll);

            lastchll = (unsigned long long)(*(lastchvalp + 2)) << 32 | (unsigned long long)(*(lastchvalp + 1));

            chgbits = (unsigned long)((double)configp->owl_chgbits_energy /
                (OWLESC * configp->owl_calib_energy * (configp->owl_voltage / OWLVREF)));
            chgbits = max(chgbits, 1UL);

            deltall = abs((long long)energyll - (long long)lastchll);
            unchanged = ( deltall >= (unsigned long long)chgbits ) ? 0 : 1;

            trig = OwlEnergyTrig;
            vflags |= (status & ORE_LOBAT) ? SEC_LOBAT : 0;

            create_flagslist (vtype, vflags, flagslist);

            sprintf(outbuf, "func %12s : hu %c%-2d %sEnergy %.4f%s %s%s%s%s(%s)",  
               funclabel[func], hc, unit, chstr,
               ((double)energyll / 10000.0 * OWLESC * configp->owl_calib_energy * (configp->owl_voltage / OWLVREF)), "kWh",
               rawstring, cntrstr, batstr, flagslist, aliasp[index].label);

            x10global.longvdata = longvdata;
            x10global.longvdata2 = longvdata2;
            x10global.longvdata3 = longvdata3;
            x10global.sigcount = counter;           

            *prevvalp = longvdata;
            *(prevvalp + 1) = longvdata2;
            *(prevvalp + 2) = longvdata3;

            if ( unchanged == 0 ) {
//               x10state[hcode].state[ChgState] |= bitmap;
               changestate |= bitmap;
               *lastchvalp = longvdata;
               *(lastchvalp + 1) = longvdata2;
               *(lastchvalp + 2) = longvdata3;
            }
            else {
//               x10state[hcode].state[ChgState] &= ~bitmap;
               changestate &= ~bitmap;
            }
         }

         break;                      

      default :
         break;
   }

#if 0
      case OreElec2 :  /* OWL CM119 */
         loc = aliasp[index].storage_index;
         longvdata = longvdata2 = longvdata3 = 0;

         /* OWL data storage. Note that here we are using one extra storage */
         /* location for the 32-bit power and two extra storage locations   */
         /* for the 64-bit total energy.                                    */
         /*  [loc]     => Power status, channel, and battery                */
         /*  [loc + 1] => Power 32-bit value                                */
         /*  [loc + 2] => Energy status, channel, and battery               */
         /*  [loc + 3] => Energy (Low) 32-bit value                         */
         /*  [loc + 4] => Energy (High) 32-bit value                        */
         /*  [loc + 5] => Power last changed status, channel, and battery   */
         /*  [loc + 6] => Power last changed 32-bit value                   */
         /*  [loc + 7] => Energy last changed status, channel, and battery  */
         /*  [loc + 8] => Energy (Low) last changed 32-bit value            */
         /*  [loc + 9] => Energy (High) last changed 32-bit value           */

         counter = vdatap[1] & 0x0f;
         sprintf(cntrstr, "Cntr %x ", counter);

         uknbytes = (vdatap[11] << 16) | (vdatap[12] << 8) | vdatap[13];


         if ( seq == 1 ) {
            /* Power in kW */
            loc = aliasp[index].storage_index;
            status = 0;
//            /* Save interval since last transmission */
//            intvstrfunc(intvstr, time(NULL), x10state[hcode].timestamp[ucode]);

            longvdata = (unsigned long)(channel & 0x0fu) << ORE_CHANSHFT;

            status = ORE_VALID | batstatus;
            longvdata |= status | ((unsigned long)blevel << ORE_BATSHFT);

            prevvalp = &x10global.data_storage[loc];
            lastchvalp = &x10global.data_storage[loc + 5];

            /* Process power */

            power = ((vdatap[5] & 0x0f) << 16) | (vdatap[4] << 8) | vdatap[3];

            if ( configp->ore_display_count == YES )
               sprintf(rawstring, "[%lu] ", power);

            lastchpower = *(lastchvalp + 1);

            chgbits = (unsigned long)((double)configp->owl_chgbits_power /
                (OWLPSC * configp->owl_calib_power * (configp->owl_voltage / OWLVREF)));
            chgbits = max(chgbits, 1UL);

            delta = power - lastchpower;
            *sunchanged = ( abs(delta) >= chgbits ) ? 0 : 1;

            longvdata2 = power;

            func = OwlPowerFunc;
            trig = OwlPowerTrig;
            vflags |= (status & ORE_LOBAT) ? SEC_LOBAT : 0;

            create_flagslist (vtype, vflags, flagslist);

            sprintf(outbuf, "func %12s : hu %c%-2d %sPower %.3f%s %s%s%s%s(%s)",  
               funclabel[func], hc, unit, chstr,
               ((double)power / 1000.0 * OWLPSC * configp->owl_calib_power * (configp->owl_voltage / OWLVREF)), "kW",
               rawstring, cntrstr, batstr, flagslist, aliasp[index].label);

            x10global.longvdata = longvdata;
            x10global.longvdata2 = longvdata2;            

            *prevvalp = longvdata;
            *(prevvalp + 1) = longvdata2;

            if ( *sunchanged == 0 ) {
               x10state[hcode].state[ChgState] |= bitmap;
               *lastchvalp = longvdata;
               *(lastchvalp + 1) = longvdata2;
            }
            else {
               x10state[hcode].state[ChgState] &= ~bitmap;
            }
         }
         else if ( seq == 2 ) {
            /* Process Total Energy */
            loc = aliasp[index].storage_index;
            longvdata = (unsigned long)(channel & 0x0fu) << ORE_CHANSHFT;

            status = ORE_VALID | batstatus;

            longvdata |= status | ((unsigned long)blevel << ORE_BATSHFT);

            prevvalp = &x10global.data_storage[loc + 2];
            lastchvalp = &x10global.data_storage[loc + 7];

            /* Lower 32 bits */
            energylo = 
               ((unsigned long)(vdatap[9] & 0x0f)  << 28) |
               ((unsigned long)(vdatap[8])  << 20) |
               ((unsigned long)(vdatap[7])  << 12) |
               ((unsigned long)(vdatap[6])  <<  4) |
               ((unsigned long)(vdatap[5])  >>  4)   ;

            /* Upper 12 bits */
            energyhi =
               ((unsigned long)(vdatap[10]) <<  4) |
               ((unsigned long)(vdatap[9])  >>  4)   ;

            longvdata2 = energylo;
            longvdata3 = energyhi;

            energyll = ((unsigned long long)energyhi << 32) | (unsigned long long)energylo;

            if ( configp->ore_display_count == YES )
               sprintf(rawstring, "[%llu] ", energyll);

            lastchll = (unsigned long long)(*(lastchvalp + 2)) << 32 | (unsigned long long)(*(lastchvalp + 1));

            chgbits = (unsigned long)((double)configp->owl_chgbits_energy /
                (OWLESC * configp->owl_calib_energy * (configp->owl_voltage / OWLVREF)));
            chgbits = max(chgbits, 1UL);

            deltall = abs((long long)energyll - (long long)lastchll);
            *sunchanged = ( deltall >= (unsigned long long)chgbits ) ? 0 : 1;

            func = OwlEnergyFunc;
            trig = OwlEnergyTrig;
            vflags |= (status & ORE_LOBAT) ? SEC_LOBAT : 0;

            create_flagslist (vtype, vflags, flagslist);

            sprintf(outbuf, "func %12s : hu %c%-2d %sEnergy %.4f%s %s%s%s%s(%s)",  
               funclabel[func], hc, unit, chstr,
               ((double)energyll / 10000.0 * OWLESC * configp->owl_calib_energy * (configp->owl_voltage / OWLVREF)), "kWh",
               rawstring, cntrstr, batstr, flagslist, aliasp[index].label);

            x10global.longvdata = longvdata;
            x10global.longvdata2 = longvdata2;
            x10global.longvdata3 = longvdata3;           

            *prevvalp = longvdata;
            *(prevvalp + 1) = longvdata2;
            *(prevvalp + 2) = longvdata3;

            if ( *sunchanged == 0 ) {
               x10state[hcode].state[ChgState] |= bitmap;
               *lastchvalp = longvdata;
               *(lastchvalp + 1) = longvdata2;
               *(lastchvalp + 2) = longvdata3;
            }
            else {
               x10state[hcode].state[ChgState] &= ~bitmap;
            }
         }

         break;                      

      default :
         break;
   }
#endif


//   if ( *outbuf && configp->show_change == YES )
//      snprintf(outbuf + strlen(outbuf), sizeof(outbuf), " %s", ((unchanged) ? " UnChg":" Chg"));

   if ( *outbuf && configp->display_sensor_intv == YES && (aliasp[index].optflags & MOPT_HEARTBEAT) )
      snprintf(outbuf + strlen(outbuf), sizeof(outbuf), " Intv %s ", intvstr);

   x10state[hcode].vaddress = bitmap;
   x10state[hcode].lastcmd = func;
   x10state[hcode].lastunit = unit;
   x10state[hcode].vident[ucode] = vident;
   x10state[hcode].vflags[ucode] &= ~vflags_mask;
   x10state[hcode].vflags[ucode] |= vflags;
   x10state[hcode].timestamp[ucode] = time(NULL);
//   x10state[hcode].state[ValidState] |= (1 << ucode);
   x10global.lasthc = hcode;
   x10global.lastaddr = 0;

   if ( vflags & SEC_LOBAT ) {
      x10state[hcode].state[LoBatState] |= (1 << ucode);
   }
   else {
      x10state[hcode].state[LoBatState] &= ~(1 << ucode);
   }

   actfunc = 0;
   afuncmap = 0;
   oactfunc = func;
   ofuncmap = (1 << trig);
   trigaddr = bitmap;

   mask = modmask[VdataMask][hcode];
   active = bitmap & mask;

   if ( seq == 1 ) {
      startupstate = ~x10state[hcode].state[ValidState] & active;
      x10state[hcode].state[ValidState] |= active;
      /* Update activity timeout for sensor */
      update_activity_timeout(aliasp, index);
      update_activity_states(hcode, aliasp[index].unitbmap, S_ACTIVE);
   }

//   changestate |= batchange ? active : 0;

//   x10state[hcode].state[ModChgState] = (x10state[hcode].state[ModChgState] & ~active) | changestate;
   x10state[hcode].state[ModChgState] = changestate;

   x10state[hcode].state[ChgState] = x10state[hcode].state[ModChgState] |
      (x10state[hcode].state[ActiveChgState] & active) |
      (startupstate & ~modmask[PhysMask][hcode]);

   changestate = x10state[hcode].state[ChgState];

   if ( aliasp[index].optflags & MOPT_HEARTBEAT ) {
      *sunchanged = (changestate & active) ? 0 : 1;
   }
   else {
      *sunchanged = 0;
   }

   if ( *outbuf && configp->show_change == YES )
      snprintf(outbuf + strlen(outbuf), sizeof(outbuf), " %s", ((changestate & active) ? " Chg":" UnChg"));

   if ( i_am_state )
      write_x10state_file();

   /* Heyuhelper, if applicable */
   if ( i_am_state && signal_source & RCVI && configp->script_mode & HEYU_HELPER ) {
      launch_heyuhelper(hcode, trigaddr, func);
      return outbuf;
   }

   bmaplaunch = launched = 0;
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

      if ( launcherp[j].ofuncmap & ofuncmap &&
           launcherp[j].source & signal_source ) {
         trigactive = trigaddr & (mask | launcherp[j].signal);
         if ( (bmaplaunch = (trigactive & launcherp[j].bmaptrig) &
#if 0 
                   (x10state[hcode].state[ChgState] | ~launcherp[j].chgtrig)) ||
#endif
                   (changestate | ~launcherp[j].chgtrig)) ||
                   (launcherp[j].unitstar && !trigaddr)) {
            launcherp[j].matched = YES;
            *launchp = j;
            launcherp[j].actfunc = oactfunc;
            launcherp[j].genfunc = 0;
	    launcherp[j].xfunc = oactfunc;
	    launcherp[j].level = x10state[hcode].dimlevel[ucode];
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

   return outbuf;
#else
   return "";
#endif /* HASORE */
}   

/*----------------------------------------------------------------------------+
 | Translate Oregon RF data                                                   |
 | Buffer reference: ST_COMMAND, ST_ORE_EMU, plus:                            |
 |  vtype, subindx, hcode, ucode, seq, data[0], data[1], data[2], data[3]     |
 |   [2]     [3]     [4]    [5]   [6]    [7]     [8]      [9]      [10]       |
 +----------------------------------------------------------------------------*/
char *translate_ore_emu( unsigned char *buf, unsigned char *sunchanged, int *launchp )
{
#ifdef HASORE
   static char    outbuf[160];
   static char    intvstr[32];
   long           intv;
   char           flagslist[80];
   ALIAS          *aliasp;
   LAUNCHER       *launcherp;
   extern unsigned int signal_source;
   unsigned char  func, vtype, subindx, subtype, seq, unchanged;
   int            humid, lasthumid;
   int            baro, lastbaro;
   unsigned short vident;
   int            unit, loc;
   int            dtempc, lastdtempc, delta, nvar;
   unsigned long  longvdata;

   char           hc;
   char           chstr[8];

   unsigned char  actfunc, oactfunc;
   unsigned int   trigaddr, mask, active, trigactive;
   unsigned int   bmaplaunch;
   unsigned long  afuncmap, ofuncmap;

   int            j, /*k,*/ found, index = -1;
   unsigned char  hcode, ucode, trig = 0;
   unsigned int   bitmap, changestate, startupstate;
   unsigned long  vflags = 0, vflags_mask = 0;

   unsigned long prevdata, lastchdata;

   double temp, dbaro;

   static char *orecmd[] = {"?", "oretemp", "orerh", "orebp"};

   launcherp = configp->launcherp;
   aliasp = configp->aliasp;

   *launchp = -1;
   *sunchanged = unchanged = 0;
   flagslist[0] = '\0';
   *outbuf = '\0';
   changestate = 0;


   vtype   = buf[2];
   subindx = buf[3];
   hcode   = buf[4];
   ucode   = buf[5];
   seq     = buf[6];
   func    = VdataFunc;

   subtype = orechk[subindx].subtype;

   x10global.longvdata = 0;
   x10global.lastvtype = vtype;

   hc = code2hc(hcode);
   bitmap = 1 << ucode;
   unit = code2unit(ucode);

   /* Reconstruct unsigned long from littleendian bytes */
   longvdata = 0;
   for ( j = 0; j < 4; j++ ) 
      longvdata |= buf[7 + j] << (8 * j);

   /* Whether or not to display channel in output */
   if ( configp->ore_display_chan == YES )
      sprintf(chstr, "Ch %d ", (int)(longvdata & ORE_CHANMSK) >> ORE_CHANSHFT);
   else
      *chstr = '\0';
         
   found = 0;
   j = 0;
   /* Look up the alias, if any */
   while ( !found && aliasp && aliasp[j].line_no > 0 ) {
      if ( aliasp[j].vtype == vtype && aliasp[j].subtype == subtype && 
           bitmap == aliasp[j].unitbmap && hc == aliasp[j].housecode ) {
         index = j;
         found = 1;
         break;
      }
      j++;
   }

   if ( !found || !aliasp ) {
      sprintf(outbuf, "func %12s : hu %c%-2d func %s %sdata %lu - Missing Alias",
         "oreEmu", hc, unit, orecmd[seq], chstr, longvdata);
      return outbuf;
   }

   /* Save interval since last transmission */
   intvstrfunc(intvstr, &intv, time(NULL), x10state[hcode].timestamp[ucode]);
   x10global.interval = intv;

   vident = aliasp[index].ident[0];

   nvar = orechk[subindx].nvar;

   /* Internal check */
   if ( aliasp[index].storage_units != (2 * nvar) ) {
      sprintf(outbuf, "Internal error - storage != 2 * nvar\n");
      return outbuf;
   }
      
   loc = aliasp[index].storage_index;

   prevdata = x10global.data_storage[loc + seq - 1];
   lastchdata = x10global.data_storage[loc + nvar + seq - 1];

   switch ( seq ) {
      case 1 :  /* Temperature */
         vflags_mask = SEC_LOBAT | ORE_TMIN | ORE_TMAX;
         dtempc = (longvdata & ORE_DATAMSK) >> ORE_DATASHFT;
         if ( longvdata & ORE_NEGTEMP )
            dtempc = -dtempc;

         lastdtempc = (lastchdata & ORE_DATAMSK) >> ORE_DATASHFT;
         if ( lastchdata & ORE_NEGTEMP ) 
            lastdtempc = -lastdtempc;
         delta = abs(dtempc - lastdtempc);
         unchanged = (delta < (int)configp->ore_chgbits_t) ? 1 : 0;

         func = OreTempFunc;
         trig = OreTempTrig;
         vflags = 0;

         if ( (aliasp[index].optflags2 & MOPT2_TMIN) && (dtempc < aliasp[index].tmin) )
            vflags |= ORE_TMIN;
         if ( (aliasp[index].optflags2 & MOPT2_TMAX) && (dtempc > aliasp[index].tmax) )
            vflags |= ORE_TMAX;

         create_flagslist (vtype, vflags, flagslist);

         temp = celsius2temp((double)dtempc / 10.0, configp->ore_tscale, configp->ore_toffset);
         sprintf(outbuf, "func %12s : hu %c%-2d %sTemp "FMT_ORET"%c %s(%s)",  
            funclabel[func], hc, unit, chstr, temp, configp->ore_tscale,
            flagslist, aliasp[index].label);
         break;

      case 2 : /* Relative Humidity */
         vflags_mask = SEC_LOBAT | ORE_RHMIN | ORE_RHMAX;
         humid = (longvdata & ORE_DATAMSK) >> ORE_DATASHFT;
         lasthumid = (lastchdata & ORE_DATAMSK) >> ORE_DATASHFT;
         delta = abs(humid - lasthumid);
         unchanged = (delta < (int)configp->ore_chgbits_rh) ? 1 : 0;

         func = OreHumidFunc;
         trig = OreHumidTrig;
         vflags = 0;

         if ( (aliasp[index].optflags2 & MOPT2_RHMIN) && (humid < aliasp[index].rhmin) )
            vflags |= ORE_RHMIN;
         if ( (aliasp[index].optflags2 & MOPT2_RHMAX) && (humid > aliasp[index].rhmax) )
            vflags |= ORE_RHMAX;

         create_flagslist (vtype, vflags, flagslist);

         sprintf(outbuf, "func %12s : hu %c%-2d %sRH %d%% %s(%s)",
            funclabel[func], hc, unit, chstr, humid, flagslist, aliasp[index].label);
         break;

      case 3 : /* Barometric Pressure */
         vflags_mask = SEC_LOBAT | ORE_BPMIN | ORE_BPMAX;
         baro = (int)((longvdata & ORE_DATAMSK) >> ORE_DATASHFT);
         longvdata = (longvdata & ~ORE_DATAMSK) | ((unsigned long)baro << ORE_DATASHFT);
         lastbaro = (lastchdata & ORE_DATAMSK) >> ORE_DATASHFT;
         delta = abs(baro - lastbaro);
         unchanged = (delta < (int)configp->ore_chgbits_bp) ? 1 : 0;

         func = OreBaroFunc;
         trig = OreBaroTrig;
         vflags = 0;

         create_flagslist (vtype, vflags, flagslist);

         dbaro = (double)baro * configp->ore_bpscale + configp->ore_bpoffset;
         sprintf(outbuf, "func %12s : hu %c%-2d %sBP "FMT_OREBP"%s %s(%s)",
            funclabel[func], hc, unit, chstr, dbaro, configp->ore_bpunits,
            flagslist, aliasp[index].label);
         break;

      default :
         break;
   }            


   x10global.longvdata = longvdata;

   x10global.data_storage[loc + seq - 1] = longvdata;
   x10state[hcode].longdata = longvdata;

   if ( unchanged == 0 ) {
//      x10state[hcode].state[ChgState] |= bitmap;
      changestate |= bitmap;
      x10global.data_storage[loc + nvar + seq - 1] = longvdata;
   }
   else {
//      x10state[hcode].state[ChgState] &= ~bitmap;
      changestate &= ~bitmap;
   }

   if ( configp->display_sensor_intv == YES && (aliasp[index].optflags & MOPT_HEARTBEAT) )
      snprintf(outbuf + strlen(outbuf), sizeof(outbuf), " Intv %s ", intvstr);

   x10state[hcode].vaddress = bitmap;
   x10state[hcode].lastcmd = func;
   x10state[hcode].lastunit = unit;
   x10state[hcode].vident[ucode] = vident;
   x10state[hcode].vflags[ucode] &= ~vflags_mask;
   x10state[hcode].vflags[ucode] |= vflags;
   x10state[hcode].timestamp[ucode] = time(NULL);
//   x10state[hcode].state[ValidState] |= (1 << ucode);
   x10global.lasthc = hcode;
   x10global.lastaddr = 0;

   if ( vflags & SEC_LOBAT ) {
      x10state[hcode].state[LoBatState] |= (1 << ucode);
   }
   else {
      x10state[hcode].state[LoBatState] &= ~(1 << ucode);
   }

   actfunc = 0;
   afuncmap = 0;
   oactfunc = func;
   ofuncmap = (1 << trig);
   trigaddr = bitmap;

   mask = modmask[VdataMask][hcode];
   active = bitmap & mask;

   startupstate = ~x10state[hcode].state[ValidState] & active;
   x10state[hcode].state[ValidState] |= active;

   /* Update activity timeout for sensor */
   update_activity_timeout(aliasp, index);
   update_activity_states(hcode, aliasp[index].unitbmap, S_ACTIVE);

//   x10state[hcode].state[ModChgState] = (x10state[hcode].state[ModChgState] & ~active) | changestate;
   x10state[hcode].state[ModChgState] = changestate;

   x10state[hcode].state[ChgState] = x10state[hcode].state[ModChgState] |
      (x10state[hcode].state[ActiveChgState] & active) |
      (startupstate & ~modmask[PhysMask][hcode]);

   changestate = x10state[hcode].state[ChgState];

   if ( aliasp[index].optflags & MOPT_HEARTBEAT ) {
      *sunchanged = (changestate & active) ? 0 : 1;
   }
   else {
      *sunchanged = 0;
   }

   if ( *outbuf && configp->show_change == YES )
      snprintf(outbuf + strlen(outbuf), sizeof(outbuf), " %s", ((changestate & active) ? " Chg":" UnChg"));

   if ( i_am_state )
      write_x10state_file();

   /* Heyuhelper, if applicable */
   if ( i_am_state && signal_source & RCVI && configp->script_mode & HEYU_HELPER ) {
      launch_heyuhelper(hcode, trigaddr, func);
      return outbuf;
   }

   bmaplaunch = 0;
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

      if ( launcherp[j].ofuncmap & ofuncmap &&
           launcherp[j].source & signal_source ) {
         trigactive = trigaddr & (mask | launcherp[j].signal);
         if ( (bmaplaunch = (trigactive & launcherp[j].bmaptrig) &
#if 0 
                   (x10state[hcode].state[ChgState] | ~launcherp[j].chgtrig)) ||
#endif
                   (changestate | ~launcherp[j].chgtrig)) ||
                   (launcherp[j].unitstar && !trigaddr)) {
            launcherp[j].matched = YES;
            *launchp = j;
            launcherp[j].actfunc = oactfunc;
            launcherp[j].genfunc = 0;
	    launcherp[j].xfunc = oactfunc;
	    launcherp[j].level = x10state[hcode].dimlevel[ucode];
            launcherp[j].bmaplaunch = bmaplaunch;
            launcherp[j].actsource = signal_source;
            if ( launcherp[j].scanmode & FM_BREAK )
               break;
         }
      }
      j++;
   }

   x10state[hcode].launched = bmaplaunch;

   return outbuf;
#else
   return "";
#endif /* HASORE */
} 


#ifdef HASORE
/*---------------------------------------------------------------------+
 | Display stored data for Oregon, Electrisave, and OWL sensors        |
 +---------------------------------------------------------------------*/
int show_oregon ( void )
{

   ALIAS          *aliasp;
   char           hc;
   int            j, unit, index, count = 0, maxlabel = 0, maxmod = 0;
   int            loc, nvar, statusoffset;
   unsigned char  hcode, ucode, func;
   unsigned char  subtype, subindx, status = 0;
   unsigned long  longvdata = 0;
   unsigned long  drain;
   short          dtempc, cweight, deciamps;
   int            baro, blevel;
   int            channel;
   int            dspeed, davspeed, direction;
   double         temp, dbaro, weight, amps, dblpower, dblenergy;
   char           *unitstring;

   if ( !(aliasp = configp->aliasp) ) 
      return 0;

   /* Get maximum lengths of module name and alias label */
   index = 0;
   while ( aliasp[index].line_no > 0 ) {
      if ( aliasp[index].vtype == RF_OREGON   ||
           aliasp[index].vtype == RF_ELECSAVE ||
           aliasp[index].vtype == RF_OWL          ) {
         count++;
         maxlabel = max(maxlabel, (int)strlen(aliasp[index].label));
         maxmod = max(maxmod, (int)strlen(lookup_module_name(aliasp[index].modtype)) );
      }
      index++;
   }

   if ( count == 0 )
      return 0;

   index = 0;
   while ( aliasp[index].line_no > 0 ) {
      if ( aliasp[index].vtype != RF_OREGON &&
           aliasp[index].vtype != RF_ELECSAVE &&
           aliasp[index].vtype != RF_OWL          ) {
         index++;
         continue;
      }
      ucode = single_bmap_unit(aliasp[index].unitbmap);
      unit = code2unit(ucode);
      hc = aliasp[index].housecode;
      hcode = hc2code(hc);
      subtype = aliasp[index].subtype;
      loc = aliasp[index].storage_index;
      nvar = aliasp[index].nvar;

      for ( subindx = 0; subindx < orechk_size; subindx++ ) {
         if ( orechk[subindx].subtype == subtype )
            break;
      }

      printf("%c%-2d %-*s %-*s ", hc, unit,
          maxmod, lookup_module_name(aliasp[index].modtype),
          maxlabel, aliasp[index].label);

      channel = 0;
      for ( j = 0; j < nvar; j++ ) {
          longvdata = x10global.data_storage[loc + j];
          if ( longvdata & ORE_VALID ) {
             channel = (int)((longvdata & ORE_CHANMSK) >> ORE_CHANSHFT);
             break;
          }
      }

      if ( channel > 0 )
         printf("Ch %d ", channel);
      else
         printf("Ch - ");

      for ( j = 0; j < nvar; j++ ) {
         func = aliasp[index].funclist[j];
         statusoffset = aliasp[index].statusoffset[j];
         longvdata = x10global.data_storage[loc + j];
         status = longvdata & 0xff;

         switch ( func ) {
            case OreTempFunc :
               if ( !(longvdata & ORE_VALID) ) {
                  printf("Temp ----%c ", configp->ore_tscale);
                  break;
               }

               dtempc = (longvdata & ORE_DATAMSK) >> ORE_DATASHFT;
               if ( status & ORE_NEGTEMP )
                  dtempc = -dtempc;
               temp = celsius2temp((double)dtempc / 10.0, configp->ore_tscale, configp->ore_toffset);
               printf("Temp "FMT_ORET"%c ", temp, configp->ore_tscale);
               break;
            case OreHumidFunc :
               if ( !(longvdata & ORE_VALID) ) {
                  printf("RH --%% ");
                  break;
               }
               printf("RH %2d%% ", (int)(longvdata & ORE_DATAMSK) >> ORE_DATASHFT);
               break;
            case OreBaroFunc :
               if ( !(longvdata & ORE_VALID) ) {
                  printf("BP ---%s ", configp->ore_bpunits);
                  break;
               }
               baro = (int)(((longvdata & ORE_DATAMSK) >> ORE_DATASHFT) );
               dbaro = (double)baro * configp->ore_bpscale + configp->ore_bpoffset;
               printf("BP "FMT_OREBP"%s ", dbaro, configp->ore_bpunits);
               if ( configp->ore_display_fcast )
                  printf("%s", forecast_txt((longvdata & ORE_FCAST) >> 1));
               break;
            case OreWeightFunc :
               if ( !(longvdata & ORE_VALID) ) {
                  printf("Wgt ---%s ", configp->ore_wgtunits);
                  break;
               }
               cweight = (longvdata & ORE_DATAMSK) >> ORE_DATASHFT;
               weight = ((double)cweight / 10.0) * configp->ore_wgtscale;
               printf("Wgt "FMT_OREWGT"%s ", weight, configp->ore_wgtunits);
               break;
            case ElsCurrFunc :
               if ( !(longvdata & ORE_VALID) ) {
                  printf("Curr ----A ");
                  if ( configp->els_voltage > 0.0 )
                     printf("Pwr ----VA ");
                  break;
               }
               deciamps = (longvdata & ORE_DATAMSK) >> ORE_DATASHFT;
               amps = (double)deciamps / 10.0;
               printf("Curr %4.1fA ", amps);
               if ( configp->els_voltage > 0.0 ) {
                  printf("Pwr %4.0fVA ", amps * configp->els_voltage);
               }
               break;
            case OreWindSpFunc :
               if ( !(longvdata & ORE_VALID) ) {
                  printf("Speed ----%s ", configp->ore_windunits);
                  break;
               }
               dspeed = (longvdata & ORE_DATAMSK) >> ORE_DATASHFT;
               printf("Speed "FMT_OREWSP"%s ", ((double)dspeed / 10.0) * configp->ore_windscale, configp->ore_windunits);
               break;
            case OreWindAvSpFunc :
               if ( !(longvdata & ORE_VALID) ) {
                  printf("AvSpeed ----%s ", configp->ore_windunits);
                  break;
               }
               davspeed = (longvdata & ORE_DATAMSK) >> ORE_DATASHFT;
               printf("AvSpeed "FMT_OREWSP"%s ", ((double)davspeed / 10.0) * configp->ore_windscale, configp->ore_windunits);
               break;
            case OreWindDirFunc :
               if ( !(longvdata & ORE_VALID) ) {
                  printf("Dir ----deg ");
                  break;
               }
               direction = (longvdata & ORE_DATAMSK) >> ORE_DATASHFT;
               direction += configp->ore_windsensordir;
               if ( direction > 3600 )
                  direction %= 3600;
               else if ( direction < 0 )
                  direction += 3600;
               printf("Dir %4.1fdeg ", (double)direction / 10.0);
               break;
            case OreRainRateFunc :
               longvdata = x10global.data_storage[loc + (2 * j)];
               status = longvdata & 0xff;
               if ( subtype == OreRain1 ) {
                  unitstring = (*configp->ore_rainrateunits) ? configp->ore_rainrateunits : "mm/hr";
               }
               else {
                  unitstring = (*configp->ore_rainrateunits) ? configp->ore_rainrateunits : "inches/hr";
               }

               if ( !(longvdata & ORE_VALID) ) {
                  printf("Rate ----%s ", unitstring);
                  break;
               }
               else {
                  drain = x10global.data_storage[loc + (2 * j) + 1];
                  printf("Rate "FMT_ORERRATE"%s ", ((double)drain / 1000.0) * configp->ore_rainratescale, unitstring); 
               }

               break;
            case OreRainTotFunc :
               longvdata = x10global.data_storage[loc + (2 * j)];
               status = longvdata & 0xff;
               if ( subtype == OreRain1 ) {
                  unitstring = (*configp->ore_raintotunits) ? configp->ore_raintotunits : "mm";
               }
               else {
                  unitstring = (*configp->ore_raintotunits) ? configp->ore_raintotunits : "inches";
               }

               if ( !(longvdata & ORE_VALID) ) {
                  printf("Total ----%s ", unitstring);
                  break;
               }
               else {
                 drain = x10global.data_storage[loc + (2 * j) + 1];
                 printf("Total "FMT_ORERTOT"%s ", ((double)drain / 1000.0) * configp->ore_raintotscale, unitstring);
               }

               break;

            case OwlPowerFunc :
               longvdata = x10global.data_storage[loc + statusoffset];
               status = longvdata & 0xff;
               if ( !(longvdata & ORE_VALID ) ) {
                  printf("Power ----kW ");
               }
               else {
                  dblpower = (double)x10global.data_storage[loc + statusoffset + 1];
                  dblpower = dblpower / 1000.0 * OWLPSC * configp->owl_calib_power * (configp->owl_voltage / OWLVREF);
                  printf("Power %.3fkW ", dblpower);
               }
               break;

            case OwlEnergyFunc :
               longvdata = x10global.data_storage[loc + statusoffset];
               status = longvdata & 0xff;
               if ( !(longvdata & ORE_VALID ) ) {
                  printf("Energy ----kWh ");
               }
               else {
                  dblenergy = hilo2dbl(x10global.data_storage[loc + statusoffset + 2],
                                      x10global.data_storage[loc + statusoffset + 1]);
                  dblenergy = dblenergy / 10000.0 * OWLESC * configp->owl_calib_energy * (configp->owl_voltage / OWLVREF);
                  printf("Energy %.4fkWh ", dblenergy);
               }
               break;
                  
         }
      }

      if ( status & ORE_BATLVL ) {
         blevel = 10 * (int)((longvdata & ORE_BATMSK) >> ORE_BATSHFT);
         printf("BatLvl %d%% ", blevel );
         if ( blevel <= configp->ore_lobat )
            printf("LoBat ");
      }
      else if ( status & ORE_LOBAT ) {
         printf("LoBat ");
      }

      printf("\n");
      index++;
   }

   return 0;
}
#endif /* HASORE */

#ifdef HASORE 
/*---------------------------------------------------------------------+
 | Display an Oregon data value stored in the x10state structure.      |
 +---------------------------------------------------------------------*/
int c_orecmds ( int argc, char *argv[] )
{

   ALIAS          *aliasp;
   unsigned char  hcode, ucode, func;
   unsigned long  aflags;
   unsigned long  longvdata, rain;
   char           hc;
   unsigned int   bitmap;
   int            j;
   int            unit, index, dtempc, baro, cweight, deciamps;
   int            loc;
   int            dspeed, angle;
   int            uvfactor;
   double         temp, dbaro, weight, dblpower, dblenergy;

   static struct {
      char          *cmd;
      unsigned char func;
   } cmdlist[] = {
      {"oretemp",     OreTempFunc     },
      {"orerh",       OreHumidFunc    },
      {"orebp",       OreBaroFunc     },
      {"orewgt",      OreWeightFunc   },
      {"elscurr",     ElsCurrFunc     },
      {"orewindsp",   OreWindSpFunc   },
      {"orewindavsp", OreWindAvSpFunc },
      {"orewinddir",  OreWindDirFunc  },
      {"orerainrate", OreRainRateFunc },
      {"oreraintot",  OreRainTotFunc  },
      {"oreuv",       OreUVFunc       },
      {"owlpower",    OwlPowerFunc    },
      {"owlenergy",   OwlEnergyFunc   },
      {"",            0xff            },
   };

   int read_x10state_file ( void );

   if ( argc < 3 ) {
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
   func = 0xff;
   while ( *cmdlist[j].cmd ) {
      if ( strcmp(argv[1], cmdlist[j].cmd) == 0 ) {
         func = cmdlist[j].func;
         break;
      }
      j++;
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

   if ( (index = alias_lookup_index(hc, bitmap, RF_OREGON)) < 0    &&
        (index = alias_lookup_index(hc, bitmap, RF_ELECSAVE)) < 0  &&
        (index = alias_lookup_index(hc, bitmap, RF_OWL)) < 0          ) {
      fprintf(stderr, "Address %c%d is not configured as a sensor in the Oregon group\n", hc, unit);
      return 1;
   }

   loc = -1;
   for ( j = 0; j < aliasp[index].nvar; j++ ) {
      if ( aliasp[index].funclist[j] == func ) {
         loc = aliasp[index].storage_index + aliasp[index].statusoffset[j];
         break;
      }
   }
   if ( loc < 0 ) {
      fprintf(stderr, "Command '%s' not valid for this sensor.\n", argv[1]);
      return 1;
   }

   longvdata = x10global.data_storage[loc];

   if ( !(longvdata & ORE_VALID) ) {
      fprintf(stderr, "Not ready\n");
      return 1;
   }

   switch ( func ) {
      case OreTempFunc :
         dtempc = (longvdata & ORE_DATAMSK) >> ORE_DATASHFT;
         if ( longvdata & ORE_NEGTEMP )
            dtempc = -dtempc;
         temp = celsius2temp((double)dtempc / 10.0, configp->ore_tscale, configp->ore_toffset);
         printf("%.1f\n", temp);
         break;
     case OreHumidFunc :
         printf("%d\n", (int)((longvdata & ORE_DATAMSK) >> ORE_DATASHFT));
         break;
     case OreBaroFunc :
         baro = (longvdata & ORE_DATAMSK) >> ORE_DATASHFT;
         dbaro = (double)baro * configp->ore_bpscale + configp->ore_bpoffset;
         printf("%.1f\n", dbaro);
         break;
     case OreWeightFunc :
         cweight = (longvdata & ORE_DATAMSK) >> ORE_DATASHFT;
         weight = (double)cweight / 10.0 * configp->ore_wgtscale;
         printf(FMT_OREWGT"\n", weight);
         break;
     case ElsCurrFunc :
         deciamps = (longvdata & ORE_DATAMSK) >> ORE_DATASHFT;
         printf("%.1f\n", (double)deciamps / 10.0);
         break;
     case OreWindSpFunc :
         dspeed = (longvdata & ORE_DATAMSK) >> ORE_DATASHFT;
         printf("%.1f\n", (double)dspeed / 10.0 * configp->ore_windscale);
         break;

     case OreWindAvSpFunc :
         dspeed = (longvdata & ORE_DATAMSK) >> ORE_DATASHFT;
         printf("%.1f\n", (double)dspeed / 10.0 * configp->ore_windscale);
         break;

     case OreWindDirFunc :
         angle = (longvdata & ORE_DATAMSK) >> ORE_DATASHFT;
         angle = (angle + configp->ore_windsensordir + 3600) % 3600;
         printf("%.1f\n", (double)angle / 10.0);
         break;

     case OreRainRateFunc :
        rain = x10global.data_storage[loc + 1];
        printf(FMT_ORERRATE"\n", (double)rain / 1000.0 * configp->ore_rainratescale);
        break;

     case OreRainTotFunc :
        rain = x10global.data_storage[loc + 1];
        printf(FMT_ORERTOT"\n", (double)rain / 1000.0 * configp->ore_raintotscale);
        break;

     case OreUVFunc :
         uvfactor = (longvdata & ORE_DATAMSK) >> ORE_DATASHFT;
         printf("%d\n", uvfactor);
         break;

     case OwlPowerFunc :
         dblpower = (double)x10global.data_storage[loc + 1];
         dblpower = dblpower / 1000.0 * OWLPSC * configp->owl_calib_power * (configp->owl_voltage / OWLVREF);
         printf("%.3f\n", dblpower);
         break;

     case OwlEnergyFunc :
         dblenergy = hilo2dbl(x10global.data_storage[loc + 2], x10global.data_storage[loc + 1]);
         dblenergy = dblenergy / 10000.0 * OWLESC * configp->owl_calib_energy * (configp->owl_voltage / OWLVREF);
         printf("%.4f\n", dblenergy);
         break;

     default :
         break;
  }
  return 0;
}




/*----------------------------------------------------------------------------+
 | Send emulated Oregon data to dummy module.                                 |
 | Buffer reference: ST_COMMAND, ST_ORE_EMU, plus:                            |
 |  vtype, subindex, hcode, ucode, seq, data[0], data[1], data[2], data[3]    |
 |   [2]     [3]      [4]    [5]   [6]    [7]     [8]      [9]      [10]      |
 | where data[0] ... data[3] are the littleendian bytes of a 4-byte unsigned  |
 | long value.                                                                |
 +----------------------------------------------------------------------------*/
int send_ore_emu ( unsigned char vtype, unsigned char subindex, int seq,
    unsigned char hcode, unsigned char ucode, unsigned long lvalue )
{
   extern int sptty;
   extern int heyu_parent;
   char writefilename[PATH_LEN + 1];
   unsigned char outbuf[128];
   int  j;
   int  ignoret;

   static unsigned char template[18] = {
    0xff,0xff,0xff,3,ST_COMMAND,ST_SOURCE,0,
    0xff,0xff,0xff,0,ST_COMMAND,ST_ORE_EMU,0,0,0,0,0};

   sprintf(writefilename, "%s%s", WRITEFILE, configp->suffix);

   template[6]  = (unsigned char)heyu_parent;
   template[10] = 11;
   template[13] = vtype;
   template[14] = subindex;
   template[15] = hcode;
   template[16] = ucode;
   template[17] = seq;

   memcpy(outbuf, template, sizeof(template));
   for ( j = 0; j < 4; j++ ) {
      outbuf[(sizeof(template) + j)] = (lvalue >> (8 * j)) & 0xff;
   }

   ignoret = write(sptty, (char *)outbuf, (sizeof(template)) + 4);

   return 0;
}

/*---------------------------------------------------------------------+
 | Store an emulated Oregon data value in the x10state structure.      |
 | 'heyu ore_emu Hu <orefunc> <funcvalue>'                             |
 +---------------------------------------------------------------------*/
int c_ore_emu ( int argc, char *argv[] )
{

   ALIAS          *aliasp;
   unsigned char  hcode, ucode, func, seq, subtype;
   unsigned long  aflags;
   unsigned long  longvdata;
   char           hc;
   char           tscale;
   unsigned int   bitmap;
   int            j, subindex;
   int            unit, index,loc;
   double         tempc, dvalue;
   unsigned char  *dunit;
   char           bpunits[sizeof(config.ore_bpunits)];
   char           bpinunits[sizeof(config.ore_bpunits)];

   int            channel = 1;  /* Dummy channel */

   static struct {
      char          *cmd;
      unsigned char func;
   } cmdlist[] = {
      {"oretemp",   OreTempFunc   },
      {"orerh",     OreHumidFunc  },
      {"orebp",     OreBaroFunc   },
      {"",          0xff          },
   };

   if ( argc < 5 ) {
      fprintf(stderr, "Usage: %s %s Hu <oretemp|orerh|orebp> <value>\n", argv[0], argv[1]);
      return 1;
   }

   if ( check_for_engine() != 0 ) {
      fprintf(stderr, "State engine is not running.\n");
      return 1;
   }

   if ( (aliasp = configp->aliasp) == NULL )
      return 1;

   j = 0;
   func = 0xff;
   while ( *cmdlist[j].cmd ) {
      if ( strcmp(argv[3], cmdlist[j].cmd) == 0 ) {
         func = cmdlist[j].func;
         break;
      }
      j++;
   }

   if ( func == 0xff ) {
      fprintf(stderr, "Function must be oretemp, orerh, or orebp.\n");
      return 1;
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

   if ( (index = alias_lookup_index(hc, bitmap, RF_OREGON)) < 0 ) {
      fprintf(stderr, "Address %c%d is not configured as an Oregon emulation module type\n", hc, unit);
      return 1;
   }

   subtype = aliasp[index].subtype;
   subindex = -1;
   for ( j = 0; j < orechk_size; j++ ) {
      if ( orechk[j].subtype == subtype && orechk[j].support_mode == 2 ) {
         subindex = j;
         break;
      }
   }

   if ( subindex < 0 ) {
      fprintf(stderr, "Module is not an Oregon emulation dummy type\n");
      return 1;
   }
      
   loc = -1;
   for ( j = 0; j < aliasp[index].storage_units; j++ ) {
      if ( aliasp[index].funclist[j] == func ) {
         loc = aliasp[index].storage_index + j;
         break;
      }
   }
   if ( loc < 0 ) {
      fprintf(stderr, "Function '%s' is not supported for this sensor.\n", argv[3]);
      return 1;
   }

   tscale = (configp->ore_data_entry == SCALED) ? configp->ore_tscale : 'C';

   dvalue = strtod(argv[4], (char **)(&dunit));

   longvdata = 0;
   switch ( func ) {
      case OreTempFunc :
         if ( !strchr(" /t/r/nCFKR", toupper(*dunit)) ) {
            fprintf(stderr, "Invalid temperature scale '%s'\n", dunit);
            return 1;
         }
         if ( *dunit && strchr("CFKR", toupper(*dunit)) )
            tscale = toupper(*dunit);

         tempc = temp2celsius(dvalue, tscale, 0.0);
          
         if ( tempc < 0.0 ) {
            longvdata |= ORE_NEGTEMP;
            tempc = -tempc;
         }
         if ( tempc > 99.0 ) {
            fprintf(stderr, "Temperature "FMT_ORET" is out of range.\n", dvalue);
            return 1;
         }
         longvdata |= ((unsigned long)((tempc + .05) * 10.0)) << 8 | ORE_VALID;
         longvdata |= (unsigned long)channel << ORE_CHANSHFT;
         seq = 1;
         break;
     case OreHumidFunc :
         if ( orechk[subindex].nvar < 2 ) {
            fprintf(stderr, "Function orerh is invalid for this module type.\n");
            return 1;
         }

         if ( !strchr(" /t/r/n%%", *dunit) ) {
            fprintf(stderr, "Invalid RH scale '%c'\n", *dunit);
            return 1;
         }
         if ( dvalue < 0.0 || dvalue > 255.0 ) {
            fprintf(stderr, "Relative Humidity %.0f%% is out of range.\n", dvalue);
            return 1;
         }
         longvdata = ((unsigned long)(dvalue + 0.5)) << 8 | ORE_VALID;
         longvdata |= (unsigned long)channel << ORE_CHANSHFT;
         seq = 2;
         break;
     case OreBaroFunc :
         if ( orechk[subindex].nvar < 3 ) {
            fprintf(stderr, "Function orebp is invalid for this module type.\n");
            return 1;
         }
         if ( !strchr(" /t/r/n", *dunit) ) {
            strncpy2(bpinunits, (char *)dunit, sizeof(bpinunits) - 1);
            strlower(bpinunits);
            strncpy2(bpunits, configp->ore_bpunits, sizeof(bpunits) - 1);
            strlower(bpunits);
            if ( strcmp(bpinunits, bpunits) == 0 ) {
               longvdata = (unsigned long)(dvalue / configp->ore_bpscale);
            }
            else if ( strcmp(bpinunits, "hpa") == 0 ) {
               longvdata = (unsigned long)dvalue;
            }
            else {
               fprintf(stderr, "BP unit '%s' not recognized\n", dunit);
               return 1;
            }
         }
         else if ( configp->ore_data_entry == SCALED ) {
            longvdata = (unsigned long)(dvalue /configp->ore_bpscale);
         }
         else {
            longvdata = (unsigned long)dvalue;
         }

         if ( dvalue < 0.0 || longvdata > 0xffffu ) {
            fprintf(stderr, "Barometric Pressure "FMT_OREBP" is out of range.\n", dvalue);
            return 1;
         }
         longvdata = (longvdata << ORE_DATASHFT) | ORE_VALID;
         longvdata |= (unsigned long)channel << ORE_CHANSHFT;
         seq = 3;
         break;

     default :
         return 0;
         break;
  }

  send_ore_emu(RF_OREGON, subindex, seq, hcode, ucode, longvdata);

  return 0;
}


#endif /* HASORE */

                          
int ore_maxmin_temp ( ALIAS *aliasp, int aliasindex, char **tokens, int *ntokens )
{
#ifdef HASORE
   double tvalue;
   int    tflag;
   char   tscale, tscale_init;

   int temp_parm ( char **, int *, char *, char *, int *, double * );

   if ( configp->ore_data_entry == NATIVE )
      tscale_init = 'C';
   else
      tscale_init = configp->ore_tscale;

   tscale = tscale_init;
   if ( temp_parm(tokens, ntokens, "TMIN", &tscale, &tflag, &tvalue) != 0 )
      return 1;
   if ( tflag ) {
      aliasp[aliasindex].optflags2 |= MOPT2_TMIN;
      tvalue = temp2celsius (tvalue, tscale, 0.0 );
      if ( tvalue < -200.0 || tvalue > 200.0 ) {
         store_error_message("TMIN value is out of range.");
         return 1;
      }

      /* Round to nearest 0.1C */
      if ( tvalue < 0.0 )
         tvalue = (tvalue * 10.0) - 0.5;
      else
         tvalue = (tvalue * 10.0) + 0.5;

      aliasp[aliasindex].tmin = (int)tvalue;
   }

   tscale = tscale_init;
   if ( temp_parm(tokens, ntokens, "TMAX", &tscale, &tflag, &tvalue) != 0 )
      return 1;
   if ( tflag ) {
      aliasp[aliasindex].optflags2 |= MOPT2_TMAX;
      tvalue = temp2celsius (tvalue, tscale, 0.0 );
      if ( tvalue < -200.0 || tvalue > 200.0 ) {
         store_error_message("TMAX value is out of range.");
         return 1;
      }

      /* Round to nearest 0.1C */
      if ( tvalue < 0.0 )
         tvalue = (tvalue * 10.0) - 0.5;
      else
         tvalue = (tvalue * 10.0) + 0.5;

      aliasp[aliasindex].tmax = (int)tvalue;
   }

   return 0;
#else
   return 0;
#endif
}   

int ore_maxmin_rh ( ALIAS *aliasp, int aliasindex, char **tokens, int *ntokens )
{
#ifdef HASORE
   double rhvalue;
   int    rhflag;
   char   rhscale;

   int rh_parm ( char **, int *, char *, char *, int *, double * );

   if ( rh_parm(tokens, ntokens, "RHMIN", &rhscale, &rhflag, &rhvalue) != 0 )
      return 1;
   if ( rhflag ) {
      aliasp[aliasindex].optflags2 |= MOPT2_RHMIN;
      if ( rhvalue < 0.0 || rhvalue > 100.0 ) {
         store_error_message("RHMIN value is out of range.");
         return 1;
      }

      /* Round to nearest 1% */
      rhvalue += 0.5;

      aliasp[aliasindex].rhmin = (int)rhvalue;
   }

   if ( rh_parm(tokens, ntokens, "RHMAX", &rhscale, &rhflag, &rhvalue) != 0 )
      return 1;
   if ( rhflag ) {
      aliasp[aliasindex].optflags2 |= MOPT2_RHMAX;
      if ( rhvalue < 0.0 || rhvalue > 100.0 ) {
         store_error_message("RHMAX value is out of range.");
         return 1;
      }

      /* Round to nearest 1% */
      rhvalue += 0.5;

      aliasp[aliasindex].rhmax = (int)rhvalue;
   }

   return 0;
#else
   return 0;
#endif
}   

int ore_maxmin_bp ( ALIAS *aliasp, int aliasindex, char **tokens, int *ntokens )
{
#ifdef HASORE
   double bpvalue;
   int    bpflag;
   char   bpunits[NAME_LEN + 1];
   char   conf_units[NAME_LEN + 1];

   int bp_parm ( char **, int *, char *, char *, int *, double * );

   strcpy(conf_units, configp->ore_bpunits);
   strupper(conf_units);

   if ( bp_parm(tokens, ntokens, "BPMIN", bpunits, &bpflag, &bpvalue) != 0 ) {
      return 1;
   }
   else if ( bpflag ) {
      if ( !(*bpunits) ) {
         if (configp->ore_data_entry == SCALED )
            bpvalue = (bpvalue - configp->ore_bpoffset) / configp->ore_bpscale;
         else
            bpvalue = bpvalue - configp->ore_bpoffset;
      }
      else if ( strcmp(bpunits, "HPA") == 0 ) {
         bpvalue = bpvalue - (configp->ore_bpoffset / configp->ore_bpscale);
      }
      else if ( strcmp(bpunits, conf_units) == 0 ) {
         bpvalue = (bpvalue - configp->ore_bpoffset) / configp->ore_bpscale;
      }
      else {
         store_error_message("BPMIN units not recognized.");
         return 1;
      }

      if ( bpvalue < 0.0 || bpvalue > 1500.0 ) {
         store_error_message("BPMIN value is out of range.");
         return 1;
      }
      aliasp[aliasindex].optflags2 |= MOPT2_BPMIN;
      aliasp[aliasindex].bpmin = (int)(bpvalue + 0.5);
   }

   if ( bp_parm(tokens, ntokens, "BPMINL", bpunits, &bpflag, &bpvalue) != 0 ) {
      return 1;
   }
   else if ( bpflag ) {
      if ( !(*bpunits) ) {
         if (configp->ore_data_entry == SCALED )
            bpvalue = bpvalue / configp->ore_bpscale;
         else
            bpvalue = bpvalue;
      }
      else if ( strcmp(bpunits, "HPA") == 0 ) {
         bpvalue = bpvalue;
      }
      else if ( strcmp(bpunits, conf_units) == 0 ) {
         bpvalue = bpvalue / configp->ore_bpscale;
      }
      else {
         store_error_message("BPMINL units not recognized.");
         return 1;
      }

      if ( bpvalue < 0.0 || bpvalue > 1500.0 ) {
         store_error_message("BPMINL value is out of range.");
         return 1;
      }
      aliasp[aliasindex].optflags2 |= MOPT2_BPMIN;
      aliasp[aliasindex].bpmin = (int)(bpvalue + 0.5);
   }


   if ( bp_parm(tokens, ntokens, "BPMAX", bpunits, &bpflag, &bpvalue) != 0 ) {
      return 1;
   }
   else if ( bpflag ) {
      if ( !(*bpunits) ) {
         if (configp->ore_data_entry == SCALED )
            bpvalue = (bpvalue - configp->ore_bpoffset) / configp->ore_bpscale;
         else
            bpvalue = bpvalue - configp->ore_bpoffset;
      }
      else if ( strcmp(bpunits, "HPA") == 0 ) {
         bpvalue = bpvalue - (configp->ore_bpoffset / configp->ore_bpscale);
      }
      else if ( strcmp(bpunits, conf_units) == 0 ) {
         bpvalue = (bpvalue - configp->ore_bpoffset) / configp->ore_bpscale;
      }
      else {
         store_error_message("BPMAX units not recognized.");
         return 1;
      }

      if ( bpvalue < 0.0 || bpvalue > 1500.0 ) {
         store_error_message("BPMAX value is out of range.");
         return 1;
      }
      aliasp[aliasindex].optflags2 |= MOPT2_BPMAX;
      aliasp[aliasindex].bpmax = (int)(bpvalue + 0.5);
   }

   if ( bp_parm(tokens, ntokens, "BPMAXL", bpunits, &bpflag, &bpvalue) != 0 ) {
      return 1;
   }
   else if ( bpflag ) {
      if ( !(*bpunits) ) {
         if (configp->ore_data_entry == SCALED )
            bpvalue = bpvalue / configp->ore_bpscale;
         else
            bpvalue = bpvalue;
      }
      else if ( strcmp(bpunits, "HPA") == 0 ) {
         bpvalue = bpvalue;
      }
      else if ( strcmp(bpunits, conf_units) == 0 ) {
         bpvalue = bpvalue / configp->ore_bpscale;
      }
      else {
         store_error_message("BPMAXL units not recognized.");
         return 1;
      }

      if ( bpvalue < 0.0 || bpvalue > 1500.0 ) {
         store_error_message("BPMAXL value is out of range.");
         return 1;
      }
      aliasp[aliasindex].optflags2 |= MOPT2_BPMAX;
      aliasp[aliasindex].bpmax = (int)(bpvalue + 0.5);
   }


   return 0;
#else
   return 0;
#endif
}   

/*---------------------------------------------------------------------+
 |  Adjust the number of Electrisave sensors (1-3) per config.         |
 +---------------------------------------------------------------------*/
int set_elec1_nvar ( int nvar )
{
#ifdef HASORE
   int j;

   for ( j = 0; j < orechk_size; j++ ) {
      if ( orechk[j].subtype == OreElec1 )
         orechk[j].nvar = nvar;
   }
#endif
   return 0;
}

