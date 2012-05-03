/*----------------------------------------------------------------------------+
 |                                                                            |
 |              State and Script functions for HEYU                           |
 |            Copyright 2004-2010 Charles W. Sullivan                         |
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



#include <time.h>

/* Structure of Heyu State Table */

typedef struct {
   unsigned char  reset;
   unsigned char  lastunit;
   unsigned char  lastcmd;
   unsigned int   addressed;     /* X10 bitmap */
   unsigned int   exclusive;     /* Exclusive bitmap */
   unsigned int   sticky;        /* Stickey address bitmap */
   unsigned int   vaddress;      /* Virtual address bitmap */
   unsigned int   squelch;       /* Squelch address bitmap */
   unsigned int   lastactive;    /* Active mask for last command */
   unsigned char  xconfigmode;   /* Mode for xconfig */
   unsigned int   state[NumStates]; /* 0 = OnState  1 = DimState  2 = ChgState 3 = LightsOnState 4 = AlertState 5 = AuxAlertState */
   unsigned int   launched;      /* Launched script bitmap */
//   unsigned int   statusflags;   /* Status pending flags */
   unsigned int   rfxlobat;      /* RFXSensor low battery flags */
   unsigned long  vflags[16];    /* Virtual data flags */
   unsigned char  dimlevel[16];  /* Current dim level (or security data) */
   unsigned char  memlevel[16];  /* "memory" level - preset modules (or Aux security data) */
   unsigned long  vident[16];    /* ident - virtual modules */
   time_t         timestamp[16]; /* time of last update */
   long           longdata;      /* Data storage for Unit 0 */
   int            rcstemp;       /* RCS Temperature */
   unsigned long  rfxmeter[16];  /* RFXMeter data */
   unsigned char  grpmember[16]; /* bits 0-3 are bitmapped ext3 group memberships */
   unsigned char  grpaddr[16];   /* abs or relative group address */
   unsigned char  grplevel[16][4]; /* ext3 levels for groups 0-3 */
} x10hcode_t;

struct x10global_st {
   time_t         filestamp; /* Time of last file write */
   unsigned long  longvdata;
   unsigned long  longvdata2;
   unsigned long  longvdata3;
   unsigned char  sigcount;   /* Electrisave/Owl signal counter */
   long           interval;
   unsigned char  lastvtype;
   unsigned char  lasthc;
   unsigned int   lastaddr;
//   unsigned long  timer_count[NUM_USER_TIMERS + 1];
   long           timer_count[NUM_USER_TIMERS + 1];
   unsigned long  flags[NUM_FLAG_BANKS];  /* Common flags */
   unsigned long  sflags;          /* State and Security flags */
   unsigned long  vflags;          /* Global vflags */
   unsigned long  longdata_flags;  /* Unit 0 has valid data */
   unsigned long  rcstemp_flags;   /* RCS Temperature has valid data */
   unsigned char  dawndusk_enable; /* Set when config has Lat and Long.*/
   int            isdark_offset;
   time_t         utc0_macrostamp; /* Most recent timer macro exec. */
   time_t         utc0_dawn;
   time_t         utc0_dusk;
   time_t         utc0_tomorrow;
   x10hcode_t     x10hcode[16];
   unsigned long  data_storage[MAXDATASTORAGE];
   unsigned short rfxdata[8][32];
   unsigned long  rfxflags[8];
   unsigned short counter[32 * NUM_COUNTER_BANKS];
   unsigned long  czflags[NUM_COUNTER_BANKS];
   unsigned long  tzflags[NUM_TIMER_BANKS];
   unsigned int   hailstate;
};

int set_globsec_flags(unsigned char);
char *display_armed_status(void);
int clear_tamper_flags(void);
int identify_sent(unsigned char *, int, unsigned char *);
char *translate_rf_sent(unsigned char *, int *);
int set_counter(int, unsigned short, unsigned char);
char *translate_counter_action(unsigned char *);
int find_powerfail_scripts(unsigned char);
int find_rfflood_scripts(void);
int find_lockup_scripts(void);
char *display_variable_aux_data(unsigned char *);
int launch_script_cmd(unsigned char *);
char *display_binbuffer(unsigned char *);
char *translate_other(unsigned char *, int, unsigned char *);

