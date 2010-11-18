


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


