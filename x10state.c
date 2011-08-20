/*----------------------------------------------------------------------------+
 |                                                                            |
 |              State and Script functions for HEYU                           |
 |            Copyright 2004-2010 Charles W. Sullivan                         |
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#if defined(SYSV) || defined(FREEBSD) || defined(OPENBSD)
#include <string.h>
#else
#include <strings.h>
#endif
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <syslog.h>
#include <sys/time.h>

#ifdef POSIX
#define EXEC_POSIX
#endif

#ifdef EXEC_POSIX
#include <sys/wait.h>
#endif

#include <signal.h>
#include <time.h>
#include "x10.h"
#include "process.h"
#include "sun.h"
#include "x10state.h"
#include "rfxcom.h"
#include "digimax.h"
#include "oregon.h"
#include "local.h"
#include "version.h"

#ifdef pid_t
#define PID_T pid_t
#else
#define PID_T long
#endif


extern CONFIG config;
extern CONFIG *configp;

extern char *funclabel[];
extern int tty, sptty, verbose;
extern int i_am_monitor, i_am_state, i_am_relay, i_am_rfxmeter;
extern int heyu_parent;
extern FILE *fdsout, *fdserr;

extern char *heyu_tzname[2];
extern char *datstrf( void );
extern void datstrf2( char * );

extern char heyu_script[];
extern char statefile[];
extern char enginelockfile[];
extern char heyu_config[PATH_LEN + 1];

#define PROMSIZE 1024
static unsigned char image[PROMSIZE + 10];

extern unsigned int modmask[NumModMasks][16];
extern unsigned int vmodmask[NumVmodMasks][16];
extern unsigned int kmodmask[NumKmodMasks][16];
extern unsigned char maxdimlevel[16][16];
extern unsigned char ondimlevel[16][16];

extern char *wday_name[7];
extern char *month_name[12];

struct x10global_st x10global;
x10hcode_t *x10state = x10global.x10hcode;

#define SpecPLC  0x00000001   /* Specific PLC */
#define GenPLC   0x00000002   /* Generic PLC */
#define SpecSec  0x00000004   /* Specific Security */
#define SpecRFX  0x00000008   /* Specific RFXSensor */
#define SpecDMX  0x00000010   /* Specific Digimax */
#define SpecOre  0x00000020   /* Specific Oregon */
#define SpecKaku 0x00000040   /* Specific Kaku */
#define AnyFlag  0x80000000   /* All of any above group */

#define AnyPLC   (AnyFlag | SpecPLC)
#define AnySec   (AnyFlag | SpecSec)
#define AnyRFX   (AnyFlag | SpecRFX)
#define AnyDMX   (AnyFlag | SpecDMX)
#define AnyFunc  (AnyFlag | SpecPLC)
#define AnyOre   (AnyFlag | SpecOre)
#define AnyKaku  (AnyFlag | SpecKaku)

/* General commands which can launch a script */
/* (Maximum of 32 distinct xxxTrig entries in */
/* each table, to fit into an unsigned long   */
/* integer bitmap.)                           */

static struct trig_type_st {
   char          *label;
   unsigned long  flags;
   int           signal;
} trig_type[] = {
  {"anyplc",       AnyPLC,  0},
  {"on",           SpecPLC, OnTrig},
  {"off",          SpecPLC, OffTrig},
  {"dim",          SpecPLC, DimTrig},
  {"bright",       SpecPLC, BriTrig},
  {"lightson",     SpecPLC, LightsOnTrig},
  {"lightsoff",    SpecPLC, LightsOffTrig},
  {"allon",        SpecPLC, AllOnTrig},
  {"alloff",       SpecPLC, AllOffTrig},
  {"statuson",     SpecPLC, StatusOnTrig},
  {"status_on",    SpecPLC, StatusOnTrig},
  {"statusoff",    SpecPLC, StatusOffTrig},
  {"status_off",   SpecPLC, StatusOffTrig},
  {"status",       SpecPLC, StatusReqTrig},
  {"statusreq",    SpecPLC, StatusReqTrig},
  {"status_req",   SpecPLC, StatusReqTrig},
  {"preset",       SpecPLC, PresetTrig},
  {"extended",     SpecPLC, ExtendedTrig},
  {"xpup",         SpecPLC, ExtPowerUpTrig},
  {"xpowerup",     SpecPLC, ExtPowerUpTrig},
  {"hail",         SpecPLC, HailReqTrig},
  {"hail_ack",     SpecPLC, HailAckTrig},
  {"data_xfer",    SpecPLC, DataXferTrig},
  {"vdata",        (SpecPLC|SpecSec), VdataTrig},
  {"vdatam",       (SpecPLC|SpecSec), VdataMTrig},
  {"gon",          GenPLC,  OnTrig},
  {"goff",         GenPLC,  OffTrig},
  {"gdim",         GenPLC,  DimTrig},
};
#define NTRIGTYPES (sizeof(trig_type)/sizeof(struct trig_type_st))

static struct trig_type_st sec_trig_type[] = {
  {"anysec",       AnySec,  0},
  {"panic",        SpecSec, PanicTrig},
  {"arm",          SpecSec, ArmTrig},
  {"disarm",       SpecSec, DisarmTrig},
  {"alert",        SpecSec, AlertTrig},
  {"clear",        SpecSec, ClearTrig},
  {"sectamper",    SpecSec, TamperTrig},
  {"test",         SpecSec, TestTrig},
  {"slightson",    SpecSec, SecLightsOnTrig},
  {"slightsoff",   SpecSec, SecLightsOffTrig},
  {"sdusk",        SpecSec, DuskTrig},
  {"sdawn",        SpecSec, DawnTrig},
  {"akeyon",       SpecSec, AkeyOnTrig},
  {"akeyoff",      SpecSec, AkeyOffTrig},
  {"bkeyon",       SpecSec, BkeyOnTrig},
  {"bkeyoff",      SpecSec, BkeyOffTrig},
  {"vdata",        SpecSec, VdataTrig},
  {"vdatam",       SpecSec, VdataMTrig},
  {"inactive",     SpecSec, InactiveTrig},
};
#define NSECTRIGTYPES (sizeof(sec_trig_type)/sizeof(struct trig_type_st))


static struct trig_type_st rfx_trig_type[] = {
  {"anyrfx",       AnyRFX,  0},
  {"anydmx",       AnyDMX,  0},
  {"dmxtemp",      SpecDMX, DmxTempTrig},
  {"dmxon",        SpecDMX, DmxOnTrig},
  {"dmxoff",       SpecDMX, DmxOffTrig},
  {"dmxsetpoint",  SpecDMX, DmxSetPtTrig},
  {"rfxtemp",      SpecRFX, RFXTempTrig},
  {"rfxtemp2",     SpecRFX, RFXTemp2Trig},
  {"rfxrh",        SpecRFX, RFXHumidTrig},
  {"rfxbp",        SpecRFX, RFXPressTrig},
  {"rfxlobat",     SpecRFX, RFXLoBatTrig},
  {"rfxvad",       SpecRFX, RFXVadTrig},
  {"rfxpot",       SpecRFX, RFXPotTrig},
  {"rfxvs",        SpecRFX, RFXVsTrig},
  {"rfxother",     SpecRFX, RFXOtherTrig},
  {"rfxpulse",     SpecRFX, RFXPulseTrig},
  {"rfxpower",     SpecRFX, RFXPowerTrig},
  {"rfxwater",     SpecRFX, RFXWaterTrig},
  {"rfxgas",       SpecRFX, RFXGasTrig},
  {"rfxcount",     SpecRFX, RFXCountTrig},
};
#define NRFXTRIGTYPES (sizeof(rfx_trig_type)/sizeof(struct trig_type_st))


static struct trig_type_st ore_trig_type[] = {
  {"anyore",       AnyOre,  0},
  {"oretemp",      SpecOre, OreTempTrig},
  {"orerh",        SpecOre, OreHumidTrig},
  {"orebp",        SpecOre, OreBaroTrig},
  {"orewgt",       SpecOre, OreWeightTrig},
  {"elscurr",      SpecOre, ElsCurrTrig},
  {"orewindsp",    SpecOre, OreWindSpTrig},
  {"orewindavsp",  SpecOre, OreWindAvSpTrig},
  {"orewinddir",   SpecOre, OreWindDirTrig},
  {"oreraintot",   SpecOre, OreRainTotTrig},
  {"orerainrate",  SpecOre, OreRainRateTrig},
  {"oreuv",        SpecOre, OreUVTrig},
  {"owlpower",     SpecOre, OwlPowerTrig},
  {"owlenergy",    SpecOre, OwlEnergyTrig},
};
#define NORETRIGTYPES (sizeof(ore_trig_type)/sizeof(struct trig_type_st))

static struct trig_type_st kaku_trig_type[] = {
  {"anykaku",      AnyKaku,  0},
  {"koff",         SpecKaku, KakuOffTrig},
  {"kon",          SpecKaku, KakuOnTrig},
  {"kgrpoff",      SpecKaku, KakuGrpOffTrig},
  {"kgrpon",       SpecKaku, KakuGrpOnTrig},
  {"kpreset",      SpecKaku, KakuPreTrig},
  {"kgrppreset",   SpecKaku, KakuGrpPreTrig},
};
#define NKAKUTRIGTYPES (sizeof(kaku_trig_type)/sizeof(struct trig_type_st))
  

/* Functions included in the 'generic' group */
#define GENFUNCMAP ((1 << OnTrig) | (1 << OffTrig) | (1 << DimTrig))


void arm_delay_complete ( int );
void timer_timeout ( int );

typedef struct {
   char    timername[NAME_LEN];
   void    (*elapsed_func)(int);
} COUNTDOWN;

COUNTDOWN countdown_timer[NUM_USER_TIMERS + 1];

#define ArmDelayTimer  0

void setup_countdown_timers ( void )
{
   int j;

//   x10global.timer_count[ArmDelayTimer] = 0;
   strcpy(countdown_timer[ArmDelayTimer].timername, "armtimer");
   countdown_timer[ArmDelayTimer].elapsed_func = arm_delay_complete;

   for ( j = 1; j <= NUM_USER_TIMERS; j++ ) {
//      x10global.timer_count[j] = 0;
      sprintf(countdown_timer[j].timername, "timer%d", j);
      countdown_timer[j].elapsed_func = timer_timeout;
   }

   return;
}

struct stflags_st {
   char *label;
   int  length;
   int  state;
   int  not;
   int  andstate;
   int  andnot;
} stflags[] = {
  {"on:",            3, OnState,        0, NullState,   1},
  {"noton:",         6, OnState,        1, NullState,   1},
  {"off:",           4, OnState,        1, NullState,   1},
  {"notoff:",        7, OnState,        0, NullState,   1},
  {"dim:",           4, DimState,       0, NullState,   1},
  {"notdim:",        7, DimState,       1, NullState,   1},
  {"alert:",         6, AlertState,     0, NullState,   1},
  {"notalert:",      9, AlertState,     1, NullState,   1},
  {"clear:",         6, ClearState,     0, NullState,   1},
  {"notclear:",      9, ClearState,     1, NullState,   1},
  {"auxalert:",      9, AuxAlertState,  0, NullState,   1},
  {"notauxalert:",  12, AuxAlertState,  1, NullState,   1},
  {"auxclear:",      9, AuxClearState,  0, NullState,   1},
  {"notauxclear:",  12, AuxClearState,  1, NullState,   1},
  {"active:",        7, ActiveState,    0, NullState,   1},
  {"notactive:",    10, ActiveState,    1, NullState,   1},
  {"inactive:",      9, InactiveState,  0, NullState,   1},
  {"notinactive:",  12, InactiveState,  1, NullState,   1},
  {"valid:",         6, ValidState,     0, NullState,   1},
  {"notvalid:",      9, ValidState,     1, NullState,   1},
  {"addr:",          5, AddrState,      0, NullState,   1},
  {"notaddr:",       8, AddrState,      1, NullState,   1},
  {"tamper:",        7, TamperState,    0, NullState,   1},
  {"nottamper:",    10, TamperState,    1, NullState,   1},
  {"chg:",           4, ChgState,       0, NullState,   1},
  {"notchg:",        7, ChgState,       1, NullState,   1},
//  {"activechg:",    10, ActiveChgState, 0, NullState,   1},
//  {"notactivechg:", 13, ActiveChgState, 1, NullState,   1},
//  {"modchg:",        7, ModChgState,    0, NullState,   1},
//  {"notmodchg:",    10, ModChgState,    1, NullState,   1},
};
int num_stflags = sizeof(stflags) / sizeof(struct stflags_st);


struct virtflags_st {
   char          *label;
   int           length;
   unsigned long vflag;
   int           not;
} virtflags[] = {
  {"lobat:",        6, SEC_LOBAT,      0},
  {"notlobat:",     9, SEC_LOBAT,      1},
  {"rollover:",     9, RFX_ROLLOVER,   0},
  {"notrollover:", 12, RFX_ROLLOVER,   1},
  {"swmin:",        6, SEC_MIN,        0},
  {"notswmin:",     9, SEC_MIN,        1},
  {"swmax:",        6, SEC_MAX,        0},
  {"notswmax:",     9, SEC_MAX,        1},
  {"tmin:",         5, ORE_TMIN,       0},
  {"nottmin:",      8, ORE_TMIN,       1},
  {"tmax:",         5, ORE_TMAX,       0},
  {"nottmax:",      8, ORE_TMAX,       1},
  {"rhmin:",        6, ORE_RHMIN,      0},
  {"notrhmin:",     9, ORE_RHMIN,      1},
  {"rhmax:",        6, ORE_RHMAX,      0},
  {"notrhmax:",     9, ORE_RHMAX,      1},
  {"bpmin:",        6, ORE_BPMIN,      0},
  {"notbpmin:",     9, ORE_BPMIN,      1},
  {"bpmax:",        6, ORE_BPMAX,      0},
  {"notbpmax:",     9, ORE_BPMAX,      1},
  {"main:",         5, SEC_MAIN,       0},
  {"notmain:",      8, SEC_MAIN,       1},
  {"aux:",          4, SEC_AUX,        0},
  {"notaux:",       7, SEC_AUX,        1},
  {"init:",         5, DMX_INIT,       0},
  {"notinit:",      8, DMX_INIT,       1},
  {"heat:",         5, DMX_HEAT,       0},
  {"notheat:",      8, DMX_HEAT,       1},
  {"set:",          4, DMX_SET,        0},
  {"notset:",       7, DMX_SET,        1},
//  {"dmxtemp:",      8, DMX_TEMP,       0},
//  {"notdmxtemp:",  11, DMX_TEMP,       1},
};
int num_virtflags = sizeof(virtflags) / sizeof(struct virtflags_st);


/* Heartbeat sensor countdowns and alias indexes */
struct {
   int alias_index;
   long countdown;
} heartbeat_sensor[MAX_SEC_SENSORS];
int num_heartbeat_sensors = 0;

/* Heyu environment masks */
static struct {
   char          *query;
   unsigned long  mask;
} heyumaskval[] = {
   {"whatLevel",   HEYUMAP_LEVEL},
   {"isAppl",      HEYUMAP_APPL },
   {"isSpend",     HEYUMAP_SPEND},
   {"isOff",       HEYUMAP_OFF  },
   {"isAddr",      HEYUMAP_ADDR },
   {"isChg",       HEYUMAP_CHG  },
   {"isDim",       HEYUMAP_DIM  },
   {"isValid",     HEYUMAP_SIGNAL },
   {"isClear",     HEYUMAP_CLEAR},
   {"isAlert",     HEYUMAP_ALERT},
   {"isAuxClear",  HEYUMAP_AUXCLEAR},
   {"isAuxAlert",  HEYUMAP_AUXALERT},
   {"isSdawn",     HEYUMAP_AUXCLEAR},
   {"isSdusk",     HEYUMAP_AUXALERT},
   {"isActive",    HEYUMAP_ACTIVE},
   {"isInactive",  HEYUMAP_INACTIVE},
//   {"isModChg",    HEYUMAP_MODCHG},
   {"isOn",        HEYUMAP_ON   },
   { NULL,         0            },
};

/* Heyu environment masks for Security, RFX, Oregon flags */
static struct {
   char           *query;
   unsigned long  mask;
} heyusecmaskval[] = {
   {"isLoBat",    FLAGMAP_LOBAT    },
   {"isRollover", FLAGMAP_ROLLOVER },
   {"isSwMin",    FLAGMAP_SWMIN    },
   {"isSwMax",    FLAGMAP_SWMAX    },
   {"isMain",     FLAGMAP_MAIN     },
   {"isAux",      FLAGMAP_AUX      },
   {"isTamper",   FLAGMAP_TAMPER   },
   {"isTmin",     FLAGMAP_TMIN     },
   {"isTmax",     FLAGMAP_TMAX     },
   {"isRHmin",    FLAGMAP_RHMIN    },
   {"isRHmax",    FLAGMAP_RHMAX    },
   {"isBPmin",    FLAGMAP_BPMIN    },
   {"isBPmax",    FLAGMAP_BPMAX    },
   {"isInit",     FLAGMAP_DMXINIT  },
   {"isSet",      FLAGMAP_DMXSET   },
   {"isHeat",     FLAGMAP_DMXHEAT  },
//   {"isDmxTemp",  FLAGMAP_DMXTEMP  },
   { NULL,        0                },
};

/* Xtend environment masks */
static struct {
   char           *query;
   unsigned long  mask;
} xtendmaskval[] = {
   {"isAppl",    XTMAP_APPL },
   {"isAddr",    XTMAP_ADDR },
   {"isOn",      XTMAP_ON   },
   { NULL,       0          },
};

/*----------------------------------------------------------------------------+
 | Update the activity timer countdown for a sensor with a heartbeat.         |
 +----------------------------------------------------------------------------*/
void update_activity_timeout ( ALIAS *aliasp, int index )
{
   if ( index >= 0 && aliasp[index].optflags & MOPT_HEARTBEAT &&
        aliasp[index].hb_index >= 0 ) {
      heartbeat_sensor[aliasp[index].hb_index].countdown = aliasp[index].hb_timeout;
   }
   return;
}

/*---------------------------------------------------------------------+
 | Update the Active, Inactive, and ActiveChg states                   |
 +---------------------------------------------------------------------*/
int update_activity_states ( unsigned char hcode, unsigned int bitmap, unsigned char mode )
{
   unsigned int prevstate, newstate, changed;

   prevstate = x10state[hcode].state[InactiveState];

   if ( mode == S_INACTIVE ) {
      newstate = prevstate | bitmap;
   }
   else {
      newstate = prevstate & ~bitmap;
   }
   x10state[hcode].state[InactiveState] = newstate;
   x10state[hcode].state[ActiveState] = ~newstate & x10state[hcode].state[ValidState];

   changed = prevstate ^ newstate;

   x10state[hcode].state[ActiveChgState] =
       (x10state[hcode].state[ActiveChgState] & ~bitmap) | changed;

   return 0;
}

/*---------------------------------------------------------------------+
 | Return 1 if the sensor module type specified in an alias at the     |
 | argument address has a heartbeat, otherwise 0.                      |
 +---------------------------------------------------------------------*/
int has_heartbeat ( unsigned char hcode, unsigned int bitmap )
{
   ALIAS *aliasp;
   int   index, j = 0;

   if ( !(aliasp = configp->aliasp) )
      return 0;

   j = 0;
   while ( (index = lookup_alias_mult(hcode, bitmap, &j) >= 0) ) {
      if ( aliasp[index].optflags & MOPT_HEARTBEAT )
         return 1;
   }

   return 0;
}

/*----------------------------------------------------------------------------+
 | Converts programmed dim level (1-22) to dims (0-210).                      |
 | This is just an approximation as the number of dims actually transmitted   |
 | by the interface varies, apparently at random, but actually depending on   |
 | whether beginning on rising or falling zero crossing of AC waveform.       |
 +----------------------------------------------------------------------------*/
unsigned char level2dims ( unsigned char level, char **prefix, char **suffix )
{
   unsigned int dims, base;

   /* The value of 'base' appears to vary randomly, being either 2 or 3  */
   /* with approximately equal likelyhood.  The value 4 has occasionally */
   /* been observed.  We use 2 here.                                     */
   base = 2;
   dims = (level > 0) ? base + 11 * (level - 1) : base + 11 ;
   if ( dims > 210 ) {
      *prefix = "";
      *suffix = "+";
   }
   else {
      *prefix = "~";
      *suffix = "";
   }
   dims = min(210, dims);

   return (unsigned char)dims;
} 
   
/*----------------------------------------------------------------------------+
 | Converts dims level (0-210) to (int) percent of full ON voltage.           |
 | Linearized from measured data on an X10 LM465 (1-way) Lamp Module          | 
 | controlling a 100 Watt lamp.                                               |
 +----------------------------------------------------------------------------*/ 
int dims2pct ( unsigned char level )
{
   long int pct;

   pct = (level > 32u) ? 90 * (level - 33u) / 177 + 10 :
         (level > 1u)  ?  8 * (level -  2u) / 31  +  2 : 0 ;

   return (int)pct;
} 
   
/*----------------------------------------------------------------------------+
 | Converts type 0 (shutter) level (0-25) to (int) percent of full Open.      |
 | This is just a linear translation.                                         | 
 +----------------------------------------------------------------------------*/ 
int ext0level2pct ( unsigned int level )
{
   return 100 * (int)level /25;
}

#define MEASURED_DATA
#ifdef MEASURED_DATA
/*----------------------------------------------------------------------------+
 | Converts old-style preset level (zero-based, 0-31) to (int) percent of     |
 | full ON voltage.                                                           |
 | This is based on measurement of output voltage for a LampLinc 2000STW      |
 | (2-way) dimmer module controlling a 100 Watt lamp.                         | 
 +----------------------------------------------------------------------------*/
int presetlevel2pct ( unsigned char level )
{
   static unsigned char table[32] = {
       0, 18, 21, 23, 27, 28, 31, 34, 36, 39, 42, 45, 48, 51, 54, 57,
      60, 63, 67, 70, 73, 76, 79, 82, 85, 87, 90, 92, 95, 97, 99, 100
   };

   return (int)table[min(31u, level)]; 
}

/*----------------------------------------------------------------------------+
 | Converts extended preset level (0-63) to (int) percent of full ON voltage. |
 | (This is based on measurement of output voltage for a LM14A module         |
 | controlling a 100 Watt lamp.)                                              |
 +----------------------------------------------------------------------------*/ 
int ext3level2pct ( unsigned char level )
{
   static unsigned char table[64] = {
       0, 12, 13, 15, 15, 17, 18, 20, 21, 22, 
      23, 25, 26, 28, 29, 31, 32, 34, 35, 37,
      38, 40, 41, 43, 44, 46, 48, 50, 51, 53, 
      54, 56, 58, 60, 61, 63, 64, 66, 67, 69, 
      71, 73, 74, 76, 77, 79, 80, 82, 83, 85, 
      86, 88, 88, 90, 91, 92, 93, 95, 96, 97, 
      98, 99, 100, 100
   };

   return (int)table[min(63u, level)];
}

#else
/*----------------------------------------------------------------------------+
 | Converts (old) preset level (0-31) to (int) percent of full ON voltage.    |
 | This is just a linear translation.                                         | 
 +----------------------------------------------------------------------------*/ 
int presetlevel2pct ( unsigned char level )
{
   return 100 * (int)level / 31;
} 
   
/*----------------------------------------------------------------------------+
 | Converts extended preset level (0-63) to (int) percent of full ON voltage. |
 | (This is linearized from actual data - max deviation about 4% of full On). | 
 +----------------------------------------------------------------------------*/ 
int ext3level2pct ( unsigned char level )
{
   level = min(62, level);
   return (level > 0) ? 12 + 88 * ((int)level - 1) / 61 : 0;
}
#endif /* Measured vs Linearized data */

/*----------------------------------------------------------------------------+
 | Convert percent ON voltage to nearest above extended preset level (0-63)   |
 +----------------------------------------------------------------------------*/ 
unsigned char pct2ext3level ( int percent )
{
   unsigned char level;

   if ( percent <= 0 )   return 0u;
   if ( percent > 100 )  return 63u;

   for ( level = 0u; level < 63u; level++ ) {
      if ( ext3level2pct(level) >= percent )
         return level;
   }

   return 63u;
}

/*----------------------------------------------------------------------------+
 | Convert percent ON voltage to nearest above old style preset level (0-31)  |
 +----------------------------------------------------------------------------*/ 
unsigned char pct2presetlevel ( int percent )
{
   unsigned char level;

   if ( percent <= 0 )   return 0u;
   if ( percent >= 100 ) return 31u;

   for ( level = 0u; level < 32u; level++ ) {
      if ( presetlevel2pct(level) >= percent )
         return level;
   }

   return 31u;
}

/*----------------------------------------------------------------------------+
 | Convert percent of ON voltage to nearest above dims level (0-210)          |
 +----------------------------------------------------------------------------*/ 
unsigned char pct2dims ( int percent )
{
   unsigned char dims;

   if ( percent <= 0 )  return 0;
   if ( percent > 100 ) return 210;

   for ( dims = 0u; dims < 211u; dims++ ) {
      if ( dims2pct(dims) >= percent )
         return dims;
   }
   
   return 210;
}

/*----------------------------------------------------------------------------+
 | Convert dims (0-210) to nearest CM11A bright/dim level (0-0x31)            |
 +----------------------------------------------------------------------------*/ 
unsigned char dims2level ( unsigned char dims )
{
   if ( dims == 0 ) return 0u;
   if ( dims >= 210 ) return 20u;

   dims = max(0, (int)dims - 2);

   return min(31u, (1 + (dims / 11u)));
}


/*----------------------------------------------------------------------------+
 | Return the name of the function for the argument trigger signal            |
 +----------------------------------------------------------------------------*/
char *lookup_function ( int signal ) 
{
   int j;

   for ( j = 0; j < (int)NTRIGTYPES; j++ ) {
      if ( trig_type[j].signal == signal )
         return trig_type[j].label;
   }
   return "";
}

/*----------------------------------------------------------------------------+
 | Compare launcher flags (security, common, counter-zero) with state tables. |
 | Return 1 if not matched, 0 otherwise.                                      |
 +----------------------------------------------------------------------------*/
int is_unmatched_flags ( LAUNCHER *launcherpp )
{
   int           k, m;
   unsigned char hcode, ucode;
   unsigned int  bitmap;

   if ( (launcherpp->sflags & x10global.sflags) != launcherpp->sflags ||
        (launcherpp->notsflags & ~x10global.sflags) != launcherpp->notsflags ) {
      return 1;
   }

   for ( k = 0; k < NUM_FLAG_BANKS; k++ ) {
      if ( (launcherpp->flags[k] & x10global.flags[k]) != launcherpp->flags[k] ||
           (launcherpp->notflags[k] & ~x10global.flags[k]) != launcherpp->notflags[k] ) {
         return 1;
      }
   }

   for ( k = 0; k < NUM_COUNTER_BANKS; k++ ) {
      if ( (launcherpp->czflags[k] & x10global.czflags[k]) != launcherpp->czflags[k] ||
           (launcherpp->notczflags[k] & ~x10global.czflags[k]) != launcherpp->notczflags[k] ) {
         return 1;
      }
   }

   for ( k = 0; k < NUM_TIMER_BANKS; k++ ) {
      if ( (launcherpp->tzflags[k] & x10global.tzflags[k]) != launcherpp->tzflags[k] ||
           (launcherpp->nottzflags[k] & ~x10global.tzflags[k]) != launcherpp->nottzflags[k] ) {
         return 1;
      }
   }

   for ( k = 0; k < launcherpp->num_stflags; k++ ) {
      m = launcherpp->stlist[k].stindex;
      hcode = launcherpp->stlist[k].hcode;
      bitmap = launcherpp->stlist[k].bmap;
      if ( stflags[m].not ) {
         if ( (x10state[hcode].state[stflags[m].state] & bitmap) != 0 )
            return 1;
      }
      else {
         if ( (x10state[hcode].state[stflags[m].state] & bitmap) != bitmap )
            return 1;
      }
      if ( stflags[m].andnot ) {
         if ( (x10state[hcode].state[stflags[m].andstate] & bitmap) != 0 )
            return 1;
      }
      else {
         if ( (x10state[hcode].state[stflags[m].andstate] & bitmap) != bitmap )
            return 1;
      }
   }
   
   for ( k = 0; k < launcherpp->num_virtflags; k++ ) {
      m = launcherpp->virtlist[k].vfindex;
      hcode = launcherpp->virtlist[k].hcode;
      ucode = launcherpp->virtlist[k].ucode;
      if ( virtflags[m].not ) {
         if ( (x10state[hcode].vflags[ucode] & virtflags[m].vflag) != 0 )
            return 1;
      }
      else {
         if ( (x10state[hcode].vflags[ucode] & virtflags[m].vflag) == 0 )
            return 1;
      }
   }
   
   return 0;
}

/*----------------------------------------------------------------------------+
 | Initialize the state structure elements for (X10 encoded) hcode            |
 +----------------------------------------------------------------------------*/
void x10state_init_old ( unsigned char hcode )
{
   void reset_security_sensor_timeout ( unsigned char, unsigned int );

   ALIAS *aliasp;
   int   j, k;
   int   ucode, group;
   char  hc;

   hc = code2hc(hcode);

   x10state[hcode].lastunit = 0;
   x10state[hcode].lastcmd = 0xff;
   x10state[hcode].reset = 0;
   x10state[hcode].addressed = 0;
   x10state[hcode].sticky = 0;
   x10state[hcode].exclusive = 0;
   x10state[hcode].vaddress = 0;
   x10state[hcode].squelch = 0;
   x10state[hcode].lastactive = 0;
   x10state[hcode].xconfigmode = 0;
   for ( j = 0; j < NumStates; j++ )
      x10state[hcode].state[j] = 0;
   x10state[hcode].launched = 0;
//   x10state[hcode].statusflags = 0;
   x10state[hcode].rfxlobat = 0;
   x10state[hcode].longdata = 0;
   x10global.longdata_flags &= ~(1 << hcode);
   x10state[hcode].rcstemp = 0;
   x10global.rcstemp_flags &= ~(1 << hcode);
   for ( ucode = 0; ucode < 16; ucode++ ) {
      x10state[hcode].dimlevel[ucode] = 0;
      x10state[hcode].memlevel[ucode] = maxdimlevel[hcode][ucode];
      x10state[hcode].vident[ucode] = 0;
      x10state[hcode].timestamp[ucode] = 0;  /* Init timestamp */
      x10state[hcode].vflags[ucode] = 0;
      x10state[hcode].rfxmeter[ucode] = 0;
      x10state[hcode].grpmember[ucode] = 0;
      for ( group = 0; group < 4; group++ ) {
         x10state[hcode].grpaddr[ucode] = 0;
         x10state[hcode].grplevel[ucode][group] = 0;
      }
   }

   if ( (aliasp = configp->aliasp) == NULL )
      return;

   j = 0;
   while ( aliasp[j].line_no > 0 ) {
      if ( aliasp[j].housecode == hc ) {
         for ( k = 0; k < aliasp[j].storage_units; k++ ) {
            x10global.data_storage[aliasp[j].storage_index + k] = 0;
         }
      }
      j++;
   }

   reset_security_sensor_timeout(hcode, 0xffffu );

   return;
}

/*----------------------------------------------------------------------------+
 | Initialize the state structure elements for (X10 encoded) hcode            |
 +----------------------------------------------------------------------------*/
void x10state_init ( unsigned char hcode, unsigned int bitmap )
{
   void reset_security_sensor_timeout ( unsigned char, unsigned int );

   ALIAS *aliasp;
   int   j, k;
   int   ucode, group;
   char  hc;
   unsigned int globtamper = 0;

   hc = code2hc(hcode);

   bitmap &= 0xffffu;
   if ( bitmap == 0 )
      bitmap = 0xffffu;

   if ( bitmap == 0xffffu ) {
      x10state[hcode].reset = 0;
      x10state[hcode].lastunit = 0;
      x10state[hcode].lastcmd = 0xffu;
      x10state[hcode].xconfigmode = 0;
      x10state[hcode].longdata = 0;
      x10global.longdata_flags &= ~(1 << hcode);
      x10state[hcode].rcstemp = 0;
      x10global.rcstemp_flags &= ~(1 << hcode);
   }

   x10state[hcode].addressed &= ~bitmap;
   x10state[hcode].exclusive &= ~bitmap;
   x10state[hcode].sticky &= ~bitmap;
   x10state[hcode].vaddress &= ~bitmap;
   x10state[hcode].squelch &= ~bitmap;
   x10state[hcode].lastactive &= ~bitmap;
   x10state[hcode].launched &= ~bitmap;
   x10state[hcode].rfxlobat &= ~bitmap;
   for ( j = 0; j < NumStates; j++ ) {
      x10state[hcode].state[j] &= ~bitmap;
   }

   for ( ucode = 0; ucode < 16; ucode++ ) {
      if ( bitmap & (1 << ucode) ) {
         x10state[hcode].dimlevel[ucode] = 0;
         x10state[hcode].memlevel[ucode] = maxdimlevel[hcode][ucode];
         x10state[hcode].vident[ucode] = 0;
         x10state[hcode].timestamp[ucode] = 0;  /* Init timestamp */
         x10state[hcode].vflags[ucode] = 0;
         x10state[hcode].rfxmeter[ucode] = 0;
         x10state[hcode].grpmember[ucode] = 0;
         for ( group = 0; group < 4; group++ ) {
            x10state[hcode].grpaddr[ucode] = 0;
            x10state[hcode].grplevel[ucode][group] = 0;
         }
      }
   }

   aliasp = configp->aliasp;

   j = 0;
   while ( aliasp && aliasp[j].line_no > 0 ) {
      if ( aliasp[j].housecode == hc && (bitmap & aliasp[j].unitbmap) != 0 ) {
         for ( k = 0; k < aliasp[j].storage_units; k++ ) {
            x10global.data_storage[aliasp[j].storage_index + k] = 0;
         }
      }
      j++;
   }

   reset_security_sensor_timeout(hcode, bitmap);

   globtamper = 0;
   for ( j = 0; j < 16; j++ ) {
      globtamper |= x10state[j].state[TamperState];
   }
   if ( (globtamper & 0xffffu) == 0 ) {
      x10global.sflags &= ~GLOBSEC_TAMPER;
   }

   write_x10state_file();

   return;
}

/*----------------------------------------------------------------------------+
 | Initialize the state structure for all housecodes and global data except   |
 | security flags and countdown timer counts.                                 |
 +----------------------------------------------------------------------------*/
void x10state_init_all ( void )
{
   int hcode, j, k;

   for ( hcode = 0; hcode < 16; hcode++ ) 
      x10state_init(hcode, 0xffff);

   x10global.longvdata = 0;
   x10global.longvdata2 = 0;
   x10global.longvdata3 = 0;
   x10global.sigcount = 0;
   x10global.interval = 0;
   x10global.lastvtype = 0;
   x10global.lasthc = 0xff;
   x10global.lastaddr = 0;
   x10global.utc0_macrostamp = 0;
   x10global.hailstate = 0;
   x10global.sflags &=
      (GLOBSEC_ARMED | GLOBSEC_HOME | GLOBSEC_PENDING | GLOBSEC_TAMPER);
   x10global.vflags = 0;
   update_global_nightdark_flags(time(NULL));

   if ( x10global.timer_count[ArmDelayTimer] > 0 ) {
      x10global.timer_count[ArmDelayTimer] = 0;
      x10global.sflags &= ~(GLOBSEC_PENDING);
      x10global.sflags |= GLOBSEC_ARMED;
   }

   for ( j = 1; j <= NUM_USER_TIMERS; j++ ) {
      x10global.timer_count[j] = 0;
   }
   for ( j = 0; j < NUM_TIMER_BANKS; j++ ) {
      x10global.tzflags[j] = ~(0UL);
   }
   

   for ( j = 0; j < 8; j++ ) {
      x10global.rfxflags[j] = 0;
      for ( k = 0; k < 3; k++ ) {
         x10global.rfxdata[j][k] = 0;
      }
   }
   for ( j = 0; j < NUM_FLAG_BANKS; j++ ) {
      x10global.flags[j] = 0;
   }
   for ( j = 0; j < (32 * NUM_COUNTER_BANKS); j++ ) {
      x10global.counter[j] = 0;
   }
   for ( j = 0; j < NUM_COUNTER_BANKS; j++ )
      x10global.czflags[j] = ~(0);

   for ( j = 0; j < MAXDATASTORAGE; j++ )
      x10global.data_storage[j] = 0;

   write_x10state_file();

   return;
}

/*----------------------------------------------------------------------------+
 | Initialize the cumulative address table for all housecodes.                |
 +----------------------------------------------------------------------------*/
void x10state_init_others ( void )
{
   int hcode;

   for ( hcode = 0; hcode < 16; hcode++ ) 
      x10state[hcode].sticky = 0;

   return;
}

/*---------------------------------------------------------------------+
 | Convert Celsius temperature to scaled temperature.                  |
 +---------------------------------------------------------------------*/
double celsius2temp ( double tempc, char tscale, double toffset )
{

   double temperature;

   switch ( tscale ) {
      case 'F' :
         /* Fahrenheit */
         temperature = 1.8 * tempc + 32.0;
         break;
      case 'R' :
         /* Rankine */
         temperature = 1.8 * tempc + 32.0 + 459.67;
         break;
      case 'K' :
         /* Kelvin */
         temperature = tempc + 273.15;
         break;
      default :
         /* Celsius */
         temperature = tempc;
         break;
   }
   temperature += toffset;

   return temperature;
}

/*---------------------------------------------------------------------+
 | Convert scaled temperature to Celsius.                              |
 +---------------------------------------------------------------------*/
double temp2celsius ( double temperature, char tscale, double toffset )
{

   double tempc;

   temperature -= toffset;   

   switch ( tscale ) {
      case 'F' :
         /* Input is Fahrenheit */
         tempc = (temperature - 32.0) / 1.8;
         break;
      case 'R' :
         /* Input is Rankine */
         tempc = (temperature - 32.0 - 459.67) / 1.8;
         break;
      case 'K' :
         /* Input is Kelvin */
         tempc = temperature - 273.15;
         break;
      default :
         /* Input is Celsius */
         tempc = temperature;
         break;
   }

   return tempc;
}

/*----------------------------------------------------------------------------+
 | Elapsed time string from last timestamp in hours:minutes:seconds           |
 +----------------------------------------------------------------------------*/
void intvstrfunc ( char *intvstr, long *intv, time_t timenow, time_t timeprev )
{
   unsigned long intvu, hours, minutes, seconds;

   if ( timeprev == 0 || timenow < timeprev ) {
      sprintf(intvstr, "----");
      *intv = -1;
      return;
   }

   intvu = (unsigned long)(timenow - timeprev);
   *intv = intvu;
   hours = intvu / 3600u;
   intvu = intvu % 3600u;
   minutes = intvu / 60u;
   seconds = intvu % 60u;

   sprintf(intvstr, "%lu:%02lu:%02lu", hours, minutes, seconds );

   return;
}
   
/*---------------------------------------------------------------------+
 | Initialize the arm delay timer with the countdown time in seconds.  |
 +---------------------------------------------------------------------*/
void set_arm_delay_timer_countdown ( long seconds )
{
   x10global.timer_count[ArmDelayTimer] = seconds;

   return;
}

/*----------------------------------------------------------------------------+
 | Set the security flags represented by the argument (which has been shifted |
 | to fit into the size of the argument).                                     |
 | With this version, an Armed or Pending state cannot be changed unless      |
 | first Disarmed.                                                            |
 +----------------------------------------------------------------------------*/
int set_globsec_flags_strict ( unsigned char sflags )
{
   unsigned long lflags;
   int           retcode = 1;

   lflags = ((unsigned long)sflags << GLOBSEC_SHIFT) &
                (GLOBSEC_ARMED | GLOBSEC_HOME | GLOBSEC_PENDING);

   if ( lflags == 0 ) {
      /* Disarm */
      x10global.sflags &= ~(GLOBSEC_ARMED | GLOBSEC_HOME | GLOBSEC_PENDING);
      set_arm_delay_timer_countdown(0);
      x10global.timer_count[ArmDelayTimer] = 0;
      retcode = 0;
   }
   else if ( !(x10global.sflags & (GLOBSEC_ARMED | GLOBSEC_HOME | GLOBSEC_PENDING)) ) {
      /* Arm */
      x10global.sflags |= lflags;
      if ( x10global.sflags & GLOBSEC_PENDING ) {
         set_arm_delay_timer_countdown(config.arm_max_delay);
         if ( (x10global.timer_count[ArmDelayTimer] = config.arm_max_delay) == 0 ) {
            x10global.sflags |= GLOBSEC_ARMED;
            x10global.sflags &= ~GLOBSEC_PENDING;
         }
      }
      retcode = 0;
   }

   return retcode;
}


/*----------------------------------------------------------------------------+
 | Set the security flags represented by the argument (which has been shifted |
 | to fit into the size of the argument).                                     |
 | Allowed changes:                                                           |
 |    Disarmed --> Armed or Pending                                           |
 |    Armed    --> Disarmed                                                   |
 |    Pending  --> Armed or Disarmed                                          |
 |    Home    <--> Away                                                       |
 +----------------------------------------------------------------------------*/
int set_globsec_flags_medium ( unsigned char sflags )
{
   unsigned long lflags, gflags;

   lflags = ((unsigned long)sflags << GLOBSEC_SHIFT) &
                       (GLOBSEC_ARMED | GLOBSEC_HOME | GLOBSEC_PENDING);
   gflags = x10global.sflags & (GLOBSEC_ARMED | GLOBSEC_HOME | GLOBSEC_PENDING);

   if ( lflags == gflags ) {
      return 0;
   }

   if ( lflags == 0 ) {
      /* Disarm */
      x10global.sflags &= ~(GLOBSEC_ARMED | GLOBSEC_HOME | GLOBSEC_PENDING);
      set_arm_delay_timer_countdown(0);
      x10global.timer_count[ArmDelayTimer] = 0;
      return 0;
   }

   if ( gflags == 0 ) {
      /* Arm or Pending */
      x10global.sflags |= lflags;
      if ( x10global.sflags & GLOBSEC_PENDING ) {
         set_arm_delay_timer_countdown(config.arm_max_delay);
         if ( (x10global.timer_count[ArmDelayTimer] = config.arm_max_delay) == 0 ) {
            x10global.sflags |= GLOBSEC_ARMED;
            x10global.sflags &= ~GLOBSEC_PENDING;
         }
      }
      return 0;
   }

   if ( lflags & GLOBSEC_ARMED ||
        (gflags & GLOBSEC_ARMED && lflags & GLOBSEC_PENDING) ) {
      /* Arm */
      x10global.sflags &= ~GLOBSEC_PENDING;
      x10global.sflags |= GLOBSEC_ARMED;
      set_arm_delay_timer_countdown(0);
      x10global.timer_count[ArmDelayTimer] = 0;
      if ( (lflags & GLOBSEC_HOME) != (gflags & GLOBSEC_HOME) ) {
         /* Reverse Home or Away */
         x10global.sflags &= ~GLOBSEC_HOME;
         x10global.sflags |= (lflags & GLOBSEC_HOME);
      }
      return 0;
   }

   return 1;
}


/*----------------------------------------------------------------------------+
 | Set the security flags represented by the argument (which has been shifted |
 | to fit into the size of the argument).                                     |
 | Allowed transitions:  Any                                                  |
 +----------------------------------------------------------------------------*/
int set_globsec_flags_loose ( unsigned char sflags )
{
   x10global.sflags &= ~(GLOBSEC_ARMED | GLOBSEC_HOME | GLOBSEC_PENDING);

   x10global.sflags |= ((unsigned long)sflags << GLOBSEC_SHIFT) & 
                               (GLOBSEC_ARMED | GLOBSEC_HOME | GLOBSEC_PENDING);

   if ( x10global.sflags & GLOBSEC_PENDING ) {
      set_arm_delay_timer_countdown(config.arm_max_delay);
      if ( (x10global.timer_count[ArmDelayTimer] = config.arm_max_delay) == 0 ) {
         x10global.sflags |= GLOBSEC_ARMED;
         x10global.sflags &= ~GLOBSEC_PENDING;
      }
   }
   else {
      set_arm_delay_timer_countdown(0);
      x10global.timer_count[ArmDelayTimer] = 0;
   }

   return 0;
}

int set_globsec_flags ( unsigned char sflags ) 
{
   return
     (config.arm_logic == ArmLogicStrict) ? set_globsec_flags_strict(sflags) :
     (config.arm_logic == ArmLogicMedium) ? set_globsec_flags_medium(sflags) :
     (config.arm_logic == ArmLogicLoose)  ? set_globsec_flags_loose(sflags)  : 0;
}

int set_seclights_flag ( unsigned char sflags )
{
   if ( sflags )
      x10global.sflags |= GLOBSEC_SLIGHTS;
   else
      x10global.sflags &= ~GLOBSEC_SLIGHTS;
   return 0;
}

int set_tamper_flag ( unsigned char sflags )
{
   if ( sflags )
      x10global.sflags |= GLOBSEC_TAMPER;
   else
      x10global.sflags &= ~GLOBSEC_TAMPER;
   return 0;
}

/*----------------------------------------------------------------------------+
 | Display the Armed/Disarmed state of the system - c_show2()                 |
 +----------------------------------------------------------------------------*/
void show_armed_status ( void )
{
   char *tflag;

   tflag = (x10global.sflags & GLOBSEC_TAMPER) ? "*TAMPER*, " : "";

   if ( x10global.sflags & GLOBSEC_PENDING ) {
      printf("%sSystem will be Armed %s in %ld seconds\n", tflag,
          ((x10global.sflags & GLOBSEC_HOME) ? "Home" : "Away"),
          x10global.timer_count[ArmDelayTimer]);
   }
   else if ( (x10global.sflags & GLOBSEC_ARMED) == 0 ) {
      printf("%sSystem is Disarmed\n", tflag);
   }
   else {
      printf("%sSystem is Armed %s\n", tflag,
         ((x10global.sflags & GLOBSEC_HOME) ? "Home" : "Away") );
   }   

   return;
}


/*----------------------------------------------------------------------------+
 | Display the Armed/Disarmed state of the system (poll.c)                    |
 +----------------------------------------------------------------------------*/
char *display_armed_status ( void )
{
   static char outbuf[120];

   if ( x10global.sflags & GLOBSEC_PENDING ) {
      sprintf(outbuf, "%sSystem will be Armed %s in %ld seconds.",
          ((x10global.sflags & GLOBSEC_TAMPER) ? "*TAMPER*, " : ""),
          ((x10global.sflags & GLOBSEC_HOME) ? "Home" : "Away"),
          x10global.timer_count[ArmDelayTimer]);
      return outbuf;
   }

   *outbuf = '\0';

   if ( x10global.sflags & GLOBSEC_TAMPER )
      strcat(outbuf, "*TAMPER*, ");

   if ( (x10global.sflags & GLOBSEC_ARMED) == 0 ) {
      strcat(outbuf, "System is Disarmed");
   }
   else {
      strcat(outbuf, "System is Armed ");
      strcat(outbuf, ((x10global.sflags & GLOBSEC_HOME) ? "Home" : "Away"));
   }   

   return outbuf;
}


/*----------------------------------------------------------------------------+
 | Send a powerfail message to the heyu monitor/state engine                  |
 +----------------------------------------------------------------------------*/
void send_pfail_msg ( int spptty, unsigned char command, unsigned char parm )
{
   static unsigned char template[7] = {
     0xff,0xff,0xff,3,ST_COMMAND,0,0};

   int ignoret;

   template[5] = command;
   template[6] = parm;

   ignoret = write(spptty, template, 7);
   return;
}


/*----------------------------------------------------------------------------+
 | Internal @vdata and @vdatam commands                                       |
 +----------------------------------------------------------------------------*/
void engine_internal_vdata ( unsigned char hcode, unsigned char ucode,
                             unsigned char vdata, unsigned char vtype )
{
   unsigned int bitmap, changestate, startupstate;
   char         hc;
   unsigned char func;

   bitmap = 1 << ucode;
   hc = code2hc(hcode);

//   x10state[hcode].state[ChgState] = 0;
   changestate = 0;
   if ( vtype == RF_VDATAM ) {
      func = VdataMFunc;
      if ( vdata != x10state[hcode].memlevel[ucode] )
//         x10state[hcode].state[ChgState] |= bitmap;
         changestate |= bitmap;
      x10state[hcode].memlevel[ucode] = vdata;
   }
   else {
      func = VdataFunc;
      if ( vdata != x10state[hcode].dimlevel[ucode] )
//         x10state[hcode].state[ChgState] |= bitmap;
         changestate |= bitmap;
      x10state[hcode].dimlevel[ucode] = vdata;
   }
   x10state[hcode].lastcmd = func;
   x10state[hcode].vaddress = bitmap;
   x10state[hcode].lastunit = code2unit(ucode);
   x10state[hcode].vident[ucode] = 0;
   x10state[hcode].vflags[ucode] = 0;
   x10state[hcode].timestamp[ucode] = time(NULL);
//   x10state[hcode].state[ValidState] |= (1 << ucode);
   x10global.lasthc = hcode;
   x10global.lastaddr = 0;

   startupstate = ~x10state[hcode].state[ValidState] & bitmap;
   x10state[hcode].state[ValidState] |= bitmap;

//   x10state[hcode].state[ModChgState] = (x10state[hcode].state[ModChgState] & ~bitmap) | changestate;
   x10state[hcode].state[ModChgState] = changestate;
   x10state[hcode].state[ChgState] = x10state[hcode].state[ModChgState] |
      (x10state[hcode].state[ActiveChgState] & bitmap) |
      (startupstate & ~modmask[PhysMask][hcode]);

   if ( i_am_state ) {
      fprintf(fdsout, "%s syst func %12s : hu %c%-2d vdata 0x%02x (%s)\n",
         datstrf(), funclabel[func], hc, code2unit(ucode), vdata, lookup_label(hc, bitmap));
   }

   return;
}

#if 0
/*----------------------------------------------------------------------------+
 | Internal @vdata command                                                    |
 +----------------------------------------------------------------------------*/
void engine_internal_vdata_old ( unsigned char hcode, unsigned char ucode, unsigned char vdata )
{
   unsigned int bitmap;
   char         hc;

   bitmap = 1 << ucode;
   hc = code2hc(hcode);

   x10state[hcode].vaddress = bitmap;
   x10state[hcode].lastcmd = VdataFunc;
   x10state[hcode].lastunit = code2unit(ucode);
   x10state[hcode].vident[ucode] = 0;
   x10state[hcode].vflags[ucode] = 0;
   x10state[hcode].timestamp[ucode] = time(NULL);
   x10state[hcode].state[ValidState] |= (1 << ucode);
   x10state[hcode].state[ChgState] = 0;
   x10global.lasthc = hcode;
   x10global.lastaddr = 0;
   if ( vdata != x10state[hcode].dimlevel[ucode] )
      x10state[hcode].state[ChgState] |= bitmap;
   x10state[hcode].dimlevel[ucode] = vdata;
   if ( i_am_state ) {
      fprintf(fdsout, "%s syst func %12s : hu %c%-2d vdata 0x%02x (%s)\n",
         datstrf(), funclabel[VdataFunc], hc, code2unit(ucode), vdata, lookup_label(hc, bitmap));
   }

   return;
}
#endif

/*----------------------------------------------------------------------------+
 | Send command line launch request to the Heyu state engine                  |
 | Mode 0x00 - Check all launchers for script                                 |
 | Mode 0x01 - Check only specified launcher for script                       |
 | Mode 0x80 - Exec mode (flags only)                                         |
 +----------------------------------------------------------------------------*/
void send_launch_request ( unsigned char mode, int scriptnum, int launchernum )
{
   static unsigned char template[11] = {
    0xff, 0xff, 0xff, 7, ST_COMMAND, ST_SCRIPT, 0, 0,0, 0,0};

   int ignoret;

   template[6] = mode;
   template[7] = scriptnum & 0x00ff;
   template[8] = (scriptnum & 0xff00) >> 8;
   template[9] = launchernum & 0x00ff;
   template[10] = (launchernum & 0xff00) >> 8; 

   ignoret = write(sptty, template, sizeof(template));
   return;
}

/*----------------------------------------------------------------------------+
 | Send a binary buffer to the monitor/logfile for display                    | 
 +----------------------------------------------------------------------------*/
void send_showbuffer ( unsigned char *buffer, unsigned char bufflen )
{
   unsigned char outbuff[80];
   int ignoret;
   static unsigned char template[7] = {
     0xff, 0xff, 0xff, 0, ST_COMMAND, ST_SHOWBUFFER, 0};

   template[6] = bufflen;
   template[3] = bufflen + 3;
   memcpy(outbuff, template, sizeof(template));
   memcpy(outbuff + sizeof(template), buffer, bufflen);

   ignoret = write(sptty, outbuff, sizeof(template) + bufflen);

   return;
}

/*----------------------------------------------------------------------------+
 | Debugging tool - Displey a binary buffer in the monitor/logfile            |
 | buff[0] is length, buff + 1 = data                                         | 
 +----------------------------------------------------------------------------*/
char *display_binbuffer ( unsigned char *buff )
{
   int j;
   static char outbuff[120];

   strcpy(outbuff, "Buffer =");
   for ( j = 0; j < buff[0]; j++ ) {
      sprintf(outbuff + strlen(outbuff), " %02x", buff[1 + j]);
   }

   return outbuff;
}


/*----------------------------------------------------------------------------+
 | Send virtual data to the heyu monitor/state engine                         |
 +----------------------------------------------------------------------------*/
void send_virtual_data ( unsigned char address, unsigned char vdata,
     unsigned char vtype, unsigned char vidlo, unsigned char vidhi, unsigned char byte2, unsigned char byte3 )
{
   static unsigned char template[20] = {
    0xff,0xff,0xff,3,ST_COMMAND,ST_SOURCE,0,
    0xff,0xff,0xff,9,ST_COMMAND,ST_VDATA,0,0,0,0,0,0,0};

   int ignoret;

   template[6] = (unsigned char)heyu_parent;

   template[13] = address;
   template[14] = vdata;
   template[15] = vtype;
   template[16] = vidlo;
   template[17] = vidhi;
   template[18] = byte2;
   template[19] = byte3;

   ignoret = write(sptty, template, sizeof(template));
   return;
}

/*----------------------------------------------------------------------------+
 | Send a state command to the heyu monitor/state engine                      |
 +----------------------------------------------------------------------------*/
void send_sptty_x10state_command ( int spptty, unsigned char command, unsigned char parm )
{
   static unsigned char template[7] = {
    0xff,0xff,0xff,3,ST_COMMAND,0,0};

   int ignoret = 0;

   template[5] = command;
   template[6] = parm;

   ignoret = write(spptty, template, sizeof(template));

   return;
}


/*----------------------------------------------------------------------------+
 | Send a state command to the heyu monitor/state engine                      |
 +----------------------------------------------------------------------------*/
void send_x10state_command ( unsigned char command, unsigned char parm )
{
   extern int sptty;

   send_sptty_x10state_command(sptty, command, parm);

   return;
}

/*----------------------------------------------------------------------------+
 | Send an initstate command to the heyu monitor/state engine                 |
 +----------------------------------------------------------------------------*/
void send_initstate_command ( unsigned char hcode, unsigned int bitmap )
{
   extern int sptty;
   static unsigned char template[9] = {
    0xff, 0xff, 0xff, 5, ST_COMMAND, ST_INIT, 0, 0,0};
   int ignoret = 0;

   template[6] = hcode;
   template[7] = bitmap & 0x00ff;
   template[8] = (bitmap & 0xff00) >> 8;

   ignoret = write(sptty, template, sizeof(template));

   return;
}

/*----------------------------------------------------------------------------+
 | Translate an X10-encoded bitmap to a linear unit bitmap, i.e., where       |
 | unit 1 = 1, unit 2 = 2, unit 3 = 4, unit 4 = 8, etc.                       |
 +----------------------------------------------------------------------------*/
unsigned int x10map2linmap ( unsigned int x10map )
{
   int j;
   unsigned int mask, linmap;
   static unsigned char bitshift[] =
      {12,4,2,10,14,6,0,8,13,5,3,11,15,7,1,9};

   mask = 1;
   linmap = 0;
   for ( j = 0; j < 16; j++ ) {
     if ( x10map & mask )
        linmap |= (1 << bitshift[j]);
     mask <<= 1;
   }
   return linmap;
}   

/*----------------------------------------------------------------------------+
 | Display a text message in the heyu monitor/state engine logfile            |
 | Maximum length 80 characters.                                              |
 +----------------------------------------------------------------------------*/
int display_x10state_message ( char *message )
{
   extern int    sptty;
   int           size;
   unsigned char buf[88];

   int   ignoret;

   static unsigned char template[7] = {
     0xff,0xff,0xff,0,ST_COMMAND,ST_MSG,0};

   if ( (size = strlen(message)) > (int)(sizeof(buf) - 8) ) {
      fprintf(stderr,
        "Log message exceeds %d characters\n", (int)(sizeof(buf) - 8));
      return 1;
   }
   
   memcpy(buf, template, sizeof(template));
   memcpy(buf + sizeof(template), message, size + 1);

   buf[6] = size + 1;
   buf[3] = size + 1 + 3;

   ignoret = write(sptty, buf, size + 8);

   return 0;
}

/*----------------------------------------------------------------------------+
 | Display a text message in the heyu monitor/state engine logfile            |
 | Maximum length 80 characters.                                              |
 +----------------------------------------------------------------------------*/
int c_logmsg ( int argc, char *argv[] )
{
   if ( argc < 3 ) {
      fprintf(stderr, "%s - Too few arguments\n", argv[1]);
      return 1;
   }
   else if ( argc > 3 ) {
      fprintf(stderr, "%s - Too many arguments\n", argv[1]);
      return 1;
   }

   return display_x10state_message(argv[2]);
}   

/*----------------------------------------------------------------------------+
 | Display the raw data in argument buff + 4 as hex bytes.                    |
 | buff[2] has the type code, buff[3] has the length of the data.             |
 | (This function is used in poll.c to display a Raw or Noise packet from     |
 | heyu_aux in the log file and/or monitor.)                                  |
 | Maximum length 120 characters.                                             |
 +----------------------------------------------------------------------------*/
char *display_variable_aux_data ( unsigned char *buff )
{
   static char outbuf[120];
   int         j;

   if ( buff[2] == RF_NOISEVL ) {
      sprintf(outbuf, "func %12s : Data (hex)", "RFnoise");
   }
   else if ( buff[2] == RF_RAWVL ) {
      sprintf(outbuf, "func %12s : Data (hex)", "RFrawdata");
   }
   else {
      sprintf(outbuf, "func %12s : Data (hex)", "RFunknown");
   }

   for ( j = 0; j < buff[3]; j++ ) {
      snprintf(outbuf + strlen(outbuf), sizeof(outbuf) - strlen(outbuf) - 1,
              " %02x", buff[j + 4]);
   }

   return outbuf;
}





/*----------------------------------------------------------------------------+
 | Send an arbitrary state message to the engine.  The first parameter must   |
 | be the ST_XXX value (in hex), then followed by the hex bytes to be sent.   |
 +----------------------------------------------------------------------------*/
int c_sendarbst ( int argc, char *argv[] )
{
   unsigned char buf[80];
   long hex;
   int j;
   char *sp;

   int ignoret;

   if ( argc < 4 ) {
      fprintf(stderr, "%s - Too few arguments\n", argv[1]);
      return 1;
   }

   hex = strtol(argv[2], &sp, 16);
   if ( !strchr(" /r/n", *sp) || hex < 0 || hex > 0xff ) {
      fprintf(stderr, "Invalid ST_xxxx value '%s'\n", argv[2]);
      return 1;
   }

   buf[0] = 0xff; buf[1] = 0xff; buf[2] = 0xff;
   buf[3] = argc - 1;
   buf[4] = ST_COMMAND;
   buf[5] = (unsigned char)hex;

   for ( j = 3; j < argc; j++ ) {
      hex = strtol(argv[j], &sp, 16);
      if ( !strchr(" /r/n", *sp) || hex < 0 || hex > 0xff ) {
         fprintf(stderr, "Invalid hex byte value '%s'\n", argv[j]);
         return 1;
      }
      buf[j + 3] = (unsigned char)hex;
   }
         
   ignoret = write(sptty, buf, argc + 3);

   return 0;
}   
     
/*----------------------------------------------------------------------------+
 | Check the dims table to determine the onstate and dimstate of modules on   |
 | argument housecode, returning X10 bitmaps back through the argument list.  |
 +----------------------------------------------------------------------------*/
void get_states ( unsigned char hcode, unsigned int *onstate, unsigned int *dimstate )
{
   int           ucode;
   unsigned char level;
   unsigned int  mask, offstate, ex3mask, premask, stdmask, ex0mask, kakumask, vdatamask;

   ex3mask  = modmask[Ext3Mask][hcode];
   premask  = modmask[PresetMask][hcode];
   stdmask  = modmask[StdMask][hcode];
   ex0mask  = modmask[Ext0Mask][hcode];
   vdatamask = modmask[VdataMask][hcode];
   kakumask = kmodmask[KpreMask][hcode] | kmodmask[KGpreMask][hcode];

   offstate = 0;
   *dimstate = 0;
   for ( ucode = 0; ucode < 16; ucode++ ) {
      mask = (1 << ucode);
      level = x10state[hcode].dimlevel[ucode];
      if ( level == 0 )
         offstate |= mask;
      else if ( (mask & ex3mask  && level < 62)  ||
                (mask & ex0mask  && level < 25)  ||
                (mask & premask  && level < 31)  ||
                (mask & stdmask  && level < 210) ||
                (mask & kakumask && level < 15)  ) {
         *dimstate |= mask;
      }
      else if ( !(mask & (ex3mask | ex0mask | premask | stdmask | kakumask)) &&
		      level < ondimlevel[hcode][ucode] )
	 *dimstate |= mask;
   }
   *onstate = ~offstate & ~vdatamask;
   *dimstate &= ~vdatamask;

   return;
}
      
/*----------------------------------------------------------------------------+
 | Determine the dimstate of modules on argument housecode, returning an X10  |
 | bitmap of units which are dimmed.                                          |
 +----------------------------------------------------------------------------*/
unsigned int get_dimstate ( unsigned char hcode )
{
   int ucode;
   unsigned int bitmap, mask, dimstate = 0;

   /* Check Extended Preset Type 3 Dim Units */
   bitmap = modmask[Ext3DimMask][hcode];
   if ( bitmap ) {
      mask = 1;
      for ( ucode = 0; ucode < 16; ucode++ ) {
         if ( bitmap & mask &&
              x10state[hcode].dimlevel[ucode] > 0 &&
              x10state[hcode].dimlevel[ucode] < 62  ) {
            dimstate |= mask;
         }
         mask = mask << 1;
      }
   }

   /* Check Extended Preset Type 0 Dim Units */
   bitmap = modmask[Ext0Mask][hcode];
   if ( bitmap ) {
      mask = 1;
      for ( ucode = 0; ucode < 16; ucode++ ) {
         if ( bitmap & mask &&
              x10state[hcode].dimlevel[ucode] > 0 &&
              x10state[hcode].dimlevel[ucode] < 25  ) {
            dimstate |= mask;
         }
         mask = mask << 1;
      }
   }

   /* Check Preset Units */
   bitmap = modmask[PresetMask][hcode];
   if ( bitmap ) {
      mask = 1;
      for ( ucode = 0; ucode < 16; ucode++ ) {
         if ( bitmap & mask &&
              x10state[hcode].dimlevel[ucode] > 0 &&
              x10state[hcode].dimlevel[ucode] < 31  ) {
            dimstate |= mask;
         }
         mask = mask << 1;
      }
   }

   /* Check KAKU Units */
   bitmap = kmodmask[KpreMask][hcode] | kmodmask[KGpreMask][hcode];
   if ( bitmap ) {
      mask = 1;
      for ( ucode = 0; ucode < 16; ucode++ ) {
         if ( bitmap & mask &&
              x10state[hcode].dimlevel[ucode] > 0 &&
              x10state[hcode].dimlevel[ucode] < 15  ) {
            dimstate |= mask;
         }
         mask = mask << 1;
      }
   }

   /* Check Standard Non-Preset Units */
   bitmap = (modmask[DimMask][hcode] | modmask[BriMask][hcode]) &
            ~modmask[Ext3DimMask][hcode] & ~modmask[PresetMask][hcode];
   if ( bitmap ) {
      mask = 1;
      for ( ucode = 0; ucode < 16; ucode++ ) {
         if ( bitmap & mask &&
              x10state[hcode].dimlevel[ucode] > 0 &&
              x10state[hcode].dimlevel[ucode] < 210  ) {
            dimstate |= mask;
         }
         mask = mask << 1;
      }
   }

   return dimstate;
}


/*----------------------------------------------------------------------------+
 | Copy current dimlevel > 0 (0 is the same as OFF) to memlevel for bitmap    |
 | units.
 | This is only valid for units which support the preset or extended preset   |
 | commands.                                                                  |
 +----------------------------------------------------------------------------*/
void save_dimlevel ( unsigned char hcode, unsigned int bitmap )
{
   int ucode;
   int level;
   unsigned int mask = 1;

   for ( ucode = 0; ucode < 16; ucode++ ) {
      if ( bitmap & mask ) {
         level = x10state[hcode].dimlevel[ucode];
         if ( level > 0 )
           x10state[hcode].memlevel[ucode] = level;
      }
      mask = mask << 1;
   }
   return;
}

/*----------------------------------------------------------------------------+
 | Restore current dimlevel from memlevel for bitmap units                    |
 | This is only valid for units which support the preset or extended preset   |
 | commands.                                                                  |
 +----------------------------------------------------------------------------*/
void restore_dimlevel ( unsigned char hcode, unsigned int bitmap )
{
   int ucode;
   int level;
   unsigned int mask = 1;

   for ( ucode = 0; ucode < 16; ucode++ ) {
      if ( bitmap & mask ) {
         level = x10state[hcode].memlevel[ucode];
         x10state[hcode].dimlevel[ucode] = level;
      }
      mask = mask << 1;
   }
   return;
}


/*----------------------------------------------------------------------------+
 | Set a specific raw dimlevel 0-255 (limited to native level range).         |
 +----------------------------------------------------------------------------*/
void set_raw_level ( int rawlevel, unsigned char hcode, unsigned int bitmap )
{
   int ucode;
   unsigned int mask = 1;
   unsigned char level;

   for ( ucode = 0; ucode < 16; ucode++ ) {
      if ( bitmap & mask ) {
         level = max(0, rawlevel);
         level = min(level, maxdimlevel[hcode][ucode]);
         x10state[hcode].dimlevel[ucode] = level;
      }
      mask = mask << 1;
   }
   return;
}

/*----------------------------------------------------------------------------+
 | Set a specific dimlevel.                                                   |
 +----------------------------------------------------------------------------*/
void set_dimlevel ( int level , unsigned char hcode, unsigned int bitmap )
{
   int ucode;
   unsigned int mask = 1;

   for ( ucode = 0; ucode < 16; ucode++ ) {
      if ( bitmap & mask ) {
         x10state[hcode].dimlevel[ucode] = level;
      }
      mask = mask << 1;
   }
   return;
}

/*----------------------------------------------------------------------------+
 | Set a specific memlevel.                                                   |
 +----------------------------------------------------------------------------*/
void set_memlevel ( int level , unsigned char hcode, unsigned int bitmap )
{
   int ucode;
   unsigned int mask = 1;

   for ( ucode = 0; ucode < 16; ucode++ ) {
      if ( bitmap & mask ) {
         x10state[hcode].memlevel[ucode] = level;
      }
      mask = mask << 1;
   }
   return;
}

/*----------------------------------------------------------------------------+
 | Set the ident for virtual modules                                          |
 +----------------------------------------------------------------------------*/
void set_ident ( unsigned short vident , unsigned char hcode, unsigned int bitmap )
{
   int ucode;
   unsigned int mask = 1;

   for ( ucode = 0; ucode < 16; ucode++ ) {
      if ( bitmap & mask ) {
         x10state[hcode].vident[ucode] = vident;
      }
      mask = mask << 1;
   }
   return;
}

   
/*----------------------------------------------------------------------------+
 | Set dimlevels to '0n' level.                                               |
 +----------------------------------------------------------------------------*/
void set_max_level ( unsigned char hcode, unsigned int bitmap )
{
   int           ucode;

   if ( !bitmap )
      return;

   for ( ucode = 0; ucode < 16; ucode++ ) {
      if ( bitmap & (1 << ucode) ) {
         x10state[hcode].dimlevel[ucode] = maxdimlevel[hcode][ucode];
      }
   }

   return;
}

/*----------------------------------------------------------------------------+
 | Set memlevels to '0n' level.                                               |
 +----------------------------------------------------------------------------*/
void set_max_memlevel ( unsigned char hcode, unsigned int bitmap )
{
   int           ucode;

   if ( !bitmap )
      return;

   for ( ucode = 0; ucode < 16; ucode++ ) {
      if ( bitmap & (1 << ucode) ) {
         x10state[hcode].memlevel[ucode] = maxdimlevel[hcode][ucode];
      }
   }

   return;
}

/*----------------------------------------------------------------------------+
 | Set dimlevels to '0n' level.                                               |
 +----------------------------------------------------------------------------*/
void set_on_level ( unsigned char hcode, unsigned int bitmap )
{
   int           ucode;

   if ( !bitmap )
      return;

   for ( ucode = 0; ucode < 16; ucode++ ) {
      if ( bitmap & (1 << ucode) ) {
         x10state[hcode].dimlevel[ucode] = ondimlevel[hcode][ucode];
      }
   }

   return;
}

/*----------------------------------------------------------------------------+
 | Set a specific dimlevel no higher than that stored in memlevel.            |
 +----------------------------------------------------------------------------*/
void set_limit_dimlevel ( int level , unsigned char hcode, unsigned int bitmap )
{
   int ucode;
   unsigned int mask = 1;

   for ( ucode = 0; ucode < 16; ucode++ ) {
      if ( bitmap & mask ) {
         x10state[hcode].dimlevel[ucode] =
            min(level, (int)x10state[hcode].memlevel[ucode]);
      }
      mask = mask << 1;
   }
   return;
}

/*----------------------------------------------------------------------------+
 | Update the stored dimlevel by the amount of the argument level 0-210,      |
 | positive for brightens, negative for dims.                                 |
 +----------------------------------------------------------------------------*/
void update_dimlevel ( int level , unsigned char hcode, unsigned int bitmap )
{
   int          ucode, newlevel, abslev, table, delta;
   unsigned int active, mask;

   /* Table used in conversion of dims to change in ext3 preset level */
   /* Table[0] is used for Base-2 dims, table[1] for others */
   static int ext3table[2][17] = {
      {0, 5, 9, 14, 18, 22, 27, 31, 36, 40, 44, 49, 53, 58, 61, 62, 62},
      {0, 4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 44, 48, 52, 56, 60, 62},
   };

   /* Table used in conversion of dims to change in old-style Preset level */
   /* Table [0] is brights from level 0, [1] is dims from level 31.        */
   static int presettable[2][15] = {
      {1, 1, 3, 6, 8, 11, 14, 16, 19, 22, 24, 27, 29, 30, 32},
      {1, 1, 4, 7, 9, 12, 15, 17, 20, 22, 25, 28, 30, 32, 32},
   };

   if ( !bitmap )
      return;

   /* Standard Dimmable Units */
   active = bitmap & (modmask[DimMask][hcode] | modmask[BriMask][hcode]) &
                     modmask[StdMask][hcode] ;
   if ( active ) {
      mask = 1;
      for ( ucode = 0; ucode < 16; ucode++ ) {
         if ( active & mask ) {
            newlevel = level + x10state[hcode].dimlevel[ucode];
            newlevel = min(210, newlevel);
            newlevel = max(newlevel, (mask & modmask[TargMask][hcode]) ? 0 : 3);
            x10state[hcode].dimlevel[ucode] = newlevel;
         }
         mask = mask << 1;
      }
   }

   /* Ext3 Units */
   active = bitmap & modmask[Ext3Mask][hcode];
   if ( active ) {
      abslev = abs(level);
      table = (abslev % 11 == 2) ? 0 : 1;
      delta = ext3table[table][min(abslev/11, 16)];
      delta = (level < 0) ? -delta : delta;
      mask = 1;
      for ( ucode = 0; ucode < 16; ucode++ ) {
         if ( active & mask ) {
            newlevel = delta + x10state[hcode].dimlevel[ucode];
            newlevel = min(62, max(1, newlevel));
            x10state[hcode].dimlevel[ucode] = newlevel;
         }
         mask = mask << 1;
      }
   }

   /* Ext0 (Shutter) Units */
   active = bitmap & modmask[Ext0Mask][hcode];
   if ( active ) {
      delta = level * 25 / 210 ;
      mask = 1;
      for ( ucode = 0; ucode < 16; ucode++ ) {
         if ( active & mask ) {
            newlevel = delta + x10state[hcode].dimlevel[ucode];
            newlevel = min(25, max(0, newlevel));
            x10state[hcode].dimlevel[ucode] = newlevel;
         }
         mask = mask << 1;
      }
   }

   /* Preset1/Preset2 Units, zero-base levels 0-31 */
   active = bitmap & modmask[PresetMask][hcode];
   if ( active ) {
      abslev = abs(level);
      delta = (level > 0) ? presettable[0][min(abslev/11, 14)] :
                           -presettable[1][min(abslev/11, 14)];
      mask = 1;
      for ( ucode = 0; ucode < 16; ucode++ ) {
         if ( active & mask ) {
            newlevel = delta + x10state[hcode].dimlevel[ucode];
            newlevel = min(31, max(0, newlevel));
            x10state[hcode].dimlevel[ucode] = newlevel;
         }
         mask = mask << 1;
      }
   }

   /* Virtual units */
   active = bitmap & (modmask[DimMask][hcode] | modmask[BriMask][hcode]) &
      ~(modmask[StdMask][hcode] | modmask[Ext3Mask][hcode] | modmask[PresetMask][hcode] | modmask[Ext0Mask][hcode]);
   if ( active ) {
      mask = 1;
      for ( ucode = 0; ucode < 16; ucode++ ) {
         if ( active & mask ) {
            newlevel = level + x10state[hcode].dimlevel[ucode];
            newlevel = min((int)ondimlevel[hcode][ucode], newlevel);
            newlevel = max(newlevel, 0);
            x10state[hcode].dimlevel[ucode] = newlevel;
         }
         mask = mask << 1;
      }
   }

   return;
}      


/*----------------------------------------------------------------------------+
 | Update the sticky address state of housecode|units                         |
 | Addressing is cumulative until cleared by an initstate command.            |
 +----------------------------------------------------------------------------*/
void x10state_update_sticky_addr ( unsigned char addrbyte )
{
   unsigned char hcode, ucode;
   unsigned int  bitmap;

   hcode = (addrbyte & 0xf0u) >> 4;
   ucode = addrbyte & 0x0fu;
   bitmap = 1 << ucode;

   x10state[hcode].sticky |= bitmap;

   return;
}

/*----------------------------------------------------------------------------+
 | Update the RCS temperature storage                                         |
 | Argument 'level' is 0-31                                                   |
 +----------------------------------------------------------------------------*/
void update_rcs_data ( unsigned char hcode, int lastunit, unsigned char level )
{
   if ( (configp->rcs_temperature & (1 << hcode)) && lastunit > 10 ) {
      x10state[hcode].rcstemp = -60L + (long)level + 32L * (long)(lastunit - 11);
      x10global.rcstemp_flags |= (1 << hcode);
   }
   return;
}

/*----------------------------------------------------------------------------+
 | Update the flag states by engine.                                                    |
 +----------------------------------------------------------------------------*/
void engine_update_flags ( unsigned char bank, unsigned long flagmap, unsigned char mode )
{
   if ( mode == SETFLAG )
      x10global.flags[bank] |= flagmap; /*###*/
   else
      x10global.flags[bank] &= ~flagmap; /*###*/

   return;
}

/*----------------------------------------------------------------------------+
 | Update the flag states.                                                    |
 +----------------------------------------------------------------------------*/
void update_flags_32 ( unsigned char *buf )
{
   unsigned long flagmap;
   
   /* buf[3] = flag bank */
   flagmap = ((unsigned long)buf[4] << 24) |
             ((unsigned long)buf[5] << 16) |
             ((unsigned long)buf[6] << 8)  |
              (unsigned long)buf[7];

   engine_update_flags(buf[3], flagmap, buf[2]);

   return;
}

/*----------------------------------------------------------------------------+
 | Update the flag states.                                                    |
 +----------------------------------------------------------------------------*/
void update_flags ( unsigned char *buf )
{
   unsigned long flagmap;

   flagmap = (unsigned long)((buf[3] << 8) | buf[4]);

   engine_update_flags(0, flagmap, buf[2]);

   return;
}


/*----------------------------------------------------------------------------+
 | Update the mask for units with an exclusive attribute.                     |
 +----------------------------------------------------------------------------*/
void update_exclusive_mask ( unsigned char hcode,
                                        unsigned int bitmap, unsigned int *mask )
{
   unsigned int others;

   if ( (others = modmask[Exc4Mask][hcode] & ~bitmap) != 0 ) {
      *mask |= ( bitmap & 0x4444u ) ? others & 0x4444u :
               ( bitmap & 0x2222u ) ? others & 0x2222u :
               ( bitmap & 0x8888u ) ? others & 0x8888u :
               ( bitmap & 0x1111u ) ? others & 0x1111u : 0;
   }
   if ( (others = modmask[Exc8Mask][hcode] & ~bitmap) != 0 ) {
      *mask |= ( bitmap & 0x6666u ) ? others & 0x6666u :
               ( bitmap & 0x9999u ) ? others & 0x9999u : 0;
   }

   *mask |= modmask[Exc16Mask][hcode] & ~bitmap;

   return;
}

/*----------------------------------------------------------------------------+
 | Update the addressed/unaddresssed state of housecode|units                 |
 | Addressing is cumulative until reset by an intervening function code with  |
 | the same housecode.                                                        |
 | An alloff (all_units_off) function unaddresses all units on that housecode.|
 +----------------------------------------------------------------------------*/
void x10state_update_addr ( unsigned char addrbyte, int *launchp )
{
   unsigned char   hcode, ucode;
   unsigned int    bitmap, launched;
   int             j;
   LAUNCHER        *launcherp;
   extern unsigned int signal_source;

   launcherp = configp->launcherp;

   hcode = (addrbyte & 0xf0u) >> 4;
   ucode = addrbyte & 0x0f;
   bitmap = 1 << ucode;

   x10state[hcode].lastunit = code2unit(ucode);

   x10global.lasthc = hcode;

   if ( signal_source & RCVA ) {
      x10state[hcode].vaddress = bitmap;
   }
   else {
      x10global.lastaddr = bitmap;

      if ( x10state[hcode].reset ) {
         x10state[hcode].addressed = bitmap /* & modmask[AddrMask][hcode] */;
         x10state[hcode].reset = 0;
         x10state[hcode].exclusive = 0;
      }
      else {
         x10state[hcode].addressed |= bitmap /* & modmask[AddrMask][hcode] */;
      }

      update_exclusive_mask(hcode, bitmap, &x10state[hcode].exclusive);
    
      x10state[hcode].addressed &= ~x10state[hcode].exclusive;

      if ( bitmap & modmask[PlcSensorMask][hcode] && signal_source & RCVI && configp->hide_unchanged == YES ) {
         x10state[hcode].squelch = bitmap;
      }
      x10state[hcode].state[AddrState] = x10state[hcode].addressed;
   }

   *launchp = -1;

   if ( configp->script_mode & HEYU_HELPER )
      return;

   launched = 0;
   j = 0;
   while ( launcherp && launcherp[j].line_no > 0 ) {
      if ( launcherp[j].type != L_ADDRESS ||
           launcherp[j].hcode != hcode ||
           is_unmatched_flags(&launcherp[j])  ) {
         j++;
	 continue;
      }
      if ( launcherp[j].bmaptrig & bitmap &&
           launcherp[j].source & signal_source ) {
         *launchp = j;
         launcherp[j].matched = YES;
         launcherp[j].actfunc = 0;
         launcherp[j].genfunc = 0;
         launcherp[j].xfunc = 0;
         launcherp[j].level = 0;
         launcherp[j].bmaplaunch = bitmap;
         launcherp[j].actsource = signal_source;
         launched |= bitmap;
         if ( launcherp[j].scanmode & FM_BREAK )
            break;
      }
      j++;
   }
   x10state[hcode].launched = launched;

   return;
}

#if 0
/*----------------------------------------------------------------------------+
 | Update the addressed/unaddresssed state of housecode|units                 |
 | Addressing is cumulative until reset by an intervening function code with  |
 | the same housecode.                                                        |
 | An alloff (all_units_off) function unaddresses all units on that housecode.|
 +----------------------------------------------------------------------------*/
void x10state_update_addr ( unsigned char addrbyte, int *launchp )
{
   unsigned char   hcode, ucode;
   unsigned int    bitmap, launched;
   int             j;
   LAUNCHER        *launcherp;
   extern unsigned int signal_source;

   launcherp = configp->launcherp;

   hcode = (addrbyte & 0xf0u) >> 4;
   ucode = addrbyte & 0x0f;
   bitmap = 1 << ucode;

   x10state[hcode].lastunit = code2unit(ucode);

   x10global.lasthc = hcode;

   if ( signal_source & RCVA ) {
      x10state[hcode].vaddress = bitmap;
   }
   else {
      x10global.lastaddr = bitmap;

      if ( x10state[hcode].reset ) {
         x10state[hcode].addressed = bitmap;
         x10state[hcode].reset = 0;
         x10state[hcode].exclusive = 0;
      }
      else {
         x10state[hcode].addressed |= bitmap;
      }

      update_exclusive_mask(hcode, bitmap, &x10state[hcode].exclusive);
    
      x10state[hcode].addressed &= ~x10state[hcode].exclusive;

      if ( bitmap & modmask[PlcSensorMask][hcode] && signal_source & RCVI && configp->hide_unchanged == YES ) {
         x10state[hcode].squelch = bitmap;
      }
      x10state[hcode].state[AddrState] = x10state[hcode].addressed;
   }

   *launchp = -1;

   if ( configp->script_mode & HEYU_HELPER )
      return;

   launched = 0;
   j = 0;
   while ( launcherp && launcherp[j].line_no > 0 ) {
      if ( launcherp[j].type != L_ADDRESS ||
           launcherp[j].hcode != hcode ||
           is_unmatched_flags(&launcherp[j])  ) {
         j++;
	 continue;
      }
      if ( launcherp[j].bmaptrig & bitmap &&
           launcherp[j].source & signal_source ) {
         *launchp = j;
         launcherp[j].matched = YES;
         launcherp[j].actfunc = 0;
         launcherp[j].genfunc = 0;
         launcherp[j].xfunc = 0;
         launcherp[j].level = 0;
         launcherp[j].bmaplaunch = bitmap;
         launcherp[j].actsource = signal_source;
         launched |= bitmap;
         if ( launcherp[j].scanmode & FM_BREAK )
            break;
      }
      j++;
   }
   x10state[hcode].launched = launched;

   return;
}
#endif

/*----------------------------------------------------------------------------+
 | Update the x10state structure per the contents of the argument 'buf'       |
 | buf[0] is the housecode|function byte, buf[1] is the dim level 1-210 for   |
 | dims and brights.  (This routine handles just the standard X10 functions - |
 | Type 3 Extended code functions are handled separately.)                    |
 |                                                                            |
 | If the user has defined the type of module, e.g. lamp module, for a        |
 | specific housecode|unit address, the change of state for that address      |
 | is filtered by the supported features of that type of module, e.g. a dim   |
 | signal will be ignored for appliance modules.                              |
 |                                                                            |
 | The received signal and state are tested to see if any of the conditions   |
 | exist for triggering the launching of an external script, and if so, the   |
 | index of the launcher is passed back through argument 'launchp',           | 
 | otherwise -1 is passed back.                                               |
 +----------------------------------------------------------------------------*/
void x10state_update_func ( unsigned char *buf, int *launchp )
{
   unsigned char hcode, func, level, rawdim, lasthc;
   unsigned char actfunc, genfunc;
   unsigned int  lastaddr, addressed;
   unsigned int  trigaddr, active = 0, mask, trigactive, inactive;
   unsigned int  resumask, fullmask, fulloffmask, applmask, bbdimask, ext3dimask, lofmask, ext0mask;
   unsigned int  lubmap, exclusmask;
   unsigned int  bmaplaunch, launched;
   unsigned int  onstate, dimstate, changestate, startupstate;
   unsigned long afuncmap, gfuncmap;
   int           j, dims;
   LAUNCHER      *launcherp;
   extern unsigned int signal_source;
   int update_plcsensor_timeout ( unsigned char, unsigned int );

   launcherp = configp->launcherp;

   *launchp = -1;
  
   if ( !(i_am_state || i_am_monitor) )
      return;

   func = buf[0] & 0x0fu;
   hcode = (buf[0] & 0xf0u) >> 4;
   genfunc = actfunc = func;
   rawdim = 0;

   x10state[hcode].reset = 1;

   lasthc = x10global.lasthc;

   /* Preset1/Preset2 functions take special handling */
   if ( func == 10 || func == 11 ) {
      level = hcode;
      hcode = x10global.lasthc;
      x10global.lasthc = level;
      /* Use zero-base level (0-31) here */
      level = rev_low_nybble(level) + 16 * (func - 10);      
   }
   else {
      x10global.lasthc = hcode;
      level = 0;
   }

   if ( hcode == lasthc && x10global.lastaddr )
      lastaddr = x10global.lastaddr;
   else
      lastaddr = 0xffff;

   x10global.lastaddr = 0;

   gfuncmap = 0;
   mask = 0;
   lubmap = 1 << unit2code(x10state[hcode].lastunit);
//   x10state[hcode].state[ChgState] = 0;
   changestate = 0;
   x10state[hcode].launched = 0;

   addressed = (signal_source & RCVA) ?
            x10state[hcode].vaddress : x10state[hcode].addressed;

   trigaddr = addressed;

   switch ( func ) {
      case  0 :  /* AllOff */
         /* state = OffState; */
         afuncmap = (1 << AllOffTrig);
         gfuncmap = (1 << OffTrig);
         genfunc = OffFunc;
         mask = modmask[AllOffMask][hcode];
         trigaddr = 0xffff;
         lastaddr = 0xffff;
         onstate = x10state[hcode].state[OnState];
         active = modmask[AllOffMask][hcode];
         resumask = (modmask[ResumeMask][hcode] | modmask[ResumeDimMask][hcode]) & modmask[Ext3DimMask][hcode];
         lofmask = modmask[PresetMask][hcode] &
                   modmask[LightsOnFullMask][hcode] &
                   x10state[hcode].state[LightsOnState]; 
         save_dimlevel(hcode, active & resumask & ~lofmask);
         set_dimlevel(0, hcode, active);
         get_states(hcode, &onstate, &dimstate);
//         x10state[hcode].state[ChgState] = active &
         changestate = active &
           ((x10state[hcode].state[OnState] ^ onstate) |
            (x10state[hcode].state[DimState] ^ dimstate));
         if ( signal_source & RCVA )
            x10state[hcode].vaddress &= ~(modmask[AllOffMask][hcode] | modmask[VdataMask][hcode]);
         else
            x10state[hcode].addressed &= ~(modmask[AllOffMask][hcode] | modmask[VdataMask][hcode]);
         x10state[hcode].state[OnState] = onstate;
         x10state[hcode].state[DimState] = dimstate;
         x10state[hcode].state[LightsOnState] &= ~active;
         break;
      case  1 :  /* LightsOn */
         /* state = OnState; */
         afuncmap = (1 << LightsOnTrig);
         gfuncmap = (1 << OnTrig);
         genfunc = OnFunc;
         mask = modmask[LightsOnMask][hcode];
         trigaddr = 0xffff;
         lastaddr = 0xffff;
         resumask = modmask[ResumeMask][hcode];
         fullmask = modmask[LightsOnFullMask][hcode];
         onstate = x10state[hcode].state[OnState];
         active = modmask[LightsOnMask][hcode];
         /* Units which resume saved brightness */
         restore_dimlevel(hcode, active & resumask);
         /* Units which go to 'On' level */
         set_on_level(hcode, active & ~resumask & ~fullmask & modmask[OnFullMask][hcode]); 
         /* Units which go to full brightness */
         set_max_level(hcode, active & 
           (fullmask | (~onstate & modmask[OnFullOffMask][hcode]) | modmask[TargMask][hcode]));
         get_states(hcode, &onstate, &dimstate);
//         x10state[hcode].state[ChgState] = active &
         changestate = active &
           ((x10state[hcode].state[OnState] ^ onstate) |
            (x10state[hcode].state[DimState] ^ dimstate));
         if ( signal_source & RCVA ) {
            x10state[hcode].vaddress &=
               ~(modmask[LightsOnUnaddrMask][hcode] | modmask[VdataMask][hcode]);
	 }
         else {
            x10state[hcode].addressed &=
               ~(modmask[LightsOnUnaddrMask][hcode] | modmask[VdataMask][hcode]);
	 }
         x10state[hcode].state[OnState] = onstate;
         x10state[hcode].state[DimState] = dimstate;
         x10state[hcode].state[LightsOnState] |= active;
         break;
      case  2 :  /* On */
         /* state = OnState; */
         afuncmap = gfuncmap = (1 << OnTrig);
         if ( addressed == 0xffff ) {
            afuncmap = (1 << AllOnTrig);
         }
         mask = modmask[OnMask][hcode];
         onstate = x10state[hcode].state[OnState];
         resumask = modmask[ResumeMask][hcode];
         fullmask = modmask[OnFullMask][hcode];
         fulloffmask = modmask[OnFullOffMask][hcode];
         applmask = ~(modmask[DimMask][hcode] | modmask[BriMask][hcode]);
         exclusmask = x10state[hcode].exclusive;
         active = addressed & modmask[OnMask][hcode] & ~modmask[DeferMask][hcode];
         /* Resume-enabled units will go to saved brightness */
         restore_dimlevel(hcode, active & resumask);
         /* Modules which go to 'On' brightness */
         set_on_level(hcode, active & ~resumask & (~onstate | modmask[TargMask][hcode]));
	 /* Exclusive modules which turn Off */
         set_dimlevel(0, hcode, exclusmask);
         save_dimlevel(hcode, active & resumask);
         get_states(hcode, &onstate, &dimstate);
//         x10state[hcode].state[ChgState] =  /* active & */
         changestate = active &
           ((x10state[hcode].state[OnState] ^ onstate) |
            (x10state[hcode].state[DimState] ^ dimstate));

         if ( signal_source & RCVA ) {
            x10state[hcode].vaddress &=
               ~(modmask[OnOffUnaddrMask][hcode] | modmask[VdataMask][hcode]);
	 }
         else {
            x10state[hcode].addressed &=
               ~(modmask[OnOffUnaddrMask][hcode] | modmask[VdataMask][hcode]);
	 }

         x10state[hcode].state[OnState] = onstate;
         x10state[hcode].state[DimState] = dimstate;
         x10state[hcode].state[LightsOnState] &= ~active;
         if ( x10state[hcode].xconfigmode & 0x02u ) {
            x10state[hcode].state[SpendState] |= (active & modmask[Ext3StatusMask][hcode]);
         }

         break;
      case  3 :  /* Off */
         /* state = OffState; */
         afuncmap = gfuncmap = (1 << OffTrig);
         mask = modmask[OffMask][hcode];
         onstate = x10state[hcode].state[OnState];
         active = addressed & modmask[OffMask][hcode] & ~modmask[DeferMask][hcode];
         resumask = modmask[ResumeMask][hcode] | modmask[ResumeDimMask][hcode];
         ext0mask = modmask[ResumeMask][hcode];
         lofmask = modmask[PresetMask][hcode] &
                   modmask[LightsOnFullMask][hcode] &
                   x10state[hcode].state[LightsOnState]; 
         save_dimlevel(hcode, active & resumask & ~lofmask & ~ext0mask);
         set_dimlevel(0, hcode, active);
         get_states(hcode, &onstate, &dimstate);
//         x10state[hcode].state[ChgState] = active &
         changestate = active &
           ((x10state[hcode].state[OnState] ^ onstate) |
            (x10state[hcode].state[DimState] ^ dimstate));

         if ( signal_source & RCVA ) {
            x10state[hcode].vaddress &=
               ~(modmask[OnOffUnaddrMask][hcode] | modmask[VdataMask][hcode]);
	 }
         else {
            x10state[hcode].addressed &=
               ~(modmask[OnOffUnaddrMask][hcode] | modmask[VdataMask][hcode]);
	 }

         x10state[hcode].state[OnState]  = onstate;
         x10state[hcode].state[DimState] = dimstate;
         x10state[hcode].state[LightsOnState] &= ~active;
         if ( x10state[hcode].xconfigmode & 0x02u ) {
            x10state[hcode].state[SpendState] |= (active & modmask[Ext3StatusMask][hcode]);
         }
         break;
      case  4 :  /* Dim */
         /* state = DimState; */
         afuncmap = gfuncmap = (1 << DimTrig);
         genfunc = DimFunc;
         mask = modmask[DimMask][hcode];
         onstate = x10state[hcode].state[OnState];
	 rawdim = buf[1];
         dims = -(int)rawdim;
         active = addressed & modmask[DimMask][hcode];
         bbdimask = modmask[BriDimMask][hcode];
         resumask = modmask[ResumeDimMask][hcode];
         ext3dimask = modmask[Ext3DimMask][hcode];
         inactive = ext3dimask & ~(bbdimask | resumask) & ~onstate;
         /* Off resume-enabled units will first go to saved brightness */
         restore_dimlevel(hcode, active & resumask & ~onstate & ext3dimask);
         /* Modules which first go to full brightness if Off */
         set_max_level(hcode, active & bbdimask & ~onstate);
         update_dimlevel(dims, hcode, active & ~inactive);
         lofmask = modmask[PresetMask][hcode] &
                   modmask[LightsOnFullMask][hcode] &
                   x10state[hcode].state[LightsOnState];
         save_dimlevel(hcode, active & resumask & ~lofmask);
         get_states(hcode, &onstate, &dimstate);
//         x10state[hcode].state[ChgState] = active &
         changestate = active &
           ((x10state[hcode].state[OnState] ^ onstate) |
            (x10state[hcode].state[DimState] ^ dimstate));
         x10state[hcode].state[OnState]  = onstate;
         x10state[hcode].state[DimState] = dimstate;
         x10state[hcode].state[LightsOnState] &= ~active;
         if ( x10state[hcode].xconfigmode & 0x02u ) {
            x10state[hcode].state[SpendState] |= (active & modmask[Ext3StatusMask][hcode]);
         }
         break;
      case  5 :  /* Bright */
         /* state = DimState; */
         afuncmap = (1 << BriTrig);
         gfuncmap = (1 << DimTrig);
         genfunc = DimFunc;
         mask = modmask[BriMask][hcode];
	 rawdim = buf[1];
         dims = (int)rawdim;
         active = addressed & modmask[BriMask][hcode];
         onstate = x10state[hcode].state[OnState];
         bbdimask = modmask[BriDimMask][hcode];
         resumask = modmask[ResumeDimMask][hcode];
         ext3dimask = modmask[Ext3DimMask][hcode];
         inactive = ext3dimask & ~(bbdimask | resumask) & ~onstate;
         /* Off, resume-enabled units will first go to saved brightness */
         restore_dimlevel(hcode, active & resumask & ~onstate);
         /* Modules which first go to full brightness if Off */
         set_max_level(hcode, active & bbdimask & ~onstate);
         update_dimlevel(dims, hcode, active & ~inactive);
         lofmask = modmask[PresetMask][hcode] &
                   modmask[LightsOnFullMask][hcode] &
                   x10state[hcode].state[LightsOnState]; 
         save_dimlevel(hcode, active & resumask & ~lofmask);
         get_states(hcode, &onstate, &dimstate);
//         x10state[hcode].state[ChgState] = active &
         changestate = active &
           ((x10state[hcode].state[OnState] ^ onstate) |
            (x10state[hcode].state[DimState] ^ dimstate));
         x10state[hcode].state[OnState]  = onstate;
         x10state[hcode].state[DimState] = dimstate;
         x10state[hcode].state[LightsOnState] &= ~active;
         if ( x10state[hcode].xconfigmode & 0x02u ) {
            x10state[hcode].state[SpendState] |= (active & modmask[Ext3StatusMask][hcode]);
         }
         break;
      case  6 : /* LightsOff */
         /* state = OffState; */
         afuncmap = (1 << LightsOffTrig);
         gfuncmap = (1 << OffTrig);
         genfunc = OffFunc;
         mask = modmask[LightsOffMask][hcode];
         trigaddr = 0xffff;
         lastaddr = 0xffff;
         onstate = x10state[hcode].state[OnState];
         active = modmask[LightsOffMask][hcode];
         resumask = modmask[ResumeMask][hcode] | modmask[ResumeDimMask][hcode];
         lofmask = modmask[PresetMask][hcode] &
                   modmask[LightsOnFullMask][hcode] &
                   x10state[hcode].state[LightsOnState]; 
         save_dimlevel(hcode, active & resumask & ~lofmask);
         set_dimlevel(0, hcode, active);
         get_states(hcode, &onstate, &dimstate);
//         x10state[hcode].state[ChgState] = active &
         changestate = active &
           ((x10state[hcode].state[OnState] ^ onstate) |
            (x10state[hcode].state[DimState] ^ dimstate));
         if ( signal_source & RCVA ) {
            x10state[hcode].vaddress &=
               ~(modmask[LightsOffUnaddrMask][hcode] | modmask[VdataMask][hcode]);
	 }
         else {
            x10state[hcode].addressed &=
               ~(modmask[LightsOffUnaddrMask][hcode] | modmask[VdataMask][hcode]);
	 }
         x10state[hcode].state[OnState]  = onstate;
         x10state[hcode].state[DimState] = dimstate;
         x10state[hcode].state[LightsOnState] &= ~active;
         break;
      case 10 :  /* Preset1 */
      case 11 :  /* Preset2 */
         if ( hcode == 0xff ) {
            /* Housecode unknown - can't do anything */
            return;
         }
         if ( signal_source & RCVI ) {
            update_rcs_data(hcode, x10state[hcode].lastunit, level);
         }
         onstate = x10state[hcode].state[OnState];
         dimstate = x10state[hcode].state[DimState];
         mask = modmask[PresetMask][hcode];
         active = addressed & modmask[PresetMask][hcode];
         resumask = modmask[ResumeMask][hcode];
         afuncmap = (1 << PresetTrig);
         if ( level == 0 ) {
            /* Lowest level => Off */
            /* state = OffState; */
            gfuncmap = (1 << OffTrig);
            genfunc = OffFunc; 
            save_dimlevel(hcode, active & resumask);
            set_dimlevel(0, hcode, active);
            get_states(hcode, &onstate, &dimstate);
//            x10state[hcode].state[ChgState] = active &
            changestate = active &
              ((x10state[hcode].state[OnState] ^ onstate) |
               (x10state[hcode].state[DimState] ^ dimstate));
            x10state[hcode].state[OnState]  = onstate;
            x10state[hcode].state[DimState] = dimstate;
         }
         else if ( level == 31 ) {
            /* Highest level => On */
            /* state = OnState; */
            gfuncmap = (1 << OnTrig);
            genfunc = OnFunc; 
            set_dimlevel(31, hcode, active);
            save_dimlevel(hcode, active & resumask);
            get_states(hcode, &onstate, &dimstate);
//            x10state[hcode].state[ChgState] = active &
            changestate = active &
              ((x10state[hcode].state[OnState] ^ onstate) |
               (x10state[hcode].state[DimState] ^ dimstate));
            x10state[hcode].state[OnState]  = onstate;
            x10state[hcode].state[DimState] = dimstate;
         }       
         else {
            /* Intermediate level => Dim */
            /* state = DimState; */
            gfuncmap = (1 << DimTrig);
            genfunc = DimFunc; 
            set_dimlevel(level, hcode, active);
            save_dimlevel(hcode, active & resumask);
            get_states(hcode, &onstate, &dimstate);
//            x10state[hcode].state[ChgState] = active &
            changestate = active &
              ((x10state[hcode].state[OnState] ^ onstate) |
               (x10state[hcode].state[DimState] ^ dimstate));
            x10state[hcode].state[OnState]  = onstate;
            x10state[hcode].state[DimState] = dimstate;
         }
         x10state[hcode].state[LightsOnState] &= ~active;

         /* Preset modules unaddress themselves after receiving a preset function */
         /* (otherwise they'd be affected by the next non-Preset function on the  */
         /* same housecode), but not if they've _sent_ the preset in response to  */
         /* a status request.                                                     */
         if ( !(x10state[hcode].lastcmd == StatusReqFunc && signal_source & RCVI) )
            x10state[hcode].addressed &= ~active;
       
         break;
      case 13 :  /* Status On - limited to the last received unit address */
         /* state = OnState; */
         afuncmap = (1 << StatusOnTrig);
         if ( !(signal_source & RCVI) ) {
            active = 0;
            break;
         }
         mask = modmask[StatusOnMask][hcode];
//         active = addressed & modmask[StatusOnMask][hcode];
         active = addressed & modmask[StatusOnMask][hcode] & lubmap;
         onstate = x10state[hcode].state[OnState];
         set_on_level(hcode, active & ~onstate);
         get_states(hcode, &onstate, &dimstate);
//         x10state[hcode].state[ChgState] = active &
         changestate = active &
           ((x10state[hcode].state[OnState] ^ onstate) |
            (x10state[hcode].state[DimState] ^ dimstate));
         x10state[hcode].state[OnState]  = onstate;
         x10state[hcode].state[DimState] = dimstate;
         x10state[hcode].state[LightsOnState] &= ~active;
//         x10state[hcode].state[SpendState] &= ~addressed;
         x10state[hcode].state[SpendState] &= ~active;
         break;
      case 14 :  /* Status Off - limited to the last received unit address */
         /* state = OffState; */
         afuncmap = (1 << StatusOffTrig);
         gfuncmap = 0;
         if ( !(signal_source & RCVI) ) {
            active = 0;
            break;
         }
         mask = modmask[StatusOffMask][hcode];
//         active = addressed & modmask[StatusOffMask][hcode];
         active = addressed & modmask[StatusOffMask][hcode] & lubmap;
         onstate = x10state[hcode].state[OnState];
         dimstate = x10state[hcode].state[DimState];
         x10state[hcode].state[OnState]  &= ~active;
         x10state[hcode].state[DimState] &= ~active;
         set_dimlevel(0, hcode, active);
//         x10state[hcode].state[ChgState] = active &
         changestate = active &
           ((x10state[hcode].state[OnState] ^ onstate) |
            (x10state[hcode].state[DimState] ^ dimstate));
         x10state[hcode].state[LightsOnState] &= ~active;
//         x10state[hcode].state[SpendState] &= ~addressed;
         x10state[hcode].state[SpendState] &= ~active;
         break;
      case 15 : /* StatusReq */
         afuncmap = (1 << StatusReqTrig);
         active = addressed & modmask[StatusMask][hcode];
         x10state[hcode].state[SpendState] |= addressed;
         break;
      case 8 :  /* HailReq */
         if ( (signal_source & RCVI) == 0 )
            x10global.hailstate = 0;
         afuncmap = (1 << HailReqTrig);
         active = 0;
         break;
      case 9 :  /* Hail Ack */
         if ( signal_source & RCVI )
            x10global.hailstate |= (1 << hcode);
         afuncmap = (1 << HailAckTrig);
         active = 0;
         break;
      case 12 : /* Data Xfer */
         afuncmap = (1 << DataXferTrig);
         active = 0;
         break;
      default :
         /* state = OffState; */
         afuncmap = 0;
         mask = 0;
         trigaddr = 0;
         active = 0;
         break;
   }

   x10state[hcode].lastcmd = func;

   for ( j = 0; j < 16; j++ ) {
      if ( (addressed & vmodmask[VtstampMask][hcode]) & (1 << j) ) {
         x10state[hcode].timestamp[j] = time(NULL);
      }
   }

   startupstate = ~x10state[hcode].state[ValidState] & active;
   x10state[hcode].state[ValidState] |= active;

   if ( signal_source & RCVI && lubmap & modmask[PlcSensorMask][hcode] ) {
      update_plcsensor_timeout(hcode, lubmap);
      update_activity_states(hcode, lubmap, S_ACTIVE);
   }

//   x10state[hcode].state[ModChgState] = (x10state[hcode].state[ModChgState] & ~active) | changestate;
   x10state[hcode].state[ModChgState] = changestate;

   x10state[hcode].state[ChgState] = x10state[hcode].state[ModChgState] |
      (x10state[hcode].state[ActiveChgState] & active) |
      (startupstate & ~modmask[PhysMask][hcode]);

   changestate = x10state[hcode].state[ChgState];

   if ( signal_source & RCVI ) {
      x10state[hcode].lastactive = active;
   }

   if ( i_am_state )
      write_x10state_file();

   /* Heyuhelper, if applicable */
   if ( signal_source & RCVI && configp->script_mode & HEYU_HELPER ) {
      launch_heyuhelper(hcode, trigaddr, func);
      return;
   }

   bmaplaunch = 0;
   launched = 0;
   j = 0;
   while ( launcherp && launcherp[j].line_no > 0 ) {
      if ( launcherp[j].type != L_NORMAL ||
           launcherp[j].hcode != hcode ||
           is_unmatched_flags(&launcherp[j]) ||
           launcherp[j].vflags || launcherp[j].notvflags ||
           !(launcherp[j].bmaptrigemu & lastaddr) ) {
         j++;
	 continue;
      }

      if ( (launcherp[j].afuncmap & afuncmap || launcherp[j].gfuncmap & gfuncmap)  &&
           launcherp[j].source & signal_source ) {
         trigactive = trigaddr & (mask | launcherp[j].signal);

         if ( (bmaplaunch = (trigactive & launcherp[j].bmaptrig) &
#if 0 
                   (x10state[hcode].state[ChgState] | ~launcherp[j].chgtrig)) ||
#endif
                   (changestate | ~launcherp[j].chgtrig)) ||
                   (launcherp[j].unitstar && !trigaddr) ) {
            *launchp = j;
            launcherp[j].matched = YES;
            launcherp[j].bmaplaunch = bmaplaunch;
            launcherp[j].actfunc = func;
            launcherp[j].genfunc = genfunc;
	    launcherp[j].level = level + 1;  /* preset 1-32 */
	    launcherp[j].rawdim = rawdim;
            launcherp[j].actsource = signal_source;
            launched |= bmaplaunch;
            if ( launcherp[j].scanmode & FM_BREAK )
               break;
         }
      }
      j++;
   }
   x10state[hcode].launched = launched;

   return;
}

/*----------------------------------------------------------------------------+
 | Update the x10state structure per the contents of the argument 'buf'       |
 | for type 3 extended codes.  'buf' will always contain 4 bytes.             |
 | Supported Type/Functions are limited to 0x31, 0x33, 0x34, and 0x38.        |
 |                                                                            |
 | If the user has defined the type of module, e.g. lamp module, for a        |
 | specific housecode|unit address, the change of state for that address      |
 | is filtered by the supported features of that type of module, e.g. an      |
 | extended preset dim signal will be ignored for appliance modules.          |
 |                                                                            |
 | The received signal and state are tested to see if any of the conditions   |
 | exist for triggering the launching of an external script, and if so, the   |
 | index of the launcher is passed back through argument 'launchp',           | 
 | otherwise -1 is passed back.                                               |
 +----------------------------------------------------------------------------*/
void x10state_update_ext3func ( unsigned char *buf, int *launchp )
{
   unsigned char hcode, func, xfunc, xdata, ucode, subunit, level, grpbmap, group, subgroup;
   unsigned char grplevel;
   int      oldlevel, memlevel;
   unsigned char actfunc, genfunc;
   unsigned int  bitmap, trigaddr, mask, grpmask, active = 0, trigactive, switchmask;
   unsigned int  onstate, dimstate, changestate, startupstate;
   unsigned int  bmaplaunch, launched;
   unsigned long afuncmap, gfuncmap;
   int           j;
   LAUNCHER      *launcherp;
   extern unsigned int signal_source;

   launcherp = configp->launcherp;

   *launchp = -1;

   if ( !(i_am_state || i_am_monitor) )
      return;

   func = buf[0] & 0x0f;
   if ( func != 7 )
      return;
   actfunc = genfunc = func;
   afuncmap = gfuncmap = (1 << ExtendedTrig);

   hcode = (buf[0] & 0xf0u) >> 4;
   ucode = buf[1] & 0x0fu;

   subunit = (buf[1] & 0xf0u) >> 4;
   /* Subunit states are not supported */
   if ( subunit > 0 ) 
      return;

   xdata = buf[2];	      
   level = xdata & 0x3f;
   xfunc = buf[3];
   bitmap = 1 << ucode;
   trigaddr = bitmap;

   x10state[hcode].lastcmd = func;
//   x10state[hcode].state[ChgState] = 0;
   changestate = 0;
   x10state[hcode].lastunit = code2unit(ucode);
   x10global.lasthc = hcode;

   x10state[hcode].reset = 1;

   x10global.lastaddr = 0;

   onstate = x10state[hcode].state[OnState];
   dimstate = x10state[hcode].state[DimState];
   switchmask = modmask[Ext3Mask][hcode] & ~modmask[Ext3DimMask][hcode];

   switch ( xfunc ) {
      case 0x30 :  /* Include in group at current level */
         mask = 0;
         gfuncmap = 0;
         genfunc = 0;
         if ( (bitmap & modmask[Ext3GrpExecMask][hcode]) == 0 )
            break;
         group = (xdata & 0xc0u) >> 6;
         x10state[hcode].grpmember[ucode] |= (1 << group);

         grplevel = x10state[hcode].dimlevel[ucode];
         x10state[hcode].grplevel[ucode][group] = grplevel;
   
         if ( xdata & 0x20u ) {
            subgroup = xdata & 0x0fu;
            if ( grplevel == 63 && (modmask[Ext3DimMask][hcode] & bitmap) )
               x10state[hcode].grplevel[ucode][group] = 62;
               
            if ( modmask[Ext3GrpRelT1Mask][hcode] & bitmap ) {
               /* LM14A - All groups changed to subgroup */
               x10state[hcode].grpaddr[ucode] = subgroup | 0x80u;
               x10state[hcode].grpmember[ucode] &= ~(0x80u);
            }
            else if ( modmask[Ext3GrpRelT2Mask][hcode] & bitmap ) {
               /* LM465-1 change to a different subgroup removes other groups */
               if ( x10state[hcode].grpaddr[ucode] != (subgroup | 0x80u) ) {
                  x10state[hcode].grpmember[ucode] &= (1 << group);
               }
               x10state[hcode].grpaddr[ucode] = subgroup | 0x80u;
               x10state[hcode].grpmember[ucode] &= ~(0x80u);
            }
            else if ( modmask[Ext3GrpRelT3Mask][hcode] & bitmap ) {
               /* WS467-1 is in both absolute and relative groups */
               x10state[hcode].grpaddr[ucode] = subgroup | 0x80u;
               x10state[hcode].grpmember[ucode] |= 0x80u;
            }
         }
         else {
            x10state[hcode].grpaddr[ucode] = 0;
         }
         break;
      case 0x31 :  /* Ext Preset */
         if ( level > 0 && (switchmask & bitmap) ) {
            gfuncmap = (1 << OnTrig);
            genfunc = OnFunc; 
            mask = modmask[Ext3Mask][hcode];
            active = bitmap & mask;
//            x10state[hcode].state[ChgState] = ~x10state[hcode].state[OnState] & active;
            changestate = ~x10state[hcode].state[OnState] & active;
            x10state[hcode].state[OnState] |= active;
            x10state[hcode].state[DimState] &= active;
            set_dimlevel(63, hcode, active);
            save_dimlevel(hcode, active);
         }
         else if ( level >= 62 ) {
            /* Full On is level 62 or 63 */
            gfuncmap = (1 << OnTrig);
            genfunc = OnFunc; /* On function */
            mask = modmask[Ext3DimMask][hcode];
            active = bitmap & mask;
//            x10state[hcode].state[ChgState] =
            changestate =  
               (~x10state[hcode].state[OnState] | x10state[hcode].state[DimState]) & active;
            x10state[hcode].state[OnState] |= active;
            x10state[hcode].state[DimState] &= ~active;
            set_dimlevel(62, hcode, active);
            save_dimlevel(hcode, active);
         }
         else if ( level == 0 ) {
            /* Level 0 is Off */
            gfuncmap = (1 << OffTrig);
            genfunc = OffFunc;
            mask = modmask[Ext3Mask][hcode];
            active = bitmap & mask;
//            x10state[hcode].state[ChgState] =
            changestate =
              (x10state[hcode].state[OnState] | x10state[hcode].state[DimState]) & active;
            x10state[hcode].state[OnState] &= ~active;
            x10state[hcode].state[DimState] &= ~active;
            save_dimlevel(hcode, active);
            set_dimlevel(0, hcode, active);
         }
         else {
            /* state = DimState; */
            gfuncmap = (1 << DimTrig);
            genfunc = DimFunc;
            mask = modmask[Ext3DimMask][hcode];
            active = bitmap & mask;
//            x10state[hcode].state[ChgState] = active & 
            changestate = active &
              ~x10state[hcode].state[DimState];
            x10state[hcode].state[OnState] |= active;
            x10state[hcode].state[DimState] |= active;
            set_dimlevel(level, hcode, active);
            save_dimlevel(hcode, active);
         }
         if ( x10state[hcode].xconfigmode & 0x01u ) {
            x10state[hcode].state[SpendState] |= (bitmap & modmask[Ext3StatusMask][hcode]);
         }
         break;
      case 0x32 : /* Include in Group at specified level */
         mask = 0;
         gfuncmap = 0;
         genfunc = 0;
         group = (xdata & 0xc0u) >> 6;
         grplevel = xdata & 0x3fu;
         if ( grplevel > 0 && (switchmask & bitmap) ) {
            grplevel = 63;
         }
         x10state[hcode].grpmember[ucode] |= (1 << group);
         x10state[hcode].grplevel[ucode][group] = grplevel;
         x10state[hcode].grpaddr[ucode] = 0;
         break;
      case 0x33 : /* Ext AllOn */
         gfuncmap = (1 << OnTrig);
         genfunc = OnFunc;
         mask = modmask[Ext3Mask][hcode];
         trigaddr = 0xffff;
         active = modmask[Ext3Mask][hcode];
//         x10state[hcode].state[ChgState] = active &
         changestate = active &
            (~x10state[hcode].state[OnState] | x10state[hcode].state[DimState]);
         x10state[hcode].state[OnState] |= active;
         x10state[hcode].state[DimState] &= ~active;
         /* Ext AllOn sets full brightness regardless of previous dim level */
         set_dimlevel(62, hcode, active);
         save_dimlevel(hcode, active);
         break;
      case 0x34 : /* Ext AllOff */
         gfuncmap = (1 << OffTrig);
         genfunc = OffFunc; 
         mask = modmask[Ext3Mask][hcode];
         trigaddr = 0xffff;
         active = modmask[Ext3Mask][hcode];
//         x10state[hcode].state[ChgState] = active &
         changestate = active &
            (x10state[hcode].state[OnState] | x10state[hcode].state[DimState]);
         x10state[hcode].state[OnState] &= ~active;
         x10state[hcode].state[DimState] &= ~active;
         /* Does not set the module "memory" level to 0 */
         save_dimlevel(hcode, active & modmask[Ext3DimMask][hcode]);
         set_dimlevel(0, hcode, active);
         break;
      case 0x35 : /* Remove Housecode|Unit from groups */
         mask = 0;
         gfuncmap = 0;
         genfunc = 0;
         grpbmap = xdata & 0x0fu;
         if ( (xdata & 0xf0u ) == 0 ) {
            if ( modmask[Ext3GrpRemMask][hcode] & (1 << ucode) )
               x10state[hcode].grpmember[ucode] &= ~grpbmap;
         }
         else {
            for ( j = 0; j < 16; j++ ) { 
               if ( modmask[Ext3GrpRemMask][hcode] & (1 << j) )
                  x10state[hcode].grpmember[j] &= ~grpbmap;
            }
         }
         break;
      case 0x3c : /* Group Bright/Dim (one level) */
      case 0x36 : /* Execute Group function */
         mask = 0;
         gfuncmap = 0;
         genfunc = 0;
         trigaddr = 0;
         group = (xdata & 0xc0u) >> 6;
         grpmask = 1 << group;
         if ( (xdata & 0x20u) )
            subgroup = (xdata & 0x0fu) | 0x80;
         else
            subgroup = 0;

         for ( j = 0; j < 16; j++ ) {
            bitmap = 1 << j;
            if ( (x10state[hcode].grpmember[j] & grpmask) == 0 ||
                 (modmask[Ext3GrpExecMask][hcode] & bitmap) == 0 ||
                  (x10state[hcode].grpaddr[j] != subgroup &&
                  (x10state[hcode].grpmember[j] & 0x80u) == 0) ) {
               continue;
            }
            ucode = j;
            bitmap = 1 << ucode;
            trigaddr |= bitmap;

            if ( xfunc == 0x3c ) {
               if ( (modmask[Ext3GrpBriDimMask][hcode] & bitmap) == 0 ) {
                  continue;
               }
               oldlevel = x10state[hcode].dimlevel[ucode];
               memlevel = x10state[hcode].memlevel[ucode];
               if ( (modmask[Ext3GrpBriDimFullMask][hcode] & bitmap) != 0 ) {
                  if ( oldlevel == 0 )
                     oldlevel = memlevel;
               }              
               if ( xdata & 0x10u )
                  grplevel = min(63, oldlevel + 1);
               else
                  grplevel = max(1, oldlevel - 1);
            }
            else { 
               if ( (xdata & 0x10u) && (modmask[Ext3GrpOffMask][hcode] & bitmap) )
                  grplevel = 0;
               else if ( (xdata & 0x10u) && (modmask[Ext3GrpOffExecMask][hcode] & bitmap) )
                  grplevel = x10state[hcode].grplevel[ucode][group];
               else
                  grplevel = x10state[hcode].grplevel[ucode][group];
            }

            if ( grplevel > 0 && (bitmap & switchmask) ) {
               gfuncmap |= (1 << OnTrig);
               genfunc = OnFunc; 
               mask = modmask[Ext3Mask][hcode];
               active = bitmap & mask;
//               x10state[hcode].state[ChgState] &= ~active;
//               x10state[hcode].state[ChgState] |= ~x10state[hcode].state[OnState] & active;
               changestate = active & ~x10state[hcode].state[OnState];
               x10state[hcode].state[OnState] |= active;
               x10state[hcode].state[DimState] &= ~active;
               set_dimlevel(63, hcode, active);
               save_dimlevel(hcode, active);
            }
            else if ( grplevel >= 62 ) {
               /* Full On is level 62 or 63 */
               gfuncmap |= (1 << OnTrig);
               genfunc = OnFunc; /* On function */
               mask = modmask[Ext3DimMask][hcode];
               active = bitmap & mask;
//               x10state[hcode].state[ChgState] &= ~active;
//               x10state[hcode].state[ChgState] |=
               changestate =
                  (~x10state[hcode].state[OnState] | x10state[hcode].state[DimState]) & active;
               x10state[hcode].state[OnState] |= active;
               x10state[hcode].state[DimState] &= ~active;
               set_dimlevel(62, hcode, active);
               save_dimlevel(hcode, active);
            }
            else if ( grplevel == 0 ) {
               /* Level 0 is Off */
               gfuncmap |= (1 << OffTrig);
               genfunc = OffFunc;
               mask = modmask[Ext3Mask][hcode];
               active = bitmap & mask;
//               x10state[hcode].state[ChgState] &= ~active;
//               x10state[hcode].state[ChgState] |=
               changestate =
                 (x10state[hcode].state[OnState] | x10state[hcode].state[DimState]) & active;
               x10state[hcode].state[OnState] &= ~active;
               x10state[hcode].state[DimState] &= ~active;
               save_dimlevel(hcode, active);
               set_dimlevel(0, hcode, active);
            }
            else {
               /* state = DimState; */
               gfuncmap |= (1 << DimTrig);
               genfunc = DimFunc;
               mask = modmask[Ext3DimMask][hcode];
               active = bitmap & mask;
//               x10state[hcode].state[ChgState] &= ~active;
//               x10state[hcode].state[ChgState] |= active &
               changestate = active & 
                 ~x10state[hcode].state[DimState];
               x10state[hcode].state[OnState] |= active;
               x10state[hcode].state[DimState] |= active;
               set_dimlevel(grplevel, hcode, active);
               save_dimlevel(hcode, active);
            }
         }
         break;
      case 0x37 : /* Ext Status Req */
         gfuncmap = 0;
         genfunc = 0;
         mask = modmask[Ext3Mask][hcode];
         active = bitmap & mask;
         if ( (xdata & 0x30u) == 0x10u ) {
            /* Extended PowerUp signal from module */
            afuncmap = (1 << ExtPowerUpTrig);
         }
         else {
            x10state[hcode].state[SpendState] |= bitmap;
         }
         break;
      case 0x38 : /* Ext Status Ack */
         gfuncmap = 0;
         if ( level > 0 && (switchmask & bitmap) ) {
            gfuncmap = (1 << OnTrig);
            genfunc = OnFunc;
            mask = modmask[Ext3Mask][hcode];
            active = bitmap & mask;
//            x10state[hcode].state[ChgState] = ~x10state[hcode].state[OnState] & active;
            changestate = ~x10state[hcode].state[OnState] & active;
            x10state[hcode].state[OnState] |= active;
            x10state[hcode].state[DimState] &= ~active;
            set_dimlevel(63, hcode, active);
            x10state[hcode].state[SpendState] &= ~bitmap;
         }
         else if ( level >= 62 ) {
            gfuncmap = (1 << OnTrig);
            genfunc = OnFunc;
            mask = modmask[Ext3DimMask][hcode];
            active = bitmap & mask;
//            x10state[hcode].state[ChgState] = ~x10state[hcode].state[OnState] & active;
            changestate = ~x10state[hcode].state[OnState] & active;
            x10state[hcode].state[OnState] |= active;
            x10state[hcode].state[DimState] &= ~active;
            set_dimlevel(62, hcode, active);
            save_dimlevel(hcode, active);
            x10state[hcode].state[SpendState] &= ~bitmap;
         }
         else if ( level == 0 ) {
            /* This does not reflect the module's level  */
            /* "memory", only it's current status as OFF */
            gfuncmap = (1 << OffTrig);
            genfunc = OffFunc; 
            mask = modmask[Ext3Mask][hcode];
            active = bitmap & mask;
//            x10state[hcode].state[ChgState] = x10state[hcode].state[OnState] & active;
            changestate = x10state[hcode].state[OnState] & active;
            x10state[hcode].state[OnState] &= ~active;
            x10state[hcode].state[DimState] &= ~active;
            /* This may be inaccurate, depending on module's history */ 
            save_dimlevel(hcode, active & modmask[Ext3DimMask][hcode]);
            set_dimlevel(0, hcode, active);
            x10state[hcode].state[SpendState] &= ~bitmap;
         }
         else {
            /* state = DimState; */
            mask = modmask[Ext3DimMask][hcode];
            gfuncmap = (1 << DimTrig);
            genfunc = DimFunc;
            active = bitmap & mask;
//            x10state[hcode].state[ChgState] = ~x10state[hcode].state[DimState] & active;
            changestate = ~x10state[hcode].state[DimState] & active;
            x10state[hcode].state[OnState] |= active;
            x10state[hcode].state[DimState] |= active;
            set_dimlevel(level, hcode, active);
            save_dimlevel(hcode, active);
            x10state[hcode].state[SpendState] &= ~bitmap;
         }
         break;
      case 0x39 : /* Extended Group Status Ack */
         mask = modmask[Ext3Mask][hcode];
         active = mask & bitmap;
         if ( active ) {
            x10state[hcode].state[SpendState] &= ~bitmap;
         }
//         x10state[hcode].state[ChgState] &= ~bitmap;
         changestate = 0;
         mask = 0;
         gfuncmap = 0;
         genfunc = 0;
         break;
      case 0x3a : /* Extended Group Status Nack */
         mask = modmask[Ext3Mask][hcode];
         active = mask & bitmap;
         if ( active ) {
            x10state[hcode].state[SpendState] &= ~bitmap;
         }
//         x10state[hcode].state[ChgState] &= ~bitmap;
         changestate = 0;
         mask = 0;
         gfuncmap = 0;
         genfunc = 0;
         break;
      case 0x3b : /* Configure modules */
         mask = 0;
         gfuncmap = 0;
         genfunc = 0;
         x10state[hcode].xconfigmode = xdata & 0x03;
         break;
      default :
         /* state = OffState; */
         mask = 0;
         gfuncmap = 0;
         genfunc = 0;
         break;
   }

   if ( signal_source & RCVI )
      x10state[hcode].lastactive = 0;

   startupstate = ~x10state[hcode].state[ValidState] & active;
   x10state[hcode].state[ValidState] |= bitmap;

//   x10state[hcode].state[ModChgState] = (x10state[hcode].state[ModChgState] & ~active) | changestate;
   x10state[hcode].state[ModChgState] = changestate;

   x10state[hcode].state[ChgState] = x10state[hcode].state[ModChgState] |
      (x10state[hcode].state[ActiveChgState] & bitmap) |
      (startupstate & ~modmask[PhysMask][hcode]);

   changestate = x10state[hcode].state[ChgState];

   if ( bitmap & vmodmask[VtstampMask][hcode] ) {
      x10state[hcode].timestamp[ucode] = time(NULL);
   }

   if ( i_am_state )
      write_x10state_file();

   /* Heyuhelper, if applicable */
   if ( i_am_state != 0 && signal_source & RCVI && configp->script_mode & HEYU_HELPER ) {
      launch_heyuhelper(hcode, trigaddr, func);
      return;
   }

   bmaplaunch = 0;
   launched = 0;
   j = 0;
   while ( launcherp && launcherp[j].line_no > 0 ) {
      if ( launcherp[j].type != L_NORMAL ||
           launcherp[j].hcode != hcode ||
           is_unmatched_flags(&launcherp[j])  ) {
         j++;
	 continue;
      }
      if ( (launcherp[j].afuncmap & afuncmap || launcherp[j].gfuncmap & gfuncmap) &&
           launcherp[j].source & signal_source ) {
         trigactive = trigaddr & (mask | launcherp[j].signal);
         if ( (bmaplaunch = (trigactive & launcherp[j].bmaptrig) & 
//                   (x10state[hcode].state[ChgState] | ~launcherp[j].chgtrig)) ) {
                   (changestate | ~launcherp[j].chgtrig)) ) {
            *launchp = j;
            launcherp[j].matched = YES;
            launcherp[j].actfunc = actfunc;
            launcherp[j].genfunc = genfunc;
	    launcherp[j].xfunc = xfunc;
	    launcherp[j].level = level;
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

   return;
}

#ifdef HASEXT0
/*----------------------------------------------------------------------------+
 | Update the x10state structure per the contents of the argument 'buf'       |
 | for type 0 extended codes.  'buf' will always contain 4 bytes.             |
 | Supported Type/Functions are limited to 0x01, 0x02, 0x03, 0x04, 0x0B       |
 |                                                                            |
 | If the user has defined the type of module, e.g. lamp module, for a        |
 | specific housecode|unit address, the change of state for that address      |
 | is filtered by the supported features of that type of module, e.g. an      |
 | extended preset dim signal will be ignored for appliance modules.          |
 |                                                                            |
 | The received signal and state are tested to see if any of the conditions   |
 | exist for triggering the launching of an external script, and if so, the   |
 | index of the launcher is passed back through argument 'launchp',           | 
 | otherwise -1 is passed back.                                               |
 +----------------------------------------------------------------------------*/
void x10state_update_ext0func ( unsigned char *buf, int *launchp )
{
   unsigned char hcode, func, xfunc, ucode, subunit, level, limit;
   unsigned char actfunc, genfunc;
   unsigned int  bitmap, trigaddr, mask, active = 0, trigactive;
   unsigned int  onstate, dimstate, changestate, startupstate;
   unsigned int  bmaplaunch, launched;
   unsigned long afuncmap, gfuncmap;
   int           j;
   LAUNCHER      *launcherp;
   extern unsigned int signal_source;

   launcherp = configp->launcherp;

   *launchp = -1;

   if ( !(i_am_state || i_am_monitor) )
      return;

   func = buf[0] & 0x0f;
   if ( func != 7 )
      return;
   actfunc = genfunc = func;
   afuncmap = (1 << ExtendedTrig);

   hcode = (buf[0] & 0xf0u) >> 4;
   ucode = buf[1] & 0x0fu;

   subunit = (buf[1] & 0xf0u) >> 4;
   /* Subunit states are not supported */
   if ( subunit > 0 ) 
      return;
	      
   level = buf[2] & 0x1f;
   limit = 25;
   xfunc = buf[3];
   bitmap = 1 << ucode;
   trigaddr = bitmap;

   x10state[hcode].lastcmd = func;
//   x10state[hcode].state[ChgState] = 0;
   changestate = 0;
   x10state[hcode].lastunit = code2unit(ucode);
   x10global.lasthc = hcode;

   x10state[hcode].reset = 1;

   x10global.lastaddr = 0;

   onstate = x10state[hcode].state[OnState];
   dimstate = x10state[hcode].state[DimState];

   switch ( xfunc ) {
      case 0x01 :  /* Open, observe limit */
      case 0x03 :  /* Open, disregard and disable limit */
         mask = modmask[Ext0Mask][hcode];
         active = bitmap & mask;
         if ( level >= 25 ) {
            /* Full Open is level 25 */
            level = 25;
            gfuncmap = (1 << OnTrig);
            genfunc = OnFunc; /* On function */
         }
         else if ( level == 0 ) {
            /* Level 0 is Off */
            gfuncmap = (1 << OffTrig);
            genfunc = OffFunc;
         }
         else {
            /* state = DimState; */
            gfuncmap = (1 << DimTrig);
            genfunc = DimFunc;
         }

         if ( xfunc == 0x01 )
            set_limit_dimlevel(level, hcode, active);
         else {
            set_dimlevel(level, hcode, active);
            set_max_memlevel(hcode, active);
         }

         get_states(hcode, &onstate, &dimstate);
//         x10state[hcode].state[ChgState] = active &
         changestate = active &
           ((x10state[hcode].state[OnState] ^ onstate) |
            (x10state[hcode].state[DimState] ^ dimstate));
         x10state[hcode].state[OnState]  = onstate;
         x10state[hcode].state[DimState] = dimstate;

         break;

      case 0x02 :  /* Set limit and open to that limit */
         level = min(level, 25);
         gfuncmap = 0;
         genfunc = 0;
         mask = modmask[Ext0Mask][hcode];
         active = bitmap & mask;
         get_states(hcode, &onstate, &dimstate);
         changestate = active &
           ((x10state[hcode].state[OnState] ^ onstate) |
            (x10state[hcode].state[DimState] ^ dimstate));
         set_memlevel(level, hcode, active);
         restore_dimlevel(hcode, active);
         break;

      case 0x04 :  /* Open all shutters, disable limit */
         gfuncmap = (1 << OnTrig);
         genfunc = OnFunc;
         mask = modmask[Ext0Mask][hcode];
         trigaddr = 0xffff;
         active = modmask[Ext0Mask][hcode];
//         x10state[hcode].state[ChgState] = active &
         changestate = active &
            (~x10state[hcode].state[OnState] | x10state[hcode].state[DimState]);
         x10state[hcode].state[OnState] |= active;
         x10state[hcode].state[DimState] &= ~active;
         set_max_memlevel(hcode, active);
         break;

      case 0x0B :  /* Close all shutters */
         gfuncmap = (1 << OffTrig);
         genfunc = OffFunc; 
         mask = modmask[Ext0Mask][hcode];
         trigaddr = 0xffff;
         active = modmask[Ext0Mask][hcode];
//         x10state[hcode].state[ChgState] = active &
         changestate = active &
            (x10state[hcode].state[OnState] | x10state[hcode].state[DimState]);
         x10state[hcode].state[OnState] &= ~active;
         x10state[hcode].state[DimState] &= ~active;
         set_dimlevel(0, hcode, active);
         break;

      default :
         /* state = OffState; */
         mask = 0;
         active = 0;
         gfuncmap = 0;
         genfunc = 0;
         break;
   }

   startupstate = ~x10state[hcode].state[ValidState] & active;
   x10state[hcode].state[ValidState] |= active;
      
//   x10state[hcode].state[ModChgState] = (x10state[hcode].state[ModChgState] & ~active) | changestate;
   x10state[hcode].state[ModChgState] = changestate;

   x10state[hcode].state[ChgState] = x10state[hcode].state[ModChgState] |
      (x10state[hcode].state[ActiveChgState] & active) |
      (startupstate & ~modmask[PhysMask][hcode]);

   changestate = x10state[hcode].state[ChgState];

   if ( bitmap & vmodmask[VtstampMask][hcode] ) {
      x10state[hcode].timestamp[ucode] = time(NULL);
   }

   if ( signal_source & RCVI )
      x10state[hcode].lastactive = 0;

   if ( i_am_state )
      write_x10state_file();

   /* Heyuhelper, if applicable */
   if ( signal_source & RCVI && configp->script_mode & HEYU_HELPER ) {
      launch_heyuhelper(hcode, trigaddr, func);
      return;
   }

   bmaplaunch = 0;
   launched = 0;
   j = 0;
   while ( launcherp && launcherp[j].line_no > 0 ) {
      if ( launcherp[j].type != L_NORMAL ||
           launcherp[j].hcode != hcode ||
           is_unmatched_flags(&launcherp[j])  ) {
         j++;
	 continue;
      }
      if ( (launcherp[j].afuncmap & afuncmap || launcherp[j].gfuncmap & gfuncmap) &&
           launcherp[j].source & signal_source ) {
         trigactive = trigaddr & (mask | launcherp[j].signal);
         if ( (bmaplaunch = (trigactive & launcherp[j].bmaptrig) & 
//                   (x10state[hcode].state[ChgState] | ~launcherp[j].chgtrig)) ) {
                   (changestate | ~launcherp[j].chgtrig)) ) {
            *launchp = j;
            launcherp[j].matched = YES;
            launcherp[j].actfunc = actfunc;
            launcherp[j].genfunc = genfunc;
	    launcherp[j].xfunc = xfunc;
	    launcherp[j].level = level;
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

   return;
}

#else

/*----------------------------------------------------------------------------+
 | Dummy stub.                                                                |
 +----------------------------------------------------------------------------*/
void x10state_update_ext0func ( unsigned char *buf, int *launchp )
{
   *launchp = -1;
   return;
}

#endif  /* End of HASEXT0 block */

/*----------------------------------------------------------------------------+
 | Handler for extended code type/function which are otherwise undefined.     |
 |                                                                            |
 | The received signal and state are tested to see if any of the conditions   |
 | exist for triggering the launching of an external script, and if so, the   |
 | index of the launcher is passed back through argument 'launchp',           | 
 | otherwise -1 is passed back.                                               |
 +----------------------------------------------------------------------------*/
void x10state_update_extotherfunc ( unsigned char *buf, int *launchp )
{
   unsigned char hcode, func, xfunc, ucode, subunit, level;
   unsigned char actfunc, genfunc;
   unsigned int  bitmap, trigaddr, mask, trigactive, active;
   unsigned int  changestate, startupstate;
   unsigned int  bmaplaunch, launched;
   unsigned long afuncmap, gfuncmap;
   int           j;
   LAUNCHER      *launcherp;
   extern unsigned int signal_source;

   launcherp = configp->launcherp;

   *launchp = -1;

   if ( !(i_am_state || i_am_monitor) )
      return;

   func = buf[0] & 0x0f;
   if ( func != 7 )
      return;
   actfunc = genfunc = func;
   afuncmap = (1 << ExtendedTrig);

   hcode = (buf[0] & 0xf0u) >> 4;
   ucode = buf[1] & 0x0fu;

   subunit = (buf[1] & 0xf0u) >> 4;
   /* Subunit states are not supported */
   if ( subunit > 0 ) 
      return;
	      
   level = buf[2];
   xfunc = buf[3];
   bitmap = 1 << ucode;
   trigaddr = bitmap;

   x10state[hcode].lastcmd = func;
//   x10state[hcode].state[ChgState] = 0;
   changestate = 0;
   x10state[hcode].lastunit = code2unit(ucode);
   x10global.lasthc = hcode;

   x10state[hcode].reset = 1;

   x10global.lastaddr = 0;

   mask = 0;
   gfuncmap = 0;
   genfunc = 0;
   active = bitmap;

   startupstate = ~x10state[hcode].state[ValidState] & active;
   x10state[hcode].state[ValidState] |= active;
    
//   x10state[hcode].state[ModChgState] = (x10state[hcode].state[ModChgState] & ~active) | changestate;
   x10state[hcode].state[ModChgState] = changestate;

   x10state[hcode].state[ChgState] = x10state[hcode].state[ModChgState] |
      (x10state[hcode].state[ActiveChgState] & active) |
      (startupstate & ~modmask[PhysMask][hcode]);

   changestate = x10state[hcode].state[ChgState];

   if ( bitmap & vmodmask[VtstampMask][hcode] ) {
      x10state[hcode].timestamp[ucode] = time(NULL);
   }

   if ( signal_source & RCVI )
      x10state[hcode].lastactive = 0;

   if ( i_am_state )
      write_x10state_file();

   /* Heyuhelper, if applicable */
   if ( signal_source & RCVI && configp->script_mode & HEYU_HELPER ) {
      launch_heyuhelper(hcode, trigaddr, func);
      return;
   }

   bmaplaunch = 0;
   launched = 0;
   j = 0;
   while ( launcherp && launcherp[j].line_no > 0 ) {
      if ( launcherp[j].type != L_NORMAL ||
           launcherp[j].hcode != hcode ||
           is_unmatched_flags(&launcherp[j])  ) {
         j++;
	 continue;
      }
      if ( (launcherp[j].afuncmap & afuncmap || launcherp[j].gfuncmap & gfuncmap) &&
           launcherp[j].source & signal_source ) {
         trigactive = trigaddr & (mask | launcherp[j].signal);
         if ( (bmaplaunch = (trigactive & launcherp[j].bmaptrig) & 
//                   (x10state[hcode].state[ChgState] | ~launcherp[j].chgtrig)) ) {
                   (changestate | ~launcherp[j].chgtrig)) ) {
            *launchp = j;
            launcherp[j].matched = YES;
            launcherp[j].actfunc = actfunc;
            launcherp[j].genfunc = genfunc;
	    launcherp[j].xfunc = xfunc;
	    launcherp[j].level = level;
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

   return;
}

/*----------------------------------------------------------------------------+
 | Update the x10state structure per the contents of the argument 'buf'       |
 | for virtual modules.  'buf' will contains 6 bytes.  The first is the       |
 | standard hcode|ucode byte, the second is the data 0x00-0xff, the third is  |
 | the virtual type, the fourth is the module ID byte.                        |
 |                                                                            |
 | Only modules with attribute VIRTUAL will be updated.                       |
 |                                                                            |
 | The received signal and state are tested to see if any of the conditions   |
 | exist for triggering the launching of an external script, and if so, the   |
 | index of the launcher is passed back through argument 'launchp',           | 
 | otherwise -1 is passed back.                                               |
 +----------------------------------------------------------------------------*/
int x10state_update_virtual ( unsigned char *buf, int len, int *launchp )
{
   unsigned char  hcode, func, xfunc, ucode, vdata, vtype, hibyte, lobyte;
//   unsigned short ident, idmask;
   unsigned long  ident, idmask;
   unsigned char  actfunc, genfunc;
   unsigned int   bitmap, trigaddr, mask, active, trigactive;
   unsigned int   changestate, startupstate;
   unsigned int   bmaplaunch, launched;
   unsigned long  afuncmap = 0, gfuncmap = 0, sfuncmap = 0;
   unsigned long  sflags, vflags;
   struct xlate_vdata_st xlate_vdata;
   int            j, index, trig;
   char           hc;
   LAUNCHER       *launcherp;
   ALIAS          *aliasp;
   extern unsigned int signal_source;

   launcherp = configp->launcherp;

   *launchp = -1;

   aliasp = config.aliasp;

   genfunc = 0;
   gfuncmap = 0;
   vflags = 0;

   hcode  = (buf[0] & 0xf0u) >> 4;
   ucode  = buf[0] & 0x0fu;
   vdata  = buf[1];
   vtype  = buf[2];
   ident  = buf[3] | (buf[4] << 8);
   hibyte = buf[5];
   lobyte = buf[6];

   if ( vtype == RF_VDATAM ) {
      func = VdataMFunc;
      trig = VdataMTrig;
   }
   else {
      func = VdataFunc;
      trig = VdataTrig;
   }

   bitmap = 1 << ucode;
   hc = code2hc(hcode);

   idmask = (vtype == RF_SEC) ? configp->securid_mask : 0xffffu;

   index = alias_rev_index(hc, bitmap, vtype, (ident & idmask));

   /* Run the decoding function for the module type, if any */
   if ( index >= 0 &&
        aliasp[index].modtype >= 0 && aliasp[index].xlate_func != NULL ) {
      xlate_vdata.vdata = vdata;
      xlate_vdata.hibyte = hibyte;
      xlate_vdata.lobyte = lobyte;
      xlate_vdata.hcode = hcode;
      xlate_vdata.ucode = ucode;
      xlate_vdata.ident = ident & idmask;
      xlate_vdata.nident = aliasp[index].nident;
      xlate_vdata.identp = aliasp[index].ident;
      xlate_vdata.optflags = aliasp[index].optflags;
      xlate_vdata.optflags2 = aliasp[index].optflags2;
      /* Tamper flag is sticky */
      xlate_vdata.vflags = x10state[hcode].vflags[ucode] & SEC_TAMPER;

      /* Run the translation function */
      if ( aliasp[index].xlate_func(&xlate_vdata) != 0 )
         return 1;
      func = xlate_vdata.func;
      xfunc = xlate_vdata.xfunc;
      vflags = xlate_vdata.vflags;
      trig = xlate_vdata.trig;
   }
   
   x10state[hcode].vaddress = bitmap;
   x10state[hcode].lastcmd = func;
   x10state[hcode].lastunit = code2unit(ucode);
   x10state[hcode].vident[ucode] = ident & idmask;
   x10state[hcode].vflags[ucode] = vflags;
   x10state[hcode].timestamp[ucode] = time(NULL);
//   x10state[hcode].state[ValidState] |= (1 << ucode);
//   x10state[hcode].state[ChgState] = 0;
   changestate = 0;
   x10global.lasthc = hcode;
   x10global.lastaddr = 0;

   if ( vflags & SEC_LOBAT ) {
      x10state[hcode].state[LoBatState] |= bitmap;
   }
   else {
      x10state[hcode].state[LoBatState] &= ~bitmap;
   }

   if ( (vflags & SEC_AUX) || (func == VdataMFunc) ) {
      /* Special for DS90 and other dual signal devices, or vdatam command */
      if ( vdata != x10state[hcode].memlevel[ucode] ) {
//         x10state[hcode].state[ChgState] |= bitmap;
         changestate |= bitmap;
         x10state[hcode].memlevel[ucode] = vdata;
      }
   }
   else {
      if ( vdata != x10state[hcode].dimlevel[ucode] ) {
//         x10state[hcode].state[ChgState] |= bitmap;
         changestate |= bitmap;
         x10state[hcode].dimlevel[ucode] = vdata;
      }
   }

#if 0
   if ( vtype == RF_SEC ) {
      if ( vflags & SEC_MIN ) {
         x10state[hcode].state[SwMinState] |= bitmap;
         x10state[hcode].state[SwMaxState] &= ~bitmap;
      }
      else if ( vflags & SEC_MAX ) {
         x10state[hcode].state[SwMaxState] |= bitmap;
         x10state[hcode].state[SwMinState] &= ~bitmap;
      }
   }
#endif

   if ( func == AlertFunc ) {
      if ( vflags & SEC_AUX ) {
         x10state[hcode].state[AuxAlertState] |= bitmap;
         x10state[hcode].state[AuxClearState] &= ~bitmap;
      }
      else {
         x10state[hcode].state[AlertState] |= bitmap;
         x10state[hcode].state[ClearState] &= ~bitmap;
      }
   }
   else if ( func == ClearFunc ) {
      if ( vflags & SEC_AUX ) {
         x10state[hcode].state[AuxAlertState] &= ~bitmap;
         x10state[hcode].state[AuxClearState] |= bitmap;
      }
      else {
         x10state[hcode].state[AlertState] &= ~bitmap;
         x10state[hcode].state[ClearState] |= bitmap;
      }
   }
   else if ( func == DuskFunc ) {
      x10state[hcode].state[AuxAlertState] |= bitmap;
      x10state[hcode].state[AuxClearState] &= ~bitmap;
   }
   else if ( func == DawnFunc ) {
      x10state[hcode].state[AuxAlertState] &= ~bitmap;
      x10state[hcode].state[AuxClearState] |= bitmap;
   }

   if ( func == DisarmFunc ) {
      set_globsec_flags(0);
   }
   else if ( func == ArmFunc ) {
      if ( configp->arm_remote == AUTOMATIC || !(signal_source & RCVA) ) {
         sflags = (vflags & SEC_HOME) ? GLOBSEC_HOME : 0;
         sflags |= ((vflags & SEC_MIN) || config.arm_max_delay == 0) ? GLOBSEC_ARMED : GLOBSEC_PENDING;
         set_globsec_flags(sflags >> GLOBSEC_SHIFT);
      }
   }
   else if ( func == SecLightsOnFunc )
      set_seclights_flag(1);
   else if ( func == SecLightsOffFunc )
      set_seclights_flag(0);

   if ( func == TamperFunc ) {
      set_tamper_flag(1);
      x10state[hcode].vflags[ucode] |= SEC_TAMPER;
      x10state[hcode].state[TamperState] |= (1 << ucode);
   }


   actfunc = func;
   trigaddr = bitmap;
   if ( vtype == RF_SEC )
      sfuncmap = (1 << trig);
   else
      afuncmap = (1 << trig);

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


   if ( signal_source & RCVI )
      x10state[hcode].lastactive = 0;

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
 
      if ( (launcherp[j].afuncmap & afuncmap || launcherp[j].gfuncmap & gfuncmap || launcherp[j].sfuncmap & sfuncmap) &&
           launcherp[j].source & signal_source ) {
         trigactive = trigaddr & (mask | launcherp[j].signal);
         if ( (bmaplaunch = (trigactive & launcherp[j].bmaptrig) &
#if 0 
                   (x10state[hcode].state[ChgState] | ~launcherp[j].chgtrig)) ||
#endif
                   (changestate | ~launcherp[j].chgtrig)) ||
                   (launcherp[j].unitstar && !trigaddr)) {
            *launchp = j;
            launcherp[j].matched = YES;
            launcherp[j].actfunc = actfunc;
            launcherp[j].genfunc = genfunc;
	    launcherp[j].xfunc = 0;
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
 | Handle the special case of type RF_DUSK sensors.                           |
 +----------------------------------------------------------------------------*/
int x10state_update_duskdawn ( unsigned char *buf, int len, int *launchp )
{
   unsigned char  hcode, ucode, func, vdata;
   unsigned char  actfunc, genfunc;
   unsigned int   bitmap, trigaddr, mask, active, trigactive, vflags;
   unsigned int   bmaplaunch, launched;
   unsigned int   changestate, startupstate;
   unsigned long  afuncmap, gfuncmap;
   int            j, trig;
   char           hc;
   LAUNCHER       *launcherp;
   extern unsigned int signal_source;

   launcherp = configp->launcherp;

   *launchp = -1;

   genfunc = 0;
   gfuncmap = 0;
   vflags = 0;
   hcode  = (buf[0] & 0xf0u) >> 4;
   ucode = buf[0] & 0x0fu;
   vdata  = buf[1];

   bitmap = (1 << ucode);
   hc = code2hc(hcode);

   changestate = (x10state[hcode].dimlevel[ucode] == vdata) ? 0 : bitmap;

   if ( vdata == 0xf0u ) {
      func = DuskFunc;
      trig = DuskTrig;
   }
   else if ( vdata == 0xf8u ) {
      func = DawnFunc;
      trig = DawnTrig;
   }
   else if ( vdata == 0xe0u ) {
      func = AlertFunc;
      trig = AlertTrig;
   }
   else {
      func = VdataFunc;
      trig = VdataTrig;
   }

   
   x10state[hcode].vaddress = 0;
   x10state[hcode].lastcmd = func;
//   x10state[hcode].state[ChgState] = 0;
   x10global.lasthc = hcode;
   x10global.lastaddr = 0;

   actfunc = func;
   afuncmap = (1 << trig);
   trigaddr = bitmap;
   mask = 0;

   active = bitmap;

   startupstate = ~x10state[hcode].state[ValidState] & active;
   x10state[hcode].state[ValidState] |= active;

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
           is_unmatched_flags(&launcherp[j])  ) {
         j++;
	 continue;
      }

      if ( (launcherp[j].afuncmap & afuncmap || launcherp[j].gfuncmap & gfuncmap) &&
           launcherp[j].source & signal_source ) {
         trigactive = trigaddr & (mask | launcherp[j].signal);
         if ( (bmaplaunch = (trigactive & launcherp[j].bmaptrig) & 
//                   (x10state[hcode].state[ChgState] | ~launcherp[j].chgtrig)) ||
                   (changestate | ~launcherp[j].chgtrig)) ||
                   (launcherp[j].unitstar && !trigaddr)) {
            *launchp = j;
            launcherp[j].matched = YES;
            launcherp[j].actfunc = actfunc;
            launcherp[j].genfunc = genfunc;
	    launcherp[j].xfunc = 0;
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

/*---------------------------------------------------------------------+
 | Execute all hourly scripts                                          |
 +---------------------------------------------------------------------*/
int launch_hourly_scripts ( void )
{
   LAUNCHER      *launcherp;
   int           j, launchp = -1;
   int           exec_script ( LAUNCHER * );
   mode_t        oldumask;

   if ( (launcherp = configp->launcherp) == NULL ||
        (configp->script_mode & HEYU_HELPER)  )
      return -1;

   if ( !i_am_state )
      return -1;

   /* Redirect output to the Heyu log file */
   oldumask = umask((mode_t)configp->log_umask);
   fdsout = freopen(configp->logfile, "a", stdout);
   fdserr = freopen(configp->logfile, "a", stderr);
   umask(oldumask);

   j = 0;
   while ( launcherp[j].line_no > 0 ) {
      if ( (launcherp[j].type != L_HOURLY) ||
           is_unmatched_flags(&launcherp[j])  ) {
         j++;
         continue;
      }
      launcherp[j].matched = YES;
      launchp = j;

      if ( launcherp[j].scanmode & FM_BREAK )
         break;
      j++;
   }

   return launch_scripts(&launchp);

   return 0;
}

/*---------------------------------------------------------------------+
 | Launch -rfflood scripts which match the launch conditions.          |
 +---------------------------------------------------------------------*/
int find_rfflood_scripts ( void )
{
   LAUNCHER      *launcherp;
   int           j, launchp = -1;

   if ( (launcherp = configp->launcherp) == NULL ||
        (configp->script_mode & HEYU_HELPER)  )
      return -1;

   if ( !i_am_state )
      return -1;

   j = 0;
   while ( launcherp[j].line_no > 0 ) {
      if ( (launcherp[j].type != L_RFFLOOD) ||
           is_unmatched_flags(&launcherp[j])  ) {
         j++;
         continue;
      }
      launcherp[j].matched = YES;
      launchp = j;
      if ( launcherp[j].scanmode == FM_BREAK )
         break;
      j++;
   }
   return launchp;
}

/*---------------------------------------------------------------------+
 | Launch -rfxjam scripts which match the launch conditions.           |
 +---------------------------------------------------------------------*/
int find_rfxjam_scripts ( void )
{
   LAUNCHER      *launcherp;
   int           j, launchp = -1;

   if ( (launcherp = configp->launcherp) == NULL ||
        (configp->script_mode & HEYU_HELPER)  )
      return -1;

   if ( !i_am_state )
      return -1;

   j = 0;
   while ( launcherp[j].line_no > 0 ) {
      if ( (launcherp[j].type != L_RFXJAM) ||
           is_unmatched_flags(&launcherp[j])  ||
           (launcherp[j].vflags & x10global.vflags) != launcherp[j].vflags     ||
           (launcherp[j].notvflags & ~x10global.vflags) != launcherp[j].notvflags ) {
         j++;
         continue;
      }
      launcherp[j].matched = YES;
      launchp = j;
      if ( launcherp[j].scanmode == FM_BREAK )
         break;
      j++;
   }
   return launchp;
}

/*---------------------------------------------------------------------+
 | Launch -lockup scripts which match the launch conditions.           |
 +---------------------------------------------------------------------*/
int find_lockup_scripts ( unsigned char bootflag )
{
   LAUNCHER      *launcherp;
   int           j, launchp = -1;

   if ( (launcherp = configp->launcherp) == NULL ||
        (configp->script_mode & HEYU_HELPER)  )
      return -1;

   if ( !i_am_state )
      return -1;

   j = 0;
   while ( launcherp[j].line_no > 0 ) {
      if ( (launcherp[j].type != L_LOCKUP) ||
            is_unmatched_flags(&launcherp[j])  ) {
         j++;
         continue;
      }
      launcherp[j].matched = YES;
      launchp = j;
      if ( launcherp[j].scanmode & FM_BREAK )
         break;
      j++;
   }
   return launchp;
}
  
/*---------------------------------------------------------------------+
 | Launch powerfail scripts which match the launch conditions          |
 +---------------------------------------------------------------------*/
int find_powerfail_scripts ( unsigned char bootflag )
{
   LAUNCHER      *launcherp;
   int           j, launchp = -1;

   if ( (launcherp = configp->launcherp) == NULL ||
        (configp->script_mode & HEYU_HELPER)  )
      return -1;

   if ( !i_am_state )
      return -1;

   j = 0;
   while ( launcherp[j].line_no > 0 ) {
      if ( (launcherp[j].type != L_POWERFAIL) ||
            is_unmatched_flags(&launcherp[j])  ||
           (launcherp[j].bootflag & bootflag) != launcherp[j].bootflag ) {
         j++;
         continue;
      }
      launcherp[j].matched = YES;
      launchp = j;
      if ( launcherp[j].scanmode & FM_BREAK )
         break;
      j++;
   }

   return launchp;
}

/*---------------------------------------------------------------------+
 | Display the detailed state of each unit on the argument X10-encoded |
 | housecode, i.e., Addressed, On, Dimmed, Changed.                    |
 +---------------------------------------------------------------------*/
void x10state_show ( unsigned char hcode ) 
{
   char lasthc;
   unsigned char lastcmd;
   unsigned int lastunitbmap;
   char minibuf[32];
   char *chrs = ".*";
   int  lw = 13;
   char *bmap2asc2 ( unsigned int, unsigned int, char * );

   lastcmd = x10state[hcode].lastcmd;
   if ( lastcmd == 0xff )
      sprintf(minibuf, "_none_");
   else 
      sprintf(minibuf, "%s", funclabel[lastcmd]);

   lastunitbmap = (x10state[hcode].lastunit > 0) ?
       (1 << unit2code(x10state[hcode].lastunit)) : 0;
 
   lasthc = (x10global.lasthc == 0xff) ? '_' : code2hc(x10global.lasthc);

   printf("%*s %c\n", lw + 13, "Housecode", code2hc(hcode));
   printf("%*s  1..4...8.......16\n", lw, "Unit:");
   printf("%*s (%s)\n", lw,    "LastUnit", bmap2asc(lastunitbmap, ".u"));
   printf("%*s (%s)\n", lw,   "Addressed", bmap2asc2(x10state[hcode].vaddress,
                                                      x10state[hcode].addressed, ".va"));
   printf("%*s (%s)\n", lw,          "On", bmap2asc(x10state[hcode].state[OnState], chrs));
   printf("%*s (%s)\n", lw,      "Dimmed", bmap2asc(x10state[hcode].state[DimState], ".x"));
   printf("%*s (%s)\n", lw,     "Changed", bmap2asc(x10state[hcode].state[ChgState], ".c"));
   printf("%*s (%s)\n", lw,    "Launched", bmap2asc(x10state[hcode].launched, chrs));
   printf("Last function this housecode: %s\n", minibuf);
   printf("Last Housecode = %c\n", lasthc);
   printf("\n");
   fflush(stdout);
   return;
}

/*---------------------------------------------------------------------+
 | Return a 16 character ASCII string displaying in ascending order    |
 | an X10 unit bitmap, i.e., char[0] -> unit 1, char[15] -> unit 16.   |
 | The argument chrs is a three-character string, the 1st character of |
 | represents 'unset' units and the 2nd character the 'set' bits in    |
 | bitmap1.  The 3rd character will overwrite the 2nd character for  ` |
 | set bits in bitmap2                                                 |
 +---------------------------------------------------------------------*/
char *bmap2asc2 ( unsigned int bitmap1, unsigned int bitmap2, char *chrs )
{
   int j;
   static char outbuf[17];

   for ( j = 0; j < 16; j++ ) {
      if ( bitmap1 & (1 << j) )
         outbuf[code2unit(j) - 1] = chrs[1];
      else
         outbuf[code2unit(j) - 1] = chrs[0];
   }
   for ( j = 0; j < 16; j++ ) {
      if ( bitmap2 & (1 << j) )
         outbuf[code2unit(j) - 1] = chrs[2];
   }
   outbuf[16] = '\0';
   return outbuf;
}

/*---------------------------------------------------------------------+
 | Return a 16 character ASCII string displaying in ascending order    |
 | a 16-bit linear bitmap.                                             |
 | The argument chrs is a three-character string, the 1st character of |
 | represents 'unset' units and the 2nd character the 'set' bits in    |
 | bitmap1.  The 3rd character will overwrite the 2nd character for  ` |
 | set bits in bitmap2                                                 |
 +---------------------------------------------------------------------*/
char *linmap2asc2 ( unsigned int bitmap1, unsigned int bitmap2, char *chrs )
{
   int j;
   static char outbuf[17];

   for ( j = 0; j < 16; j++ ) {
      if ( bitmap1 & (1 << j) )
         outbuf[j] = chrs[1];
      else
         outbuf[j] = chrs[0];
   }
   for ( j = 0; j < 16; j++ ) {
      if ( bitmap2 & (1 << j) )
         outbuf[j] = chrs[2];
   }
   outbuf[16] = '\0';
   return outbuf;
}

/*---------------------------------------------------------------------+
 | Return a 16 character ASCII string displaying a state bitmap for    |
 | the unit corresponding to the index in the array, i.e. char[0] =    |
 | unit 1; char[1] = unit 2; etc.                                      |
 | The bits in each bitmap have the following meanings:                |
 |   Bit 0 : Addressed                                                 |
 |   Bit 1 : Changed                                                   |
 |   Bit 2 : Dimmed                                                    |
 |   Bit 3 : On                                                        |
 | The arguments are the unit bitmaps indicating On, Dimmed, Changed,  |
 | and Addressed, respectively.                                        |
 +---------------------------------------------------------------------*/
char *bmap2statestr ( unsigned int onmap, unsigned int dimmap,
	                     unsigned int chgmap, unsigned int addrmap )
{
  int j;
  int val;
  unsigned int bmap;
  static char outbuf[17];
  char hexdigit[] = "0123456789abcdef";

  for ( j = 0; j < 16; j++ )  {
     val = 0;
     bmap = 1 << j;
     val += (   onmap & bmap ) ? 8 : 0;
     val += (  dimmap & bmap ) ? 4 : 0;
     val += (  chgmap & bmap ) ? 2 : 0;
     val += ( addrmap & bmap ) ? 1 : 0;
     outbuf[code2unit(j) - 1] = hexdigit[val];
  }
  outbuf[16] = '\0';
  return outbuf;
}


/*---------------------------------------------------------------------+
 | Create list of SCRIPT options for launcher.                         |
 +---------------------------------------------------------------------*/
int script_option_list ( LAUNCHER *launcherp, int index, char *list )
{
   SCRIPT   *scriptp;
   unsigned char option;

   scriptp = configp->scriptp;

   option = scriptp[launcherp[index].scriptnum].script_option;
   
   *list = '\0';

   if ( !option )
      return 0;

   if ( option & SCRIPT_XTEND )
      strcat(list + strlen(list), "-xtend ");
   if ( option & SCRIPT_RAWLEVEL )
      strcat(list + strlen(list), "-rawlevel ");
   if ( option & SCRIPT_NOENV )
      strcat(list + strlen(list), "-noenv ");
   if ( option & SCRIPT_QQUIET )
      strcat(list + strlen(list), "-qquiet ");
   if ( option & SCRIPT_QUIET )
      strcat(list + strlen(list), "-quiet ");

   return 1;
}



/*---------------------------------------------------------------------+
 | Display launcher flags for specific launcher                        |
 +---------------------------------------------------------------------*/
void display_launcher_flags ( LAUNCHER *launcherp, int index )
{
   unsigned long bitmap1, bitmap2, bitmap3, bitmap4, /*bitmap5, bitmap6,*/ mask;
   unsigned int  bmap;
   unsigned char hcode;
   int           j, k;

   bitmap1 = launcherp[index].sflags;
   bitmap2 = launcherp[index].notsflags;
   bitmap3 = launcherp[index].vflags;
   bitmap4 = launcherp[index].notvflags;
//   bitmap5 = launcherp[index].czflags;
//   bitmap6 = launcherp[index].notczflags;

   /* Display state flags */

   for ( j = 0; j < launcherp[index].num_stflags; j++ ) {
      k = launcherp[index].stlist[j].stindex;
      hcode = launcherp[index].stlist[j].hcode;
      bmap = launcherp[index].stlist[j].bmap;
      printf("%s%c%s ", stflags[k].label, code2hc(hcode), bmap2units(bmap));
   }
  

   if ( launcherp[index].bootflag & R_ATSTART ) 
      printf("boot ");
   else if ( launcherp[index].bootflag & R_NOTATSTART )
      printf("notboot ");

   if ( bitmap3 & SEC_MIN         ) printf("swmin ");
   if ( bitmap3 & SEC_MAX         ) printf("swmax ");
   if ( bitmap3 & SEC_HOME        ) printf("swhome ");
   if ( bitmap3 & SEC_AWAY        ) printf("swaway ");
   if ( bitmap3 & SEC_LOBAT       ) printf("lobat ");
   if ( launcherp[index].notvflags & SEC_LOBAT )
      printf("notlobat ");
   if ( bitmap3 & RFX_JAM         ) printf("started ");
   if ( bitmap4 & RFX_JAM         ) printf("ended ");
   if ( bitmap3 & SEC_MAIN        ) printf("main ");
   if ( bitmap3 & SEC_AUX         ) printf("aux ");
   if ( bitmap3 & RFX_ROLLOVER    ) printf("rollover ");
   if ( bitmap3 & ORE_TMIN        ) printf("tmin ");
   if ( bitmap3 & ORE_TMAX        ) printf("tmax ");
   if ( bitmap3 & ORE_RHMIN       ) printf("rhmin ");
   if ( bitmap3 & ORE_RHMAX       ) printf("rhmax ");
   if ( bitmap3 & ORE_BPMIN       ) printf("bpmin ");
   if ( bitmap3 & ORE_BPMAX       ) printf("bpmax ");

   if ( bitmap3 & DMX_SET         ) printf("set ");
   if ( bitmap4 & DMX_SET         ) printf("notset ");
   if ( bitmap3 & DMX_HEAT        ) printf("heat ");
   if ( bitmap4 & DMX_HEAT        ) printf("cool ");
   if ( bitmap3 & DMX_INIT        ) printf("init ");
   if ( bitmap4 & DMX_INIT        ) printf("notinit ");
   if ( bitmap3 & DMX_TEMP        ) printf("temp ");
   if ( bitmap4 & DMX_TEMP        ) printf("nottemp ");
           
   if ( bitmap1 & NIGHT_FLAG      ) printf("night ");
   if ( bitmap2 & NIGHT_FLAG      ) printf("notnight ");
   if ( bitmap1 & DARK_FLAG       ) printf("dark ");
   if ( bitmap2 & DARK_FLAG       ) printf("notdark ");
   if ( bitmap1 & GLOBSEC_TAMPER  ) printf("tamper ");
   if ( bitmap1 & GLOBSEC_FLOOD   ) printf("started ");
   if ( bitmap2 & GLOBSEC_FLOOD   ) printf("ended ");
   if ( bitmap1 & GLOBSEC_ARMED   ) printf("armed ");
   if ( bitmap1 & GLOBSEC_PENDING ) printf("armpending ");
   if ((bitmap2 & GLOBSEC_ARMED) &&
       (bitmap2 & GLOBSEC_PENDING)) printf("disarmed ");
   else if
      ( bitmap2 & GLOBSEC_ARMED   ) printf("notarmed ");
   if ( bitmap1 & GLOBSEC_HOME    ) printf("home ");
   if ( bitmap2 & GLOBSEC_HOME    ) printf("away ");

   for ( j = 0; j < NUM_FLAG_BANKS; j++ ) {
      bitmap1 = launcherp[index].flags[j];
      bitmap2 = launcherp[index].notflags[j];
      mask = 1;
      for ( k = 0; k < 32; k++ ) {
         if ( bitmap1 & mask ) printf("flag%d ", (32 * j) + k + 1);
         if ( bitmap2 & mask ) printf("notflag%d ", (32 * j) + k + 1);
         mask = mask << 1;
      }
   }

   for ( j = 0; j < NUM_COUNTER_BANKS; j++ ) {
      bitmap1 = launcherp[index].czflags[j];
      bitmap2 = launcherp[index].notczflags[j];
      mask = 1;
      for ( k = 0; k < 32; k++ ) {
         if ( bitmap1 & mask ) printf("czflag%d ", (32 * j) + k + 1);
         if ( bitmap2 & mask ) printf("notczflag%d ", (32 * j) + k + 1);
         mask = mask << 1;
      }
   }

   for ( j = 0; j < NUM_TIMER_BANKS; j++ ) {
      bitmap1 = launcherp[index].tzflags[j];
      bitmap2 = launcherp[index].nottzflags[j];
      mask = 1;
      for ( k = 0; k < 32; k++ ) {
         if ( bitmap1 & mask ) printf("tzflag%d ", (32 * j) + k + 1);
         if ( bitmap2 & mask ) printf("nottzflag%d ", (32 * j) + k + 1);
         mask = mask << 1;
      }
   }

   return;
}

/*---------------------------------------------------------------------+
 | Display launcher sources for specific launcher                      |
 +---------------------------------------------------------------------*/
void display_launcher_sources ( LAUNCHER *launcherp, int index )
{
   unsigned int source;

   source = launcherp[index].source;

   if ( source & RCVI )  printf("rcvi ");
   if ( source & RCVT )  printf("rcvt ");
   if ( source & SNDC )  printf("sndc ");
   if ( source & SNDM )  printf("sndm ");
   if ( source & SNDS )  printf("snds ");
   if ( source & SNDT )  printf("sndt ");
   if ( source & SNDP )  printf("sndp ");
   if ( source & SNDA )  printf("snda ");
   if ( source & RCVA )  printf("rcva ");
   if ( source & XMTF )  printf("xmtf ");

   return;
}

/*---------------------------------------------------------------------+
 | Display all script -sensorfail launch conditions.                   |
 +---------------------------------------------------------------------*/
void show_sensorfail_launcher ( void )
{
   int           j, lw = 13;
   LAUNCHER      *launcherp;
   char          list[80];
   char          *flowstr;

   if ( (launcherp = configp->launcherp) == NULL ) {
      return;
   }

   j = 0;
   while ( launcherp[j].line_no > 0 ) {
      if ( launcherp[j].type == L_SENSORFAIL ) {
         printf("\n");
         flowstr = (configp->scanmode == launcherp[j].scanmode) ? "" : 
                   (launcherp[j].scanmode & FM_BREAK) ? "break" : "continue";
         sprintf(list, "-sensorfail %s", flowstr);
         printf("%*s: %-20s", lw, "Launch Mode", list);

         printf("%s: ", "Flags");
         display_launcher_flags(launcherp, j);

         printf("\n");
         if ( configp->scriptp[launcherp[j].scriptnum].num_launchers > 1 ) {
            sprintf(list, "%s [%d]", launcherp[j].label, launcherp[j].launchernum); 
            printf("%*s: %-18s\n", lw, "Script label", list);
         }
         else {
            printf("%*s: %-18s\n", lw, "Script label", launcherp[j].label);
         }

         if ( script_option_list(launcherp, j, list) > 0 )
            printf("%*s: %s\n", lw, "Script opt", list);

         printf("%*s: %s\n", lw, "Command Line",
		  configp->scriptp[launcherp[j].scriptnum].cmdline);

      }
      j++;
   }
   return;
}

/*---------------------------------------------------------------------+
 | Display all script -timout launch conditions.                       |
 +---------------------------------------------------------------------*/
void show_timeout_launcher ( void )
{
   int           j, lw = 13;
   LAUNCHER      *launcherp;
   char          list[80];
   char          *flowstr;

   if ( (launcherp = configp->launcherp) == NULL ) {
      return;
   }

   j = 0;
   while ( launcherp[j].line_no > 0 ) {
      if ( launcherp[j].type == L_TIMEOUT ) {
         printf("\n");
         flowstr = (configp->scanmode == launcherp[j].scanmode) ? "" : 
                   (launcherp[j].scanmode & FM_BREAK) ? "break" : "continue";
         sprintf(list, "-timeout %s", flowstr);
         printf("%*s: %-20s", lw, "Launch Mode", list);

         if ( launcherp[j].timer == 0 )
            printf("%s: armdelay ", "Flags");
         else
            printf("%s: timer%-2d ", "Flags", launcherp[j].timer);

         display_launcher_flags(launcherp, j);

         printf("\n");
         if ( configp->scriptp[launcherp[j].scriptnum].num_launchers > 1 ) {
            sprintf(list, "%s [%d]", launcherp[j].label, launcherp[j].launchernum); 
            printf("%*s: %-18s\n", lw, "Script label", list);
         }
         else {
            printf("%*s: %-18s\n", lw, "Script label", launcherp[j].label);
         }

         if ( script_option_list(launcherp, j, list) > 0 )
            printf("%*s: %s\n", lw, "Script opt", list);

         printf("%*s: %s\n", lw, "Command Line",
		  configp->scriptp[launcherp[j].scriptnum].cmdline);

      }
      j++;
   }
   return;
}

/*---------------------------------------------------------------------+
 | Display all script -rfflood launch conditions.                      |
 +---------------------------------------------------------------------*/
void show_rfflood_launcher ( void )
{
   int           j, lw = 13;
   LAUNCHER      *launcherp;
   char          list[80];
   char          *flowstr;

   if ( (launcherp = configp->launcherp) == NULL ) {
      return;
   }

   j = 0;
   while ( launcherp[j].line_no > 0 ) {
      if ( launcherp[j].type == L_RFFLOOD ) {
         printf("\n");
         flowstr = (configp->scanmode == launcherp[j].scanmode) ? "" : 
                   (launcherp[j].scanmode & FM_BREAK) ? "break" : "continue";
         sprintf(list, "-rfflood %s", flowstr);
         printf("%*s: %-20s", lw, "Launch Mode", list);

         printf("%s: ", "Flags");
         display_launcher_flags(launcherp, j);
         printf("\n");

         if ( configp->scriptp[launcherp[j].scriptnum].num_launchers > 1 ) {
            sprintf(list, "%s [%d]", launcherp[j].label, launcherp[j].launchernum); 
            printf("%*s: %-18s\n", lw, "Script label", list);
         }
         else {
            printf("%*s: %-18s\n", lw, "Script label", launcherp[j].label);
         }

         if ( script_option_list(launcherp, j, list) > 0 )
            printf("%*s: %s\n", lw, "Script opt", list);

         printf("%*s: %s\n", lw, "Command Line",
		  configp->scriptp[launcherp[j].scriptnum].cmdline);

      }
      j++;
   }
   return;
}

/*---------------------------------------------------------------------+
 | Display all script -rfxjam launch conditions.                       |
 +---------------------------------------------------------------------*/
void show_rfxjam_launcher ( void )
{
   int           j, lw = 13;
   LAUNCHER      *launcherp;
   char          list[80];
   char          *flowstr;

   if ( (launcherp = configp->launcherp) == NULL ) {
      return;
   }

   j = 0;
   while ( launcherp[j].line_no > 0 ) {
      if ( launcherp[j].type == L_RFXJAM ) {
         printf("\n");
         flowstr = (configp->scanmode == launcherp[j].scanmode) ? "" : 
                   (launcherp[j].scanmode & FM_BREAK) ? "break" : "continue";
         sprintf(list, "-rfxjam %s", flowstr);
         printf("%*s: %-20s", lw, "Launch Mode", list);

         printf("%s: ", "Flags");
         display_launcher_flags(launcherp, j);
         printf("\n");

         if ( configp->scriptp[launcherp[j].scriptnum].num_launchers > 1 ) {
            sprintf(list, "%s [%d]", launcherp[j].label, launcherp[j].launchernum); 
            printf("%*s: %-18s\n", lw, "Script label", list);
         }
         else {
            printf("%*s: %-18s\n", lw, "Script label", launcherp[j].label);
         }

         if ( script_option_list(launcherp, j, list) > 0 )
            printf("%*s: %s\n", lw, "Script opt", list);

         printf("%*s: %s\n", lw, "Command Line",
		  configp->scriptp[launcherp[j].scriptnum].cmdline);

      }
      j++;
   }
   return;
}

/*---------------------------------------------------------------------+
 | Display all script -powerfail launch conditions.                    |
 +---------------------------------------------------------------------*/
void show_powerfail_launcher ( void )
{
   int           j, lw = 13;
   LAUNCHER      *launcherp;
   char          list[80];
   char          *flowstr;

   if ( (launcherp = configp->launcherp) == NULL ) {
      return;
   }

   j = 0;
   while ( launcherp[j].line_no > 0 ) {
      if ( launcherp[j].type == L_POWERFAIL ) {
         printf("\n");
         flowstr = (configp->scanmode == launcherp[j].scanmode) ? "" : 
                   (launcherp[j].scanmode & FM_BREAK) ? "break" : "continue";
         sprintf(list, "-powerfail %s", flowstr);
         printf("%*s: %-20s", lw, "Launch Mode", list);

         printf("%s: ", "Flags");
         display_launcher_flags(launcherp, j);
         printf("\n");

         if ( configp->scriptp[launcherp[j].scriptnum].num_launchers > 1 ) {
            sprintf(list, "%s [%d]", launcherp[j].label, launcherp[j].launchernum); 
            printf("%*s: %-18s\n", lw, "Script label", list);
         }
         else {
            printf("%*s: %-18s\n", lw, "Script label", launcherp[j].label);
         }

         if ( script_option_list(launcherp, j, list) > 0 )
            printf("%*s: %s\n", lw, "Script opt", list);

         printf("%*s: %s\n", lw, "Command Line",
		  configp->scriptp[launcherp[j].scriptnum].cmdline);

      }
      j++;
   }
   return;
}

/*---------------------------------------------------------------------+
 | Display all script -exec launch conditions.                         |
 +---------------------------------------------------------------------*/
void show_exec_launcher ( void )
{
   int           j, lw = 13;
   LAUNCHER      *launcherp;
   char          list[80];
   char          *flowstr;

   if ( (launcherp = configp->launcherp) == NULL ) {
      return;
   }

   j = 0;
   while ( launcherp[j].line_no > 0 ) {
      if ( launcherp[j].type == L_EXEC ) {
         printf("\n");
         flowstr = (configp->scanmode == launcherp[j].scanmode) ? "" : 
                   (launcherp[j].scanmode & FM_BREAK) ? "break" : "continue";
         sprintf(list, "-exec %s", flowstr);
         printf("%*s: %-20s", lw, "Launch Mode", list);

         printf("%s: ", "Flags");
         display_launcher_flags(launcherp, j);
         printf("\n");

         if ( configp->scriptp[launcherp[j].scriptnum].num_launchers > 1 ) {
            sprintf(list, "%s [%d]", launcherp[j].label, launcherp[j].launchernum); 
            printf("%*s: %-18s\n", lw, "Script label", list);
         }
         else {
            printf("%*s: %-18s\n", lw, "Script label", launcherp[j].label);
         }

         if ( script_option_list(launcherp, j, list) > 0 )
            printf("%*s: %s\n", lw, "Script opt", list);

         printf("%*s: %s\n", lw, "Command Line",
		  configp->scriptp[launcherp[j].scriptnum].cmdline);

      }
      j++;
   }
   return;
}

/*---------------------------------------------------------------------+
 | Display all script -hourly launch conditions.                       |
 +---------------------------------------------------------------------*/
void show_hourly_launcher ( void )
{
   int           j, lw = 13;
   LAUNCHER      *launcherp;
   char          list[80];
   char          *flowstr;

   if ( (launcherp = configp->launcherp) == NULL ) {
      return;
   }

   j = 0;
   while ( launcherp[j].line_no > 0 ) {
      if ( launcherp[j].type == L_HOURLY ) {
         printf("\n");
         flowstr = (configp->scanmode == launcherp[j].scanmode) ? "" : 
                   (launcherp[j].scanmode & FM_BREAK) ? "break" : "continue";
         sprintf(list, "-hourly %s", flowstr);
         printf("%*s: %-20s", lw, "Launch Mode", list);

         printf("%s: ", "Flags");
         display_launcher_flags(launcherp, j);
         printf("\n");

         if ( configp->scriptp[launcherp[j].scriptnum].num_launchers > 1 ) {
            sprintf(list, "%s [%d]", launcherp[j].label, launcherp[j].launchernum); 
            printf("%*s: %-18s\n", lw, "Script label", list);
         }
         else {
            printf("%*s: %-18s\n", lw, "Script label", launcherp[j].label);
         }

         if ( script_option_list(launcherp, j, list) > 0 )
            printf("%*s: %s\n", lw, "Script opt", list);

         printf("%*s: %s\n", lw, "Command Line",
		  configp->scriptp[launcherp[j].scriptnum].cmdline);

      }
      j++;
   }
   return;
}

/*---------------------------------------------------------------------+
 | Display all script launch conditions for argument housecode.        |
 +---------------------------------------------------------------------*/
void show_launcher ( unsigned char hcode )
{
   unsigned int  signal, bmaptrig, chgtrig;
   unsigned long funcmap, afuncmap, gfuncmap, xfuncmap;
   unsigned long sfuncmap, ofuncmap, kfuncmap;
   unsigned int  bmaptrigemu;
   unsigned int  source;
   int           j, lw = 13;
   LAUNCHER      *launcherp;
   SCRIPT        *scriptp;
   char          minibuf[64];
   char          *flowstr;

   if ( (launcherp = configp->launcherp) == NULL ||
        (scriptp   = configp->scriptp  ) == NULL    ) {
      return;
   }

   j = 0;
   while ( launcherp[j].line_no > 0 ) {
      if ( launcherp[j].hcode != hcode ||
           launcherp[j].type == L_POWERFAIL ||
           launcherp[j].type == L_SENSORFAIL ||
           launcherp[j].type == L_RFFLOOD ||
           launcherp[j].type == L_RFXJAM  ||
           launcherp[j].type == L_TIMEOUT ||
           launcherp[j].type == L_EXEC    ||
           launcherp[j].type == L_HOURLY    ) {
         j++;
         continue;
      }
      signal = launcherp[j].signal;
      afuncmap = launcherp[j].afuncmap;
      gfuncmap = launcherp[j].gfuncmap;
      xfuncmap = launcherp[j].xfuncmap;
      sfuncmap = launcherp[j].sfuncmap;
      ofuncmap = launcherp[j].ofuncmap;
      kfuncmap = launcherp[j].kfuncmap;
      funcmap = gfuncmap | afuncmap;
      bmaptrig = launcherp[j].bmaptrig;
      chgtrig = launcherp[j].chgtrig;
      source = launcherp[j].source;
      bmaptrigemu = launcherp[j].bmaptrigemu;

      printf("\n");
      if ( launcherp[j].unitstar ) {
         sprintf(minibuf, "%c*", code2hc(launcherp[j].hcode));
         printf("%*s: %-27s", lw, "Address", minibuf);
      }
      else {    
         sprintf(minibuf, "%c%s", code2hc(launcherp[j].hcode), bmap2units(bmaptrig));
         printf("%*s: %-27s", lw, "Address", minibuf);
      }

      printf("%s: ", "Functions");
      if ( gfuncmap & (1 << OnTrig) ) {
         printf("%s ", "gon");
         funcmap &= ~((1 << OnTrig) | (1 << LightsOnTrig) | (1 << AllOnTrig));
      }
      if ( gfuncmap & (1 << OffTrig) ) {
         printf("%s ", "goff");
         funcmap &= ~((1 << OffTrig) | (1 << LightsOffTrig) | (1 << AllOffTrig));
      }
      if ( gfuncmap & (1 << DimTrig) ) {
         printf("%s ", "gdim");
         funcmap &= ~((1 << DimTrig) | (1 << BriTrig));
      }
      if ( funcmap & (1 << OnTrig) ) printf("%s ", "on");
      if ( funcmap & (1 << OffTrig) ) printf("%s ", "off");
      if ( funcmap & (1 << DimTrig) ) printf("%s ", "dim");
      if ( funcmap & (1 << BriTrig) ) printf("%s ", "bright");
      if ( funcmap & (1 << LightsOnTrig) ) printf("%s ", "lightson");
      if ( funcmap & (1 << LightsOffTrig) ) printf("%s ","lightsoff");
      if ( funcmap & (1 << AllOnTrig) ) printf("%s ", "allon");
      if ( funcmap & (1 << AllOffTrig) ) printf("%s ", "alloff");
      if ( funcmap & (1 << PresetTrig) ) printf("%s ", "preset");
      if ( funcmap & (1 << ExtendedTrig) ) printf("%s ", "extended");
      if ( funcmap & (1 << StatusReqTrig) ) printf("%s ", "status");
      if ( funcmap & (1 << StatusOnTrig) ) printf("%s ", "status_on");
      if ( funcmap & (1 << StatusOffTrig) ) printf("%s ", "status_off");
      if ( funcmap & (1 << HailReqTrig) ) printf("%s ", "hail");
      if ( funcmap & (1 << HailAckTrig) ) printf("%s ", "hail_ack");
      if ( funcmap & (1 << DataXferTrig) ) printf("%s ", "data_xfer");
      if ( funcmap & (1 << ExtPowerUpTrig) ) printf("%s ", "xpowerup");
      if ( funcmap & (1 << VdataTrig) ) printf("%s ", "vdata");

      if ( sfuncmap & (1 << PanicTrig) ) printf("%s ", "panic");
      if ( sfuncmap & (1 << ArmTrig) ) printf("%s ", "arm");
      if ( sfuncmap & (1 << DisarmTrig) ) printf("%s ", "disarm");
      if ( sfuncmap & (1 << AlertTrig) ) printf("%s ", "alert");
      if ( sfuncmap & (1 << ClearTrig) ) printf("%s ", "clear");
      if ( sfuncmap & (1 << TamperTrig) ) printf("%s ", "sectamper");
      if ( sfuncmap & (1 << TestTrig)  ) printf("%s ", "test");
      if ( sfuncmap & (1 << SecLightsOnTrig) ) printf("%s ", "slightson");
      if ( sfuncmap & (1 << SecLightsOffTrig) ) printf("%s ", "slightsoff");
      if ( sfuncmap & (1 << AkeyOnTrig) ) printf("%s ", "akeyon");
      if ( sfuncmap & (1 << AkeyOffTrig) ) printf("%s ", "akeyoff");
      if ( sfuncmap & (1 << BkeyOnTrig) ) printf("%s ", "bkeyon");
      if ( sfuncmap & (1 << BkeyOffTrig) ) printf("%s ", "bkeyoff");
      if ( sfuncmap & (1 << DawnTrig) ) printf("%s ", "sdawn");
      if ( sfuncmap & (1 << DuskTrig) ) printf("%s ", "sdusk");
      if ( sfuncmap & (1 << InactiveTrig) ) printf("%s ", "inactive");

      if ( xfuncmap & (1 << RFXTempTrig) )  printf("%s ", "rfxtemp");
      if ( xfuncmap & (1 << RFXTemp2Trig) ) printf("%s ", "rfxtemp2");
      if ( xfuncmap & (1 << RFXHumidTrig) ) printf("%s ", "rfxrh");
      if ( xfuncmap & (1 << RFXPressTrig) ) printf("%s ", "rfxbp");
      if ( xfuncmap & (1 << RFXLoBatTrig) ) printf("%s ", "rfxlobat");
      if ( xfuncmap & (1 << RFXVsTrig) )    printf("%s ", "rfxvs");
      if ( xfuncmap & (1 << RFXVadTrig) )   printf("%s ", "rfxvad");
      if ( xfuncmap & (1 << RFXOtherTrig) ) printf("%s ", "rfxother");
      if ( xfuncmap & (1 << RFXPulseTrig) ) printf("%s ", "rfxpulse");
      if ( xfuncmap & (1 << RFXCountTrig) ) printf("%s ", "rfxcount");
      if ( xfuncmap & (1 << RFXPowerTrig) ) printf("%s ", "rfxpower");
      if ( xfuncmap & (1 << RFXWaterTrig) ) printf("%s ", "rfxwater");
      if ( xfuncmap & (1 << RFXGasTrig) )   printf("%s ", "rfxgas");
      if ( xfuncmap & (1 << DmxTempTrig) )  printf("%s ", "dmxtemp");
      if ( xfuncmap & (1 << DmxOnTrig) )    printf("%s ", "dmxon");
      if ( xfuncmap & (1 << DmxOffTrig) )   printf("%s ", "dmxoff");
      if ( xfuncmap & (1 << DmxSetPtTrig) )  printf("%s ", "dmxsetpoint");

      if ( ofuncmap & (1 << OreTempTrig) )  printf("%s ", "oretemp");
      if ( ofuncmap & (1 << OreHumidTrig) )  printf("%s ", "orerh");
      if ( ofuncmap & (1 << OreBaroTrig) )  printf("%s ", "orebp");
      if ( ofuncmap & (1 << OreWeightTrig) )  printf("%s ", "orewgt");
      if ( ofuncmap & (1 << OreWindSpTrig) ) printf("%s ", "orewindsp");
      if ( ofuncmap & (1 << OreWindAvSpTrig) ) printf("%s ", "orewindavsp");
      if ( ofuncmap & (1 << OreWindSpTrig) ) printf("%s ", "orewindsp");
      if ( ofuncmap & (1 << OreRainRateTrig) ) printf("%s ", "orerain");
      if ( ofuncmap & (1 << OreRainTotTrig) ) printf("%s ", "oreraintot");
      if ( ofuncmap & (1 << OwlPowerTrig) ) printf("%s ", "owlpower");
      if ( ofuncmap & (1 << OwlEnergyTrig) ) printf("%s ", "owlenergy");
      if ( ofuncmap & (1 << ElsCurrTrig) ) printf("%s ", "elscurr");


      if ( kfuncmap & (1 << KakuOnTrig) ) printf("%s ", "kon");
      if ( kfuncmap & (1 << KakuOffTrig) ) printf("%s ", "koff");
      if ( kfuncmap & (1 << KakuGrpOnTrig) ) printf("%s ", "kgrpon");
      if ( kfuncmap & (1 << KakuGrpOffTrig) ) printf("%s ", "kgrpoff");
      if ( kfuncmap & (1 << KakuPreTrig) ) printf("%s ", "kpreset");
      if ( kfuncmap & (1 << KakuGrpPreTrig) ) printf("%s ", "kgrppreset");

      printf("\n");

      printf("%*s: ", lw, "Launch Mode");
      flowstr = (configp->scanmode == launcherp[j].scanmode) ? "" : 
                (launcherp[j].scanmode & FM_BREAK) ? "break " : "continue ";
      if ( launcherp[j].type == L_ADDRESS )
         sprintf(minibuf, "%s", "address");
      else
         sprintf(minibuf, "%s%s%s%s", (chgtrig ? "changed " : ""),
	   ((bmaptrig & signal) ? "signal " : "module "),
	   ((launcherp[j].trigemuflag) ? "trigemu " : ""), flowstr );
      printf("%-31s", minibuf);

      printf("%s: ", "Flags");
      display_launcher_flags(launcherp, j);
      printf("\n");

      if ( scriptp[launcherp[j].scriptnum].num_launchers > 1 ) {
         sprintf(minibuf, "%s [%d]", launcherp[j].label, launcherp[j].launchernum); 
         printf("%*s: %-29s", lw, "Script label", minibuf);
      }
      else {
         printf("%*s: %-29s", lw, "Script label", launcherp[j].label);
      }

      printf("%s: ", "Sources");
      if ( source & RCVI )  printf("rcvi ");
      if ( source & RCVT )  printf("rcvt ");
      if ( source & SNDC )  printf("sndc ");
      if ( source & SNDM )  printf("sndm ");
      if ( source & SNDS )  printf("snds ");
      if ( source & SNDT )  printf("sndt ");
      if ( source & SNDP )  printf("sndp ");
      if ( source & SNDA )  printf("snda ");
      if ( source & RCVA )  printf("rcva ");
      if ( source & XMTF )  printf("xmtf ");
      printf("\n");

      if ( script_option_list(launcherp, j, minibuf) > 0 )
         printf("%*s: %s\n", lw, "Script opt", minibuf);

      printf("%*s: %s\n", lw, "Command Line",
		  configp->scriptp[launcherp[j].scriptnum].cmdline);

      j++;
   }

   return;
}


void show_all_launchers ( void )
{
   static int hcode_table[16] =
        {6, 14, 2, 10, 1, 9, 5, 13, 7, 15, 3, 11, 0, 8, 4, 12};
   int j;

   if ( configp->launcherp == NULL ) {
      return;
   }

   for ( j = 0; j < 16; j++ )
      show_launcher(hcode_table[j]);

   show_powerfail_launcher();
   show_sensorfail_launcher();
   show_rfflood_launcher();
   show_rfxjam_launcher();
   show_timeout_launcher();
   show_exec_launcher();
   show_hourly_launcher();

   return;
}
   
/*---------------------------------------------------------------------+
 | Display the sticky address state for all housecodes and units.      |
 +---------------------------------------------------------------------*/
void show_sticky_addr ( void ) 
{
   unsigned char hcode;
   char          hc;
   int           j, unit;
   char          outbuf[17];
   char          label[16];
   char          *chrs = "*.";
   int           lw = 13;

   printf("Cumulative received addresses\n");
   printf("%*s  1..4...8.......16\n", lw, "Unit:");
   for ( hc = 'A'; hc <= 'P'; hc++ ) {
      hcode = hc2code(hc);

      for ( j = 0; j < 16; j++ ) {
         unit = code2unit(j) - 1;
         if ( x10state[hcode].sticky & (1 << j) ) {
            outbuf[unit] = chrs[0];
         }
         else
            outbuf[unit] = chrs[1];
      }
      sprintf(label, "Housecode %c", hc);
      outbuf[16] = '\0';
      printf("%*s (%s)\n", lw, label, outbuf);
   }
   return;
}

/*---------------------------------------------------------------------+
 | Display the On/Off/Dimmed state for all housecodes and units.       |
 +---------------------------------------------------------------------*/
void show_housemap ( void ) 
{
   unsigned char hcode;
   char          hc;
   int           j, unit;
   char          outbuf[17];
   char          label[16];
   char          *chrs = "*x.";
   int           lw = 13;

   printf("%*s %s\n", lw, "", "* = On  x = Dimmed");
   printf("%*s  1..4...8.......16\n", lw, "Unit:");
   for ( hc = 'A'; hc <= 'P'; hc++ ) {
      hcode = hc2code(hc);

      for ( j = 0; j < 16; j++ ) {
         unit = code2unit(j) - 1;
         if ( x10state[hcode].state[DimState] & (1 << j) ) {
            outbuf[unit] = chrs[1];
         }
         else if ( x10state[hcode].state[OnState] & (1 << j) ) {
            outbuf[unit] = chrs[0];
         }
         else
            outbuf[unit] = chrs[2];
      }
      sprintf(label, "Housecode %c", hc);
      outbuf[16] = '\0';
      printf("%*s (%s)\n", lw, label, outbuf);
   }
   return;
}

/*---------------------------------------------------------------------+
 | Display the flag states, including night and dark flags             |
 +---------------------------------------------------------------------*/
void show_flags_old ( void )
{
   int           j, lw = 13;
   char          *chrs = "01-";
   char          outbuf[64];
   int           offset = 0;

   for ( j = 0; j < (int)sizeof(outbuf); j++ )
      outbuf[j] = ' ';

   printf("%*s %s\n\n", lw, "Flag states", "0 = Clear  1 = Set");
   printf("%*s  1..4...8.......16    night dark\n", lw, "Flag:");

   outbuf[offset++] = '(';

   for ( j = 0; j < 16; j++ ) {
      if ( x10global.flags[0] & (1 << j) ) 
         outbuf[offset] = chrs[1];
      else
         outbuf[offset] = chrs[0];
      offset++;
   }
   outbuf[offset++] = ')';
   for ( j = 16; j < 18; j++ ) {
      offset += 6;
      if ( x10global.sflags & (1 << j) )
         outbuf[offset] = x10global.dawndusk_enable ? chrs[1] : chrs[2];
      else
         outbuf[offset] = x10global.dawndusk_enable ? chrs[0] : chrs[2];
   }
      
   outbuf[++offset] = '\0';

   printf("%*s %s\n\n", lw, "", outbuf);

   offset = 0;
   printf("%*s  1..4...8.......16\n", lw, "CzFlag:");
   outbuf[offset++] = '(';

   for ( j = 0; j < 16; j++ ) {
      if ( x10global.czflags[0] & (1 << j) ) 
         outbuf[offset] = chrs[1];
      else
         outbuf[offset] = chrs[0];
      offset++;
   }
   outbuf[offset++] = ')';
   outbuf[++offset] = '\0';

   printf("%*s %s\n\n", lw, "", outbuf);

   return;
}

         
/*---------------------------------------------------------------------+
 | Display the flag states, including night and dark flags             |
 +---------------------------------------------------------------------*/
void show_geoflags ( void )
{
   int           j, lw = 13;
   char          *chrs = "01-";
   char          outbuf[64];
   int           offset = 0;

   for ( j = 0; j < (int)sizeof(outbuf); j++ )
      outbuf[j] = ' ';

   printf("%*s %s\n\n", lw, "Flag states", "0 = Clear  1 = Set");
   printf("%*s  1..4...8.......16    night dark\n", lw, "Flag:");

   outbuf[offset++] = '(';

   for ( j = 0; j < 16; j++ ) {
#if 0
      if ( x10global.sflags & (1 << j) )
#endif 
      if ( x10global.flags[0] & (1 << j) )
         outbuf[offset] = chrs[1];
      else
         outbuf[offset] = chrs[0];
      offset++;
   }
   outbuf[offset++] = ')';
   for ( j = 16; j < 18; j++ ) {
      offset += 6;
      if ( x10global.sflags & (1 << j) )
         outbuf[offset] = x10global.dawndusk_enable ? chrs[1] : chrs[2];
      else
         outbuf[offset] = x10global.dawndusk_enable ? chrs[0] : chrs[2];
   }
      
   outbuf[++offset] = '\0';

   printf("%*s %s\n\n", lw, "", outbuf);

   return;
}

/*---------------------------------------------------------------------+
 | Display the flag/czflag/tzflag states, plus night and dark flags    |
 +---------------------------------------------------------------------*/
#define INDENT 12
#define FLAGS_GROUP 4

void show_long_flags ( void )
{
   int j, k;
   unsigned long mask;
   char *chrs = "01-";
 
   for ( j = 0; j < NUM_FLAG_BANKS; j++ ) {
      if ( (32 * NUM_FLAG_BANKS) >= 100 ) {
         printf("\n%*s", INDENT, " ");
         for ( k = 0; k < 32; k += FLAGS_GROUP )
            printf("%-*d", FLAGS_GROUP + 1, (j * 32 + 1 + k) / 100);
      }
      printf("\n%*s", INDENT, " ");
      for ( k = 0; k < 32; k += FLAGS_GROUP ) 
         printf("%-*d", FLAGS_GROUP + 1, ((j * 32 + 1 + k) % 100) / 10);
      printf("\n     Flag:  ");
      for ( k = 0; k < 32; k += FLAGS_GROUP ) 
         printf("%-*d", FLAGS_GROUP + 1, (j * 32 + 1 + k) % 10);
      if ( j == 0 )
         printf("    night   dark");
      printf("\n%*s(", INDENT - 1, " ");

      mask = 1;
      for ( k = 0; k < 32; k++ ) {
         if ( !(k % FLAGS_GROUP) && k > 0 )
            printf(" ");
         printf("%c", ((x10global.flags[j] & mask) ? chrs[1] : chrs[0]) );
         mask <<= 1;
      }
      printf(")");

      if ( j == 0 && x10global.dawndusk_enable ) {         
         printf("      %c", ((x10global.sflags & NIGHT_FLAG) ? chrs[1] : chrs[0]) );
         printf("       %c", ((x10global.sflags & DARK_FLAG) ? chrs[1] : chrs[0]) );
      }
      else if ( j == 0 )
         printf("      -       -");

      printf("\n");

   }
   for ( j = 0; j < NUM_COUNTER_BANKS; j++ ) {
      if ( (32 * NUM_COUNTER_BANKS) >= 100 ) {
         printf("\n%*s", INDENT, " ");
         for ( k = 0; k < 32; k += FLAGS_GROUP )
            printf("%-*d", FLAGS_GROUP + 1, (j * 32 + 1 + k) / 100);
      }
      printf("\n%*s", INDENT, " ");
      for ( k = 0; k < 32; k += FLAGS_GROUP ) 
         printf("%-*d", FLAGS_GROUP + 1, ((j * 32 + 1 + k) % 100) / 10);
      printf("\n   CzFlag:  ");
      for ( k = 0; k < 32; k += FLAGS_GROUP ) 
         printf("%-*d", FLAGS_GROUP + 1, (j * 32 + 1 + k) % 10);
      printf("\n%*s(", INDENT - 1, " ");

      mask = 1;
      for ( k = 0; k < 32; k++ ) {
         if ( !(k % FLAGS_GROUP) && k > 0 )
            printf(" ");
         printf("%c", ((x10global.czflags[j] & mask) ? chrs[1] : chrs[0]) );
         mask <<= 1;
      }
      printf(")\n");
   }
   printf("\n");

   for ( j = 0; j < NUM_TIMER_BANKS; j++ ) {
      if ( (32 * NUM_TIMER_BANKS) >= 100 ) {
         printf("\n%*s", INDENT, " ");
         for ( k = 0; k < 32; k += FLAGS_GROUP )
            printf("%-*d", FLAGS_GROUP + 1, (j * 32 + 1 + k) / 100);
      }
      printf("\n%*s", INDENT, " ");
      for ( k = 0; k < 32; k += FLAGS_GROUP ) 
         printf("%-*d", FLAGS_GROUP + 1, ((j * 32 + 1 + k) % 100) / 10);
      printf("\n   TzFlag:  ");
      for ( k = 0; k < 32; k += FLAGS_GROUP ) 
         printf("%-*d", FLAGS_GROUP + 1, (j * 32 + 1 + k) % 10);
      printf("\n%*s(", INDENT - 1, " ");

      mask = 1UL;
      for ( k = 0; k < 32; k++ ) {
         if ( !(k % FLAGS_GROUP) && k > 0 )
            printf(" ");
         printf("%c", ((x10global.tzflags[j] & mask) ? chrs[1] : chrs[0]) );
         mask <<= 1;
      }
      printf(")\n");
   }
   printf("\n");

   return;
}

         
/*---------------------------------------------------------------------+
 | Function to delete the X10state file.                               |
 +---------------------------------------------------------------------*/
void remove_x10state_file ( void )
{
   int   code;

   code = remove( statefile ) ;
   if ( code != 0 && errno != 2 ) {
      (void)fprintf(stderr, 
         "WARNING: Unable to delete X10 State File %s - errno = %d\n",
          statefile, errno);
   }
   return;
}

/*---------------------------------------------------------------------+
 | Function to write the x10state structure to the X10state file.      |
 +---------------------------------------------------------------------*/
void write_x10state_file ( void )
{
   FILE *fd;

   if ( !i_am_state )
      return;

   if ( verbose )
      fprintf(stderr, "Writing state file\n");

   if ( !(fd = fopen(statefile, "w")) ) {
      fprintf(stderr, "Unable to open X10 State File '%s' for writing.\n",
         statefile);
      exit(1);
   }

   x10global.filestamp = time(NULL);
   if ( fwrite((void *)(&x10global), 1, (sizeof(x10global)), fd) != sizeof(x10global) ) {
      fprintf(stderr, "Unable to write X10 State File.\n");
   }

   fclose(fd);
   return;
}

/*---------------------------------------------------------------------+
 | Get the last-modified time of the x10state file.                    |
 +---------------------------------------------------------------------*/
long int modtime_x10state_file ( void )
{
   struct stat statbuf;
   int retcode;

   retcode = stat(statefile, &statbuf);

   if ( retcode == 0 )
      return statbuf.st_mtime;

   return (long)retcode;
}

/*---------------------------------------------------------------------+
 | Verify that the state engine is running by checking for its lock    |
 | file.                                                               |
 +---------------------------------------------------------------------*/
int check_for_engine ( void )
{
   struct stat statbuf;
   int retcode;

   retcode = stat(enginelockfile, &statbuf);

   return retcode;
}

/*---------------------------------------------------------------------+
 | Function to read the x10state file and store the contents in the    |
 | x10state structure.                                                 |
 +---------------------------------------------------------------------*/
int read_x10state_file (void )
{
   FILE *fd = NULL;  /* Keep some compilers happy; ditto with nread */
   int  j, nread = 0;
   unsigned char inbuffer[sizeof(x10global) + 8];

   for ( j = 0; j < 3; j++ ) {
      if ( (fd = fopen(statefile, "r")) == NULL ) {
         millisleep(100);
         continue;
      }
      nread = fread((void *)inbuffer, 1, (sizeof(x10global) + 1), fd );
      fclose(fd);
      if ( nread == (sizeof(x10global)) ) {
         memcpy((void *)(&x10global), inbuffer, nread);
         break;
      }
      millisleep(100);
   }

   if ( nread != (sizeof(x10global)) ) {
      return 2;
   }
   return 0;
}
#if 0
/*---------------------------------------------------------------------+
 | Function to read the x10state file and store the contents in the    |
 | x10state structure.                                                 |
 +---------------------------------------------------------------------*/
int read_x10state_file (void )
{
   FILE *fd = NULL;  /* Keep some compilers happy; ditto with nread */
   int  j, nread = 0;
   unsigned char inbuffer[sizeof(x10global) + 8];

   for ( j = 0; j < 3; j++ ) {
      if ( (fd = fopen(statefile, "r")) )
         break;
      millisleep(100);
   }

   if ( !fd ) {
      return 1;
   }

   for ( j = 0; j < 3; j++ ) {
      nread = fread((void *)inbuffer, 1, (sizeof(x10global) + 1), fd );
      if ( nread == (sizeof(x10global)) ) {
         memcpy((void *)(&x10global), inbuffer, nread);
         break;
      }
      rewind(fd);
      millisleep(100);
   }

   if ( nread != (sizeof(x10global)) ) {
      return 2;
   }
   fclose(fd);
   return 0;
}
#endif

/*---------------------------------------------------------------------+
 | Send command to heyu_engine process to write the x10 state file,    |
 | then read it and store in the state structure.                      |
 +---------------------------------------------------------------------*/
int fetch_x10state ( void )
{
   struct stat statbuf;
   int         j, retcode;
   time_t      now;
   int         retries = 20;

   if ( check_for_engine() != 0 ) {
      fprintf(stderr, "State engine is not running.\n");
      return 1;
   }

   now = time(NULL);
   send_x10state_command(ST_WRITE, 0);
   for ( j = 0; j < retries; j++ ) {
      if ( (retcode = stat(statefile, &statbuf)) == 0 &&
           read_x10state_file() == 0  &&
           x10global.filestamp >= now ) {
         break;
      }
      millisleep(100);
   }
   if ( j >= retries ) {
      fprintf(stderr, "Unable to read current x10state - is state engine running?\n");
      return 1;
   }

   return 0;
}


/*---------------------------------------------------------------------+
 | Send command to heyu_engine process to write the x10 state file,    |
 | then read it and store in the state structure.                      |
 +---------------------------------------------------------------------*/
int fetch_x10state_old ( void )
{
   struct stat statbuf;
   int         j, retcode;
   time_t      now;
   int         retries = 50;

   if ( check_for_engine() != 0 ) {
      fprintf(stderr, "State engine is not running.\n");
      return 1;
   }

   time(&now);
   send_x10state_command(ST_WRITE, 0);
   for ( j = 0; j < retries; j++ ) {
      if ( (retcode = stat(statefile, &statbuf)) == 0 &&
           statbuf.st_mtime >= now ) {
         if ( read_x10state_file() == 0 )
            break;
      }
      millisleep(100);
   }
   if ( j >= retries ) {
      fprintf(stderr, "Unable to read current x10state - is state engine running?\n");
      return 1;
   }

   return 0;
}

/*---------------------------------------------------------------------+
 | Identify the type of data read from the spoolfile which was written |
 | there by heyu, i.e., excluding data received from the interface.    |
 +---------------------------------------------------------------------*/
int identify_sent ( unsigned char *buf, int len )
{
   int type;
   unsigned char chksum;

   chksum = checksum(buf, len);
   
   if ( buf[0] == 0 && len == 1 )
      type = SENT_WRMI;  /* WRMI */
   else if ( buf[0] == 0x04 && len == 2 )
      type = SENT_ADDR;  /* Address */
   else if ( (buf[0] & 0x07) == 0x06 && len == 2 )
      type = SENT_FUNC;  /* Standard Function */
   else if ( buf[0] == 0x07 && len == 5 )
      type = SENT_EXTFUNC;  /* Extended function */
   else if ( buf[0] == ST_COMMAND && buf[1] == ST_XMITRF && len == 6 )
      type = SENT_RF; /* CM17A command */
   else if ( buf[0] == ST_COMMAND && buf[1] == ST_MSG &&
             len == (buf[2] + 3) )
      type = SENT_MESSAGE; /* Display message */
   else if ( buf[0] == ST_COMMAND && buf[1] == ST_VDATA && len == 9 )
      type = SENT_VDATA; /* Virtual data */
   else if ( buf[0] == ST_COMMAND && buf[1] == ST_LONGVDATA )
      type = SENT_LONGVDATA; /* Virtual data */
   else if ( buf[0] == ST_COMMAND && buf[1] == ST_GENLONG )
      type = SENT_GENLONG;
   else if ( buf[0] == ST_COMMAND && buf[1] == ST_FLAGS && len == 5 )
      type = SENT_FLAGS;  /* Update flag states */
   else if ( buf[0] == ST_COMMAND && buf[1] == ST_FLAGS32 && len == 8 )
      type = SENT_FLAGS32;  /* Update flag states */
   else if ( buf[0] == ST_COMMAND && buf[1] == ST_COUNTER /* && len == 7 */)
      type = SENT_COUNTER;  /* Set, Increment, or Decrement counter */
   else if ( buf[0] == ST_COMMAND && buf[1] == ST_CLRSTATUS && len == 5 )
      type = SENT_CLRSTATUS; /* Clear status flags */
   else if ( buf[0] == ST_COMMAND && buf[1] == ST_PFAIL && len == 3 )
      type = SENT_PFAIL;  /* Relay reports a powerfail update */
   else if ( buf[0] == ST_COMMAND && buf[1] == ST_RFFLOOD && len == 3 )
      type = SENT_RFFLOOD; /* Aux reports an RF Flood */
   else if ( buf[0] == ST_COMMAND && buf[1] == ST_LOCKUP && len == 3 )
      type = SENT_LOCKUP; /* Relay reports a CM11A lockup condition */
   else if ( buf[0] == ST_COMMAND && buf[1] == ST_RFXTYPE && len == 4 ) 
      type = SENT_RFXTYPE; /* An RFXSerial initialization packet */
   else if ( buf[0] == ST_COMMAND && buf[1] == ST_RFXSERIAL && len == 10 ) 
      type = SENT_RFXSERIAL; /* An RFXSerial initialization packet */
   else if ( buf[0] == ST_COMMAND && buf[1] == ST_SHOWBUFFER )
      type = SENT_SHOWBUFFER; /* Display a binary buffer */
   else if ( buf[0] == ST_COMMAND && buf[1] == ST_VARIABLE_LEN ) {
      if ( buf[2] == RF_KAKU )
         type = SENT_KAKU; /* KaKu signal */
      else if ( buf[2] == RF_VISONIC )
         type = SENT_VISONIC;  /* Visonic signal */
      else 
         type = SENT_RFVARIABLE; /* An RFXSerial raw or noise packet */
   }
   else if ( buf[0] == ST_COMMAND && len == 3 )
      type = SENT_STCMD;  /* Monitor/state control command */
   else if ( buf[0] == 0x0C && len == 2 && buf[1] == 0x56 )
      type = SENT_ADDR;  /* Address of G1 with 5a fix */
   else if ( buf[0] == 0x0F && len == 5 && chksum == 0x62 )
      type = SENT_EXTFUNC;  /* Extended function with 5a fix */
   else if ( buf[0] == ST_COMMAND && buf[1] == ST_SETTIMER && len == 8 )
      type = SENT_SETTIMER; /* Set a countdown timer */
   else if ( buf[0] == ST_COMMAND && buf[1] == ST_ORE_EMU )
      type = SENT_ORE_EMU;  /* Oregon emulation */
   else if ( buf[0] == ST_COMMAND && buf[1] == ST_SCRIPT /*&& len == 7*/ )
      type = SENT_SCRIPT; /* heyu launch command */
   else if ( buf[0] == ST_COMMAND && buf[1] == ST_INIT && len == 5 )
      type = SENT_INITSTATE; /* Initstate command */
   else
      type = SENT_OTHER;  /* Other, i.e., info, setclock, upload, etc. */

   return type;
}


void create_flagslist ( unsigned char vtype, unsigned long vflags, char *flagslist )
{
   
   if ( vtype == RF_SEC ) {
      if ( vflags & SEC_TAMPER )
         strcat(flagslist, "Tamper ");
      if ( vflags & SEC_MAIN )
         strcat(flagslist, "Main ");
      if ( vflags & SEC_AUX )
         strcat(flagslist, "Aux ");
      if ( vflags & SEC_HOME )
         strcat(flagslist, "swHome ");
      if ( vflags & SEC_AWAY )
         strcat(flagslist, "swAway ");
      if ( vflags & SEC_MIN )
         strcat(flagslist, "swMin ");
      if ( vflags & SEC_MAX )
         strcat(flagslist, "swMax ");
      if ( vflags & SEC_LOBAT )
         strcat(flagslist, "LoBat ");
   }
   if ( vtype == RF_OREGON ) {
      if ( vflags & ORE_TMIN )
         strcat(flagslist, "TmiN ");
      if ( vflags & ORE_TMAX )
         strcat(flagslist, "TmaX ");
      if ( vflags & ORE_RHMIN )
         strcat(flagslist, "RHmiN ");
      if ( vflags & ORE_RHMAX )
         strcat(flagslist, "RHmaX ");
      if ( vflags & ORE_BPMIN )
         strcat(flagslist, "BPmiN ");
      if ( vflags & ORE_BPMAX )
         strcat(flagslist, "BPmaX ");
      if ( vflags & SEC_LOBAT )
         strcat(flagslist, "LoBat ");
      if ( vflags & RFX_ROLLOVER )
         strcat(flagslist, "rollover ");
   }
   if ( vtype == RF_ELECSAVE || vtype == RF_OWL ) {
      if ( vflags & SEC_LOBAT )
         strcat(flagslist, "LoBat ");
      if ( vflags & RFX_ROLLOVER )
         strcat(flagslist, "rollover ");
   }
   if ( vtype == RF_DIGIMAX ) {
      if ( vflags & DMX_INIT )
         strcat(flagslist, "Init ");
      if ( vflags & DMX_SET )
         strcat(flagslist, "Set ");
      else
         strcat(flagslist, "NotSet ");
      if ( vflags & DMX_HEAT ) 
         strcat(flagslist, "Heat ");
      else 
         strcat(flagslist, "Cool ");
   }

   return;
}

/*---------------------------------------------------------------------+
 | Interpret Virtual data string, update the state, and test whether   |
 | any launch condition is satisfied.                                  |
 | buffer references:                                                  |
 |  ST_COMMAND, ST_VDATA, addr, vdata, vtype, vident, videnthi, hibyte, lobyte   |
 +---------------------------------------------------------------------*/
char *translate_virtual ( unsigned char *buf, int len, unsigned char *sunchanged, int *launchp )
{
   static char outbuf[120];
   static char intvstr[32];
   long intv;
   char flagslist[80];
   char vdatabuf[32];
   char hc;
   int  j, k, found, index = -1;
   unsigned char hcode, ucode, unit, func, vdata, vtype, /*vident,*/ byte2, byte3;
//   unsigned short vident, idmask;
   unsigned long vident, idmask;
   unsigned int bitmap;
   unsigned long vflags = 0;
   /* unsigned long flood; */
   ALIAS *aliasp;
   static char *typename[] = {"Std", "Ent", "Sec", "RFXSensor", "RFXMeter", "Dusk", "Visonic", "Noise"};

#ifdef HASRFXS
   char *marker = "";
   char valbuf[60];
   int  stype;
   unsigned int inmap, outmap;
   double temperature, vsup, var2, advolt;
   extern int x10state_update_rfxsensor ( unsigned char *, int, int * );
#endif
   *launchp = -1;
   *sunchanged = 0;
   flagslist[0] = '\0';

   aliasp = config.aliasp;

   vdata  = buf[3];
   vtype  = buf[4];
//   vident = buf[5] | (buf[6] << 8);
   byte2  = buf[7];
   byte3  = buf[8];
   func   = VdataFunc;

if ( vtype == RF_VISONIC ) {
   vident = buf[5] | (buf[6] << 8) | (buf[7] << 16);
}
else {
   vident = buf[5] | (buf[6] << 8);
}

   x10global.longvdata = 0;
   x10global.lastvtype = vtype;

   if ( vtype == RF_STD || vtype == RF_VDATAM ) {
      hcode = (buf[2] & 0xf0u) >> 4;
      hc = code2hc(hcode);
      ucode = buf[2] & 0x0fu;
      bitmap = (1 << ucode);
      unit = code2unit(ucode);
      vdata = buf[3];
      func = (vtype == RF_VDATAM) ? VdataMFunc : VdataFunc;
      if ( x10state_update_virtual(buf + 2, len - 2, launchp) != 0 )
         return "";
      sprintf(outbuf, "func %12s : hu %c%-2d vdata 0x%02x (%s)",
         funclabel[func], hc, unit, vdata, lookup_label(hc, (1 << ucode)));
      return outbuf;
   }

   if ( vtype == RF_DUSK || vtype == RF_SECX ) {
      *vdatabuf = '\0';
      hcode = (buf[2] & 0xf0u) >> 4;
      hc = code2hc(hcode);
      ucode = buf[2] & 0x0fu;
      bitmap = 1 << ucode;
      unit = code2unit(ucode);
      if ( vdata == 0xf0u ) 
         func = DuskFunc;
      else if ( vdata == 0xf8u )
         func = DawnFunc;
      else if ( vdata == 0xe0u )
         func = AlertFunc;        
      else {
         func = VdataFunc;
         sprintf(vdatabuf, "vdata 0x%02x ", vdata);
      } 
      if ( x10state_update_duskdawn(buf + 2, len -2, launchp) != 0 )
         return "";
      sprintf(outbuf, "func %12s : hu %c%d %s(%s)", funclabel[func], hc, unit, vdatabuf, lookup_label(hc, bitmap));
      return outbuf;
   }

   if ( vtype == RF_FLOOD ) {
      if ( vdata & SEC_FLOOD )
         x10global.sflags |= GLOBSEC_FLOOD;
      else
         x10global.sflags &= ~GLOBSEC_FLOOD;
      sprintf(outbuf, "           RF Flood : %s", ((vdata & SEC_FLOOD) ? "Started" : "Ended"));
      *launchp = find_rfflood_scripts();
      return outbuf;
   }

   if ( vtype == RF_XJAM ) {
      if ( vdata == 0xe0u ) {
         x10global.vflags |= RFX_JAM;
         x10global.sflags |= GLOBSEC_RFXJAM;
      }
      else {
         x10global.vflags &= ~RFX_JAM;
         x10global.sflags &= ~GLOBSEC_RFXJAM;
      }
      if ( vident == 0xffu ) {
         x10global.vflags |= SEC_MAIN;
         x10global.vflags &= ~SEC_AUX;
      }
      else {
         x10global.vflags &= ~SEC_MAIN;
         x10global.vflags |= SEC_AUX;
      }
      sprintf(outbuf, "         RF Jamming : %s %s",
       ((vdata == 0xe0u) ? "Started" : "Ended"),
       ((vident == 0xffu) ? "Main" : "Aux") );
      *launchp = find_rfxjam_scripts();
      return outbuf;
   }
   
   if ( vtype == RF_XJAM32 ) {
      if ( vident == 0x07u ) {
         x10global.vflags |= RFX_JAM;
         x10global.sflags |= GLOBSEC_RFXJAM;
      }
      else {
         x10global.vflags &= ~RFX_JAM;
         x10global.sflags &= ~GLOBSEC_RFXJAM;
      }
      if ( vdata == 0xffu ) {
         x10global.vflags |= SEC_MAIN;
         x10global.vflags &= ~SEC_AUX;
      }
      else {
         x10global.vflags &= ~SEC_MAIN;
         x10global.vflags |= SEC_AUX;
      }
      sprintf(outbuf, "         RF Jamming : %s %s",
       ((vident == 0x07u) ? "Started" : "Ended"),
       ((vdata == 0xffu) ? "Main" : "Aux") );
      *launchp = find_rfxjam_scripts();
      return outbuf;
   }
   
//   if ( vtype == RF_SEC || vtype == RF_ENT ) {
   if ( vtype == RF_SEC || vtype == RF_ENT || vtype == RF_VISONIC ) {
      idmask = (vtype == RF_SEC) ? configp->securid_mask : 0xffffu;
      hcode = ucode = 0;
      found = 0;
      j = 0;
      /* Look up the alias, if any */
      while ( !found && aliasp && aliasp[j].line_no > 0 ) {
         if ( aliasp[j].vtype == vtype && 
              (bitmap = aliasp[j].unitbmap) > 0 &&
              (ucode = single_bmap_unit(bitmap)) != 0xff ) {
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
         sprintf(outbuf, "func %12s : Type %s ID 0x%02lX Data 0x%02X",
           "RFdata", typename[vtype], vident, vdata);
         return outbuf;
      }

      x10global.longvdata = (vident << 8) | vdata;

      hc = aliasp[index].housecode;
      hcode = hc2code(hc);
      unit = code2unit(ucode);

      /* Save interval since last transmission */
      intvstrfunc(intvstr, &intv, time(NULL), x10state[hcode].timestamp[ucode]);
      x10global.interval = intv;

#if 0
      /* Update activity timeout for security sensors with heartbeat */
      update_activity_timeout(aliasp, index);
      update_activity_states(hcode, (1 << ucode), S_ACTIVE);
#endif

      /* Add address to buf and update the state */
      buf[2] = (hcode & 0x0fu) << 4 | (ucode & 0x0fu);
      if ( x10state_update_virtual(buf + 2, len - 2, launchp) != 0 )
         return "";

      func = x10state[hcode].lastcmd;
      vflags = x10state[hcode].vflags[ucode];
      create_flagslist(vtype, vflags, flagslist);

      if ( vtype == RF_SEC ) {
         if ( func == VdataFunc ) {
            sprintf(outbuf, "func %12s : hu %c%-2d vdata 0x%02x %s (%s)",
              funclabel[func], hc, unit, vdata, flagslist, aliasp[index].label);
         }
         else {
            sprintf(outbuf, "func %12s : hu %c%-2d %s (%s)",
                funclabel[func], hc, unit, flagslist, aliasp[index].label);
         }

         if ( (aliasp[index].optflags & MOPT_HEARTBEAT) && 
              (x10state[hcode].state[ChgState] & (1 << ucode)) == 0 ) {
            *sunchanged = 1;
         }

         if ( configp->display_sensor_intv == YES && (aliasp[index].optflags & MOPT_HEARTBEAT) ) {
            snprintf(outbuf + strlen(outbuf), sizeof(outbuf), " Intv %s ", intvstr);
         }

         if ( configp->show_change == YES ) {
            if ( x10state[hcode].state[ChgState] & (1 << ucode) ) {
               strcat(outbuf, " Chg");
            }
            else {
               strcat(outbuf, " UnChg");
            }
         }
      }
      else if ( vtype == RF_ENT && aliasp[index].nident == 1 ) {
         sprintf(outbuf, "func %12s : hu %c%-2d vdata 0x%02X (%s)",
           funclabel[func], hc, unit, vdata, aliasp[index].label);
      }
      else if ( vtype == RF_ENT ) {
         sprintf(outbuf, "func %12s : hu %c%-2d lvdata 0x%04lX (%s)",
           funclabel[func], hc, unit, x10global.longvdata, aliasp[index].label);
      }
      else {
         sprintf(outbuf, "func %12s : hu %c%-2d vdata 0x%02X (%s)",
           funclabel[func], hc, unit, vdata, aliasp[index].label);
      }
   }
#ifdef HASRFXS
   else if ( vtype == RF_XSENSOR ) {
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
         marker = ((vident % 4) == 0) ? "*" : " ";
         sprintf(outbuf, "func %12s : Type %s ID 0x%02lX%s Data 0x%02X%02X",
           "RFdata", typename[vtype], vident, marker, byte2, byte3);
         return outbuf;
      }

      x10global.longvdata = (byte2 << 8) | byte3;

      hc = aliasp[index].housecode;
      hcode = hc2code(hc);
      unit = code2unit(ucode);

      /* Save interval since last transmission */
      intvstrfunc(intvstr, &intv, time(NULL), x10state[hcode].timestamp[ucode]);

#if 0
      /* Update activity timeout for sensor */
      update_activity_timeout(aliasp, index);
      update_activity_states(hcode, (1 << ucode), S_ACTIVE);
#endif

      /* Add address to buf and update the state */
      buf[2] = (hcode & 0x0fu) << 4 | (ucode & 0x0fu);
      if ( x10state_update_rfxsensor(buf + 2, len - 2, launchp) != 0 )
         return "";

      func = x10state[hcode].lastcmd;
      vflags = x10state[hcode].vflags[ucode];
      create_flagslist(vtype, vflags, flagslist);

#if 0
      activechange = (configp->active_change == YES) ? x10state[hcode].state[ActiveChgState] : 0;

      /* Indicate whether no change in state for Sensor module */
      if ( (aliasp[index].optflags & MOPT_RFXSENSOR) && 
           ((x10state[hcode].state[ChgState] | activechange) & (1 << ucode)) == 0 ) {
         *sunchanged = 1;
      }
#endif
      if ( (x10state[hcode].state[ChgState] & (1 << ucode)) == 0 )
         *sunchanged = 1;

      stype = vident % 4;

      if ( x10global.longvdata & 0x10u ) {
         if ( stype == RFX_S && byte2 == 0x02u ) {
            sprintf(outbuf, "func %12s : hu %c%-2d Low Battery (%s)",
               funclabel[func], hc, unit, aliasp[index].label);
         }
         else if ( byte2 & 0x80u ) {
            sprintf(outbuf, "func %12s : hu %c%-2d ID %02lX Error 0x%02X (%s)",
               funclabel[func], hc, unit, vident, byte2, aliasp[index].label);
         }
         else {
            sprintf(outbuf, "func %12s : hu %c%-2d ID %02lX Code 0x%02X (%s)",
               funclabel[func], hc, unit, vident, byte2, aliasp[index].label);
         }
         return outbuf;
      }

      decode_rfxsensor_data(aliasp, index, &inmap, &outmap, &temperature, &vsup, &advolt, &var2);

      if ( stype == RFX_T ) {
         sprintf(valbuf, FMT_RFXT, temperature);
         sprintf(outbuf, "func %12s : hu %c%-2d Temp %s%c (%s)",
            funclabel[func], hc, unit, valbuf, configp->rfx_tscale, aliasp[index].label);
      }
      else if ( stype == RFX_S ) {
         sprintf(outbuf, "func %12s : hu %c%-2d Vs %.2fV (%s)",
            funclabel[func], hc, unit, vsup, aliasp[index].label);
      }
      else if ( stype == RFX_H ) {
         if ( inmap & RFXO_H ) {
            if ( outmap & RFXO_H ) {
               sprintf(valbuf, FMT_RFXRH, var2);
               sprintf(outbuf, "func %12s : hu %c%-2d RH %s%% (%s)",
                  funclabel[func], hc, unit, valbuf, aliasp[index].label);
            }
            else {
               sprintf(outbuf, "func %12s : hu %c%-2d RH n/a (%s)",
                  funclabel[func], hc, unit, aliasp[index].label);
            }
         }
         else if ( inmap & RFXO_B ) {
            if ( outmap & RFXO_B ) {
               sprintf(valbuf, FMT_RFXBP, var2); 
               sprintf(outbuf, "func %12s : hu %c%-2d BP %s %s (%s)",
                  funclabel[func], hc, unit, valbuf, configp->rfx_bpunits, aliasp[index].label);
            }
            else {
               sprintf(outbuf, "func %12s : hu %c%-2d BP n/a %s (%s)",
                  funclabel[func], hc, unit, configp->rfx_bpunits, aliasp[index].label);
            }
         }
         else if ( inmap & RFXO_P ) {
            if ( outmap & RFXO_P ) {
               sprintf(outbuf, "func %12s : hu %c%-2d Pot %.2f%% (%s)",
                  funclabel[func], hc, unit, var2, aliasp[index].label);
            }
            else {
               sprintf(outbuf, "func %12s : hu %c%-2d Pot n/a (%s)",
                  funclabel[func], hc, unit, aliasp[index].label);
            }
         }
         else if ( inmap & RFXO_V ) {
            sprintf(valbuf, FMT_RFXVAD, var2);
            sprintf(outbuf, "func %12s : hu %c%-2d Vad %s %s (%s)",
               funclabel[func], hc, unit, valbuf, configp->rfx_vadunits, aliasp[index].label);
         }
         else if ( inmap & RFXO_T2 ) {
            sprintf(valbuf, FMT_RFXT, temperature);
            sprintf(outbuf, "func %12s : hu %c%-2d Temp2 %s%c (%s)",
               funclabel[func], hc, unit, valbuf, configp->rfx_tscale, aliasp[index].label);
         }
         else {
            sprintf(outbuf, "func %12s : hu %c%-2d Vadi %.2fV (%s)",
               funclabel[func], hc, unit, advolt, aliasp[index].label);
         }
      }

      if ( configp->display_sensor_intv == YES && (aliasp[index].optflags & MOPT_HEARTBEAT) ) {
         snprintf(outbuf + strlen(outbuf), sizeof(outbuf), " Intv %s ", intvstr);
      }
      if ( configp->show_change == YES ) {
         snprintf(outbuf + strlen(outbuf), sizeof(outbuf), "%s", (*sunchanged ? " UnChg" : " Chg"));
      }
   }
#endif /* HASRFXS */
   else if ( vtype == RF_RAWW800 ) {
      x10global.longvdata = (buf[3] << 24) | (buf[5] << 16) | (buf[6] << 8) | buf[7];
      sprintf(outbuf, "func %12s : Data (hex) %02X %02X %02X %02X",
         "RFrawdata", buf[3], buf[5], buf[7], buf[8]);
   }
   else if ( vtype == RF_NOISEW800 ) {
      x10global.longvdata = (buf[3] << 24) | (buf[5] << 16) | (buf[6] << 8) | buf[7];
      sprintf(outbuf, "func %12s : Data (hex) %02X %02X %02X %02X",
         "RFnoise", buf[3], buf[5], buf[7], buf[8]);
   }
   else if ( vtype == RF_RAWMR26 ) {
      x10global.longvdata = (buf[2] << 8) | buf[3];
      sprintf(outbuf, "func %12s : Data (hex) %02X %02X %02X %02X %02X",
         "RFrawdata", buf[2], buf[3], buf[5], buf[7], buf[8]);
   }
   else if ( vtype == RF_NOISEMR26 ) {
      x10global.longvdata = (buf[2] << 8) | buf[3];
      sprintf(outbuf, "func %12s : Data (hex) %02X %02X %02X %02X %02X",
         "RFnoise", buf[2], buf[3], buf[5], buf[7], buf[8]);
   }
   else {
      x10global.longvdata = (buf[3] << 24) | (buf[5] << 16) | (buf[6] << 8) | buf[7];
      sprintf(outbuf, "func %12s : Type 0x%02x Data (hex) %02x %02x %02x %02x",
         "RFdata", vtype, buf[3], buf[5], buf[6], buf[7]);
   }
 
   return outbuf;
}

/*---------------------------------------------------------------------+
 | Interpret Virtual data string, update the state, and test whether   |
 | any launch condition is satisfied.                                  |
 | buffer references:                                                  |
 |  ST_COMMAND, ST_LONGVDATA, vtype, seq, vidhi, vidlo, nbytes, bytes 1-N   |
 +---------------------------------------------------------------------*/
char *translate_long_virtual ( unsigned char *buf, unsigned char *sunchanged, int *launchp )
{
   extern char *translate_rfxmeter ( unsigned char *, unsigned char *, int * );
   extern char *translate_digimax ( unsigned char *, unsigned char *, int * );

   if ( buf[2] == RF_XMETER )
      return translate_rfxmeter(buf, sunchanged, launchp);
   else if ( buf[2] == RF_DIGIMAX )
      return translate_digimax(buf, sunchanged, launchp);

   return "";
}

/*----------------------------------------------------------------------------+
 | Translate general aux data.                                                |
 | Buffer reference: ST_COMMAND, ST_GENLONG, plus:                            |
 |  vtype, subtype, vidhi, vidlo, seq, buflen, buffer                         |
 |   [2]     [3]     [4]    [5]   [6]    [7]    [8+]                          |
 +----------------------------------------------------------------------------*/
char *translate_gen_longdata ( unsigned char *buff, unsigned char *sunchanged, int *launchp )
{
   if ( buff[2] == RF_OREGON || buff[2] == RF_ELECSAVE || buff[2] == RF_OWL ) {
      return translate_oregon(buff, sunchanged, launchp);
   }
   return "";
}


/*---------------------------------------------------------------------+
 | Interpret byte string sent to CM11A, update the state, and test     |
 | whether any launch condition is satisfied.                          |
 +---------------------------------------------------------------------*/
char *translate_sent ( unsigned char *buf, int len, int *launchp )
{ 
   static char outbuf[120];
   char hc;
   unsigned char hcode, func, level, unit, subunit, chksum;
   unsigned char xgroup, xfunc, xsubfunc, xtype, xdata;
   unsigned char newbuf[2];
   unsigned int bmap, memloc;
   int dims;
   double ddims;
   char *prefix, *suffix;

   *launchp = -1;

   chksum = checksum(buf, len);
   
   if ( buf[0] == 0 && len == 1 ) {
      return "";
   }
   else if ( (buf[0] == 0x04 && len == 2) ||
	     (buf[0] == 0x0C && len == 2 && buf[1] == 0x56 ) ) {
      /* Address */
      hcode = (buf[1] & 0xf0u) >> 4;
      hc = code2hc(hcode);
      unit = code2unit(buf[1] & 0x0fu);
      bmap = 1 << (buf[1] & 0x0fu);
      x10state_update_addr(buf[1], launchp); 
      sprintf(outbuf, "addr unit     %3d : hu %c%-2d (%s)",
         unit, hc, unit, lookup_label(hc, bmap));
      return outbuf;
   }
   else if ( (buf[0] & 0x07) == 0x06 && len == 2 ) {
      /* Standard function */
      level = (buf[0] & 0xf8u) >> 3;
      dims = level2dims(level, &prefix, &suffix);
      ddims = 100. * (double)dims / 210.;
      hcode = (buf[1] & 0xf0u) >> 4;
      hc = code2hc(hcode);
      func = buf[1] & 0x0f;
      if ( level > 0 && func != 4 && func != 5 ) {
         sprintf(outbuf, "Unknown transmission: %02x %02x", buf[0], buf[1]);
         return outbuf;
      }
      newbuf[0] = buf[1];
      newbuf[1] = dims;

      x10state_update_func(newbuf, launchp);

      switch ( func ) {
         case 4 :  /* Dim */
            sprintf(outbuf,
		 "func %12s : hc %c dim %%%02.0f [%s%d%s]",
               funclabel[func], hc, ddims, prefix, dims, suffix);
            return outbuf;
         case 5 :  /* Bright */
            sprintf(outbuf,
	         "func %12s : hc %c bright %%%02.0f [%s%d%s]",
               funclabel[func], hc, ddims, prefix, dims, suffix);
            return outbuf;
         case 10 : /* Preset level */
         case 11 : /* Preset level */
            level = rev_low_nybble(hcode) + 1;
            level += (func == 11) ? 16 : 0;
            sprintf(outbuf, "func %12s : level %d", funclabel[func], level);
            return outbuf;
         default:
            sprintf(outbuf, "func %12s : hc %c", funclabel[func], hc);
      }
   }
   else if ( (buf[0] == 0x07 && len == 5) ||
	     (buf[0] == 0x0F && len == 5 && chksum == 0x62) ) {
      /* Extended code function */
      char stmp[16];
      if ( (buf[1] & 0x0fu) != 0x07 ) {
         sprintf(outbuf, "Unknown transmission: %02x %02x %02x %02x %02x",
             buf[0], buf[1], buf[2], buf[3], buf[4]);
         return outbuf;
      }
      hcode = (buf[1] & 0xf0u) >> 4;
      hc = code2hc(hcode);
      unit = code2unit(buf[2] & 0x0fu);
      subunit = (buf[2] & 0xf0u) >> 4;
      bmap = 1 << (buf[2] & 0x0fu);
      xfunc = buf[4];
      xtype = (xfunc & 0xf0u) >> 4;
      xdata = buf[3];

      if ( xtype == 3 ) {
         x10state_update_ext3func(buf + 1, launchp);
      }
#ifdef HASEXT0
      else if ( xtype == 0 ) {
         x10state_update_ext0func(buf + 1, launchp);
      }
#endif
      else {
         x10state_update_extotherfunc(buf + 1, launchp);
      }

      stmp[0] = '\0';

      if ( xfunc == 0x30 ) {
         if ( (xdata & 0x30u) == 0x20 )
            sprintf(outbuf, "func      xGrpAdd : hu %c%-2d%s group %d.%-2d (%s)",
               hc, unit, stmp, (xdata >> 6), (xdata & 0x0fu) + 1, lookup_label(hc, bmap));
         else
            sprintf(outbuf, "func      xGrpAdd : hu %c%-2d%s group %d (%s)",
               hc, unit, stmp, (xdata >> 6), lookup_label(hc, bmap));
      }
      else if ( xfunc == 0x31 ) {
         if ( xdata & 0xc0u ) {
            sprintf(outbuf, "func      xPreset : hu %c%-2d%s level %d ramp %d (%s)",
               hc, unit, stmp, xdata & 0x3fu, (xdata & 0xc0u) >> 6, lookup_label(hc, bmap));
         }
         else {
            sprintf(outbuf, "func      xPreset : hu %c%-2d%s level %d (%s)",
               hc, unit, stmp, xdata, lookup_label(hc, bmap));
         }
      }
      else if ( xfunc == 0x32 )
         sprintf(outbuf, "func   xGrpAddLvl : hu %c%-2d%s group %d level %d (%s)",
            hc, unit, stmp, ((xdata & 0xc0) >> 6), (xdata & 0x3f), lookup_label(hc, bmap));
      else if ( xfunc == 0x33 )
         sprintf(outbuf, "func       xAllOn : hc %c", hc);
      else if ( xfunc == 0x34 )
         sprintf(outbuf, "func      xAllOff : hc %c", hc);
      else if ( xfunc == 0x35 ) {
         if ( (xdata & 0x30) == 0 ) {
            sprintf(outbuf, "func      xGrpRem : hu %c%-2d%s group(s) %s (%s)",
               hc, unit, stmp, linmap2list(xdata & 0x0f), lookup_label(hc, bmap));
         }
         else {
            sprintf(outbuf, "func   xGrpRemAll : hc %c group(s) %s",
               hc, linmap2list(xdata & 0x0f));
         }
      }
      else if ( xfunc == 0x36 ) {
         xsubfunc = (xdata & 0x30u) >> 4;
         xgroup = (xdata & 0xc0u) >> 6;
         if ( xsubfunc == 0 )
            sprintf(outbuf, "func     xGrpExec : hc %c group %d",
               hc, xgroup);
         else if ( xsubfunc == 2 )
            sprintf(outbuf, "func     xGrpExec : hc %c group %d.%-2d",
               hc, xgroup, (xdata & 0x0fu) + 1);
         else if ( xsubfunc == 1 )
            sprintf(outbuf, "func      xGrpOff : hc %c group %d",
               hc, xgroup);
         else
            sprintf(outbuf, "func      xGrpOff : hc %c group %d.%-2d",
               hc, xgroup, (xdata & 0x0fu) + 1);

      }
      else if ( xfunc == 0x37 ) {
         xsubfunc = (xdata & 0x30u) >> 4;
         xgroup = (xdata & 0xc0u) >> 6;
         if ( xsubfunc == 0 ) 
            sprintf(outbuf, "func   xStatusReq : hu %c%-2d%s (%s)",
               hc, unit, stmp, lookup_label(hc, bmap));
         else if ( xsubfunc == 1 )
            sprintf(outbuf, "func     xPowerUp : hu %c%-2d%s (%s)",
                hc, unit, stmp, lookup_label(hc, bmap));
         else if ( xsubfunc == 2 )
            sprintf(outbuf, "func   xGrpStatus : hu %c%-2d%s group %d (%s)",
                hc, unit, stmp, (xdata & 0xc0) >> 6, lookup_label(hc, bmap));
         else 
            sprintf(outbuf, "func   xGrpStatus : hu %c%-2d%s group %d.%-2d (%s)",
               hc, unit, stmp, xgroup, (xdata & 0x0fu) + 1, lookup_label(hc, bmap));
      }
      else if ( xfunc == 0x38 ) {
         if ( xdata & 0x40 )
            sprintf(outbuf, "func   xStatusAck : hu %c%-2d%s Switch %-3s %s (%s)",
               hc, unit, stmp, ((xdata & 0x3f) ? "On " : "Off"),
               ((xdata & 0x80) ? "LoadOK" : "NoLoad"), 
               lookup_label(hc, bmap));
         else
            sprintf(outbuf, "func   xStatusAck : hu %c%-2d%s Lamp level %d, %s (%s)",
               hc, unit, stmp, xdata & 0x3f, 
               ((xdata & 0x80) ? "BulbOK" : "NoBulb"), 
               lookup_label(hc, bmap));
      }
      else if ( xfunc == 0x3b ) {
           sprintf(outbuf, "func      xConfig : hc %c mode=%d",
              hc, xdata);
      }
      else if ( xfunc == 0x3C ) {
         /* Unsupported by X-10 modules */
         xsubfunc = (xdata & 0x30u) >> 4;
         xgroup = (xdata & 0xc0u) >> 6;
         if ( xsubfunc == 0 )
            sprintf(outbuf, "func      xGrpDim : hc %c group %d",
               hc, xgroup);
         else if ( xsubfunc == 2 )
            sprintf(outbuf, "func      xGrpDim : hc %c group %d.%-2d",
               hc, xgroup, (xdata & 0x0fu) + 1);
         else if ( xsubfunc == 1 )
            sprintf(outbuf, "func   xGrpBright : hc %c group %d",
               hc, xgroup);
         else
            sprintf(outbuf, "func   xGrpBright : hc %c group %d.%-2d",
               hc, xgroup, (xdata & 0x0fu) + 1);

      }

#ifdef HASEXT0
      else if ( xfunc == 0x01 ) {
           sprintf(outbuf, "func    shOpenLim : hu %c%-2d%s level %d (%s)",
              hc, unit, stmp, xdata & 0x1f, lookup_label(hc, bmap));
      }
      else if ( xfunc == 0x03 ) {
           sprintf(outbuf, "func       shOpen : hu %c%-2d%s level %d (%s)",
              hc, unit, stmp, xdata & 0x1f, lookup_label(hc, bmap));
      }
      else if ( xfunc == 0x02 ) {
           sprintf(outbuf, "func     shSetLim : hu %c%-2d%s level %d (%s)",
              hc, unit, stmp, xdata & 0x1f, lookup_label(hc, bmap));
      }
      else if ( xfunc == 0x04 ) {
           sprintf(outbuf, "func    shOpenAll : hc %c", hc);
      }
      else if ( xfunc == 0x0B ) {
           sprintf(outbuf, "func   shCloseAll : hc %c", hc);
      }
#endif  /* HASEXT0 block */

      else if ( xfunc == 0xff )
            sprintf(outbuf, "func      ExtCode : Incomplete xcode in buffer.");
      else {
            sprintf(outbuf, "func     xFunc %02x : hu %c%-2d%s data=0x%02x (%s)",
            xfunc, hc, unit, stmp, xdata, lookup_label(hc, bmap));
      }

      return outbuf;
   }
   else if ( buf[0] == 0xfb && len == 19 ) {
      chksum = checksum(buf + 1, len - 1);
      memloc = (buf[1] << 8) + buf[2];
      sprintf(outbuf, "Upload to EEPROM location %03x, checksum = %02x",
          memloc, chksum);
      return outbuf;
   }
   else if ( buf[0] == 0x8b && len == 1 ) {
      sprintf(outbuf, "Request CM11A info");
      return outbuf;
   }
   else if ( buf[0] == 0x9b && len == 7 ) {
      sprintf(outbuf, "Update CM11A info");
      return outbuf;
   }
   else if ( buf[0] == 0xeb && len == 1 ) {
      *outbuf = '\0';
/*
      sprintf(outbuf, "Enable serial ring signal");
*/
      return outbuf;
   }
   else if ( buf[0] == 0xdb && len == 1 ) {
      *outbuf = '\0';
/*
      sprintf(outbuf, "Disable serial ring signal");
*/
      return outbuf;
   }
   else if ( buf[0] == ST_COMMAND && len == 3 ) {
      sprintf(outbuf, "State command");
      return outbuf;
   }
   else if ( buf[0] == 0xfb && len == 43 && configp->device_type & DEV_CM10A ) {
      sprintf(outbuf, "Initialize CM10A");
      return outbuf;
   }
   else {
      sprintf(outbuf, "Unknown transmission: %d bytes, 1st byte = %02x",
         len, buf[0]);
   }
   return outbuf;
}


/*---------------------------------------------------------------------+
 | Interpret bytes sent to CM17A                                       |
 +---------------------------------------------------------------------*/
char *translate_rf_sent ( unsigned char *buf, int *launchp )
{
   extern void xlate_rf( unsigned char, char **, unsigned int, char *,
                                               int *, unsigned char * );

   static char   outbuf[80];
   char verb[16];
   unsigned char type, bursts, nosw, addr;
   char          hc;
   unsigned int  rfword, dimlevel;
   char          *fname, *vp, *nsw;
   int           unit;
   unsigned char updatebuf[8];
   int           launchaddr, launchfunc;
   extern unsigned int signal_source;

   type = buf[2];
   rfword = buf[3] << 8 | buf[4];
   bursts = buf[5];
   xlate_rf( type, &fname, rfword, &hc, &unit, &nosw );


   if ( configp->disp_rf_xmit == VERBOSE ) {
      sprintf(verb, " [%02x %02x %d]", buf[3], buf[4], buf[5]);
      vp = verb;
   }
   else {
      vp = "";
   }

   nsw = ( nosw && (type == 0 || unit == 1 || unit == 9) ) ? " NoSw" : "";

   if ( type == 9 ) 
      sprintf(outbuf, "RF           fArb : bytes 0x%02X 0x%02X count %d%s",
		      buf[3], buf[4], buf[5], vp);
   else if ( type == 10 ) 
      sprintf(outbuf, "RF           fArw : word 0x%04X count %d%s",
		      ((buf[3] << 8) | buf[4]), buf[5], vp);
   else if ( type == 11 ) 
      sprintf(outbuf, "RF           fLux : word 0x%04X count %d%s",
		      ((buf[3] << 8) | buf[4]), buf[5], vp);
   else if ( type == 2 || type == 3 )
      sprintf(outbuf, "RF   %12s : hu %c%-2d%s%s", fname, hc, unit, nsw, vp);
   else if ( type == 4 || type == 5 )
      sprintf(outbuf, "RF   %12s : hc %c count %d%s", fname, hc, bursts, vp);
   else
      sprintf(outbuf, "RF   %12s : hc %c%s%s", fname, hc, nsw, vp);

   *launchp = launchaddr = launchfunc = -1;

   if ( configp->process_xmit == YES ) {
      signal_source = XMTF;
      if ( type == 2 || type == 3 ) {
         addr = (hc2code(hc) << 4) | unit2code(unit);
         x10state_update_addr(addr, &launchaddr);
      }
      if ( type < 7 ) {
         updatebuf[0] = (hc2code(hc) << 4) | (type & 0x0f);
         dimlevel = min((210 * bursts) / 32, 210); 
         updatebuf[1] = (unsigned char)dimlevel;
         x10state_update_func(updatebuf, &launchfunc);
      }  
      *launchp = (launchfunc < 0) ? launchaddr : launchfunc;
   }

   return outbuf;
}
        

/*---------------------------------------------------------------------+
 | Interpret byte string sent to CM11A.                                |
 +---------------------------------------------------------------------*/
char *translate_other ( unsigned char *buf, int len )
{ 
   static char outbuf[80];
   unsigned char chksum;
   unsigned int memloc;

   if ( buf[0] == 0 && len == 1 ) {
      return "";
   }
   else if ( buf[0] == 0xfb && len == 19 ) {
      chksum = checksum(buf + 1, len - 1);
      memloc = (buf[1] << 8) + buf[2];
      sprintf(outbuf, "Upload to EEPROM location %03x, checksum = %02x",
          memloc, chksum);
      return outbuf;
   }
   else if ( buf[0] == 0x8b && len == 1 ) {
      sprintf(outbuf, "Request CM11A info");
      return outbuf;
   }
   else if ( buf[0] == 0x9b && len == 7 ) {
      sprintf(outbuf, "Update CM11A info");
      return outbuf;
   }
   else if ( buf[0] == 0xeb && len == 1 ) {
      *outbuf = '\0';
/*
      sprintf(outbuf, "Enable serial ring signal");
*/
      return outbuf;
   }
   else if ( buf[0] == 0xdb && len == 1 ) {
      *outbuf = '\0';
/*
      sprintf(outbuf, "Disable serial ring signal");
*/
      return outbuf;
   }
   else if ( buf[0] == ST_COMMAND && len == 3 ) {
      sprintf(outbuf, "State command");
      return outbuf;
   }
   else if ( buf[0] == 0xfb && len == 43 && configp->device_type & DEV_CM10A ) {
      sprintf(outbuf, "Initialize CM10A");
      return outbuf;
   }
   else {
      sprintf(outbuf, "Unknown transmission: %d bytes, 1st byte = %02x",
         len, buf[0]);
   }
   return outbuf;
}

/*---------------------------------------------------------------------+
 | Create argument list string for heyuhelper.                         |
 | If argument big = 0, a token for only the last unit is included,    |
 | otherwise tokens are include for each unit in the launcher bitmap.  |
 +---------------------------------------------------------------------*/
char *helper_string ( LAUNCHER *launcherp, int big )
{
   int           ucode;
   unsigned int  bitmap, mask;
   unsigned char actfunc;
   char          hc;
   char          buffer[256];
   char          minibuf[32];
   char          *dup;

   bitmap = launcherp->bmaplaunch;
   actfunc = launcherp->actfunc;
   hc = tolower((int)code2hc(launcherp->hcode));

   strncpy2(buffer, ((big) ? "bighelper=" : "helper="), sizeof(buffer) - 1);
   for ( ucode = 15; ucode >= 0; ucode-- ) {
      mask = 1 << ucode;
      if ( bitmap & mask ) {
         sprintf(minibuf, "%c%d%s ",
            hc, code2unit(ucode), funclabel[actfunc]);
         strncat(buffer, minibuf, sizeof(buffer) - 1 - strlen(buffer));
         if ( !big )
            break;
      }
   }
   if ( (dup = strdup(buffer)) == NULL ) {
      fprintf(stderr, "Out of memory in helper_string()\n");
      exit(1);
   }
   return dup;
}
 

/*---------------------------------------------------------------------+
 | Return the heyu state in the same bitmap format as is put into the  |
 | environment when a script is launched.                              |
 +---------------------------------------------------------------------*/
unsigned long get_heyu_state ( unsigned char hcode, unsigned char ucode, int mode ) 
{
   unsigned int  bitmap;
   unsigned long value;
   int           level;

   bitmap = (1 << ucode);

   if ( mode == PCTMODE ) {   
      /* Start with the dim level (0-100) */
      level = x10state[hcode].dimlevel[ucode];
      if ( modmask[Ext3Mask][hcode] & bitmap )
         value = ext3level2pct(level);
      else if ( modmask[Ext0Mask][hcode] & bitmap )
         value = ext0level2pct(level);
      else if ( modmask[PresetMask][hcode] & bitmap )
         value = presetlevel2pct(level);
      else if ( modmask[StdMask][hcode] & bitmap )
         value = dims2pct(level);
      else if ( vmodmask[VkakuMask][hcode] & bitmap )
         value = 100 * level / 15;
      else 
         value = 100 * level / ondimlevel[hcode][ucode];
   }
   else {
      /* Start with the raw dim level */
      level = x10state[hcode].dimlevel[ucode];
      if ( modmask[PresetMask][hcode] & bitmap )
         value = level + 1;
      else 
         value = level;
   }

   /* Add the state bits */
   if ( bitmap & ~(modmask[DimMask][hcode] | modmask[BriMask][hcode]) )
      value |= HEYUMAP_APPL;
   if ( bitmap & x10state[hcode].state[SpendState] )
      value |= HEYUMAP_SPEND;        
   if ( bitmap & ~x10state[hcode].state[OnState] )
      value |= HEYUMAP_OFF;
   if ( bitmap & x10state[hcode].addressed )
      value |= HEYUMAP_ADDR;
   if ( bitmap & x10state[hcode].state[ChgState] )
      value |= HEYUMAP_CHG;
   if ( bitmap & x10state[hcode].state[DimState] )
      value |= HEYUMAP_DIM;
   if ( bitmap & x10state[hcode].state[ValidState] )
      value |= HEYUMAP_SIGNAL;
//   if ( bitmap & ~x10state[hcode].state[AlertState] & x10state[hcode].state[ValidState] & vmodmask[VsecMask][hcode])
   if ( bitmap & x10state[hcode].state[ClearState] )
      value |= HEYUMAP_CLEAR;
   if ( bitmap & x10state[hcode].state[AlertState] )
      value |= HEYUMAP_ALERT;
//   if ( bitmap & ~x10state[hcode].state[AuxAlertState] & x10state[hcode].state[ValidState] & vmodmask[VsecMask][hcode])
   if ( bitmap & x10state[hcode].state[AuxClearState] )
      value |= HEYUMAP_AUXCLEAR;
   if ( bitmap & x10state[hcode].state[AuxAlertState] )
      value |= HEYUMAP_AUXALERT;
   if ( bitmap & x10state[hcode].state[ActiveState] )
      value |= HEYUMAP_ACTIVE;
   if ( bitmap & x10state[hcode].state[InactiveState] )
      value |= HEYUMAP_INACTIVE;
   if ( bitmap & x10state[hcode].state[ModChgState] )
      value |= HEYUMAP_MODCHG;
   if ( bitmap & x10state[hcode].state[OnState] )
      value |= HEYUMAP_ON;


   return value;
}

#if 0  
/*---------------------------------------------------------------------+
 | Return the heyu state in the same bitmap format as is put into the  |
 | environment when a script is launched with "-rawlevel" option.      |
 +---------------------------------------------------------------------*/
unsigned long get_heyu_rawstate_old ( unsigned char hcode, unsigned char ucode ) 
{
   unsigned int  bitmap;
   unsigned long value;
   int           level;

   bitmap = (1 << ucode);
   
   /* Start with the raw dim level */
   level = x10state[hcode].dimlevel[ucode];
   if ( modmask[PresetMask][hcode] & bitmap )
      value = level + 1;
   else 
      value = level;

   /* Add the state bits */
   if ( bitmap & ~(modmask[DimMask][hcode] | modmask[BriMask][hcode]) )
      value |= HEYUMAP_APPL;         
   if ( bitmap & x10state[hcode].state[SpendState] )
      value |= HEYUMAP_SPEND;        
   if ( bitmap & ~x10state[hcode].state[OnState] )
      value |= HEYUMAP_OFF;
   if ( bitmap & x10state[hcode].addressed )
      value |= HEYUMAP_ADDR;
   if ( bitmap & x10state[hcode].state[ChgState] )
      value |= HEYUMAP_CHG;
   if ( bitmap & x10state[hcode].state[DimState] )
      value |= HEYUMAP_DIM;
   if ( bitmap & x10state[hcode].state[ValidState] )
      value |= HEYUMAP_SIGNAL;
   if ( bitmap & x10state[hcode].state[ActiveState] )
      value |= HEYUMAP_ACTIVE;
   if ( bitmap & x10state[hcode].state[InactiveState] )
      value |= HEYUMAP_INACTIVE;
   if ( bitmap & x10state[hcode].state[ActiveChgState] )
      value |= HEYUMAP_ACTIVECHG;
   if ( bitmap & x10state[hcode].state[OnState] )
      value |= HEYUMAP_ON;

   return value;
}
#endif

unsigned long get_secflag_state ( unsigned char hcode, unsigned char ucode)
{
   unsigned long flagvalue;
   unsigned int  bitmap;

   bitmap = 1 << ucode;

   flagvalue = 0;

   if ( x10state[hcode].state[LoBatState] & bitmap ) flagvalue |= FLAGMAP_LOBAT;
   if ( x10state[hcode].vflags[ucode] & RFX_ROLLOVER ) flagvalue |= FLAGMAP_ROLLOVER;
//   if ( x10state[hcode].state[SwMinState] & bitmap ) flagvalue |= FLAGMAP_SWMIN;
//   if ( x10state[hcode].state[SwMaxState] & bitmap ) flagvalue |= FLAGMAP_SWMAX;
   if ( x10state[hcode].vflags[ucode] & SEC_MIN    ) flagvalue |= FLAGMAP_SWMIN;
   if ( x10state[hcode].vflags[ucode] & SEC_MAX    ) flagvalue |= FLAGMAP_SWMAX;
   if ( x10state[hcode].vflags[ucode] & SEC_MAIN   ) flagvalue |= FLAGMAP_MAIN;
   if ( x10state[hcode].vflags[ucode] & SEC_AUX    ) flagvalue |= FLAGMAP_AUX;
   if ( x10state[hcode].state[TamperState] & bitmap ) flagvalue |= FLAGMAP_TAMPER;

   return flagvalue;
}

unsigned long get_rfxflag_state ( unsigned char hcode, unsigned char ucode)
{
   unsigned long flagvalue;
   unsigned int  bitmap;

   bitmap = 1 << ucode;

   flagvalue = 0;

   if ( x10state[hcode].state[LoBatState] & bitmap ) flagvalue |= FLAGMAP_LOBAT;
   if ( x10state[hcode].vflags[ucode] & RFX_ROLLOVER ) flagvalue |= FLAGMAP_ROLLOVER;
   if ( x10state[hcode].vflags[ucode] & ORE_TMIN ) flagvalue |= FLAGMAP_TMIN;
   if ( x10state[hcode].vflags[ucode] & ORE_TMAX ) flagvalue |= FLAGMAP_TMAX;
   if ( x10state[hcode].vflags[ucode] & ORE_RHMIN ) flagvalue |= FLAGMAP_RHMIN;
   if ( x10state[hcode].vflags[ucode] & ORE_RHMAX ) flagvalue |= FLAGMAP_RHMAX;
   if ( x10state[hcode].vflags[ucode] & ORE_BPMIN ) flagvalue |= FLAGMAP_BPMIN;
   if ( x10state[hcode].vflags[ucode] & ORE_BPMAX ) flagvalue |= FLAGMAP_BPMAX;

   return flagvalue;
}

unsigned long get_oreflag_state ( unsigned char hcode, unsigned char ucode)
{
   unsigned long flagvalue;
   unsigned int  bitmap;

   bitmap = 1 << ucode;

   flagvalue = 0;

   if ( x10state[hcode].state[LoBatState] & bitmap ) flagvalue |= FLAGMAP_LOBAT;
   if ( x10state[hcode].vflags[ucode] & RFX_ROLLOVER ) flagvalue |= FLAGMAP_ROLLOVER;
   if ( x10state[hcode].vflags[ucode] & ORE_TMIN ) flagvalue |= FLAGMAP_TMIN;
   if ( x10state[hcode].vflags[ucode] & ORE_TMAX ) flagvalue |= FLAGMAP_TMAX;
   if ( x10state[hcode].vflags[ucode] & ORE_RHMIN ) flagvalue |= FLAGMAP_RHMIN;
   if ( x10state[hcode].vflags[ucode] & ORE_RHMAX ) flagvalue |= FLAGMAP_RHMAX;
   if ( x10state[hcode].vflags[ucode] & ORE_BPMIN ) flagvalue |= FLAGMAP_BPMIN;
   if ( x10state[hcode].vflags[ucode] & ORE_BPMAX ) flagvalue |= FLAGMAP_BPMAX;

   return flagvalue;
}

unsigned long get_dmxflag_state ( unsigned char hcode, unsigned char ucode)
{
   unsigned long flagvalue;
   unsigned int  bitmap;

   bitmap = 1 << ucode;

   flagvalue = 0;

   if ( x10state[hcode].state[LoBatState] & bitmap ) flagvalue |= FLAGMAP_LOBAT;
   if ( x10state[hcode].vflags[ucode] & DMX_SET ) flagvalue |= FLAGMAP_DMXSET;
   if ( x10state[hcode].vflags[ucode] & DMX_HEAT ) flagvalue |= FLAGMAP_DMXHEAT;
   if ( x10state[hcode].vflags[ucode] & DMX_INIT ) flagvalue |= FLAGMAP_DMXINIT;
   if ( x10state[hcode].vflags[ucode] & DMX_TEMP ) flagvalue |= FLAGMAP_DMXTEMP;

   return flagvalue;
}

unsigned long get_vflag_state ( unsigned char hcode, unsigned char ucode )
{
   return get_secflag_state(hcode, ucode) |
          get_rfxflag_state(hcode, ucode) |
          get_oreflag_state(hcode, ucode) |
          get_dmxflag_state(hcode, ucode)   ;
}

  
/*---------------------------------------------------------------------+
 | Return the xtend state in the same bitmap format as is put into the |
 | environment when a script is launched.                              |
 +---------------------------------------------------------------------*/
unsigned long get_xtend_state ( unsigned char hcode, unsigned char ucode ) 
{
   unsigned int  bitmap;
   unsigned long value;

   bitmap = (1 << ucode);

   value = 0;
   
   /* Add the state bits */
   if ( bitmap & ~(modmask[DimMask][hcode] | modmask[BriMask][hcode]) )
      value |= XTMAP_APPL;         
   if ( bitmap & x10state[hcode].addressed )
      value |= XTMAP_ADDR;
   if ( bitmap & x10state[hcode].state[OnState] )
      value |= XTMAP_ON;

   return value;
}
  
/*---------------------------------------------------------------------+
 | Allocate memory for argument envstr.                                |
 +---------------------------------------------------------------------*/
char *add_envptr ( char *envstr )
{
   char *dup;
   char *out_of_memory = "add_envptr() : Out of memory\n";
      
   
   if ( (dup = strdup(envstr)) == NULL ) {
      fprintf(stderr, "%s", out_of_memory);
      exit(1);
   }
   return dup;   
}
   
/*---------------------------------------------------------------------+
 | Create a minimal environment for the -noenv option                  |
 +---------------------------------------------------------------------*/
char **create_noenv_environment ( LAUNCHER *launcherp, unsigned char daemon )
{
   
   extern char   **environ;

   int           j, size, newsize;
   char          **envp, **ep;
   char          minibuf[512];
   static int    sizchrptr = sizeof(char *);


   if ( daemon == D_RELAY || (launcherp && launcherp->type == L_POWERFAIL) )
      putenv("HEYU_PARENT=RELAY");    
   else
      putenv("HEYU_PARENT=ENGINE");

   /* Get length of original environment */
   size = 0;
   while ( environ[size] != NULL )
      size++;

   newsize = size + 50;

   if ( (envp = calloc(newsize, sizchrptr)) == NULL ) {
      fprintf(stderr, "create_noenv__environment() : out_of_memory\n");
      exit(1);
   }

   ep = envp;

   *ep++ = add_envptr("IAMNOENV=1");

   /* Put config file pathname in environment for child processes */
   sprintf(minibuf, "X10CONFIG=%s", pathspec(heyu_config));
   *ep++ = add_envptr(minibuf);


   /* Append the user's original environment */
   for ( j = 0; j < size; j++ ) {
      *ep++ = add_envptr(environ[j]);
   }

   /* Add NULL terminator */
   *ep++ = NULL;

   return envp;
}

/*---------------------------------------------------------------------+
 | Create the environment for heyu scripts.                            |
 +---------------------------------------------------------------------*/
char **create_heyu_environment( LAUNCHER *launcherp, unsigned char option)
{
   extern char   **environ;
   
   int           j, k, size, newsize, aliasindex;
   unsigned char hcode, ucode, lucode, actfunc /*, vtype*/;
   char          **envp, **ep, *pathptr, *newpathptr;
   char          hc;
   unsigned int  bitmap /*, addressed, onstate, appliance*/;
   unsigned long value, /*flagvalue, allflagvalue,*/ vflags;
//   unsigned int  dimstate, chgstate, modchgstate, spendstate;
   unsigned int  actsource;
   char          *srcname;
   long int      dawn, dusk, systime;
   int           expire;
   char          *aliaslabel;
   char          minibuf[512];
   static int    sizchrptr = sizeof(char *);
   time_t        now;
   struct tm     *tms;
   extern unsigned char bootflag;
   extern int    alias_vtype_count ( unsigned char );
   ALIAS         *aliasp;

   unsigned long longvdata;
   int           unit;

#ifdef HASDMX
   int           tempc, settempc;
   unsigned char status, oostatus;
#endif /* HASDMX */


#if (defined(HASRFXS) || defined(HASRFXM))
   char          valbuf[80];
#endif 

#ifdef HASRFXS
   unsigned int  inmap, outmap;
   double        temp, vsup, vad, var2;
#endif /* HASRFXS */

#ifdef HASRFXM
   unsigned long rfxdata;
   extern int    npowerpanels;
   extern int    powerpanel_query(unsigned char, unsigned long *);
#endif /* HASRFXM */

#ifdef HASORE
   int    otempc;
   double otemp;
   int    obaro;
   double odbaro;
   int    cweight;
   double weight;
   int    channel;
   int    blevel;
   int    fcast;
   int    deciamps;
   int    dwind;
   double dblwind;
   unsigned long rain;
   double dblrain;
   extern char *forecast_txt( int );
   int    uvfactor;
   double dblpower, dblenergy;
#endif /* HASORE */

#if 0
static struct {
   char          *query;
   unsigned long  mask;
} heyumaskval[] = {
   {"whatLevel",   HEYUMAP_LEVEL},
   {"isAppl",      HEYUMAP_APPL },
   {"isSpend",     HEYUMAP_SPEND},
   {"isOff",       HEYUMAP_OFF  },
   {"isAddr",      HEYUMAP_ADDR },
   {"isChg",       HEYUMAP_CHG  },
   {"isDim",       HEYUMAP_DIM  },
   {"isValid",     HEYUMAP_SIGNAL },
   {"isClear",     HEYUMAP_CLEAR},
   {"isAlert",     HEYUMAP_ALERT},
   {"isAuxClear",  HEYUMAP_AUXCLEAR},
   {"isAuxAlert",  HEYUMAP_AUXALERT},
   {"isActive",    HEYUMAP_ACTIVE},
   {"isInactive",  HEYUMAP_INACTIVE},
   {"isModChg",    HEYUMAP_MODCHG},
   {"isOn",        HEYUMAP_ON   },
   { NULL,        0            },
};

static struct {
   char           *query;
   unsigned long  mask;
} heyusecmaskval[] = {
   {"isLoBat",    FLAGMAP_LOBAT    },
   {"isRollover", FLAGMAP_ROLLOVER },
   {"isSwMin",    FLAGMAP_SWMIN    },
   {"isSwMax",    FLAGMAP_SWMAX    },
   {"isMain",     FLAGMAP_MAIN     },
   {"isAux",      FLAGMAP_AUX      },
   {"isTamper",   FLAGMAP_TAMPER   },
   {"isTmin",     FLAGMAP_TMIN     },
   {"isTmax",     FLAGMAP_TMAX     },
   {"isRHmin",    FLAGMAP_RHMIN    },
   {"isRHmax",    FLAGMAP_RHMAX    },
   {"isBPmin",    FLAGMAP_BPMIN    },
   {"isBPmax",    FLAGMAP_BPMAX    },
   {"isDmxSet",   FLAGMAP_DMXSET   },
   {"isDmxHeat",  FLAGMAP_DMXHEAT  },
   {"isDmxTemp",  FLAGMAP_DMXTEMP  },
   { NULL,        0                },
};
#endif  /* Reference */

   aliasp = configp->aliasp;
   unit = 0;

   longvdata = x10global.longvdata;

   if ( launcherp->type != L_POWERFAIL )   
      putenv("HEYU_PARENT=ENGINE");
   else
      putenv("HEYU_PARENT=RELAY");

   /* Get length of original environment */
   size = 0;
   while ( environ[size] != NULL )
      size++;

   newsize = size + 16 + 77 + alias_count() +
         256 +                       /* States */
         256 +                       /* vFlag states */
         32 * NUM_FLAG_BANKS +       /* Common flags */
         64 * NUM_COUNTER_BANKS +    /* Counters and counter-zero flags */
         64 * NUM_TIMER_BANKS + 1 +  /* Timers and timer-zero flags */
         6 * alias_vtype_count(RF_XSENSOR) +
         4 * alias_vtype_count(RF_XMETER) +
        14 * alias_vtype_count(RF_DIGIMAX) +
        36 * alias_vtype_count(RF_OREGON) +
        12 * alias_vtype_count(RF_ELECSAVE) +
        14 * alias_vtype_count(RF_OWL) +
         2 * alias_vtype_count(RF_SEC);

   if ( (envp = calloc(newsize, sizchrptr)) == NULL ) {
      fprintf(stderr, "create_heyu_environment() : out of memory\n");
      exit(1);
   }

   ep = envp;

   *ep++ = add_envptr("IAMHEYU=1");

   /* Put config file pathname in environment for child processes */
   sprintf(minibuf, "X10CONFIG=%s", pathspec(heyu_config));
   *ep++ = add_envptr(minibuf);

   /* Add version and release date */
   sprintf(minibuf, "HEYU_VERSION=%s", VERSION);
   *ep++ = add_envptr(minibuf);
   sprintf(minibuf, "HEYU_RELEASE_DATE=%s", RELEASE_DATE);
   *ep++ = add_envptr(minibuf);

   /* Prefix and/or Append to PATH if requested */
   if ( (*configp->launchpath_prefix || *configp->launchpath_append) && (pathptr = getenv("PATH")) ) {
      /* Old PATH might be very large - malloc space */
      newpathptr = malloc(8 + strlen(pathptr) + strlen(configp->launchpath_prefix) + strlen(configp->launchpath_append));
      if ( newpathptr ) {
         strcpy(newpathptr, "PATH=");
         if ( *configp->launchpath_prefix ) {
            strcat(newpathptr, configp->launchpath_prefix);
            strcat(newpathptr, ":");
         }
         strcat(newpathptr, pathptr);
         if ( *configp->launchpath_append ) {
            strcat(newpathptr, ":");
            strcat(newpathptr, configp->launchpath_append);
         }       
         putenv(newpathptr);
      }
   }

   /* Create the various masks */
   j = 0;
   while ( heyumaskval[j].query ) {
      sprintf(minibuf, "%s=%lu", heyumaskval[j].query, heyumaskval[j].mask);
      *ep++ = add_envptr(minibuf);
      j++;
   }

   j = 0;
   while ( heyusecmaskval[j].query ) {
      sprintf(minibuf, "%s=%lu", heyusecmaskval[j].query, heyusecmaskval[j].mask);
      *ep++ = add_envptr(minibuf);
      j++;
   }

   hcode = launcherp->hcode;
   actfunc = launcherp->actfunc;
   actsource = launcherp->actsource;
   
   /* Variables for scripts launched by a "normal" launcher, */
   /* i.e., not a powerfail or address launcher.             */
   if ( launcherp->type == L_NORMAL ) {
      /* Put in the variables 'helper' and 'bighelper' */
      /* with arguments for heyuhelper                 */
      *ep++ = helper_string(launcherp, 0);
      *ep++ = helper_string(launcherp, 1);

   }

   /* Variables for scripts launched by normal or address launchers */
   if ( launcherp->type == L_NORMAL ||
        launcherp->type == L_ADDRESS       ) {

      if ( launcherp->type == L_NORMAL ) {
         sprintf(minibuf, "X10_Function=%s", funclabel[actfunc]);
	 *ep++ = add_envptr(minibuf);
         sprintf(minibuf, "X10_function=%s", funclabel[actfunc]);
	 strlower(minibuf + 1);
	 *ep++ = add_envptr(minibuf);
      }
      else {
         sprintf(minibuf, "X10_Function=none");
         *ep++ = add_envptr(minibuf);
      }
    
      sprintf(minibuf, "X10_Housecode=%c", code2hc(hcode));
      *ep++ = add_envptr(minibuf);

      sprintf(minibuf, "X10_Linmap=%u", x10map2linmap(launcherp->bmaplaunch));
      *ep++ = add_envptr(minibuf);

      sprintf(minibuf, "X10_LastUnit=%d", x10state[hcode].lastunit);
      *ep++ = add_envptr(minibuf);

      if ( launcherp->bmaplaunch != 0 ) {

         lucode = 0xff;
         if ( x10state[hcode].lastunit > 0 ) {
            lucode = unit2code(x10state[hcode].lastunit);
            sprintf(minibuf, "X10_LastUnitAlias=%s", lookup_label(code2hc(hcode), lucode) );
         }

         if ( lucode <= 0x0f && (launcherp->bmaplaunch & (1 << lucode)) ) {
            ucode = lucode;
         }
         else {
            for ( ucode = 0; ucode < 16; ucode++ ) {
               if ( launcherp->bmaplaunch & (1 << ucode) )
                  break;
            }
         }

         sprintf(minibuf, "X10_Unit=%d", code2unit(ucode));
         *ep++ = add_envptr(minibuf);

         sprintf(minibuf, "X10_UnitAlias=%s", lookup_label(code2hc(hcode), (1 << ucode)) );
         *ep++ = add_envptr(minibuf);

         sprintf(minibuf, "X10_Timestamp=%ld", (unsigned long)x10state[hcode].timestamp[ucode]);
         *ep++ = add_envptr(minibuf);

         sprintf(minibuf, "X10_Vident=%lu", x10state[hcode].vident[ucode]);
         *ep++ = add_envptr(minibuf);

         /* Security functions local switch flags */
         if ( actfunc == ArmFunc ) {
            sprintf(minibuf, "X10_swHome=%d", ((x10state[hcode].vflags[ucode] & SEC_HOME) ? 1 : 0) );
            *ep++ = add_envptr(minibuf);
            sprintf(minibuf, "X10_swAway=%d", ((x10state[hcode].vflags[ucode] & SEC_AWAY) ? 1 : 0) );
            *ep++ = add_envptr(minibuf);
         }
         if ( actfunc == ArmFunc || actfunc == AlertFunc || actfunc == ClearFunc ) {
            sprintf(minibuf, "X10_swMin=%d", ((x10state[hcode].vflags[ucode] & SEC_MIN) ? 1 : 0) );
            *ep++ = add_envptr(minibuf);
            sprintf(minibuf, "X10_swMax=%d", ((x10state[hcode].vflags[ucode] & SEC_MAX) ? 1 : 0) );
            *ep++ = add_envptr(minibuf);
         }

         if ( actfunc == AlertFunc || actfunc == ClearFunc ) {
            sprintf(minibuf, "X10_LoBat=%d", ((x10state[hcode].vflags[ucode] & SEC_LOBAT) ? 1 : 0) );
            *ep++ = add_envptr(minibuf);
         }
         if ( actfunc == RFXLoBatFunc ) {
            *ep++ = add_envptr("X10_LoBat=1");
         }

         if ( actfunc == RFXPowerFunc || actfunc == RFXWaterFunc ||
              actfunc == RFXGasFunc || actfunc == RFXPulseFunc || actfunc == RFXCountFunc ||
              actfunc == OreRainTotFunc ) {
            sprintf(minibuf, "X10_rollover=%d", ((x10state[hcode].vflags[ucode] & RFX_ROLLOVER) ? 1 : 0) );
            *ep++ = add_envptr(minibuf);
         }

#ifdef HASDMX        
         if ( actfunc == DmxSetPtFunc ) {
            longvdata = x10global.longvdata;
            settempc = (int)((longvdata & TSETPMASK) >> TSETPSHIFT);
            sprintf(minibuf, "X10_dmxSetpoint=%d", settempc);
            *ep++ = add_envptr(minibuf);
         }
         if ( actfunc == DmxTempFunc ) {
            longvdata = x10global.longvdata;
            tempc = (int)((longvdata & TCURRMASK) >> TCURRSHIFT);
            tempc = (tempc & 0x80) ? (0x80 - tempc) : tempc;
            sprintf(minibuf, "X10_dmxTemp=%d", tempc);
            *ep++ = add_envptr(minibuf);
         }
         if ( actfunc == DmxOnFunc || actfunc == DmxOffFunc ) {
            longvdata = x10global.longvdata;
            sprintf(minibuf, "X10_dmxHeat=%d", ((longvdata & HEATMASK) ? 0 : 1));
            *ep++ = add_envptr(minibuf);
         }
         if ( actfunc == DmxSetPtFunc || actfunc == DmxTempFunc ||
              actfunc == DmxOnFunc || actfunc == DmxOffFunc ) {
            longvdata = x10global.longvdata;
            status = (unsigned char)((longvdata & STATMASK) >> STATSHIFT);
            sprintf(minibuf, "X10_dmxHeat=%d", ((longvdata & HEATMASK) ? 0 : 1));
            *ep++ = add_envptr(minibuf);
            sprintf(minibuf, "X10_dmxInit=%d", ((status == 3) ? 1 : 0));
            *ep++ = add_envptr(minibuf);
            sprintf(minibuf, "X10_dmxSet=%d", ((status > 0) ? 1 : 0));
            *ep++ = add_envptr(minibuf);
            oostatus = (longvdata & ONOFFMASK) >> ONOFFSHIFT;
            if ( oostatus > 0 ) {
               oostatus = (oostatus == 1) ? 1 : 0;
               if ( (longvdata & HEATMASK) && (DMXCOOLSWAP == YES) )
                  oostatus = (oostatus + 1) % 2;
               sprintf(minibuf, "X10_dmxStatus=%d", oostatus);
               *ep++ = add_envptr(minibuf);
            }
         }
        
#endif /* HASDMX */
      }

#ifdef HASORE
      if ( actfunc == OreTempFunc || actfunc == OreHumidFunc || actfunc == OreBaroFunc ||
           actfunc == OreWeightFunc || actfunc == ElsCurrFunc ||
           actfunc == OreWindSpFunc || actfunc == OreWindAvSpFunc || actfunc == OreWindDirFunc ||
           actfunc == OreRainRateFunc || actfunc == OreRainTotFunc ||
           actfunc == OwlPowerFunc || actfunc == OwlEnergyFunc ) {
         longvdata = x10global.longvdata;
         if ( (channel = (int)((longvdata & ORE_CHANMSK) >> ORE_CHANSHFT)) > 0 ) {
            sprintf(minibuf, "X10_oreCh=%d", channel);
            *ep++ = add_envptr(minibuf);
         }
         if ( longvdata & ORE_BATLVL ) {
            blevel = (longvdata & ORE_BATMSK) >> ORE_BATSHFT;
            sprintf(minibuf, "X10_oreBatLvl=%d", blevel * 10);
            *ep++ = add_envptr(minibuf);
         }
         if ( x10global.interval >= 0 ) {
            sprintf(minibuf, "X10_oreIntv=%ld", x10global.interval);
            *ep++ = add_envptr(minibuf);
         }
         sprintf(minibuf, "X10_oreLoBat=%d", ((longvdata & ORE_LOBAT) ? 1 : 0));
         *ep++ = add_envptr(minibuf);
      }

      if ( actfunc == OreTempFunc ) {
         longvdata = x10global.longvdata;
         otempc = (int)(longvdata & ORE_DATAMSK) >> ORE_DATASHFT;
         if ( (longvdata & ORE_NEGTEMP) )
            otempc = -otempc;
         otemp = celsius2temp((double)otempc / 10.0, configp->ore_tscale, configp->ore_toffset);
         sprintf(minibuf, "X10_oreTemp="FMT_ORET, otemp);
         *ep++ = add_envptr(minibuf);
      }
      else if ( actfunc == OreHumidFunc ) {
         longvdata = x10global.longvdata;
         sprintf(minibuf, "X10_oreRH=%d", (int)((longvdata & ORE_DATAMSK) >> ORE_DATASHFT));
         *ep++ = add_envptr(minibuf);
      }
      else if ( actfunc == OreBaroFunc ) {
         longvdata = x10global.longvdata;
         obaro = (int)((longvdata & ORE_DATAMSK) >> ORE_DATASHFT);
         odbaro = (double)obaro * configp->ore_bpscale + configp->ore_bpoffset;
         sprintf(minibuf, "X10_oreBP="FMT_OREBP, odbaro);
         *ep++ = add_envptr(minibuf);
         if ( (fcast = (longvdata & ORE_FCAST) >> ORE_FCASTSHFT) > 0 ) {
            sprintf(minibuf, "X10_oreForecast=%s", forecast_txt(fcast));
            *ep++ = add_envptr(minibuf);
         }
      }
      else if ( actfunc == OreWeightFunc ) {
         longvdata = x10global.longvdata;
         cweight = (int)((longvdata & ORE_DATAMSK) >> ORE_DATASHFT);
         weight = (double)cweight / 10.0 * configp->ore_wgtscale;
         sprintf(minibuf, "X10_oreWgt="FMT_OREWGT, weight);
         *ep++ = add_envptr(minibuf);
      }
      else if ( actfunc == ElsCurrFunc ) {
         longvdata = x10global.longvdata;
         deciamps = (int)((longvdata & ORE_DATAMSK) >> ORE_DATASHFT);
         sprintf(minibuf, "X10_elsCurr=%.1f", (double)deciamps / 10.0);
         *ep++ = add_envptr(minibuf);
         sprintf(minibuf, "X10_elsSigCount=%u", x10global.sigcount);
         *ep++ = add_envptr(minibuf);
         sprintf(minibuf, "X10_elsPwr=%.0f", configp->els_voltage * (double)deciamps / 10.0);
         *ep++ = add_envptr(minibuf);
      } 
      else if ( actfunc == OreWindSpFunc ) {
         longvdata = x10global.longvdata;
         dwind = (int)((longvdata & ORE_DATAMSK) >> ORE_DATASHFT);
         dblwind = (double)dwind / 10.0 * configp->ore_windscale;
         sprintf(minibuf, "X10_oreWindSp="FMT_OREWSP, dblwind);
         *ep++ = add_envptr(minibuf);
      }
      else if ( actfunc == OreWindAvSpFunc ) {
         longvdata = x10global.longvdata;
         dwind = (int)((longvdata & ORE_DATAMSK) >> ORE_DATASHFT);
         dblwind = (double)dwind / 10.0 * configp->ore_windscale;
         sprintf(minibuf, "X10_oreWindAvSp="FMT_OREWSP, dblwind);
         *ep++ = add_envptr(minibuf);
      }
      else if ( actfunc == OreWindDirFunc ) {
         longvdata = x10global.longvdata;
         dwind = (int)((longvdata & ORE_DATAMSK) >> ORE_DATASHFT);
         dwind = (dwind + configp->ore_windsensordir + 3600) % 3600;
         dblwind = (double)dwind / 10.0;
         sprintf(minibuf, "X10_oreWindDir="FMT_OREWDIR, dblwind);
         *ep++ = add_envptr(minibuf);
      }
      else if ( actfunc == OreRainRateFunc ) {
         longvdata = x10global.longvdata2;
         rain = (longvdata & ORE_DATAMSK) >> ORE_DATASHFT;
         dblrain = (double)rain / 1000.0 * configp->ore_rainratescale;
         sprintf(minibuf, "X10_oreRainRate="FMT_ORERRATE, dblrain);
         *ep++ = add_envptr(minibuf);
      }
      else if ( actfunc == OreRainTotFunc ) {
         rain = x10global.longvdata2;
         dblrain = (double)rain / 1000.0 * configp->ore_raintotscale;
         sprintf(minibuf, "X10_oreRainTot="FMT_ORERTOT, dblrain);
         *ep++ = add_envptr(minibuf);
      }
      else if ( actfunc == OreUVFunc ) {
         longvdata = x10global.longvdata;
         uvfactor = (int)((longvdata & ORE_DATAMSK) >> ORE_DATASHFT);
         sprintf(minibuf, "X10_oreUV=%d", uvfactor);
         *ep++ = add_envptr(minibuf);
      }
      else if ( actfunc == OwlPowerFunc ) {
         dblpower = (double)x10global.longvdata2;
         dblpower = dblpower / 1000.0 * OWLPSC * configp->owl_calib_power * (configp->owl_voltage / OWLVREF);
         sprintf(minibuf, "X10_owlPower=%.3f", dblpower);
         *ep++ = add_envptr(minibuf);
         sprintf(minibuf, "X10_owlPowerCount=%lu", x10global.longvdata2);
         *ep++ = add_envptr(minibuf);
         sprintf(minibuf, "X10_owlSigCount=%u", x10global.sigcount);
         *ep++ = add_envptr(minibuf);
      }
      else if ( actfunc == OwlEnergyFunc ) {
         dblenergy = hilo2dbl(x10global.longvdata3, x10global.longvdata2);
         dblenergy = dblenergy / 10000.0 * OWLESC * configp->owl_calib_energy * (configp->owl_voltage / OWLVREF);
         sprintf(minibuf, "X10_owlEnergy=%.4f", dblenergy);
         *ep++ = add_envptr(minibuf);
#ifdef HASULL
         sprintf(minibuf, "X10_owlEnergyCount=%lld",
            (unsigned long long)x10global.longvdata3 << 32 | (unsigned long long)x10global.longvdata2);
#else
         sprintf(minibuf, "X10_owlEnergyCount=0x%lx%08lx", x10global.longvdata3, x10global.longvdata2);
#endif
         *ep++ = add_envptr(minibuf);         
         sprintf(minibuf, "X10_owlSigCount=%u", x10global.sigcount);
         *ep++ = add_envptr(minibuf);
      }


#endif /* HASORE */
            
      if ( actfunc == DimFunc || actfunc == BrightFunc ) {
         sprintf(minibuf, "X10_RawVal=%d", launcherp->rawdim);
         *ep++ = add_envptr(minibuf);
         sprintf(minibuf, "X10_RawLevel=%d",
	      (launcherp->rawdim - 2) / 11 + 1);
         *ep++ = add_envptr(minibuf);
         if ( actfunc == 4 ) {
            sprintf(minibuf, "X10_DimVal=%d", launcherp->rawdim);
            *ep++ = add_envptr(minibuf);
            sprintf(minibuf, "X10_BrightVal=-%d", launcherp->rawdim);
            *ep++ = add_envptr(minibuf);
         }
         else {
            sprintf(minibuf, "X10_DimVal=-%d", launcherp->rawdim);
            *ep++ = add_envptr(minibuf);
            sprintf(minibuf, "X10_BrightVal=%d", launcherp->rawdim);
            *ep++ = add_envptr(minibuf);	 
         }      
      }	   
      else if ( actfunc == Preset1Func || actfunc == Preset2Func ) {
         sprintf(minibuf, "X10_PresetLevel=%d", launcherp->level);   
         *ep++ = add_envptr(minibuf);
      }	    
      else if ( actfunc == ExtendedFunc ) {
         sprintf(minibuf, "X10_Xfunc=%d", launcherp->xfunc);   
         *ep++ = add_envptr(minibuf);

         sprintf(minibuf, "X10_xFunc=%d", launcherp->xfunc);   
         *ep++ = add_envptr(minibuf);

         sprintf(minibuf, "X10_Xdata=%d", launcherp->level);   
         *ep++ = add_envptr(minibuf);
      }
      else if ( actfunc == VdataFunc ) {
         sprintf(minibuf, "X10_Vdata=%d", launcherp->level);   
         *ep++ = add_envptr(minibuf);

         sprintf(minibuf, "X10_vData=%d", launcherp->level);   
         *ep++ = add_envptr(minibuf);

         sprintf(minibuf, "X10_Lvdata=%ld", x10global.longvdata);
         *ep++ = add_envptr(minibuf);
         sprintf(minibuf, "X10_lvData=%ld", x10global.longvdata);
         *ep++ = add_envptr(minibuf);
      }

      srcname =
         (actsource & RCVI) ? "RCVI" :
         (actsource & RCVA) ? "RCVA" :
         (actsource & SNDC) ? "SNDC" :
         (actsource & SNDM) ? "SNDM" :
         (actsource & SNDS) ? "SNDS" :
         (actsource & SNDA) ? "SNDA" :
         (actsource & SNDT) ? "SNDT" :
         (actsource & RCVT) ? "RCVT" :
         (actsource & SNDP) ? "SNDP" :
         (actsource & XMTF) ? "XMTF" : "";

      if ( *srcname ) {
         sprintf(minibuf, "X10_Source=%s", srcname);
         *ep++ = add_envptr(minibuf);
      }      
   }

   if ( launcherp->type == L_POWERFAIL ) {
      sprintf(minibuf, "X10_Boot=%d", ((bootflag & R_ATSTART) ? 1 : 0) );
      *ep++ = add_envptr(minibuf);
   }
   else if ( launcherp->type == L_TIMEOUT ) {
      sprintf(minibuf, "X10_Timer=%d", launcherp->timer);
      *ep++ = add_envptr(minibuf);
   }
   else if ( launcherp->type == L_SENSORFAIL && config.aliasp != NULL ) {
      aliasindex = launcherp->sensor;
      sprintf(minibuf, "X10_Sensor=%c%s", config.aliasp[aliasindex].housecode,
         bmap2units(config.aliasp[aliasindex].unitbmap));
      *ep++ = add_envptr(minibuf);
   }

   /* The following are valid for all launchers */

   /* Put in the common flag variables */
   for ( k = 0; k < NUM_FLAG_BANKS; k++ ) {
      for ( j = 0; j < 32; j++ ) {
         sprintf(minibuf, "X10_Flag%d=%d", (32 * k) + j+1, ((x10global.flags[k] & (1UL << j)) ? 1 : 0) );
         *ep++ = add_envptr(minibuf);
      }
   }

   /* Put in the counters and counter-zero flags */
   for ( j = 0; j < (32 * NUM_COUNTER_BANKS); j++ ) {
      sprintf(minibuf, "X10_Counter%d=%d", j+1, x10global.counter[j]);
      *ep++ = add_envptr(minibuf);
      sprintf(minibuf, "X10_CzFlag%d=%d", j + 1, ((x10global.czflags[j / 32] & (1UL << (j % 32)) ? 1 : 0)) );
      *ep++ = add_envptr(minibuf);
   }

   /* Put in the countdown timers */
   /* Timer 0 is the arm-pending timer */
   sprintf(minibuf, "X10_ArmPendingTimer=%ld", x10global.timer_count[0]);
   *ep++ = add_envptr(minibuf);

   /* The rest are the user-settable timers and timer-zero flags */
   for ( j = 1; j <= NUM_USER_TIMERS; j++ ) {
      sprintf(minibuf, "X10_Timer%d=%ld", j, x10global.timer_count[j]);
      *ep++ = add_envptr(minibuf);
      sprintf(minibuf, "X10_TzFlag%d=%d", j, ((x10global.tzflags[(j - 1) / 32] & (1UL << ((j - 1) % 32)) ? 1 : 0)) );
      *ep++ = add_envptr(minibuf);
   }
   
   /* Global Security flags */
   sprintf(minibuf, "X10_Tamper=%d", ((x10global.sflags & GLOBSEC_TAMPER) ? 1 : 0) );
   *ep++ = add_envptr(minibuf);
   sprintf(minibuf, "X10_Armed=%d", ((x10global.sflags & GLOBSEC_ARMED) ? 1 : 0) );
   *ep++ = add_envptr(minibuf);
   sprintf(minibuf, "X10_ArmPending=%d", ((x10global.sflags & GLOBSEC_PENDING) ? 1 : 0) );
   *ep++ = add_envptr(minibuf);
   sprintf(minibuf, "X10_Disarmed=%d", ( !(x10global.sflags & GLOBSEC_ARMED) ? 1 : 0) );
   *ep++ = add_envptr(minibuf);
   sprintf(minibuf, "X10_Home=%d", ((x10global.sflags & GLOBSEC_HOME) ? 1 : 0) );
   *ep++ = add_envptr(minibuf);
   sprintf(minibuf, "X10_Away=%d",
       (((x10global.sflags & GLOBSEC_ARMED) && !(x10global.sflags & GLOBSEC_HOME)) ? 1 : 0) );
   *ep++ = add_envptr(minibuf);
   sprintf(minibuf, "X10_SecLights=%d", ((x10global.sflags & GLOBSEC_SLIGHTS) ? 1 : 0) );
   *ep++ = add_envptr(minibuf);

   /* Put in the variables X10_DawnTime, X10_DuskTime, and X10_SysTime */
   if ( dawndusk_today(&dawn, &dusk) == 0 ) {
      sprintf(minibuf, "%s=%ld", "X10_DawnTime", dawn);
      *ep++ = add_envptr(minibuf);
      sprintf(minibuf, "%s=%ld", "X10_DuskTime", dusk);
      *ep++ = add_envptr(minibuf);
   }

   /* Add Clock/Calendar variables */

   time(&now);
   tms = localtime(&now);

   systime = 3600L * (long)tms->tm_hour + 60L * (long)tms->tm_min + (long)tms->tm_sec;
   sprintf(minibuf, "%s=%ld", "X10_SysTime", systime);
   *ep++ = add_envptr(minibuf);
   
   sprintf(minibuf, "%s=%d", "X10_Year", tms->tm_year + 1900);
   *ep++ = add_envptr(minibuf);
   
   sprintf(minibuf, "%s=%d", "X10_Month", tms->tm_mon + 1);
   *ep++ = add_envptr(minibuf);
   
   sprintf(minibuf, "%s=%s", "X10_MonthName", month_name[tms->tm_mon]);
   *ep++ = add_envptr(minibuf);
   
   sprintf(minibuf, "%s=%d", "X10_Day", tms->tm_mday);
   *ep++ = add_envptr(minibuf);
   
   sprintf(minibuf, "%s=%d", "X10_WeekDay", tms->tm_wday);
   *ep++ = add_envptr(minibuf);
   
   sprintf(minibuf, "%s=%s", "X10_WeekDayName", wday_name[tms->tm_wday]);
   *ep++ = add_envptr(minibuf);
   
   sprintf(minibuf, "%s=%d", "X10_Hour", tms->tm_hour);
   *ep++ = add_envptr(minibuf);
   
   sprintf(minibuf, "%s=%d", "X10_Minute", tms->tm_min);
   *ep++ = add_envptr(minibuf);
   
   sprintf(minibuf, "%s=%d", "X10_Second", tms->tm_sec);
   *ep++ = add_envptr(minibuf);
   
   sprintf(minibuf, "%s=%d", "X10_isDST", ((tms->tm_isdst > 0) ? 1 : 0));
   *ep++ = add_envptr(minibuf);

   sprintf(minibuf, "%s=%ld", "X10_GMT", (unsigned long)now);
   *ep++ = add_envptr(minibuf);

   sprintf(minibuf, "%s=", "X10_DateString");
   gendate(minibuf + strlen(minibuf), time(NULL), YES, YES);
   strtrim(minibuf);
   *ep++ = add_envptr(minibuf);

   if ( x10global.dawndusk_enable ) {
      /* Logical variable is true between dusk and dawn */
      sprintf(minibuf, "%s=%d", 
        "X10_isNightTime", ((x10global.sflags & NIGHT_FLAG) ? 1 : 0));
      *ep++ = add_envptr(minibuf);

      /* Logical variable is true in the interval after Dusk */
      /* and before Dawn by the number of minutes defined by */
      /* the config directive ISDARK_OFFSET                  */
      sprintf(minibuf, "%s=%d",
        "X10_isDarkTime", ((x10global.sflags & DARK_FLAG) ? 1 : 0));
      *ep++ = add_envptr(minibuf);
   }

   /* Add upload expiration status and time */
   expire = get_upload_expire();

   sprintf(minibuf, "%s=%d", "X10_Expire", expire);
   *ep++ = add_envptr(minibuf);

      
   /* Add bitmap for each Housecode|Unit */
   for ( hcode = 0; hcode < 16; hcode++ ) {
      hc = code2hc(hcode);
#if 0
      addressed = x10state[hcode].addressed;
      onstate = x10state[hcode].state[OnState];
      dimstate = x10state[hcode].state[DimState];
      chgstate = x10state[hcode].state[ChgState];
      modchgstate = x10state[hcode].state[ModChgState];
      spendstate = x10state[hcode].state[SpendState];
      appliance = ~(modmask[DimMask][hcode] | modmask[BriMask][hcode]);
#endif

      /* Add stored long data in unit 0 variable, e.g., X10_A0=nnnn */
      if ( x10global.longdata_flags & (unsigned long)(1 << hcode) ) {            
         sprintf(minibuf, "X10_%c0=%ld", hc, x10state[hcode].longdata);
         *ep++ = add_envptr(minibuf);
      }

      /* Add RCS temperature if thus configured and valid */
      if ( (configp->rcs_temperature & (1 << hcode)) &&
           (x10global.rcstemp_flags & (unsigned long)(1 << hcode)) ) {
         sprintf(minibuf, "X10_%c0_Temp=%d", hc, x10state[hcode].rcstemp);
         *ep++ = add_envptr(minibuf);
      }

      for ( ucode = 0; ucode < 16; ucode++ ) {
         bitmap = 1 << ucode;
         if ( option & SCRIPT_RAWLEVEL )
            value = get_heyu_state(hcode, ucode, RAWMODE);
         else
            value = get_heyu_state(hcode, ucode, PCTMODE);
         /* Add standard variable, e.g., X10_A9=nnnn */            
         sprintf(minibuf, "X10_%c%d=%lu", hc, code2unit(ucode), value);
         *ep++ = add_envptr(minibuf);


         /* Add 'alias' variable, e.g., x10_porch_light=nnnn */
         j = 0;
         while ( (aliasindex = lookup_alias_mult(hc, bitmap, &j)) >= 0 ) {
            sprintf(minibuf, "%s_%s=%lu",
                configp->env_alias_prefix, aliasp[aliasindex].label, value);
            *ep++ = add_envptr(minibuf);

#if 0
            vtype = aliasp[aliasindex].vtype;
            allflagvalue = 0;
            if ( vtype == RF_SEC || (aliasp[aliasindex].optflags & MOPT_PLCSENSOR) > 0 ) {
               vflags = x10state[hcode].vflags[ucode];
               flagvalue = get_secflag_state(hcode, ucode);
               allflagvalue |= flagvalue;
               sprintf(minibuf, "X10_%c%d_secFlags=%lu",
                  hc, code2unit(ucode), flagvalue);
               *ep++ = add_envptr(minibuf);
               sprintf(minibuf, "%s_%s_secFlags=%lu",
                  configp->env_alias_prefix, aliasp[aliasindex].label, flagvalue);
               *ep++ = add_envptr(minibuf);
            }
            else if ( vtype == RF_XSENSOR || vtype == RF_XMETER ) {
               vflags = x10state[hcode].vflags[ucode];
               flagvalue = get_rfxflag_state(hcode, ucode);
               allflagvalue |= flagvalue;
               sprintf(minibuf, "X10_%c%d_rfxFlags=%lu",
                  hc, code2unit(ucode), flagvalue);
               *ep++ = add_envptr(minibuf);
               sprintf(minibuf, "%s_%s_rfxFlags=%lu",
                  configp->env_alias_prefix, aliasp[aliasindex].label, flagvalue);
               *ep++ = add_envptr(minibuf);
            }
            else if ( vtype == RF_OREGON || vtype == RF_ELECSAVE || vtype == RF_OWL ) {
               vflags = x10state[hcode].vflags[ucode];
               flagvalue = get_oreflag_state(hcode, ucode);
               allflagvalue |= flagvalue;
               sprintf(minibuf, "X10_%c%d_oreFlags=%lu",
                  hc, code2unit(ucode), flagvalue);
               *ep++ = add_envptr(minibuf);
               sprintf(minibuf, "%s_%s_oreFlags=%lu",
                  configp->env_alias_prefix, aliasp[aliasindex].label, flagvalue);
               *ep++ = add_envptr(minibuf);
            }
            else if ( vtype == RF_DIGIMAX ) {
               vflags = x10state[hcode].vflags[ucode];
               flagvalue = get_dmxflag_state(hcode, ucode);
               allflagvalue |= flagvalue;
               sprintf(minibuf, "X10_%c%d_dmxFlags=%lu",
                  hc, code2unit(ucode), flagvalue);
               *ep++ = add_envptr(minibuf);
               sprintf(minibuf, "%s_%s_dmxFlags=%lu",
                  configp->env_alias_prefix, aliasp[aliasindex].label, flagvalue);
               *ep++ = add_envptr(minibuf);
            }
#endif
            vflags = get_vflag_state(hcode, ucode);
            sprintf(minibuf, "X10_%c%d_vFlags=%lu",
               hc, code2unit(ucode), vflags);
            *ep++ = add_envptr(minibuf);
            sprintf(minibuf, "%s_%s_vFlags=%lu",
               configp->env_alias_prefix, aliasp[aliasindex].label, vflags);
            *ep++ = add_envptr(minibuf);
         }
      }

      /* Add long data for unit 0 aliases */
      j = 0;
      while ( (aliaslabel = lookup_label_mult(hc, 0, &j)) != NULL ) {
         if ( x10global.longdata_flags & (unsigned long)(1 << hcode) ) {
            sprintf(minibuf, "%s_%s=%ld",
               configp->env_alias_prefix, aliaslabel, x10state[hcode].longdata);
            *ep++ = add_envptr(minibuf);
         }
         if ( x10global.rcstemp_flags & (1 << hcode) ) {
            sprintf(minibuf, "%s_%s_Temp=%d",
               configp->env_alias_prefix, aliaslabel, x10state[hcode].rcstemp);
            *ep++ = add_envptr(minibuf);
         }
      }
   }

#ifdef HASRFXM

   /* Add RFXMeter data if any */
   aliasp = configp->aliasp;
   j = 0;
   while ( aliasp && aliasp[j].line_no > 0 ) {
      if ( aliasp[j].vtype != RF_XMETER ) {
         j++;
         continue;
      }
      ucode = single_bmap_unit((aliasp[j].unitbmap));
      unit = code2unit(ucode);
      hc = aliasp[j].housecode;
      hcode = hc2code(hc);

      rfxdata = x10state[hcode].rfxmeter[ucode];
      if ( (rfxdata & 1) == 0 ) {
         j++;
         continue;
      }
      rfxdata = rfxdata >> 8;

      aliaslabel = aliasp[j].label;

      if ( (aliasp[j].optflags & MOPT_RFXPOWER) && *(configp->rfx_powerunits) ) {
         sprintf(valbuf, FMT_RFXPOWER, (double)rfxdata * configp->rfx_powerscale);
         sprintf(minibuf, "X10_%c%d_Power=%s", hc, unit, valbuf);
         *ep++ = add_envptr(minibuf);
         sprintf(minibuf, "%s_%s_Power=%s", configp->env_alias_prefix, aliaslabel, valbuf);
         *ep++ = add_envptr(minibuf);
      }
      else if ( (aliasp[j].optflags & MOPT_RFXWATER) && *(configp->rfx_waterunits) ) {
         sprintf(valbuf, FMT_RFXWATER, (double)rfxdata * configp->rfx_waterscale);
         sprintf(minibuf, "X10_%c%d_Water=%s", hc, unit, valbuf);
         *ep++ = add_envptr(minibuf);
         sprintf(minibuf, "%s_%s_Water=%s", configp->env_alias_prefix, aliaslabel, valbuf);
         *ep++ = add_envptr(minibuf);
      }
      else if ( (aliasp[j].optflags & MOPT_RFXGAS) && *(configp->rfx_gasunits) ) {
         sprintf(valbuf, FMT_RFXGAS, (double)rfxdata * configp->rfx_gasscale);
         sprintf(minibuf, "X10_%c%d_Gas=%s", hc, unit, valbuf);
         *ep++ = add_envptr(minibuf);
         sprintf(minibuf, "%s_%s_Gas=%s", configp->env_alias_prefix, aliaslabel, valbuf);
         *ep++ = add_envptr(minibuf);
      }
      else if ( (aliasp[j].optflags & MOPT_RFXPULSE) && *(configp->rfx_pulseunits) ) {
         sprintf(valbuf, FMT_RFXPULSE, (double)rfxdata * configp->rfx_pulsescale);
         sprintf(minibuf, "X10_%c%d_Pulse=%s", hc, unit, valbuf);
         *ep++ = add_envptr(minibuf);
         sprintf(minibuf, "%s_%s_Pulse=%s", configp->env_alias_prefix, aliaslabel, valbuf);
         *ep++ = add_envptr(minibuf);
      }
 
      sprintf(minibuf, "X10_%c%d_Count=%ld", hc, unit, rfxdata);
      *ep++ = add_envptr(minibuf);
      sprintf(minibuf, "%s_%s_Count=%ld", configp->env_alias_prefix, aliaslabel, rfxdata);
      *ep++ = add_envptr(minibuf);

      j++;
   }

   /* Power panel totals */
   for ( j = 0; j < npowerpanels; j++ ) {
      if ( powerpanel_query((unsigned char)j, &rfxdata) == 0 ) {
         sprintf(valbuf, FMT_RFXPOWER, (double)rfxdata * configp->rfx_powerscale);
         sprintf(minibuf, "X10_Panel_%d=%s", j, valbuf);
         *ep++ = add_envptr(minibuf);
      }
   }

#endif /* HASRFXM */

#ifdef HASRFXS
   /* Add RFXSensor data if any */
   aliasp = configp->aliasp;
   j = 0;
   while ( aliasp && aliasp[j].line_no > 0 ) {
      if ( aliasp[j].vtype != RF_XSENSOR ) {
         j++;
         continue;
      }

      unit = code2unit(single_bmap_unit((aliasp[j].unitbmap)));
      hc = aliasp[j].housecode;
      aliaslabel = aliasp[j].label;

      decode_rfxsensor_data(aliasp, j, &inmap, &outmap, &temp, &vsup, &vad, &var2);

      if ( outmap & RFXO_T ) {
         sprintf(valbuf, FMT_RFXT, temp);
         sprintf(minibuf, "X10_%c%d_Temp=%s", hc, unit, valbuf);
         *ep++ = add_envptr(minibuf);
         sprintf(minibuf, "%s_%s_Temp=%s", configp->env_alias_prefix, aliaslabel, valbuf);
         *ep++ = add_envptr(minibuf);
      }
      if ( outmap & RFXO_S ) {
         sprintf(minibuf, "X10_%c%d_Vs=%.2f", hc, unit, vsup);
         *ep++ = add_envptr(minibuf);
         sprintf(minibuf, "%s_%s_Vs=%.2f", configp->env_alias_prefix, aliaslabel, vsup);
         *ep++ = add_envptr(minibuf);
      }
//      if ( outmap & RFXO_V ) {
      if ( outmap & (RFXO_V | RFXO_H | RFXO_B | RFXO_P) ) {
         sprintf(minibuf, "X10_%c%d_Vadi=%.2f", hc, unit, vad);
         *ep++ = add_envptr(minibuf);
         sprintf(minibuf, "%s_%s_Vadi=%.2f", configp->env_alias_prefix, aliaslabel, vad);
         *ep++ = add_envptr(minibuf);
      }

      if ( outmap & RFXO_H ) {
         sprintf(valbuf, FMT_RFXRH, var2);
         sprintf(minibuf, "X10_%c%d_RH=%s", hc, unit, valbuf);
         *ep++ = add_envptr(minibuf);
         sprintf(minibuf, "%s_%s_RH=%s", configp->env_alias_prefix, aliaslabel, valbuf);
         *ep++ = add_envptr(minibuf);
      }
      else if ( outmap & RFXO_B ) {
         sprintf(valbuf, FMT_RFXBP, var2);
         sprintf(minibuf, "X10_%c%d_BP=%s", hc, unit, valbuf);
         *ep++ = add_envptr(minibuf);
         sprintf(minibuf, "%s_%s_BP=%s", configp->env_alias_prefix, aliaslabel, valbuf);
         *ep++ = add_envptr(minibuf);
      }
      else if ( outmap & RFXO_P ) {
         sprintf(minibuf, "X10_%c%d_Pot=%.2f", hc, unit, var2);
         *ep++ = add_envptr(minibuf);
         sprintf(minibuf, "%s_%s_Pot=%.2f", configp->env_alias_prefix, aliaslabel, var2);
         *ep++ = add_envptr(minibuf);
      }
      else if ( outmap & RFXO_T2 ) {
         sprintf(valbuf, FMT_RFXT, temp);
         sprintf(minibuf, "X10_%c%d_Temp2=%s", hc, unit, valbuf);
         *ep++ = add_envptr(minibuf);
         sprintf(minibuf, "%s_%s_Temp2=%s", configp->env_alias_prefix, aliaslabel, valbuf);
         *ep++ = add_envptr(minibuf);
      }
      else if ( outmap & inmap & RFXO_V ) {
         sprintf(valbuf, FMT_RFXVAD, var2);
         sprintf(minibuf, "X10_%c%d_Vad=%s", hc, unit, valbuf);
         *ep++ = add_envptr(minibuf);
         sprintf(minibuf, "%s_%s_Vad=%s", configp->env_alias_prefix, aliaslabel, valbuf);
         *ep++ = add_envptr(minibuf);
      }

      j++;
   }
#endif /* HASRFXS */

#ifdef HASDMX
   /* Add Digimax data if any */
   aliasp = configp->aliasp;
   j = 0;
   while ( aliasp && aliasp[j].line_no > 0 ) {
      int loc;
      if ( aliasp[j].vtype != RF_DIGIMAX ) {
         j++;
         continue;
      }
      unit = code2unit(single_bmap_unit((aliasp[j].unitbmap)));
      hc = aliasp[j].housecode;
      aliaslabel = aliasp[j].label;
      loc = aliasp[j].storage_index;
      if ( (longvdata = x10global.data_storage[loc]) == 0 ) {
         j++;
         continue;
      }

      status = (longvdata & STATMASK) >> STATSHIFT;

      tempc = (longvdata & TCURRMASK) >> TCURRSHIFT;
      tempc = (tempc & 0x80) ? 0x80 - tempc : tempc;
      sprintf(minibuf, "X10_%c%d_dmxTemp=%d", hc, unit, tempc);
      *ep++ = add_envptr(minibuf);
      sprintf(minibuf, "%s_%s_dmxTemp=%d",
          configp->env_alias_prefix, aliaslabel, tempc);
      *ep++ = add_envptr(minibuf);

      if ( longvdata & SETPFLAG ) {
         settempc = (longvdata & TSETPMASK) >> TSETPSHIFT;
         sprintf(minibuf, "X10_%c%d_dmxSetpoint=%d", hc, unit, settempc);
         *ep++ = add_envptr(minibuf);
         sprintf(minibuf, "%s_%s_dmxSetpoint=%d",
             configp->env_alias_prefix, aliaslabel, settempc);
         *ep++ = add_envptr(minibuf);
      }

      sprintf(minibuf, "X10_%c%d_dmxSet=%d", hc, unit, ((longvdata & SETPFLAG) ? 1 : 0));
      *ep++ = add_envptr(minibuf);
      sprintf(minibuf, "%s_%s_dmxSet=%d",
          configp->env_alias_prefix, aliaslabel, ((longvdata & SETPFLAG) ? 1 : 0));
      *ep++ = add_envptr(minibuf);

      oostatus = (longvdata & ONOFFMASK) >> ONOFFSHIFT;
      if ( oostatus > 0 ) {
         oostatus = (oostatus == 1) ? 1 : 0;
         if ( (longvdata & HEATMASK) && (DMXCOOLSWAP == YES) )
            oostatus = (oostatus + 1) % 2;
         sprintf(minibuf, "X10_%c%d_dmxStatus=%d", hc, unit, oostatus);
         *ep++ = add_envptr(minibuf);
         sprintf(minibuf, "%s_%s_dmxStatus=%d",
             configp->env_alias_prefix, aliaslabel, oostatus);
         *ep++ = add_envptr(minibuf);
      }

      sprintf(minibuf, "X10_%c%d_dmxInit=%d", hc, unit, ((status == 3) ? 1 : 0));
      *ep++ = add_envptr(minibuf);
      sprintf(minibuf, "%s_%s_dmxInit=%d",
          configp->env_alias_prefix, aliaslabel, ((status == 3) ? 1 : 0));
      *ep++ = add_envptr(minibuf);

      sprintf(minibuf, "X10_%c%d_dmxHeat=%d", hc, unit, ((longvdata & HEATMASK) ? 0 : 1));
      *ep++ = add_envptr(minibuf);
      sprintf(minibuf, "%s_%s_dmxHeat=%d",
          configp->env_alias_prefix, aliaslabel, ((longvdata & HEATMASK) ? 0 : 1));
      *ep++ = add_envptr(minibuf);

      j++;
   }
#endif /* HASDMX */
      
#ifdef HASORE
   /* Add Oregon data if any */
   aliasp = configp->aliasp;
   aliasindex = 0;
   while ( aliasp && aliasp[aliasindex].line_no > 0 ) {
      int k, loc, func;
      if ( aliasp[aliasindex].vtype != RF_OREGON    &&
           aliasp[aliasindex].vtype != RF_ELECSAVE  &&
           aliasp[aliasindex].vtype != RF_OWL           ) {
         aliasindex++;
         continue;
      }

      unit = code2unit(single_bmap_unit((aliasp[aliasindex].unitbmap)));
      hc = aliasp[aliasindex].housecode;
      aliaslabel = aliasp[aliasindex].label;
      for ( k = 0; k < aliasp[aliasindex].nvar; k++ ) {
         func = aliasp[aliasindex].funclist[k];
         loc = aliasp[aliasindex].storage_index + aliasp[aliasindex].statusoffset[k];
         if ( (longvdata = x10global.data_storage[loc]) == 0 )
            break;

         if ( (k == 0) && (channel = (int)((longvdata & ORE_CHANMSK) >> ORE_CHANSHFT)) > 0 ) {
            sprintf(minibuf, "X10_%c%d_oreCh=%d", hc, unit, channel);
            *ep++ = add_envptr(minibuf);
            sprintf(minibuf, "%s_%s_oreCh=%d", 
                 configp->env_alias_prefix, aliaslabel, channel);
            *ep++ = add_envptr(minibuf);
         }
         if ( (k == 0) && (longvdata & ORE_BATLVL) ) {
            blevel = (int)((longvdata & ORE_BATMSK) >> ORE_BATSHFT);
            sprintf(minibuf, "X10_%c%d_oreBatLvl=%d", hc, unit, blevel * 10);
            *ep++ = add_envptr(minibuf);
            sprintf(minibuf, "%s_%s_oreBatLvl=%d", 
                 configp->env_alias_prefix, aliaslabel, blevel * 10);
            *ep++ = add_envptr(minibuf);
         }
         if ( k == 0 ) {
            sprintf(minibuf, "X10_%c%d_oreLoBat=%d", hc, unit, ((longvdata & ORE_LOBAT) ? 1 : 0));
            *ep++ = add_envptr(minibuf);
            sprintf(minibuf, "%s_%s_oreLoBat=%d", 
                 configp->env_alias_prefix, aliaslabel, ((longvdata & ORE_LOBAT) ? 1 : 0));
            *ep++ = add_envptr(minibuf);
         }

         switch ( func ) {
            case OreTempFunc :
               otempc = (longvdata & ORE_DATAMSK) >> ORE_DATASHFT;
               if ( longvdata & ORE_NEGTEMP )
                  otempc = -otempc;
               otemp = celsius2temp((double)otempc / 10.0, configp->ore_tscale, configp->ore_toffset);
               sprintf(minibuf, "X10_%c%d_oreTemp="FMT_ORET, hc, unit, otemp);
               *ep++ = add_envptr(minibuf);
               sprintf(minibuf, "%s_%s_oreTemp="FMT_ORET,
                   configp->env_alias_prefix, aliaslabel, otemp);
               *ep++ = add_envptr(minibuf);
               break;

            case OreHumidFunc :
               sprintf(minibuf, "X10_%c%d_oreRH=%d", hc, unit, (int)((longvdata & ORE_DATAMSK) >> ORE_DATASHFT));
               *ep++ = add_envptr(minibuf);
               sprintf(minibuf, "%s_%s_oreRH=%d", configp->env_alias_prefix, aliaslabel,
                     (int)((longvdata & ORE_DATAMSK) >> ORE_DATASHFT));
               *ep++ = add_envptr(minibuf);
               break;

            case OreBaroFunc :
               obaro = (int)(longvdata & ORE_DATAMSK) >> ORE_DATASHFT;
               odbaro = (double)obaro * configp->ore_bpscale + configp->ore_bpoffset;
               sprintf(minibuf, "X10_%c%d_oreBP"FMT_OREBP, hc, unit, odbaro);
               *ep++ = add_envptr(minibuf);
               sprintf(minibuf, "%s_%s_oreBP="FMT_OREBP, configp->env_alias_prefix, aliaslabel, odbaro);
               *ep++ = add_envptr(minibuf);
               if ( (fcast = (longvdata & ORE_FCAST) >> ORE_FCASTSHFT) > 0 ) {
                  sprintf(minibuf, "X10_%c%d_oreForecast=%s", hc, unit, forecast_txt(fcast));
                  *ep++ = add_envptr(minibuf);
                  sprintf(minibuf, "%s_%s_oreForecast=%s",configp->env_alias_prefix, aliaslabel, forecast_txt(fcast));
                  *ep++ = add_envptr(minibuf); 
               }
               break;

            case OreWeightFunc :
               cweight = (int)(longvdata & ORE_DATAMSK) >> ORE_DATASHFT;
               weight = (double)cweight / 10.0 * configp->ore_wgtscale;
               sprintf(minibuf, "X10_%c%d_oreWgt="FMT_OREWGT, hc, unit, weight);
               *ep++ = add_envptr(minibuf);
               sprintf(minibuf, "%s_%s_oreWgt="FMT_OREWGT, configp->env_alias_prefix, aliaslabel, weight);
               *ep++ = add_envptr(minibuf);
               break;

            case ElsCurrFunc :
               if ( (channel = (int)((longvdata & ORE_CHANMSK) >> ORE_CHANSHFT)) > 0 ) {
                  sprintf(minibuf, "X10_%c%d_elsCh=%d", hc, unit, channel);
                  *ep++ = add_envptr(minibuf);
                  sprintf(minibuf, "%s_%s_elsCh=%d", 
                       configp->env_alias_prefix, aliaslabel, channel);
                  *ep++ = add_envptr(minibuf);
               }
               deciamps = (int)(longvdata & ORE_DATAMSK) >> ORE_DATASHFT;
               sprintf(minibuf, "X10_%c%d_elsCurr=%.1f", hc, unit, (double)deciamps / 10.0);
               *ep++ = add_envptr(minibuf);
               sprintf(minibuf, "%s_%s_elsCurr=%.1f", configp->env_alias_prefix, aliaslabel,
                 (double)deciamps / 10.0);
               *ep++ = add_envptr(minibuf);
               sprintf(minibuf, "X10_%c%d_elsPwr=%.0f", hc, unit,
                  configp->els_voltage * (double)deciamps / 10.0);
               *ep++ = add_envptr(minibuf);
               sprintf(minibuf, "%s_%s_elsPwr=%.0f", configp->env_alias_prefix, aliaslabel,
                  configp->els_voltage * (double)deciamps / 10.0);
               *ep++ = add_envptr(minibuf);


               sprintf(minibuf, "X10_%c%d_elsLoBat=%d", hc, unit, ((longvdata & ORE_LOBAT) ? 1 : 0));
               *ep++ = add_envptr(minibuf);
               sprintf(minibuf, "%s_%s_elsLoBat=%d", 
                    configp->env_alias_prefix, aliaslabel, ((longvdata & ORE_LOBAT) ? 1 : 0));
               *ep++ = add_envptr(minibuf);
               break;

            case OreWindSpFunc :
               dwind = (int)(longvdata & ORE_DATAMSK) >> ORE_DATASHFT;
               dblwind = (double)dwind / 10.0 * configp->ore_windscale;
               sprintf(minibuf, "X10_%c%d_oreWindSp="FMT_OREWSP, hc, unit, dblwind);
               *ep++ = add_envptr(minibuf);
               sprintf(minibuf, "%s_%s_oreWindSp="FMT_OREWSP, configp->env_alias_prefix, aliaslabel, dblwind);
               *ep++ = add_envptr(minibuf);

               break;

            case OreWindAvSpFunc :
               dwind = (int)(longvdata & ORE_DATAMSK) >> ORE_DATASHFT;
               dblwind = (double)dwind / 10.0 * configp->ore_windscale;
               sprintf(minibuf, "X10_%c%d_oreWindAvSp="FMT_OREWSP, hc, unit, dblwind);
               *ep++ = add_envptr(minibuf);
               sprintf(minibuf, "%s_%s_oreWindAvSp="FMT_OREWSP, configp->env_alias_prefix, aliaslabel, dblwind);
               *ep++ = add_envptr(minibuf);
               break;

            case OreWindDirFunc :
               dwind = (int)(longvdata & ORE_DATAMSK) >> ORE_DATASHFT;
               dwind = (dwind + configp->ore_windsensordir + 3600) % 3600;
               dblwind = (double)dwind / 10.0;
               sprintf(minibuf, "X10_%c%d_oreWindDir=%.1f", hc, unit, dblwind);
               *ep++ = add_envptr(minibuf);
               sprintf(minibuf, "%s_%s_oreWindDir=%.1f", configp->env_alias_prefix, aliaslabel, dblwind);
               *ep++ = add_envptr(minibuf);
               break;

            case OreRainRateFunc :
               /* 32 bit rainfall rate is stored in an adjoining location */
               rain = x10global.data_storage[loc + 1];
               dblrain = (double)rain / 1000.0 * configp->ore_rainratescale;
               sprintf(minibuf, "X10_%c%d_oreRainRate="FMT_ORERRATE, hc, unit, dblrain);
               *ep++ = add_envptr(minibuf);
               sprintf(minibuf, "%s_%s_oreRainRate="FMT_ORERRATE, configp->env_alias_prefix, aliaslabel, dblrain);
               *ep++ = add_envptr(minibuf);
               break;

            case OreRainTotFunc :
               /* 32 bit total rainfall is stored in an adjoining location */
               rain = x10global.data_storage[loc + 1];
               dblrain = (double)rain / 1000.0 * configp->ore_raintotscale;
               sprintf(minibuf, "X10_%c%d_oreRainTot="FMT_ORERTOT, hc, unit, dblrain);
               *ep++ = add_envptr(minibuf);
               sprintf(minibuf, "%s_%s_oreRainTot="FMT_ORERTOT, configp->env_alias_prefix, aliaslabel, dblrain);
               *ep++ = add_envptr(minibuf);
               break;

            case OreUVFunc :
               uvfactor = (int)(longvdata & ORE_DATAMSK) >> ORE_DATASHFT;
               sprintf(minibuf, "X10_%c%d_oreUV=%d", hc, unit, uvfactor);
               *ep++ = add_envptr(minibuf);
               sprintf(minibuf, "%s_%s_oreUV=%d", configp->env_alias_prefix, aliaslabel, uvfactor);
               *ep++ = add_envptr(minibuf);
               break;

            case OwlPowerFunc :
               dblpower = (double)x10global.data_storage[loc + 1];
               dblpower = dblpower / 1000.0 * OWLPSC * configp->owl_calib_power * (configp->owl_voltage / OWLVREF);
               sprintf(minibuf, "X10_%c%d_owlPower=%.3f", hc, unit, dblpower);
               *ep++ = add_envptr(minibuf);
               sprintf(minibuf, "%s_%s_owlPower=%.3f", configp->env_alias_prefix, aliaslabel, dblpower);
               *ep++ = add_envptr(minibuf);
               sprintf(minibuf, "X10_%c%d_owlPowerCount=%ld", hc, unit, x10global.data_storage[loc + 1]);
               *ep++ = add_envptr(minibuf);
               break;

            case OwlEnergyFunc :
               dblenergy = hilo2dbl(x10global.data_storage[loc + 2], x10global.data_storage[loc + 1]);
               dblenergy = dblenergy / 10000.0 * OWLESC * configp->owl_calib_energy * (configp->owl_voltage / OWLVREF);
               sprintf(minibuf, "X10_%c%d_owlEnergy=%.4f", hc, unit, dblenergy);
               *ep++ = add_envptr(minibuf);
               sprintf(minibuf, "%s_%s_owlEnergy=%.4f", configp->env_alias_prefix, aliaslabel, dblenergy);
               *ep++ = add_envptr(minibuf);
#ifdef HASULL
               sprintf(minibuf, "X10_%c%d_owlEnergyCount=%llu", hc, unit,
                  (unsigned long long)x10global.data_storage[loc + 2] << 32 | (unsigned long long)x10global.data_storage[loc + 1]);
#else
               sprintf(minibuf, "X10_%c%d_owlEnergyCount=0x%lx%08lx", hc, unit,
                  x10global.data_storage[loc + 2], x10global.data_storage[loc + 2]);
#endif
               *ep++ = add_envptr(minibuf);

               break;

            default :
               break;
         }
      }
      aliasindex++;
   }
#endif /* HASORE */

   /* Append the user's original environment */
   for ( j = 0; j < size; j++ ) {
      *ep++ = add_envptr(environ[j]);
   }

   /* Add NULL terminator */
   *ep++ = NULL;

   /* Make sure we haven't exceeded allocated size */
   if ( (int)(ep - envp) > newsize ) {
      fprintf(stderr, "Internal error in create_heyu_environment()\n");
      fprintf(stderr, "Allocated = %d, actual = %d\n",
           newsize, (int)(ep - envp));
      exit(1);
   }

   return envp;
}


/*---------------------------------------------------------------------+
 | Create an environment compatible with Xtend scripts.                |
 +---------------------------------------------------------------------*/
char **create_xtend_environment( LAUNCHER *launcherp )
{
   
   extern char   **environ;

   int           j, size, newsize;
   unsigned char hcode, ucode;
   char          **envp, **ep;
   char          hc;
   unsigned int  value, bitmap, addressed, onstate, appliance;
   char          minibuf[512];
   static int    sizchrptr = sizeof(char *);

#if 0
   static struct {
      char *query;
      int  mask;
   } xtendmaskval[] = {
      {"isAppl",    XTMAP_APPL },
      {"isAddr",    XTMAP_ADDR },
      {"isOn",      XTMAP_ON   },
      { NULL,       0          },
   };
#endif  /* Reference */

   
   if ( launcherp->type == L_NORMAL )   
      putenv("HEYU_PARENT=ENGINE");
   else
      putenv("HEYU_PARENT=RELAY");

   /* Get length of original environment */
   size = 0;
   while ( environ[size] != NULL )
      size++;

   newsize = size + 256 + 50;

   if ( (envp = calloc(newsize, sizchrptr)) == NULL ) {
      fprintf(stderr, "create_xtend_environment() : out_of_memory\n");
      exit(1);
   }

   ep = envp;

   *ep++ = add_envptr("IAMXTEND=1");

   /* Put config file pathname in environment for child processes */
   sprintf(minibuf, "X10CONFIG=%s", pathspec(heyu_config));
   *ep++ = add_envptr(minibuf);

   /* Create the various masks */
   j = 0;
   while ( xtendmaskval[j].query ) {
      sprintf(minibuf, "%s=%lu", xtendmaskval[j].query, xtendmaskval[j].mask);
      *ep++ = add_envptr(minibuf);
      j++;
   }

   for ( hcode = 0; hcode < 16; hcode++ ) {
      hc = code2hc(hcode);
      addressed = x10state[hcode].addressed;
      onstate = x10state[hcode].state[OnState];
      appliance = ~(modmask[DimMask][hcode] | modmask[BriMask][hcode]);
      for ( ucode = 0; ucode < 16; ucode++ ) {
         bitmap = 1 << ucode;
         value = 0;

         /* Add the state bits */
         if ( appliance & bitmap )
            value |= XTMAP_APPL;         
         if ( addressed & bitmap )
            value |= XTMAP_ADDR;
         if ( onstate & bitmap )
            value |= XTMAP_ON;
            
         sprintf(minibuf, "X10_%c%d=%d", hc, code2unit(ucode), value);
         *ep++ = add_envptr(minibuf);
      }
   }

   /* Append the user's original environment */
   for ( j = 0; j < size; j++ ) {
      *ep++ = add_envptr(environ[j]);
   }

   /* Add NULL terminator */
   *ep++ = NULL;

   return envp;
}

void free_environment ( char **envp )
{
   int j = 0;

   if ( !envp )
      return;
   
   while ( envp[j] != NULL ) {
      free(envp[j]);
      j++;
   }

   free(envp);
   return;
}

/*---------------------------------------------------------------------+
 | For debugging.                                                      |
 +---------------------------------------------------------------------*/
void display_environment ( char **envp ) 
{
   int j = 0;
   
   while ( envp[j] != NULL ) {
      printf("%s\n", envp[j]);
      j++;
   }
}

/*---------------------------------------------------------------------+
 | Execute internal engine precomands at beginning of cmdline.         |
 | On return, cmdptr points to remainder of command line.              |
 | Return codes:                                                       |
 |  0  if normal return.                                               |
 | -1  if remainder of command line is to be skipped, e.g., for        |
 |     @decskpz or @decskpnz commands.                                 |
 |  1  if error.                                                       |
 +---------------------------------------------------------------------*/
int exec_internal_precommands ( char *cmdline, char **cmdptr )
{
   int  tokc, retcode = 0;
   char **tokv;
   char *bufp;
   int  direct_command ( int, char **, int );
   char command[80];

   *cmdptr = bufp = cmdline;
   while ( 1 ) {
      get_token(command, &bufp, ";", sizeof(command) - 1);
      strtrim(command);
      if ( *command != '@' )
         break;
      tokenize(command, " \t\r\n", &tokc, &tokv);
      retcode = direct_command(tokc, tokv, CMD_INT);
      if ( *bufp == ';' )
         bufp++;
      *cmdptr = bufp;
      free(tokv);
      if ( retcode != 0 )
         break;
   }
   return retcode;
}

#ifdef EXEC_POSIX
/*---------------------------------------------------------------------+
 | Execute a general script by the state engine - POSIX                |
 +---------------------------------------------------------------------*/
int exec_script ( LAUNCHER *launcherp ) 
{

   int      retcode, scriptnum, /*lindex,*/ status, precode;
   PID_T    pid;
   unsigned char option;
   char     *cmdline, *label, *cmdptr;
   char     **envp;
   char     *argv[4];
   char     *shell;

   if ( !i_am_state || configp->script_ctrl == DISABLE )
      return 0;

   scriptnum = launcherp->scriptnum;

   cmdline = configp->scriptp[scriptnum].cmdline;
   option = configp->scriptp[scriptnum].script_option;
   label = configp->scriptp[scriptnum].label;
   shell = configp->script_shell;

   if ( !(option & SCRIPT_QQUIET) ) {
      if ( option & SCRIPT_QUIET )
         fprintf(stdout, "%s Launching '%s': <quiet>\n", datstrf(), label);
      else
         fprintf(stdout, "%s Launching '%s': %s\n", datstrf(), label, cmdline);
      fflush(stdout);
   }

   precode = exec_internal_precommands(cmdline, &cmdptr);

   write_x10state_file();

   if ( precode > 0 ) {
      fprintf(stderr, "%s\n", error_message());
      fflush(stderr);
      return 1;
   }
   else if ( precode < 0 || *cmdptr == '\0' ) {
      return 0;
   }

   argv[0] = shell;
   argv[1] = "-c";
   argv[2] = cmdptr;
   argv[3] = NULL;

   retcode = fork();

   if ( retcode == -1 ) {
      fprintf(stderr, "Unable to fork() for launching script.\n");
      fflush(stderr);
      return 1;
   }

   if ( retcode == 0 ) {
      /* In child process */
      if ( (pid = fork()) > (PID_T)0 ) {
	 /* Child dies; grandchild inherited by init */
         _exit(0);
      }
      else if ( pid < (PID_T)0 ) {
         fprintf(stderr, "Failed to double fork() for launching script.\n");
	 fflush(stderr);
	 _exit(1);
      }
      else {
         if ( option & SCRIPT_NOENV ) 
            envp = create_noenv_environment(launcherp, D_ENGINE);
         else if ( option & SCRIPT_XTEND )
            envp = create_xtend_environment(launcherp);
         else
            envp = create_heyu_environment(launcherp, option);
         execve(shell, argv, envp);
         perror("Execution of script has failed");
         exit(1);
      }
   }

   /* Wait for child process */
   while ( waitpid(retcode, &status, 0) < (PID_T)0 && errno == EINTR )
      ;
   if (WEXITSTATUS(status) != 0 ) {
     fprintf(stderr, "Unable to double fork() for launching script.\n");
     return 1;
   }
   
   return 0;
}    

/*---------------------------------------------------------------------+
 | Launch a power-fail script by the heyu relay - POSIX                |
 +---------------------------------------------------------------------*/
int relay_powerfail_script ( void ) 
{
   int      status;
   PID_T    pid, retpid;
   char     **envp;
   char     *argv[4];

   if ( configp->script_ctrl == DISABLE )
      return 0;

   if ( !configp->pfail_script )
      return 0;

   argv[0] = configp->script_shell;
   argv[1] = "-c";
   argv[2] = configp->pfail_script;
   argv[3] = NULL;

   retpid = fork();

   if ( retpid == (PID_T)(-1) ) {
      syslog(LOG_ERR,"Unable to fork() in relay_powerfail_script().\n");
      return 1;
   }

   if ( retpid == (PID_T)0 ) {
      /* In child process */
      if ( (pid = fork()) > (PID_T)0 ) {
	 /* Child dies; grandchild inherited by init */
         _exit(0);
      }
      else if ( pid < (PID_T)0 ) {
         syslog(LOG_ERR,"Failed to double fork() in relay_powerfail_script().");
	 _exit(1);
      }
      else {
         envp = create_noenv_environment(NULL, D_RELAY);
         execve(configp->script_shell, argv, envp);
         perror("Execution of relay_powerfail_script() has failed");
         exit(1);
      }
   }

   /* Wait for child process */
   while ( waitpid(retpid, &status, 0) < (PID_T)0 && errno == EINTR )
      ;
   if (WEXITSTATUS(status) != 0 ) {
      syslog(LOG_ERR,"waitpid failed in relay_powerfail_script().");
      return 1;
   }
   
   return 0;
}    

/*---------------------------------------------------------------------+
 | Start the state engine from main() - POSIX                          |
 +---------------------------------------------------------------------*/
int start_engine_main ( void ) 
{
   int         status;
   PID_T       pid, retpid;
   char        **envp;
   char        *argv[4];
   extern char heyuprogname[PATH_LEN + 1 + 10];

   strncat(heyuprogname, " engine", sizeof(heyuprogname) - 1 - strlen(heyuprogname));

   argv[0] = configp->script_shell;
   argv[1] = "-c";
   argv[2] = heyuprogname;
   argv[3] = NULL;

   retpid = fork();

   if ( retpid == (PID_T)(-1) ) {
      syslog(LOG_ERR,"Unable to fork() in start_engine_main().\n");
      return 1;
   }

   if ( retpid == (PID_T)0 ) {
      /* In child process */
      if ( (pid = fork()) > (PID_T)0 ) {
	 /* Child dies; grandchild inherited by init */
         _exit(0);
      }
      else if ( pid < (PID_T)0 ) {
         syslog(LOG_ERR,"Failed to double fork() in start_engine_main().");
	 _exit(1);
      }
      else {
         envp = create_noenv_environment(NULL, D_RELAY);
         execve(configp->script_shell, argv, envp);
         perror("Execution of start_engine_main() has failed");
         exit(1);
      }
   }

   /* Wait for child process */
   while ( waitpid(retpid, &status, 0) < (PID_T)0 && errno == EINTR )
      ;
   if (WEXITSTATUS(status) != 0 ) {
      syslog(LOG_ERR,"waitpid failed in start_engine_main().");
      return 1;
   }
   
   return 0;
}    

/*---------------------------------------------------------------------+
 | Launch heyuhelper - POSIX                                           |
 +---------------------------------------------------------------------*/
int launch_heyuhelper ( unsigned char hcode, unsigned int bitmap,
                                                unsigned char actfunc ) 
{
   int           status;
   PID_T         pid, retpid;
   char          *argv[4];
   char          *shell;
   char          **envp;
   char          hc;
   char          cmdline[128];

   if ( configp->script_ctrl == DISABLE )
      return 0;

   write_x10state_file();
   
   hc = tolower((int)code2hc(hcode));

   sprintf(cmdline, "heyuhelper %c%d%s",
	hc, x10state[hcode].lastunit, funclabel[actfunc]);

   shell = configp->script_shell;
   
   argv[0] = shell;
   argv[1] = "-c";
   argv[2] = cmdline;
   argv[3] = 0;

   retpid = fork();

   if ( retpid == (PID_T)(-1) ) {
      fprintf(stderr, "Unable to fork() for launching heyuhelper.\n");
      return 1;
   }

   if ( retpid == (PID_T)0 ) {
      /* In child process */
      if ( (pid = fork()) > (PID_T)0 ) {
	 /* Child dies; grandchild inherited by init */
         _exit(0); 
      }
      else if ( pid < (PID_T)0 ) {
	 fprintf(stderr, "Unable to fork() 2nd gen for launching heyuhelper.\n");
	 fflush(stderr);
	 _exit(1);
      }
      else {
	 envp = create_noenv_environment(NULL, D_ENGINE);
         execve(shell, argv, envp);
         /* Silently exit if failure */
         exit(1);
      }
   }

   /* wait for child process */
   while ( waitpid(retpid, &status, 0) < (PID_T)0 && errno == EINTR )
      ;
   if (WEXITSTATUS(status) != 0 ) {
     fprintf(stderr, "Unable to double fork() for launching heyuhelper.\n");
     return 1;
   }

   return 0;
} 
     
      
#else
/*---------------------------------------------------------------------+
 | Execute a general script by the state engine - non-POSIX            |
 +---------------------------------------------------------------------*/
int exec_script ( LAUNCHER *launcherp ) 
{

   int      scriptnum, /*lindex,*/ precode;
   PID_T    retpid;
   unsigned char option;
   char     *cmdline, *label, *cmdptr;
   char     **envp;
   char     *argv[4];
   char     *shell;

   if ( !i_am_state || configp->script_ctrl == DISABLE )
      return 0;

   signal(SIGCHLD, SIG_IGN);
   
   scriptnum = launcherp->scriptnum;
   cmdline = configp->scriptp[scriptnum].cmdline;
   option = configp->scriptp[scriptnum].script_option;
   label = configp->scriptp[scriptnum].label;
   shell = configp->script_shell;

   if ( !(option & SCRIPT_QQUIET) ) {
      if ( option & SCRIPT_QUIET )
         fprintf(stdout, "%s Launching '%s': <quiet>\n", datstrf(), label);
      else
         fprintf(stdout, "%s Launching '%s': %s\n", datstrf(), label, cmdline);
      fflush(stdout);
   }

   precode = exec_internal_precommands(cmdline, &cmdptr);
   
   write_x10state_file();

   if ( precode > 0 ) {
      fprintf(stderr, "%s\n", error_message());
      fflush(stderr);
      return 1;
   }
   else if ( precode < 0 || *cmdptr == '\0' ) {
      return 0;
   }

   argv[0] = shell;
   argv[1] = "-c";
   argv[2] = cmdptr;
   argv[3] = 0;

   retpid = fork();

   if ( retpid == (PID_T)(-1) ) {
      fprintf(stderr, "Unable to fork() for launching script.\n");
      return 1;
   }

   if ( retpid == (PID_T)0 ) {
      /* In child process */
      if ( option & SCRIPT_NOENV )
         envp = create_noenv_environment(launcherp, D_ENGINE); 
      else if ( option & SCRIPT_XTEND )
         envp = create_xtend_environment(launcherp);
      else
         envp = create_heyu_environment(launcherp, option);
      execve(shell, argv, envp);
      perror("Execution of script has failed");
      exit(1);
   }

   return 0;
}    


/*---------------------------------------------------------------------+
 | Launch a power-fail script by the heyu relay - non-POSIX            |
 +---------------------------------------------------------------------*/
int relay_powerfail_script ( void ) 
{
   PID_T    retpid;
   char     **envp;
   char     *argv[4];

   if ( configp->script_ctrl == DISABLE )
      return 0;

   if ( !configp->pfail_script )
      return 0;

   signal(SIGCHLD, SIG_IGN);

   argv[0] = configp->script_shell;
   argv[1] = "-c";
   argv[2] = configp->pfail_script;
   argv[3] = 0;

   retpid = fork();

   if ( retpid == (PID_T)(-1) ) {
      fprintf(stderr, "Unable to fork() in relay_powerfail_script().\n");
      return 1;
   }

   if ( retpid == (PID_T)0 ) {
      /* In child process */
      envp = create_noenv_environment(NULL, D_RELAY); 
      execve(configp->script_shell, argv, envp);
      perror("Execution of relay_powerfail_script() has failed");
      exit(1);
   }

   return 0;
}    
     
/*---------------------------------------------------------------------+
 | Start the state engine from main() - non-POSIX                      |
 +---------------------------------------------------------------------*/
int start_engine_main ( void ) 
{
   int         j;
   PID_T       retpid;
   char        **envp;
   char        *argv[4];
   extern char heyuprogname[PATH_LEN + 1 + 10];

   strncat(heyuprogname, " engine", sizeof(heyuprogname) - 1 - strlen(heyuprogname));

   signal(SIGCHLD, SIG_IGN);

   argv[0] = configp->script_shell;
   argv[1] = "-c";
   argv[2] = heyuprogname;
   argv[3] = 0;

   retpid = fork();

   if ( retpid == (PID_T)(-1) ) {
      fprintf(stderr, "Unable to fork() in start_engine_main().\n");
      return 1;
   }

   if ( retpid == (PID_T)0 ) {
      /* In child process */
      envp = create_noenv_environment(NULL, D_RELAY); 
      execve(configp->script_shell, argv, envp);
      perror("Execution of start_engine_main() has failed");
      exit(1);
   }

   /* Wait until engine is ready to go */
   for ( j = 0; j < 20; j++ ) { 
      if ( check_for_engine() == 0 )
         break;
      millisleep(100);
   }
   
   return 0;
}    
     
/*---------------------------------------------------------------------+
 | Launch heyuhelper - non-POSIX.                                      |
 +---------------------------------------------------------------------*/
int launch_heyuhelper ( unsigned char hcode, unsigned int bitmap,
                                                unsigned char actfunc ) 
{

   PID_T         retpid;
   char          *argv[4];
   char          *shell;
   char          **envp;
   char          hc;
   char          cmdline[128];

   if ( configp->script_ctrl == DISABLE )
      return 0;

   signal(SIGCHLD, SIG_IGN);

   write_x10state_file();
   
   hc = tolower(code2hc(hcode));

   sprintf(cmdline, "heyuhelper %c%d%s",
	hc, x10state[hcode].lastunit, funclabel[actfunc]);

   shell = configp->script_shell;
   
   argv[0] = shell;
   argv[1] = "-c";
   argv[2] = cmdline;
   argv[3] = NULL;

   retpid = fork();

   if ( retpid == (PID_T)(-1) ) {
      fprintf(stderr, "Unable to fork() for launching heyuhelper.\n");
      return 1;
   }

   if ( retpid == (PID_T)0 ) {
      /* In child process */
      envp = create_noenv_environment(NULL, D_ENGINE);
      execve(shell, argv, envp);
      /* Silently exit if failure */
      exit(1);
   }

   return 0;
}      
 
#endif  /* End of ifdef EXEC_POSIX / else block */

      
/*---------------------------------------------------------------------+
 | Launch the script identified in LAUNCHER at index launchp           |
 +---------------------------------------------------------------------*/
int launch_script_old ( int *launchp )
{
   LAUNCHER *launcherp;

   launcherp = &configp->launcherp[*launchp];
   *launchp = -1;

   return exec_script(launcherp);
}

/*---------------------------------------------------------------------+
 | Launch the script identified in LAUNCHER at index launchp           |
 +---------------------------------------------------------------------*/
int launch_scripts ( int *launchp )
{
   LAUNCHER *launcherp;
   int      j, retcode = 0;

   if ( !i_am_state || !(launcherp = configp->launcherp) )
      return 0;

   j = 0;
   while ( launcherp[j].line_no > 0 ) {
      if ( launcherp[j].matched == YES ) {
         launcherp[j].matched = NO;
         *launchp = -1;
         retcode |= exec_script(&launcherp[j]);
      }
      j++;
   }

   return retcode;
}

/*---------------------------------------------------------------------+
 | Display state information in a variety of formats (primarily for    |
 | use in scripts) in response to command line requests.               |
 +---------------------------------------------------------------------*/
int c_x10state ( int argc, char *argv[] )
{
   extern void show_module_mask ( unsigned char );

   char          hc;
   long          delta;
   unsigned char hcode, ucode, unit, check;
   unsigned char level;
   unsigned int  state, bitmap, aflags;
   unsigned long vident, value;
   int           j;
   char          *query;

   argv++;
   argc--;

   check = check_for_engine();

   if ( strcmp(argv[0], "enginestate") == 0 ) {
      printf("%d\n", (check ? 0 : 1));
      return 0;
   }

   if ( check != 0 ) {
      fprintf(stderr, "State engine is not running.\n");
      return 1;
   }

   if ( argc < 1 ) {
      fprintf(stderr, "Too few arguments\n");
      return 1;
   }

   get_configuration(CONFIG_INIT);


   if ( strcmp(argv[0], "initstateold") == 0 ) {
      /* Initialize the state structure to 0 */
      if ( argc == 1 ) {
         send_x10state_command( ST_INIT_ALL, 0 );
         return 0;
      }
      else {
         aflags = parse_addr(argv[1], &hc, &bitmap);
         if ( !(aflags & A_VALID) || !(aflags & A_HCODE) || (aflags & A_DUMMY) ) {
            fprintf(stderr, "Invalid argument %s\n", argv[1]);
            return 1;
         }
         if ( bitmap ) 
            fprintf(stderr, "Unit code ignored\n");

         send_x10state_command( ST_INIT, hc2code(hc) );
      }
      return 0;
   }

   if ( strcmp(argv[0], "initstate") == 0 ) {
      /* Initialize the state structure to 0 */
      if ( argc == 1 ) {
         send_x10state_command( ST_INIT_ALL, 0 );
         return 0;
      }
      else {
         aflags = parse_addr(argv[1], &hc, &bitmap);
         if ( !(aflags & A_VALID) || !(aflags & A_HCODE) || (aflags & A_DUMMY) ) {
            fprintf(stderr, "Invalid argument %s\n", argv[1]);
            return 1;
         }
         send_initstate_command(hc2code(hc), bitmap);
      }
      return 0;
   }

   if ( strcmp(argv[0], "initothers") == 0 ) {
      /* Initialize the cumulative address table to 0 */
      send_x10state_command( ST_INIT_OTHERS, 0 );
      return 0;
   }
   
   if ( strcmp(argv[0], "fetchstate") == 0 ) {
      return fetch_x10state();
   }
   else {
      if ( read_x10state_file() != 0 ) {
         fprintf(stderr, "Unable to read state file.\n");
         return 1;
      }
   }

   if ( strcmp(argv[0], "onstate")     == 0 ||
        strcmp(argv[0], "offstate")    == 0 ||
        strcmp(argv[0], "dimstate")    == 0 ||
        strcmp(argv[0], "fullonstate") == 0 ||
        strcmp(argv[0], "chgstate")    == 0 ||
        strcmp(argv[0], "addrstate")   == 0 ||
        strcmp(argv[0], "alertstate")  == 0 ||
        strcmp(argv[0], "clearstate")  == 0 ||
        strcmp(argv[0], "auxalertstate")  == 0 ||
        strcmp(argv[0], "auxclearstate") == 0 ||
        strcmp(argv[0], "lobatstate") == 0 ||
        strcmp(argv[0], "inactivestate") == 0 ||
        strcmp(argv[0], "activestate")   == 0 ||
        strcmp(argv[0], "activechgstate") == 0  ||
        strcmp(argv[0], "tamperstate") == 0 ||
//        strcmp(argv[0], "swminstate") == 0 ||
//        strcmp(argv[0], "swmaxstate") == 0 ||
        strcmp(argv[0], "modchgstate") == 0 ||
        strcmp(argv[0], "validstate") == 0 ) {

      if ( argc < 2 ) {
         fprintf(stderr, "%s: Housecode[Unit] needed\n", argv[0]);
         return 1;
      }

      aflags = parse_addr(argv[1], &hc, &bitmap) ;
      if ( !(aflags & A_VALID) || !(aflags & A_HCODE) || (aflags & A_DUMMY) ) {
         fprintf(stderr, "%s: Invalid Housecode|Unit '%s'\n", argv[0], argv[1]);
         return 1;
      }
      if ( aflags & A_MULT && config.state_ctrl == SC_SINGLE ) {
         fprintf(stderr, "%s: Only a single unit code is acceptable\n", argv[0]);
         return 1;
      }

      if ( aflags & A_MINUS )
         bitmap = 0;
      if ( aflags & A_PLUS && bitmap == 0 )
	 bitmap = 1;
      if ( bitmap == 0 )
         bitmap = 0xffff;

      hcode = hc2code(hc);
      
      if ( configp->state_format == NEW ) {
         if ( strcmp(argv[0], "onstate") == 0 ) 
	    state = x10state[hcode].state[OnState] & bitmap;
	 else if ( strcmp(argv[0], "offstate") == 0 )
	    state = ~x10state[hcode].state[OnState] & bitmap;
	 else if ( strcmp(argv[0], "dimstate") == 0 )
	    state = x10state[hcode].state[DimState] & bitmap;
	 else if ( strcmp(argv[0], "fullonstate") == 0 )
	    state = (~x10state[hcode].state[DimState] & x10state[hcode].state[OnState]) & bitmap;
	 else if ( strcmp(argv[0], "chgstate") == 0 )
            state = x10state[hcode].state[ChgState] & bitmap;
	 else if ( strcmp(argv[0], "modchgstate") == 0 )
            state = x10state[hcode].state[ModChgState] & bitmap;
         else if ( strcmp(argv[0], "alertstate") == 0 )
            state = x10state[hcode].state[AlertState] & bitmap;
	 else if ( strcmp(argv[0], "clearstate") == 0 )
            state = x10state[hcode].state[ClearState] & bitmap;
         else if ( strcmp(argv[0], "auxalertstate") == 0 )
            state = x10state[hcode].state[AuxAlertState] & bitmap;
	 else if ( strcmp(argv[0], "auxclearstate") == 0 )
            state = x10state[hcode].state[AuxClearState] & bitmap;
         else if ( strcmp(argv[0], "validstate") == 0 )
            state = x10state[hcode].state[ValidState] & bitmap;
	 else if ( strcmp(argv[0], "addrstate") == 0 ) {
            if ( configp->autofetch == YES && fetch_x10state() != 0 )
	       return 1;
	    state = x10state[hcode].addressed & bitmap;
	 }
         else if ( strcmp(argv[0], "lobatstate") == 0 ) {
            state = x10state[hcode].state[LoBatState] & bitmap;
         }
         else if ( strcmp(argv[0], "tamperstate") == 0 ) {
            state = x10state[hcode].state[TamperState] & bitmap;
         }
#if 0
         else if ( strcmp(argv[0], "swminstate") == 0 ) {
            state = x10state[hcode].state[SwMinState] & bitmap;
         }
         else if ( strcmp(argv[0], "swmaxstate") == 0 ) {
            state = x10state[hcode].state[SwMaxState] & bitmap;
         }
#endif
         else if ( strcmp(argv[0], "inactivestate") == 0 ) {
            fetch_x10state();
            state = x10state[hcode].state[InactiveState] & bitmap;
         }
         else if ( strcmp(argv[0], "activestate") == 0 ) {
            fetch_x10state();
            state = x10state[hcode].state[ActiveState] & bitmap;
         }
         else if ( strcmp(argv[0], "activechgstate") == 0 ) {
            fetch_x10state();
            state = x10state[hcode].state[ActiveChgState] & bitmap;
         }
	 else {
	    state = 0;
         }
	 
	 if ( bitmap == 0xffff || config.state_ctrl == SC_BITMAP )
            printf("%d\n", x10map2linmap(state));
         else
            printf("%d\n", ((state & bitmap) ? 1 : 0) );

	 return 0;
      }
            		 
      if ( configp->state_format == OLD ) {
	 /* heyuhelper format */
         if ( !bitmap ) {
            fprintf(stderr, "%s: No unit specified in '%s'\n", argv[0], argv[1]);
            return 1;
         }
		    
         hc = tolower((int)hc);
         ucode = single_bmap_unit(bitmap);
         unit = code2unit(ucode);
      
         if ( strcmp(argv[0], "onstate") == 0 ) {
            if ( x10state[hcode].state[OnState] & bitmap )
               printf("%c%dOn\n", hc, unit);
            else
               printf("%c%dOff\n", hc, unit);
         }
         else if ( strcmp(argv[0], "dimstate") == 0 ) {
            if ( x10state[hcode].state[DimState] & bitmap )
               printf("%c%dDim\n", hc, unit);
            else if ( x10state[hcode].state[OnState] & bitmap )
               printf("%c%dOn\n", hc, unit);
            else
               printf("%c%dOff\n", hc, unit);
         }
         else if ( strcmp(argv[0], "chgstate") == 0 ) {
            if ( x10state[hcode].state[ChgState] & bitmap )
               printf("%c%dChg\n", hc, unit);
            else
               printf("%c%dUnchg\n", hc, unit);
         }
         else if ( strcmp(argv[0], "modchgstate") == 0 ) {
            if ( x10state[hcode].state[ModChgState] & bitmap )
               printf("%c%dChg\n", hc, unit);
            else
               printf("%c%dUnchg\n", hc, unit);
         }
         else if ( strcmp(argv[0], "addrstate") == 0 ) {
            if ( configp->autofetch == YES && fetch_x10state() != 0 )
               return 1;	       
            if ( x10state[hcode].addressed & bitmap )
               printf("%c%dAddr\n", hc, unit);
            else
               printf("%c%dUnaddr\n", hc, unit);
         }
	 return 0;
      }
   }


   if ( strcmp(argv[0], "dimlevel")    == 0 ||
        strcmp(argv[0], "rawlevel")    == 0 ||
        strcmp(argv[0], "memlevel")    == 0 ||
        strcmp(argv[0], "rawmemlevel") == 0 ||
        strcmp(argv[0], "rcstemp")     == 0 ||
        strcmp(argv[0], "vident")      == 0 ||
        strcmp(argv[0], "sincelast")   == 0 ||
        strcmp(argv[0], "heyu_state")  == 0 ||
        strcmp(argv[0], "heyu_rawstate")  == 0 ||
        strcmp(argv[0], "heyu_vflagstate") == 0 ||
        strcmp(argv[0], "secflag_state") == 0 ||
        strcmp(argv[0], "rfxflag_state") == 0 ||
        strcmp(argv[0], "oreflag_state") == 0 ||
        strcmp(argv[0], "dmxflag_state") == 0 ||
        strcmp(argv[0], "verbose_rawstate") == 0 ||
        strcmp(argv[0], "xtend_state") == 0   ) {

      if ( argc < 2 ) {
         if ( strcmp(argv[0], "rcstemp") == 0 )
            fprintf(stderr, "%s: Housecode needed\n", argv[0]);
         else
            fprintf(stderr, "%s: Housecode|Unit needed\n", argv[0]);
         return 1;
      }

      aflags = parse_addr(argv[1], &hc, &bitmap) ;
      if ( !(aflags & A_VALID) || !(aflags & A_HCODE) || (aflags & A_DUMMY) ) {
         fprintf(stderr, "%s: Invalid Housecode|Unit '%s'\n", argv[0], argv[1]);
         return 1;
      }
      if ( aflags & A_MULT ) {
         fprintf(stderr, "%s: Only a single unit code is acceptable\n", argv[0]);
         return 1;
      }

      if ( aflags & A_MINUS )
         bitmap = 0;
      if ( aflags & A_PLUS && bitmap == 0 )
	 bitmap = 1;
      
      hcode = hc2code(hc);

      if ( strcmp(argv[0], "rcstemp") == 0 ) {
         if ( (configp->rcs_temperature & (1 << hcode)) == 0 ) {
            fprintf(stderr,
               "This housecode is not configured in the RCS_DECODE directive.\n");
            return 1;
         }
         else if ( x10global.rcstemp_flags & (unsigned long)(1 << hcode) ) {
            printf("%d\n", x10state[hcode].rcstemp);
            return 0;
         }
         else {
            fprintf(stderr, "No temperature data has been stored.\n");
            return 1;
         }
      } 

      if ( !bitmap ) {
         if ( strcmp(argv[0], "rawlevel") == 0 ) {
            if ( x10global.longdata_flags & (unsigned long)(1 << hcode) ) {
               printf("%ld\n", x10state[hcode].longdata);
               return 0;
            }
            else {
               fprintf(stderr, "No valid data has been stored.\n");
               return 1;
            }
         }
         else {
            fprintf(stderr, "%s: No unit specified in '%s'\n", argv[0], argv[1]);
            return 1;
         }
      }

      hc = tolower((int)hc);
      ucode = single_bmap_unit(bitmap);
      unit = code2unit(ucode);
      if ( strcmp(argv[0], "dimlevel") == 0 ) {
         /* Level as a percentage of full On */
         level = x10state[hcode].dimlevel[ucode];
         if ( modmask[Ext3Mask][hcode] & bitmap )
            printf("%d\n", ext3level2pct(level));
         else if ( modmask[PresetMask][hcode] & bitmap )
            printf("%d\n", presetlevel2pct(level));
         else if ( modmask[StdMask][hcode] & bitmap )
            printf("%d\n", dims2pct(level));
	 else if ( modmask[Ext0Mask][hcode] & bitmap )
            printf("%d\n", ext0level2pct(level));
         else if ( vmodmask[VkakuMask][hcode] & bitmap )
            printf("%d\n", (int)(100 * level) / 15);
         else {
            printf("%d\n", (int)(100 * level) / ondimlevel[hcode][ucode]);
         }
      }
      else if ( strcmp(argv[0], "memlevel") == 0 ) {
         /* Memory level as a percentage of full On */
         level = x10state[hcode].memlevel[ucode];
         if ( modmask[Ext3Mask][hcode] & bitmap )
            printf("%d\n", ext3level2pct(level));
         else if ( modmask[PresetMask][hcode] & bitmap )
            printf("%d\n", presetlevel2pct(level));
         else if ( modmask[StdMask][hcode] & bitmap )
            printf("%d\n", dims2pct(level));
	 else if ( modmask[Ext0Mask][hcode] & bitmap )
            printf("%d\n", ext0level2pct(level));
         else if ( vmodmask[VkakuMask][hcode] & bitmap )
            printf("%d\n", (int)(100 * level) / 15);
         else {
            printf("%d\n", (int)(100 * level) / ondimlevel[hcode][ucode]);
         }
      }
      else if ( strcmp(argv[0], "rawlevel") == 0 ) {
         /* Native module level */
         level = x10state[hcode].dimlevel[ucode];
         if ( modmask[PresetMask][hcode] & bitmap )
            printf("%d\n", level + 1);
         else {
            printf("%d\n", level);
         }
      }
      else if ( strcmp(argv[0], "rawmemlevel") == 0 ) {
         /* Native module memory level */
         level = x10state[hcode].memlevel[ucode];
         if ( modmask[PresetMask][hcode] & bitmap )
            printf("%d\n", level + 1);
         else {
            printf("%d\n", level);
         }
      }
      else if ( strcmp(argv[0], "vident") == 0 ) {
         /* Virtual ident */
         vident = x10state[hcode].vident[ucode];
         printf("0x%02lX\n", vident);
      }
      else if ( strcmp(argv[0], "sincelast") == 0 ) {
         /* Seconds since last signal */
         delta = (long)(time(NULL) - x10state[hcode].timestamp[ucode]);
         if ( x10state[hcode].timestamp[ucode] == 0 ||
              delta < 0 || delta > (365L * 86400L) ) {
            printf("-1\n");
         }
         else {
            printf("%ld\n", delta) ;
         }
      }
      else if ( strcmp(argv[0], "heyu_state") == 0 ) {
         /* Heyu script bitmap format */
         if ( configp->autofetch == YES && fetch_x10state() != 0 )
            return 1;	       
         printf("%lu\n", get_heyu_state(hcode, ucode, PCTMODE));
      } 
      else if ( strcmp(argv[0], "heyu_rawstate") == 0 ) {
         /* Heyu script bitmap format, but with native levels */
         if ( configp->autofetch == YES && fetch_x10state() != 0 )
            return 1;	       
         printf("%lu\n", get_heyu_state(hcode, ucode, RAWMODE));
      } 
      else if ( strcmp(argv[0], "xtend_state") == 0 ) {
         /* Xtend script bitmap format */
         if ( configp->autofetch == YES && fetch_x10state() != 0 )
            return 1;	       
         printf("%lu\n", get_xtend_state(hcode, ucode));
      }
      else if ( strcmp(argv[0], "heyu_vflagstate") == 0 ) {
         /* All vflags */
         printf("%lu\n", get_vflag_state(hcode, ucode));
      }
#if 0
      else if ( strcmp(argv[0], "secflag_state") == 0 ) {
         /* Security flags */
         printf("%lu\n", get_secflag_state(hcode, ucode));
      }
      else if ( strcmp(argv[0], "rfxflag_state") == 0 ) {
         /* Security flags */
         printf("%lu\n", get_rfxflag_state(hcode, ucode));
      }
      else if ( strcmp(argv[0], "oreflag_state") == 0 ) {
         /* Security flags */
         printf("%lu\n", get_oreflag_state(hcode, ucode));
      }
      else if ( strcmp(argv[0], "dmxflag_state") == 0 ) {
         /* Security flags */
         printf("%lu\n", get_dmxflag_state(hcode, ucode));
      }
#endif
      else if ( strcmp(argv[0], "verbose_rawstate") == 0 ) {
         fetch_x10state();
         value = get_heyu_state(hcode, ucode, RAWMODE);
         printf("$X10_%c%d = %lu\n", code2hc(hcode), code2unit(ucode), value);
         printf("   %-11s 0x%02lx\n", heyumaskval[0].query, value & heyumaskval[0].mask);
         j = 1;
         while ( (query = heyumaskval[j].query) ) {
            printf("   %-11s %d\n", query, ((value & heyumaskval[j].mask) ? 1 : 0) );
            j++;
         }

         value = get_vflag_state(hcode, ucode);
         printf("\n$X10_%c%d_heyu_vflagstate = %lu\n", code2hc(hcode), code2unit(ucode), value);
         j = 0;
         while ( (query = heyusecmaskval[j].query) ) {
            printf("   %-11s %d\n", query, ((value & heyusecmaskval[j].mask) ? 1 : 0) );
            j++;
         }
      } 
  
      return 0;
   }

   if ( strcmp(argv[0], "statestr") == 0 ) {
      if ( argc > 1 ) {
         aflags = parse_addr(argv[1], &hc, &bitmap) ;
         if ( !(aflags & A_VALID) || !(aflags & A_HCODE) || (aflags & A_DUMMY) ) {
            fprintf(stderr, "%s: Invalid Housecode in '%s'\n", argv[0], argv[1]);
            return 1;
         }

	 if ( aflags & A_MINUS )
            bitmap = 0;
	 if ( aflags & A_PLUS && bitmap == 0 )
	    bitmap = 1;
	 
         if ( bitmap ) {
            fprintf(stderr, "%s: Units in '%s' ignored\n", argv[0], argv[1]);
         }

	 if ( configp->autofetch == YES && fetch_x10state() != 0 )
            return 1;
	    
         hcode = hc2code(hc);
         hc = toupper((int)hc);

         printf("%s\n",
	      bmap2statestr(x10state[hcode].state[OnState],
                            x10state[hcode].state[DimState],
	                    x10state[hcode].state[ChgState],
	                    x10state[hcode].addressed) );

         return 0;
      }
      fprintf(stderr, "%s: Housecode needed\n", argv[0]);
      return 1;
   }


   fprintf(stderr, "State command '%s' not recognized\n", argv[0]);
   return 1;
}

/*---------------------------------------------------------------------+
 | Display a table showing the dimlevels of every Housecode|Unit,      |
 | expressed as a (integer) percentage of full On brightness.          |
 +---------------------------------------------------------------------*/
void show_all_dimlevels ( void )
{
   char hc;
   int  unit;
   unsigned char hcode, ucode, level;
   unsigned int mask;

   printf("%20sPercent of Full Brightness\n", "");
   printf("  Unit:");
   for ( unit = 1; unit <= 16; unit++ )
      printf("%3d ", unit);
   printf("\n");
   printf("Hcode  ");
   for ( unit = 1; unit <= 16; unit++ )
      printf(" -- ");
   printf("\n");
   
   for ( hc = 'A'; hc <= 'P'; hc++ ) {
      printf("  %c    ", hc);
      hcode = hc2code(hc);
      for ( unit = 1; unit <= 16; unit++ ) {
         ucode = unit2code(unit);
         mask = 1 << ucode;
         level = x10state[hcode].dimlevel[ucode];
         if ( modmask[Ext3Mask][hcode] & mask )
            printf("%3d ", ext3level2pct(level));
         else if ( modmask[PresetMask][hcode] & mask )
            printf("%3d ", presetlevel2pct(level));
         else if ( modmask[StdMask][hcode] & mask ) 
            printf("%3d ", dims2pct(level));
         else if ( modmask[Ext0Mask][hcode] & mask )
            printf("%3d ", ext0level2pct(level));
	 else {
	    printf("%3d ", (int)(100 * level) / ondimlevel[hcode][ucode]);
         }
      }
      printf("\n");
   }

   return;
}

/*---------------------------------------------------------------------+
 | Display a table showing the dimlevel of each Housecode|Unit in the  |
 | native form for the module at the address, i.e., 0-210 for standard |
 | modules, 1-32 for Preset modules, and 0-63 for Extended modules.    |
 | Preset and Extended levels are prefixed respectively by 'p' and 'e'.|
 +---------------------------------------------------------------------*/
void show_all_dimlevels_raw ( void )
{
   char hc;
   int  unit;
   unsigned char hcode, ucode, level;
   unsigned int mask;
   char buffer[16];

   printf("Raw Levels:\n (p: Preset 1-32, e: Ext 0-63, s: Shutter 0-25, v: Virt 00-ff, else 0-210)\n");
   printf("  Unit:");
   for ( unit = 1; unit <= 16; unit++ )
      printf("%3d ", unit);
   printf("\n");
   printf("Hcode  ");
   for ( unit = 1; unit <= 16; unit++ )
      printf(" -- ");
   printf("\n");
   
   for ( hc = 'A'; hc <= 'P'; hc++ ) {
      printf("  %c    ", hc);
      hcode = hc2code(hc);
      for ( unit = 1; unit <= 16; unit++ ) {
         ucode = unit2code(unit);
         mask = 1 << ucode;
         level = x10state[hcode].dimlevel[ucode];
         if ( modmask[Ext3Mask][hcode] & mask ) 
            sprintf(buffer, "e%d", level);
         else if ( modmask[PresetMask][hcode] & mask )
            sprintf(buffer, "p%d", level + 1);
         else if ( modmask[StdMask][hcode] & mask )
            sprintf(buffer, "%d", level);
         else if ( modmask[Ext0Mask][hcode] & mask ) 
            sprintf(buffer, "s%d", level);
         else if ( modmask[VdataMask][hcode] & mask ) 
            sprintf(buffer, "v%02x", level);
	 else 
	    sprintf(buffer, "%d", level);
         printf("%3s ", buffer);
      }
      printf("\n");
   }

   return;
}


/*---------------------------------------------------------------------+
 | Return the index to a new LAUNCHER in the array of same.            |
 +---------------------------------------------------------------------*/
int launcher_index ( LAUNCHER **launcherpp ) 
{
   static int    size, maxsize;
   static int    sizlauncher = sizeof(LAUNCHER);
   int           j, k, index, blksize = 5;

   /* Allocate initial block if not already done */
   if ( *launcherpp == NULL ) {
      *launcherpp = calloc(blksize, sizlauncher );
      if ( *launcherpp == NULL ) {
         (void) fprintf(stderr, "Unable to allocate memory for Launcher.\n");
         exit(1);
      }
      maxsize = blksize;
      size = 0;
      for ( j = 0; j < maxsize; j++ ) {
         (*launcherpp)[j].type = L_NORMAL;
         (*launcherpp)[j].matched = NO;
         (*launcherpp)[j].scanmode = 0;
         (*launcherpp)[j].line_no = -1 ;
         (*launcherpp)[j].scriptnum = -1;
         (*launcherpp)[j].label[0] = '\0';
         (*launcherpp)[j].bmaptrig = 0;
         (*launcherpp)[j].chgtrig = 0;
         for ( k = 0; k < NUM_FLAG_BANKS; k++ ) {
            (*launcherpp)[j].flags[k] = 0;
            (*launcherpp)[j].notflags[k] = 0;
         }
         (*launcherpp)[j].sflags = 0;
         (*launcherpp)[j].notsflags = 0;
         (*launcherpp)[j].vflags = 0;
         (*launcherpp)[j].notvflags = 0;
         for ( k = 0; k < NUM_COUNTER_BANKS; k++ ) {
            (*launcherpp)[j].czflags[k] = 0;
            (*launcherpp)[j].notczflags[k] = 0;
         }
         for ( k = 0; k < NUM_TIMER_BANKS; k++ ) {
            (*launcherpp)[j].tzflags[k] = 0;
            (*launcherpp)[j].nottzflags[k] = 0;
         }
         (*launcherpp)[j].bootflag = 0;
         (*launcherpp)[j].trigemuflag = 0;
         (*launcherpp)[j].unitstar = 0;
         (*launcherpp)[j].timer = 0;
         (*launcherpp)[j].sensor = 0;
         (*launcherpp)[j].afuncmap = 0;
         (*launcherpp)[j].gfuncmap = 0;
         (*launcherpp)[j].xfuncmap = 0;
         (*launcherpp)[j].sfuncmap = 0;
         (*launcherpp)[j].ofuncmap = 0;
         (*launcherpp)[j].kfuncmap = 0;
         (*launcherpp)[j].signal = 0;
         (*launcherpp)[j].module = 0;
         (*launcherpp)[j].source = 0;
         (*launcherpp)[j].nosource = 0;
         (*launcherpp)[j].oksofar = 0;
         (*launcherpp)[j].actsource = 0;
         (*launcherpp)[j].num_stflags = 0;
      }
   }

   /* Check to see if there's an available location          */
   /* If not, increase the size of the memory allocation.    */
   /* (Always leave room for a final termination indicator.) */
   if ( size == (maxsize - 1) ) {
      maxsize += blksize ;
      *launcherpp = realloc(*launcherpp, maxsize * sizlauncher );
      if ( *launcherpp == NULL ) {
         (void) fprintf(stderr, "Unable to increase size of Launcher list.\n");
         exit(1);
      }

      /* Initialize the new memory allocation */
      for ( j = size; j < maxsize; j++ ) {
         (*launcherpp)[j].type = L_NORMAL;
         (*launcherpp)[j].matched = NO;
         (*launcherpp)[j].scanmode = 0;
         (*launcherpp)[j].line_no = -1 ;
         (*launcherpp)[j].scriptnum = -1;
         (*launcherpp)[j].label[0] = '\0';
         (*launcherpp)[j].bmaptrig = 0;
         (*launcherpp)[j].chgtrig = 0;
         for ( k = 0; k < NUM_FLAG_BANKS; k++ ) {
            (*launcherpp)[j].flags[k] = 0;
            (*launcherpp)[j].notflags[k] = 0;
         }
         (*launcherpp)[j].sflags = 0;
         (*launcherpp)[j].notsflags = 0;
         (*launcherpp)[j].vflags = 0;
         (*launcherpp)[j].notvflags = 0;
         for ( k = 0; k < NUM_COUNTER_BANKS; k++ ) {
            (*launcherpp)[j].czflags[k] = 0;
            (*launcherpp)[j].notczflags[k] = 0;
         }
         for ( k = 0; k < NUM_TIMER_BANKS; k++ ) {
            (*launcherpp)[j].tzflags[k] = 0;
            (*launcherpp)[j].nottzflags[k] = 0;
         }
         (*launcherpp)[j].bootflag = 0;
         (*launcherpp)[j].trigemuflag = 0;
         (*launcherpp)[j].unitstar = 0;
         (*launcherpp)[j].timer = 0;
         (*launcherpp)[j].sensor = 0;
         (*launcherpp)[j].afuncmap = 0;
         (*launcherpp)[j].gfuncmap = 0;
         (*launcherpp)[j].xfuncmap = 0;
         (*launcherpp)[j].sfuncmap = 0;
         (*launcherpp)[j].ofuncmap = 0;
         (*launcherpp)[j].kfuncmap = 0;
         (*launcherpp)[j].signal = 0;
         (*launcherpp)[j].module = 0;
         (*launcherpp)[j].source = 0;
         (*launcherpp)[j].nosource = 0;
         (*launcherpp)[j].oksofar = 0;
         (*launcherpp)[j].actsource = 0;
         (*launcherpp)[j].num_stflags = 0;
      }
   }

   index = size;
   size += 1;
   return index;
}

/*---------------------------------------------------------------------+
 | Parse a launch condition for common flags, security flags, czflags, |
 | and scanmode and load into the LAUNCHER.                            |
 +---------------------------------------------------------------------*/
int get_global_flags( LAUNCHER *launcherp, int tokc, char **tokv ) 
{
   int           k, m, flag;  
   char          *sp;
   char          errmsg[128];
   char          tbuf[32];
   unsigned int  aflags, bitmap;
   char          hc;

   launcherp->sflags = launcherp->notsflags = launcherp->scanmode = 0;
   launcherp->num_stflags = launcherp->num_virtflags = 0;

   for ( k = 0; k < NUM_FLAG_BANKS; k++ ) {
      launcherp->flags[k] = 0;
      launcherp->notflags[k] = 0;
   }
   for ( k = 0; k < NUM_COUNTER_BANKS; k++ ) {
      launcherp->czflags[k] = 0;
      launcherp->notczflags[k] = 0;
   }

   for ( k = 0; k < NUM_TIMER_BANKS; k++ ) {
      launcherp->tzflags[k] = 0;
      launcherp->nottzflags[k] = 0;
   }

   launcherp->num_stflags = 0;

   /* Scan tokens for flags */
   for ( k = 1; k < tokc; k++ ) {
      strncpy2(tbuf, tokv[k], (sizeof(tbuf)));
      strlower(tbuf);

      /* Search for state flags */
      if ( strchr(tbuf, ':') ) {
         for ( m = 0; m < num_stflags; m++ ) {
            if ( strncmp(tbuf, stflags[m].label, stflags[m].length) == 0 ) {
               aflags = parse_addr(tokv[k] + stflags[m].length, &hc, &bitmap);
               if ( (aflags & (A_VALID | A_HCODE | A_BMAP | A_PLUS | A_MINUS)) != (A_VALID | A_HCODE | A_BMAP) ) {
                  sprintf(errmsg, "Invalid state flag housecode|unit '%s'", tokv[k] + stflags[m].length);
                  store_error_message(errmsg);
                  return -1;
               }
               if ( aflags & A_MULT ) {
                  sprintf(errmsg, "'%s' Only a single unit code is acceptable", tokv[k] + stflags[m].length);
                  store_error_message(errmsg);
                  return 1;
               }

               if ( launcherp->num_stflags >= 32 ) {
                  store_error_message("Too many state flags - 32 max.");
                  return -1;
               }
               launcherp->stlist[launcherp->num_stflags].stindex = m;
               launcherp->stlist[launcherp->num_stflags].hcode = hc2code(hc);
               launcherp->stlist[launcherp->num_stflags].bmap = bitmap;
               launcherp->num_stflags++;

               *tokv[k] = '\0';
               break;
            }
         }

         if ( !(*tokv[k]) )
            continue;

         for ( m = 0; m < num_virtflags; m++ ) {
            if ( strncmp(tbuf, virtflags[m].label, virtflags[m].length) == 0 ) {
               aflags = parse_addr(tokv[k] + virtflags[m].length, &hc, &bitmap);
               if ( (aflags & (A_VALID | A_HCODE | A_BMAP | A_PLUS | A_MINUS)) != (A_VALID | A_HCODE | A_BMAP) ) {
                  sprintf(errmsg, "Invalid state flag housecode|unit '%s'", tokv[k] + virtflags[m].length);
                  store_error_message(errmsg);
                  return -1;
               }
               if ( aflags & A_MULT ) {
                  sprintf(errmsg, "'%s' Only a single unit code is acceptable", tokv[k] + virtflags[m].length);
                  store_error_message(errmsg);
                  return 1;
               }

               if ( launcherp->num_virtflags >= 32 ) {
                  store_error_message("Too many state flags - 32 max.");
                  return -1;
               }
               launcherp->virtlist[launcherp->num_virtflags].vfindex = m;
               launcherp->virtlist[launcherp->num_virtflags].hcode = hc2code(hc);
               launcherp->virtlist[launcherp->num_virtflags].ucode = single_bmap_unit(bitmap);
               launcherp->num_virtflags++;

               *tokv[k] = '\0';
               break;
            }
         }
      }

      if ( !(*tokv[k]) )
         continue;

      /* Search for other flags and keywords */
      if ( strncmp("flag", tbuf, 4) == 0 ) {
         flag = (int)strtol(tbuf + 4, &sp, 10);
         if ( !strchr(" /t/n", *sp) || flag < 1 || flag > (32 * NUM_FLAG_BANKS) ) {
            sprintf(errmsg, "Invalid flag launch parameter '%s'", tokv[k]);
            store_error_message(errmsg);
            return -1;
         }
         launcherp->flags[(flag - 1) / 32] |= 1UL << ((flag - 1) % 32);
         *tokv[k] = '\0';
      }
      else if ( strncmp("notflag", tbuf, 7) == 0 ) {
         flag = (int)strtol(tbuf + 7, &sp, 10);
         if ( !strchr(" /t/n", *sp) || flag < 1 || flag > (32 * NUM_FLAG_BANKS) ) {
            sprintf(errmsg, "Invalid notflag launch parameter '%s'", tokv[k]);
            store_error_message(errmsg);
            return -1;
         }
         launcherp->notflags[(flag - 1) / 32] |= 1UL << ((flag - 1) % 32);
         *tokv[k] = '\0';
      }

      else if ( strncmp("czflag", tbuf, 6) == 0 ) {
         flag = (int)strtol(tbuf + 6, &sp, 10);
         if ( !strchr(" /t/n", *sp) || flag < 1 || flag > (32 * NUM_COUNTER_BANKS)) {
            sprintf(errmsg, "Invalid czflag launch parameter '%s'", tokv[k]);
            store_error_message(errmsg);
            return -1;
         }
         launcherp->czflags[(flag - 1) / 32] |= 1UL << ((flag - 1) % 32);
         *tokv[k] = '\0';
      }
      else if ( strncmp("notczflag", tbuf, 9) == 0 ) {
         flag = (int)strtol(tbuf + 9, &sp, 10);
         if ( !strchr(" /t/n", *sp) || flag < 1 || flag > (32 * NUM_COUNTER_BANKS)) {
            sprintf(errmsg, "Invalid notczflag launch parameter '%s'", tokv[k]);
            store_error_message(errmsg);
            return -1;
         }
         launcherp->notczflags[(flag - 1) / 32] |= 1UL << ((flag - 1) % 32);
         *tokv[k] = '\0';
      }

      else if ( strncmp("tzflag", tbuf, 6) == 0 ) {
         flag = (int)strtol(tbuf + 6, &sp, 10);
         if ( !strchr(" /t/n", *sp) || flag < 1 || flag > (32 * NUM_TIMER_BANKS)) {
            sprintf(errmsg, "Invalid tzflag launch parameter '%s'", tokv[k]);
            store_error_message(errmsg);
            return -1;
         }
         launcherp->tzflags[(flag - 1) / 32] |= 1UL << ((flag - 1) % 32);
         *tokv[k] = '\0';
      }
      else if ( strncmp("nottzflag", tbuf, 9) == 0 ) {
         flag = (int)strtol(tbuf + 9, &sp, 10);
         if ( !strchr(" /t/n", *sp) || flag < 1 || flag > (32 * NUM_TIMER_BANKS)) {
            sprintf(errmsg, "Invalid nottzflag launch parameter '%s'", tokv[k]);
            store_error_message(errmsg);
            return -1;
         }
         launcherp->nottzflags[(flag - 1) / 32] |= 1UL << ((flag - 1) % 32);
         *tokv[k] = '\0';
      }

      else if ( strcmp("night", tbuf) == 0 ) {
         launcherp->sflags |= NIGHT_FLAG;
         *tokv[k] = '\0';
      }
      else if ( strcmp("notnight", tbuf) == 0 ) {
         launcherp->notsflags |= NIGHT_FLAG;
         *tokv[k] = '\0';
      }
      else if ( strcmp("dark", tbuf) == 0 ) {
         launcherp->sflags |= DARK_FLAG;
         *tokv[k] = '\0';
      }
      else if ( strcmp("notdark", tbuf) == 0 ) {
         launcherp->notsflags |= DARK_FLAG;
         *tokv[k] = '\0';
      }
      else if ( strcmp("tamper", tbuf) == 0 ) {
         launcherp->sflags |= GLOBSEC_TAMPER;
         *tokv[k] = '\0';
      }
      else if ( strcmp("nottamper", tbuf) == 0 ) {
         launcherp->notsflags |= GLOBSEC_TAMPER;
         *tokv[k] = '\0';
      }
      else if ( strcmp("armed", tbuf) == 0 ) {
         launcherp->sflags |= GLOBSEC_ARMED;
         *tokv[k] = '\0';
      }
      else if ( strcmp("notarmed", tbuf) == 0 ) {
         launcherp->notsflags |= GLOBSEC_ARMED;
         *tokv[k] = '\0';
      }
      else if ( strcmp("disarmed", tbuf) == 0 ) {
         launcherp->notsflags |= (GLOBSEC_ARMED | GLOBSEC_PENDING);
         *tokv[k] = '\0';
      }
      else if ( strcmp("notdisarmed", tbuf) == 0 ) {
         launcherp->sflags |= (GLOBSEC_ARMED | GLOBSEC_PENDING);
         *tokv[k] = '\0';
      }
      else if ( strcmp("armpending", tbuf) == 0 ) {
         launcherp->sflags |= GLOBSEC_PENDING;
         *tokv[k] = '\0';
      }
      else if ( strcmp("notarmpending", tbuf) == 0 ) {
         launcherp->notsflags |= GLOBSEC_PENDING;
         *tokv[k] = '\0';
      }
      else if ( strcmp("home", tbuf) == 0 ) {
         launcherp->sflags |= GLOBSEC_HOME;
         *tokv[k] = '\0';
      }
      else if ( strcmp("away", tbuf) == 0 ) {
         launcherp->notsflags |= GLOBSEC_HOME;
         *tokv[k] = '\0';
      }
      else if ( strcmp("continue", tbuf) == 0 ) {
         launcherp->scanmode |= FM_CONTINUE;
         *tokv[k] = '\0';
      }
      else if ( strcmp("break", tbuf) == 0 ) {
         launcherp->scanmode |= FM_BREAK;
         *tokv[k] = '\0';
      }
   }

   /* home and away flags require armed or armpending - default to armed */
   if ( ((launcherp->sflags | launcherp->notsflags) & GLOBSEC_HOME) &&
        !(launcherp->sflags & (GLOBSEC_ARMED | GLOBSEC_PENDING)) ) {
      launcherp->sflags |= GLOBSEC_ARMED;
   }


   for ( k = 0; k < NUM_FLAG_BANKS; k++ ) {
      if ( launcherp->flags[k] & launcherp->notflags[k] ) {
         store_error_message("Conflicting common flag/notflag in launch conditions.");
         return -1;
      }
   }

   for ( k = 0; k < NUM_COUNTER_BANKS; k++ ) {
      if ( launcherp->czflags[k] & launcherp->notczflags[k] ) {
         store_error_message("Conflicting czflag/notczflag in launch conditions.");
         return -1;
      }
   }

   for ( k = 0; k < NUM_TIMER_BANKS; k++ ) {
      if ( launcherp->tzflags[k] & launcherp->nottzflags[k] ) {
         store_error_message("Conflicting tzflag/nottzflag in launch conditions.");
         return -1;
      }
   }

   if ( launcherp->sflags & launcherp->notsflags ) {
      store_error_message("Conflicting global flag/notflag in launch conditions.");
      return -1;
   }

   if ( launcherp->scanmode == (FM_CONTINUE | FM_BREAK) ) {
      store_error_message("Conflicting continue/break in launch conditions.");
      return -1;
   }

   return 0;
} 

/*---------------------------------------------------------------------+
 | Parse a launch condition for common, security, and czflags and      |
 | return the flagmaps via the argument list.                          |
 +---------------------------------------------------------------------*/
int get_global_flags_old ( unsigned long *cflags, unsigned long *notcflags,
                       unsigned long *sflags, unsigned long *notsflags,
                       unsigned long *czflags, unsigned long *notczflags,
                       unsigned char *scanmode, int tokc, char **tokv ) 
{
   int           k, flag;  
   char          *sp;
   char          errmsg[128];
   char          tbuf[32];

   *sflags = *notsflags = *scanmode = 0;

   for ( k = 0; k < NUM_FLAG_BANKS; k++ ) {
      cflags[k] = 0;
      notcflags[k] = 0;
   }

   for ( k = 0; k < NUM_COUNTER_BANKS; k++ ) {
      czflags[k] = 0;
      notczflags[k] = 0;
   }

   /* Scan the remainder of the tokens for keywords */
   for ( k = 1; k < tokc; k++ ) {
      strncpy2(tbuf, tokv[k], (sizeof(tbuf)));
      strlower(tbuf);

      if ( strncmp("flag", tbuf, 4) == 0 ) {
         flag = (int)strtol(tbuf + 4, &sp, 10);
         if ( !strchr(" /t/n", *sp) || flag < 1 || flag > (32 * NUM_FLAG_BANKS) ) {
            sprintf(errmsg, "Invalid flag launch parameter '%s'", tokv[k]);
            store_error_message(errmsg);
            return -1;
         }
         cflags[(flag - 1) / 32] |= 1UL << ((flag - 1) % 32);
         *tokv[k] = '\0';
      }
      else if ( strncmp("notflag", tbuf, 7) == 0 ) {
         flag = (int)strtol(tbuf + 7, &sp, 10);
         if ( !strchr(" /t/n", *sp) || flag < 1 || flag > (32 * NUM_FLAG_BANKS) ) {
            sprintf(errmsg, "Invalid notflag launch parameter '%s'", tokv[k]);
            store_error_message(errmsg);
            return -1;
         }
         notcflags[(flag - 1) / 32] |= 1UL << ((flag - 1) % 32);
         *tokv[k] = '\0';
      }

      else if ( strncmp("czflag", tbuf, 6) == 0 ) {
         flag = (int)strtol(tbuf + 6, &sp, 10);
         if ( !strchr(" /t/n", *sp) || flag < 1 || flag > (32 * NUM_COUNTER_BANKS)) {
            sprintf(errmsg, "Invalid cxflag launch parameter '%s'", tokv[k]);
            store_error_message(errmsg);
            return -1;
         }
         czflags[(flag - 1) / 32] |= 1UL << ((flag - 1) % 32);
         *tokv[k] = '\0';
      }
      else if ( strncmp("notczflag", tbuf, 9) == 0 ) {
         flag = (int)strtol(tbuf + 9, &sp, 10);
         if ( !strchr(" /t/n", *sp) || flag < 1 || flag > (32 * NUM_COUNTER_BANKS)) {
            sprintf(errmsg, "Invalid notczflag launch parameter '%s'", tokv[k]);
            store_error_message(errmsg);
            return -1;
         }
         notczflags[(flag - 1) / 32] |= 1UL << ((flag - 1) % 32);
         *tokv[k] = '\0';
      }

      else if ( strcmp("night", tbuf) == 0 ) {
         *sflags |= NIGHT_FLAG;
         *tokv[k] = '\0';
      }
      else if ( strcmp("notnight", tbuf) == 0 ) {
         *notsflags |= NIGHT_FLAG;
         *tokv[k] = '\0';
      }
      else if ( strcmp("dark", tbuf) == 0 ) {
         *sflags |= DARK_FLAG;
         *tokv[k] = '\0';
      }
      else if ( strcmp("notdark", tbuf) == 0 ) {
         *notsflags |= DARK_FLAG;
         *tokv[k] = '\0';
      }
      else if ( strcmp("tamper", tbuf) == 0 ) {
         *sflags |= GLOBSEC_TAMPER;
         *tokv[k] = '\0';
      }
      else if ( strcmp("nottamper", tbuf) == 0 ) {
         *notsflags |= GLOBSEC_TAMPER;
         *tokv[k] = '\0';
      }
      else if ( strcmp("armed", tbuf) == 0 ) {
         *sflags |= GLOBSEC_ARMED;
         *tokv[k] = '\0';
      }
      else if ( strcmp("notarmed", tbuf) == 0 ) {
         *notsflags |= GLOBSEC_ARMED;
         *tokv[k] = '\0';
      }
      else if ( strcmp("disarmed", tbuf) == 0 ) {
         *notsflags |= (GLOBSEC_ARMED | GLOBSEC_PENDING);
         *tokv[k] = '\0';
      }
      else if ( strcmp("notdisarmed", tbuf) == 0 ) {
         *sflags |= (GLOBSEC_ARMED | GLOBSEC_PENDING);
         *tokv[k] = '\0';
      }
      else if ( strcmp("armpending", tbuf) == 0 ) {
         *sflags |= GLOBSEC_PENDING;
         *tokv[k] = '\0';
      }
      else if ( strcmp("notarmpending", tbuf) == 0 ) {
         *notsflags |= GLOBSEC_PENDING;
         *tokv[k] = '\0';
      }
      else if ( strcmp("home", tbuf) == 0 ) {
         *sflags |= GLOBSEC_HOME;
         *tokv[k] = '\0';
      }
      else if ( strcmp("away", tbuf) == 0 ) {
         *notsflags |= GLOBSEC_HOME;
         *tokv[k] = '\0';
      }
      else if ( strcmp("continue", tbuf) == 0 ) {
         *scanmode |= FM_CONTINUE;
         *tokv[k] = '\0';
      }
      else if ( strcmp("break", tbuf) == 0 ) {
         *scanmode |= FM_BREAK;
         *tokv[k] = '\0';
      }
   }

   /* home and away flags require armed or armpending - default to armed */
   if ( ((*sflags | *notsflags) & GLOBSEC_HOME) &&
        !(*sflags & (GLOBSEC_ARMED | GLOBSEC_PENDING)) ) {
      *sflags |= GLOBSEC_ARMED;
   }


   for ( k = 0; k < NUM_FLAG_BANKS; k++ ) {
      if ( cflags[k] & notcflags[k] ) {
         store_error_message("Conflicting common flag/notflag in launch conditions.");
         return -1;
      }
   }

   for ( k = 0; k < NUM_COUNTER_BANKS; k++ ) {
      if ( czflags[k] & notczflags[k] ) {
         store_error_message("Conflicting czflag/notczflag in launch conditions.");
         return -1;
      }
   }

   if ( *sflags & *notsflags ) {
      store_error_message("Conflicting global flag/notflag in launch conditions.");
      return -1;
   }
   if ( *scanmode == (FM_CONTINUE | FM_BREAK) ) {
      store_error_message("Conflicting continue/break in launch conditions.");
      return -1;
   }

   return 0;
} 

/*---------------------------------------------------------------------+
 | Create an exec launcher, i.e., the script is launched only from the |
 | command line with the 'heyu launch' command.                        |
 | Return 0 if successful, or -1 if error.                             |
 +---------------------------------------------------------------------*/
int create_exec_launcher ( LAUNCHER *launcherp, int tokc, char **tokv )
{
   int j;
   char errmsg[128];

   launcherp->type = L_EXEC;

   if ( get_global_flags(launcherp, tokc, tokv) != 0 ) {
      return -1;
   }

   for ( j = 1; j < tokc; j++ ) {
      if ( *tokv[j] != '\0' ) {
        sprintf(errmsg, "Launch parameter '%s' is invalid for -exec script", tokv[j]);
        store_error_message(errmsg);
        return -1;
      }
   }

   launcherp->oksofar = 1;

   return 0;
}

/*---------------------------------------------------------------------+
 | Create an -hourly launcher, triggered once every hour on the hour.  |
 | Return 0 if successful, or -1 if error.                             |
 +---------------------------------------------------------------------*/
int create_hourly_launcher ( LAUNCHER *launcherp, int tokc, char **tokv )
{
   int j;
   char errmsg[128];

   launcherp->type = L_HOURLY;

   if ( get_global_flags(launcherp, tokc, tokv) != 0 ) {
      return -1;
   }

   for ( j = 1; j < tokc; j++ ) {
      if ( *tokv[j] != '\0' ) {
        sprintf(errmsg, "Launch parameter '%s' is invalid for -hourly script", tokv[j]);
        store_error_message(errmsg);
        return -1;
      }
   }

   launcherp->oksofar = 1;

   return 0;
}

/*---------------------------------------------------------------------+
 | Create a sensorfail launcher, i.e., triggered when any security     |
 | security sensor inactive countdown reaches zero.                    |
 | Return 0 if successful, or -1 if error.                             |
 +---------------------------------------------------------------------*/
int create_sensorfail_launcher ( LAUNCHER *launcherp, int tokc, char **tokv ) 
{
   int           k;  
   char          errmsg[128];

   launcherp->type = L_SENSORFAIL;

   launcherp->bmaptrigemu = 0xffff;

   /* Scan tokens for Global flags */
   if ( get_global_flags(launcherp, tokc, tokv) != 0 ) {
      return -1;
   }

   for ( k = 1; k < tokc; k++ ) {
      if ( *tokv[k] != '\0' ) {
        sprintf(errmsg, "Launch parameter '%s' is invalid for -sensorfail script", tokv[k]);
        store_error_message(errmsg);
        return -1;
      }
   }

   launcherp->oksofar = 1;

   return 0;

} 


/*---------------------------------------------------------------------+
 | Create a timeout launcher, i.e., triggered when the specified timer |
 | (1-16) countdown reaches zero.                                      |
 | Return 0 if successful, or -1 if error.                             |
 +---------------------------------------------------------------------*/
int create_timeout_launcher ( LAUNCHER *launcherp, int tokc, char **tokv ) 
{
   int           k, timer, tcount = 0;  
   char          *sp;
   char          errmsg[128];
   char          tbuf[32];

   launcherp->type = L_TIMEOUT;

   launcherp->bmaptrigemu = 0xffff;

   /* Scan tokens for Global flags */
   if ( get_global_flags(launcherp, tokc, tokv) != 0 ) {
      return -1;
   }

   /* Scan the remainder of the tokens for timer flags */
   for ( k = 1; k < tokc; k++ ) {
      strncpy2(tbuf, tokv[k], (sizeof(tbuf)));
      strlower(tbuf);

      if ( strncmp("timer", tbuf, 5) == 0 ) {
         timer = (int)strtol(tbuf + 5, &sp, 10);
         if ( !strchr(" /t/n", *sp) || timer < 1 || timer > NUM_USER_TIMERS ) {
            sprintf(errmsg, "Invalid timer launch parameter '%s'", tokv[k]);
            store_error_message(errmsg);
            return -1;
         }
         launcherp->timer = timer;
         tcount++;
         *tokv[k] = '\0';
      }
      else if ( strcmp("armdelay", tbuf) == 0 ) {
         launcherp->timer = 0;
         tcount++;
         *tokv[k] = '\0';
      }
   }

   for ( k = 1; k < tokc; k++ ) {
      if ( *tokv[k] != '\0' ) {
        sprintf(errmsg, "Launch parameter '%s' is invalid for -timer script", tokv[k]);
        store_error_message(errmsg);
        return -1;
      }
   }

   if ( tcount == 0 ) {
      store_error_message("No timer (armtimer or timer1 ... timer32) has been specified.");
      return -1;
   }
   else if ( tcount > 1 ) {
      store_error_message("Only one timer (armtimer or timer1 ... timer32) may be specified.");
      return -1;
   }
         
   launcherp->oksofar = 1;

   return 0;
} 


/*---------------------------------------------------------------------+
 | Create an RF Flood launcher, i.e., triggered when heyu_aux detects  |
 | an unbroken flood of RF signals.                                    |
 | Return 0 if successful, or -1 if error.                             |
 +---------------------------------------------------------------------*/
int create_rfflood_launcher ( LAUNCHER *launcherp, int tokc, char **tokv ) 
{
   int           k;  
   char          errmsg[128];
   char          tbuf[32];

   launcherp->type = L_RFFLOOD;

   launcherp->bmaptrigemu = 0xffff;

   /* Scan tokens for Global flags */
   if ( get_global_flags(launcherp, tokc, tokv) != 0 ) {
      return -1;
   }

   /* Scan the remainder of the tokens for specific flags */
   for ( k = 1; k < tokc; k++ ) {
      strncpy2(tbuf, tokv[k], (sizeof(tbuf)));
      strlower(tbuf);

      if ( strcmp("started", tbuf) == 0 ) {
         launcherp->sflags |= GLOBSEC_FLOOD;
         *tokv[k] = '\0';
      }
      else if ( strcmp("ended", tbuf) == 0 ) {
         launcherp->notsflags |= GLOBSEC_FLOOD;
         *tokv[k] = '\0';
      }
      else {
         sprintf(errmsg, "Launch parameter '%s' is invalid for -rfflood launcher", tokv[k]);
         store_error_message(errmsg);
         return -1;
      }
   }

         
   launcherp->oksofar = 1;

   return 0;
} 

/*---------------------------------------------------------------------+
 | Create an -rfjamming launcher, i.e., triggered when heyu_aux        |
 | receives an RF Jamming signal from an (older) RFXCOM receiver.      |
 | Return 0 if successful, or -1 if error.                             |
 +---------------------------------------------------------------------*/
int create_rfxjam_launcher ( LAUNCHER *launcherp, int tokc, char **tokv ) 
{
   int           k;  
   char          errmsg[128];
   char          tbuf[32];

   launcherp->type = L_RFXJAM;

   launcherp->bmaptrigemu = 0xffff;

   /* Scan tokens for Global flags */
   if ( get_global_flags(launcherp, tokc, tokv) != 0 ) {
      return -1;
   }

   /* Scan the remainder of the tokens for specific flags */
   for ( k = 1; k < tokc; k++ ) {
      strncpy2(tbuf, tokv[k], (sizeof(tbuf)));
      strlower(tbuf);

      if ( strcmp("started", tbuf) == 0 ) {
         launcherp->sflags |= GLOBSEC_FLOOD;
         *tokv[k] = '\0';
      }
      else if ( strcmp("ended", tbuf) == 0 ) {
         launcherp->notsflags |= GLOBSEC_FLOOD;
         *tokv[k] = '\0';
      }
      else if ( strcmp("main", tbuf) == 0 ) {
         launcherp->vflags |= SEC_MAIN;
         *tokv[k] = '\0';
      }
      else if ( strcmp("aux", tbuf) == 0 ) {
         launcherp->vflags |= SEC_AUX;
         *tokv[k] = '\0';
      }
      else {
         sprintf(errmsg, "Launch parameter '%s' is invalid for -rfxjam launcher", tokv[k]);
         store_error_message(errmsg);
         return -1;
      }
   }

         
   launcherp->oksofar = 1;

   return 0;
} 

/*---------------------------------------------------------------------+
 | Create a lockup launcher, i.e., triggered when heyu_relay says the  |
 | CM11A appears to be locked up.                                      |
 | Return 0 if successful, or -1 if error.                             |
 +---------------------------------------------------------------------*/
int create_lockup_launcher ( LAUNCHER *launcherp, int tokc, char **tokv ) 
{
   int           k;  
   char          errmsg[128];
   char          tbuf[32];

   launcherp->type = L_LOCKUP;

   launcherp->bmaptrigemu = 0xffff;

   /* Scan tokens for Global flags */
   if ( get_global_flags(launcherp, tokc, tokv) != 0 ) {
      return -1;
   }

   /* Scan the remainder of the tokens for specific flags */
   for ( k = 1; k < tokc; k++ ) {
      strncpy2(tbuf, tokv[k], (sizeof(tbuf)));
      strlower(tbuf);

      if ( strcmp("started", tbuf) == 0 ) {
         launcherp->sflags |= GLOBSEC_FLOOD;
         *tokv[k] = '\0';
      }
      else if ( strcmp("ended", tbuf) == 0 ) {
         launcherp->notsflags |= GLOBSEC_FLOOD;
         *tokv[k] = '\0';
      }
      else if ( strcmp("boot", tbuf) == 0 ) {
         launcherp->bootflag |= R_ATSTART;
      }
      else if ( strcmp("notboot", tbuf) == 0 ) {
         launcherp->bootflag |= R_NOTATSTART;
      }
      else {
         sprintf(errmsg, "Launch parameter '%s' is invalid for -lockup launcher", tokv[k]);
         store_error_message(errmsg);
         return -1;
      }
   }

   if ( launcherp->bootflag == (R_ATSTART | R_NOTATSTART) ) {
      store_error_message("Conflicting boot/notboot in launch conditions.");
      return -1;
   }

         
   launcherp->oksofar = 1;

   return 0;
} 

/*---------------------------------------------------------------------+
 | Create a powerfail launcher, i.e., triggered when the CM11A polls   |
 | for a time update.  Return 0 if successful, or -1 if error.         |
 +---------------------------------------------------------------------*/
int create_powerfail_launcher ( LAUNCHER *launcherp, int tokc, char **tokv ) 
{
   int           k;  
   char          errmsg[128];
   char          tbuf[32];

   launcherp->type = L_POWERFAIL;

   launcherp->bmaptrigemu = 0xffff;

   /* Scan tokens for Global flags */
   if ( get_global_flags(launcherp, tokc, tokv) != 0 ) {
      return -1;
   }

   /* Scan the remainder of the tokens for specific flags */
   for ( k = 1; k < tokc; k++ ) {
      strncpy2(tbuf, tokv[k], (sizeof(tbuf)));
      strlower(tbuf);

      if ( strcmp("boot", tbuf) == 0 ) {
         launcherp->bootflag |= R_ATSTART;
      }
      else if ( strcmp("notboot", tbuf) == 0 ) {
         launcherp->bootflag |= R_NOTATSTART;
      }
      else {
         sprintf(errmsg, "Launch parameter '%s' is invalid for -powerfail launcher", tokv[k]);
         store_error_message(errmsg);
         return -1;
      }
   }

   if ( launcherp->bootflag == (R_ATSTART | R_NOTATSTART) ) {
      store_error_message("Conflicting boot/notboot in launch conditions.");
      return -1;
   }
         
   launcherp->oksofar = 1;

   return 0;
} 

/*---------------------------------------------------------------------+
 | Create a normal launcher, i.e., triggered with a standard X10       |
 | signal.  Return 0 if successful, or -1 if error.                    |
 +---------------------------------------------------------------------*/
int create_normal_launcher ( LAUNCHER *launcherp, int tokc, char **tokv ) 
{
   int           k, m, mm, mask, flag, found_func;  
   char          hc;
   char          errmsg[128];
   unsigned char hcode, change;
   unsigned char actual, generic, signal, module, address;
   unsigned int  aflags, source, nosource;
   unsigned int  bitmap;
   char          tbuf[32];

//   int get_global_flags ( LAUNCHER *, int, char ** );
   change = 0; module = 0; signal = 0; source = 0; nosource = 0;
   actual = 0; generic = 0; address = 0;

   launcherp->type = L_NORMAL;
   launcherp->bmaptrigemu = 0xffff;

   aflags = parse_addr(tokv[0], &hc, &bitmap);
   if ( !(aflags & A_VALID) || !(aflags & A_HCODE) || !bitmap ) {
      sprintf(errmsg, "Invalid Housecode|Units '%s' in Launcher.", tokv[0]);
      store_error_message(errmsg);
      return -1;
   }
   if ( aflags & A_STAR ) {
      launcherp->unitstar = 1;
   }

   hcode = hc2code(hc);
   launcherp->hcode = hc2code(hc);
   launcherp->bmaptrig = bitmap;

   /* Scan tokens for Global flags */
   if ( get_global_flags(launcherp, tokc, tokv) != 0 ) {
      return -1;
   }

   /* Scan the remainder of the tokens for specific flags and keywords */
   for ( k = 1; k < tokc; k++ ) {
      strncpy2(tbuf, tokv[k], (sizeof(tbuf)));
      strlower(tbuf);
      if ( strcmp("address", tbuf) == 0 ) {
         address = 1;
         *tokv[k] = '\0';
      }
      else if ( strcmp("changed", tbuf) == 0 ) {
         change = 1;
         *tokv[k] = '\0';
      }
      else if ( strcmp("module", tbuf) == 0 ) {
         module = 1;
         *tokv[k] = '\0';
      }
      else if ( strcmp("signal", tbuf) == 0 ) {
         signal = 1;
         *tokv[k] = '\0';
      }
      else if ( strcmp("nosrc", tbuf) == 0 ) {
         source |= LSNONE; 
         *tokv[k] = '\0';
      }
      else if ( strcmp("anysrc", tbuf) == 0 ) {
         source |= LSALL; 
         *tokv[k] = '\0';
      }
      else if ( strcmp("sndc", tbuf) == 0 ) {
         source |= SNDC; 
         *tokv[k] = '\0';
      }
      else if ( strcmp("snds", tbuf) == 0 ) {
         source |= SNDS; 
         *tokv[k] = '\0';
      }
      else if ( strcmp("sndm", tbuf) == 0 ) {
         source |= SNDM; 
         *tokv[k] = '\0';
      }
      else if ( strcmp("snda", tbuf) == 0 ) {
         source |= SNDA; 
         *tokv[k] = '\0';
      }
      else if ( strcmp("sndt", tbuf) == 0 ) {
         source |= SNDT; 
         *tokv[k] = '\0';
      }
      else if ( strcmp("rcvi", tbuf) == 0 ) {
         source |= RCVI; 
         *tokv[k] = '\0';
      }
      else if ( strcmp("rcvt", tbuf) == 0 ) {
         source |= RCVT; 
         *tokv[k] = '\0';
      }
      else if ( strcmp("sndp", tbuf) == 0 ) {
         source |= SNDP; 
         *tokv[k] = '\0';
      }
      else if ( strcmp("snda", tbuf) == 0 ) {
         source |= SNDA; 
         *tokv[k] = '\0';
      }
      else if ( strcmp("rcva", tbuf) == 0 ) {
         source |= RCVA; 
         *tokv[k] = '\0';
      }
      else if ( strcmp("xmtf", tbuf) == 0 ) {
         source |= XMTF; 
         *tokv[k] = '\0';
      }

      else if ( strcmp("nosndc", tbuf) == 0 ) {
         nosource |= SNDC; 
         *tokv[k] = '\0';
      }
      else if ( strcmp("nosnds", tbuf) == 0 ) {
         nosource |= SNDS; 
         *tokv[k] = '\0';
      }
      else if ( strcmp("nosndm", tbuf) == 0 ) {
         nosource |= SNDM; 
         *tokv[k] = '\0';
      }
      else if ( strcmp("nosnda", tbuf) == 0 ) {
         nosource |= SNDA; 
         *tokv[k] = '\0';
      }
      else if ( strcmp("nosndt", tbuf) == 0 ) {
         nosource |= SNDT; 
         *tokv[k] = '\0';
      }
      else if ( strcmp("norcvi", tbuf) == 0 ) {
         nosource |= RCVI; 
         *tokv[k] = '\0';
      }
      else if ( strcmp("norcvt", tbuf) == 0 ) {
         nosource |= RCVT; 
         *tokv[k] = '\0';
      }
      else if ( strcmp("nosndp", tbuf) == 0 ) {
         nosource |= SNDP; 
         *tokv[k] = '\0';
      }
      else if ( strcmp("nosnda", tbuf) == 0 ) {
         nosource |= SNDA; 
         *tokv[k] = '\0';
      }
      else if ( strcmp("norcva", tbuf) == 0 ) {
         nosource |= RCVA; 
         *tokv[k] = '\0';
      }
      else if ( strcmp("noxmtf", tbuf) == 0 ) {
         nosource |= XMTF; 
         *tokv[k] = '\0';
      }
      else if ( strcmp("trigemu", tbuf) == 0 ) {
         launcherp->trigemuflag = 1;
	 launcherp->bmaptrigemu = bitmap;
	 *tokv[k] = '\0';
      }

      else if ( strcmp("swmin", tbuf) == 0 ) {
         /* For the command itself */
         launcherp->vflags |= SEC_MIN;
         *tokv[k] = '\0';
      }
      else if ( strcmp("swmax", tbuf) == 0 ) {
         /* For the command itself */
         launcherp->vflags |= SEC_MAX;
         *tokv[k] = '\0';
      }
      else if ( strcmp("swhome", tbuf) == 0 ) {
         launcherp->vflags |= SEC_HOME;
         *tokv[k] = '\0';
      }
      else if ( strcmp("swaway", tbuf) == 0 ) {
         launcherp->vflags |= SEC_AWAY;
         *tokv[k] = '\0';
      }
      else if ( strcmp("lobat", tbuf) == 0 ) {
         launcherp->vflags |= SEC_LOBAT;
         *tokv[k] = '\0';
      }
      else if ( strcmp("notlobat", tbuf) == 0 ) {
         launcherp->notvflags |= SEC_LOBAT;
         *tokv[k] = '\0';
      }
      else if ( strcmp("main", tbuf) == 0 ) {
         /* For the command itself */
         launcherp->vflags |= SEC_MAIN;
         *tokv[k] = '\0';
      }
      else if ( strcmp("aux", tbuf) == 0 ) {
         /* For the command itself */
         launcherp->vflags |= SEC_AUX;
         *tokv[k] = '\0';
      }
      else if ( strcmp("rollover", tbuf) == 0 ) {
         /* RFXMeter or Oregon Rain sensor count rollover */
         launcherp->vflags |= RFX_ROLLOVER;
         *tokv[k] = '\0';
      }

#ifdef HASDMX
      else if ( strcmp("heat", tbuf) == 0 || strcmp("notcool", tbuf) == 0 ) {
         /* Digimax */
         launcherp->vflags |= DMX_HEAT;
         *tokv[k] = '\0';
      }
      else if ( strcmp("cool", tbuf) == 0 || strcmp("notheat", tbuf) == 0 ) {
         /* Digimax */
         launcherp->notvflags |= DMX_HEAT;
         *tokv[k] = '\0';
      }
      else if ( strcmp("set", tbuf) == 0 ) {
         /* Digimax */
         launcherp->vflags |= DMX_SET;
         *tokv[k] = '\0';
      }
      else if ( strcmp("notset", tbuf) == 0 ) {
         /* Digimax */
         launcherp->notvflags |= DMX_SET;
         *tokv[k] = '\0';
      }
      else if ( strcmp("init", tbuf) == 0 ) {
         /* Digimax */
         launcherp->vflags |= DMX_INIT;
         *tokv[k] = '\0';
      }
      else if ( strcmp("notinit", tbuf) == 0 ) {
         /* Digimax */
         launcherp->notvflags |= DMX_INIT;
         *tokv[k] = '\0';
      }
      else if ( strcmp("temp", tbuf) == 0 ) {
         /* Digimax */
         launcherp->vflags |= DMX_TEMP;
         *tokv[k] = '\0';
      }
      else if ( strcmp("nottemp", tbuf) == 0 ) {
         /* Digimax */
         launcherp->notvflags |= DMX_TEMP;
         *tokv[k] = '\0';
      }
#endif  /* HASDMX */

#ifdef  HASORE
      else if ( strcmp("tmin", tbuf) == 0 ) {
         /* Oregon Temperature */
         launcherp->vflags |= ORE_TMIN;
         *tokv[k] = '\0';
      }
      else if ( strcmp("tmax", tbuf) == 0 ) {
         /* Oregon Temperature */
         launcherp->vflags |= ORE_TMAX;
         *tokv[k] = '\0';
      }
      else if ( strcmp("rhmin", tbuf) == 0 ) {
         /* Oregon Relative Humidity */
         launcherp->vflags |= ORE_RHMIN;
         *tokv[k] = '\0';
      }
      else if ( strcmp("rhmax", tbuf) == 0 ) {
         /* Oregon Relative Humidity */
         launcherp->vflags |= ORE_RHMAX;
         *tokv[k] = '\0';
      }

      else if ( strcmp("bpmin", tbuf) == 0 ) {
         /* Oregon Barometric Pressure */
         launcherp->vflags |= ORE_BPMIN;
         *tokv[k] = '\0';
      }
      else if ( strcmp("bpmax", tbuf) == 0 ) {
         /* Oregon Barometric Pressure */
         launcherp->vflags |= ORE_BPMAX;
         *tokv[k] = '\0';
      }
#endif   /* HASORE */

   }

   for ( k = 1; k < tokc; k++ ) {
      if ( *tokv[k] == '\0' )
         continue;
      if ( strcmp(tokv[k], "anyfunc") == 0 ) {
         launcherp->afuncmap = ~(0);
         launcherp->sfuncmap = ~(0);
         launcherp->xfuncmap = ~(0);
         launcherp->ofuncmap = ~(0);
         launcherp->kfuncmap = ~(0);
         found_func = 1;
         continue;
      }

      found_func = 0;

      for ( m = 0; m < (int)NTRIGTYPES; m++ ) {
         if ( strcmp(trig_type[m].label, tokv[k]) == 0 ) {
            if ( trig_type[m].flags & AnyFlag ) {
               mask = trig_type[m].flags & ~AnyFlag;
               for ( mm = 0; mm < (int)NTRIGTYPES; mm++ ) {
                  if ( (flag = trig_type[mm].flags) & AnyFlag )
                     continue;
                  launcherp->afuncmap |= (mask & flag) ? (1 << trig_type[mm].signal) : 0;
               }
            }
            else if ( trig_type[m].flags & GenPLC ) {
               launcherp->gfuncmap |= (1 << trig_type[m].signal);
            }
            else if ( trig_type[m].flags & SpecSec ) {
               /* vdata can be sent from the command line or received as a security signal */
               launcherp->afuncmap |= (1 << trig_type[m].signal);
               launcherp->sfuncmap |= (1 << trig_type[m].signal);
            }
            else {
               launcherp->afuncmap |= (1 << trig_type[m].signal);
            }
            found_func = 1;
            break;
         }
      }
      if ( found_func )
         continue;

      for ( m = 0; m < (int)NSECTRIGTYPES; m++ ) {
         if ( strcmp(sec_trig_type[m].label, tokv[k]) == 0 ) {
            if ( sec_trig_type[m].flags & AnyFlag ) {
               mask = sec_trig_type[m].flags & ~AnyFlag;
               for ( mm = 0; mm < (int)NSECTRIGTYPES; mm++ ) {
                  if ( (flag = sec_trig_type[mm].flags) & AnyFlag )
                     continue;
                  launcherp->sfuncmap |= (mask & flag) ? (1 << sec_trig_type[mm].signal) : 0;
               }
            }
            else {
               launcherp->sfuncmap |= (1 << sec_trig_type[m].signal);
            }
            found_func = 1;
            break;
         }
      }
      if ( found_func )
         continue;

      for ( m = 0; m < (int)NORETRIGTYPES; m++ ) {
         if ( strcmp(ore_trig_type[m].label, tokv[k]) == 0 ) {
            if ( ore_trig_type[m].flags & AnyFlag ) {
               mask = ore_trig_type[m].flags & ~AnyFlag;
               for ( mm = 0; mm < (int)NORETRIGTYPES; mm++ ) {
                  if ( (flag = ore_trig_type[mm].flags) & AnyFlag )
                     continue;
                  launcherp->ofuncmap |= (mask & flag) ? (1 << ore_trig_type[mm].signal) : 0;
               }
            }
            else {
               launcherp->ofuncmap |= (1 << ore_trig_type[m].signal);
            }
            found_func = 1;
            break;
         }
      }
      if ( found_func )
         continue;

      for ( m = 0; m < (int)NKAKUTRIGTYPES; m++ ) {
         if ( strcmp(kaku_trig_type[m].label, tokv[k]) == 0 ) {
            if ( kaku_trig_type[m].flags & AnyFlag ) {
               mask = kaku_trig_type[m].flags & ~AnyFlag;
               for ( mm = 0; mm < (int)NKAKUTRIGTYPES; mm++ ) {
                  if ( (flag = kaku_trig_type[mm].flags) & AnyFlag )
                     continue;
                  launcherp->kfuncmap |= (mask & flag) ? (1 << kaku_trig_type[mm].signal) : 0;
               }
            }
            else {
               launcherp->kfuncmap |= (1 << kaku_trig_type[m].signal);
            }
            found_func = 1;
            break;
         }
      }
      if ( found_func )
         continue;

      for ( m = 0; m < (int)NRFXTRIGTYPES; m++ ) {
         if ( strcmp(rfx_trig_type[m].label, tokv[k]) == 0 ) {
            if ( rfx_trig_type[m].flags & AnyFlag ) {
               mask = rfx_trig_type[m].flags & ~AnyFlag;
               for ( mm = 0; mm < (int)NRFXTRIGTYPES; mm++ ) {
                  if ( (flag = rfx_trig_type[mm].flags) & AnyFlag )
                     continue;
                  launcherp->xfuncmap |= (mask & flag) ? (1 << rfx_trig_type[mm].signal) : 0;
               }
            }
            else {
               launcherp->xfuncmap |= (1 << rfx_trig_type[m].signal);
            }
            found_func = 1;
            break;
         }
      }

      if ( !found_func ) {
         sprintf(errmsg, "Invalid launch parameter '%s'", tokv[k]);
         store_error_message(errmsg);
         return -1;
      }   
   }

   if ( launcherp->gfuncmap == 0 && launcherp->afuncmap == 0 &&
        launcherp->xfuncmap == 0 && launcherp->sfuncmap == 0 &&
        launcherp->ofuncmap == 0 && launcherp->kfuncmap == 0 &&
        address == 0 && change == 0 ) {
      store_error_message("No launch function has been specified.");
      return -1;
   }

   if ( address && (launcherp->gfuncmap || launcherp->afuncmap ||
       launcherp->xfuncmap || launcherp->sfuncmap || launcherp->ofuncmap  || launcherp->kfuncmap) ) {
      store_error_message("Keyword 'address' is incompatible with any functions.");
      return -1;
   }
   else if ( address && (change || module) ) {
      store_error_message("Keywords 'changed' and 'module' are invalid in an address launcher.");
      return -1;
   }
   else if ( address ) {
      launcherp->type = L_ADDRESS;
   }
   else {}

   if ( change && signal ) {
      store_error_message("Keywords change and signal are incompatible.");
      return -1;
   }
   if ( module && signal ) {
      store_error_message("Keywords module and signal are incompatible.");
      return -1;
   }
   if ( change ) {
      launcherp->chgtrig = launcherp->bmaptrig;
      module = 1;
      signal = 0;
   }
   else
      launcherp->chgtrig = 0;

   if ( module ) {
      launcherp->module = bitmap;
      launcherp->signal = 0;
   }
   else if ( signal ) {
      launcherp->module = 0;
      launcherp->signal = bitmap;
   }
   else {
      launcherp->module = 0;
      launcherp->signal = 0;
   }

   launcherp->source = source;
   launcherp->nosource = nosource;

   launcherp->oksofar = 1;

   return 0;
}
 

/*---------------------------------------------------------------------+
 | Create individual launchers with argument label from the sets of    |
 | launch conditions in argument tail.  Return 0 if successful, or -1  |
 | if error.                                                           |
 +---------------------------------------------------------------------*/
int create_launchers ( LAUNCHER **launcherpp, char *label, int line_no, char *tail ) 
{

   int           j, index, retcode = 0;  
   int           argc, tokc;
   char          **argv, **tokv;
   char          *sp;
   char          errmsg[128];
   LAUNCHER      *launcherp;

   sp = tail;

   /* Tokenize the line into individual sets of launch conditions */
   tokenize( sp, ";", &argc, &argv );

   if ( argc == 0 ) {
       store_error_message("No launch parameters specified");
       free(argv);
       return -1;
   }

   for ( j = 0; j < argc; j++ ) {
      /* Tokenize each set of launch conditions */
      tokenize(argv[j], " \t", &tokc, &tokv);
      if ( tokc < 1 ) {
         sprintf(errmsg, "No launch parameters specified for %s", tokv[0]);
         store_error_message(errmsg);
         free(tokv);
         free(argv);
         return -1;
      }

      index = launcher_index(launcherpp);

      (*launcherpp)[index].line_no = line_no;
      strncpy2((*launcherpp)[index].label, label, NAME_LEN);

      launcherp = &(*launcherpp)[index];

      if ( strcmp(tokv[0], "-powerfail") == 0 ) 
         retcode = create_powerfail_launcher(launcherp, tokc, tokv);
      else if ( strcmp(tokv[0], "-rfflood") == 0 )
         retcode = create_rfflood_launcher(launcherp, tokc, tokv);
      else if ( strcmp(tokv[0], "-timeout") == 0 )
         retcode = create_timeout_launcher(launcherp, tokc, tokv);
      else if ( strcmp(tokv[0], "-sensorfail") == 0 )
         retcode = create_sensorfail_launcher(launcherp, tokc, tokv);
      else if ( strcmp(tokv[0], "-lockup") == 0 )
         retcode = create_lockup_launcher(launcherp, tokc, tokv);
      else if ( strcmp(tokv[0], "-rfjamming") == 0 )
         retcode = create_rfxjam_launcher(launcherp, tokc, tokv);
      else if ( strcmp(tokv[0], "-exec") == 0 )
         retcode = create_exec_launcher(launcherp, tokc, tokv);
      else if ( strcmp(tokv[0], "-hourly") == 0 )
         retcode = create_hourly_launcher(launcherp, tokc, tokv);
      else {
         retcode = create_normal_launcher(launcherp, tokc, tokv);
      }

      free(tokv);

      if ( retcode != 0 )
         break;
   }

   free(argv);

   return retcode;
}


/*---------------------------------------------------------------------+
 | Add a launcher to the array of LAUNCHER structures for each set     |
 | of launch conditions in argument tail.  Return 0 if successful.     | 
 +---------------------------------------------------------------------*/
int add_launchers ( LAUNCHER **launcherpp, int line_no, char *tail ) 
{

   char          label[51];
   char          errmsg[128];
   char          *sp;

   sp = tail;

   /* The first token is the script label */
   if ( *get_token(label, &sp, " \t", 50) == '\0' || *sp == '\0' ) {
      store_error_message("Too few items on line.");
      return -1;
   }
   if ( (int)strlen(label) > NAME_LEN ) {
      sprintf(errmsg, "Label '%s' too long (max %d characters)", label, NAME_LEN);
      store_error_message(errmsg);
      return -1;
   }

   strtrim(sp);
   if ( *sp == '\0' ) {
      store_error_message("Too few items on line.");
      return -1;
   }

   return create_launchers(launcherpp, label, line_no, sp);
}


/*---------------------------------------------------------------------+
 | Make the appropriate changes/additions/error checks for launchers   |
 | once all the config file information has been stored.               |
 +---------------------------------------------------------------------*/
int finalize_launchers ( void )
{
   unsigned char mode, matched;
   int           j, k, noloc, lnum, errors = 0;
   LAUNCHER      *launcherp;
   SCRIPT        *scriptp;

   mode = configp->launch_mode ;
   launcherp = configp->launcherp;
   scriptp = configp->scriptp;
   noloc = (configp->loc_flag == (LONGITUDE | LATITUDE)) ? 0 : 1;

   if ( !launcherp && !scriptp )
      return 0;
   else if ( launcherp && !scriptp ) {
      fprintf(stderr, "Config File: No SCRIPTs defined for LAUNCHERs");
      return 1;
   }
   else if ( !launcherp && scriptp ) {
      fprintf(stderr, "Config File: No LAUNCHERs defined for SCRIPTs");
      return 1;
   }
 
   errors = 0;

   /* Check for duplicate script labels */
   k = 0;
   while ( scriptp[k].line_no > 0 ) {
      j = k + 1;
      while ( scriptp[j].line_no > 0 ) {
         if ( strcmp(scriptp[j].label, scriptp[k].label) == 0 ) {
            fprintf(stderr, "Config Line %02d: Script label '%s' duplicates line %d\n",
               scriptp[j].line_no, scriptp[j].label, scriptp[k].line_no);
            errors++;
         }
         j++;
      }
      k++;
   }


   /* Match up scripts and launchers */         
   k = 0;
   while ( scriptp[k].line_no > 0 ) {
      j = 0;
      matched = 0;
      lnum = 0;
      scriptp[k].num_launchers = 0;
      while ( launcherp[j].line_no > 0 ) {
         if ( strcmp(launcherp[j].label, scriptp[k].label) == 0 ) {
            matched = 1;
            launcherp[j].scriptnum = k;
            launcherp[j].launchernum = lnum++;
            scriptp[k].num_launchers += 1;
         }
         j++;
      }
      if ( !matched ) {
         fprintf(stderr, "Config Line %02d: No LAUNCHER for SCRIPT with label '%s'\n", 
                 scriptp[k].line_no, scriptp[k].label);
         errors++;
      }
      k++;
   }

   j = 0;
   while ( launcherp[j].line_no > 0 ) {
      /* Skip known bad launchers */
      if ( !(launcherp[j].oksofar) ) {
         j++;
         continue;
      }

      if ( launcherp[j].scriptnum < 0 ) {
         fprintf(stderr, "Config Line %02d: No SCRIPT with label '%s' found\n",
                    launcherp[j].line_no, launcherp[j].label);
         errors++;
      }
         
      if ( noloc &&
           ((launcherp[j].sflags | launcherp[j].notsflags) & (NIGHT_FLAG | DARK_FLAG)) ) {
         fprintf(stderr,
             "Config Line %02d: Flags night/notnight/dark/notdark require LONGITUDE and LATITUDE\n", 
                 launcherp[j].line_no);
         errors++;
      }

      if ( launcherp[j].scanmode == 0 ) {
         launcherp[j].scanmode = configp->scanmode;
      }

      if ( configp->inactive_handling == NEW && launcherp[j].type == L_SENSORFAIL ) {
         fprintf(stderr, "Config Line %02d: Sensorfail script ignored.", launcherp[j].line_no);
      }      

      if ( launcherp[j].type != L_NORMAL && launcherp[j].type != L_ADDRESS ) {
         j++;
         continue;
      }

      /* Consolidate signal sources */
      if ( launcherp[j].source & LSNONE ) 
         launcherp[j].source &= ~(launcherp[j].nosource | LSNONE);
      else
         launcherp[j].source =
           (configp->launch_source | launcherp[j].source) & ~launcherp[j].nosource;

      if ( !launcherp[j].source ) {
         fprintf(stderr,
            "Config Line %02d: No sources (e.g., RCVI) for LAUNCHER with label '%s'\n",
                    launcherp[j].line_no, launcherp[j].label);
         errors++;
      }
         
      if ( launcherp[j].signal )
         launcherp[j].signal = launcherp[j].bmaptrig;
      else if ( launcherp[j].module )
         launcherp[j].signal = 0;
      else if ( mode == TMODE_SIGNAL ) 
         launcherp[j].signal = launcherp[j].bmaptrig;
      else
         launcherp[j].signal = 0;


      /* Fix up 'changed' function if no other function specified */
      if ( launcherp[j].chgtrig != 0 ) {
         if ( (launcherp[j].afuncmap | launcherp[j].gfuncmap | launcherp[j].sfuncmap |
               launcherp[j].xfuncmap | launcherp[j].ofuncmap | launcherp[j].kfuncmap) == 0 ) {
#if 0
         if ( launcherp[j].afuncmap == 0 && launcherp[j].gfuncmap == 0 &&
              launcherp[j].xfuncmap == 0 && launcherp[j].ofuncmap == 0 &&
              launcherp[j].sfuncmap == 0 && launcherp[j].kfuncmap == 0 ) {
#endif
            launcherp[j].afuncmap = ~(0);
            launcherp[j].gfuncmap = ~(0);
            launcherp[j].sfuncmap = ~(0);
            launcherp[j].xfuncmap = ~(0);
            launcherp[j].ofuncmap = ~(0);
            launcherp[j].kfuncmap = ~(0);
         }
      }  

      j++;
   }

   return errors;
}


/*---------------------------------------------------------------------+
 | Add a script to the array of SCRIPT structures and return           |
 | the index of the new member.                                        |
 +---------------------------------------------------------------------*/
int add_script ( SCRIPT **scriptpp, LAUNCHER **launcherpp, int line_no, char *tail )
{
   static int size, maxsize;
   static int sizscript = sizeof(SCRIPT);
   int        blksize = 5;
   int        j, index;
   char       *sp, *dup;
   unsigned char opts;
   char       token[51];
   char       errmsg[128];
   char       *separator = "::";

   /* Allocate memory for script structure */

   /* Allocate initial block if not already done */
   if ( *scriptpp == NULL ) {
      *scriptpp = calloc(blksize, sizscript );
      if ( *scriptpp == NULL ) {
         (void) fprintf(stderr, "Unable to allocate memory for Script.\n");
         exit(1);
      }
      maxsize = blksize;
      size = 0;
      for ( j = 0; j < maxsize; j++ ) {
         (*scriptpp)[j].line_no = -1 ;
         (*scriptpp)[j].label[0] = '\0';
         (*scriptpp)[j].script_option = 0;
         (*scriptpp)[j].cmdline = NULL;
      }
   }

   /* Check to see if there's an available location          */
   /* If not, increase the size of the memory allocation.    */
   /* (Always leave room for a final termination indicator.) */
   if ( size == (maxsize - 1) ) {
      maxsize += blksize ;
      *scriptpp = realloc(*scriptpp, maxsize * sizscript );
      if ( *scriptpp == NULL ) {
         (void) fprintf(stderr, "Unable to increase size of Script list.\n");
         exit(1);
      }

      /* Initialize the new memory allocation */
      for ( j = size; j < maxsize; j++ ) {
         (*scriptpp)[j].line_no = -1 ;
         (*scriptpp)[j].label[0] = '\0';
         (*scriptpp)[j].script_option = 0;
         (*scriptpp)[j].cmdline = NULL;
      }
   }

   index = size;
   size += 1;

   /* Now add the new script information */

   sp = tail;

   (*scriptpp)[index].line_no = line_no;

   /* Locate substring separating the launcher from the command line */
   if ( (sp = strstr(tail, separator)) == NULL ) {
      sprintf(errmsg, "Invalid SCRIPT format - no '%s' separator\n", separator);
      store_error_message(errmsg);
      return -1;
   }

   *sp = '\0';
    sp += strlen(separator);

   strtrim(sp);
   if ( *sp == '\0' ) {
      store_error_message("Missing script command line\n");
      return -1;
   }

#if 0
   while ( *sp == '-' ) {   
      /* Look for -xtend option */
      if ( strncmp(sp, "-x", 2) == 0 ) {
         (*scriptpp)[index].script_option |= SCRIPT_XTEND;
         /* Bypass the token */
         get_token(token, &sp, " \t", 50);
         strtrim(sp);
         continue;
      }

      /* Look for -noenv option */
      if ( strncmp(sp, "-n", 2) == 0 ) {
         (*scriptpp)[index].script_option |= SCRIPT_NOENV;
         /* Bypass the token */
         get_token(token, &sp, " \t", 50);
         strtrim(sp);
         continue;
      }

      /* Look for -rawlevel option */
      if ( strncmp(sp, "-r", 2) == 0 ) {
         (*scriptpp)[index].script_option |= SCRIPT_RAWLEVEL;
         /* Bypass the token */
         get_token(token, &sp, " \t", 50);
         strtrim(sp);
         continue;
      }

      /* Look for -qquiet or -quiet option */
      if ( strncmp(sp, "-qq", 3) == 0 ) {
         (*scriptpp)[index].script_option |= SCRIPT_QQUIET;
         /* Bypass the token */
         get_token(token, &sp, " \t", 50);
         strtrim(sp);
         continue;
      }

      if ( strncmp(sp, "-q", 2) == 0 ) {
         (*scriptpp)[index].script_option |= SCRIPT_QUIET;
         /* Bypass the token */
         get_token(token, &sp, " \t", 50);
         strtrim(sp);
         continue;
      }

      get_token(token, &sp, " \t", 50);
      strtrim(sp);
      sprintf(errmsg, "Invalid script option '%s'\n", token);
      store_error_message(errmsg);
      return -1;
   }

   opts = (*scriptpp)[index].script_option;
   if ( (opts & SCRIPT_NOENV && opts & (SCRIPT_XTEND | SCRIPT_RAWLEVEL)) ||
        (opts & SCRIPT_QUIET && opts & SCRIPT_QQUIET)  ||
        (opts & SCRIPT_XTEND && opts & SCRIPT_RAWLEVEL)  ) {
      store_error_message("Invalid combination of script options\n");
      return -1;
   }
#endif

   opts = 0;
   while ( *sp == '-' ) {   
      if ( strncmp(sp, "-qq", 3) == 0 ) {
         opts |= SCRIPT_QQUIET;
      }
      else if ( strncmp(sp, "-q", 2) == 0 ) {
         opts |= SCRIPT_QUIET;
      }
      else if ( strncmp(sp, "-r", 2) == 0 ) {
         opts |= SCRIPT_RAWLEVEL;
      }
      else if ( strncmp(sp, "-n", 2) == 0 ) {
         opts |= SCRIPT_NOENV;
      }
      else if ( strncmp(sp, "-x", 2) == 0 ) {
         opts |= SCRIPT_XTEND;
      }
      else {
         get_token(token, &sp, " \t", 50);
         sprintf(errmsg, "Invalid script option '%s'\n", token);
         store_error_message(errmsg);
         return -1;
      }
      /* Bypass the token */
      get_token(token, &sp, " \t", 50);
      strtrim(sp);
   }

   if ( (opts & SCRIPT_NOENV && opts & (SCRIPT_XTEND | SCRIPT_RAWLEVEL)) ||
        (opts & SCRIPT_QUIET && opts & SCRIPT_QQUIET)  ||
        (opts & SCRIPT_XTEND && opts & SCRIPT_RAWLEVEL)  ) {
      store_error_message("Invalid combination of script options\n");
      return -1;
   }
   (*scriptpp)[index].script_option = opts;
      
   if ( *sp == '\0' ) {
      store_error_message("Missing script command line\n");
      return -1;
   }

   if ( (dup = strdup(sp)) == NULL ) {
      fprintf(stderr, "Unable to allocate memory for script command line.\n");
      exit(1);
   }
   (*scriptpp)[index].cmdline = dup;

   sp = tail;
   
   /* Does script have a label? */
   get_token(token, &sp, " \t", 50);
   if ( strcmp(token, "-l") == 0 ) {
      /* Yes */
      get_token(token, &sp, " \t", 50);
      strncpy2((*scriptpp)[index].label, token, NAME_LEN);
   }
   else {
      /* No - create default label */
      sprintf((*scriptpp)[index].label, "Script_%02d", line_no);
      sp = tail;
   }

   strtrim(sp);

   /* Add launchers if specified here, otherwise assume user will */
   /* add them as a separate config item.                         */
   if ( *sp != '\0' ) {
      if ( create_launchers(launcherpp, (*scriptpp)[index].label, line_no, sp) < 0 )
         return -1;
   }

   return index;
}
      
/*---------------------------------------------------------------------+ 
 | Load the x10image file and pass back its checksum in the argument.  |
 | Return 1 if OK, 0 otherwise.                                        |
 +---------------------------------------------------------------------*/
int load_image ( int *chksump )
{

   FILE *fd;

   if ( !(fd = fopen(pathspec(IMAGE_FILE), "r")) ) {
      return 0;
   }

   if ( fread((void *)image, 1, PROMSIZE + 2, fd) != PROMSIZE ) {
      fclose(fd);
      return 0;
   }
   fclose(fd);

   if ( (*chksump = image_chksum(image)) == -1 )
      return 0;

   return 1;
}


/*---------------------------------------------------------------------+ 
 | Load the x10image file and compare it's checksum with the argument. |
 | Return 1 if OK, 0 otherwise.                                        |
 +---------------------------------------------------------------------*/
int loadcheck_image ( int chksum )
{

   FILE *fd;
   int  sum;

   if ( !(fd = fopen(pathspec(IMAGE_FILE), "r")) ) {
      return 0;
   }

   if ( fread((void *)image, 1, PROMSIZE + 2, fd) != PROMSIZE ) {
      fclose(fd);
      return 0;
   }
   fclose(fd);

   sum = image_chksum(image);
   if ( sum != chksum || sum == -1 )
      return 0;

   return 1;
}

/*---------------------------------------------------------------------+
 | Return a pointer to the (static) image.                             |
 +---------------------------------------------------------------------*/
unsigned char *image_ptr ( void )
{
   return image;
}

/*---------------------------------------------------------------------+ 
 | Scan the cm11a memory image to locate the trigger having argument   |
 | tag which resulted in execution of the macro at macaddr.  Pass back |
 | the housecode|unit and housecode|function bytes through the         |
 | argument list.  Return the tag (1-6) of the matching trigger, or 0  |
 | if no match or if the tag is 7 (indicating anything more than 6).   |
 +---------------------------------------------------------------------*/
int find_trigger ( unsigned char *image, unsigned int macaddr,
    unsigned char tag, unsigned char *hcu, unsigned char *hcf,
    unsigned char *fwver )
{
   unsigned char func, count, *sp;
   unsigned int  addr;

   if ( tag > 6 )
      return 0;

   count = 0;

   /* Start of trigger table */
   addr = ((image[0] << 8) | image[1]) & 0x3ff;
   sp = image + addr;

   /* Future:                                          */
   /* Heyu stores CM11A firmware version in a byte     */
   /* between the timer table terminator 0xff and the  */
   /* start of the trigger table.                      */
   *fwver = *(sp - 1); 

   while ( sp < (image + PROMSIZE - 2) ) {
      if ( *sp == 0xff && *(sp+1) == 0xff )
         break;
      addr = (((*(sp+1)) << 8) | *(sp+2)) & 0x7ff;
      if ( addr == macaddr && ++count == tag ) {
         *hcu = *sp;
         func = (*(sp+1) & 0x80 ) ? 2 : 3;
         *hcf = (*hcu & 0xf0u) | func;
         return count;
      }
      sp += 3;
   }

   return 0;
}

/*---------------------------------------------------------------------+
 | Display the trigger with argument tag which executed the macro at   |
 | macaddr.  Also update the system state and launch a script if so    |
 | programmed.                                                         | 
 +---------------------------------------------------------------------*/
void display_trigger( char *statstr, unsigned int macaddr, unsigned char tag )
{
   unsigned char hcu, hcf, fwver, buf[4];
   int           launchp;
   int           count;

   count = find_trigger(image, macaddr, tag, &hcu, &hcf, &fwver);

   /* Just return if macro delay > 0 */
   if ( image[macaddr] > 0 )
      return;

   if ( count > 0 ) {
      buf[0] = 0x04;
      buf[1] = hcu;
      fprintf(fdsout, "%s rcvt %s\n", statstr, translate_sent(buf, 2, &launchp));
      buf[0] = 0x06;
      buf[1] = hcf;
      fprintf(fdsout, "%s rcvt %s\n", statstr, translate_sent(buf, 2, &launchp));
      if ( launchp >= 0 )
         launch_scripts(&launchp);
   }
   else {
      fprintf(fdsout, "%s rcvt Undetermined trigger\n", statstr);
   }

   return;
}

/*---------------------------------------------------------------------+
 |  Perform multiple functions when a macro is executed:               |
 |     Display individual addresses/functions in monitor               |
 |     Update x10status for each address/function                      |
 |     If trigger conditions are met, launch the user's script.        |
 |                                                                     |
 |  Argument 'type' is TRIGGER_EXEC or TIMER_EXEC                      |
 +---------------------------------------------------------------------*/
int expand_macro( char *statstr, unsigned int macaddr, int type )
{
   int           nelem, j;
   unsigned char hcode, ucode, level, len;
   unsigned int  bitmap, mask;
   unsigned char buf[8];
   unsigned char *elemp;
   int           launchp;
   char          *pfix;

   static unsigned char cmdlen[16] = {
      3,3,3,3,4,4,3,6,3,3,3,3,3,3,3,3
   };

   if ( type == TRIGGER_EXEC ) 
      pfix = "sndt";
   else
      pfix = "sndm"; 

   nelem = image[macaddr + 1];
   elemp = image + macaddr + 2;

   for ( j = 0; j < nelem; j++ ) {
      hcode = (elemp[0] & 0xf0u) >> 4;
      len = cmdlen[elemp[0] & 0x0f];

      bitmap = (elemp[1] << 8) | elemp[2];

      /* Display addresses */
      mask = 1;
      for ( ucode = 0; ucode < 16; ucode++ ) {
         if ( bitmap & mask ) {
            buf[0] = 0x04;
            buf[1] = (hcode << 4) | ucode ;
            fprintf(fdsout, "%s %s %s\n", statstr, pfix, translate_sent(buf, 2, &launchp));
         }
         mask = mask << 1;
      }

      if ( len == 3 ) {
         /* Basic command */
         buf[0] = 0x06;
         buf[1] = elemp[0];
         fprintf(fdsout, "%s %s %s\n", statstr, pfix, translate_sent(buf, 2, &launchp));
         if ( launchp >= 0 )
            launch_scripts(&launchp);
      }
      else if ( len == 4 ) {
         /* Dim or Bright command */
         level = elemp[3] & 0x1f;
         if ( elemp[3] & 0x80 ) {
            /* Brighten bit is set */
            buf[0] = (22 << 3) | 0x06;
            buf[1] = (hcode << 4) | 5;
            fprintf(fdsout, "%s %s %s\n", statstr, pfix, translate_sent(buf, 2, &launchp));
            if ( launchp >= 0 )
               launch_scripts(&launchp);
         }
         buf[0] = (level << 3) | 0x06;
         buf[1] = elemp[0];
         fprintf(fdsout, "%s %s %s\n", statstr, pfix, translate_sent(buf, 2, &launchp));
         if ( launchp >= 0 )
            launch_scripts(&launchp);
      }
      else {
         /* Extended command */
         buf[0] = 0x07;
         buf[1] = elemp[0];
         buf[2] = elemp[3];
         buf[3] = elemp[4];
         buf[4] = elemp[5];
         fprintf(fdsout, "%s %s %s\n", statstr, pfix, translate_sent(buf, 5, &launchp));
         if ( launchp >= 0 )
            launch_scripts(&launchp);
      }
      elemp += len;
   }
   return 1;
}

/*---------------------------------------------------------------------+
 | Write a lock file for the state engine.  Return 1 if one already    |
 | exists or can't be written; 0 otherwise.                            |
 +---------------------------------------------------------------------*/
int lock_state_engine ( PID_T pid )
{
   FILE *fdlock;
   int  acc;

   if ( (acc = access(enginelockfile, F_OK)) == 0 ) {
      /* File exists - assume state engine is running */
      printf("Heyu state engine is already running.\n");
      return 1;
   }

   if ( (fdlock = fopen(enginelockfile, "w")) == NULL ) {
      fprintf(stderr, "Unable to lock heyu state engine.");
      return 1;
   }

   fprintf(fdlock, "%ld\n", (long)pid);
   fclose(fdlock);

   return 0;
}

/*---------------------------------------------------------------------+
 | Unlink the lock file for the state engine.                          |
 +---------------------------------------------------------------------*/
int unlock_state_engine ( void )
{
   int retcode;

   retcode = unlink(enginelockfile);

   if ( retcode == 0 || errno == ENOENT )
      return 0;

   return retcode;
}

/*---------------------------------------------------------------------+
 | Return 1 if a lock file exists for the state engine, 0 otherwise.   |
 +---------------------------------------------------------------------*/
int checklock_state_engine ( void )
{
   int  acc;

   if ( (acc = access(enginelockfile, F_OK)) == 0 ) {
      return 1;
   }
   return 0;
}

/*---------------------------------------------------------------------+
 | Tell the state engine daemon to shut itself down and wait for it to |
 | unlink the lock file.  This will fail if called by heyu on a        |
 | different channel (serial_port) since the spool file is different.  |
 | Return 0 if successful or non-zero otherwise.                       |
 +---------------------------------------------------------------------*/
int stop_state_engine ( void )
{
   int  j, k, retcode = 1;

   if ( (retcode = checklock_state_engine()) != 0 ) {
      for ( j = 0; j < 3; j++ ) {
         send_x10state_command(ST_EXIT, 0);
         for ( k = 0; k < 10; k++ ) {
            millisleep(100);
            if ( (retcode = checklock_state_engine()) == 0 )
               return 0;
         }
      }
   }
   
   /* Orphan lockfile? */
   retcode = unlock_state_engine();   

   return retcode;
}


#ifdef EXEC_POSIX

/*---------------------------------------------------------------------+
 | Start the state engine process - POSIX style.                       |
 +---------------------------------------------------------------------*/
int c_start_engine ( int argc, char *argv[] )
{
   extern int    c_engine(int, char **);

   int           status;
   char          *dup;
   char          tmpfile[PATH_LEN + 1 + 10];
   PID_T         pid, retpid;
   extern        char *argptr;

   if ( check_for_engine() == 0 ) {
      fprintf(stderr,"heyu_engine is running - use 'heyu restart' to reconfigure\n");
      return 1;
   }

   retpid = fork();

   if ( retpid == (PID_T)(-1) ) {
      fprintf(stderr, "Unable to fork() for starting state engine.\n");
      fflush(stderr);
      return 1;
   }

   if ( retpid == (PID_T)0 ) {
      /* In child process */
      if ( (pid = fork()) > (PID_T)0 ) {
         /* Child dies; grandchild inherited by init */
         _exit(0);
      }
      else if ( pid < (PID_T)0 ) {
         fprintf(stderr, "Failed to double fork() for state engine.\n");
         fflush(stderr);
         _exit(1);
      }
      else {
         /* Put config path into environment for any scripts calling heyu */
         sprintf(tmpfile, "X10CONFIG=%s", pathspec(heyu_config));
         if ( (dup = strdup(tmpfile)) == NULL || putenv(dup) == -1 ) {
            fprintf(stderr, "Memory allocation error in c_start_engine()\n");
            exit(1);
         }

         strcpy(argptr, "heyu_engine");

         close(0);

         pid = setsid();
         if ( pid < (PID_T)0 ) {
            fprintf(stderr, "Unable to setsid() for state engine.\n");
            exit(1);
         }

         i_am_state = 1;
         i_am_relay = 0;
         heyu_parent = D_CMDLINE;
         if ( read_x10state_file() != 0 )
            x10state_init_all();
         lock_state_engine(pid);
         c_engine(argc, argv);
         return 0;
      }
   }

   /* Wait for child process to die */
   while ( waitpid(retpid, &status, 0) < (PID_T)0 && errno == EINTR )
      ;
   if (WEXITSTATUS(status) != 0 ) {
     fprintf(stderr, "Unable to double fork() for starting engine.\n");
     return 1;
   }

   return 0;
}      

#else      

/*---------------------------------------------------------------------+
 | Start the state engine process - non-POSIX                          |
 +---------------------------------------------------------------------*/
int c_start_engine ( int argc, char *argv[] )
{
   extern int    c_engine(int, char **);

   char          *dup;
   char          tmpfile[PATH_LEN + 1 + 10];
   PID_T         pid, retpid;
   extern        char *argptr;


   if ( check_for_engine() == 0 ) {
      fprintf(stderr,"heyu_engine is running - use 'heyu restart' to reconfigure\n");
      return 1;
   }

   signal(SIGCHLD, SIG_IGN);

   retpid = fork();

   if ( retpid == (PID_T)(-1) ) {
      fprintf(stderr, "Unable to fork() for starting state engine.\n");
      return 1;
   }

   if ( retpid == (PID_T)0 ) {
      /* Put config path into environment for any scripts calling heyu */
      sprintf(tmpfile, "X10CONFIG=%s", pathspec(heyu_config));
      if ( (dup = strdup(tmpfile)) == NULL || putenv(dup) == -1 ) {
         fprintf(stderr, "Memory allocation error in c_start_engine()\n");
         exit(1);
      } 
     
      strcpy(argptr, "heyu_engine");

      close(0);
      close(1);
      close(2);

      pid = setsid();
      if ( pid < (PID_T)0 ) {
         fprintf(stderr, "Failure in setsid() for starting state engine.\n");
         exit(1);
      }
      i_am_state = 1;
      i_am_relay = 0;
      heyu_parent = D_CMDLINE;
      if ( read_x10state_file() != 0 )
         x10state_init_all();
      lock_state_engine(pid);
      c_engine(argc, argv);
      return 0;
   }

   return 0;
}      
      
#endif  /* End of ifdef EXEC_POSIX / else block */   

/*----------------------------------------------------------------------------+
 | Instruct state engine to update the flags.  Argument mode is SETFLAG or    |
 | CLRFLAG                                                                    |
 +----------------------------------------------------------------------------*/
int send_long_flag_update ( unsigned char bank, unsigned long flagmap, unsigned char mode )
{
   extern int sptty;

   int ignoret;

   static unsigned char template[12] = {
    0xff,0xff,0xff,8,ST_COMMAND,ST_FLAGS32,0,0, 0,0,0,0};

   template[6] = mode;
   template[7] = bank;
   template[8] = (flagmap & 0xff000000) >> 24;
   template[9] = (flagmap & 0x00ff0000) >> 16;
   template[10] = (flagmap & 0x0000ff00) >> 8;
   template[11] = flagmap & 0x000000ff;

   ignoret = write(sptty, template, sizeof(template));

   return 0;
}

/*----------------------------------------------------------------------------+
 | Instruct state engine to update the flags.  Argument mode is SETFLAG or    |
 | CLRFLAG                                                                    |
 +----------------------------------------------------------------------------*/
int send_flag_update ( unsigned int flagmap, unsigned char mode )
{
   extern int sptty;

   int ignoret;

   static unsigned char template[9] = {
    0xff,0xff,0xff,5,ST_COMMAND,ST_FLAGS,0,0,0};

   template[6] = mode;
   template[7] = (flagmap & 0xff00) >> 8;
   template[8] = flagmap & 0xff;

   ignoret = write(sptty, template, sizeof(template));

   return 0;
}


/*----------------------------------------------------------------------------+
 | Instruct state engine to clear the status flags for hcode and bitmap       |
 +----------------------------------------------------------------------------*/
int send_clear_statusflags ( unsigned char hcode, unsigned int bitmap )
{
   extern int sptty;

   int ignoret;
  
   static unsigned char template[9] = {
     0xff,0xff,0xff,5,ST_COMMAND,ST_CLRSTATUS,0,0,0};

   template[6] = hcode;
   template[7] = (bitmap & 0xff00) >> 8;
   template[8] = bitmap & 0xff;

   ignoret = write(sptty, template, sizeof(template));
   return 0;
}


void clear_statusflags ( unsigned char hcode, unsigned int bitmap )
{
//   x10state[hcode].statusflags &= ~bitmap;
   x10state[hcode].state[SpendState] &= ~bitmap;

   return;
}

/*---------------------------------------------------------------------+
 | Return states of Night and/or Dark flags.                           |
 |  'nightstate' or 'darkstate' return Boolean.                        |
 |  'sunstate' returns bitmap where night = 1, dark = 2                |
 +---------------------------------------------------------------------*/
int c_sunstate ( int argc, char *argv[] )
{
   unsigned int  flagmap;

   if ( check_for_engine() != 0 ) {
      fprintf(stderr, "State engine is not running.\n");
      return 1;
   }

   if ( argc > 2 ) {
      fprintf(stderr, "%s: Too many parameters.\n", argv[1]);
      return 1;
   }

   if ( read_x10state_file() != 0 ) {
      fprintf(stderr, "Unable to read state file.\n");
      return 1;
   }

   if ( !x10global.dawndusk_enable ) {
      fprintf(stderr, "LATITUDE and/or LONGITUDE have not been defined.\n");
      return 1;
   }

   if ( strcmp(argv[1], "nightstate") == 0 ) {
      printf("%d\n", ((x10global.sflags & NIGHT_FLAG) ? 1 : 0));
   }
   else if ( strcmp(argv[1], "darkstate") == 0 ) {
      printf("%d\n", ((x10global.sflags & DARK_FLAG) ? 1 : 0));
   }
   else if ( strcmp(argv[1], "sunstate") == 0 ) {
      flagmap = (x10global.sflags & NIGHT_FLAG) ? 1 : 0;
      flagmap |= (x10global.sflags & DARK_FLAG) ? 2 : 0;
      printf("%d\n", flagmap);
   }

   return 0;
}

/*---------------------------------------------------------------------+
 | Read counter                                                        |
 +---------------------------------------------------------------------*/
int c_counter ( int argc, char *argv[] )
{
   int           index;
   char          *sp;

   if ( check_for_engine() != 0 ) {
      fprintf(stderr, "State engine is not running.\n");
      return 1;
   }

   if ( argc < 3 ) {
      fprintf(stderr, "%s: Too few parameters.\n", argv[1]);
      return 1;
   }
   else if ( argc > 3 ) {
      fprintf(stderr, "%s: Too many parameters.\n", argv[1]);
      return 1;
   }

   if ( strcmp(argv[1], "counter") == 0 )  {
      index = (int)strtol(argv[2], &sp, 10);
      if ( !strchr(" /t/n", *sp) || index < 1 || index > 32 * NUM_COUNTER_BANKS ) {
         fprintf(stderr, "Invalid counter number.\n");
         return 1;
      }
      if ( read_x10state_file() != 0 ) {
         fprintf(stderr, "Unable to read state file.\n");
         return 1;
      }
      printf("%d\n", x10global.counter[index - 1]);
   }
      
   return 0;
}

/*---------------------------------------------------------------------+
 | Flag reading and setting commands.                                  |
 +---------------------------------------------------------------------*/
int c_flagstate ( int argc, char *argv[] )
{
   unsigned long flagmap;
   int           flag;
   char          *sp;

   if ( check_for_engine() != 0 ) {
      fprintf(stderr, "State engine is not running.\n");
      return 1;
   }

   if ( argc < 3 ) {
      fprintf(stderr, "%s: Too few parameters.\n", argv[1]);
      return 1;
   }
   else if ( argc > 3 ) {
      fprintf(stderr, "%s: Too many parameters.\n", argv[1]);
      return 1;
   }

   if ( strcmp(argv[1], "flagstate") == 0 )  {
      flag = (int)strtol(argv[2], &sp, 10);
      if ( !strchr(" /t/n", *sp) || flag < 1 || flag > 32 * NUM_FLAG_BANKS ) {
         fprintf(stderr, "Invalid flag number.\n");
         return 1;
      }
      if ( read_x10state_file() != 0 ) {
         fprintf(stderr, "Unable to read state file.\n");
         return 1;
      }
      flagmap = (1 << ((flag - 1) % 32)) & x10global.flags[(flag - 1) / 32]; /*###*/
      printf("%d\n", ((flagmap) ? 1 : 0));

      return 0;
   }
      
   clear_error_message();

   flagmap = flags2longmap(argv[2]);

   if ( *error_message() ) {
      /* Display error from error handler */
      fprintf(stderr, "%s: %s\n", argv[1], error_message());
      return 1;
   }

   if ( strncmp(argv[1], "setflag", 7)   == 0 )
      send_long_flag_update(0, flagmap, SETFLAG);
   else if ( strncmp(argv[1], "clrflag", 7) == 0 )
      send_long_flag_update(0, flagmap, CLRFLAG);

   return 0;
}

/*---------------------------------------------------------------------+
 | Status flag query.  (A status flag for Hu is set when a status_req  |
 | is sent or received and cleared when a status_on or status_off is   |
 | sent or received.                                                   |
 +---------------------------------------------------------------------*/
int c_status_state ( int argc, char *argv[] )
{
   unsigned int  aflags, bitmap, flagstate;
   char          hc;

   if ( check_for_engine() != 0 ) {
      fprintf(stderr, "%s: State engine is not running.\n", argv[1]);
      return 1;
   }

   if ( argc < 3 ) {
      fprintf(stderr, "%s: Too few parameters.\n", argv[1]);
      return 1;
   }
   else if ( argc > 3 ) {
      fprintf(stderr, "%s: Too many parameters.\n", argv[1]);
      return 1;
   }

   aflags = parse_addr(argv[2], &hc, &bitmap) ;
   if ( !(aflags & A_VALID) || !(aflags & A_HCODE) || (aflags & A_DUMMY) ) {
      fprintf(stderr, "%s: Invalid Housecode|Unit '%s'\n", argv[1], argv[2]);
      return 1;
   }
   if ( aflags & A_MULT ) {
      fprintf(stderr, "%s: Only a single unit code is acceptable\n", argv[1]);
      return 1;
   }

   if ( read_x10state_file() != 0 ) {
      fprintf(stderr, "%s: Unable to read state file.\n", argv[1]);
      return 1;
   }

   if ( aflags & A_MINUS )
      bitmap = 0;
   if ( aflags & A_PLUS && bitmap == 0 )
      bitmap = 1;

   flagstate = x10state[hc2code(hc)].state[SpendState];
//   flagstate = x10state[hc2code(hc)].statusflags;

   if ( bitmap )
      printf("%d\n", ((flagstate & bitmap) ? 1 : 0) );
   else
      printf("%d\n", x10map2linmap(flagstate));
	 
   return 0;
}

/*---------------------------------------------------------------------+
 | Display timestamp for a module address                              |
 +---------------------------------------------------------------------*/
int show_signal_timestamp ( char *address )
{
   unsigned int  aflags, bitmap;
   unsigned char ucode;
   char          hc;
   time_t        timestamp;
   struct tm     *tmp;
   extern char   *heyu_tzname[2];
   extern void   fix_tznames ( void );

   aflags = parse_addr(address, &hc, &bitmap);

   if ( !(aflags & A_VALID) || (aflags & A_MULT) || bitmap == 0 ) {
      fprintf(stderr, "Invalid Hu address parameter.\n");
      return 1;
   }
   ucode = single_bmap_unit(bitmap);

   timestamp = x10state[hc2code(hc)].timestamp[ucode];

   if ( timestamp == 0 ) {
      printf("No signal has been recorded for this address.\n");
      return 0;
   }

   fix_tznames();
   
   tmp = localtime(&timestamp);
   printf("%c%d: %s  %02d:%02d:%02d  %s %02d %d %s\n",
     hc, code2unit(ucode), wday_name[tmp->tm_wday],
     tmp->tm_hour, tmp->tm_min, tmp->tm_sec, month_name[tmp->tm_mon],
     tmp->tm_mday, tmp->tm_year + 1900, heyu_tzname[tmp->tm_isdst]);

   return 0;
}

/*---------------------------------------------------------------------+
 | Initialize a countdown timer with the countdown time in seconds.    |
 +---------------------------------------------------------------------*/
void set_user_timer_countdown ( int timer_nbr, long seconds )
{
   if ( timer_nbr < 0 || timer_nbr > NUM_USER_TIMERS )
      return;

   x10global.timer_count[timer_nbr] = seconds;

   if ( seconds == 0 )
      x10global.tzflags[(timer_nbr - 1) / 32] |= (1UL << ((timer_nbr - 1) % 32));
   else
      x10global.tzflags[(timer_nbr - 1) / 32] &= ~(1UL << ((timer_nbr - 1) % 32));

   return;
}

/*---------------------------------------------------------------------+
 | Reset all user timers 1 through NUM_USER_TIMERS                     |
 +---------------------------------------------------------------------*/
void reset_user_timers ( void )
{
   int j;

   for ( j = 1; j <= NUM_USER_TIMERS; j++ )
      x10global.timer_count[j] = 0;

   for ( j = 0; j < NUM_TIMER_BANKS; j++ )
      x10global.tzflags[j] = ~(0UL);

   return;
}

/*---------------------------------------------------------------------+
 | Display @settimer setting                                           |
 +---------------------------------------------------------------------*/
void display_settimer_setting ( int timer, long timeout )
{
   if ( timeout < 120 )
      fprintf(fdsout, "%s Countdown timer %d set to %ld seconds\n", datstrf(), timer, timeout);
   else 
      fprintf(fdsout, "%s Countdown timer %d set to %ld:%02ld:%02ld\n", datstrf(),
         timer, timeout / 3600L, (timeout % 3600L) / 60L, timeout % 3600L % 60L);
   fflush(fdsout);
   return;
}

   
/*---------------------------------------------------------------------+
 | Translate settimer message and set the timer countdown              |
 +---------------------------------------------------------------------*/
char *translate_settimer_message( unsigned char *buffer )
{
   unsigned long timeout = 0;
   int           j, timer;
   static char   message[64];

   timer = buffer[2] | (buffer[3] << 8);
   for ( j = 0; j < 4; j++ ) 
      timeout |= (unsigned long)(buffer[j + 4] << (8 * j));

   set_user_timer_countdown(timer, (long)timeout);

   if ( timeout < 120 )
      sprintf(message, "Countdown timer %d set to %ld seconds", timer, timeout);
   else 
      sprintf(message, "Countdown timer %d set to %ld:%02ld:%02ld",
         timer, timeout / 3600L, (timeout % 3600L) / 60L, timeout % 3600L % 60L);

   return message;
}   
   

/*---------------------------------------------------------------------+
 | Send settimer message to State Engine.                              |
 +---------------------------------------------------------------------*/
void send_settimer_msg( unsigned short timer, long timeout )
{
   int  j;

   int ignoret;

   static unsigned char template[12] =
     {0xff, 0xff, 0xff, 8, ST_COMMAND, ST_SETTIMER, 0,0, 0,0,0,0};

   template[6] = timer & 0x00ff;
   template[7] = (timer & 0xff00) >> 8;
   for ( j = 0; j < 4; j++ ) {
      template[j + 8] = (unsigned char)(timeout & 0xff);
      timeout >>= 8;
   }
   ignoret = write(sptty, (char *)template, sizeof(template));
   return;
}

/*---------------------------------------------------------------------+
 | Display security flags as bitmap:                                   |
 |   0=disarmed, 1=armed, 2=home, 4=armpending, 8 = tamper             |
 +---------------------------------------------------------------------*/
int c_armedstate( int argc, char *argv[] )
{
   long globsec;

   if ( check_for_engine() != 0 ) {
      fprintf(stderr, "Engine is not running\n");
      return 1;
   }

   fetch_x10state();

   globsec = x10global.sflags & (GLOBSEC_ARMED | GLOBSEC_HOME | GLOBSEC_PENDING | GLOBSEC_TAMPER);

   printf("%ld\n", (globsec >> GLOBSEC_SHIFT));

   return 0;
}

/*---------------------------------------------------------------------+
 | Display security Armed, Disarmed, Pending, Home, Away               |
 +---------------------------------------------------------------------*/
int c_armed_status( int argc, char *argv[] )
{
   if ( check_for_engine() != 0 ) {
      fprintf(stderr, "Engine is not running\n");
      return 1;
   }

   fetch_x10state();

   printf("%s\n", display_armed_status());
   return 0;
}

/*---------------------------------------------------------------------+
 | Create general date or date/time string.                            |
 +---------------------------------------------------------------------*/
void gendate ( char *datestr, time_t timestamp, unsigned char showyear, unsigned char showtime )
{
   struct tm *tmp;
   char   sep;

   tmp = localtime(&timestamp);

   sep = configp->date_separator;
   *datestr = '\0';

   if ( configp->date_format == YMD_ORDER ) {
      /* [yyyy/]mm/dd */
      if ( showyear == YES) {
         sprintf(datestr, "%d%c", tmp->tm_year + 1900, sep);
      }
      sprintf(datestr + strlen(datestr), "%02d%c%02d",
         tmp->tm_mon + 1, sep, tmp->tm_mday);
   }
   else if ( configp->date_format == MDY_ORDER ) {
      /* mm/dd[/yyyy] */
      sprintf(datestr, "%02d%c%02d",
         tmp->tm_mon + 1, sep, tmp->tm_mday);
      if ( showyear == YES) {
         sprintf(datestr + strlen(datestr), "%c%d", sep, tmp->tm_year + 1900);
      }
   }
   else if ( configp->date_format == DMY_ORDER ) {
      /* dd/mm/yyyy */
      sprintf(datestr, "%02d%c%02d", tmp->tm_mday, sep, tmp->tm_mon + 1);
      if ( showyear == YES ) {
         sprintf(datestr + strlen(datestr), "%c%d", sep, tmp->tm_year + 1900);
      }
   }

   if ( showtime == YES )
      sprintf(datestr + strlen(datestr), " %02d:%02d:%02d",
          tmp->tm_hour, tmp->tm_min, tmp->tm_sec);

   return;
}

/*---------------------------------------------------------------------+
 | Date display in seconds from epoch.                                 |
 +---------------------------------------------------------------------*/
void datstrf_epoch ( char *buffer )
{
   struct timeval tvstruct;
   struct timeval *tv = &tvstruct;

   gettimeofday(tv, NULL);

   sprintf(buffer, "%ld.%03ld ",
       (unsigned long)tv->tv_sec, (unsigned long)tv->tv_usec/1000);

   return;
}
 
/*---------------------------------------------------------------------+
 | Date display with optional subdirectory for monitor and log file.   |
 +---------------------------------------------------------------------*/
char *datstrf ( void ) 
{
   extern struct opt_st *optptr; 
   static char buffer[40];

   if ( configp->logdate_unix == YES )
      datstrf_epoch(buffer);
   else {
      gendate(buffer, time(NULL), configp->logdate_year, YES);
      strcat(buffer, " ");
   }

   if ( configp->disp_subdir == YES ) {
      if ( optptr->dispsub[0] != '\0' ) 
         strcat(buffer, optptr->dispsub);
      else
         strcat(buffer, " ");
   }

   return buffer;
}

/*---------------------------------------------------------------------+
 | Launch -timeout scripts for timer number                            |
 +---------------------------------------------------------------------*/
int launch_timeout_scripts ( int timer )
{
   LAUNCHER *launcherp;
   int      j, launchp = -1;

   if ( !i_am_state || !(launcherp = configp->launcherp) )
      return 0;

   j = 0;
   while ( launcherp[j].line_no > 0 ) {
      if ( (launcherp[j].type != L_TIMEOUT) ||
           (launcherp[j].timer != timer) ||
           is_unmatched_flags(&launcherp[j])  ) {
         j++;
         continue;
      }
      launcherp[j].matched = YES;
      launchp = j;
      if ( launcherp[j].scanmode & FM_BREAK )
         break;
      j++;
  }

  return launch_scripts(&launchp);

}
        

/*---------------------------------------------------------------------+
 | Timeout processing - called by countdown function.                  |
 +---------------------------------------------------------------------*/
void timer_timeout ( int timer )
{

   if ( i_am_state || i_am_monitor ) {
      fprintf(fdsout, "%s Timeout, %s\n", datstrf(), countdown_timer[timer].timername);
      fflush(fdsout);
   }
   if ( i_am_state ) {
      write_x10state_file();
      launch_timeout_scripts(timer);
   }

   return;
}

/*---------------------------------------------------------------------+
 | Raise Armed flag - called by countdown function.                    |
 +---------------------------------------------------------------------*/
void arm_delay_complete ( int armtimer ) 
{

   x10global.sflags |= GLOBSEC_ARMED;
   x10global.sflags &= ~GLOBSEC_PENDING;

   if ( i_am_state || i_am_monitor ) {
      fprintf(fdsout, "%s %s\n", datstrf(), display_armed_status());
      fflush(fdsout);
   }
   if ( i_am_state ) {
      write_x10state_file();
      launch_timeout_scripts(armtimer);
   }

   return;
}

/*---------------------------------------------------------------------+
 | Execute sensorfail scripts                                          |
 +---------------------------------------------------------------------*/
int launch_sensorfail_scripts ( int index )
{
   LAUNCHER *launcherp;
   int      j, launchp = -1;

   if ( !i_am_state || !(launcherp = configp->launcherp) )
      return 0;

   j = 0;
   while ( launcherp[j].line_no > 0 ) {
      if ( (launcherp[j].type != L_SENSORFAIL) ||
           is_unmatched_flags(&launcherp[j])  ) {
         j++;
         continue;
      }
      launcherp[j].matched = YES;
      launcherp[j].sensor = index;
      launchp = j;
      if ( launcherp[j].scanmode & FM_BREAK )
         break;
      j++;
  }

  return launch_scripts(&launchp);

}
        
/*---------------------------------------------------------------------+
 | Launch inactive sensor script - called by countdown function.       |
 +---------------------------------------------------------------------*/
void sensor_elapsed_func_old ( int index )
{
   ALIAS *aliasp;

   aliasp = configp->aliasp;

   if ( i_am_state || i_am_monitor ) {
      fprintf(fdsout, "%s Inactive sensor %c%s (%s)\n", datstrf(),
         aliasp[index].housecode, bmap2units(aliasp[index].unitbmap),
         aliasp[index].label);
      fflush(fdsout);
   }
   if ( i_am_state ) {
      write_x10state_file();
      launch_sensorfail_scripts(index);
   }

   return;
}

/*---------------------------------------------------------------------+
 | Launch inactive sensor script - called by countdown function.       |
 +---------------------------------------------------------------------*/
void sensor_elapsed_func ( int index )
{
   unsigned char  hcode, func, ucode, vdata;
   unsigned char  actfunc, genfunc;
   unsigned int   bitmap, trigaddr, mask, active, trigactive;
   unsigned int   changestate, startupstate;
   unsigned int   bmaplaunch, launched;
   unsigned long  afuncmap = 0, gfuncmap = 0, sfuncmap = 0;
   int            j, trig, hide = 0, launchp = -1;
   char           hc;
   LAUNCHER       *launcherp;
   ALIAS          *aliasp;
   unsigned int   source;
   char           *srcname;

   aliasp = configp->aliasp;
   launcherp = configp->launcherp;

   hc = aliasp[index].housecode;
   bitmap = aliasp[index].unitbmap;

   hcode = hc2code(hc);
   ucode = single_bmap_unit(bitmap);

   func = InactiveFunc;
   trig = InactiveTrig;
   vdata = 0;

   actfunc = func;
   genfunc = 0;
   afuncmap = gfuncmap = 0;
   
   active = bitmap;

   startupstate = ~x10state[hcode].state[ValidState] & active;

//   x10state[hcode].dimlevel[ucode] = 0xffu;
   changestate = x10state[hcode].state[ActiveChgState] & active;
   x10state[hcode].state[ChgState] = (x10state[hcode].state[ChgState] & ~active) | changestate;
   changestate = x10state[hcode].state[ChgState];

   hide = (configp->hide_unchanged_inactive == YES && (changestate & active) == 0) ? 1 : 0;


   if ( aliasp[index].optflags & MOPT_PLCSENSOR ) {
       mask = modmask[PlcSensorMask][hcode];
       source = RCVI;
       srcname = "rcvi";
   }
   else {
       mask = modmask[VdataMask][hcode];
       source = RCVA;
       srcname = "rcva";
   }

   sfuncmap = (1 << trig);
   
   trigaddr = bitmap;

   bmaplaunch = 0;
   launched = 0;
   j = 0;
   while ( launcherp && launcherp[j].line_no > 0 ) {
      if ( launcherp[j].type != L_NORMAL ||
           launcherp[j].hcode != hcode ||
           is_unmatched_flags(&launcherp[j])  ) {
         j++;
	 continue;
      }
 
      if ( launcherp[j].sfuncmap & sfuncmap  &&
           launcherp[j].source & source ) {
         trigactive = trigaddr & (mask | launcherp[j].signal);
         if ( (bmaplaunch = (trigactive & launcherp[j].bmaptrig) &
              (changestate | ~launcherp[j].chgtrig)) ||
              (launcherp[j].unitstar && !trigaddr)) {
            launchp = j;
            launcherp[j].matched = YES;
            launcherp[j].actfunc = actfunc;
            launcherp[j].genfunc = genfunc;
	    launcherp[j].xfunc = 0;
	    launcherp[j].level = vdata;
            launcherp[j].bmaplaunch = bmaplaunch;
            launcherp[j].actsource = source;
            launched |= bmaplaunch;
            if ( launcherp[j].scanmode & FM_BREAK )
               break;
         }
      }
      j++;
   }

   x10state[hcode].launched = launched;

   if ( (i_am_state || i_am_monitor) &&  (hide == 0 || launchp >= 0) )  {
      fprintf(fdsout, "%s %4s func %12s : hu %c%s  (%s)\n", datstrf(), 
         srcname, funclabel[func], hc, bmap2units(bitmap), aliasp[index].label);
      fflush(fdsout);
   }

   if ( i_am_state ) {
      write_x10state_file();
   }

   if ( launchp >= 0 )
      launch_scripts(&launchp);

   return;
}

  
/*---------------------------------------------------------------------+
 | Display state of security, RFX, Oregon sensors                      |
 +---------------------------------------------------------------------*/
int show_security_sensors ( void )
{
   ALIAS         *aliasp;
   char          hc;
   int           j, unit, count = 0, maxmod = 0, maxlabel = 0;
   int           is_rfx;
   unsigned char hcode, ucode;
   unsigned int  bitmap, active, inactive;
   time_t        timestamp;
   char          *batstatus;
   char          datestr[40];

   if ( !(aliasp = config.aliasp) )
      return 0;

   if ( fetch_x10state() != 0 )
      return 1;

   /* Get maximum lengths of module name and alias label */
   j = 0;
   while ( aliasp[j].line_no > 0 ) {
      if ( (aliasp[j].optflags & (MOPT_SENSOR | MOPT_RFXSENSOR | MOPT_RFXMETER | MOPT_PLCSENSOR)) != 0 ) {
         count++;
         maxlabel = max(maxlabel, (int)strlen(aliasp[j].label));
         maxmod = max(maxmod, (int)strlen(lookup_module_name(aliasp[j].modtype)) );
      }
      j++;
   }

   if ( count == 0 ) {
      printf("No security sensors have been defined.\n");
      return 0;
   }

   j = 0;
   while ( aliasp[j].line_no > 0 ) {
      if ( (aliasp[j].optflags & (MOPT_SENSOR | MOPT_RFXSENSOR | MOPT_RFXMETER | MOPT_PLCSENSOR)) == 0 ) {
         j++;
         continue;
      }

      hc = aliasp[j].housecode;
      hcode = hc2code(hc);
      is_rfx = (aliasp[j].optflags & MOPT_RFXSENSOR) ? 1 : 0;
      for ( unit = 1; unit <= 16; unit++ ) {
         ucode = unit2code(unit);
         bitmap = aliasp[j].unitbmap & (1 << ucode);
         if ( bitmap ) {
            timestamp = x10state[hcode].timestamp[ucode];
            inactive = x10state[hcode].state[InactiveState] & bitmap;
//            active = ~inactive & x10state[hcode].state[ValidState] & bitmap;
            active = x10state[hcode].state[ActiveState] & bitmap;

            batstatus = ((aliasp[j].optflags & MOPT_LOBAT) == 0) ? "n/a," :
              (timestamp == 0) ? "---," :
              ( (is_rfx && (x10state[hcode].rfxlobat & (1 << ucode))) ||
                (x10state[hcode].vflags[ucode] & SEC_LOBAT) ) ? "LOW," : "OK,";   

            printf("%c%-2d %-*s  %-*s  %sbattery %-4s ", hc, unit,
              maxmod, lookup_module_name(aliasp[j].modtype),
              maxlabel, aliasp[j].label,
              ((x10state[hcode].vflags[ucode] & SEC_TAMPER) ? "*TAMPER*, " : ""),
              batstatus);


            if ( aliasp[j].optflags & MOPT_HEARTBEAT ) {
               if ( !(active | inactive) )
                  printf("active ---, tstamp ---\n");
               else if ( active ) {
                  gendate(datestr, timestamp, NO, YES);
                  printf("active YES, tstamp %s\n", datestr);
               }
               else if ( timestamp == 0 ) {
                  printf("active NO,  tstamp ---\n");
               }
               else {
                  gendate(datestr, timestamp, NO, YES);
                  printf("active NO,  tstamp %s\n", datestr);
               }
            }
            else {
               if ( timestamp == 0 ) {
                  printf("active n/a, tstamp ---\n");
               }
               else {
                  gendate(datestr, timestamp, NO, YES);
                  printf("active n/a, tstamp %s\n", datestr);
               }
            }
 
         }
      }
      j++;
   }
   return 0;
}

/*---------------------------------------------------------------------+
 | Display state of security, RFX, Oregon sensors                      |
 +---------------------------------------------------------------------*/
int show_security_sensors_old ( void )
{
   ALIAS         *aliasp;
   char          hc;
   int           j, unit, count = 0, maxmod = 0, maxlabel = 0;
   int           is_rfx;
   unsigned char hcode, ucode;
   unsigned int  bitmap;
   time_t        timenow, timestamp;
   long          elapsed;
   char          *batstatus;
   char          datestr[40];

   if ( !(aliasp = config.aliasp) )
      return 0;

   /* Get maximum lengths of module name and alias label */
   j = 0;
   while ( aliasp[j].line_no > 0 ) {
      if ( (aliasp[j].optflags & (MOPT_SENSOR | MOPT_RFXSENSOR | MOPT_RFXMETER | MOPT_PLCSENSOR)) != 0 ) {
         count++;
         maxlabel = max(maxlabel, (int)strlen(aliasp[j].label));
         maxmod = max(maxmod, (int)strlen(lookup_module_name(aliasp[j].modtype)) );
      }
      j++;
   }

   if ( count == 0 ) {
      printf("No security sensors have been defined.\n");
      return 0;
   }

   j = 0;
   while ( aliasp[j].line_no > 0 ) {
      if ( (aliasp[j].optflags & (MOPT_SENSOR | MOPT_RFXSENSOR | MOPT_RFXMETER | MOPT_PLCSENSOR)) == 0 ) {
         j++;
         continue;
      }

      hc = aliasp[j].housecode;
      hcode = hc2code(hc);
      bitmap = aliasp[j].unitbmap;
      timenow = time(NULL);
      is_rfx = (aliasp[j].optflags & MOPT_RFXSENSOR) ? 1 : 0;
      for ( unit = 1; unit <= 16; unit++ ) {
         ucode = unit2code(unit);
         if ( bitmap & (1 << ucode) ) {
            timestamp = x10state[hcode].timestamp[ucode];
            elapsed = (long)(timenow - timestamp);

            batstatus = ((aliasp[j].optflags & MOPT_LOBAT) == 0) ? "n/a," :
              (timestamp == 0) ? "---," :
              ( (is_rfx && (x10state[hcode].rfxlobat & (1 << ucode))) ||
                (x10state[hcode].vflags[ucode] & SEC_LOBAT) ) ? "LOW," : "OK,";   

            printf("%c%-2d %-*s  %-*s  %sbattery %-4s ", hc, unit,
              maxmod, lookup_module_name(aliasp[j].modtype),
              maxlabel, aliasp[j].label,
              ((x10state[hcode].vflags[ucode] & SEC_TAMPER) ? "*TAMPER*, " : ""),
              batstatus);


            if ( aliasp[j].optflags & MOPT_HEARTBEAT ) {
               if ( timestamp == 0 )
                  printf("active ---, tstamp ---\n");
               else if ( elapsed < config.inactive_timeout ) {
                  gendate(datestr, timestamp, NO, YES);
                  printf("active YES, tstamp %s\n", datestr);
               }
               else {
                  gendate(datestr, timestamp, NO, YES);
                  printf("active NO,  tstamp %s\n", datestr);
               }
            }
            else {
               if ( timestamp == 0 ) {
                  printf("active n/a, tstamp ---\n");
               }
               else {
                  gendate(datestr, timestamp, NO, YES);
                  printf("active n/a, tstamp %s\n", datestr);
               }
            }
 
         }
      }
      j++;
   }
   return 0;
}

/*---------------------------------------------------------------------+
 | Check all modules defined by their module_type in an alias as a     |
 | security sensor.  OR the results from all security sensors.         |
 | Display a bitmap with 0 = OK, 1 = low battery, 2 = inactive         |
 | Display an error message if the heyu_engine is not running.         |
 +---------------------------------------------------------------------*/
int c_sensorfault ( int argc, char *argv[] )
{
   ALIAS         *aliasp;
   int           j, count = 0;
   unsigned char hcode, ucode, status = 0;
   unsigned int  bitmap;

   if ( check_for_engine() != 0 ) {
      fprintf(stderr, "State engine is not running.\n");
      return 1;
   }

   if ( argc > 2 ) {
      fprintf(stderr, "This command takes no parameters.\n");
      return 1;
   }

   fetch_x10state();

   aliasp = config.aliasp;

   j = 0;
   while ( aliasp && aliasp[j].line_no > 0 ) {
      if ( (aliasp[j].optflags & (MOPT_HEARTBEAT | MOPT_LOBAT)) == 0 ) {
         j++;
         continue;
      }
      hcode = hc2code(aliasp[j].housecode);
      bitmap = aliasp[j].unitbmap;
      for ( ucode = 0; ucode < 16; ucode++ ) {
         if ( bitmap & (1 << ucode) ) {
            count++;
            if ( x10state[hcode].state[InactiveState] & (1 << ucode) )
               status |= 2;
#if 0
            if ( x10state[hcode].vflags[ucode] & SEC_LOBAT )
               status |= 1;
            if ( x10state[hcode].vflags[ucode] & SEC_TAMPER )
               status |= 8;
#endif
            if ( x10state[hcode].state[LoBatState] & (1 << ucode) )
               status |= 1;
         }
      }
      j++;
   }
   if ( x10global.sflags & GLOBSEC_TAMPER )
      status |= 8;
   
   printf("%d\n", status);

   return 0;
}

#if 0
/*---------------------------------------------------------------------+
 | Update the Active, Inactive, and ActiveChg states                   |
 +---------------------------------------------------------------------*/
int update_activity_states ( unsigned char hcode, unsigned int bitmap, unsigned char mode )
{
   unsigned int prevstate, newstate, changed;

   prevstate = x10state[hcode].state[InactiveState];

   if ( mode == S_INACTIVE ) {
      newstate = prevstate | bitmap;
   }
   else {
      newstate = prevstate & ~bitmap;
   }
   x10state[hcode].state[InactiveState] = newstate;
   x10state[hcode].state[ActiveState] = ~newstate & x10state[hcode].state[ValidState];

   changed = prevstate ^ newstate;

   x10state[hcode].state[ActiveChgState] =
       (x10state[hcode].state[ActiveChgState] & ~bitmap) | changed;

   return 0;
}
#endif

/*---------------------------------------------------------------------+
 | Reset ValidState for all modules                                    |
 +---------------------------------------------------------------------*/
void invalidate_all_signals ( void )
{
   unsigned char hcode;

   for ( hcode = 0; hcode < 16; hcode++ )
      x10state[hcode].state[ValidState] = 0;

   return;
}


/*---------------------------------------------------------------------+
 | Reset ValidState for virtual modules                                |
 +---------------------------------------------------------------------*/
void invalidate_virtual_signals ( void )
{
   unsigned char hcode;

   for ( hcode = 0; hcode < 16; hcode++ )
      x10state[hcode].state[ValidState] &= ~modmask[VdataMask][hcode];

   return;
}


/*---------------------------------------------------------------------+
 | Reset the heartbeat_sensors countdown array for argument modules    |
 +---------------------------------------------------------------------*/
void reset_security_sensor_timeout ( unsigned char hcode, unsigned int bitmap )
{
   ALIAS *aliasp;
   unsigned int activebmap;
   int j, index;

   if ( !(aliasp = configp->aliasp) )
      return;

   for ( j = 0; j < num_heartbeat_sensors; j++ ) {
      index = heartbeat_sensor[j].alias_index;
      activebmap = aliasp[index].unitbmap & bitmap;
      if ( hc2code(aliasp[index].housecode) == hcode && activebmap != 0 ) {
         heartbeat_sensor[j].countdown = aliasp[heartbeat_sensor[j].alias_index].hb_timeout;
         update_activity_states(hcode, activebmap, S_ACTIVE);
      }
   }
   return;
}   

/*---------------------------------------------------------------------+
 | Initialize the heartbeat_sensors countdown array.                   |
 +---------------------------------------------------------------------*/
int initialize_security_sensor_timeout ( CONFIG *configp )
{
   ALIAS         *aliasp;
   int           j;
   unsigned char hcode, ucode;
   time_t        now; 

   if ( !(aliasp = configp->aliasp) )
      return 0;

   time(&now);

   num_heartbeat_sensors = 0;
   j = 0;
   while ( aliasp[j].line_no > 0 && num_heartbeat_sensors < MAX_SEC_SENSORS ) {
      hcode = hc2code(aliasp[j].housecode);
      ucode = single_bmap_unit(aliasp[j].unitbmap);
      if ( ucode < 0x10u &&
           (aliasp[j].optflags & MOPT_HEARTBEAT) &&
          !(aliasp[j].optflags & MOPT_RFIGNORE) ) {
         heartbeat_sensor[num_heartbeat_sensors].countdown = aliasp[j].hb_timeout;

         if ( (x10state[hcode].timestamp[ucode] != 0) &&
              (now - x10state[hcode].timestamp[ucode]) > aliasp[j].hb_timeout ) {
            update_activity_states(hcode, (1 << ucode), S_INACTIVE);
         }
         else {
            update_activity_states(hcode, (1 << ucode), S_ACTIVE);
         }

         aliasp[j].hb_index = num_heartbeat_sensors;
         heartbeat_sensor[num_heartbeat_sensors++].alias_index = j;
      }
      j++;
   }

   return num_heartbeat_sensors;
}

/*---------------------------------------------------------------------+
 | Initialize timestamps                                               |
 +---------------------------------------------------------------------*/
void initialize_timestamps ( void )
{
   int hcode, ucode;

   for ( hcode = 0; hcode < 16; hcode++ ) {
      for ( ucode = 0; ucode < 16; ucode++ ) {
        x10state[hcode].timestamp[ucode] = 0;
      }
   }
   return ;
}

/*---------------------------------------------------------------------+
 | Clear global and module tamper flags                                |
 +---------------------------------------------------------------------*/
int clear_tamper_flags ( void )
{
   unsigned char hcode, ucode;

   for ( hcode = 0; hcode < 16; hcode++ ) {
      for ( ucode = 0; ucode < 16; ucode++ ) {
         x10state[hcode].vflags[ucode] &= ~SEC_TAMPER;
      }
      x10state[hcode].state[TamperState] = 0;
   }

   x10global.sflags &= ~GLOBSEC_TAMPER;

   return 0;
}

/*---------------------------------------------------------------------+
 | Load UTC0 times of today's Dawn and Dusk into global structure.     |
 | Called at heyu_engine startup or restart, and at Midnight, STD Time |
 +---------------------------------------------------------------------*/
int set_global_dawndusk ( time_t utc0 )
{

   if ( x10global.dawndusk_enable == 0 ) {
      x10global.utc0_dawn = x10global.utc0_dusk = 0;
      return 0;
   }

   local_dawndusk(utc0, &x10global.utc0_dawn, &x10global.utc0_dusk);

   return 0;
}

/*---------------------------------------------------------------------+
 | Called at startup and again once every minute to update the 'night' |
 | and 'dark' global flags.                                            |
 +---------------------------------------------------------------------*/
int update_global_nightdark_flags ( time_t utc0 )
{
   time_t         dawndark, duskdark;
   unsigned long  oldflags;

   if ( x10global.dawndusk_enable == 0 ) {
      x10global.sflags &= ~(NIGHT_FLAG | DARK_FLAG);
      return 0;
   }

   oldflags = x10global.sflags;

   if ( x10global.utc0_dawn <= x10global.utc0_dusk ) {   
      if ( utc0 < x10global.utc0_dawn || utc0 >= x10global.utc0_dusk )
         x10global.sflags |= NIGHT_FLAG;
      else 
         x10global.sflags &= ~NIGHT_FLAG;
   }
   else {
      if ( utc0 >= x10global.utc0_dawn || utc0 < x10global.utc0_dusk ) 
         x10global.sflags &= ~NIGHT_FLAG;
      else
         x10global.sflags |= NIGHT_FLAG;
   }


   dawndark = x10global.utc0_dawn - (time_t)60 * (time_t)x10global.isdark_offset;
   duskdark = x10global.utc0_dusk + (time_t)60 * (time_t)x10global.isdark_offset;

   if ( dawndark <= duskdark ) {
      if ( utc0 < dawndark || utc0 >= duskdark )
         x10global.sflags |= DARK_FLAG;
      else
         x10global.sflags &= ~DARK_FLAG;
   }
   else {
      if ( utc0 >= dawndark || utc0 < duskdark )
         x10global.sflags &= ~DARK_FLAG;
      else
         x10global.sflags |= DARK_FLAG;
   }

   if ( i_am_state && ((oldflags ^ x10global.sflags) & (NIGHT_FLAG | DARK_FLAG)) )
      write_x10state_file();

   return 0;
}

/*---------------------------------------------------------------------+
 | Display global Dawn and Dusk in Civil Time                          |
 +---------------------------------------------------------------------*/
char *display_global_dawndusk ( void )
{
   static char buffer[64];
   struct tm *tmp;

   tmp = localtime(&x10global.utc0_dawn);
   sprintf(buffer, "dawn = %02d:%02d, ", tmp->tm_hour, tmp->tm_min);
   tmp = localtime(&x10global.utc0_dusk);
   sprintf(buffer + strlen(buffer), "dusk = %02d:%02d\n", tmp->tm_hour, tmp->tm_min);
   return buffer;
}

/*---------------------------------------------------------------------+
 | Pass back the times of dawn and dusk expressed as seconds after     |
 | midnight civil (wall clock) time.                                   |
 +---------------------------------------------------------------------*/
int dawndusk_today ( long *dawn, long *dusk )
{
   struct tm *tmp;

   if ( x10global.dawndusk_enable == 0 )
      return -1;

   tmp = localtime(&x10global.utc0_dawn);
   *dawn = 3600L * (long)tmp->tm_hour + 60L * (long)tmp->tm_min + (long)tmp->tm_sec;

   tmp = localtime(&x10global.utc0_dusk);
   *dusk = 3600L * (long)tmp->tm_hour + 60L * (long)tmp->tm_min + (long)tmp->tm_sec;

   return 0;
} 
   
/*---------------------------------------------------------------------+
 | Print remaining countdown times for active timers, i.e., with       |
 | non-zero countdown times.                                           |
 +---------------------------------------------------------------------*/
int show_global_timers ( void )
{
   int  j;
   long count;

   for ( j = 1; j <= NUM_USER_TIMERS; j++ ) {
      if ( (count = x10global.timer_count[j]) > 0 )
         printf("Timer %2d  %ld:%02ld:%02ld\n", j, count/3600L, (count % 3600L)/60L, count % 60);
   }
   return 0;
}

/*---------------------------------------------------------------------+
 | Print global Dawn and Dusk times for today.                         |
 +---------------------------------------------------------------------*/
int show_state_dawndusk ( void )
{
   struct tm   *tmp;
   extern char *heyu_tzname[2];
   extern void fix_tznames ( void );
   long        lutc0;
   time_t      midnight;

   static char   *sunmodelabel[] = {"Sunrise/Sunset", "Civil Twilight",
        "Nautical Twilight", "Astronomical Twilight"};

   if ( configp->loc_flag != (LONGITUDE | LATITUDE) ||
        x10global.dawndusk_enable == 0 ) {
      fprintf(stderr, "LONGITUDE and LATITUDE must be specified in config file.\n");
      return -1;
   }

   fix_tznames();

   lutc0 = (long)time(NULL);
   midnight = (time_t)(lutc0 - ((lutc0 - configp->tzone) % 86400L));

   if ( x10global.utc0_dawn < midnight || x10global.utc0_dawn >= (midnight + (time_t)86400) ||
        x10global.utc0_dusk < midnight || x10global.utc0_dusk >= (midnight + (time_t)86400)   ) {
      fprintf(stderr, "Internal error: Dawn/Dusk times are for the wrong date.\n");
      return -1;
   }

   tmp = localtime(&x10global.utc0_dawn);
   printf("Dawn = %02d:%02d %s", tmp->tm_hour, tmp->tm_min, heyu_tzname[tmp->tm_isdst]);

   tmp = localtime(&x10global.utc0_dusk);
   printf("  Dusk = %02d:%02d %s", tmp->tm_hour,  tmp->tm_min, heyu_tzname[tmp->tm_isdst]);
   printf("  (%s)\n", sunmodelabel[configp->sunmode]);

   return 0;
}

/*---------------------------------------------------------------------+
 | Compute the UTC0 time for 24:00:00 Std Time tonight, i.e., 00:00:00 |
 | tomorrow, from the current UTC0 time and configured timezone.       |
 +---------------------------------------------------------------------*/
void set_global_tomorrow ( time_t utc0 )
{
   long lutc0;

   lutc0 = (long)utc0;

   x10global.utc0_tomorrow =
        (time_t)(lutc0 - ((lutc0 - configp->tzone) % 86400L) + 86400L);
   return;
}

/*---------------------------------------------------------------------+
 | Return 1 if argument utc0 has reached or exceeded utc0 for 00:00:00 |
 | hours tomorrow; otherwise return 0.                                 |
 +---------------------------------------------------------------------*/
int is_tomorrow ( time_t utc0 )
{
   return (utc0 >= x10global.utc0_tomorrow) ? 1 : 0;
}

/*---------------------------------------------------------------------+
 | Called at heyu_engine startup and at a restart                      |
 +---------------------------------------------------------------------*/
int engine_local_setup ( int mode )
{
   int check4poll(int, int);
   void create_rfxmeter_panels( void );
   int  clear_data_storage( void );
   time_t tnow;
   mode_t oldumask;

   /* Redirect output from engine to the Heyu log file */
//   if ( i_am_state ) {
      oldumask = umask((mode_t)configp->log_umask);
      fdsout = freopen(configp->logfile, "a", stdout);
      fdserr = freopen(configp->logfile, "a", stderr);
      umask(oldumask);
//   }
//   else {
//      fdsout = stdout;
//      fdserr = stderr;
//   }
 
   tnow = time(NULL);
   x10global.dawndusk_enable = (configp->loc_flag == (LONGITUDE | LATITUDE)) ? 1 : 0;
   x10global.isdark_offset = configp->isdark_offset;
   set_global_dawndusk(tnow);
   update_global_nightdark_flags(tnow);
   set_global_tomorrow(tnow);

   clear_data_storage();

   if ( mode == E_START ) {
      invalidate_all_signals();
      reset_user_timers();
   }

   initialize_security_sensor_timeout(configp);

   if ( i_am_state )
      write_x10state_file();

   return 0;
}

/*---------------------------------------------------------------------+
 | Called at heyu_engine startup and at a restart                      |
 +---------------------------------------------------------------------*/
int engine_local_setup_old ( void )
{
   int check4poll(int, int);
   void create_rfxmeter_panels( void );
   int  clear_data_storage( void );
   time_t tnow;
   mode_t oldumask;

   /* Redirect output to the Heyu log file */
   oldumask = umask((mode_t)configp->log_umask);
   fdsout = freopen(configp->logfile, "a", stdout);
   fdserr = freopen(configp->logfile, "a", stderr);
   umask(oldumask);
 
   tnow = time(NULL);
   x10global.dawndusk_enable = (configp->loc_flag == (LONGITUDE | LATITUDE)) ? 1 : 0;
   x10global.isdark_offset = configp->isdark_offset;
   set_global_dawndusk(tnow);
   update_global_nightdark_flags(tnow);
   set_global_tomorrow(tnow);

   clear_data_storage();
   invalidate_all_signals();
   reset_user_timers();

   initialize_security_sensor_timeout(configp);

   write_x10state_file();

   return 0;
}

/*---------------------------------------------------------------------+
 | Called at midnight, standard time                                   |
 +---------------------------------------------------------------------*/
void midnight_tick ( time_t utc0 )
{
   set_global_dawndusk(utc0);
   
   return;
}

/*---------------------------------------------------------------------+
 | Called once per hour                                                |
 +---------------------------------------------------------------------*/
void hour_tick ( time_t utc0 )
{
   launch_hourly_scripts();
   return;
}

/*---------------------------------------------------------------------+
 | Called once per minute                                              |
 +---------------------------------------------------------------------*/
void minute_tick ( time_t utc0 )
{

   update_global_nightdark_flags(utc0);

   /* Sanity - in case user has reset system time */
   set_global_tomorrow(utc0);

   return;
}

/*---------------------------------------------------------------------+
 | Called approximately once per second to actuate timer countdown.    |
 | (The time delta is included in case a tick is missed.)              |
 +---------------------------------------------------------------------*/
void second_tick ( time_t utc0, long delta ) 
{
   ALIAS         *aliasp;
   int           j, index;
   unsigned char hcode;

   /* Countdown Arm-pending and user timers */
   for ( j = 0; j <= NUM_USER_TIMERS; j++ ) {
      if ( x10global.timer_count[j] > 0 &&
           (x10global.timer_count[j] -= delta) <= 0 ) {
         x10global.timer_count[j] = 0;
         if ( j > 0 )
            x10global.tzflags[(j - 1) / 32] |= (1UL << ((j - 1) % 32));

         countdown_timer[j].elapsed_func(j);
      }
   }

   /* Countdown sensor heartbeat timers */
   if ( (aliasp = configp->aliasp) != NULL ) {
      for ( j = 0; j < num_heartbeat_sensors; j++ ) {
         index = heartbeat_sensor[j].alias_index;
         hcode = hc2code(aliasp[index].housecode);
         if ( heartbeat_sensor[j].countdown > 0 &&
              (heartbeat_sensor[j].countdown -= delta) <= 0 ) {
            heartbeat_sensor[j].countdown = aliasp[index].hb_timeout;
            update_activity_states(hcode, aliasp[index].unitbmap, S_INACTIVE);
            if ( configp->inactive_handling == OLD )
               sensor_elapsed_func_old(index);
            else
               sensor_elapsed_func(index);
         }
      }
   }

   return;
}

/*---------------------------------------------------------------------+
 | Display a table of Extended Code groups and levels for housecode    |
 +---------------------------------------------------------------------*/
void show_extended_groups ( unsigned char hcode )
{
   unsigned char ucode, grpmask, grel;
   unsigned int  bitmap, mask, relmask;
   int           unit, group, found;
   char          outbuf[80];
   char          minbuf[16];

   printf("Group levels (0-63) for Housecode %c\n", code2hc(hcode));

   printf("  Unit:");
   for ( unit = 1; unit <= 16; unit++ )
      printf("%3d", unit);
   printf("\nGroup  ");
   for ( unit = 1; unit <= 16; unit++ )
      printf(" --");
   printf("\n");

   mask = modmask[Ext3GrpExecMask][hcode];
   relmask = modmask[Ext3GrpRelMask][hcode];

   for ( group = 0; group < 4; group++ ) {
      grpmask = 1 << group;
      sprintf(outbuf, "  %d    ", group);
      /* Display the absolute group */
      for ( unit = 1; unit <= 16; unit++ ) {
         ucode = unit2code(unit);
         bitmap = 1 << ucode;
         if ( mask & bitmap ) {
            if ( (x10state[hcode].grpmember[ucode] & grpmask) &&
                 ((x10state[hcode].grpaddr[ucode] & 0x80u) == 0 ||
                  (x10state[hcode].grpmember[ucode] & 0x80u) != 0) ) {
               sprintf(minbuf, "%3d", x10state[hcode].grplevel[ucode][group]);
            }
            else {
               sprintf(minbuf, "  -");
            }
         }
         else {
            sprintf(minbuf, "  .");
         }
         strcat(outbuf, minbuf);
      }
      printf("%s\n", outbuf);

      /* Scan for relative groups and display if any */
      for ( grel = 0x80u; grel < 0x8fu; grel++ ) {
         sprintf(outbuf, "  %d.%-2d ", group, (grel & 0x7fu) + 1);
         found = 0;
         for ( unit = 1; unit <= 16; unit++ ) {
            ucode = unit2code(unit);
            bitmap = 1 << ucode;
            if ( mask & bitmap ) {
               if ( (x10state[hcode].grpmember[ucode] & grpmask) &&
                     x10state[hcode].grpaddr[ucode] == grel ) {
                  found = 1;
                  sprintf(minbuf, "%3d", x10state[hcode].grplevel[ucode][group]);
               }
               else {
                  sprintf(minbuf, "  -");
               }
            }
            else {
               sprintf(minbuf, "  .");
            }
            strcat(outbuf, minbuf);
         }
         if ( found ) {
            printf("%s\n", outbuf);
         }
      }   
   }

   printf("\n");
      
   return;
}
   
/*---------------------------------------------------------------------+
 | Copy xmodule information from x10state table to argument arrays.    |
 +---------------------------------------------------------------------*/
int copy_xmodule_info ( unsigned char hcode, unsigned char grpmember[],
                   unsigned char grpaddr[], unsigned char grplevel[][4],
                   unsigned char *xconfigmode )
{
   int ucode, group;

   for ( ucode = 0; ucode < 16; ucode++ ) {
      grpmember[ucode] = x10state[hcode].grpmember[ucode];
      grpaddr[ucode] = x10state[hcode].grpaddr[ucode];
      for ( group = 0; group < 4; group++ )
         grplevel[ucode][group] = x10state[hcode].grplevel[ucode][group];
   }

   *xconfigmode = x10state[hcode].xconfigmode;

   return 0;
}
   
/*---------------------------------------------------------------------+
 | Search the array of ALIAS structures for an alias having the        |
 | argument housecode, bitmap and vtype.  Return the alias index if    |
 | found, otherwise -1.                                                |
 +---------------------------------------------------------------------*/
int alias_lookup_index ( char hc, unsigned int bitmap, unsigned char vtype )
{
   ALIAS *aliasp;
   int   j;

   aliasp = configp->aliasp;
   hc = toupper((int)hc);

   j = 0;
   while ( aliasp && aliasp[j].line_no > 0 ) {
      if ( hc == aliasp[j].housecode &&
           bitmap == aliasp[j].unitbmap &&
           vtype == aliasp[j].vtype ) {
         return j;
      }
      j++;
   }
   return -1;
}

/*---------------------------------------------------------------------+
 | Used to update the record of the last timed macro execution.        |
 +---------------------------------------------------------------------*/
void set_macro_timestamp ( time_t now )
{
   x10global.utc0_macrostamp = now;
   return;
}

/*---------------------------------------------------------------------+
 | Retrieve the record of the last timed macro execution.              |
 +---------------------------------------------------------------------*/
time_t get_macro_timestamp ( void )
{ 
   return x10global.utc0_macrostamp;
}


/*---------------------------------------------------------------------+
 | Set, Increment, or Decrement internal counter and flags.            |
 +---------------------------------------------------------------------*/
int set_counter ( int index, unsigned short count, unsigned char mode )
{
   int retcode = 0;

   if ( mode == CNT_DEC ) {
      /* Decrement counter by 1 */
      if ( x10global.counter[index - 1] > 0 )
         x10global.counter[index - 1] -= 1;
   }
   else if ( mode == CNT_DSKPZ ) {
      /* Decrement counter by 1 and Skip if Zero */
      if ( x10global.counter[index - 1] > 0 )
         x10global.counter[index - 1] -= 1;
      if ( x10global.counter[index - 1] == 0 )
         retcode = -1;
   }
   else if ( mode == CNT_DSKPNZ ) {
      /* Decrement counter by 1 and Skip if Not Zero */
      if ( x10global.counter[index - 1] > 0 ) 
         x10global.counter[index - 1] -= 1;
      if ( x10global.counter[index - 1] > 0 )
         retcode = -1;
   }
   else if ( mode == CNT_DSKPNZIZ ) {
      /* Decrement counter by 1 and Skip if Not Zero */
      /* (or if counter is 0 to start with).         */
      if ( x10global.counter[index - 1] > 0 ) {
         x10global.counter[index - 1] -= 1;
         if ( x10global.counter[index - 1] > 0 )
            retcode = -1;
      }
      else {
         retcode = -1;
      }
   }
   else if ( mode == CNT_INC ) {
      /* Increment counter by 1 */
      if ( x10global.counter[index - 1] < 0xffff )
         x10global.counter[index - 1] += 1;
   }
   else {
      /* Set counter value */
      x10global.counter[index - 1] = count;
   }

   /* Set or reset counter = zero flag */
   if ( x10global.counter[index - 1] == 0 ) 
      x10global.czflags[(index - 1) / 32] |= (unsigned long)(1 << ((index - 1) % 32));
   else
      x10global.czflags[(index - 1) / 32] &= ~(unsigned long)(1 << ((index - 1) % 32));

   return retcode;
}
         
char *translate_counter_action ( unsigned char *buf )
{
   int index, count, mode;
   static char outbuf[80];

   index = buf[2] | (buf[3] << 8);
   count = buf[4] | (buf[5] << 8);
   mode  = buf[6];

   sprintf(outbuf, "Counter %d %s to %d", index,
      ((mode == CNT_INC) ? "incremented" :
       (mode == CNT_DEC) ? "decremented" : "set"), x10global.counter[index - 1]);

   return outbuf;
}
   
/*----------------------------------------------------------------------------+
 | Send a setcounter message to the heyu monitor/state engine                 |
 +----------------------------------------------------------------------------*/
void send_setcounter_msg ( int index, unsigned short count, unsigned char mode )
{
   extern int sptty;

   int ignoret;

   static unsigned char template[11] = {
     0xff,0xff,0xff,7,ST_COMMAND,ST_COUNTER,0,0, 0,0, 0};

   template[6] = index & 0x00ff;
   template[7] = (index & 0xff00) >> 8;
   template[8] = count & 0x00ff;
   template[9] = (count & 0xff00) >> 8;
   template[10] = mode;

   ignoret = write(sptty, template, sizeof(template));
   return;
}

/*----------------------------------------------------------------------------+
 | Update the timeout for a PLC Sensor                                        |
 +----------------------------------------------------------------------------*/
int update_plcsensor_timeout ( unsigned char hcode, unsigned int bitmap )
{
   ALIAS *aliasp;
   int   j;
   char  hc;

   if ( !(aliasp = configp->aliasp) )
      return 0;

   hc = code2hc(hcode);
   j = 0;
   while ( aliasp[j].line_no > 0 ) {
      if ( aliasp[j].housecode == hc && aliasp[j].unitbmap == bitmap ) {
         update_activity_timeout(aliasp, j);
         update_activity_states(hcode, bitmap, S_ACTIVE);
         break;
      }
      j++;
   }
   return 0;
}

   
/*---------------------------------------------------------------------+
 | Set contents of all data in x10global.data_storage[] array to zero  |
 +---------------------------------------------------------------------*/
int clear_data_storage ( void )
{
   int j;

   for ( j = 0; j < MAXDATASTORAGE; j++ )
      x10global.data_storage[j] = 0;

   return 0;
}


/*---------------------------------------------------------------------+
 | Assign data storage location in x10global.data_storage array for    |
 | sensors requiring it.                                               |
 +---------------------------------------------------------------------*/
int assign_data_storage ( void )
{
   ALIAS         *aliasp;
   int           j, index;
   char          errmsg[80];

   if ( !(aliasp = configp->aliasp) )
      return 0;

   index = 0;
   j = 0;
   while ( aliasp[j].line_no > 0 ) {
      if ( aliasp[j].storage_units == 0 ) {
         j++;
         continue;
      }
      if ( index < MAXDATASTORAGE ) {
         aliasp[j].storage_index = index;
         index += aliasp[j].storage_units;
      }
      else {
         sprintf(errmsg, "Data storage limit exceeded.");
         store_error_message(errmsg);
         return 1;
      }
      j++;
   }
   for ( j = 0; j < index; j++ )
      x10global.data_storage[j] = 0;

   return 0;
}

int c_doit ( int argc, char *argv[] )
{
   return 0;
}

/*---------------------------------------------------------------------+
 | Launch a script from the command line.                              |
 +---------------------------------------------------------------------*/
int c_launch ( int argc, char *argv[] )
{
   LAUNCHER *launcherp;
   SCRIPT   *scriptp;
   int      j, nl, scriptnum, launchernum;
   char     *sp, *scriptlabel;
   unsigned char eflag, mode = 0;


   if ( argc < 3 ) {
      printf("Usage: %s %s [-L<n>] [-e] <script_label>\n", argv[0], argv[1]);
      printf("  -L<n>  Test only launcher <n> launch conditions\n");
      printf("  -e     Test only flags in launch conditions\n");
      return 0;
   }

   if ( !(launcherp = configp->launcherp) ||
        !(scriptp   = configp->scriptp)      ) {
      fprintf(stderr, "No scripts are configured.\n");
      return 1;
   }

   scriptlabel = "";

   launchernum = -1;
   eflag = 0;
   for ( j = 2; j < argc; j++ ) {
      if ( *argv[j] != '-' ) {
         scriptlabel = argv[j];
         if ( j != (argc - 1) ) {
            fprintf(stderr, "Usage: %s %s [-e] [-L<n>] <script_label>\n", argv[0], argv[1]);
            return 1;
         } 
         break;
      }
      else if ( strcmp(argv[j], "-e") == 0 ) {
         eflag = 0x80;
      }
      else if ( strncmp(argv[j], "-L", 2) == 0 ) {
         launchernum = -1;
         sp = " ";
         if ( strlen(argv[j]) > 2 ) {
            launchernum = strtol((argv[j] + 2), &sp, 10);
         }
         if ( *sp || launchernum < 0) {
            fprintf(stderr, "Invalid (or missing) launcher number in '%s'\n", argv[j]);
            return 1;
         }
         mode = 1;
      }
      else {
         fprintf(stderr, "Unknown switch '%s'\n", argv[j]);
         return 1;
      }
   }

   j = 0; scriptnum = -1;
   while ( launcherp[j].line_no > 0 ) {
      if ( strcmp(launcherp[j].label, scriptlabel) == 0 ) {
         nl = scriptp[launcherp[j].scriptnum].num_launchers;
         if ( launchernum > (nl - 1) ) {
            fprintf(stderr, "Launcher number %d outside range 0-%d\n", launchernum, (nl - 1));
            return 1;
         }
         scriptnum = launcherp[j].scriptnum;
         break;
      }
      j++;
   }

   if ( scriptnum < 0 ) {
      fprintf(stderr, "Script '%s' not found.\n", argv[2]);
      return 1;
   }

   mode |= eflag;

   send_launch_request(mode, scriptnum, ((launchernum >= 0) ? launchernum : 0));

   return 0;
}


/*---------------------------------------------------------------------+
 | Test launch conditions against global flags and against the states  |
 | Addressed/On/Off/Dim/Changed of the HU addresses (where applicable) |
 | If eflag > 0, test only against global flags.                       |
 | Return 1 if successful, 0 otherwise.                                |
 +---------------------------------------------------------------------*/
int test_launcher ( LAUNCHER *launcherp, unsigned char eflag )
{
   unsigned int bitmap, funcmap, chgmask, active;
   unsigned char hcode;

   hcode = launcherp->hcode;
   if ( launcherp->unitstar )
      bitmap = 0xffff;
   else
      bitmap = launcherp->bmaptrig; 

   if ( is_unmatched_flags(launcherp) )
      return 0;

   if ( eflag || (launcherp->type & ~(L_NORMAL | L_ADDRESS)) ) {
      launcherp->type = L_EXEC;
      launcherp->bmaplaunch = 0;
      launcherp->actfunc = InvalidFunc;
      launcherp->actsource = SNDC;
      return 1;
   }

   if ( launcherp->type == L_ADDRESS ) {
      if ( (active = bitmap & x10state[hcode].addressed) ) {
         launcherp->bmaplaunch = active;
         launcherp->actsource = SNDC;
         return 1;
      }
      else {
         return 0;
      }
   }

   if ( launcherp->type != L_NORMAL ) {
      return 0;
   }

   if ( launcherp->chgtrig > 0 )
      chgmask = x10state[hcode].state[ChgState];
   else
      chgmask = 0xffff;

   funcmap = launcherp->afuncmap | launcherp->gfuncmap;
   if ( funcmap & (1 << OnTrig)  &&
        bitmap &  x10state[hcode].state[OnState] &&
        bitmap & chgmask ) {
      launcherp->bmaplaunch = bitmap & x10state[hcode].state[OnState];
      launcherp->actfunc = OnFunc;
   }
   else if ( funcmap & (1 << OffTrig) &&
        bitmap & ~x10state[hcode].state[OnState] &&
        bitmap & chgmask ) {
      launcherp->bmaplaunch = bitmap & ~x10state[hcode].state[OnState];
      launcherp->actfunc = OffFunc;
   }
   else if ( funcmap & (1 << DimTrig) &&
        bitmap & x10state[hcode].state[DimState] &&
        bitmap & chgmask ) {
      launcherp->bmaplaunch = bitmap & x10state[hcode].state[DimState];
      launcherp->actfunc = DimFunc;
   }
   else {
      return 0;
   }

   launcherp->actsource = SNDC;

   return 1;
}

/*---------------------------------------------------------------------+
 | Select launcher(s) to be tested.  Test each one in turn and execute |
 | the script for the first one which passes.  (Since the launcher is  |
 | modified by the test, we first make a local copy to work with so as |
 | not to change the original.)                                        |
 | The buffer will have been sent to heyu_engine with the function     |
 | send_launch_request()                                               |
 +---------------------------------------------------------------------*/
int launch_script_cmd ( unsigned char *buffer )
{
   LAUNCHER *launcherpp, *launcherp;
   LAUNCHER launchercopy;
   unsigned char mode, eflag, save_ctrl;
   int j, scriptnum, launchernum, retcode;

   mode = buffer[2] & 0x7f;
   eflag = buffer[2] & 0x80;
   scriptnum = buffer[3] | (buffer[4] << 8);
   launchernum = buffer[5] | (buffer[6] << 8);

   if ( configp->scriptp == NULL || configp->launcherp == NULL ) {
      return 1;
   }


   launcherpp = configp->launcherp;

   if ( mode == 0 ) {
      /* Check all launchers for the script */
      j = 0;
      while ( launcherpp[j].line_no > 0 ) {
         if ( launcherpp[j].scriptnum == scriptnum ) {
            memcpy(&launchercopy, &launcherpp[j], sizeof(LAUNCHER));
            launcherp = &launchercopy;
            if ( test_launcher(launcherp, eflag) == 1 ) {
               save_ctrl = configp->script_ctrl;
               configp->script_ctrl = ENABLE;
               retcode = exec_script(launcherp);
               configp->script_ctrl = save_ctrl;
               return retcode;
            }
         }
         j++;
      }
   }
   else {
      /* Check only the specified launcher number for the script */
      j = 0;
      while ( launcherpp[j].line_no > 0 ) {
         if ( launcherpp[j].scriptnum == scriptnum &&
              launcherpp[j].launchernum == launchernum ) {
            memcpy(&launchercopy, &launcherpp[j], sizeof(LAUNCHER));
            launcherp = &launchercopy;
            if ( test_launcher(launcherp, eflag) == 1 ) {
               save_ctrl = configp->script_ctrl;
               configp->script_ctrl = ENABLE;
               retcode = exec_script(launcherp);
               configp->script_ctrl = save_ctrl;
               return retcode;
            }
            else {
               return 0;
            }
         }
         j++;
      }
   }
   return 0;
}

/*---------------------------------------------------------------------+
 | Display warnings for security zones in fault, i.e., Door/Window     |
 | sensors in Alert status.                                            |
 +---------------------------------------------------------------------*/
int warn_zone_faults ( FILE *fp, char *datestr )
{
   ALIAS *aliasp;
   int   j, retcode = 0;
   char  hc;
   unsigned char hcode, ucode;

   if ( !(aliasp = configp->aliasp) ) 
      return 0;

   j = 0;
   while ( aliasp[j].line_no > 0 ) {
      if ( !(aliasp[j].vtype == RF_SEC) || (ucode = single_bmap_unit(aliasp[j].unitbmap)) == 0xff ) {
         j++;
         continue;
      }
      hc = aliasp[j].housecode;
      hcode = hc2code(hc);
      if ( x10state[hcode].vflags[ucode] & SEC_FAULT ) {
         fprintf(fp, "%s *** Warning: %c%d (%s) is in Alert mode.\n", datestr, hc, code2unit(ucode), aliasp[j].label);
         retcode = 1;
      }
      j++;
   }
   return retcode;
} 
      
/*---------------------------------------------------------------------+
 | Display stored data for X10 Security sensors                        |
 +---------------------------------------------------------------------*/
int show_x10_security ( void )
{

   ALIAS          *aliasp;
   char           hc;
   int            unit, index, count = 0, maxlabel = 0, maxmod = 0;
   unsigned char  hcode, ucode;
   unsigned long  vflags;

   struct xlate_vdata_st xlate_vdata;

   if ( !(aliasp = configp->aliasp) ) 
      return 0;

   /* Get maximum lengths of module name and alias label */
   index = 0;
   while ( aliasp[index].line_no > 0 ) {
      if ( aliasp[index].vtype == RF_SEC && (aliasp[index].optflags & MOPT_SENSOR) ) {
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
      if ( aliasp[index].vtype != RF_SEC || !(aliasp[index].optflags & MOPT_SENSOR) ) {
         index++;
         continue;
      }

      ucode = single_bmap_unit(aliasp[index].unitbmap);
      unit = code2unit(ucode);
      hc = aliasp[index].housecode;
      hcode = hc2code(hc);

      if ( x10state[hcode].vflags[ucode] & SEC_AUX ) {
         xlate_vdata.vdata = x10state[hcode].memlevel[ucode];
      }
      else {
         xlate_vdata.vdata = x10state[hcode].dimlevel[ucode];
      }
      xlate_vdata.optflags = aliasp[index].optflags;
      xlate_vdata.optflags2 = aliasp[index].optflags2;
      xlate_vdata.ident = 0;
      xlate_vdata.identp = aliasp[index].ident;
      xlate_vdata.nident = aliasp[index].nident;
      xlate_vdata.vflags = x10state[hcode].vflags[ucode];
      
      if ( aliasp[index].xlate_func(&xlate_vdata) != 0 ) { 
         fprintf(stderr, "Show function error.\n");
         fflush(stderr);
         return 1;
      }
      
      vflags = xlate_vdata.vflags;

      printf("%c%-2d %-*s %-*s ", hc, unit,
          maxmod, lookup_module_name(aliasp[index].modtype),
          maxlabel, aliasp[index].label);

      if ( x10state[hcode].timestamp[ucode] == 0 ) {
         printf("----\n");
         index++;
         continue;
      }

      printf("%s ", funclabel[xlate_vdata.func]);

      if ( vflags & SEC_MAX )
         printf("swMax ");
      else if ( vflags & SEC_MIN )
         printf("swMin ");

      if ( vflags & SEC_MAIN )
         printf("Main ");
      else if ( vflags & SEC_AUX )
         printf("Aux");

      if ( vflags & SEC_LOBAT )
         printf("LoBat ");
      printf("\n");

      index++;
   }

   return 0;
}

void display_env_masks ( void )
{
   int j;

   printf("\nMasks for Heyu environment\n");
   printf("variables X10_Hu and x10_<Hu_alias>\n");
   j = 0;
   while ( heyumaskval[j].query ) {
//      printf("  $%-10s  0x%08lx  %7lu\n", heyumaskval[j].query, heyumaskval[j].mask, heyumaskval[j].mask);
      printf("  $%s\n", heyumaskval[j].query);
      j++;
   }

   printf("\nMasks for Heyu environment\n");
   printf("variables X10_Hu_vFlags and x10_<Hu_alias>_vFlags\n");
   j = 0;
   while ( heyusecmaskval[j].query ) {
//      printf("  $%-10s  0x%08lx  %7lu\n", heyusecmaskval[j].query, heyusecmaskval[j].mask, heyusecmaskval[j].mask);
      printf("  $%s\n", heyusecmaskval[j].query);
      j++;
   }

   printf("\nMasks for Xtend environment\n");
   printf("variables X10_Hu and x10_<Hu_alias>\n");
   j = 0;
   while ( xtendmaskval[j].query ) {
//      printf("  $%-10s  0x%08lx  %7lu\n", xtendmaskval[j].query, xtendmaskval[j].mask, xtendmaskval[j].mask);
      printf("  $%s\n", xtendmaskval[j].query);
      j++;
   }
   printf("\n");

   return;
}

/*---------------------------------------------------------------------+
 | Return the index of an alias having the argument housecode and      |
 | unit bitmap, and having a configured module type.                   |
 +---------------------------------------------------------------------*/
int locate_alias_module_index ( char housecode, unsigned int unitbmap )
{
   ALIAS *aliasp;
   int   index;

   if ( !(aliasp = configp->aliasp) ) 
      return -1;

   index = 0;
   while ( aliasp[index].line_no > 0 ) {
      if ( aliasp[index].housecode == housecode &&
           aliasp[index].unitbmap == unitbmap &&
           aliasp[index].modtype >= 0 ) {
         return index;
      } 
      index++;
   }
   return -1;
}

static struct func_type_st {
   char          *label;
   int           func;
} sec_func_type[] = {
  {"panic",        PanicFunc},
  {"arm",          ArmFunc},
  {"disarm",       DisarmFunc},
  {"alert",        AlertFunc},
  {"clear",        ClearFunc},
  {"test",         TestFunc},
  {"sectamper",    TamperFunc},
  {"slightson",    SecLightsOnFunc},
  {"slightsoff",   SecLightsOffFunc},
  {"sdusk",        DuskFunc},
  {"sdawn",        DawnFunc},
  {"akeyon",       AkeyOnFunc},
  {"akeyoff",      AkeyOffFunc},
  {"bkeyon",       BkeyOnFunc},
  {"bkeyoff",      BkeyOffFunc},
  {"inactive",     InactiveFunc},
  {"",             0}
};

static struct local_flags_st {
  char *label;
  unsigned long flag;
} local_flags[] = {
  {"lobat",   SEC_LOBAT},
  {"swhome",  SEC_HOME},
  {"swaway",  SEC_AWAY},
  {"swmin",   SEC_MIN},
  {"swmax",   SEC_MAX},
  {"main",    SEC_MAIN},
  {"aux",     SEC_AUX},
  {"",        0}
};

/*---------------------------------------------------------------------+
 | Emulate an X10 Security transmission from the command line.         |
 +---------------------------------------------------------------------*/
int c_sec_emu ( int argc, char *argv[] )
{
   ALIAS        *aliasp;
   char         hc;
   unsigned int bitmap;
   int          j, k, index, found;
   int          func;
   unsigned char vdata;
   unsigned long ident;
   unsigned long vflags;
   unsigned int  aflags;
   struct xlate_vdata_st xlate_vdata;

   if ( argc < 4 ) {
      fprintf(stderr, "Too few parameters.\n");
      return 1;
   }

   aflags = parse_addr(argv[2], &hc, &bitmap);

   if ( !(aflags & A_VALID) || (aflags & (A_PLUS | A_MINUS | A_MULT)) || bitmap == 0) {
      fprintf(stderr, "Invalid Housecode|unit parameter '%s'.\n", argv[2]);
      return 1;
   }

   index = locate_alias_module_index(hc, bitmap);

   aliasp = configp->aliasp;

   if ( index < 0 || !aliasp ||
        aliasp[index].vtype != RF_SEC ||
        aliasp[index].xlate_func == NULL ) {
      fprintf(stderr, "Security device not found at Hu address.\n");
      return 1;
   }

   j = 0; found = 0;
   while ( *sec_func_type[j].label ) {
      if ( strcmp(argv[3], sec_func_type[j].label) == 0 ) {
         func = sec_func_type[j].func;
         found = 1;
         break;
      }
      j++;
   }
   if ( !found ) {
      fprintf(stderr, "Invalid security function '%s'.\n", argv[3]);
      return 1;
   }

   vflags = 0;
   for ( k = 4; k < argc; k++ ) {
      strlower(argv[k]);
      j = 0; found = 0;
      while ( *local_flags[j].label ) {
         if ( strcmp(argv[k], local_flags[j].label) == 0 ) {
            vflags |= local_flags[j].flag;
            found = 1;
            break;
         }
         j++;
      }
      if ( !found ) {
         fprintf(stderr, "Invalid flag '%s'.\n", argv[k]);
         return 1;
      }
   }


   xlate_vdata.optflags = aliasp[index].optflags & ~(MOPT_MAIN | MOPT_AUX);
   xlate_vdata.optflags2 = aliasp[index].optflags2;
   xlate_vdata.nident = aliasp[index].nident;
   xlate_vdata.identp = aliasp[index].ident;

   if ( (vflags & SEC_AUX) && (aliasp[index].nident > 1) ) 
      xlate_vdata.ident = xlate_vdata.identp[1] = ident = aliasp[index].ident[1];
   else
      xlate_vdata.ident = xlate_vdata.identp[0] = ident = aliasp[index].ident[0];

   found = 0; vdata = 0xff;
   for ( j = 0; j <= 0xff; j++ ) {
      xlate_vdata.vdata = j;
      xlate_vdata.vflags = 0;
      if ( aliasp[index].xlate_func(&xlate_vdata) != 0 ) {
         fprintf(stderr, "Function error.\n");
         return 1;
      }
      if ( func == xlate_vdata.func && vflags == (xlate_vdata.vflags & ~SEC_FAULT) ) {
         vdata = j;
         found = 1;
         break;
      }
   }
   if ( !found ) {
      fprintf(stderr, "Function and/or Flag mismatch for module type %s.\n",
         lookup_module_name(aliasp[index].modtype));
      return 1;
   }

   send_virtual_cmd_data(0, vdata, RF_SEC, (ident & 0xffu), ((ident & 0xff00u) >> 8), 0, 0);

   return 0;
}

/*---------------------------------------------------------------------+
 | Parse the flags list and return a long bitmap, with bit 0 = flag 1, |
 | bit 1 = flag 2, etc.                                                |
 +---------------------------------------------------------------------*/
int flaglist2banks ( unsigned long *flagbank, int banks, char *str )
{

   char           buffer[256];
   char           errmsg[80];
   char           *tail, *sp;
   int            flagmax = 32 * banks;

   unsigned char  flist[32 * banks];
   int            j, ustart, flag;


   for ( j = 0; j < 32 * banks; j++ )
      flist[j] = 0;

   for ( j = 0; j < banks; j++ )
      flagbank[j] = 0;

   ustart = 0; tail = str;
   while ( *(get_token(buffer, &tail, "-,", 255)) ) {
      flag = (int)strtol(buffer, &sp, 10);
      if ( *sp ) {
         sprintf(errmsg, "Invalid char '%c' in flags list.", *sp);
         store_error_message(errmsg);
         return 1;
      }
      if ( flag < 1 || flag > flagmax ) {
         sprintf(errmsg, "Flag number %d outside range 1-%d.", flag, flagmax);
         store_error_message(errmsg);
         return 1;
      }

      if ( *tail == ',' || *tail == '\0' ) {
         if ( ustart ) {
            for ( j = ustart; j <= flag; j++ )
               flist[j-1] = 1;
            ustart = 0;
            continue;
         }
         else {
            flist[flag-1] = 1;
            continue;
         }
      }
      else {
         ustart = flag;
      }
   }

   for ( j = 0; j < (int)sizeof(flist); j++ ) {
      if ( flist[j] )
        flagbank[j / 32] |= 1UL << (j % 32);
   }

   return 0;
}

/*---------------------------------------------------------------------+
 | Display list of state flags.  Omit display of "not" flags.          |
 +---------------------------------------------------------------------*/
int c_stateflaglist ( int argc, char *argv[] )
{
   int j;
   char buffer[32];

   printf("Hu = Housecode|single_unit or Alias label.\n");
   printf("Prefix a flag with \"not\" to negate it.\n");

   for ( j = 0; j < num_stflags; j++ ) {
      if ( strncmp(stflags[j].label, "not", 3) == 0 )
         continue;
      strncpy2(buffer, stflags[j].label, stflags[j].length);
      strcat(buffer, "Hu");
      printf("%s\n", buffer);
   }
   for ( j = 0; j < num_virtflags; j++ ) {
      if ( strncmp(virtflags[j].label, "not", 3) == 0 )
         continue;
      strncpy2(buffer, virtflags[j].label, virtflags[j].length);
      strcat(buffer, "Hu");
      printf("%s\n", buffer);
   }
   return 0;
}

int c_masklist ( int argc, char *argv[] )
{
   display_env_masks();
   return 0;
}

int get_env_funcmask ( int index, char **query, int *mask )
{
   if ( !heyumaskval[index].query )
      return -1;

   *query = heyumaskval[index].query;
   *mask = heyumaskval[index].mask;

   return 0;
}

int get_env_flagmask ( int index, char **query, int *mask )
{
   if ( !heyusecmaskval[index].query )
      return -1;

   *query = heyusecmaskval[index].query;
   *mask = heyusecmaskval[index].mask;

   return 0;
}

   
   
