
/*----------------------------------------------------------------------------+
 |                                                                            |
 |                    Enhanced HEYU Functionality                             |
 |             Copyright 2002-2008 Charles W. Sullivan                        |
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
#include <ctype.h>
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif

#ifdef TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#include <syslog.h>

#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#include "x10.h"
#include "process.h"
#include "x10state.h"

#if defined(RAND_MAX)
#define RandMax  RAND_MAX
#elif defined(RANDOM_MAX)
#define RandMax RANDOM_MAX
#else
#define RandMax LONG_MAX
#endif

extern int tty, tty_aux;
extern int line_no;
extern int sptty, i_am_relay, i_am_aux, i_am_state;
extern int heyu_parent;
extern int verbose;
extern int xwrite(), xread(), exread(), sxread(), check4poll();
extern int is_modem_support(void);
extern CONFIG  config;
extern CONFIG  *configp;

extern struct x10global_st x10global;
extern x10hcode_t *x10state;


unsigned char alert_cmd[6];
unsigned char alert_ack[3];

int special_func = 0;

char *typename[] = {"???", "Scene", "Usersyn"};

#if 0  /* Reference - Flags for parse_addr() function */
#define A_VALID   1
#define A_HCODE   2
#define A_BMAP    4
#define A_PLUS    8
#define A_MINUS  16
#define A_ALIAS  32
#define A_DUMMY  64
#define A_MULT  128
#define A_STAR  256
#endif /* Reference */
         
/* Arguments for the various commands */
static char *helparg[] = {
   "[H]",                /* 0 HC optional */
   "H",                  /* 1 HC only */
   "Hu",                 /* 2 HC | single unit */
   "HU",                 /* 3 HC | unit string */
   "HU <level>",         /* 4 Dim/Bright level*/
   "HU <level>",         /* 5 Preset dim level*/
   "<level>",            /* 6 Preset dim level only */
   "HU <level>",         /* 7 Extended Preset level*/
   "<T/F> HU <Data>",    /* 8 Arbitrary extended code command (hex)*/
   "<command ...>",      /* 9 Send HC|Funct only, no address */
   "xx xx xx ...",       /* 10 Arbitrary hex bytes */
   "H <\"text\">",       /* 11 Arbitrary quoted text message */
   "",                   /* 12 No arguments */
   "[<command>]",        /* 13 For help command */
   "N.NNN",              /* 14 Pause delay */
   "HU [HU [...]]",      /* 15 Multiple addresses */
   "HU <command>",       /* 16 For Turn command */
   "<query_command>",    /* 17 Special functions */
   "HU <linked>",        /* 18 Preset for macros */
   "H <mode 0-3>",       /* 19 Ext status_ack mode */
   "<\"text\">",         /* 20 Message to log file */
   "NNN",                /* 21 Delay in minutes */
   "H[U] <count>",       /* 22 RF Dim/Bright */
   "xx xx <count>",      /* 23 Two hex bytes */
   "HU <count>",         /* 24 RF Dimbo */
   "H[u]",               /* 25 State commands */
   "n[,n...]",           /* 26 Flag set/clear commands */
   "H[U]",               /* 27 Status flag clear command */
   "HU <byte>",          /* 28 Virtual data */
   "N <hh:mm:ss>",       /* 29 Countdown timer */
   "xxxx xxxx ...",      /* 30 Multiple 16-bit hex words */
   "<parameters>",       /* 31 Special for UX17/23 command */
   "HU g <level>",       /* 32 Include in group at level*/
   "HU g[,g,...]",       /* 33 Delete from group(s) */
   "H G",                /* 34 Execute group[.subgroup] functions */
   "HU G",               /* 35 Include in group[.subgroup] */
   "H g[,g,...]",        /* 36 Delete from group(s) */
   "HU <level> <ramp>",  /* 37 Extended Preset level with ramp */
   "N <count>",          /* 38 Set counter */
   "N",                  /* 39 Increment or Decrement counter */
   "[parameters]",       /* 40 Arm system */
   "N [MIN] MAX",        /* 41 Random countdown timer */
   "[MIN] MAX",          /* 42 Random delay */
};

static char *options[] = {
    "Usage: heyu [options] <command>   (Run 'heyu help' for commands)\n",
    " [Options]",
    "    -v             Enable verbose mode",
    "    -c <pathname>  Specify full configuration file pathname",
    "    -s <pathname>  Specify full schedule file pathname",
    "    -0 ... -9      Config file is in subdirectory /0 ... /9",
    "                   of standard location, e.g., $HOME/.heyu/3/x10config",
};

/* Descriptions of "administrative" commands */
static char *helpadmin[][3] = {
    {"info","","Display CM11A registers, clock, and upload status"},
    {"help","[category|command]","This screen [category or command]"},
    {"help","help","Explain help usage"},
    {"start","","Start Heyu background processes (daemons)"},
    {"stop","","Stop Heyu background processes"},
    {"restart","","Reconfigure running Heyu background processes"},
    {"engine","","Start the (optional) Heyu state engine daemon"},
    {"aux","","Start the (optional) Heyu auxiliary RF input daemon"},
    {"monitor","","Monitor X10 activity (end with <BREAK>)"},
    {"date","","Return date in date(1) input format"},
    {"erase","","Zero CM11A EEPROM, erasing all events and macros"},
    {"syn","","Display built-in synonyms for direct commands"},
    {"<label>","","Execute a scene or usersyn defined in the config file"},
    {"show","<option>","(Enter 'heyu show' to see options)"},
    {"script_ctrl","<option>","Launch scripts disable|enable"},
    {"initstate","[H[U]]","Zero entire state table or just for address H[U]"},
    {"initothers","","Zero cumulative address table"},
    {"reset","[H]","Reset interface to housecode H or default"},
    {"setclock","","Set CM11A clock to system clock (per schedule)"},
    {"readclock","","Display CM11A and system clocks (per schedule)"},
    {"newbattery","","Reset CM11A battery timer to zero"},
    {"purge","","Cancel pending CM11A delayed macros"},
    {"clear","","Clear CM11A registers"},
    {"list","","Display Lock, Spool, and System base directory names"},
    {"upload","[check|croncheck]","Upload schedule to CM11A or check schedule file"},
    {"upload","status|cronstatus","Display status of uploaded schedule"},
    {"catchup","","Emulate uploaded Timers from 0:00 to current time today"},
    {"trigger","Hu on|off","Emulate Trigger in uploaded schedule"},
    {"macro","<label>","Emulate Macro in uploaded schedule"},
    {"utility","<option>","(Enter 'heyu utility' to see options)"},
    {"logmsg","<\"text\">","Display text message in log file and/or monitor"},
    {"wait","[timeout]","Wait until macro execution is completed"},
    {"cm10a_init","","Initialize CM10A interface. (For CM10A only!)"},
    {"restore_groups","","Restore extended group and xconfig settings"},
    {"logtail","[N]","Display tail [N lines] of log file"},
    {"sec_emu","Hu <func> <flags>","Emulate an X10 Security signal"},
    {"launch","<parameters>","Launch script. Enter 'heyu launch' for usage"},
    {"conflist","","Display list of all configuration directives"},
    {"modlist","","Display list of all module types"},
    {"stateflaglist","","Display list of all state flags"},
    {"masklist","","Display lists of all environment variable masks"},
    {"webhook","<parameters>","Enter 'heyu webhook' for usage"}, 
    {"version","","Display the Heyu version and exit"},
    {NULL, NULL, NULL}
};

static char *helpstate[][3] = {
    {"enginestate","","Display 1 if state engine is running, else 0"},
    {"armedstate","","Bitmap: 1 = armed, 2 = home, 4 = armpending, 8 = tamper"},
    {"sensorfault","","Bitmap: 1 = low battery, 2 = inactive, 8 = tamper"},
    {"flagstate","n","Boolean state of flag n"},
    {"nightstate","","Boolean state of night flag"},
    {"darkstate","","Boolean state of dark flag"},
    {"sunstate","","Bitmap: 1 = night, 2 = dark"},
    {"spendstate","H[u]","Status-pending bitmap of H or Boolean Hu"},
    {"onstate","H[u]","On-state bitmap of H or Boolean Hu"},
    {"offstate","H[u]","On-state complement bitmap of H or Boolean Hu"},
    {"dimstate","H[u]","Dim-state bitmap of H or Boolean Hu"},
    {"fullonstate","H[u]","FullOn-state bitmap of H or Boolean Hu"},
    {"chgstate","H[u]","Changed-state bitmap of H or Boolean Hu"},
    {"alertstate","H[u]","Alert-state bitmap of H or Boolean Hu"},
    {"clearstate","H[u]","Clear-state bitmap of H or Boolean Hu"},
    {"auxalertstate","H[u]","AuxAlert-state bitmap of H or Boolean Hu"},
    {"auxclearstate","H[u]","AuxClear-state bitmap of H or Boolean Hu"},
    {"addrstate","H[u]","Addressed-state bitmap of H or Boolean Hu"},
    {"activestate","H[u]","Active-state bitmap of H or Boolean Hu"},
    {"inactivestate","H[u]","Inactive-state bitmap of H or Boolean Hu"},
    {"lobatstate","H[u]","Low_Battery-state bitmap of H or Boolean Hu"},
    {"validstate","H[u]","Valid function state bitmap of H or Boolean Hu"},
    {"statestr","H","State mini-bitmaps of all units as ASCII string"},
    {"dimlevel","Hu","Brighness level of module Hu as 0-100%"},
    {"rawlevel","Hu","Native level (0-210, 1-32, or 0-63) of module Hu"},
    {"memlevel","Hu","Stored level 0-100% for module Hu with memory"},
    {"rawmemlevel","Hu","Stored native level for module Hu with memory"},
    {"heyu_state","Hu","Heyu script environment state bitmap (as integer)"},
    {"heyu_rawstate","Hu","Heyu script raw environment state bitmap (as integer)"},
    {"heyu_vflagstate","Hu","Heyu script vFlag environment state bitmap (as integer)"},
#if 0
#if defined(HASRFXS) || defined(HASRFXM)
    {"rfxflag_state","Hu","RFXsensor/RFXmeter flag state bitmap (as integer)"},
#endif
#ifdef HASORE
    {"oreflag_state","Hu","Oregon (Electrisave, Owl) flag state bitmap (as integer)"},
#endif
#ifdef HASDMX
    {"dmxflag_state","Hu","Digimax flag state bitmap (as integer)"},
#endif
#endif
    {"xtend_state","Hu","Xtend script environment state bitmap (as integer)"},
    {"rcstemp","H","RCS compatible temperature (stored value)"},
    {"fetchstate","","See man page heyu(1)"},
    {NULL, NULL, NULL}
};

#ifdef HASRFXS
static char *helprfxsensor[][3] = {
    {"rfxtemp","Hu","Temperature"},
    {"rfxrh","Hu","Relative Humidity"},
    {"rfxbp","Hu","Barometric Pressure"},
    {"rfxpot","Hu","Potentiometer setting"},
    {"rfxvs","Hu","Supply Voltage"},
    {"rfxvad","Hu","A/D Voltage"},
    {"rfxvadi","Hu","Internal A/D Voltage"},
    {"rfxlobat","Hu","Low Battery state (Boolean)"},
    {"rfxtemp2","Hu","Second Temperature"},
    {NULL, NULL, NULL}
};
#endif /* HASRFXS */

#ifdef HASRFXM
static char *helprfxmeter[][3] = {
    {"rfxpower","Hu","Watt-Hour meter reading"},
    {"rfxpanel","[n]","Total Watt-Hours for Power Panel [n]"},
    {"rfxwater","Hu","Water meter reading"},
    {"rfxgas","Hu","Gas meter reading"},
    {"rfxpulse","Hu","Pulse meter reading"},
    {"rfxcount","Hu","Raw counter of any meter"},
    {NULL, NULL, NULL}
};
#endif /* HASRFXM */

#ifdef HASDMX
static char *helpdigimax[][3] = {
    {"dmxtemp","Hu","DigiMax current temperature (C)"},
    {"dmxsetpoint","Hu","DigiMax setpoint temperature (C)"},
    {"dmxstatus","Hu","DigiMax On/Off status (1 = On)"},
    {"dmxmode","Hu","DigiMax Heat/Cool mode (1 = Heat)"},
    {NULL, NULL, NULL}
};
#endif /* HASDMX */

#ifdef HASORE
static char *helporegon[][3] = {
    {"oretemp","Hu","Oregon sensor Temperature"},
    {"orerh","Hu","Oregon sensor Relative Humidity"},
    {"orebp","Hu","Oregon sensor Barometric Pressure"},
    {"orewindavsp","Hu","Oregon sensor Wind Average Speed"},
    {"orewindsp","Hu","Oregon sensor Wind Instantaneous Speed"},
    {"orewinddir","Hu","Oregon sensor Wind Direction"},
    {"orerainrate","Hu","Oregon sensor Rainfall Rate"},
    {"oreraintot","Hu","Oregon sensor Total Rainfall"},
    {"ore_emu","Hu <func> <data>","Enter data in Oregon emulation module"},
    {"elscurr","Hu","Electrisave CM113 Current"},
    {"owlpower","Hu","Owl CM119 Power"},
    {"owlenergy","Hu","Owl CM119 Energy"},
    {NULL, NULL, NULL}
};
#endif /* HASORE */

/* Descriptions of direct and uploaded macro commands */
static char *helpdirect[] = {
   /*  0 */ "Turn units ON",
   /*  1 */ "Turn units OFF",
   /*  2 */ "Turn All Lights ON",
   /*  3 */ "Turn All Lights OFF (**)",
   /*  4 */ "Turn All Units OFF",
   /*  5 */ "Dim units by <level> (1-22)",
   /*  6 */ "Dim units by <level> (1-22) after full bright",
   /*  7 */ "Brighten units by <level> (1-22)",
   /*  8 */ "Brighten units by <level> (1-22) after full bright",
   /*  9 */ "Request ON/OFF status (two-way modules)",
   /* 10 */ "Status Acknowledge ON",
   /* 11 */ "Status Acknowledge OFF",
   /* 12 */ "Preset units to <level> (1-32)",
   /* 13 */ "Preset to <level> (1-32) (function only)",
   /* 14 */ "Extended Preset <level> (0-63) (LM14A)",
   /* 15 */ "Extended All Units ON (LM14A)",
   /* 16 */ "Extended All Units OFF (LM14A)",
   /* 17 */ "Extended Status Request (LM14A)",
   /* 18 */ "Extended Status Acknowledge",
   /* 19 */ "Extended command - general",
   /* 20 */ "Extended command - general, await ack",
   /* 21 */ "Send arbitrary hex bytes as addresses",
   /* 22 */ "Send Housecode|Units addresses only",
   /* 23 */ "Send command function only",
   /* 24 */ "Hail other devices",
   /* 25 */ "Hail Acknowledge",
   /* 26 */ "Send quoted text (special protocol)",
   /* 27 */ "Data Transfer (function code 0xC)",
   /* 28 */ "Send All_Units_Off to All Housecodes",
   /* 29 */ "Extended Turn Units Full ON (LM14A)",
   /* 30 */ "Extended Turn Units Full OFF (LM14A)",
   /* 31 */ "This screen, or specific command help",
   /* 32 */ "Display synonyms for direct commands",
   /* 33 */ "Hail other devices, await ack",
   /* 34 */ "Legacy status command as above",
   /* 35 */ "Legacy preset command as above",
   /* 36 */ "Pause for N.NNN seconds",
   /* 37 */ "Turn Units 1-16 ON",
   /* 38 */ "Change state on|off|up|down [vv]",
   /* 39 */ "Legacy turn command as above",
   /* 40 */ "Request temperature (RCS compatible)",
   /* 41 */ "Send bytes directly to interface",
   /* 42 */ "Limited Preset for macros - see manpage",
   /* 43 */ "Extended config auto status mode (LM14A)",
   /* 44 */ "Request RCS compatible status",
   /* 45 */ "Delay for NNN minutes, NNN = 0-240",
   /* 46 */ "Pause until next tick of system clock",
   /* 47 */ "Transmit RF On",
   /* 48 */ "Transmit RF Off",
   /* 49 */ "Transmit RF Dims [after On]",
   /* 50 */ "Transmit RF Brights [after On]",
   /* 51 */ "Transmit RF All Lights On",
   /* 51 */ "Transmit RF All Lights Off (**)",
   /* 53 */ "Transmit RF All Units Off",
   /* 54 */ "Transmit RF Arbitrary 2-byte hex code",
   /* 55 */ "Reset CM17A device",
   /* 56 */ "Transmit RF Dims after Off",   
   /* 57 */ "Transmit RF Dims [after On] (#)",
   /* 58 */ "Transmit RF Brights [after On] (#)",
   /* 59 */ "Transmit RF Dims after Off (#)",
   /* 60 */ "Transmit RF Arbitrary 2-byte hex code (#)",
   /* 61 */ "Open shutter to level (0-25), enforce limit",
   /* 62 */ "Set limit (0-25) and open shutter to limit",
   /* 63 */ "Open shutter to level (0-25) and cancel limit",
   /* 64 */ "Open all shutters fully and cancel limit",
   /* 65 */ "Close all shutters fully",
   /* 66 */ "Extended module power-up signal (LM14A)",
   /* 67 */ "Set one or more flags (@)",
   /* 68 */ "Clear one or more flags (@)",
   /* 69 */ "Clear status-pending flags (@)",
   /* 70 */ "Write data (0-255) to virtual module (@)",
   /* 71 */ "Set countdown timer N to hh:mm:ss (@)",
   /* 72 */ "Reset all countdown timers to zero (@)",
   /* 73 */ "Transmit RF multiple 16-bit hex words",
   /* 74 */ "Clear security tamper flags (@)",
   /* 75 */ "Transmit RF multiple 16-bit hex words (#)",
   /* 76 */ "Special for UX23A/UX17A - see manpage",
   /* 77 */ "Special for UX23A/UX17A - see manpage (#)",
   /* 78 */ "Sleep for N.NNN seconds",
   /* 79 */ "Include in Group (G = 0-3) at current level",
   /* 80 */ "Remove HU from one or more Groups (g = 0-3)",
   /* 81 */ "Execute extended Group G function (LM14A)",
   /* 82 */ "Extended Group G Status Request (LM14A)",
   /* 83 */ "Remove H from one or more Groups (g = 0-3)",
   /* 84 */ "Include in Group (g = 0-3) at <level>",
   /* 85 */ "Extended Group G Off (**)",
   /* 86 */ "Extended Group G Dim one level (**)",
   /* 87 */ "Extended Group G Bright one level (**)",
   /* 88 */ "Extended Preset <level> (0-63) <ramp> (0-3)",
   /* 89 */ "Set counter N to <count> (0-64K) (@)",
   /* 90 */ "Increment counter N by 1 (@)",
   /* 91 */ "Decrement counter N by 1 (@)",
   /* 92 */ "Decrement counter N and skip if Zero (@)",
   /* 93 */ "Decrement counter N and skip if Greater than Zero (@)",
   /* 94 */ "Arm system [home|away] [min|max] (@)",
   /* 95 */ "Disarm system (@)",
   /* 96 */ "Delay random time [MIN-]MAX minutes (0-240)",
   /* 97 */ "Set timer N to random [MIN-]MAX hh:mm:ss (@)",
   /* 98 */ "Write mem data (0-255) to virtual module (@)",
   /* 99 */ "Null command - does nothing (@)",
   /*100 */ "Dec counter N and skip if Non-Zero or Initial Zero (@)",
   /*101 */ "Null command - does nothing",
   /*102 */ "Emulate Status Acknowledge for 1-way modules",
};

   
void display_helpnotes ( void )
{
   printf("\n  (*)  Not available for use in uploaded macros.\n");
   printf("  (**) Many lamp modules do NOT support this command.\n");
#ifdef HASCM17A   
   printf("  (#)  Fast CM17A command - see man x10cm17a(5) for configuration.\n");
#endif
   printf("  (@)  Ignored unless state engine is running.\n");

   return;
}

void display_manpage_list ( void )
{
   printf("\n Man pages:\n heyu(1), x10config(5), x10sched(5), x10scripts(5), x10aux(5)");
#ifdef HASCM17A   
   printf(", x10cm17a(5)");
#endif
#ifdef HASRFXS
   printf(", x10rfxsensors(5)");
#endif
#ifdef HASRFXM
   printf(", x10rfxmeters(5)");
#endif
#ifdef HASDMX
   printf(", x10digimax(5)");
#endif
#ifdef HASORE
   printf(", x10oregon(5)");
#endif
#ifdef HASKAKU
   printf(", x10kaku(5)");
#endif
   printf(".\n\n");
   return;
}


/* Flags for x10command table below */
#define  F_ALL  0x00000001 /* "All" command; no unit code required on input */
#define  F_DED  0x00000002 /* Display extended code data byte in report */
#define  F_NOA  0x00000004 /* Suppress HC|Unit address byte transmission */
#define  F_BRI  0x00000008 /* Brighten before dimming */
#define  F_FAD  0x00000010 /* Force HC|Unit address byte(s) transmission */
#define  F_ARB  0x00000020 /* Use special format for arbitrary extended code */
#define  F_NMA  0x00000040 /* Unavailable as an uploadable macro */
#define  F_HID  0x00000080 /* Hidden command */
#define  F_STA  0x00000100 /* Status/xStatus Request (await status) */
#define  F_NUM  0x00000200 /* Arguments are all numbers */
#define  F_HLP  0x00000400 /* For help menu only - not a CM11A command */
#define  F_NMAI 0x00000800 /* Unavailable and ignored for uploaded macro */
#define  F_TRN  0x00001000 /* For Turn command */
#define  F_SPF  0x00002000 /* For special function processing */
#define  F_MAC  0x00004000 /* Special version for uploaded macros */
#define  F_FIR  0x00008000 /* CM17A (Firecracker) command */
#define  F_FFM  0x00010000 /* CM17A "fast" mode */
#define  F_SHU  0x00020000 /* Extended Type 0 (Marmitek SW10) shutter commands */
#define  F_EXT  0x00040000 /* Extended Type 3 (LM14A, AM14A) commands */
#define  F_FLSC 0x00080000 /* Flag set and clear commands */
#define  F_GRP  0x00100000 /* Extended code group commands */
#define  F_ONN  0x00200000 /* On required before brightening */
#define  F_INT  0x00400000 /* Internal - no PLC interface required */
#define  F_DUM  0x00800000 /* No PLC interface required */

#define  F_RFS (F_FIR | F_NMA)
#define  F_RFF (F_FIR | F_NMA | F_FFM)
#define  F_FLG (F_FLSC | F_NMA | F_NUM)
#define  F_XGP (F_GRP | F_EXT)

/*---------------------------------------------------------------------+
 | Master table of commands and synonyms.  The first of any grouping   |
 | having all fields identical except for the label is considered the  |
 | command; subsequent entries in the grouping are treated as built-in |
 | synonyms.                                                           |
 +---------------------------------------------------------------------*/
/* Names for arbitrary extended code functions */
#define XARBNAME   "xfunc"
#define XARBNAMEW  "xfuncw"
  
static struct cmd_list {
   char          *label;
   unsigned char code;
   int           length;   /* Length as a macro */
   int           minargs;
   int           maxargs;
   unsigned long flags;
   unsigned char subcode;  /* Type|command for extended code commands */
   unsigned char subsubcode;
   unsigned char argtype;
   unsigned char help;
} x10command[] = {
   {"on",               2, 3, 1, 1, 0,                0, 0, 3, 0 },   /* Simple ON command */
   {"turnon",           2, 3, 1, 1, 0,                0, 0, 3, 0 },   /* Simple ON command */
   {"off",              3, 3, 1, 1, 0,                0, 0, 3, 1 },   /* Simple OFF command */
   {"turnoff",          3, 3, 1, 1, 0,                0, 0, 3, 1 },   /* Simple OFF command */
   {"bright",           5, 4, 2, 2, 0,                0, 0, 4, 7 },   /* Brighten command */
   {"turnup",           5, 4, 2, 2, 0,                0, 0, 4, 7 },   /* Brighten command */
   {"up",               5, 4, 2, 2, 0,                0, 0, 4, 7 },   /* Brighten command */
   {"brighten",         5, 4, 2, 2, 0,                0, 0, 4, 7 },   /* Brighten command */
   {"tbright",          5, 4, 2, 2, F_NMA | F_HID,    1, 0, 4, 7 },   /* Brighten command - triggered */
   {"brightb",          5, 4, 2, 2, F_BRI,            0, 0, 4, 8 },   /* Brighten command with brighten first */
   {"bbright",          5, 4, 2, 2, F_BRI,            0, 0, 4, 8 },   /* Brighten command with brighten first */
   {"dim",              4, 4, 2, 2, 0,                0, 0, 4, 5 },   /* Dim command without brighten first */
   {"turndown",         4, 4, 2, 2, 0,                0, 0, 4, 5 },   /* Dim command without brighten first */
   {"down",             4, 4, 2, 2, 0,                0, 0, 4, 5 },   /* Dim command without brighten first */
   {"dimb",             4, 4, 2, 2, F_BRI,            0, 0, 4, 6 },   /* Dim command with brighten first */
   {"bdim",             4, 4, 2, 2, F_BRI,            0, 0, 4, 6 },   /* Dim command with brighten first */
   {"obdim",            4, 4, 2, 2, F_BRI | F_ONN,    0, 0, 4, 6 },   /* Dim command with on, brighten first */
   {"tdim",             4, 4, 2, 2, F_NMA | F_HID,    1, 0, 4, 5 },   /* Dim command - triggered */
   {"lightson",         1, 3, 1, 1, F_ALL,            0, 0, 1, 2 },   /* All_Lights_On */
   {"lights_on",        1, 3, 1, 1, F_ALL,            0, 0, 1, 2 },   /* All_Lights_On */
   {"all_lights_on",    1, 3, 1, 1, F_ALL,            0, 0, 1, 2 },   /* All_Lights_On */
   {"alllightson",      1, 3, 1, 1, F_ALL,            0, 0, 1, 2 },   /* All_Lights_On */
   {"lightsoff",        6, 3, 1, 1, F_ALL,            0, 0, 1, 3 },   /* All_Lights_Off */
   {"lights_off",       6, 3, 1, 1, F_ALL,            0, 0, 1, 3 },   /* All_Lights_Off */
   {"all_lights_off",   6, 3, 1, 1, F_ALL,            0, 0, 1, 3 },   /* All_Lights_Off */
   {"alllightsoff",     6, 3, 1, 1, F_ALL,            0, 0, 1, 3 },   /* All_Lights_Off */
   {"allon",           26,-1, 1, 1, F_ALL,            0, 0, 1,37 },   /* Units 1-16 On  */
   {"all_on",          26,-1, 1, 1, F_ALL,            0, 0, 1,37 },   /* Units 1-16 On  */
   {"all_units_on",    26,-1, 1, 1, F_ALL,            0, 0, 1,37 },   /* Units 1-16 On  */
   {"allunitson",      26,-1, 1, 1, F_ALL,            0, 0, 1,37 },   /* Units 1-16 On  */
   {"units_on",        26,-1, 1, 1, F_ALL,            0, 0, 1,37 },   /* Units 1-16 On  */
   {"unitson",         26,-1, 1, 1, F_ALL,            0, 0, 1,37 },   /* Units 1-16 On  */
   {"alloff",           0, 3, 1, 1, F_ALL,            0, 0, 1, 4 },   /* All_Units_Off  */
   {"all_off",          0, 3, 1, 1, F_ALL,            0, 0, 1, 4 },   /* All_Units_Off  */
   {"all_units_off",    0, 3, 1, 1, F_ALL,            0, 0, 1, 4 },   /* All_Units_Off  */
   {"allunitsoff",      0, 3, 1, 1, F_ALL,            0, 0, 1, 4 },   /* All_Units_Off  */
   {"units_off",        0, 3, 1, 1, F_ALL,            0, 0, 1, 4 },   /* All_Units_Off  */
   {"unitsoff",         0, 3, 1, 1, F_ALL,            0, 0, 1, 4 },   /* All_Units_Off  */
   {"turn",            27,-1, 1,-1, F_TRN,            0, 0,16,38 },   /* Turn command */
   {"preset",          10, 3, 2, 2, F_NMA,            0, 0, 5,12 },   /* Old Preset (not for macro) */
   {"preset_dim",      10, 3, 2, 2, F_NMA,            0, 0, 5,12 },   /* Old Preset (not for macro) */
   {"preset",          11, 3, 2, 2, F_NMA | F_HID,    0, 0, 5,12 },   /* Old Preset (not for macro) */
   {"preset_dim",      11, 3, 2, 2, F_NMA | F_HID,    0, 0, 5,12 },   /* Old Preset (not for macro) */
   {"mpreset",         10, 3, 2, 2, F_MAC,            0, 0,18,42 },   /* Limited old Preset for macros */
   {"macro_preset",    10, 3, 2, 2, F_MAC,            0, 0,18,42 },   /* Limited old Preset for macros */
   {"mpreset",         11, 3, 2, 2, F_MAC | F_HID,    0, 0,18,42 },   /* Limited old Preset for macros */
   {"macro_preset",    11, 3, 2, 2, F_MAC | F_HID,    0, 0,18,42 },   /* Limited old Preset for macros */
   {"preset_level",    10, 3, 1, 1, F_NOA | F_NUM,    0, 0, 6,13 },   /* Old Preset with no address byte */
   {"presetlevel",     10, 3, 1, 1, F_NOA | F_NUM,    0, 0, 6,13 },   /* Old Preset with no address byte */
   {"preset_level",    11, 3, 1, 1, F_NOA|F_NUM|F_HID, 0, 0, 6,13 },   /* Old Preset with no address byte */
   {"presetlevel",     11, 3, 1, 1, F_NOA|F_NUM|F_HID, 0, 0, 6,13 },   /* Old Preset with no address byte */
   {"status",          15, 3, 1, 1, F_STA,            0, 0, 3, 9 },   /* Status Request */
   {"status_req",      15, 3, 1, 1, F_STA,            0, 0, 3, 9 },   /* Status Request */
   {"statusreq",       15, 3, 1, 1, F_STA,            0, 0, 3, 9 },   /* Status Request */
   {"status_on",       13, 3, 1, 1, F_ALL,            0, 0, 3,10 },   /* Status On */
   {"statuson",        13, 3, 1, 1, F_ALL,            0, 0, 3,10 },   /* Status On */
   {"status_off",      14, 3, 1, 1, F_ALL,            0, 0, 3,11 },   /* Status Off */
   {"statusoff",       14, 3, 1, 1, F_ALL,            0, 0, 3,11 },   /* Status Off */
   {"status_emu",      48,-1, 1, 1, F_NMA|F_HID,      0, 0, 2,102},   /* Emulate Status Ack */
   {"hail",             8, 3, 0, 1, F_ALL,            0, 0, 0,24 },   /* Hail Request */
   {"hail_req",         8, 3, 0, 1, F_ALL,            0, 0, 0,24 },   /* Hail Request */
   {"hailw",            8, 3, 0, 1, F_ALL|F_STA|F_NMA, 0, 0, 0,33 },   /* Hail Request, wait for ack */
   {"hail_ack",         9, 3, 0, 1, F_ALL,            0, 0, 0,25 },   /* Hail Acknowledge */
   {"hailack",          9, 3, 0, 1, F_ALL,            0, 0, 0,25 },   /* Hail Acknowledge */
   {"data_xfer",       12, 3, 1, 1, F_ALL,            0, 0, 1,27 },   /* Extended data transfer (0xC) */
   {"dataxfer",        12, 3, 1, 1, F_ALL,            0, 0, 1,27 },   /* Extended data transfer (0xC) */

   {"xon",              7, 6, 1, 1, F_EXT | F_DED, 0xfe, 0, 3,29 },   /* Extended Full ON (LM14A) */
   {"xoff",             7, 6, 1, 1, F_EXT | F_DED, 0xfd, 0, 3,30 },   /* Extended Full OFF (LM14A) */
   {"xpreset",          7, 6, 2, 2, F_EXT | F_DED, 0x31, 0, 7,14 },   /* Extended Preset Dim (LM14A) */
   {"ext_preset",       7, 6, 2, 2, F_EXT | F_DED, 0x31, 0, 7,14 },   /* Extended Preset Dim (LM14A) */
   {"xdim",             7, 6, 2, 2, F_EXT | F_DED, 0x31, 0, 7,14 },   /* Extended Preset Dim (LM14A) */
   {"xpresetramp",      7, 6, 3, 3, F_EXT|F_DED|F_HID|F_NMA, 0x31, 0,37,88 }, /* Extended Preset Dim with ramp */
   {"xallon",           7, 6, 1, 1, F_EXT | F_ALL, 0x33, 0, 1,15 },   /* Extended All Units On */
   {"xall_on",          7, 6, 1, 1, F_EXT | F_ALL, 0x33, 0, 1,15 },   /* Extended All Units On */
   {"ext_all_on",       7, 6, 1, 1, F_EXT | F_ALL, 0x33, 0, 1,15 },   /* Extended All Units On */
   {"xalloff",          7, 6, 1, 1, F_EXT | F_ALL, 0x34, 0, 1,16 },   /* Extended All Units Off */
   {"xall_off",         7, 6, 1, 1, F_EXT | F_ALL, 0x34, 0, 1,16 },   /* Extended All Units Off */
   {"ext_all_off",      7, 6, 1, 1, F_EXT | F_ALL, 0x34, 0, 1,16 },   /* Extended All Units Off */
   {"xstatus",          7, 6, 1, 1, F_EXT | F_STA, 0x37, 0, 3,17 },   /* Extended Status Request */
   {"ext_status",       7, 6, 1, 1, F_EXT | F_STA, 0x37, 0, 3,17 },   /* Extended Status Request */
   {"ext_status_req",   7, 6, 1, 1, F_EXT | F_STA, 0x37, 0, 3,17 },   /* Extended Status Request */
   {"xconfig",          7, 6, 2, 2, F_EXT|F_ALL|F_DED, 0x3b, 0,19,43 },   /* Extended Auto Status Mode */
   {"xpowerup",         7, 6, 1, 1, F_EXT,         0x37, 1, 3,66 },   /* Extended Module PowerUp */
   {"xpup",             7, 6, 1, 1, F_EXT,         0x37, 1, 3,66 },   /* Extended Module PowerUp */
   {"xgrpadd",          7, 6, 2, 2, F_XGP,         0x30, 0,35,79 },   /* Extended add to group */ 
   {"xga",              7, 6, 2, 2, F_XGP,         0x30, 0,35,79 },   /* Extended add to group */ 
   {"xgrpaddlvl",       7, 6, 3, 3, F_XGP,         0x32, 0,32,84 },   /* Extended add to group at level */ 
   {"xgal",             7, 6, 3, 3, F_XGP,         0x32, 0,32,84 },   /* Extended add to group at level */ 
   {"xgrprem",          7, 6, 2, 2, F_XGP,         0x35, 0,33,80 },   /* Extended remove from group(s) */ 
   {"xgr",              7, 6, 2, 2, F_XGP,         0x35, 0,33,80 },   /* Extended remove from group(s) */ 
   {"xgrpremall",       7, 6, 2, 2, F_XGP | F_ALL, 0x35, 3,36,83 },   /* Extended delete from group(s) */
   {"xgra",             7, 6, 2, 2, F_XGP | F_ALL, 0x35, 3,36,83 },   /* Extended delete from group(s) */
   {"xgrpexec",         7, 6, 2, 2, F_XGP | F_ALL, 0x36, 0,34,81 },   /* Extended delete from group */
   {"xgx",              7, 6, 2, 2, F_XGP | F_ALL, 0x36, 0,34,81 },   /* Extended delete from group */
   {"xgrpexec",         7, 6, 2, 2, F_XGP|F_ALL|F_HID, 0x36, 2,34,81 },   /* (Needed for report.txt) */
   {"xgrpstatus",       7, 6, 2, 2, F_XGP | F_STA, 0x37, 2,35,82 },   /* Extended group status request */ 
   {"xgs",              7, 6, 2, 2, F_XGP | F_STA, 0x37, 2,35,82 },   /* Extended group status request */ 
   {"xgrpstatusreq",    7, 6, 2, 2, F_XGP | F_STA, 0x37, 2,35,82 },   /* Extended group status request */ 
   {"xgrpstatus",       7, 6, 2, 2, F_XGP|F_STA|F_HID, 0x37, 3,35,82 },  /* (Needed for report.txt) */ 
   {"xgrpoff",          7, 6, 2, 2, F_XGP|F_ALL|F_HID, 0x36, 1,34,85 },   /* Ext group Off - maybe!!!*/
   {"xgrpdim",          7, 6, 2, 2, F_XGP|F_ALL|F_HID, 0x3c, 0,34,86 },   /* Ext group Dim */
   {"xgrpdim",          7, 6, 2, 2, F_XGP|F_ALL|F_HID, 0x3c, 2,34,86 },  /* (Needed for report.txt) */
   {"xgrpbright",       7, 6, 2, 2, F_XGP|F_ALL|F_HID, 0x3c, 1,34,87 },   /* Ext group Bright */
   {"xgrpbright",       7, 6, 2, 2, F_XGP|F_ALL|F_HID, 0x3c, 3,34,87 },  /* (Needed for report.txt) */
   { XARBNAME,          7, 6, 3, 3, F_ARB,         0xff, 0, 8,19 },   /* Extended code - arbitrary */
   { XARBNAMEW,         7, 6, 3, 3, F_ARB|F_STA|F_NMA, 0xff, 0,8,20 }, /* Extended Status Req - arbitrary */

   {"address",         20,-1, 1,-1, F_NMA,            0, 0,15,22 },   /* Address bytes only (*)*/
   {"function",        21,-1, 1,-1, F_NOA,            0, 0, 9,23 },   /* HC|Func byte only */
   {"kill_all_hc",     24,-1, 0, 0, 0,                0, 0,12,28 },   /* Send All_Units_Off to all housecodes */
   {"pause",           25,-1, 1, 1, F_NMA|F_NUM|F_DUM, 0, 0,14,36 },   /* Pause seconds (locked) */
   {"sleep",           41,-1, 1, 1, F_NMA|F_NUM|F_DUM, 0, 0,14,78 },   /* Sleep seconds (unlocked) */
   {"delay",           32,-1, 1, 1, F_NMA|F_NUM|F_DUM, 0, 0,21,45 },   /* Delay minutes (unlocked) */
   {"rdelay",          45,-1, 1, 2, F_NMA|F_NUM|F_DUM, 0, 0,42,96 },   /* Random delay minutes (unlocked) */
   {"rcs_req",         28,-1, 1,-1, F_SPF | F_NMA,    0, 0,17,44 },   /* RCS-compatible general status query */
   {"temp_req",        28,-1, 1,-1, F_SPF | F_NMA,    0, 0,17,40 },   /* RCS-compatible temperature query */
   {"vdata",           37,-1, 2, 2, F_NMA|F_DUM,      0, 0,28,70 },   /* Virtual data */ 
   {"vdatam",          37,-1, 2, 2, F_NMA|F_DUM,      1, 0,28,98 },   /* Virtual memory data */ 

   {"command2cm11a",   31,-1, 1,-1, F_NMA|F_NUM|F_HID, 0, 0,10,41 },  /* Send bytes directly to interface */
   {"bytes2cm11a",     29,-1, 1,-1, F_NMA|F_NUM|F_HID, 0, 0,10,41 },  /* Send bytes directly to interface */
   {"sendbytes",       22,-1, 1,-1, F_NMA|F_NUM|F_HID, 0, 0,10,21 },   /* Send arbitrary hex bytes (*)*/
   {"sendtext",        23,-1, 2, 2, F_NMA|F_ALL|F_HID, 0, 0,11,26 },   /* Send (quoted) text message (*)*/
   {"pausetick",       33,-1, 0, 0, F_NMA|F_HID|F_DUM, 0, 0,12,46 },   /* Pause until next tick of sys clock */

   {"arm",             43,-1, 0, 4, F_NMA|F_NUM|F_DUM, SETFLAG, 0,40,94 },   /* Arm system */
   {"disarm",          43,-1, 0, 0, F_NMA|F_NUM|F_DUM, CLRFLAG, 0,12,95 },   /* Disarm system */

   {"setflag",         35,-1, 1, 1, F_FLG|F_DUM,     SETFLAG, 0,26,67 },   /* Set software flags */
   {"setflags",        35,-1, 1, 1, F_FLG|F_DUM,     SETFLAG, 0,26,67 },   /* Set software flags */
   {"clrflag",         35,-1, 1, 1, F_FLG|F_DUM,     CLRFLAG, 0,26,68 },   /* Clear software flags */
   {"clrflags",        35,-1, 1, 1, F_FLG|F_DUM,     CLRFLAG, 0,26,68 },   /* Clear software flags */
   {"settimer",        38,-1, 2, 2, F_NMA|F_NUM|F_DUM,     0, 0,29,71 },   /* Set countdown timer */
   {"setrtimer",       46,-1, 2, 3, F_NMA|F_NUM|F_DUM,     0, 0,41,97 },   /* Set random countdown timer */
   {"clrtimers",       39,-1, 0, 0, F_NMA|F_NUM|F_DUM,     0, 0,12,72 },   /* Reset all timers to zero */
   {"clrspend",        36,-1, 1, 1, F_NMA|F_DUM,           0, 0,27,69 },   /* Clear status flags */
   {"clrstatus",       36,-1, 1, 1, F_NMA|F_DUM,           0, 0,27,69 },   /* Clear status flags */
   {"clrtamper",       40,-1, 0, 0, F_NMA|F_NUM|F_DUM,     0, 0,12,74 },   /* Clear tamper flags */

   {"setcount",        42,-1, 2, 2, F_NMA|F_NUM|F_DUM, CNT_SET, 0,38,89 },   /* Set counter value */
   {"setc",            42,-1, 2, 2, F_NMA|F_NUM|F_DUM, CNT_SET, 0,38,89 },   /* Set counter value */
   {"inccount",        42,-1, 1, 1, F_NMA|F_NUM|F_DUM, CNT_INC, 0,39,90 },   /* Increment counter value */
   {"incc",            42,-1, 1, 1, F_NMA|F_NUM|F_DUM, CNT_INC, 0,39,90 },   /* Increment counter value */
   {"deccount",        42,-1, 1, 1, F_NMA|F_NUM|F_DUM, CNT_DEC, 0,39,91 },   /* Decrement counter value */
   {"decc",            42,-1, 1, 1, F_NMA|F_NUM|F_DUM, CNT_DEC, 0,39,91 },   /* Decrement counter value */
   {"null",            47,-1, 0, 0, F_NMA|F_DUM,           0, 0, 12,101 },   /* Null, does nothing */

   {"@arm",            43,-1, 0, 4, F_FLG|F_INT, SETFLAG, 0,40,94 },   /* Arm system */
   {"@disarm",         43,-1, 0, 0, F_FLG|F_INT, CLRFLAG, 0,12,95 },   /* Disarm system */

   {"@setflag",        35,-1, 1, 1, F_FLG|F_INT, SETFLAG, 0,26,67 },   /* Set software flags */
   {"@setflags",       35,-1, 1, 1, F_FLG|F_INT, SETFLAG, 0,26,67 },   /* Set software flags */
   {"@setf",           35,-1, 1, 1, F_FLG|F_INT, SETFLAG, 0,26,67 },   /* Set software flags */
   {"@clrflag",        35,-1, 1, 1, F_FLG|F_INT, CLRFLAG, 0,26,68 },   /* Clear software flags */
   {"@clrflags",       35,-1, 1, 1, F_FLG|F_INT, CLRFLAG, 0,26,68 },   /* Clear software flags */
   {"@clrf",           35,-1, 1, 1, F_FLG|F_INT, CLRFLAG, 0,26,68 },   /* Clear software flags */
   {"@settimer",       38,-1, 2, 2, F_NMA|F_NUM|F_INT, 0, 0,29,71 },   /* Set countdown timer */
   {"@sett",           38,-1, 2, 2, F_NMA|F_NUM|F_INT, 0, 0,29,71 },   /* Set countdown timer */
   {"@setrtimer",      46,-1, 2, 3, F_NMA|F_NUM|F_INT, 0, 0,41,97 },   /* Set random countdown timer */
   {"@clrtimers",      39,-1, 0, 0, F_NMA|F_NUM|F_INT, 0, 0,12,72 },   /* Reset all timers to zero */
   {"@clrt",           39,-1, 0, 0, F_NMA|F_NUM|F_INT, 0, 0,12,72 },   /* Reset all timers to zero */
   {"@clrspend",       36,-1, 1, 1, F_NMA|F_INT,       0, 0,27,69 },   /* Clear status pending flags */
   {"@clrstatus",      36,-1, 1, 1, F_NMA|F_INT,       0, 0,27,69 },   /* Clear status pending flags */
   {"@clrs",           36,-1, 1, 1, F_NMA|F_INT,       0, 0,27,69 },   /* Clear status pending flags */
   {"@vdata",          37,-1, 2, 2, F_NMA|F_INT,       0, 0,28,70 },   /* Virtual data */ 
   {"@vdatam",         37,-1, 2, 2, F_NMA|F_INT,       1, 0,28,98 },   /* Virtual memory data */ 

   {"@setcount",       42,-1, 2, 2, F_NMA|F_NUM|F_INT, CNT_SET, 0,38,89 },   /* Set counter value */
   {"@setc",           42,-1, 2, 2, F_NMA|F_NUM|F_INT, CNT_SET, 0,38,89 },   /* Set counter value */
   {"@inccount",       42,-1, 1, 1, F_NMA|F_NUM|F_INT, CNT_INC, 0,39,90 },   /* Increment counter value */
   {"@incc",           42,-1, 1, 1, F_NMA|F_NUM|F_INT, CNT_INC, 0,39,90 },   /* Increment counter value */
   {"@deccount",       42,-1, 1, 1, F_NMA|F_NUM|F_INT, CNT_DEC, 0,39,91 },   /* Decrement counter value */
   {"@decc",           42,-1, 1, 1, F_NMA|F_NUM|F_INT, CNT_DEC, 0,39,91 },   /* Decrement counter value */
   {"@decskpz",        42,-1, 1, 1, F_NMA|F_NUM|F_INT, CNT_DSKPZ,  0,39,92 },   /* Dec Skip Z */
   {"@decskpgz",       42,-1, 1, 1, F_NMA|F_NUM|F_INT, CNT_DSKPNZ, 0,39,93 },   /* Dec Skip GTZ */
   {"@decskpnziz",     42,-1, 1, 1, F_NMA|F_NUM|F_INT, CNT_DSKPNZIZ, 0,39,100 }, /* Dec Skip NZ or Init Z */
   {"@decskpnz",       42,-1, 1, 1, F_NMA|F_NUM|F_INT, CNT_DSKPNZIZ, 0,39,100 }, /* Dec Skip NZ or Init Z */

   {"@null",           44,-1, 0, 0, F_NMA|F_INT,       0,       0,12,99 },   /* Null command */



#ifdef HASEXT0   /* Extended Type 0 (SW10 shutter controller) commands */
   {"shopen",           7, 6, 2, 2, F_DED | F_SHU, 0x03, 0, 7,63 },   /* Shutter open, ignore limit */
   {"shopenlim",        7, 6, 2, 2, F_DED | F_SHU, 0x01, 0, 7,61 },   /* Shutter open, enforce limit */
   {"shsetlim",         7, 6, 2, 2, F_DED | F_SHU, 0x02, 0, 7,62 },   /* Shutter set limit */
   {"shopenall",        7, 6, 1, 1, F_ALL | F_SHU, 0x04, 0, 1,64 },   /* Shutter all full open */
   {"shcloseall",       7, 6, 1, 1, F_ALL | F_SHU, 0x0B, 0, 1,65 },   /* Shutter all full close */
#endif

#ifdef HASCM17A  /* CM17A ("Firecracker") commands */   
   {"freset",          34,-1, 0, 0, F_RFS,            8, 0,12,55 },   /* CM17A Reset */
   {"fon",             34,-1, 1, 1, F_RFS,            2, 0, 3,47 },   /* CM17A On RF command */
   {"foff",            34,-1, 1, 1, F_RFS,            3, 0, 3,48 },   /* CM17A Off RF command */
   {"fbright",         34,-1, 2, 2, F_RFS,            5, 0,22,50 },   /* CM17A Bright RF command */
   {"fdim",            34,-1, 2, 2, F_RFS,            4, 0,22,49 },   /* CM17A Dim RF command */
   {"fdimbo",          34,-1, 2, 2, F_RFS,            7, 0,24,56 },   /* CM17A Dimb RF command */
   {"flightson",       34,-1, 1, 1, F_RFS | F_ALL,    1, 0, 1,51 },   /* CM17A AllLightsOn RF command */
   {"flightsoff",      34,-1, 1, 1, F_RFS | F_ALL,    6, 0, 1,52 },   /* CM17A AllLightsOff RF command */
   {"falloff",         34,-1, 1, 1, F_RFS | F_ALL,    0, 0, 1,53 },   /* CM17A AllUnitsOff RF command */
   {"farb",            34,-1, 3, 3, F_RFS | F_NUM,    9, 0,23,54 },   /* CM17A Arbitrary RF command */
   {"farw",            34,-1, 1,-1, F_RFS | F_NUM,   10, 0,30,73 },   /* CM17A Arbit 16 bit RF command */
   {"flux",            34,-1, 3,-1, F_RFS | F_NUM,   11, 0,31,76 },   /* CM17A Arbit UX17/23 RF command */
   {"ffbright",        34,-1, 2, 2, F_RFF,            5, 0,22,58 },   /* CM17A Fast Bright RF command */
   {"ffdim",           34,-1, 2, 2, F_RFF,            4, 0,22,57 },   /* CM17A Fast Dim RF command */
   {"ffdimbo",         34,-1, 2, 2, F_RFF,            7, 0,24,59 },   /* CM17A Dimb RF command */
   {"ffarb",           34,-1, 3, 3, F_RFF | F_NUM,    9, 0,23,60 },   /* CM17A Fast Arbit RF command */  
   {"ffarw",           34,-1, 1,-1, F_RFF | F_NUM,   10, 0,30,75 },   /* CM17A Arbit 16 bit RF command */
   {"fflux",           34,-1, 3,-1, F_RFF | F_NUM,   11, 0,31,77 },   /* CM17A Arbit UX17/23 RF command */
#endif
};
int nx10cmds = sizeof(x10command)/sizeof(struct cmd_list);

	
/* Function labels for monitor display (must align with enum in process.h) */
/* (with fake add-on function "AllOn") */

char *funclabel[76] = {
   "AllOff", "LightsOn", "On", "Off", "Dim", "Bright", "LightsOff",
   "Extended", "Hail", "HailAck", "Preset", "Preset",
   "DataXfer", "StatusOn", "StatusOff", "StatusReq", "AllOn",
   "xPowerUp", "vData", "vDataM", "Panic", "Arm", "Disarm",
   "Alert", "Clear", "Test", "sLightsOn", "sLightsOff", "secTamper",
   "sDusk", "sDawn", "AkeyOn", "AkeyOff", "BkeyOn", "BkeyOff",
   "rfxTemp", "rfxTemp2", "rfxRH", "rfxBP", "rfxVad", "rfxPot", "rfxVs",
   "rfxLoBat", "rfxOther", "rfxPulse", "rfxPower", "rfxWater", "rfxGas",
   "rfxCount", "dmxTemp", "dmxOn", "dmxOff", "dmxSetpoint",
   "oreTemp", "oreRH", "oreBP", "oreWgt",
   "oreWindSp", "oreWindAvSp", "oreWindDir",
   "oreRainRate", "oreRainTot", "elsCurr", "oreUV",
   "kOff", "kOn", "kGrpOff", "kGrpOn", "kUnkFunc", "kPreset", "kGrpPreset", "kUnkPresetFunc",
   "owlPower", "owlEnergy", "Inactive",
   "_Invalid_",
};


char *ext3funclabel[17] = {
   "", "xPreset", "", "xAllOn", "xAllOff", "", "", "xStatusReq", "xStatusAck",
   "", "", "xConfig", "", "", "", "", "xPowerUp",
};

char *rfxfunclabel[8] = {
   "rfxTemp", "rfxTemp2", "rfxHumidity", "rfxPressure", "rfxVad",
   "rfxVs", "rfxLoBat", "rfxOther",
};


char *rcs_temp ( int function, int predim, char hc, int unit )
{
   static char buffer[80];
   int         temp;
   
   if ( (function != 10 && function != 11) || unit < 11 ) {
      buffer[0] = '\0';
      return (char *)NULL;      
   }

   temp = -60 + (predim - 1) + 32 * (unit - 11);
   sprintf(buffer, " Temperature =%3d  : Location %c\n",
      temp, toupper((int)hc));

   return buffer;
}
      
/*---------------------------------------------------------------------+
 | Return the length of a macro element corresponding to the           |
 | argument command code.                                              |
 +---------------------------------------------------------------------*/
int macro_element_length ( unsigned char cmdcode )
{
   int j, length = -1;

   
   for ( j = 0; j < nx10cmds; j++ ) {
      if ( x10command[j].code == cmdcode && x10command[j].length > 0 ) {
         length = x10command[j].length;
         break;
      }
   }
   return length;
}
 
/*---------------------------------------------------------------------+
 | Return 1 if argument command label is in the administrative command |
 | (help) table, otherwise return 0.                                   |
 +---------------------------------------------------------------------*/
int is_admin_cmd ( char *label )
{
   int j;

   j = 0;
   while ( helpadmin[j][0] != NULL ) {
      if ( strcmp(label, helpadmin[j][0]) == 0 )
         return 1;
      j++;
   }
   return 0;
}

/*---------------------------------------------------------------------+
 | Return 1 if argument command label is in the x10command[] table,    |
 | otherwise return 0.                                                 |
 +---------------------------------------------------------------------*/
int is_direct_cmd ( char *label )
{
   int j;

   for ( j = 0; j < nx10cmds; j++ ) {
      if ( strcmp(label, x10command[j].label) == 0 &&
           !(x10command[j].flags & F_HLP) )
         return 1;
   }
   return 0;
}
   
/*---------------------------------------------------------------------+
 | Return 1 if argument command label is a scene or is in the          |
 | x10command[] table, otherwise return 0.                             |
 +---------------------------------------------------------------------*/
int is_heyu_cmd ( char *label )
{
   if ( strchr(label, ';') || strchr(label, ' ') || strchr(label, '\t') ) {
      /* Possible compound command */
      return 1;
   }

   if ( lookup_scene(configp->scenep, label) >= 0 ) {
      return 1;
   }

   if ( is_direct_cmd(label) ) {
      return 1;
   }

   return 0;
}   


/*---------------------------------------------------------------------+
 | Return a double random number between 0.0 and 1.0                   |
 +---------------------------------------------------------------------*/
double random_float ( void )
{
   static   int is_seeded;
   unsigned int seed;
   struct timeval tvstruct, *tv = &tvstruct;

   if ( !is_seeded ) {
      gettimeofday(tv, NULL);
      seed = (unsigned long)tv->tv_sec + (unsigned long)tv->tv_usec;
      srandom(seed);
      is_seeded = 1;
   }
   return (double)random() / (double)RandMax;
}

/*---------------------------------------------------------------------+
 | Sleep for argument milliseconds.                                    |
 +---------------------------------------------------------------------*/
void millisleep( long millisec )
{
   #ifdef HAVE_NSLEEP
   struct timestruc_t tspec;

   if ( millisec == 0 )
      return;

   tspec.tv_sec = millisec / 1000;
   tspec.tv_nsec = 1000000L * (millisec % 1000);

   while ( nsleep( &tspec, &tspec ) == -1 );
   #else
#ifndef HAVE_NANOSLEEP
   struct timeval tspec;
#else
   struct timespec tspec;
#endif /* HAVE_NANOSLEEP */

   if ( millisec == 0 )
      return;

#ifndef HAVE_NANOSLEEP
   tspec.tv_sec = millisec / 1000;
   tspec.tv_usec = 1000 * (millisec % 1000);   
   while ( usleep(tspec.tv_usec) == -1 );
#else
   tspec.tv_sec = millisec / 1000;
   tspec.tv_nsec = 1000000L * (millisec % 1000);
   while ( nanosleep( &tspec, &tspec ) == -1 );
#endif /* HAVE_NANOSLEEP */
   #endif  /* HAVE_NSLEEP */

   return;
}   

/*---------------------------------------------------------------------+
 | Sleep for argument microseconds.                                    |
 +---------------------------------------------------------------------*/
void microsleep( long microsec )
{
   #ifdef HAVE_NSLEEP
   struct timestruc_t tspec;

   if ( microsec == 0 )
      return;

   tspec.tv_sec = microsec / 1000000L;
   tspec.tv_nsec = 1000L * (microsec % 1000000L);

   while ( nsleep( &tspec, &tspec ) == -1 );
   #else
#ifndef HAVE_NANOSLEEP
   struct timeval tspec;
#else
   struct timespec tspec;
#endif

   if ( microsec == 0 )
      return;

#ifndef HAVE_NANOSLEEP
   tspec.tv_sec = microsec / 1000000;
   tspec.tv_usec = microsec % 1000000;
   while ( usleep(tspec.tv_usec) == -1 );
#else
   tspec.tv_sec = microsec / 1000000L;
   tspec.tv_nsec = 1000L * (microsec % 1000000L);
   while ( nanosleep( &tspec, &tspec ) == -1 );
#endif /* HAVE_NANOSLEEP */
   #endif /* HAVE_NSLEEP */

   return;
}   

/*---------------------------------------------------------------------+
 | Compare two commands.  Return 0 if identical except for label.      |
 +---------------------------------------------------------------------*/
static int cmp_commands( struct cmd_list *one, struct cmd_list *two )
{
   
   if ( !one || !two ||
        one->flags   != two->flags   ||
	one->code    != two->code    ||
	one->subcode != two->subcode ||
        one->length  != two->length  ||
        one->minargs != two->minargs ||
	one->maxargs != two->maxargs ||
	one->argtype != two->argtype ||
	one->help    != two->help        )
      return 1;
   return 0;
}


/*---------------------------------------------------------------------+
 | Parse the units string.  Pass back the X10 units bitmap through the |
 | argument list.  Return 0 if valid bitmap, otherwise write error msg |
 | to error handler and return 1.  Unit 0 is considered a valid unit   |
 | number but doesn't appear in the bitmap unless it's the only unit,  |
 | whereupon the bitmap will be 0.                                     |
 +---------------------------------------------------------------------*/
int parse_units ( char *str, unsigned int *bitmap )
{

   static int   umap[] = {6,14,2,10,1,9,5,13,7,15,3,11,0,8,4,12};
   char         buffer[256];
   char         errmsg[128];
   char         *tail, *sp;

   int          ulist[17];
   int          j, ustart, unit;

   clear_error_message();

   if ( strcmp(str, "*") == 0 ) {
      *bitmap = 0xffff;
      return 0;
   }

   for ( j = 0; j < 17; j++ )
      ulist[j] = 0;

   ustart = -1;
   tail = str;
   while ( *(get_token(buffer, &tail, "-,", 255)) ) {
      unit = (int)strtol(buffer, &sp, 10);
      if ( *sp ) {
         sprintf(errmsg, "Invalid character '%c' in units list", *sp);
         store_error_message(errmsg);
         return 1;
      }

      if ( unit < 0 || unit > 16 ) {
         sprintf(errmsg, "Unit number outside range 0-16");
         store_error_message(errmsg);
         return 1;
      }

      if ( *tail == ',' || *tail == '\0' ) {
         if ( ustart > -1 ) {
            if ( ustart <= unit ) {
               for ( j = ustart; j <= unit; j++ )
                  ulist[j] = 1;
               ustart = -1;
               continue;
            }
            else {
               for ( j = unit; j <= ustart; j++ )
                  ulist[j] = 1;
               ustart = -1;
               continue;
            }
         }
         else {
            ulist[unit] = 1;
            continue;
         }
      }
      else {
         ustart = unit;
      }
   }

   *bitmap = 0;
   for ( j = 0; j < 16; j++ )
      *bitmap |= (ulist[j+1]) << umap[j];

   return 0;
}

/*---------------------------------------------------------------------+
 | Return the number of units represented by a bitmap                  |
 +---------------------------------------------------------------------*/
int count_bitmap_units ( unsigned int bitmap )
{
   int j, mask, count;

   mask = 1;
   count = 0;
   for ( j = 0; j < 16; j++ ) {
      if ( bitmap & mask )
         count++;
      mask = mask << 1;
   }
   return count;
}
         
/*---------------------------------------------------------------------+
 | Parse the Houscode|Units token, which may be comprised of:          |
 |   Optional '+' or '-' prefix, plus:                                 |
 |   An ALIAS, or:                                                     |
 |     Housecode letter in range A-P, or '_' for placeholder.          |
 |     Heyu format units string, e.g., 2,3,5-8,10,13-15                |
 | Pass back the Housecode letter and X10 units bitmap through the     |
 | argument list.                                                      |
 | Return flags indicating the presence of each component:             |
 |   A_VALID    Valid address.                                         |
 |   A_HCODE    Housecode A-P has been returned.                       |
 |   A_BMAP     Non-zero bitmap has been returned                      |
 |   A_PLUS     Prefix '+' sign                                        |
 |   A_MINUS    Prefix '-' sign                                        |
 |   A_ALIAS    Housecode and bitmap came from an alias in config      |
 |   A_DUMMY    Replaceable address, e.g., $1, $3                      |
 |   A_MULT     Bitmap contains multiple units.                        |
 |   A_STAR     Asterisk ('*') for units list.                         |
 +---------------------------------------------------------------------*/
unsigned int parse_addr ( char *token, char *housecode,
                                                     unsigned int *bitmap )
{
   char          errmsg[128];
   unsigned int  flags = 0;
   char          *sp;
   char          hc;
   
   sp = token;

   if ( strchr(sp, '$') ) {
      /* Supply a fake value that will always pass scene verification */
      /* for a parameter containing a positional parameter.           */
      *housecode = 'A';
      *bitmap = 1;
      flags = (A_DUMMY | A_VALID | A_HCODE | A_BMAP | A_PLUS);
      return flags;
   }

   if ( *sp == '+' )  {
      flags |= A_PLUS;
      sp++;
   }
   else if ( *sp == '-' ) {
      flags |= A_MINUS;
      sp++;
   }

   if ( *sp == '\0' ) {
      *housecode = '_';
      *bitmap = 0;
      flags |= A_VALID;
      return flags;
   }


   if ( alias_lookup(sp, housecode, bitmap) >= 0 ) {
      flags |= (A_ALIAS | A_HCODE | A_VALID);
      if ( *bitmap )
         flags |= A_BMAP;
      if ( count_bitmap_units(*bitmap) > 1 )
         flags |= A_MULT;
      return flags;
   }

   hc = toupper((int)(*sp));

   if ( hc == '_' )
      hc = configp->housecode;

   if ( (hc != '_') && (hc < 'A' || hc > 'P') ) {
      *housecode = '_';
      *bitmap = 0;
      sprintf(errmsg, "Invalid housecode '%c' in HU '%s'", *sp, token);
      store_error_message(errmsg);
      flags &= ~(A_VALID | A_HCODE);
      return flags;
   }
   
   *housecode = hc;
   if ( hc != '_' )
      flags |= A_HCODE;
   sp++;

   if ( *sp == '\0' ) {
      *bitmap = 0;
      flags |= A_VALID;
      return flags;
   }
   else if ( *sp == '*' ) {
      *bitmap = 0xffff;
      flags |= (A_VALID | A_STAR);
      return flags;
   }
   else if ( parse_units(sp, bitmap) != 0 ) {
      /* Prefix the message written to error handler */
      sprintf(errmsg, "HU '%s': ", token);
      add_error_prefix(errmsg);
      return 0;
   }

   if ( *bitmap )
      flags |= A_BMAP;
   if ( count_bitmap_units(*bitmap) > 1 )
      flags |= A_MULT;

   flags |= A_VALID;

   return flags;
}

/*---------------------------------------------------------------+
 |  Return the 8-bit checksum of the bytes in argument buffer.   |
 +---------------------------------------------------------------*/     
unsigned char checksum( unsigned char *buffer, int length ) 
{
   int j;
   unsigned char sum = 0; 

   for ( j = 0; j < length; j++ )
      sum += buffer[j];

   return (sum & 0xff);
}


/*---------------------------------------------------------------+
 | Create buffers with three two-byte command function codes and |
 | their checksums (which must all be different and less than    |
 | 0x80).  These are used to alert the relay that a command with |
 | a checksum 0x5A will be immediately following.                |
 +---------------------------------------------------------------*/
void create_alerts ( void )
{
   extern unsigned char alert_cmd[6];
   extern unsigned char alert_ack[3];

   static unsigned char cmdbytes[] = {0x86,0xCC, 0x46,0xCC, 0x86,0xBC};

   int j;
   unsigned char *sp;

   memcpy(alert_cmd, (void *)cmdbytes, 6);

   for ( j = 0; j < 3; j++ ) {
      sp = alert_cmd + j + j;
      alert_ack[j] = checksum(sp, 2);
   }
   if ( verbose ) {
      fprintf(stderr, "Alert acks are 0x%02x, 0x%02x, 0x%02x\n",
         alert_ack[0], alert_ack[1], alert_ack[2]);
   }

   return;
}

   
/*---------------------------------------------------------------+
 | Alert function which sends three function codes in a row      |
 | without the "WRMI" byte after each.  When the relay receives  |
 | the sequence of three checksums, it is alerted that the next  |
 | 0x5A it receives is a checksum rather than an incoming signal.|
 +---------------------------------------------------------------*/
int send_checksum_alert ( void )
{
   extern unsigned char alert_cmd[6];
   extern unsigned char alert_ack[3];

   int  k, nread;
   char inbuf[16];
   int  xread();
   int  ignoret;
   
   send_x10state_command(ST_CHKSUM, 0);

   for ( k = 0; k < 3; k++ ) {
         ignoret = write(tty, (char *)(alert_cmd + k + k), 2);
         nread = exread(sptty, inbuf, 1, 1);
         if ( nread != 1 || inbuf[0] != alert_ack[k] ) {
            return 1;
         }
   }
   return 0;
   
   fprintf(stderr, "send_checksum_alert() failed.\n");

   check4poll(0, 0);
   return 1;
}

/*---------------------------------------------------------------+
 |  Display a message from one of the send_* functions.          |
 +---------------------------------------------------------------*/ 
void display_send_message ( char *message ) 
{
   if ( i_am_aux || i_am_relay )
      display_x10state_message(message);
   else
      fprintf(stderr, "%s\n", message);
   return;
}
      
/*---------------------------------------------------------------+
 |  Send the contents of the buffer to the interface.            |
 |  Return 0 if successful, 1 otherwise.                         |
 |  Argument 'chkoff' - which byte starts the checksum.          |
 |  Argument 'syncmode' - external triggering.                   |
 +---------------------------------------------------------------*/  
int send_buffer ( unsigned char *buffer, int length,
                          int chkoff, int timeout, int syncmode )
{
   unsigned char chksum;
   unsigned char inbuff[16];
   int           nread;
   char          msgbuff[80];
   extern int    wait_external_trigger(int);    

   if ( tty == TTY_DUMMY ) {
      display_send_message("Command is invalid for dummy TTY.");
      return 1;
   }

   chksum = checksum(buffer + chkoff, length - chkoff);

   if ( chksum == 0x5a ) {
      if ( verbose )
         display_send_message("Alert relay to command with checksum 0x5A");
      if ( send_checksum_alert() != 0 )
         return 1;
   }

   (void) xwrite(tty, (char *) buffer, length);

   /* get a check sum in reply */

   nread = xread(sptty, inbuff, 1, 1);

   if ( chksum == inbuff[0] ) {
      /* Checksum is OK; tell interface to transmit the code. */
      if ( verbose )
         display_send_message("Checksum confirmed");

      /* Defer sending until the appropriate half-cycle if so          */
      /* specified (assuming we have the external hardware installed). */
      if ( syncmode != NO_SYNC )  	 
         wait_external_trigger(syncmode);

      (void) xwrite(tty, "\00" , 1);	/* WRMI (we really mean it) */
   }
   else if (!nread) {
      if ( verbose )
         display_send_message("Checksum NOT confirmed, no response from interface");
      syslog(LOG_WARNING, "Checksum NOT confirmed, no response from interface");
      return 1;
   }
   else  {
      if ( verbose ) {
         sprintf(msgbuff, "Checksum %02x NOT confirmed - should be %02x\n",
            inbuff[0], chksum);
         display_send_message(msgbuff);
      }
      return 1;
   }

   if ( (nread = xread(sptty, inbuff, 1, timeout)) >= 1 ) {
      if( inbuff[0] == 0x55 ) {
         /* interface is ready again */
         if ( verbose )
            display_send_message("Interface is ready.");
         return 0;
      }
      else if ( nread == 1 && chksum == 0x5a ) {
         if ( verbose )
            display_send_message("Checksum 0x5a workaround.");
         return 0;
      }
      else if ( nread == 1 && inbuff[0] == 0x5a ) {
         /* Incoming command - forget about the 0x55 */
         if ( verbose )
            display_send_message("send_buffer() interrupted by incoming command");
         return 2;
      }
      else {
         sprintf(msgbuff,
          "Interface not ready - received %d bytes beginning %02x\n",
            nread, inbuff[0]);
         display_send_message(msgbuff);
         return 1;
      }
   }
   
   return 1;
}
 

/*---------------------------------------------------------------+
 |  Send an encoded housecode|bitmap to the interface.           |
 |  Try up to 3 times.  Return 0 if successful, otherwise 1.     |
 |  Argument housecode is a X10 encoded char A-P                 |
 |  Argument bitmap is encoded per X10                           |
 +---------------------------------------------------------------*/
int send_address ( unsigned char hcode, unsigned int bitmap,
                                      int syncmode )
{  
   unsigned int  mask;
   unsigned char buffer[4];
   char          msgbuff[80];
   int           j, k, result = 1;
   int           count, timeout = 10;
   extern int    is_ring ( void );

   if ( tty == TTY_DUMMY ) {
      display_send_message("Command is invalid for dummy TTY.");
      return 1;
   }
      
   if ( bitmap == 0 )
      return 0;

   /* Defer sending if Ring Indicator line is asserted, which */
   /* indicates there is incoming X10 data.                   */
   if ( configp->check_RI_line == YES ) {
      count = 10;
      while ( is_ring() > 0 && --count ) { 
         sleep(1);
         if ( verbose && !i_am_relay )
            display_send_message("Defer while RI line is asserted.");
      }
      if ( !count )
         display_send_message("RI serial line may be stuck.");
   }

   if ( busywait(configp->auto_wait) != 0 ) {
      display_send_message("Unable to send address bytes - busywait() timeout.");
      return 1;
   }

   /* Inform other processes of parent process */
   send_x10state_command(ST_SOURCE, (unsigned char)heyu_parent);

   hcode = hcode << 4;

   mask = 1;
   for ( j = 0; j < 16; j++ ) {
      if ( bitmap & mask ) {
         buffer[0] = 0x04;
         buffer[1] = hcode | j ;
	 
	 /* Kluge "fix" for checksum 5A problem. */
	 /* CM11A seems to disregard a bit in the Dim field. */
         /* (The only address affected by this is G1) */
	 if ( checksum(buffer, 2) == 0x5A && configp->fix_5a == YES )
	    buffer[0] = 0x0C;

         if ( verbose ) {
            sprintf(msgbuff, "Sending address bytes: %02x %02x\n",
               buffer[0], buffer[1]);
            display_send_message(msgbuff);
         }

         for ( k = 0; k < configp->send_retries; k++ ) {
            if ( (result = send_buffer(buffer, 2, 0, timeout, syncmode)) == 0 )
               break;
            sleep(1);
         }

         /* Delay so many powerline cycles between commands */
         millisleep((long)configp->cm11_post_delay); 

         if ( result != 0 ) {
            display_send_message("Unable to send address bytes");
            return 1;
         }
      }   
            
      mask = mask << 1;
   }

   return 0;
}

/*---------------------------------------------------------------+
 |  Send an X10 command buffer to the interface. Try up to 3     |
 |  times.  Return 0 if sucessful, otherwise 1.                  | 
 +---------------------------------------------------------------*/
int send_command ( unsigned char *buffer, int len, 
                unsigned int showstatus, int syncmode )
{
   int        j, result = 1;
   int        count, timeout = 10;
   char       msgbuff[128];
   extern int is_ring ( void );

   if ( tty == TTY_DUMMY ) {
      display_send_message("Command is invalid for dummy TTY.");
      return 1;
   }

   if ( len == 0 )
      return 0;

   /* Defer sending if Ring Indicator line is asserted, which */
   /* indicates there is incoming X10 data.                   */
   if ( !i_am_aux && configp->check_RI_line == YES ) {
      count = 10;
      while ( is_ring() > 0 && --count ) { 
         sleep(1);
         if ( verbose && !i_am_relay )
            display_send_message("Defer while RI line is asserted.");
      }
      if ( !count )
         display_send_message("RI serial line may be stuck.");
   }

   if ( busywait(configp->auto_wait) != 0 ) {
      display_send_message("Unable to send command bytes - busywait() timeout.");
      return 1;
   }

   /* Inform other processes of parent process */
   send_x10state_command(ST_SOURCE, (unsigned char)heyu_parent);
  
   if ( verbose ) {
      sprintf(msgbuff, "Sending command bytes:");
      for ( j = 0; j < len; j++ )
         sprintf(msgbuff + strlen(msgbuff), " %02x", buffer[j]);
      display_send_message(msgbuff);
   }

   for ( j = 0; j < configp->send_retries; j++ )  {
      if ( (result = send_buffer(buffer, len, 0, timeout, syncmode)) == 0 ||
           result == 2 ) {
         break;
      }
      sleep(1);
   }
   if ( result != 0 ) {
      display_send_message("Unable to send command bytes");
      return 1;
   }

   if ( special_func > 0 ) {
      /* Command has called for special processing of */
      /* reply from module, e.g, decoding of preset   */
      /* reply from TempLinc into temperature         */
      for ( j = 0; j < configp->spf_timeout; j++ ) {
         check4poll(0, 1);
      }
   }
   else if ( showstatus ) {
      /* Display status if expected */
      for ( j = 0; j < configp->status_timeout; j++ ) {
         check4poll(1, 1);
      }
   }
   else {
         check4poll(0, 0);
   }

   /* Delay so many powerline cycles between commands */
   millisleep((long)configp->cm11_post_delay); 

   if ( verbose )
      display_send_message("Transmission OK");

   return 0;

}

/*---------------------------------------------------------------+
 | Disable the CM11A from asserting the RI serial line.          |
 +---------------------------------------------------------------*/
int ri_disable ( void )
{
   unsigned char buf[2];

   buf[0] = 0xdb;

   return send_buffer(buf, 1, 0, 1, 0);
}


/*---------------------------------------------------------------+
 | Enable the CM11A to assert the RI serial line.                |
 +---------------------------------------------------------------*/
int ri_enable ( void )
{
   unsigned char buf[2];

   buf[0] = 0xeb;

   return send_buffer(buf, 1, 0, 1, 0);
}


/*---------------------------------------------------------------+
 | Send an RI control command, either enable or disable,         |
 | depending on the config directive RING_CTRL.                  |
 +---------------------------------------------------------------*/
int ri_control ( void )
{
   unsigned char buf[2];

   buf[0] = (configp->ring_ctrl == DISABLE) ? 0xdb : 0xeb ;

   return send_buffer(buf, 1, 0, 1, 0);
}

/*---------------------------------------------------------------+
 | Send an RI control command from the Heyu relay, either enable |
 | or disable, depending on the config directive RING_CTRL.      |
 +---------------------------------------------------------------*/
int relay_ri_control ( void )
{
    int      j, nread;
    unsigned char outbuf[2], inbuf[2];

    int ignoret;

    outbuf[0] = (configp->ring_ctrl == DISABLE) ? 0xdb : 0xeb;
   
    for ( j = 0; j < 3; j++ ) {
       ignoret = write(tty, (char *) outbuf, 1);
       nread = sxread(tty, inbuf, 1, 1);

       if ( nread != 1 || inbuf[0] != outbuf[0] ) {
          continue;
       }

       ignoret = write(tty, "\0", 1);   /* WRMI */
       nread = sxread(tty, inbuf, 1, 1);

       if ( nread == 1 && inbuf[0] == 0x55 ) {
          return 0;
       }
    }

    millisleep(SETCLOCK_DELAY);
    
    return 1;
}
   



/*---------------------------------------------------------------+
 | Command to disable the CM11A RI line.                         |
 +---------------------------------------------------------------*/
int c_ri_disable ( int argc, char *argv[] )
{
   int  j, result = 0;

   for ( j = 0; j < 3; j++ )  {
      if ( (result = ri_disable()) == 0 ) 
         break;
      sleep(1);
   }

   if ( result == 0 )
      printf("CM11A RI serial line disabled.\n");
   else
      fprintf(stderr, "No response to ri_disable command.\n");

   check4poll(0, 0);

   return result;
}

/*---------------------------------------------------------------+
 | Command to enable the CM11A RI line.                          |
 +---------------------------------------------------------------*/
int c_ri_enable ( int argc, char *argv[] )
{
   int  j, result = 0;

   for ( j = 0; j < 3; j++ )  {
      if ( (result = ri_enable()) == 0 ) 
         break;
      sleep(1);
   }

   if ( result == 0 )
      printf("CM11A RI serial line enabled.\n");
   else
      fprintf(stderr, "No response to ri_enable command.\n");

   check4poll(0, 0);

   return result;
}

/*---------------------------------------------------------------+
 | Ping the CM11A retries times.                                 |
 | Return 0 if response OK, 1 otherwise.                         |
 +---------------------------------------------------------------*/
int relay_ping ( int tty, int retries )
{
    int      j, nread;
    unsigned char outbuf[2], inbuf[2];

    int ignoret;

    outbuf[0] = (configp->ring_ctrl == DISABLE) ? 0xdb : 0xeb;
   
    for ( j = 0; j < retries; j++ ) {
       ignoret = write(tty, (char *) outbuf, 1);
       nread = sxread(tty, inbuf, 1, 1);

       if ( nread != 1 || inbuf[0] != outbuf[0] ) {
          continue;
       }

       ignoret = write(tty, "\0", 1);   /* WRMI */
       nread = sxread(tty, inbuf, 1, 1);

       if ( nread == 1 && inbuf[0] == 0x55 ) {
          return 0;
       }
    }

    millisleep(SETCLOCK_DELAY);
    
    return 1;
}
   

/*---------------------------------------------------------------+
 | Ping the CM11A by sending the RI control command.             |
 +---------------------------------------------------------------*/
int c_ping ( int argc, char *argv[] )
{
   int  j, result = 0;

   for ( j = 0; j < 3; j++ )  {
      if ( (result = ri_control()) == 0 ) 
         break;
      sleep(1);
   }

   if ( result == 0 )
      printf("Ping response OK\n");
   else
      fprintf(stderr, "No response to ping.\n");

   check4poll(0, 0);

   return result;
}
   
/*---------------------------------------------------------------+
 | Called by c_busywait() below.                                 |
 +---------------------------------------------------------------*/
int busywait ( int timeout )
{
   int  j, nread;
   unsigned char buf[2], inbuf[16];
   int xread();

   int ignoret;

   if ( timeout <= 0 )
      return 0;   

   /* RI control byte used for ping */
   buf[0] = (configp->ring_ctrl == DISABLE) ? 0xdb : 0xeb ;
   buf[1] = 0x00;

   send_x10state_command (ST_BUSYWAIT, 1);
   
   for ( j = 0; j < timeout; j++ ) {      
      ignoret = write(tty, (char *)buf, 1);
      nread = xread(sptty, (char *)inbuf, 1, 1);

      if ( nread != 1 || inbuf[0] != buf[0] ) {
         millisleep(60);
         continue;
      }
      ignoret = write(tty, (char *)(buf + 1), 1);
      nread = xread(sptty, (char *)inbuf, 1, 1);
      
      if ( nread == 1 && inbuf[0] == 0x55u ) {
         return 0;
      }
      break;
   }

   send_x10state_command (ST_BUSYWAIT, 0);
   check4poll(0, 0);
   return 1;
}  
   
/*---------------------------------------------------------------+
 | Wait for CM11A to respond to "pings" or until timeout.        |
 | Return 1 if timeout or error, otherwise 0.                    | 
 +---------------------------------------------------------------*/
int c_busywait ( int argc, char *argv[] )
{
   int  timeout;
   char *sp;
   
   timeout = 30;
   
   if ( argc > 2 ) {
      timeout = (int)strtol(argv[2], &sp, 10);
      if ( !strchr(" \t\n", *sp) || timeout < 0 || timeout > 300 ) {
         fprintf(stderr, "Invalid %s timeout parameter.\n", argv[1]);
         return 1;
      }
   }
   return busywait(timeout);
}  
   
/*---------------------------------------------------------------+
 | Display help for engine internal commands.                    |
 +---------------------------------------------------------------*/
void display_internal_help ( void )
{
   int  j;
   int  szlbl, szarg, sztot;
   char buffer[128];
   struct cmd_list *one = NULL, *two;

   /* Get max string lengths for formatting output */
   szlbl = szarg = sztot = 0;
   for ( j = 0; j < nx10cmds; j++ ) {
      if ( x10command[j].flags & F_HID || !(x10command[j].flags & F_INT) )  {
         continue;
      }
      sztot = max(sztot, ((int)strlen(x10command[j].label) + 
                      (int)strlen(helparg[x10command[j].argtype])) );
   }
   for ( j = 0; j < nx10cmds; j++ ) {
      if ( x10command[j].flags & F_HID || !(x10command[j].flags & F_INT) )  {
         continue;
      }
      two = &x10command[j];
      if ( cmp_commands(one, two) ) {
         sprintf(buffer, "%s  %s", x10command[j].label, helparg[x10command[j].argtype]);
         printf("  %-*s  %s%s\n", sztot + 2, buffer,
             helpdirect[x10command[j].help],
             (x10command[j].flags & (F_NMA | F_NMAI)) ? " (*)" : "");
         one = two;
      }
   }
   return;
}
    
/*---------------------------------------------------------------+
 | Display help for general command list other than direct or    |
 | cm17a commands.                                               | 
 +---------------------------------------------------------------*/
void display_gen_help ( char *helplist[][3] )
{
   int  j;
   int  szlbl, szarg, sztot;
   char buffer[128];

   /* Get max string lengths for formatting output */
   szlbl = szarg = sztot = 0;
   j = 0;
   while ( helplist[j][0] != NULL ) {
      szlbl = max(szlbl, (int)strlen(helplist[j][0]));
      szarg = max(szarg, (int)strlen(helplist[j][1]));
      sztot = max(sztot, ((int)strlen(helplist[j][0]) + 
                               (int)strlen(helplist[j][1])));
      j++;
   }

   j = 0;
   while ( helplist[j][0] != NULL ) {
      /* Compact format */
      sprintf(buffer, "%s  %s", helplist[j][0], helplist[j][1]);    
      printf("  heyu  %-*s  %s\n", sztot + 2, buffer, helplist[j][2]);
      j++;
   }
   return;
}

/*---------------------------------------------------------------+
 | Display help for argument command, or all commands if empty   |
 | string.                                                       | 
 +---------------------------------------------------------------*/
#define HELP_OPTIONS   1
#define HELP_ADMIN     2
#define HELP_STATE     4
#define HELP_DIRECT    8
#define HELP_CM17A    16
#define HELP_RFXS     32
#define HELP_RFXM     64
#define HELP_INT     128
#define HELP_SHUTTER 256
#define HELP_HELP    512
#define HELP_DMX    1024
#define HELP_ORE    2048
#define HELP_ALL    (HELP_OPTIONS | HELP_ADMIN | HELP_STATE | HELP_DIRECT | HELP_CM17A | HELP_RFXS | HELP_RFXM | HELP_INT | HELP_SHUTTER | HELP_DMX | HELP_ORE )
#define COMPACT_FORMAT
void command_help ( char *command ) 
{
   int j, ext0done = 0;
   int szlbl, szarg, sztot, list = 0;
   struct cmd_list *one = NULL, *two;
   char buffer[256];

   strlower(command);
   list = (*command == '\0')                    ?  HELP_ALL     :
          (!strcmp(command,  "help"))           ?  HELP_HELP    :
          (!strncmp(command, "options", 6))     ?  HELP_OPTIONS :
          (!strncmp(command, "admin", 5))       ?  HELP_ADMIN   :
          (!strcmp(command,  "state"))          ?  HELP_STATE   :
          (!strcmp(command,  "direct"))         ?  HELP_DIRECT  :
          (!strcmp(command,  "shutter"))        ?  HELP_SHUTTER :
          (!strncmp(command, "cm17a", 4))       ?  HELP_CM17A   :
          (!strcmp(command,  "firecracker"))    ?  HELP_CM17A   :
          (!strncmp(command, "rfxsensor", 4))   ?  HELP_RFXS    :
          (!strncmp(command, "rfxmeter", 4))    ?  HELP_RFXM    : 
          (!strncmp(command, "digimax", 3))     ?  HELP_DMX     :
          (!strncmp(command, "oregon", 3))      ?  HELP_ORE     :
          (!strncmp(command, "internal", 3))    ?  HELP_INT     : 0;
#if 0
          (!strncmp(command, "rfx", 3))      ?  (HELP_RFXS | HELP_RFXM)    : 0 ;
#endif

#ifdef HASRFXS
      j = 0;
      while ( helprfxsensor[j][0] != NULL ) {
         if ( strcmp(command, helprfxsensor[j][0]) == 0 ) {
            printf("Usage: heyu  %s %s  %s\n", helprfxsensor[j][0], 
                helprfxsensor[j][1], helprfxsensor[j][2]);
            return;
         }
         j++;
      }
#endif /* HASRFXS */

#ifdef HASRFXM
      j = 0;
      while ( helprfxmeter[j][0] != NULL ) {
         if ( strcmp(command, helprfxmeter[j][0]) == 0 ) {
            printf("Usage: heyu  %s %s  %s\n", helprfxmeter[j][0], 
                helprfxmeter[j][1], helprfxmeter[j][2]);
            return;
         }
         j++;
      }
#endif /* HASRFXM */

#ifdef HASDMX
      j = 0;
      while ( helpdigimax[j][0] != NULL ) {
         if ( strcmp(command, helpdigimax[j][0]) == 0 ) {
            printf("Usage: heyu  %s %s  %s\n", helpdigimax[j][0], 
                helpdigimax[j][1], helpdigimax[j][2]);
            return;
         }
         j++;
      }
#endif /* HASDMX */

#ifdef HASORE
      j = 0;
      while ( helporegon[j][0] != NULL ) {
         if ( strcmp(command, helporegon[j][0]) == 0 ) {
            printf("Usage: heyu  %s %s  %s\n", helporegon[j][0], 
                helporegon[j][1], helporegon[j][2]);
            return;
         }
         j++;
      }
#endif /* HASORE */

   if ( list & HELP_OPTIONS ) {
      /* Display options */
      printf("\n");
      for ( j = 0; j < (int)(sizeof(options)/sizeof(char *)); j++ ) {
         printf("%s\n", options[j]);
      }
   }

   if ( list & HELP_HELP ) {
      printf("\nUsage: heyu help [category|command]  All help [or specific Category or Command]\n");
      printf(" Categories: options admin direct");
#ifdef HASEXT0
      printf(" shutter");
#endif
#ifdef HASCM17A
      printf(" cm17a");
#endif
      printf(" internal");
      printf(" state");
#ifdef HASRFXS
      printf(" rfxsensor");
#endif
#ifdef HASRFXM
      printf(" rfxmeter");
#endif
#ifdef HASDMX
      printf(" digimax");
#endif
#ifdef HASORE
      printf(" oregon");
#endif
      printf(" webhook");
      printf("\n");
   }

   if ( list & HELP_ADMIN ) {
      /* Administrative commands */
      printf("\n [Administrative commands  (H = Housecode)]\n");
      display_gen_help(helpadmin);
   }

   if ( list & HELP_STATE ) {
      /* State commands */
      printf("\n [State commands  (H = Housecode, u = Single unit) - require heyu engine]\n");
      display_gen_help(helpstate);
   }

#if HASRFXS
   if ( (list & HELP_STATE) || (list & HELP_RFXS) ) {
      /* RFXSensor state commands */
      printf("\n [RFXSensor state commands  (H = Housecode, u = Single unit) - require heyu engine]\n");
      display_gen_help(helprfxsensor);
   }
#endif /* HASRFXS */

#ifdef HASRFXM
   if ( (list & HELP_STATE) || (list & HELP_RFXM) ) {
      /* RFXMeter state commands */
      printf("\n [RFXMeter state commands  (H = Housecode, u = Single unit) - require heyu engine]\n");
      display_gen_help(helprfxmeter);
   }
#endif /* HASRFXM */

#ifdef HASDMX
   if ( (list & HELP_STATE) || (list & HELP_DMX) ) {
      /* Digimax state commands */
      printf("\n [DigiMax state commands  (H = Housecode, u = Single unit) - require heyu engine]\n");
      display_gen_help(helpdigimax);
   }
#endif /* HASDMX */

#ifdef HASORE
   if ( (list & HELP_STATE) || (list & HELP_ORE) ) {
      /* Oregon state commands */
      printf("\n [Oregon group state commands  (H = Housecode, u = Single unit) - require heyu engine]\n");
      display_gen_help(helporegon);
   }
#endif /* HASORE */

   if ( list & HELP_DIRECT ) {
      /* Direct commands */
      /* Get max string lengths for formatting output */
      szlbl = szarg = sztot = 0;
      for ( j = 0; j < nx10cmds; j++ ) {
         if ( x10command[j].flags & (F_HID | F_INT))
            continue;
         sztot = max(sztot, ((int)strlen(x10command[j].label) + 
                      (int)strlen(helparg[x10command[j].argtype])) );
      }
      printf("\n [Direct commands  (H = Housecode, U = Units list)]\n");        
      for ( j = 0; j < nx10cmds; j++ ) {
	 two = &x10command[j];
	 if ( x10command[j].flags & (F_HID | F_INT | F_FIR | F_SHU) ) {
	    /* Command not shown in this menu */
            continue;
	 }
         if ( cmp_commands(one, two) ) {
            sprintf(buffer, "%s  %s", x10command[j].label, helparg[x10command[j].argtype]);
            printf("  heyu  %-*s  %s%s\n", sztot + 2, buffer,
                helpdirect[x10command[j].help],
                (x10command[j].flags & (F_NMA | F_NMAI)) ? " (*)" : "");
	    one = two;
	 }
      }
    
      #ifdef HASEXT0 
      printf("\n [Shutter (Extended Type 0) direct commands  (H = Housecode, U = Units list)]\n");        
      for ( j = 0; j < nx10cmds; j++ ) {
         two = &x10command[j];
	 if ( !(x10command[j].flags & F_SHU) ) {
            continue;
	 }
         if ( cmp_commands(one, two) ) {
            sprintf(buffer, "%s  %s", x10command[j].label, helparg[x10command[j].argtype]);
            printf("  heyu  %-*s  %s%s\n", sztot + 2, buffer,
                helpdirect[x10command[j].help],
                (x10command[j].flags & (F_NMA | F_NMAI)) ? " (*)" : "");
	    one = two;
	 }
      }
      #endif /* End ifdef */
      ext0done = 1;
   }

   #ifdef HASEXT0 
   if ( list & HELP_SHUTTER && !ext0done) { 
      printf("\n [Shutter (Extended Type 0) direct commands  (H = Housecode, U = Units list)]\n");        
      /* Get max string lengths for formatting output */
      szlbl = szarg = sztot = 0;
      for ( j = 0; j < nx10cmds; j++ ) {
         if ( x10command[j].flags & F_HID || !(x10command[j].flags & F_SHU) )
            continue;
         sztot = max(sztot, ((int)strlen(x10command[j].label) + 
                      (int)strlen(helparg[x10command[j].argtype])) );
      }

      for ( j = 0; j < nx10cmds; j++ ) {
         two = &x10command[j];
         if ( !(x10command[j].flags & F_SHU) ) {
            continue;
         }
         if ( cmp_commands(one, two) ) {
            sprintf(buffer, "%s  %s", x10command[j].label, helparg[x10command[j].argtype]);
            printf("  heyu  %-*s  %s%s\n", sztot + 2, buffer,
                helpdirect[x10command[j].help],
                (x10command[j].flags & (F_NMA | F_NMAI)) ? " (*)" : "");
            one = two;
         }
      }
   }
   #endif /* End ifdef */

   #ifdef HASCM17A
   if ( list & HELP_CM17A ) { 
      /* Get max string lengths for formatting output */
      szlbl = szarg = sztot = 0;
      for ( j = 0; j < nx10cmds; j++ ) {
         if ( x10command[j].flags & F_HID )
            continue;
         sztot = max(sztot, ((int)strlen(x10command[j].label) + 
                      (int)strlen(helparg[x10command[j].argtype])) );
      }

      printf("\n [CM17A \"Firecracker\" commands (H = Housecode, U = Units list)]\n");        
      for ( j = 0; j < nx10cmds; j++ ) {
         two = &x10command[j];
	 if ( !(x10command[j].flags & F_FIR) ) {
            continue;
	 }
         if ( cmp_commands(one, two) ) {
            sprintf(buffer, "%s  %s", x10command[j].label, helparg[x10command[j].argtype]);
            printf("  heyu  %-*s  %s%s\n", sztot + 2, buffer,
                helpdirect[x10command[j].help],
                (x10command[j].flags & (F_NMA | F_NMAI)) ? " (*)" : "");
	    one = two;
	 }
      }
   }
   #endif /* End ifdef */

   if ( list & HELP_INT ) {
      /* Internal engine precommands */
      printf("\n [Internal engine precommands  (See man pages)]\n");
      display_internal_help();
   }

   if ( list & (HELP_DIRECT | HELP_CM17A) ) {   
      display_helpnotes();
      display_manpage_list();
      return;
   } 
   else if ( list > 0 ) {
      display_manpage_list();
      return;
   }  
   else if ( list == 0 ) {
      /* Display specific command */

      j = 0;
      while ( helpadmin[j][0] != NULL ) { 
         if ( strcmp(command, helpadmin[j][0]) == 0 ) {
            printf("Usage: heyu  %s %s  %s\n", helpadmin[j][0], 
                helpadmin[j][1], helpadmin[j][2]);
            return;
         }
         j++;
      }

      j = 0;
      while ( helpstate[j][0] != NULL ) {     
         if ( strcmp(command, helpstate[j][0]) == 0 ) {
            printf("Usage: heyu  %s %s  %s\n", helpstate[j][0], 
                helpstate[j][1], helpstate[j][2]);
            return;
         }
         j++;
      }

      for ( j = 0; j < nx10cmds; j++ ) {
	 if ( strcmp(command, x10command[j].label) == 0 )
            break;
      }
      if ( j < nx10cmds ) {
         printf("Usage: %s%s %s   %s%s\n",
            (x10command[j].flags & F_INT) ? "" : "heyu  ",
            x10command[j].label,
            helparg[x10command[j].argtype], helpdirect[x10command[j].help],
            (x10command[j].flags & (F_NMA | F_NMAI)) ? " (*)" : "");
      }
#if 0
      if ( j < nx10cmds ) {
         printf("Usage: heyu  %s %s   %s%s\n", x10command[j].label,
            helparg[x10command[j].argtype], helpdirect[x10command[j].help],
            (x10command[j].flags & (F_NMA | F_NMAI)) ? " (*)" : "");
      }
#endif
      else {
	 printf("Command '%s' not recognized.\n", command);
         return;
      }
      return;
   }
}


/*---------------------------------------------------------------+
 | Send a hail_ack (for use only by state engine)                |
 +---------------------------------------------------------------*/
void acknowledge_hail ( void )
{
   extern   char default_housecode;
   extern   int  heyu_parent;
   int      save_parent;
   unsigned char buf[4];
   unsigned char hcode;

   hcode = hc2code(default_housecode);

   buf[0] = 0x06;
   buf[1] = hcode << 4 | 9;

   /* Temporarily set heyu_parent so that ack will be sent */
   /* as if from a script.                                */

   save_parent = heyu_parent;
   heyu_parent = D_ENGINE;
   send_command(buf, 2, 0, NO_SYNC);
   heyu_parent = save_parent;

   return;
}

         
/*---------------------------------------------------------------+
 | Display a list of all commands and their synonyms.            |
 +---------------------------------------------------------------*/
void display_syn ( void ) 
{
   int j;
   int szlbl;
   char delim = ' ';
   struct cmd_list *one = NULL, *two;

   /* Get max label length for formatting output */
   szlbl = 0;
   for ( j = 0; j < nx10cmds; j++ ) {
      if ( x10command[j].flags & (F_HLP | F_HID) )
         continue;
      szlbl = max(szlbl, (int)strlen(x10command[j].label));
   }

   printf(" %-*s   %s\n", szlbl, "Command","Built-In Synonyms");
   printf(" %-*s   %s",   szlbl, "-------","-----------------");

   for ( j = 0; j < nx10cmds; j++ ) {
      two = &x10command[j];
      if ( x10command[j].flags & (F_HLP | F_HID) ) {
         /* Don't display hidden or purely help commands */
         continue;
      }
      if ( cmp_commands(one, two) ) {
         printf("\n %-*s ", szlbl, x10command[j].label);
         one = two;
         delim = ' ';
      }
      else {
         printf("%c %s", delim, x10command[j].label);
         delim = ','; 
      }
   }
   printf("\n");
   return;
}

	       

void cmd_usage ( FILE *fd, int index )
{
   fprintf(fd, "Usage: heyu %s %s\n", 
        x10command[index].label, helparg[x10command[index].argtype]);
}

/*---------------------------------------------------------------+
 | Send to the CM11A or merely verify the syntax of the command  |
 | represented by the argc tokens in list argv[] according to    |
 | input argument 'mode':                                        |
 |   mode = CMD_VERIFY   Merely verify the syntax                |
 |   mode = CMD_RUN      Actually send the command               |
 |   mode = CMD_INT      Run internal engine precommand          |
 | Return 0 if successful; 1 otherwise.                          |
 | (The CMD_VERIFY mode is intended for verifying the syntax     |
 | of scene commands in the x10config file.)                     | 
 +---------------------------------------------------------------*/

int direct_command( int argc, char *argv[], int mode )
{
	
#ifdef HASCM17A  /* Used only for CM17A commands */
    extern unsigned int rf_unit_code[16];
    extern unsigned int rf_func_code[7];
    extern unsigned int rf_nosw_code[8];
    extern int display_rf_xmit(unsigned char, unsigned int, int);
    extern int write_cm17a( unsigned int, int, unsigned char );
    extern int reset_cm17a( void );
    extern void rf_post_delay( void );
    extern void rf_farb_delay( void );
    extern void rf_farw_delay( void );
    extern void rf_flux_delay( long );

    int           precode;
    int           bursts, burstgroup, savebursts;
    int           byte1, byte2;
    unsigned int  rfword, rfonword, noswitch, nsw;
    unsigned char rfhcode, rfmode = RF_SLOW;
    long          lxdata;
    int           flux_delay;
#endif  /* End of HASCM17A block */

    extern int line_sync_mode( void );
    extern int send_flag_update ( unsigned int, unsigned char );
    extern int send_long_flag_update ( unsigned char, unsigned long, unsigned char );
    extern int clear_status_flags ( unsigned char, unsigned int );
    extern void send_settimer_msg ( unsigned short, long );
    extern int  munlock ( char * );
    extern int  lock_for_write ( void );
    extern int  parse_ext3_group ( char *, unsigned char *, unsigned char * );
    extern int  set_counter ( int, unsigned short, unsigned char );
    extern void send_setcounter_msg ( int, unsigned short, unsigned char );
    extern void engine_update_flags ( unsigned char, unsigned long, unsigned char );
    extern void engine_internal_vdata ( unsigned char, unsigned char, unsigned char, unsigned char );
    extern int  set_globsec_flags ( unsigned char ); 
    extern char *datstrf ( void ); 
    extern char *display_armed_status ( void );
    extern int  read_x10state_file ( void );
    int         get_armed_parms ( int, char **, unsigned char *, unsigned char );
    extern int    line_no;
    int           j, k, icmd, nargs, radix, subcode, subsubcode;
    unsigned int  mask, aflags, bitmap, sendmap;
    unsigned long flags;
    char          *label, *sp;
    char          hc;
    char          errmsg[128];
    int           dimval, dimcode, fullbright, timeout;
    long          delay, mindelay, maxdelay;
    double        pause;
    unsigned char cmdbuf[16], cmdcode, hcode, switchv, switchcode, testbuf[128];
    unsigned char vtype;
    int           minargs, maxargs; 
    int           xdata = 0;
    int           ramp;
    unsigned char group, subgroup;
    static char   reorder[] = {6,14,2,10,1,9,5,13,7,15,3,11,0,8,4,12};
    int           syncmode;
    int           timer_no;
    long          timer_countdown, min_countdown, max_countdown;
    int           value;
    long          longvalue;
    int           counter_no;
    unsigned short counter_value;
    char          writefilename[PATH_LEN + 1];
    unsigned char sflags;

int flaglist2banks ( unsigned long *flagbank, int banks, char *str );
unsigned long flagbank[NUM_FLAG_BANKS];

    timeout = 10;
    line_no = 0;
    flags = 0;
    special_func = 0;
    syncmode = line_sync_mode();

    clear_error_message();

    /* Handle a few special cases */

    if ( strcmp(argv[0], "function") == 0 ) {
       /* Special case: "function" command */
       if ( argc < 2 ) {
          store_error_message("Command 'function': Too few parameters");
          return 1;
       }
       argv++;
       argc--;
       flags |= F_NOA;
    }
    else if ( strcmp(argv[0], "turn") == 0 ) {
       /* Special case: "turn" command */
       if ( argc < 3 ) {
          store_error_message("Command 'turn': Too few parameters");
          return 1;
       }
       argv++;
       argc--;
       flags |= F_TRN;
       /* Swap first two parameters */
       sp = argv[0];
       argv[0] = argv[1];
       argv[1] = sp;
    }
    else if ( strcmp(argv[0], "temp_req") == 0 || 
              strcmp(argv[0], "rcs_req")  == 0 ) {
       if ( argc < 2 ) {
	  sprintf(errmsg, "Command '%s': Too few parameters", argv[0]);
          store_error_message(errmsg);
          return 1;
       }
       special_func = (mode == CMD_VERIFY) ? 0 : 1;
       argv++;
       argc--;
       flags |= F_SPF;
    }
    nargs = argc - 1;
     
    /* Identify the command */
    for ( icmd = 0; icmd < nx10cmds; icmd++ ) {
       if ( !strcmp(argv[0], x10command[icmd].label) )
          break;
    }
    if ( icmd == nx10cmds ) {
       sprintf(errmsg, "Invalid command '%s'", argv[0]);
       store_error_message(errmsg);
       return 1;
    }

    label    = x10command[icmd].label;
    minargs  = x10command[icmd].minargs;
    maxargs  = x10command[icmd].maxargs;
    flags   |= x10command[icmd].flags;
    cmdcode  = x10command[icmd].code;
    subcode  = x10command[icmd].subcode;
    subsubcode = x10command[icmd].subsubcode;
    radix    = (flags & F_ARB) ?  16 : 10;

    if ( flags & F_INT && mode != CMD_INT ) {
       sprintf(errmsg, "Invalid use of internal engine precommand '%s'", label);
       store_error_message(errmsg);
       return 1;
    }

    if ( (configp->device_type & DEV_DUMMY) && !(flags & (F_INT | F_DUM)) ) {
       sprintf(errmsg, "Command '%s' is not valid for TTY dummy", label);
       store_error_message(errmsg);
       return 1;
    }
           
#ifndef HASCM17A 
    /* CM17A support not included */
    if ( flags & F_FIR ) {
       sprintf(errmsg,
	  "Command '%s': Heyu was compiled without CM17A support.", label);
       store_error_message(errmsg);
       return 1;
    }
#else
    if ( flags & F_FIR && mode != CMD_VERIFY && !is_modem_support() ) {
       sprintf(errmsg,
       "Command '%s': CM17A operation is unsupported by serial port hardware.", label);
       store_error_message(errmsg);
       return 1;
    }
#endif /* End ifndef */

    /* Help menu items are invalid in scenes */
    if ( flags & F_HLP && mode == CMD_VERIFY ) {
       sprintf(errmsg, "Command '%s' is not valid in scenes/usersyns", label);
       store_error_message(errmsg);
       return 1;
    }
    
    /* If this is just a help menu item, return */
    if ( flags & F_HLP )
       return 0;

    /* Only commands with command code <= 6  or code == 26 or 34-7 are valid with "turn" */
    if ( flags & F_TRN && !(cmdcode <= 6 || cmdcode == 26 || (cmdcode == 34 && subcode <= 7)) ) {
       sprintf(errmsg, 
         "Command '%s' cannot be used with the 'turn' command", label); 
       store_error_message(errmsg);
       return 1;
    }

    if ( nargs < minargs ) {
       sprintf(errmsg, "Command '%s': Too few parameters", label);
       store_error_message(errmsg);
       return 1;
    }
    if ( maxargs != -1 && nargs > maxargs ) {
       sprintf(errmsg, "Command '%s': Too many parameters", label);
       store_error_message(errmsg);
       return 1;
    }

    /* Handle a few special cases */

    if ( flags & F_ARB ) {
       /* If arbitrary extended code, get subcode and adjust argument count */
       if ( strchr(argv[1], '$') && mode == CMD_VERIFY) {  
          /* Fake a dummy parameter for scene verification */
          subcode = 0x30;
          sp = " ";
       }
       else { 
          subcode = (int)strtol(argv[1], &sp, radix);
       }

       if ( !strchr(" \t\n", *sp) || subcode < 0 || subcode > 0xff ) {
          sprintf(errmsg,
             "Command '%s': Extended function code '%s' outside range 00-0xff",
                 label, argv[1]);
          store_error_message(errmsg);
          return 1;
       }
       argv++;
       argc--;
       nargs--;
    }

    
    /* Default HC|Unit */
    hc = '_';
    bitmap = 0;
    aflags = A_VALID;

    if ( !(flags & F_NUM) && nargs > 0 ) {
       /* parse it as an alias or HC|Units token. */
       aflags = parse_addr(argv[1], &hc, &bitmap);
    }

    /* Make sure a positional  parameter didn't slip through */
    if ( aflags & A_DUMMY && mode == CMD_RUN ) {
       sprintf(errmsg,
          "Command '%s': Unresolved positional parameter '%s'",
             label, argv[1]);
       store_error_message(errmsg);
       return 1;
    }
       
    if ( !(aflags & A_VALID) ) {
       sprintf(errmsg, "Command '%s': ", label);
       add_error_prefix(errmsg);
       return 1;
    }

    if ( !(aflags & A_HCODE) && !(flags & F_NUM) && minargs > 0 )  {
       sprintf(errmsg, "Command '%s': Housecode required", label);
       store_error_message(errmsg);
       return 1;
    }

    /* Heyu shouldn't wait for (x)status_ack when executed from a script */
    if ( heyu_parent == D_ENGINE )
       flags &= ~F_STA;

    switchv = cmdcode;

    switch ( switchv ) {

       case 21 : /* Function only - we should never get here */
       case 27 : /* Turn command - we should never get here */
       case 28 : /* Temp_Req - we should never get here */
       case 99 : /* Legacy commands - we should never get here */
          fprintf(stderr, "Internal error - direct_command() - case %d\n", switchv);
          return 1;

       case  8 :  /* Hail */
       case  9 :  /* Hail Ack */
          if ( !(aflags & A_HCODE) ) {
             hc = configp->housecode;
          }
       case  0 :  /* All Units Off  */
       case  1 :  /* All Lights On  */
       case  6 :  /* All Lights Off */
       case 12 :  /* Data Transfer */
          if ( bitmap != 0 && !(aflags & A_PLUS) && !(aflags & A_MINUS) && configp->force_addr == NO ) {
             sprintf(errmsg, "Unitcode ignored for '%s' command.", label);
             store_error_message(errmsg);
             bitmap = 0;
          }
          if ( bitmap == 0 && (aflags & A_PLUS || configp->force_addr == YES) )
             bitmap = 1;

       case  2 :  /* On */
       case  3 :  /* Off */
       case 13 :  /* Status On */
       case 14 :  /* Status Off */
          if ( aflags & A_MINUS || flags & F_NOA )
             bitmap = 0;
          else if ( bitmap == 0 && !(flags & F_ALL) ) {
             sprintf(errmsg, "Command '%s': No Unitcode specified", label);
             store_error_message(errmsg);
             return 1;
          }

          if ( mode == CMD_VERIFY )
             break;

          hcode = hc2code(hc);

          cmdbuf[0] = 0x06;
          cmdbuf[1] = (hcode << 4) | cmdcode;

                 
          if ( send_address(hcode, bitmap, syncmode) != 0 ||
               send_command(cmdbuf, 2, (flags & F_STA), syncmode) != 0 )
             return 1; 
          
          break;

       case 48 : /* Status Ack emulation (experimental) */
          if ( aflags & A_MINUS || bitmap == 0 ) {
             sprintf(errmsg, "Command '%s': No Unitcode specified", label);
             store_error_message(errmsg);
             return 1;
          }
          else if ( aflags & A_MULT ) {
             sprintf(errmsg, "Command '%s': Only a single unitcode is allowed", label);
             store_error_message(errmsg);
             return 1;
          }

          if ( mode == CMD_VERIFY )
             break;

          if ( read_x10state_file() != 0 ) {
             sprintf(errmsg, "Command '%s': Unable to read state file", label);
             store_error_message(errmsg);
             return 1;
          }

          hcode = hc2code(hc);

          cmdcode = (x10state[hcode].state[OnState] & bitmap) ? 13 : 14; /* StatusOn : StatusOff */
          
          cmdbuf[0] = 0x06;
          cmdbuf[1] = (hcode << 4) | cmdcode;
                 
          if ( send_address(hcode, bitmap, syncmode) != 0 ||
               send_command(cmdbuf, 2, (flags & F_STA), syncmode) != 0 )
             return 1; 
          
          break;

       case  4 :  /* Dim */
       case  5 :  /* Bright */
          if ( strchr(argv[2], '$') && mode == CMD_VERIFY ) {
             /* Fake a dim value for a dummy parameter */
             dimval = 10;
             sp = " ";
          }
          else {
             dimval = (int)strtol(argv[2], &sp, 10);
          }

          if ( !strchr(" \t\n", *sp) ) {
	     sprintf(errmsg,
		"Command '%s': Invalid Dim/Bright parameter '%s'", label, argv[2]);
             store_error_message(errmsg);
	     return 1;
	  }
	  else if ( configp->restrict_dims == YES && (dimval < 1 || dimval > 22) ) {
             sprintf(errmsg, 
                "Command '%s': Dim/Bright value outside range 1-22", label);
             store_error_message(errmsg);
	     return 1;
	  }
	  else if ( configp->restrict_dims == NO && (dimval < 0 || dimval > 31) ) {
             sprintf(errmsg, 
                "Command '%s': Dim/Bright value outside range 0-31", label);
             store_error_message(errmsg);
             return 1;
          }

          if ( aflags & A_MINUS || flags & F_NOA )
             bitmap = 0;
          else if ( bitmap == 0 ) {
             sprintf(errmsg, "Command '%s': No Unitcode specified", label);
             store_error_message(errmsg);
             return 1;
          }

          if ( mode == CMD_VERIFY )
             break;

          hcode = hc2code(hc);

	  fullbright = configp->full_bright;

          syncmode = subcode ? RISE_SYNC : syncmode ;
	  
	  /* Kluge for housecode D to circumvent 0x5A checksum problem */
	  /* when sending dim Dx 22. Set dim value to 23 instead.      */ 
	  if ( hcode == hc2code('D') && configp->fix_5a == YES ) {
	     fullbright = 23;
	     dimval = (dimval == 22) ? 23 : dimval;
	  }

          /* Address the device(s) */
          if ( send_address(hcode, bitmap, syncmode) != 0 ) {
             return 1;
          }

          /* If On is required (e.g., for WS467-1) send it */
          if ( flags & F_ONN ) {
             cmdbuf[0] = 0x06;
             cmdbuf[1] = (hcode << 4) | 0x02;
             if ( send_command(cmdbuf, 2, (flags & F_STA), syncmode) != 0 )
                return 1;
          }
 	  
          /* If "brighten before dimming" flag is set, emulate a CM11A macro */
          /* by first brightening to full brightness.                        */
          if ( flags & F_BRI ) {
             cmdbuf[0] = (fullbright << 3) | 0x06;
             cmdbuf[1] = (hcode << 4) | 5;

             cmdbuf[2] = (dimval << 3) | 0x06;
             cmdbuf[3] = (hcode << 4) | cmdcode;
             
             if ( send_command(cmdbuf, 2, (flags & F_STA), syncmode) != 0 ||
                  send_command(cmdbuf + 2, 2, (flags & F_STA), syncmode) != 0 ) {
                return 1;
             }
          }
          else {
             cmdbuf[0] = (dimval << 3) | 0x06;
             cmdbuf[1] = (hcode << 4) | cmdcode;

             if ( send_command(cmdbuf, 2, (flags & F_STA), syncmode) != 0 )
                return 1;
          }

          break;

       case 10 :  /* Old style preset 1 or preset_level 1 */
       case 11 :  /* Old style preset 2 or preset_level 2 */
          if ( aflags & A_MINUS || flags & F_NOA )
             bitmap = 0;
          else if ( bitmap == 0 ) {
             sprintf(errmsg, "Command '%s': No Unitcode specified", label);
             store_error_message(errmsg); 
             return 1;
          }

          /* (nargs = 2 for preset; nargs = 1 for preset_level) */

          if ( strchr(argv[nargs], '$') && mode == CMD_VERIFY ) {
             /* Fake a dim dummy parameter for scene verification */
             dimval = 10;
             sp = " ";
          }
          else
             dimval = (int)strtol(argv[nargs], &sp, 10);

          if ( !strchr(" \t\n", *sp) || dimval < 1 || dimval > 32 ) {
             sprintf(errmsg, "Command '%s': dim level outside range 1-32", label);
             store_error_message(errmsg);
             return 1;
          }

          if ( mode == CMD_VERIFY )
             break;

          if ( (dimcode = dimval) > 16 ) {
             cmdcode = 11;
             dimcode -= 16;
          }
          dimcode = rev_low_nybble(dimcode - 1);

          hcode = hc2code(hc);

          if ( flags & F_MAC && dimcode != hcode ) {
             sprintf(errmsg, "Command '%s': Preset level %d not supported for housecode %c",
                label, dimval, hc);
             store_error_message(errmsg);
             return 1;
          }

          cmdbuf[0] = 0x06;
          cmdbuf[1] = (dimcode << 4) | cmdcode;

          if ( send_address(hcode, bitmap, syncmode) != 0 ||
               send_command(cmdbuf, 2, (flags & F_STA), syncmode) != 0 )
             return 1;

          break;

       case 15 :  /* Status Req */
          sendmap = bitmap;
          if ( aflags & A_MINUS || flags & F_NOA ) {
             bitmap = 1;
             sendmap = 0;
          }
          else if ( bitmap == 0 ) {
             sprintf(errmsg, "Command '%s': No Unitcode specified", label);
             store_error_message(errmsg); 
             return 1;
          }

          if ( mode == CMD_VERIFY )
             break;

          hcode = hc2code(hc);
             
          cmdbuf[0] = 0x06;
          cmdbuf[1] = (hcode << 4) | cmdcode;

          if ( flags & F_SPF )
             flags &= ~F_STA;

          /* Send each address/command pair individually to allow */
          /* associating a response with a particular unit code   */
          mask = 1;
          for ( k = 0; k < 16; k++ )  {
             if ( bitmap & mask ) {
                if ( send_address(hcode, sendmap & mask, syncmode) != 0 ||
                     send_command(cmdbuf, 2, (flags & F_STA), syncmode) != 0 )
                   return 1;
             }
             mask = mask << 1;
          }         
          break;

       case 20 : /* Send address bytes only */
          /* Start over again with the address tokens */
          for ( k = 1; k < argc; k++ ) {
             aflags = parse_addr(argv[k], &hc, &bitmap);
             if ( !(aflags & A_VALID) ) {
                sprintf(errmsg, "Command '%s': ", label);
                add_error_prefix(errmsg);
                return 1;
             }
             else if ( !(aflags & A_HCODE) || bitmap == 0 ) {
                sprintf(errmsg, 
                   "Command '%s': invalid HU or Alias '%s'", label, argv[k]);
                store_error_message(errmsg); 
                return 1;
             }

             if ( mode == CMD_VERIFY )
                continue;

             hcode = hc2code(hc);

             if ( send_address(hcode, bitmap, syncmode) != 0 )
                return 1;

             /* A delay between addresses may be needed for */
             /* programming some SwitchLinc devices.  (The  */
             /* TempLinc programs OK without it.)           */
             millisleep(60);
          }
          break;

       case 29 : /* Send bytes directly to cm11a (highly experimental) */
       case 31 : /* Send bytes directly to cm11a for transmission (highly experimental) */
          /* First check that each arg is a byte */
          for ( k = 1; k < argc; k++ ) {
             if ( strchr(argv[k], '$') && mode == CMD_VERIFY ) {
                /* Fake a dummy value for scene verification */
                xdata = 10;
                sp = " ";
             }
             else {
                xdata = (int)strtol(argv[k], &sp, 16);
             }
             if ( !strchr(" \t\n", *sp) || xdata < 0 || xdata > 0xff ) {
                sprintf(errmsg, 
                   "Command '%s': Data '%s' outside range 0-FF", label, argv[k]);
                store_error_message(errmsg);
                return 1;
             }
          }

          if ( mode == CMD_VERIFY )
             break;

          for ( k = 1; k < argc; k++ ) {
             xdata = (int)strtol(argv[k], NULL, 16);
             testbuf[k-1] = xdata;
          }

	  if ( switchv == 29 )
             (void) xwrite(tty, (char *) testbuf, argc - 1);
          else
	     send_command(testbuf, argc - 1, 0, syncmode);
          break;


       case 22 : /* Send arbitrary series of hex bytes as addresses */
          /* First check that each byte is in range 00-FF */
          for ( k = 1; k < argc; k++ ) {
             if ( strchr(argv[k], '$') && mode == CMD_VERIFY ) {
                /* Fake a dummy value for scene verification */
                xdata = 10;
                sp = " ";
             }
             else {
                xdata = (int)strtol(argv[k], &sp, 16);
             }
             if ( !strchr(" \t\n", *sp) || xdata < 0 || xdata > 0xff ) {
                sprintf(errmsg, 
                   "Command '%s': Data '%s' outside range 0-FF", label, argv[k]);
                store_error_message(errmsg);
                return 1;
             }
          }

          if ( mode == CMD_VERIFY )
             break;

          for ( k = 1; k < argc; k++ ) {
             xdata = (int)strtol(argv[k], NULL, 16);
             if ( send_address(xdata >> 4, (1 << (xdata & 0x0f)), syncmode) != 0 )
                return 1;
          }
          break;

       case 23 : /* Send (quoted) text message */
          /* Format:                                                          */
          /* Send each character as two address bytes - high nybble first     */
          if ( !(aflags & A_DUMMY) && bitmap != 0 ) {
             sprintf(errmsg, "Unitcode ignored for '%s' command.", label);
             store_error_message(errmsg);
             bitmap = 0;
          } 
          sp = argv[2];

          if ( mode == CMD_VERIFY )
             break;

          hcode = hc2code(hc);

          printf("Sending %d characters as Hcode|hi Hcode|low bytes:\n", 
                (int)strlen(sp));

          while ( *sp != '\0' ) {
             if ( send_address(hcode, (1 << (*sp >> 4)), syncmode) != 0 ||
                  send_address(hcode, (1 << (*sp & 0x0f)), syncmode) != 0 )
                return 1;
             printf("%c", *sp);
             fflush(stdout);
             sp++;
          }
          printf("\n");
          break;

       case 24 :  /* Send All_Units_Off to all Housecodes */
          if ( mode == CMD_VERIFY )
             break;

          bitmap = (configp->force_addr == YES) ?  1 : 0;
          cmdbuf[0] = 0x06;
	  for ( j = 0; j < 16; j++ ) {
	     hcode = reorder[j];
	     cmdbuf[1] = (hcode << 4);
	     if ( send_address(hcode, bitmap, syncmode) != 0 ||
                  send_command(cmdbuf, 2, 0, syncmode) != 0 )
                return 1;
             millisleep(60);	     
	  }
	  break;

       case 25 : /* Pause (for use in scenes) */
          if ( strchr(argv[1], '$') && mode == CMD_VERIFY ) {
             /* Fake a dummy parameter for scene verification */
             pause = 1.0;
             sp = " ";
          }
          else {
             pause = strtod(argv[1], &sp);
          }
          if ( !strchr(" \t\n", *sp) || pause < 0.0 ) {
             sprintf(errmsg, "Command '%s': invalid Pause value '%s'", label, argv[1]);
             store_error_message(errmsg);
             return 1;
          }

          if ( mode == CMD_VERIFY )
             break;

          if ( verbose )
             printf("pause %.3f seconds\n", pause);
          millisleep((int)(1000. * pause));

          break;
      
       case 41 : /* Sleep (for use in scenes) */
          if ( strchr(argv[1], '$') && mode == CMD_VERIFY ) {
             /* Fake a dummy parameter for scene verification */
             pause = 1.0;
             sp = " ";
          }
          else {
             pause = strtod(argv[1], &sp);
          }
          if ( !strchr(" \t\n", *sp) || pause < 0.0 ) {
             sprintf(errmsg, "Command '%s': invalid sleep value '%s'", label, argv[1]);
             store_error_message(errmsg);
             return 1;
          }

          if ( mode == CMD_VERIFY )
             break;

          sprintf(writefilename, "%s%s", WRITEFILE, configp->suffix);
          munlock(writefilename);

          millisleep((int)(1000. * pause));

          if ( lock_for_write() != 0 ) {
             fprintf(stderr, "Unable to re-lock after sleep command\n");
             exit(1);
          }

          break;

       case 45 : /* Random delay in minutes */
          if ( strchr(argv[1], '$') && mode == CMD_VERIFY ) {
             /* Fake a dummy parameter for scene verification */
             delay = 1;
             sp = "";
          }
          else {
             delay = strtol(argv[1], &sp, 10);
          }

          if ( !strchr(" \t\n", *sp) || delay < 0 || delay > 240 ) {
             sprintf(errmsg, "Command '%s': Invalid delay time '%s'", label, argv[1]);
             store_error_message(errmsg);
             return 1;
          }

          if ( nargs == 1 ) {
             maxdelay = delay;
             mindelay = 0;
             sp = "";
          }
          else {
             mindelay = delay;
             if ( strchr(argv[2], '$') && mode == CMD_VERIFY ) {
                /* Fake a dummy parameter for scene verification */
                maxdelay = 5;
                sp = " ";
             }
             else {
                maxdelay = strtol(argv[2], &sp, 10);
             }
          }

          if ( !strchr(" \t\n", *sp) || maxdelay < 0 || maxdelay > 240 ) {
             sprintf(errmsg, "Command '%s': Invalid delay time '%s'", label, argv[1]);
             store_error_message(errmsg);
             return 1;
          }

          if ( maxdelay < mindelay ) {
             delay = maxdelay;
             maxdelay = mindelay;
             mindelay = delay;
          }

          if ( mode == CMD_VERIFY )
             break;

          delay = (double)(maxdelay - mindelay) * random_float() + 0.5;
          delay += mindelay;

          if ( !i_am_relay && !i_am_aux ) {
             printf("%s Random delay %ld minutes\n", datstrf(), delay);
             fflush(stdout);
          }

          sprintf(writefilename, "%s%s", WRITEFILE, configp->suffix);
          munlock(writefilename);

          millisleep(60000L * delay);

          if ( lock_for_write() != 0 ) {
             fprintf(stderr, "Unable to re-lock after rdelay command\n");
             exit(1);
          }

          break;
      
       case 32 : /* Delay in minutes */
          if ( strchr(argv[1], '$') && mode == CMD_VERIFY ) {
             /* Fake a dummy parameter for scene verification */
             delay = 1;
             sp = " ";
          }
          else {
             delay = strtol(argv[1], &sp, 10);
          }
          if ( !strchr(" \t\n", *sp) || delay < 0 || delay > 240 ) {
             sprintf(errmsg, "Command '%s': Invalid delay time '%s'", label, argv[1]);
             store_error_message(errmsg);
             return 1;
          }

          if ( mode == CMD_VERIFY )
             break;

          if ( verbose ) {
             printf("%s Delay %ld minutes\n", datstrf(), delay);
          }

          sprintf(writefilename, "%s%s", WRITEFILE, configp->suffix);
          munlock(writefilename);

          millisleep(60000L * delay);

          if ( lock_for_write() != 0 ) {
             fprintf(stderr, "Unable to re-lock after delay command\n");
             exit(1);
          }

          break;

       case 33 :  /* Wait for next tick of system clock */
	  if ( mode == CMD_VERIFY )
	     break;

	  wait_next_tick();

	  break;
      
       case 26 :  /* All Units On */
          if ( bitmap != 0 && !(aflags & A_MINUS) ) {
             sprintf(errmsg, "Unitcode ignored for '%s' command.", label);
             store_error_message(errmsg);
          }
          bitmap = 0xFFFF;

          if ( mode == CMD_VERIFY )
             break;

          hcode = hc2code(hc);

          cmdbuf[0] = 0x06;
          cmdbuf[1] = (hcode << 4) | 2 ;
                 
          if ( send_address(hcode, bitmap, syncmode) != 0 ||
               send_command(cmdbuf, 2, (flags & F_STA), syncmode) != 0 )
             return 1; 
          
          break;

       case 30 :  /* Show X10 state */
          if ( mode == CMD_VERIFY )
             break;

          hcode = hc2code(hc);

          cmdbuf[0] = 0x7b;
          cmdbuf[1] = 0;
          cmdbuf[2] = hcode << 4;

          xwrite(tty, cmdbuf, 3);
          millisleep(100);

          break;

       case 35 : /* Set or Clear flags */
 
          if ( strchr(argv[1], '$') && mode == CMD_VERIFY ) {
             /* Fake a dummy parameter for scene verification */
             sp = "1";
          }
          else {
             sp = argv[1];
          }

          if ( flaglist2banks(flagbank, NUM_FLAG_BANKS, sp) ) {
             return 1;
          }

          if ( mode == CMD_VERIFY )
             break;

          if ( flags & F_INT ) {
             for ( j = 0; j < NUM_FLAG_BANKS; j++ ) {
                if ( flagbank[j] )
                   engine_update_flags(j, flagbank[j], subcode);
             }
             break;
          }

          if ( verbose ) {
             if ( subcode == SETFLAG )
                printf("Set flags %s\n", argv[1]);
             else
                printf("Clear flags %s\n", argv[1]);
          }

          for ( j = 0; j < NUM_FLAG_BANKS; j++ ) {
             if ( flagbank[j] )
                send_long_flag_update(j, flagbank[j], subcode);
          }

          break;
#if 0
       case 35 : /* Set or Clear flags */ 
          if ( strchr(argv[1], '$') && mode == CMD_VERIFY ) {
             /* Fake a dummy parameter for scene verification */
             longmap = 1;
          }
          else if ( (longmap = flags2longmap(argv[1])) == 0 ) {
/*
             sprintf(errmsg, "Command '%s': invalid flags specified '%s'", label, argv[1]);
             store_error_message(errmsg);
*/
             return 1;
          }

          if ( mode == CMD_VERIFY )
             break;

          if ( flags & F_INT ) {
             engine_update_flags(0, longmap, subcode);
             break;
          }

          if ( verbose ) {
             if ( subcode == SETFLAG )
                printf("Set flags %s\n", argv[1]);
             else
                printf("Clear flags %s\n", argv[1]);
          }

          send_long_flag_update(0, longmap, subcode);

          break;
#endif

       case 36 : /* Clear status flags */
          if ( mode == CMD_VERIFY )
             break;

          if ( aflags & A_MINUS || bitmap == 0 )
             bitmap = 0xffff;

          if ( flags & F_INT ) {
             clear_statusflags(hc2code(hc), bitmap);
             break;
          }

          send_clear_statusflags(hc2code(hc), bitmap);

          break;

       case 37 : /* Virtual Data */
	  if ( aflags & A_PLUS || aflags & A_MINUS ) {
	     sprintf(errmsg,
	       "Command '%s': '+/-' prefixes invalid for vdata[x] command.", label);
	     store_error_message(errmsg);
	     return 1;
	  }
	  if ( flags & F_NOA ) {
             sprintf(errmsg, "'function' is invalid for vdata[x] command.");
             store_error_message(errmsg);
             return 1;	     
	  }
          if ( strchr(argv[2], '$') && mode == CMD_VERIFY ) {
             /* Fake a dim value for a dummy parameter */
             dimval = 10;
             sp = " ";
          }
          else {
             strlower(argv[2]);
             if ( strncmp(argv[2], "0x", 2) == 0 )
                dimval = (int)strtol(argv[2], &sp, 16);
             else
                dimval = (int)strtol(argv[2], &sp, 10);
          }

          if ( !strchr(" \t\n", *sp) || dimval < 0 || dimval > 0xff ) {
	     sprintf(errmsg,
		"Command '%s': Invalid virtual data parameter '%s'", label, argv[2]);
             store_error_message(errmsg);
	     return 1;
	  }

          if ( mode == CMD_VERIFY )
             break;

          hcode = hc2code(hc);
          vtype = (subcode == 1) ? RF_VDATAM : RF_STD;

          if ( flags & F_INT ) {
             mask = 1;
             for ( j = 0; j < 16; j++ ) {
                if ( bitmap & mask ) {
                   engine_internal_vdata(hcode, j, dimval, vtype);
                }
                mask = mask << 1;
             }
             if ( i_am_state )
                write_x10state_file();

             break;
            
          }

          mask = 1;
          for ( j = 0; j < 16; j++ ) {
             if ( bitmap & mask ) {
                send_virtual_data((hcode << 4) | j, dimval, vtype, 0, 0, 0, 0);
             }
             mask = mask << 1;
          }

          break;

       case 38 : /* Set user countdown timer */
          if ( strchr(argv[1], '$') && mode == CMD_VERIFY ) {
             /* Fake a dummy parameter for scene verification */
             timer_no = 1;
          }
          else {
             timer_no = (int)strtol(argv[1], &sp, 10);
             if ( !strchr(" \t\n\r", *sp) ) {
                sprintf(errmsg,
		 "Command '%s': Invalid timer number '%s'", label, argv[1]);
                store_error_message(errmsg);
                return 1;
             }
          }
        
          if ( timer_no < 1 || timer_no > NUM_USER_TIMERS ) {
             sprintf(errmsg,
                "Command '%s': Timer number outside range 1-%d", label, NUM_USER_TIMERS);
             store_error_message(errmsg);
             return 1;
          }


          if ( strchr(argv[2], '$') && mode == CMD_VERIFY ) {
             /* Fake a dummy parameter for scene verification */
             timer_countdown = 10;
          }
          else {
             timer_countdown = parse_hhmmss(argv[2], 3);
             if ( timer_countdown < 0 ) {
                sprintf(errmsg,
		 "Command '%s': Invalid timer countdown '%s'", label, argv[2]);
                store_error_message(errmsg);
                return 1;
             }
          }

          if ( mode == CMD_VERIFY )
             break;

          if ( flags & F_INT ) {
             set_user_timer_countdown(timer_no, timer_countdown);
             display_settimer_setting(timer_no, timer_countdown);
             break;
          }

          send_settimer_msg(timer_no, timer_countdown);

          break;

       case 46 : /* Set user random countdown timer */
          if ( strchr(argv[1], '$') && mode == CMD_VERIFY ) {
             /* Fake a dummy parameter for scene verification */
             timer_no = 1;
          }
          else {
             timer_no = (int)strtol(argv[1], &sp, 10);
             if ( !strchr(" \t\n\r", *sp) ) {
                sprintf(errmsg,
		 "Command '%s': Invalid timer number '%s'", label, argv[1]);
                store_error_message(errmsg);
                return 1;
             }
          }
        
          if ( timer_no < 1 || timer_no > NUM_USER_TIMERS ) {
             sprintf(errmsg,
                "Command '%s': Timer number outside range 1-%d", label, NUM_USER_TIMERS);
             store_error_message(errmsg);
             return 1;
          }


          if ( strchr(argv[2], '$') && mode == CMD_VERIFY ) {
             /* Fake a dummy parameter for scene verification */
             timer_countdown = 10;
          }
          else {
             /* Get first time parameter */
             timer_countdown = parse_hhmmss(argv[2], 3);
             if ( timer_countdown < 0 ) {
                sprintf(errmsg,
		 "Command '%s': Invalid timer countdown '%s'", label, argv[2]);
                store_error_message(errmsg);
                return 1;
             }
          }

          if ( nargs == 2 ) {
             max_countdown = timer_countdown;
             min_countdown = 1;
          }
          else {
             min_countdown = timer_countdown;
             if ( strchr(argv[3], '$') && mode == CMD_VERIFY ) {
                max_countdown = 5;
             }
             else {
                max_countdown = parse_hhmmss(argv[3], 3);
                if ( max_countdown < 0 ) {
                   sprintf(errmsg,
                    "Command '%s': Invalid timer countdown '%s'", label, argv[2]);
                   store_error_message(errmsg);
                   return 1;
                }
             }
          }
                
          if ( max_countdown < min_countdown ) {
             longvalue = min_countdown;
             min_countdown = max_countdown;
             max_countdown = longvalue;
          }
 
          if ( mode == CMD_VERIFY )
             break;

          timer_countdown = (double)(max_countdown - min_countdown) * random_float() + 0.5;
          timer_countdown += min_countdown;

          if ( flags & F_INT ) {
             set_user_timer_countdown(timer_no, timer_countdown);
             display_settimer_setting(timer_no, timer_countdown);
             break;
          }

          send_settimer_msg(timer_no, timer_countdown);

          break;

       case 39 : /* Reset all user timers */

          if ( mode == CMD_VERIFY )
             break;

          if ( flags & F_INT ) {
             reset_user_timers();
             break;
          }

          send_x10state_command(ST_CLRTIMERS, 0);

          break;

       case 40 : /* Clear tamper flags */

          if ( mode == CMD_VERIFY )
             break;

          send_x10state_command(ST_CLRTAMPER, 0);

          break;

	  
       case 42 : /* Set counter */
          if ( strchr(argv[1], '$') && mode == CMD_VERIFY ) {
             /* Fake a dummy parameter for scene verification */
             counter_no = 1;
          }
          else {
             counter_no = (int)strtol(argv[1], &sp, 10);
             if ( !strchr(" \t\n\r", *sp) ) {
                sprintf(errmsg,
		 "Command '%s': Invalid counter number '%s'", label, argv[1]);
                store_error_message(errmsg);
                return 1;
             }
          }
        
          if ( counter_no < 1 || counter_no > (32 * NUM_COUNTER_BANKS) ) {
             sprintf(errmsg,
                "Command '%s': Counter number outside range 1-%d", label, 32 * NUM_COUNTER_BANKS);
             store_error_message(errmsg);
             return 1;
          }

          counter_value = 10;
          if ( subcode == CNT_SET ) {
             if ( strchr(argv[2], '$') && mode == CMD_VERIFY ) {
                /* Fake a dummy parameter for scene verification */
                counter_value = 10;
             }
             else  {
                value = (int)strtol(argv[2], &sp, 10);
                if ( !strchr(" \t\n\r", *sp) || value < 0 || value > 0xffff ) {
                   sprintf(errmsg,
                      "Command '%s': Invalid counter value '%s'", label, argv[2]);
                   store_error_message(errmsg);
                   return 1;
                }
                counter_value = (unsigned short)value;
             }
          }

          if ( mode == CMD_VERIFY )
             break;

          if ( flags & F_INT ) {
             return set_counter(counter_no, counter_value, subcode);
             break;
          }

          send_setcounter_msg(counter_no, counter_value, subcode);

          break;

       case 43 : /* Arm or Disarm system */
          if ( subcode == CLRFLAG ) {
             sflags = 0;
          }
          else {
             if ( get_armed_parms(argc, argv, &sflags, mode) != 0 ) 
                return 1;
          }

          if ( mode == CMD_VERIFY )
             break;

          if ( subcode == SETFLAG ) {
             read_x10state_file();
             warn_zone_faults(stdout, "");
          }

          if ( flags & F_INT ) {             
             set_globsec_flags(sflags);
             write_x10state_file();
             printf("%s %s\n", datstrf(), display_armed_status());
             fflush(stdout);
          }
          else {
             send_x10state_command(ST_SECFLAGS, (unsigned char)sflags);
          }

          break;

       case 47 : /* Null direct command (does nothing) */
       case 44 : /* Null internal precommand (does nothing) */
            break;

       case  7 :  /* Extended Code commands */
          sendmap = (aflags & A_PLUS) ? bitmap : 0 ;
          if ( sendmap == 0 && configp->force_addr == YES ) 
             sendmap = 0xFFFF;

          hcode = hc2code(hc);

          if ( flags & F_ARB ) 
             switchcode = 0xff;
          else
             switchcode = subcode;

          switch ( switchcode ) {
             case 0x30 :  /* Include in group[.subgroup] at current level */
                if ( bitmap == 0 ) {
                   sprintf(errmsg, "Command '%s': No Unitcode specified", label);
                   store_error_message(errmsg);
                   return 1;
                }

                /* Get group */
                if ( strchr(argv[2], '$') && mode == CMD_VERIFY ) {
                   /* Fake a dummy parameter for scene verification */
                   group = 0;
                   subgroup = 0;
                }
                else {
                   if ( parse_ext3_group(argv[2], &group, &subgroup) != 0 ) {
                      sprintf(errmsg, "Command '%s': %s", label, error_message());
                      store_error_message(errmsg);
                      return 1;
                   }
                }

                xdata = group << 6;

                if ( subgroup & 0x80u ) {
                   xdata |= (subgroup & 0x0fu) | 0x20u;                   
                }
                break;

             case 0x32 : /* Include in group at level */
                if ( bitmap == 0 ) {
                   sprintf(errmsg, "Command '%s': No Unitcode specified", label);
                   store_error_message(errmsg);
                   return 1;
                }

                /* Get group */
                if ( strchr(argv[2], '$') && mode == CMD_VERIFY ) {
                   /* Fake a dummy parameter for scene verification */
                   group = 0;
                   subgroup = 0;
                }
                else {
                   if ( parse_ext3_group(argv[2], &group, &subgroup) != 0 ) {
                      sprintf(errmsg, "Command '%s': %s", label, error_message());
                      store_error_message(errmsg);
                      return 1;
                   }
                }
                if ( subgroup & 0x80u ) {
                   sprintf(errmsg,
                     "Command '%s': Cannot specify level with group.subgroup", label);
                   store_error_message(errmsg);
                   return 1;
                }

                /* Get level 0-63 */
                if ( strchr(argv[3], '$') && mode == CMD_VERIFY ) {
                   /* Fake a dummy parameter for scene verification */
                   dimval = 10;
                   sp = " ";
                }
                else {
                   dimval = (int)strtol(argv[3], &sp, 10);
                }

                if ( !strchr(" \t\n", *sp) || dimval < 0 || dimval > 63 ) {
                   sprintf(errmsg,
                     "Command '%s': Extended Preset level outside range 0-63", label);
                   store_error_message(errmsg);
                   return 1;
                }
                xdata = (group << 6) | dimval;
                break;

             case 0x31 :  /* Extended Preset Dim */
                if ( bitmap == 0 ) {
                   sprintf(errmsg, "Command '%s': No Unitcode specified", label);
                   store_error_message(errmsg);
                   return 1;
                }

                /* Get dim level */
                if ( strchr(argv[2], '$') && mode == CMD_VERIFY ) {
                   /* Fake a dummy parameter for scene verification */
                   xdata = 10;
                   sp = " ";
                }
                else {
                   xdata = (int)strtol(argv[2], &sp, 10);
                }
  
                if ( !strchr(" \t\n", *sp) || xdata < 0 || xdata > 63 ) {
                   sprintf(errmsg,
                     "Command '%s': Extended Preset level outside range 0-63", label);
                   store_error_message(errmsg);
                   return 1;
                }

                /* Get ramp rate */
                ramp = 0;
                if ( nargs == 3 ) {
                   if ( strchr(argv[3], '$') && mode == CMD_VERIFY ) {
                      /* Fake a dummy parameter for scene verification */
                      ramp = 0;
                      sp = " ";
                   }
                   else {
                      ramp = (int)strtol(argv[3], &sp, 10);
                   }
                }
                if ( !strchr(" \t\n", *sp) || ramp < 0 || ramp > 3 ) {
                   sprintf(errmsg,
                     "Command '%s': Extended Preset ramp rate outside range 0-3", label);
                   store_error_message(errmsg);
                   return 1;
                }
                xdata |= ramp << 6;

                break;

             case 0x35 : /* Remove from extended group(s) */
                if ( strchr(argv[2], '$') && mode == CMD_VERIFY ) {
                   xdata = 0x01;
                }
                else if ( (xdata = (int)list2linmap(argv[2], 0, 3)) == 0 ) {
                   sprintf(errmsg, "Command '%s': Group number %s", label, error_message());
                   store_error_message(errmsg);
                   return 1;
                }

                if ( subsubcode == 0 ) {
                   if ( bitmap == 0 ) {
                      sprintf(errmsg, "Command '%s': No Unitcode specified", label);
                      store_error_message(errmsg);
                      return 1;
                   }
                }
                else {
                   xdata |= 0xf0;
                   if ( bitmap != 0 && !(aflags & A_PLUS) && !(aflags & A_MINUS) &&
                        configp->force_addr == NO) {
                      sprintf(errmsg, "Unitcode ignored for '%s' command.", label);
                      store_error_message(errmsg);
                      *errmsg = '\0';
                      bitmap = 1;
                   }
                   else if ( bitmap == 0 ) {
                      bitmap = 1;
                   }
                }

                if ( aflags & A_PLUS )
                   sendmap = bitmap;

                break;
                
             case 0x36 : /* Execute group function */
             case 0x3c : /* Extended Group Dim/Bright */
                if ( bitmap != 0 && !(aflags & A_PLUS) && !(aflags & A_MINUS) ) {
                   sprintf(errmsg, "Unitcode ignored for '%s' command.", label);
                   store_error_message(errmsg);
                   bitmap = 1;
                }
                else if ( bitmap == 0 )
                   bitmap = 1;

                if ( aflags & A_PLUS )
                   sendmap = bitmap;

                /* Get group */
                if ( strchr(argv[2], '$') && mode == CMD_VERIFY ) {
                   /* Fake a dummy parameter for scene verification */
                   group = 0;
                   subgroup = 0;
                   sp = " ";
                }
                else {
                   if ( parse_ext3_group(argv[2], &group, &subgroup) != 0 ) {
                      sprintf(errmsg, "Command '%s': %s", label, error_message());
                      store_error_message(errmsg);
                      return 1;
                   }
                }

                xdata = (group << 6) | (subsubcode << 4);
                if ( subgroup & 0x80u ) {
                   xdata |= 0x20u | (subgroup & 0x0f);
                }
                break;

             case 0x3b : /* Set module auto status_ack mode */
                if ( bitmap != 0 && !(aflags & A_PLUS) && !(aflags & A_MINUS) ) {
                   sprintf(errmsg, "Unitcode ignored for '%s' command.", label);
                   store_error_message(errmsg);
                   bitmap = 1;
                }
                else if ( bitmap == 0 )
                   bitmap = 1;

                if ( aflags & A_PLUS )
                   sendmap = bitmap;

                /* Get mode (0-3) */
                if ( strchr(argv[2], '$') && mode == CMD_VERIFY ) {
                   /* Fake a dummy parameter for scene verification */
                   xdata = 3;
                   sp = " ";
                }
                else {
                   xdata = (int)strtol(argv[2], &sp, 10);
                }
  
                if ( !strchr(" \t\n", *sp) || xdata < 0 || xdata > 3 ) {
                   sprintf(errmsg,
                     "Command '%s': Extended Config Mode outside range 0-3", label);
                   store_error_message(errmsg);
                   return 1;
                }
                break;

             case 0xfe :  /* Extended ON ( = Extended Preset Dim level 63) */
                if ( bitmap == 0 ) {
                   sprintf(errmsg, "Command '%s': No Unitcode specified", label);
                   store_error_message(errmsg);
                   return 1;
                }
                subcode = 0x31;
                xdata = 63;
                break;                

             case 0xfd :  /* Extended OFF ( = Extended Preset Dim level 0) */
                if ( bitmap == 0 ) {
                   sprintf(errmsg, "command '%s': No Unitcode specified", label);
                   store_error_message(errmsg);
                   return 1;
                }
                subcode = 0x31;
                xdata = 0;
                break;                

             case 0x33 :  /* Extended All On */
             case 0x34 :  /* Extended All Off */
             case 0x04 :  /* Shutter Open All */
             case 0x0B :  /* Shutter Close All */

                if ( bitmap != 0 && !(aflags & A_PLUS) && !(aflags & A_MINUS) ) {
                   sprintf(errmsg, "Unitcode ignored for '%s' command.", label);
                   store_error_message(errmsg);
                   bitmap = 1;
                }
                else if ( bitmap == 0 )
                   bitmap = 1;

                if ( aflags & A_PLUS )
                   sendmap = bitmap;

                xdata = 0;
                break;

             case 0x37 : /* Extended Status Requests */
                if ( subsubcode == 0 ) {
                   /* Extended Output Status Req */
                   if ( bitmap == 0 ) {
                      sprintf(errmsg, "Command '%s': No Unitcode specified", label);
                      store_error_message(errmsg);
                      return 1;
                   }
                   xdata = 0;
                }
                else if ( subsubcode == 1 ) {                
                   /* Extended Power Up */
                   if ( bitmap == 0 ) {
                      sprintf(errmsg, "Command '%s': No Unitcode specified", label);
                      store_error_message(errmsg);
                      return 1;
                   }
                   xdata = 0x10;
                }
                else  {
                   /* Group Status Req */
                   /* Get group */
                   if ( strchr(argv[2], '$') && mode == CMD_VERIFY ) {
                      /* Fake a dummy parameter for scene verification */
                      group = 0;
                      subgroup = 0;
                   }
                   else  {
                      if ( parse_ext3_group(argv[2], &group, &subgroup) != 0 ) {
                         sprintf(errmsg, "Command '%s': %s", label, error_message());
                         store_error_message(errmsg);
                         return 1;
                      }
                      subsubcode = (subgroup & 0x80u) ? 3 : 2;
                   }
                   xdata = (group << 6) | (subsubcode << 4) | (subgroup & 0x0fu);
                }
                break;


             case 0x01 : /* Shutter open, observe limit */
             case 0x03 : /* Shutter open, disregard limit */
             case 0x02 : /* Shutter set limit */
                if ( bitmap == 0 ) {
                   sprintf(errmsg, "Command '%s': No Unitcode specified", label);
                   store_error_message(errmsg);
                   return 1;
                }
                /* Get dim level */
                if ( strchr(argv[2], '$') && mode == CMD_VERIFY ) {
                   /* Fake a dummy parameter for scene verification */
                   xdata = 10;
                   sp = " ";
                }
                else {
                   xdata = (int)strtol(argv[2], &sp, 10);
                }
  
                if ( !strchr(" \t\n", *sp) || xdata < 0 || xdata > 25 ) {
                   sprintf(errmsg,
                     "Command '%s': Shutter level outside range 0-25", label);
                   store_error_message(errmsg);
                   return 1;
                }
                break;


             case 0xff :  /* Arbitrary Extended Function */
                if ( bitmap == 0 ) {
                   sprintf(errmsg, "Command '%s': No Unitcode specified", label);
                   store_error_message(errmsg);
                   return 1;
                }
                if ( strchr(argv[2], '$') && mode == CMD_VERIFY ) {
                   /* Fake a dummy parameter for scene verification */
                   xdata = 10;
                   sp = " ";
                }
                else {
                   xdata = (int)strtol(argv[2], &sp, 16);
                }
                if ( !strchr(" \t\n", *sp) || xdata < 0 || xdata > 0xff ) {
                   sprintf(errmsg,
                      "Command '%s': Data '%s' outside range 0-FF", label, argv[2]);
                   store_error_message(errmsg); 
                   return 1;
                }
                break ;

             default :
                sprintf(errmsg, "Command '%s': Not implemented", label);
                store_error_message(errmsg);
                return 1;                
          }

          if ( mode == CMD_VERIFY )
             break;

          /* Header for extended code */
          cmdbuf[0] = 0x07;

          /* House code and extended code function */
          cmdbuf[1] = (hcode << 4) | 0x07 ;

          /* Unit code */
          /* cmdbuf[2] = unit code is filled in below */

          /* "data" value */
          cmdbuf[3] = (unsigned char)(xdata & 0xff);

          /* Extended Type/command */
          cmdbuf[4] = (unsigned char)(subcode & 0xff);


          mask = 1;
          for ( j = 0; j < 16; j++ ) {
             if ( bitmap & mask ) {
                cmdbuf[2] = j;

		/* Kluge "fix" for checksum 5A problem.    */
		/* CM11A seems to disregard the dim field  */
		/* in the header byte.                     */
		if ( checksum(cmdbuf, 5) == 0x5A && configp->fix_5a == YES )
		   cmdbuf[0] = 0x0F;
		
                if ( send_address(hcode, sendmap & mask, syncmode) != 0 ||
                     send_command(cmdbuf, 5, flags & F_STA, syncmode) != 0 )
                   return 1;
             }
             mask = mask << 1;
          }
             
          break;

#ifdef HASCM17A	  
       case 34 : /* CM17A "Firecracker" RF commands */
          noswitch = 0;

	  if ( aflags & A_PLUS && !(aflags & A_DUMMY) ) {
	     sprintf(errmsg,
	       "Command '%s': '+' prefix invalid for RF commands.", label);
	     store_error_message(errmsg);
	     return 1;
	  }
	  if ( flags & F_NOA ) {
             sprintf(errmsg, "'function' is invalid for RF commands.");
             store_error_message(errmsg);
             return 1;	     
	  }
	  if ( flags & F_FFM )
	     rfmode = RF_FAST;
		     
	  switch ( subcode ) {
	     case 0 :  /* RF All Units Off */
	     case 1 :  /* RF All Lights On */
	     case 6 :  /* RF All Lights Off */
		if ( bitmap != 0 && !(aflags & A_MINUS) && !(aflags & A_PLUS)) {
                   sprintf(errmsg, "Unitcode ignored for '%s' command.", label);
                   store_error_message(errmsg);
		}

		if ( mode == CMD_VERIFY )
		   return 0;

                if ( configp->rf_noswitch == YES )
                   noswitch = rf_nosw_code[subcode];
		
                bursts = configp->rf_bursts[subcode];
		
		rfhcode = rev_low_nybble(hc2code(hc));
		
		rfword = (rfhcode << 12) | rf_func_code[subcode] | noswitch;
                display_rf_xmit(subcode, rfword, bursts);
		if ( write_cm17a(rfword, bursts, rfmode) != 0 ) {
                   sprintf(errmsg,
		      "Command '%s': Unable to write to CM17A device.", label);
                   store_error_message(errmsg);
                   return 1;
                }
		rf_post_delay();

		break;

	     case 2 :  /* RF On */
	     case 3 :  /* RF Off */
		if ( aflags & A_MINUS ) {
		   sprintf(errmsg,
		     "Command '%s': '-' prefix invalid for this command.\n", label);
		   store_error_message(errmsg);
		   return 1;
                }
		
		if ( bitmap == 0 ) {
                   sprintf(errmsg, "Command '%s': No Unitcode specified", label);
                   store_error_message(errmsg);
                   return 1;
                }

                if ( mode == CMD_VERIFY )
                   return 0;
		
                if ( configp->rf_noswitch == YES )
                   noswitch = rf_nosw_code[subcode];

                bursts = configp->rf_bursts[subcode];
		
		rfhcode = rev_low_nybble(hc2code(hc));

		mask = 1;
                for ( j = 0; j < 16; j++ ) {
                   if ( bitmap & mask ) {
                      nsw = (bitmap & 0x00c0) ? noswitch : 0;  /* Units 1 or 9 */
		      rfword = (rfhcode << 12) | rf_unit_code[j] | rf_func_code[subcode] | nsw;
                      display_rf_xmit(subcode, rfword, bursts);
		      if ( write_cm17a(rfword, bursts, rfmode) != 0 ) {
                         sprintf(errmsg,
			   "Command '%s': Unable to write to CM17A device.", label);
                         store_error_message(errmsg);
                         return 1;
                      }
		      rf_post_delay();
		   }
		   mask = mask << 1;
		}

		break;

	     case 7 :  /* RF Dim from full bright (turn Off before dimming) */
		
		if ( aflags & A_MINUS ) {
		   sprintf(errmsg,
                      "Command '%s': '-' prefix invalid for this command", label);
		   store_error_message(errmsg);
		   return 1;
		}
		if ( bitmap == 0 ) {
                   sprintf(errmsg,
		     "Command '%s': No Unitcode specified", label);
                   store_error_message(errmsg);
                   return 1;
		}
		
	     case 4 :  /* RF Dim */
	     case 5 :  /* RF Bright */

		if ( (aflags & A_MINUS) )
		   bitmap = 0;

		sp = " ";
		if ( strchr(argv[2], '$') && mode == CMD_VERIFY )
		   bursts = 1;
		else  
		   bursts = strtol(argv[2], &sp, 10);
		
		if ( !strchr(" \t\n", *sp) || bursts < 1 || bursts > 255 ) {
                   sprintf(errmsg, "Command '%s': Invalid argument '%s'", label, argv[2]);
                   store_error_message(errmsg);
                   return 1;
                }

                if ( mode == CMD_VERIFY )
		   break;

		if ( subcode == 7 ) {
		   subcode = 4;
		   precode = 3;   /* Turn Off before dimming */
		}
		else
	           precode = 2;   /* Turn On before dimming */

                if ( configp->rf_noswitch == YES ) 
                   noswitch = rf_nosw_code[precode];

		burstgroup = min(bursts, (int)configp->rf_bursts[subcode]);
		
		if ( rfmode == RF_FAST ) 
		   burstgroup = bursts;

		rfhcode = rev_low_nybble(hc2code(hc));
		
		if ( bitmap == 0 ) {
                   /* Transmit dims/brights only */
		   rfword = (rfhcode << 12) | rf_func_code[subcode];
                   display_rf_xmit(subcode, rfword, bursts);
		   while ( bursts > 0 ) {
                      if ( write_cm17a(rfword, min(bursts, burstgroup), rfmode) != 0 ) {
                         sprintf(errmsg,
                            "Command '%s': Unable to write to CM17A device.", label);
                         store_error_message(errmsg);
                         return 1;
                      }
                      rf_post_delay();
		      bursts -= burstgroup;
                   }
		}
		else {
                   /* Precede dims/brights with an On (or Off) command to set the address. */
		   mask = 1;
		   savebursts = bursts;
		   for ( j = 0; j < 16; j++ ) {
                      if ( bitmap & mask ) {
                         nsw = (bitmap & 0x00c0) ? noswitch : 0;  /* Units 1 or 9 */
		         rfonword = (rfhcode << 12) | rf_unit_code[j] | rf_func_code[precode] | nsw;
                         display_rf_xmit(precode, rfonword, configp->rf_bursts[precode]);
			 rfword = (rfhcode << 12) | rf_func_code[subcode];			 
                         display_rf_xmit(subcode, rfword, bursts);
                         if ( write_cm17a(rfonword, configp->rf_bursts[precode], RF_SLOW) != 0 ) {
                            sprintf(errmsg,
			      "Command '%s': Unable to write to CM17A device.", label);
                            store_error_message(errmsg);
                            return 1;
                         }
		         rf_post_delay();

			 while ( bursts > 0 ) {
                            if ( write_cm17a(rfword, min(bursts, burstgroup), rfmode) != 0 ) {
                               sprintf(errmsg,
				 "Command '%s': Unable to write to CM17A device.", label);
                               store_error_message(errmsg);
                               return 1;
                            }
		            rf_post_delay();
			    bursts -= burstgroup;
			 }
                      }
		      mask = mask << 1;
		      bursts = savebursts;
		   }
		}

		break;

	     case 9 : /* RF Arbitary 2-byte code */
		byte1 = byte2 = 0;
		sp = " ";
		if ( strchr(argv[1], '$') && mode == CMD_VERIFY )
		   byte1 = 0x10;
		else 
		   byte1 = strtol(argv[1], &sp, 16);
		if ( !strchr(" \t\n", *sp) || byte1 < 0 || byte1 > 0xff ) {
                   sprintf(errmsg,
		     "Command '%s': Invalid argument '%s'", label, argv[1]);
                   store_error_message(errmsg);
                   return 1;
                }

		sp = " ";
		if ( strchr(argv[2], '$') && mode == CMD_VERIFY )
		   byte2 = 0x10;
		else 
		   byte2 = strtol(argv[2], &sp, 16);
		if ( !strchr(" \t\n", *sp) || byte2 < 0 || byte2 > 0xff ) {
                   sprintf(errmsg,
		     "Command '%s': Invalid argument '%s'", label, argv[2]);
                   store_error_message(errmsg);
                   return 1;
                }
	
	        if ( strchr(argv[3], '$') && mode == CMD_VERIFY )
		   bursts = 1;
		else
                   bursts = strtol(argv[3], &sp, 10);
		
                if ( !strchr(" \t\n", *sp) || bursts < 1 || bursts > 255 ) {
                   sprintf(errmsg,
		        "Command '%s': Invalid <count> parameter", label);
	       	   store_error_message(errmsg);
	     	   return 1;
                }

                if ( mode == CMD_VERIFY )
	           return 0;

		burstgroup = min(bursts, (int)configp->rf_bursts[subcode]);
		if ( rfmode == RF_FAST )
		   burstgroup = bursts;

		rfword = byte1 << 8 | byte2;
                display_rf_xmit(subcode, rfword, bursts);

                while ( bursts > 0 ) {		
		   if ( write_cm17a(rfword, min(bursts, burstgroup), rfmode) != 0 ) {
                      sprintf(errmsg,
		        "Command '%s': Unable to write to CM17A device.", label);
                      store_error_message(errmsg);
                      return 1;
                   }
		   rf_farb_delay();
		   bursts -= burstgroup;
		}

                break;

             case 8 :  /* Reset CM17A */

		if ( mode == CMD_VERIFY )
		   return 0;

                if ( reset_cm17a() != 0 ) {
                   sprintf(errmsg,
		     "Command '%s': Unable to reset CM17A device.", label);
                   store_error_message(errmsg);
                   return 1;
                }

                break;

             case 10 : /* farw - Multiple arbitrary 16 bit rfwords */

                /* First check that each rfword is in range 0000-FFFF */
                for ( k = 1; k < argc; k++ ) {
                   if ( strchr(argv[k], '$') && mode == CMD_VERIFY ) {
                      /* Fake a dummy value for scene verification */
                      lxdata = 10;
                      sp = " ";
                   }
                   else {
                      lxdata = strtol(argv[k], &sp, 16);
                   }
                   if ( !strchr(" \t\n\r", *sp) || lxdata < 0 || lxdata > 0xffff ) {
                      sprintf(errmsg, 
                         "Command '%s': Data '%s' outside range 0-FFFF", label, argv[k]);
                      store_error_message(errmsg);
                      return 1;
                   }
                }

                if ( mode == CMD_VERIFY )
                   break;

                bursts = configp->rf_bursts[subcode];
                for ( k = 1; k < argc; k++ ) {
                   lxdata = strtol(argv[k], &sp, 16);
                   if ( !strchr(" \t\n\r", *sp) || lxdata < 0 || lxdata > 0xffff ) {
                      sprintf(errmsg, 
                         "Command '%s': Data '%s' outside range 0-FFFF", label, argv[k]);
                      store_error_message(errmsg);
                      return 1;
                   }
                   rfword = (unsigned int)lxdata;
                   display_rf_xmit(10, rfword, bursts);
                   if ( write_cm17a(rfword, bursts, rfmode) != 0 ) {
                      sprintf(errmsg,
                          "Command '%s': Unable to write to CM17A device.", label);
                      store_error_message(errmsg);
                      return 1;
                   }
                   rf_farw_delay();
                }
                break;		

             case 11 : /* flux - Multiple arbitrary 16 bit rfwords */

                /* First two parameters are number of bursts and post-delay */
                /* in milliseconds.  Both are decimal.                      */
                if ( strchr(argv[1], '&') && mode == CMD_VERIFY ) {
                   bursts = 5;
                   sp = " ";
                }
                else {
                   bursts = strtol(argv[1], &sp, 10);
                }
                if ( !strchr(" \t\n\r", *sp) || bursts < 1 ) {
                   sprintf(errmsg, 
                      "Command '%s': Invalid bursts number '%s'", label, argv[1]);
                   store_error_message(errmsg);
                   return 1;
                }
                   
                if ( strchr(argv[2], '&') && mode == CMD_VERIFY ) {
                   flux_delay = 200;
                   sp = " ";
                }
                else {
                   flux_delay = strtol(argv[2], &sp, 10);
                }
                if ( !strchr(" \t\n\r", *sp) || flux_delay < 0 ) {
                   sprintf(errmsg, 
                      "Command '%s': Invalid delay '%s'", label, argv[2]);
                   store_error_message(errmsg);
                   return 1;
                }
                   
                /* Subsequent parameters are 16-bit hex rfwords.*/   
                /* Check that each rfword is in range 0000-FFFF */
                for ( k = 3; k < argc; k++ ) {
                   if ( strchr(argv[k], '$') && mode == CMD_VERIFY ) {
                      /* Fake a dummy value for scene verification */
                      lxdata = 10;
                      sp = " ";
                   }
                   else {
                      lxdata = strtol(argv[k], &sp, 16);
                   }
                   if ( !strchr(" \t\n\r", *sp) || lxdata < 0 || lxdata > 0xffff ) {
                      sprintf(errmsg, 
                         "Command '%s': Data '%s' outside range 0-FFFF", label, argv[k]);
                      store_error_message(errmsg);
                      return 1;
                   }
                }

                if ( mode == CMD_VERIFY )
                   break;

                for ( k = 3; k < argc; k++ ) {
                   lxdata = strtol(argv[k], &sp, 16);
                   if ( !strchr(" \t\n\r", *sp) || lxdata < 0 || lxdata > 0xffff ) {
                      sprintf(errmsg, 
                         "Command '%s': Data '%s' outside range 0-FFFF", label, argv[k]);
                      store_error_message(errmsg);
                      return 1;
                   }
                   rfword = (unsigned int)lxdata;
                   display_rf_xmit(11, rfword, bursts);
                   if ( write_cm17a(rfword, bursts, rfmode) != 0 ) {
                      sprintf(errmsg,
                          "Command '%s': Unable to write to CM17A device.", label);
                      store_error_message(errmsg);
                      return 1;
                   }
                   rf_flux_delay(flux_delay);
                }
                break;		

	     default :	   
		break;
	  }
          break;
#endif /* End of HASCM17A */	  
             
       default :
          sprintf(errmsg, "Command '%s': Not implemented", label);
          store_error_message(errmsg);
          return 1;
    }

    return 0;
}

/*---------------------------------------------------------------+
 | Generate the elements for an uploaded macro and pass back     |
 | through the argument list.  Return 0 if successful, otherwise |
 | write message to error handling routine and return 1.         | 
 +---------------------------------------------------------------*/
int macro_command( int argc, char *argv[],
                 int *nelem, int *nbytes, unsigned char *elements )
{

    int           j, k, icmd, nargs, radix, subcode, subsubcode;
    unsigned int  mask, aflags, bitmap, sendmap;
    unsigned long flags;
    char          *label, *sp;
    unsigned char *ep;
    char          hc;
    char          errmsg[128];
    int           dimval, dimcode, delay, level;
    unsigned char group, subgroup;
    unsigned char cmdcode, hcode, switchv, switchcode;
    int           minargs, maxargs; 
    /* unsigned */ int  xdata;
    static char   reorder[] = {6,14,2,10,1,9,5,13,7,15,3,11,0,8,4,12};
    extern int    parse_ext3_group ( char *, unsigned char *, unsigned char * );

    clear_error_message();

    flags = 0;

    /* Handle a few special cases */

    if ( strcmp(argv[0], "function") == 0 ) {
       /* Special case: "function" command */
       if ( argc < 2 ) {
          store_error_message("Command 'function': Too few parameters");
          return 1;
       }
       argv++;
       argc--;
       flags |= F_NOA;
    }
    else if ( strcmp(argv[0], "turn") == 0 ) {
       /* Special case: "turn" command */
       if ( argc < 3 ) {
          store_error_message("Command 'turn': Too few parameters");
          return 1;
       }
       flags |= F_TRN;
       argv++;
       argc--;
       /* Swap first two parameters */
       sp = argv[0];
       argv[0] = argv[1];
       argv[1] = sp;
    }
    nargs = argc - 1;
      
    /* Identify the command */
    for ( icmd = 0; icmd < nx10cmds; icmd++ ) {
       if ( !strcmp(argv[0], x10command[icmd].label) )
          break;
    }
    if ( icmd == nx10cmds ) {
       sprintf(errmsg, "Invalid command '%s'", argv[0]);
       store_error_message(errmsg);
       return 1;
    }

    label    = x10command[icmd].label;
    minargs  = x10command[icmd].minargs;
    maxargs  = x10command[icmd].maxargs;
    flags   |= x10command[icmd].flags;
    cmdcode  = x10command[icmd].code;
    subcode  = x10command[icmd].subcode;
    subsubcode  = x10command[icmd].subsubcode;
    radix    = (flags & F_ARB) ?  16 : 10;

    /* Make sure this command is available for uploaded macros */
    if ( flags & F_NMA || flags & F_HLP ) {
       sprintf(errmsg, "'%s' command is invalid for uploaded macros", label);
       store_error_message(errmsg);
       return 1;
    }

    if ( flags & F_NMAI ) {
       sprintf(errmsg, "Warning: '%s' command is ignored for uploaded macros", label);
       store_error_message(errmsg);
       *nelem = *nbytes = 0;
       return 0;
    }
    
    if ( nargs < minargs ) {
       sprintf(errmsg, "Command '%s': Too few arguments", label);
       store_error_message(errmsg);
       return 1;
    }
    if ( maxargs != -1 && nargs > maxargs ) {
       cmd_usage(stderr, icmd );
       sprintf(errmsg, "Command '%s': Too many arguments", label);
       store_error_message(errmsg);
       return 1;
    }

    /* Handle a few special cases */

    if ( flags & F_ARB ) {
       /* If arbitrary extended code, get subcode and adjust argument count */
       subcode = (int)strtol(argv[1], &sp, radix);
       argv++;
       argc--;
       nargs--;
       if ( !strchr(" \t\n", *sp) || subcode < 0 || subcode > 0xff ) {
          sprintf(errmsg, "Command '%s': Extended code function outside range 00-0xff", label);
          store_error_message(errmsg);
          return 1;
       }
    }

    /* Only commands with command code <= 6  or code == 26 are valid with "turn" */
    if ( flags & F_TRN && cmdcode > 6 && cmdcode != 26 ) {
       sprintf(errmsg, 
         "Command '%s' cannot be used with the 'turn' command", label); 
       store_error_message(errmsg);
       return 1;
    }

    /* Default HC|Unit */
    hc = '_';
    bitmap = 0;
    aflags = A_VALID;

    if ( !(flags & F_NUM) && nargs > 0 ) {
       /* parse it as an alias or HC|Units token. */
       aflags = parse_addr(argv[1], &hc, &bitmap);
    }

    if ( !(aflags & A_VALID) ) {
       sprintf(errmsg, "Command '%s': ", label);
       add_error_prefix(errmsg);
       return 1;
    }

    /* Make sure a positional parameter didn't slip through */
    if ( aflags & A_DUMMY ) {
       sprintf(errmsg,
          "Command '%s': Unresolved positional parameter '%s'",
             label, argv[1]);
       store_error_message(errmsg);
       return 1;
    }
       
    if ( !(aflags & A_HCODE) && !(flags & F_NUM) && minargs > 0 )  {
       sprintf(errmsg, "Command '%s': Housecode required", label);
       store_error_message(errmsg);
       return 1;
    }

    switchv = cmdcode;

    switch ( switchv ) {

       case 20 : /* Send address bytes only */
       case 21 : /* Function only */
       case 22 : /* Send arbitrary series of hex bytes */
       case 23 : /* Send (quoted) text message */
       case 25 : /* Pause */
       case 27 : /* Turn */
       case 28 : /* Temp_Req */
       case 99 : /* Legacy commands */
          /* We should never get here */
             sprintf(errmsg, "Internal error - Invalid macro command '%s'", label);
             store_error_message(errmsg);
             return 1;

       case  8 :  /* Hail */
       case  9 :  /* Hail Ack */
          if ( !(aflags & A_HCODE) ) {
             hc = configp->housecode;
          }
       case  0 :  /* All Units Off  */
       case  1 :  /* All Lights On  */
       case  6 :  /* All Lights Off */
       case 12 :  /* Data Transfer */
          if ( bitmap != 0 && !(aflags & A_PLUS) && !(aflags & A_MINUS) && configp->force_addr == NO) {
             sprintf(errmsg, "Unitcode ignored for '%s' command.", label);
             store_error_message(errmsg);
             *errmsg = '\0';
             bitmap = 0;
          }
          if ( bitmap == 0 && (aflags & A_PLUS || configp->force_addr == YES) )
             bitmap = 1;

       case  2 :  /* On */
       case  3 :  /* Off */
       case 13 :  /* Status On */
       case 14 :  /* Status Off */
          if ( aflags & A_MINUS || flags & F_NOA )
             bitmap = 0;
          else if ( bitmap == 0 && !(flags & F_ALL) ) {
             sprintf(errmsg, "Command '%s': No Unitcode specified", label);
             store_error_message(errmsg);
             return 1;
          }

          hcode = hc2code(hc);

          elements[0] = (hcode << 4) | cmdcode;
          elements[1] = (unsigned char)((bitmap & 0xff00) >> 8);
          elements[2] = (unsigned char)((bitmap & 0x00ff));
          *nelem = 1;
          *nbytes = 3;

          break;
                 
       case  4 :  /* Dim */
       case  5 :  /* Bright */
          dimval = (int)strtol(argv[2], &sp, 10); 
          if ( !strchr(" \t\n", *sp) ) {
	     sprintf(errmsg,
		"Command '%s': Invalid Dim/Bright parameter '%s'", label, argv[2]);
             store_error_message(errmsg);
	     return 1;
	  }
	  else if ( configp->restrict_dims == YES && (dimval < 1 || dimval > 22) ) {
             sprintf(errmsg, 
                "Command '%s': Dim/Bright value outside range 1-22", label);
             store_error_message(errmsg);
	     return 1;
	  }
	  else if ( configp->restrict_dims == NO && (dimval < 0 || dimval > 31) ) {
             sprintf(errmsg, 
                "Command '%s': Dim/Bright value outside range 0-31", label);
             store_error_message(errmsg);
             return 1;
          }

          if ( aflags & A_MINUS || flags & F_NOA )
             bitmap = 0;
          else if ( bitmap == 0 ) {
             sprintf(errmsg, "Command '%s': No Unitcode specified", label);
             store_error_message(errmsg);
             return 1;
          }

          hcode = hc2code(hc);

          if ( flags & F_ONN ) {
             /* On signal needed before bright/dim (WS467-1 redesign) */
             elements[0] = (hcode << 4) | 0x02;
             elements[1] = (unsigned char)((bitmap & 0xff00) >> 8);
             elements[2] = (unsigned char)((bitmap & 0x00ff));
             elements[3] = (hcode << 4) | cmdcode;
             elements[4] = 0;
             elements[5] = 0;
             elements[6] = (unsigned char)(dimval | ((flags & F_BRI) ? 0x80 : 0));          
             *nelem = 2;
             *nbytes = 7;
          }
          else {
             elements[0] = (hcode << 4) | cmdcode;
             elements[1] = (unsigned char)((bitmap & 0xff00) >> 8);
             elements[2] = (unsigned char)((bitmap & 0x00ff));
             elements[3] = (unsigned char)(dimval | ((flags & F_BRI) ? 0x80 : 0));          
             *nelem = 1;
             *nbytes = 4;
          }

          break;
    
       case 10 :  /* Old style preset 1 */
       case 11 :  /* Old style preset 2 */

          if ( nargs == 2 )
             /* Limited preset for macros */
             dimval = (int)strtol(argv[2], &sp, 10);
          else
             /* preset level only */
             dimval = (int)strtol(argv[1], &sp, 10);

          if ( !strchr(" \t\n", *sp) || dimval < 1 || dimval > 32 ) {
             sprintf(errmsg, "Command '%s': Level outside range 1-32", label);
             store_error_message(errmsg);
             return 1;
          }

          if ( (dimcode = dimval) > 16 ) {
             cmdcode = 11;
             dimcode -= 16;
          }
          dimcode = rev_low_nybble(dimcode - 1);

          if ( nargs == 2 && (hcode = hc2code(hc)) != dimcode  ) {
             sprintf(errmsg, "Command '%s': Preset level %d not supported for housecode %c",
                label, dimval, hc);
             store_error_message(errmsg);
             return 1;
          }

          if ( aflags & A_MINUS || flags & F_NOA )
             bitmap = 0;
          else if ( bitmap == 0 ) {
             sprintf(errmsg, "Command '%s': No Unitcode specified", label);
             store_error_message(errmsg);
             return 1;
          }

          elements[0] = (dimcode << 4) | cmdcode;
          elements[1] = (unsigned char)((bitmap & 0xff00) >> 8);
          elements[2] = (unsigned char)((bitmap & 0x00ff));
          *nelem = 1;
          *nbytes = 3;

          break;

       case 15 :  /* Status Req */
          sendmap = bitmap;
          if ( aflags & A_MINUS || flags & F_NOA ) {
             bitmap = 1;
             sendmap = 0;
          }
          else if ( bitmap == 0 ) {
             sprintf(errmsg, "Command '%s': No Unitcode specified", label);
             store_error_message(errmsg);
             return 1;
          }

          hcode = hc2code(hc);

          ep = elements;
          *nelem = 0;
          *nbytes = 0;
             
          /* Send each address/command pair individually to allow */
          /* associating a response to a particular unit code     */
          mask = 1;
          for ( k = 0; k < 16; k++ )  {
             if ( bitmap & mask ) {
                *ep++ = (hcode << 4) | cmdcode;
                *ep++ = (unsigned char)((bitmap & 0xff00) >> 8);
                *ep++ = (unsigned char)((bitmap & 0x00ff));
                *nelem += 1;
                *nbytes += 3;
             }
             mask = mask << 1;
          }         
          break;

       case 24 :  /* Send All_Units_Off to all Housecodes */
          bitmap = (configp->force_addr == YES) ?  1 : 0;
          ep = elements;
	  for ( j = 0; j < 16; j++ ) {
	     hcode = reorder[j];    /* Send housecodes in alphabetic order */
             *ep++ = (hcode << 4);  /* cmdcode = 0 */
             *ep++ = (unsigned char)((bitmap & 0xff00) >> 8);
             *ep++ = (unsigned char)((bitmap & 0x00ff));
          }
          *nelem = 16;
          *nbytes = 48;

	  break;

       case 26 :  /* All Units On (same as 'on H1-16') */
          if ( bitmap != 0 && !(aflags & A_MINUS) ) {
             sprintf(errmsg, "Unitcode ignored for '%s' command.", label);
             store_error_message(errmsg);
          }
          bitmap = 0xFFFF;

          hcode = hc2code(hc);

          elements[0] = (hcode << 4) | 2 ;
          elements[1] = (unsigned char)((bitmap & 0xff00) >> 8);
          elements[2] = (unsigned char)((bitmap & 0x00ff));
          *nelem = 1;
          *nbytes = 3;
          
          break;

       case 32 :  /* Delay in minutes */
          delay = (int)strtol(argv[1], &sp, 10);
          if ( !strchr(" \t\n", *sp) ) {
             sprintf(errmsg, 
	        "Command '%s': Invalid character '%c' in delay value", label, *sp);
             store_error_message(errmsg);
             return 1;
          }
          if ( delay < 0 || delay > 240 ) {
             sprintf(errmsg, "Command '%s': value %d outside range 0-240 minutes",
		 label, delay);
             store_error_message(errmsg);
             return 1;
          }
	  	  
	  sprintf(errmsg, "Command '%s': Not yet implemented for macros", label);
	  store_error_message(errmsg);
	  return 1;
	  
       case  7 :  /* Extended Code commands */
          sendmap = (aflags & A_PLUS) ? bitmap : 0 ;
          if ( sendmap == 0 && configp->force_addr == YES )
             sendmap = 0xFFFF;

          hcode = hc2code(hc);
          xdata = 0;

          if ( flags & F_ARB ) 
             switchcode = 0xff;
          else
             switchcode = subcode;

          switch ( switchcode ) {
             case 0x30 :  /* Extended Group Include */
                if ( bitmap == 0 ) {
                   sprintf(errmsg, "Command '%s': No Unitcode specified", label);
                   store_error_message(errmsg);
                   return 1;
                }

                if ( parse_ext3_group(argv[2], &group, &subgroup) != 0 ) {
                   sprintf(errmsg,
                      "Command '%s': %s", label, error_message());
                   store_error_message(errmsg);
                   return 1;
                } 

                xdata = (group << 6);
                if ( subgroup & 0x80u ) {
                   xdata |= (subgroup & 0x0fu) | 0x20u;
                }

                break;

             case 0x31 :  /* Extended Preset Dim */
                if ( bitmap == 0 ) {
                   sprintf(errmsg, "Command '%s': No Unitcode specified", label);
                   store_error_message(errmsg);
                   return 1;
                }

                /* Get dim level */
                xdata = (int)strtol(argv[2], &sp, 10);  
                if ( !strchr(" \t\n", *sp) || xdata < 0 || xdata > 63 ) {
                   sprintf(errmsg, "Command '%s': Extended Preset level outside range 0-63", label);
                   store_error_message(errmsg);
                   return 1;
                }
                break;

             case 0x32 :  /* Extended Group Include at Level*/
                if ( bitmap == 0 ) {
                   sprintf(errmsg, "Command '%s': No Unitcode specified", label);
                   store_error_message(errmsg);
                   return 1;
                }

                if ( parse_ext3_group(argv[2], &group, &subgroup) != 0 ) {
                   sprintf(errmsg,
                      "Command '%s': %s", label, error_message());
                   store_error_message(errmsg);
                   return 1;
                } 

                xdata = (group << 6);

                if ( subgroup & 0x80u ) {
                   sprintf(errmsg,
                      "Command '%s': Cannot specify group.subgroup with level", label);
                   store_error_message(errmsg);
                   return 1;
                }

                level = (int)strtol(argv[3], &sp, 10);
                if ( !strchr(" \t\n", *sp) || level < 0 || level > 63 ) {
                   sprintf(errmsg,
                     "Command '%s': Level outside range 0-63", label);
                   store_error_message(errmsg);
                   return 1;
                }
                xdata |= (level & 0x3f);

                break;
           
             case 0x33 :  /* Extended All On */
             case 0x34 :  /* Extended All Off */
                if ( bitmap != 0 && !(aflags & A_PLUS) && !(aflags & A_MINUS) && configp->force_addr == NO) {
                   sprintf(errmsg, "Unitcode ignored for '%s' command.", label);
                   store_error_message(errmsg);
                   *errmsg = '\0';
                   bitmap = 1;
                }
                else if ( bitmap == 0 )
                   bitmap = 1;

                if ( aflags & A_PLUS )
                   sendmap = bitmap;

                break;

             case 0x35 :  /* Remove from group(s) */
                if ( (xdata = (int)list2linmap(argv[2], 0, 3)) == 0 ) {
                   sprintf(errmsg, "Command '%s': Group number %s", label, error_message());
                   store_error_message(errmsg);
                   return 1;
                }

                if ( subsubcode == 0 ) {
                   if ( bitmap == 0 ) {
                      sprintf(errmsg, "Command '%s': No Unitcode specified", label);
                      store_error_message(errmsg);
                      return 1;
                   }
                }
                else {
                   xdata |= 0xf0;
                   if ( bitmap != 0 && !(aflags & A_PLUS) && !(aflags & A_MINUS) &&
                        configp->force_addr == NO) {
                      sprintf(errmsg, "Unitcode ignored for '%s' command.", label);
                      store_error_message(errmsg);
                      *errmsg = '\0';
                      bitmap = 1;
                   }
                   else if ( bitmap == 0 ) {
                      bitmap = 1;
                   }
                }

                if ( aflags & A_PLUS )
                   sendmap = bitmap;

                break;
                
             case 0x36 :  /* Execute group functions */
                if ( bitmap != 0 && !(aflags & A_PLUS) && !(aflags & A_MINUS) && configp->force_addr == NO) {
                   sprintf(errmsg, "Unitcode ignored for '%s' command.", label);
                   store_error_message(errmsg);
                   *errmsg = '\0';
                   bitmap = 1;
                }
                else if ( bitmap == 0 ) {
                   bitmap = 1;
                }

                if ( aflags & A_PLUS )
                   sendmap = bitmap;

                if ( parse_ext3_group(argv[2], &group, &subgroup) != 0 ) {
                   sprintf(errmsg,
                      "Command '%s': %s", label, error_message());
                   store_error_message(errmsg);
                   return 1;
                }

                xdata = group << 6;

                if ( subgroup & 0x80u ) {
                   xdata |= 0x20u | (subgroup & 0x0f);
                }

                if ( subsubcode == 1 ) {
                   xdata |= 0x10u;
                }

                break;

             case 0x37 :  /* Extended Status Requests */
                if ( bitmap == 0 ) {
                   sprintf(errmsg, "Command '%s': No Unitcode specified", label);
                   store_error_message(errmsg);
                   return 1;
                }

                if ( subsubcode == 0 ) {
                   /* Output status request */
                   xdata = 0;
                }
                else if ( subsubcode == 1 ) {
                   /* Powerup status request */
                   xdata = 0x10;
                }
                else {
                   if ( parse_ext3_group(argv[2], &group, &subgroup) != 0 ) {
                      sprintf(errmsg,
                         "Command '%s': %s", label, error_message());
                      store_error_message(errmsg);
                      return 1;
                   }

                   xdata = (group << 6) | 0x20u;

                   if ( subgroup & 0x80u ) {
                      xdata |= 0x10u | (subgroup & 0x0f);
                   }
                }

                break;

             case 0x3b :  /* Extended Configure Auto Status_Ack Mode */
                if ( bitmap != 0 && !(aflags & A_PLUS) && !(aflags & A_MINUS) && configp->force_addr == NO) {
                   sprintf(errmsg, "Unitcode ignored for '%s' command.", label);
                   store_error_message(errmsg);
                   *errmsg = '\0';
                   bitmap = 1;
                }
                else if ( bitmap == 0 )
                   bitmap = 1;

                if ( aflags & A_PLUS )
                   sendmap = bitmap;

                /* Get mode */
                xdata = (int)strtol(argv[2], &sp, 10);  
                if ( !strchr(" \t\n", *sp) || xdata < 0 || xdata > 3 ) {
                   sprintf(errmsg, "Command '%s': Extended Config Mode outside range 0-3", label);
                   store_error_message(errmsg);
                   return 1;
                }
                break;

             case 0x3c :  /* Extended group bright/dim functions */
                if ( bitmap != 0 && !(aflags & A_PLUS) && !(aflags & A_MINUS) && configp->force_addr == NO) {
                   sprintf(errmsg, "Unitcode ignored for '%s' command.", label);
                   store_error_message(errmsg);
                   *errmsg = '\0';
                   bitmap = 1;
                }
                else if ( bitmap == 0 ) {
                   bitmap = 1;
                }

                if ( aflags & A_PLUS )
                   sendmap = bitmap;

                if ( parse_ext3_group(argv[2], &group, &subgroup) != 0 ) {
                   sprintf(errmsg,
                      "Command '%s': %s", label, error_message());
                   store_error_message(errmsg);
                   return 1;
                }

                xdata = group << 6;

                if ( subgroup & 0x80u ) {
                   xdata |= 0x20u | (subgroup & 0x0f);
                }

                if ( subsubcode == 1 ) {
                   xdata |= 0x10u;
                }

                break;

             case 0xfd :  /* Extended OFF ( = Extended Preset Dim level 0) */
                if ( bitmap == 0 ) {
                   sprintf(errmsg, "Command '%s': No Unitcode specified", label);
                   store_error_message(errmsg);
                   return 1;
                }
                subcode = 0x31;
                xdata = 0;
                break;                

             case 0xfe :  /* Extended ON ( = Extended Preset Dim level 63) */
                if ( bitmap == 0 ) {
                   sprintf(errmsg, "Command '%s': No Unitcode specified", label);
                   store_error_message(errmsg);
                   return 1;
                }
                subcode = 0x31;
                xdata = 63;
                break;                

             case 0xff :  /* Arbitrary Extended Function */
                if ( bitmap == 0 ) {
                   sprintf(errmsg, "Command '%s': No Unitcode specified", label);
                   return 1;
                }
                xdata = (int)strtol(argv[2], &sp, 16);
                if ( !strchr(" \t\n", *sp) || xdata < 0 || xdata > 0xff ) {
                   sprintf(errmsg, 
                      "Command '%s': Data '%s' outside range 0-FF", label, argv[2]);
                   store_error_message(errmsg);
                   return 1;
                }
                break ;

             case 0x01 :  /* Shutter open, observe limit */
             case 0x03 :  /* Shutter open, ignore limit */
             case 0x02 :  /* Set shutter limit */
                if ( bitmap == 0 ) {
                   sprintf(errmsg, "Command '%s': No Unitcode specified", label);
                   store_error_message(errmsg);
                   return 1;
                }

                /* Get dim level */
                xdata = (int)strtol(argv[2], &sp, 10);  
                if ( !strchr(" \t\n", *sp) || xdata < 0 || xdata > 25 ) {
                   sprintf(errmsg, "Command '%s': Shutter level outside range 0-25", label);
                   store_error_message(errmsg);
                   return 1;
                }
                break;

             case 0x04 :  /* Shutter Open All */
             case 0x0B :  /* Shutter Close All */

                if ( bitmap != 0 && !(aflags & A_PLUS) && !(aflags & A_MINUS) && configp->force_addr == NO) {
                   sprintf(errmsg, "Unitcode ignored for '%s' command.", label);
                   store_error_message(errmsg);
                   *errmsg = '\0';
                   bitmap = 1;
                }
                else if ( bitmap == 0 )
                   bitmap = 1;

                if ( aflags & A_PLUS )
                   sendmap = bitmap;

                break;

             default :
                sprintf(errmsg, "Command not implemented");
                store_error_message(errmsg);
                return 1;                
          }

          mask = 1;
          ep = elements;
          *nelem = 0;
          *nbytes = 0;
          for ( j = 0; j < 16; j++ ) {
             if ( bitmap & mask ) {
                *ep++ = (hcode << 4) | cmdcode;
                *ep++ = (unsigned char)(((sendmap & mask) & 0xff00) >> 8);
                *ep++ = (unsigned char)(((sendmap & mask) & 0x00ff));
                *ep++ = j;
                *ep++ = (unsigned char)(xdata & 0xff);
                *ep++ = (unsigned char)(subcode & 0xff);
                *nelem += 1;
                *nbytes += 6;
             }
             mask = mask << 1;
          }             
          break;

       default :
          sprintf(errmsg, "Command not implemented");
          store_error_message(errmsg);
          return 1;
    }

    return 0;
}

     
/*---------------------------------------------------------------+
 | Return the maximum number of positional parameters in a       |
 | scene, i.e., the highest integer 'nn' represented in a token  |
 | a token of the form "$nn".  Return -1 if the "nn" is missing, |
 | is less than 1, or is not an integer.                         |
 +---------------------------------------------------------------*/
int max_parms ( int tokc, char *tokv[] )
{
   int  j, parmno, count, maxparm = 0;
   int  bracket;
   char *sp, *tp;
   char errmsg[128];
   int  parmchk[1000];

   for ( j = 0; j < 32; j++ )
      parmchk[j] = 0;

   for ( j = 1; j < tokc; j++ ) {
      tp = tokv[j];      
      while ((sp = strchr(tp, '$')) != NULL ) {
         sp++;
         bracket = (*sp == '{') ? 1 : 0 ;
         parmno = (int)strtol(sp + bracket, &sp, 10);
         if ( (bracket && *sp != '}') || parmno < 1 ) {
            sprintf(errmsg, 
               "Command '%s': Invalid positional parameter in '%s'",
                   tokv[0], tokv[j]);
            store_error_message(errmsg);
            return -1;
         }
         if ( parmno > configp->max_pparms ) {
            sprintf(errmsg, "Too many (%d) positional parameters - max %d",
               parmno, configp->max_pparms);
            store_error_message(errmsg);
            return -1;
         }
         parmchk[parmno] = 1;
         maxparm = max(maxparm, parmno);
         if ( bracket )
            sp++;
         tp = sp;
      }
   }

   count = 0;
   for ( j = 1; j < 32; j++ )
      count += parmchk[j];

   /* Warn the user if all positional parameters within the */
   /* range 1 through maxparm arene't used in the scene.    */
   if ( count < maxparm ) {
      sprintf(errmsg, 
        "Warning: Not all positional parameters specified are used");
      store_error_message(errmsg);
   }
   return maxparm;
}
       
      
/*---------------------------------------------------------------+
 | Verify that the syntax of commands in a scene is correct.     |
 +---------------------------------------------------------------*/
int verify_scene ( char *body ) 
{
   int  j;
   int  cmdc, tokc, count;
   char **cmdv, **tokv;
   char *sp;

   sp = strdup(body);

   /* Separate scene into individual commands */
   tokenize(sp, ";", &cmdc, &cmdv);
   count = 0;
   for ( j = 0; j < cmdc; j++ ) {
      strtrim(cmdv[j]);
      if ( *cmdv[j] == '\0' )
         continue;
      count++;
      /* Separate commands into individual tokens */
      tokenize(cmdv[j], " \t", &tokc, &tokv);
      /* Verify the individual command */
      if ( direct_command(tokc, tokv, CMD_VERIFY) != 0 ) {
         free(tokv);
         free(cmdv);
         free(sp);
         return 1;
      }
      free(tokv);
   }
   free(cmdv);
   free(sp);
   if ( !count ) {
      store_error_message("Warning: Contains no commands.");
   }
   return 0;
}


/*---------------------------------------------------------------+
 | Replace the positional parameters in the members of list tokv |
 | with actual values.  It is presumed that the number of        |
 | members in 'actual' has already be verified to be sufficient. |
 | The new _members_ in tokvp must be freed after use.           |
 +---------------------------------------------------------------*/
void replace_dummy_parms ( int tokc, char ***tokvp, char *actual[] )
{
   int j, parmno, bracket;
   char *sp;
   char buffer1[128];
   char buffer2[128];

   for ( j = 0; j < tokc; j++ ) {
      strncpy2(buffer1, (*tokvp)[j], sizeof(buffer1) - 1);

      while ( (sp = strchr(buffer1, '$')) != NULL ) {   
         strncpy2(buffer2, buffer1, (sp - buffer1));
         sp++;
         bracket = (*sp == '{') ? 1 : 0 ;
         parmno = (int)strtol(sp + bracket, &sp, 10);
         strncat(buffer2, actual[parmno], sizeof(buffer2) - 1 - strlen(buffer2));
         if ( bracket )
            sp++;
	 strncat(buffer2, sp, sizeof(buffer2) - 1 - strlen(buffer2));
         strncpy2(buffer1, buffer2, sizeof(buffer1) - 1);
      }
      if ( ((*tokvp)[j] = strdup(buffer1)) == NULL ) {
         fprintf(stderr, "Unable to allocate memory in replace_dummy_parms()\n");
         exit(1);
      }
   }

   return;
}

/*---------------------------------------------------------------+
 | Message when 'heyu linewidth NN' is entered at the PC command |
 | prompt.  This command is intended to be intercepted by the    |
 | WEBCLI web interface and be an instruction to that interface. |
 +---------------------------------------------------------------*/ 
int display_linewidth_msg ( void )
{
   printf("Dummy command - intended for web interface.\n");
   return 0;
}

/*---------------------------------------------------------------+
 | Handler for direct commands and scenes entered from the       |
 | command line.                                                 |
 +---------------------------------------------------------------*/
int c_command( int argc, char *argv[] )
{
   int    j, k, m, cmdc, tokc, retcode;
   char   **cmdv, **tokv;
   char   *sp;
   SCENE  *scenep;

   scenep = config.scenep;

    /* Determine whether 2nd level command under "cmd". */
    /* If so, offset all arguments by 1.                */
    if ( strcmp(argv[1], "cmd") == 0 ) {
       argv++;
       argc--;
    }
    if ( argc < 2 )
       return 1;

    /* Special case: "help" command */
    if ( strcmp(argv[1], "help") == 0 ) {
       if ( argc < 3 ) 
	  command_help("");
       else
	  command_help(argv[2]);
       return 0;
    }
    
    /* Special case: "syn" (display synonyms) command */
    if ( strcmp(argv[1], "syn") == 0 ) {
       display_syn();
       return 0;
    }

    /* Special case: "linewidth" dummy command */
    if ( strcmp(argv[1], "linewidth") == 0 ) {
       display_linewidth_msg();
       return 0;
    }

    /* If the argument contains one or more ';' characters, it's */
    /* tentatively a compound ("command-line macro") command.    */
#if 0
if ( (sp = strstr(argv[1], "\\;")) != NULL ) {
  strncpy(sp, "; ", 2);
  printf("argv[1] = '%s'\n", argv[1]);
}
#endif
    if ( strchr(argv[1], ';') || strchr(argv[1], ' ') || strchr(argv[1], '\t') ) {
       sp = argv[1];
       while ( (sp = strstr(sp, "\\;")) != NULL ) {
          strncpy(sp, "; ", 2);
          sp++;
       }
       if ( verify_scene(argv[1]) != 0 ) {
          fprintf(stderr, "%s\n", error_message());
          clear_error_message();
          return 1;
       }
       /* Separate commands delimited by ';' */
       sp = strdup(argv[1]);
       tokenize(sp, ";", &cmdc, &cmdv);
       for ( k = 0; k < cmdc; k++ ) {
	  strtrim(cmdv[k]);
	  if ( *cmdv[k] == '\0' )
             continue;

          tokenize(cmdv[k], " \t", &tokc, &tokv);

          retcode = direct_command(tokc, tokv, CMD_RUN);

          free(tokv);

          if ( retcode != 0 ) {
             /* Display fatal message from error handler */
             fprintf(stderr, "%s\n", error_message());
             clear_error_message();
             free(cmdv);
             return retcode;
          }
       }
       free(cmdv);
       free(sp);

       return 0;
    }
         

    /* See if it's a user-defined scene */
    if ( (j = lookup_scene(scenep, argv[1])) >= 0 ) {

       if ( argc < (scenep[j].nparms + 2) ) {
          fprintf(stderr, 
             "(Config %02d: %s '%s'): Too few parameters - %d required.\n", 
                scenep[j].line_no, typename[scenep[j].type], scenep[j].label, 
                scenep[j].nparms);
          return 1;
       }
       else if ( argc > (scenep[j].nparms + 2) ) {
          fprintf(stderr, 
             "(Config %02d: %s '%s'): Too many parameters - only %d accepted.\n", 
                scenep[j].line_no, typename[scenep[j].type], scenep[j].label,
                scenep[j].nparms);
          return 1;
       }

       sp = strdup(scenep[j].body);
       /* Separate scene commands delimited by ';' */
       tokenize(sp, ";", &cmdc, &cmdv);
       for ( k = 0; k < cmdc; k++ ) {
	  strtrim(cmdv[k]);
	  if ( *cmdv[k] == '\0' )
             continue;
          tokenize(cmdv[k], " ", &tokc, &tokv);

          /* Substitute for any positional parameters */
          /* This function allocates new memory for   */
          /* each parameter since the replacement may */
          /* be longer than the string in argv, so    */
          /* each must afterwarsds be freed.          */
          replace_dummy_parms(tokc, &tokv, argv + 1);

          retcode = direct_command(tokc, tokv, CMD_RUN);

          /* Free each member of tokv, then tokv itself */
          for ( m = 0; m < tokc; m++ )
             free(tokv[m]);
          free(tokv);

          if ( retcode != 0 ) {
             /* Display fatal message from error handler */
             fprintf(stderr, "(Config %02d: %s '%s'): %s\n", 
                scenep[j].line_no, typename[scenep[j].type], scenep[j].label,
                error_message());
             clear_error_message();
             free(cmdv);
             free(sp);
             return retcode;
          }
          else if ( *error_message() ) {
             /* Display warning from error handler */
             fprintf(stderr, "(Config %02d: %s '%s'): %s\n", 
                scenep[j].line_no, typename[scenep[j].type], scenep[j].label,
                error_message());
          }
          clear_error_message();             
       }
       free(cmdv);
       free(sp);

       return 0;
    }

    /* It's an individual command.                      */
    /* Reduce the argument list so that the X10 command */
    /* itself becomes argv[0]                           */

    retcode = direct_command(--argc, ++argv, CMD_RUN);
    if ( retcode != 0 ) {
       fprintf(stderr, "%s\n", error_message());
       clear_error_message();
       return retcode;
    }
    else if ( *error_message() ) {
       fprintf(stderr, "%s\n", error_message());
       clear_error_message();
    }
  
    return 0;
}
    

        
/*---------------------------------------------------------------------+
 | Display macros for final_report()                                   |
 | Argument 'type' is either USED or NOTUSED.                          |
 +---------------------------------------------------------------------*/
int display_macros ( FILE *fd, unsigned char type, 
                           MACRO *macrop, unsigned char *elementp )
{
   int           i, j, k, m, switchval, label_max, minargs;
   char          delim, modflag;
   unsigned char cmdcode, subcode, subsubcode, hcode, subunit, bflag, dim;
   unsigned int  bmap;
   unsigned long flags;

   /* Find the maximum length of a macro label */
   label_max = 0;
   j = 0;
   while ( macrop[j].line_no > 0 ) {
      /* Skip null macro and those of the wrong type (used/unused) */
      if ( macrop[j].use != type || macrop[j].isnull ) {
         j++;
         continue;
      }
     
      /* Skip derived macros which aren't used */
      if ( type == NOTUSED && (macrop[j].refer & DERIVED_MACRO) ) {
         j++;
         continue;
      } 

      label_max = max(label_max, (int)strlen(macrop[j].label) );
      j++;
   }

   if ( label_max == 0 )
      return 0;

   /* Now display the macros */
   j = 0;
   while ( macrop[j].line_no > 0 ) {
      if ( macrop[j].use != type || macrop[j].isnull ) {
         j++;
         continue;
      }

      /* Skip derived macros which aren't used */
      if ( type == NOTUSED && (macrop[j].refer & DERIVED_MACRO) ) {
         j++;
         continue;
      } 
      
      modflag = (macrop[j].modflag == UNMODIFIED) ? ' ' : '*' ;

      if ( type == USED && configp->display_offset == YES )
         (void) fprintf( fd, "%03x macro  %-*s %3d %c", macrop[j].offset,
             label_max, macrop[j].label, macrop[j].delay, modflag);
      else
         (void) fprintf( fd, "macro  %-*s %3d %c", label_max,
             macrop[j].label, macrop[j].delay, modflag);


      delim = ' ';
      i = macrop[j].element ;
      for ( k = 0; k < macrop[j].nelem; k++ ) {
         cmdcode = elementp[i] & 0x0f;
         hcode = code2hc((elementp[i] & 0xf0u) >> 4);
         bmap = elementp[i+1] << 8 | elementp[i+2];
         switchval = -1;

         bflag = 0;
         if ( cmdcode == 4 || cmdcode == 5 ) {
            /* Dim or Bright command                    */
            /* Check both command code and brighten bit */
            bflag = (elementp[i+3] & 0x80) ? F_BRI : 0;
            for ( m = 0; m < nx10cmds; m++ ) {
               if ( x10command[m].code == cmdcode && 
                    (x10command[m].flags & F_BRI) == bflag ) {
                  switchval = m;
                  break;
               }
            }
         }
         else if ( cmdcode == 10 || cmdcode == 11 ) {
            /* Preset command */
            if ( bmap == 0 ) {
               /* Check for preset_level only */
               for ( m = 0; m < nx10cmds; m++ ) {
                  if ( x10command[m].code == cmdcode && 
                       x10command[m].flags & F_NOA ) {
                     switchval = m;
                     break;
                  }
               }
            }
            else {
               /* Check for mpreset */
               for ( m = 0; m < nx10cmds; m++ ) {
                  if ( x10command[m].code == cmdcode && 
                       x10command[m].flags & F_MAC ) {
                     switchval = m;
                     break;
                  }
               }
            }
         }
         else {
            for ( m = 0; m < nx10cmds; m++ ) {
               if ( x10command[m].code == cmdcode ) {
                  switchval = m;
                  break;
               }
            }
         }

         if ( switchval < 0 || switchval == nx10cmds ) {
            (void) fprintf(stderr, "display_macros(): Internal error 1\n");

            break;
         }

         switch ( cmdcode ) {
            case 0 :  /* All Units Off */
            case 1 :  /* All Lights On */
            case 6 :  /* All Lights Off */             
            case 8 :  /* Hail Request */
            case 9 :  /* Hail Ack */
            case 12 : /* Data Transfer */               
	    case 13 : /* Status On */
	    case 14 : /* Status Off */
               /* Bitmap is normally zero for these commands */
               if ( bmap ) {
                  /* Display the '+' prefixed HC and unit */  
                  fprintf( fd, "%c %s +%c%s",
                      delim, x10command[switchval].label, hcode,
                      bmap2units(bmap) );
               }
               else {
                  /* Just the HC */
                  fprintf( fd, "%c %s %c",
                      delim, x10command[switchval].label, hcode);
               }

               i += x10command[switchval].length;
               break;

            case 2 :  /* On */
            case 3 :  /* Off */
            case 15 : /* Status Request */
               if ( bmap ) {
                  fprintf( fd, "%c %s %c%s",
                      delim, x10command[switchval].label, hcode,
                      bmap2units(bmap) );
               }
               else {
                  fprintf( fd, "%c %s -%c",
                      delim, x10command[switchval].label, hcode);
               }

               i += x10command[switchval].length;
               break;

            case 4 :  /* Dim or Dimb */
            case 5 :  /* Bright or Brightb */
               if ( bmap ) {
                  fprintf( fd, "%c %s %c%s %d",
                     delim, x10command[switchval].label, hcode,
                     bmap2units(bmap), elementp[i+3] & 0x1f );
               }
               else {
                  fprintf( fd, "%c %s -%c %d",
                     delim, x10command[switchval].label, hcode,
                     elementp[i+3] & 0x1f );
               }

               i += x10command[switchval].length;

               break;

            case 10 : /* Preset Dim (1) - mpreset or level only */
            case 11 : /* Preset Dim (2) - mpreset or level only */
               dim = rev_low_nybble((elementp[i] & 0xf0u) >> 4) + 1;
               dim += (cmdcode == 11) ? 16 : 0;
               if ( bmap == 0 ) {
                  fprintf( fd, "%c %s %d", 
                     delim, x10command[switchval].label, dim);
               }
               else {
                  fprintf( fd, "%c %s %c%s %d",
                     delim, x10command[switchval].label,
                       hcode, bmap2units(bmap), dim );
               }

               i += x10command[switchval].length;
               break;

            case  7 : /* Extended Code */
               subcode = elementp[i+5];
               if ( subcode == 0x37 || subcode == 0x36 || subcode == 0x35 || subcode == 0x3c ) {
                  subsubcode = (elementp[i+4] & 0x30) >> 4;
                  for ( m = 0; m < nx10cmds; m++ ) {
                     if ( x10command[m].code == cmdcode && 
                          x10command[m].subcode == subcode &&
                          x10command[m].subsubcode == subsubcode ) 
                        break;
                  }
               }
               else {
                  for ( m = 0; m < nx10cmds; m++ ) {
                     if ( x10command[m].code == cmdcode && 
                          x10command[m].subcode == subcode ) 
                       break;
                  }
               }

               /* If not standard command, locate entry for arbitrary code */
               if ( m >= nx10cmds ) {
                  for ( m = 0; m < nx10cmds; m++ ) {
                     if ( !(strcmp(x10command[m].label, XARBNAME)) )
                        break;
                  }
               }

               flags = x10command[m].flags;
               minargs = x10command[m].minargs;
               subsubcode = x10command[m].subsubcode;

               if ( flags & F_ARB )
                  fprintf( fd, "%c %s %02X ", delim, x10command[m].label, subcode);
               else
                  fprintf( fd, "%c %s ", delim, x10command[m].label); 

               /* Display the HC, prefixed with a '+' if bitmap is not 0 */
               if ( bmap )
                  fprintf(fd, "+%c", hcode);
               else
                  fprintf( fd, "%c", hcode);

               /* Display the unit, if any */
               if ( !(flags & F_ALL) || flags & F_FAD || flags & F_ARB ) {
                  fprintf( fd, "%d", code2unit(elementp[i+3] & 0x0f) );
		  subunit = (elementp[i+3] & 0xf0u) >> 4;
		  if ( subunit > 0 )
		     fprintf( fd, "(%02x)", subunit );
	       }
               else if ( flags & F_ALL && bmap ) {
                  fprintf( fd, "%s", bmap2units(bmap) ); 
               }

               /* Display the data byte info if required */
               if ( subcode == 0x30u || subcode == 0x36u || subcode == 0x3cu ) {
                  if ( elementp[i+4] & 0x20u )
                     fprintf(fd, " %d.%-2d", elementp[i+4] >> 6, (elementp[i+4] & 0x0fu) + 1);
                  else
                     fprintf(fd, " %d", elementp[i+4] >> 6);
               }
               else if ( subcode == 0x35u ) {
                  fprintf( fd, " %s", linmap2list(elementp[i+4] & 0x0f));
               }
               else if ( subcode == 0x37u && (elementp[i+4] & 0x20u) ) {
                  if ( elementp[i+4] & 0x10u )
                     fprintf(fd, " %d.%-2d", elementp[i+4] >> 6, (elementp[i+4] & 0x0fu) + 1);
                  else
                     fprintf(fd, " %d", elementp[i+4] >> 6);
               }
               else if ( flags & F_GRP ) {
                  if ( minargs > 1 )
                     fprintf( fd, " %d", elementp[i+4] >> 6);
                  if ( minargs > 2 )
                     fprintf( fd, " %02d", elementp[i+4] & 0x3f);
               }   
               else if ( flags & F_DED )
                  fprintf( fd, " %02d", elementp[i+4]);
               else if ( flags & F_ARB )
                  fprintf( fd, " %02X", elementp[i+4]);
 
               i += x10command[m].length;
               break;

            default :
               fprintf(stderr, "display_macros(): Internal error 2.\n");
               break;               
         }

         delim = ';';
      }
      (void) fprintf( fd, "\n" );
      j++ ;
   }

   return 1;
}

/*---------------------------------------------------------------------+
 | Display error message and return 1 if configured for a CM10A        |
 | interface.  Otherwise just return 0.                                |
 +---------------------------------------------------------------------*/
int invalidate_for_cm10a ( void )
{
   if ( configp->device_type & DEV_CM10A ) {
      fprintf(stderr, "Command is invalid for CM10A interface.\n");
      return 1;
   }
   return 0;
}   

/*---------------------------------------------------------------------+
 | Instruct daemons to re-read the config file.                        |
 +---------------------------------------------------------------------*/
int c_restart_old ( int argc, char *argv[] )
{
   send_x10state_command(ST_RESTART, 0);
   return 0;
}


/*---------------------------------------------------------------------+
 | Instruct daemons to re-read the config file.                        |
 +---------------------------------------------------------------------*/
int c_restart ( int argc, char *argv[] )
{
   FILE *fp;
   char message[127];
   char restart_r[PATH_LEN + 1];
   char restart_a[PATH_LEN + 1];
   int  rflag = 1, aflag = 0;
   int  count = 100;
   struct stat rstat, astat;

   char *ignoretp;

   strncpy2(restart_r, pathspec(RESTART_RELAY_FILE), PATH_LEN);

   if ( config.ttyaux[0] != '\0' /* check_for_aux() == 0 */ ) {
      strncpy2(restart_a, pathspec(RESTART_AUX_FILE), PATH_LEN);
      aflag = 1;
   }   
   
   if ( !(fp = fopen(restart_r, "w")) ) {
      fprintf(stderr, "Unable to restart heyu_relay.\n");
      return 1;
   }
   fclose(fp);

   if ( aflag ) {
      if ( !(fp = fopen(restart_a, "w")) ) {
         fprintf(stderr, "Unable to restart heyu_aux.\n");
         unlink(restart_r);
         return 1;
      }
      fclose(fp);
   }
    
   while ( count-- && (rflag || aflag) ) {
      millisleep(100);
      rflag = (rflag && !stat(restart_r, &rstat)) ? 1 : 0;
      aflag = (aflag && !stat(restart_a, &astat)) ? 1 : 0;
   }

   if ( count == 0 ) {
      if ( rflag ) {
         fp = fopen(restart_r, "r");
         ignoretp = fgets(message, sizeof(message), fp);
         fprintf(stderr, "%s", message);
         fclose(fp);         
         fprintf(stderr, "Restart heyu_relay failed.\n");
         unlink(restart_r);
      }
      if ( aflag ) {
         fp = fopen(restart_a, "r");
         ignoretp = fgets(message, sizeof(message), fp);
         fprintf(stderr, "%s", message);
         fclose(fp);         
         fprintf(stderr, "Restart heyu_aux failed.\n");
         unlink(restart_a);
      }
      return 1;
   }

   return 0;
}

/*---------------------------------------------------------------------+
 | Global script enable/disable                                        |
 +---------------------------------------------------------------------*/
int c_script_ctrl ( int argc, char *argv[] )
{
   extern int check_for_engine ( void );
   int funct;

   if ( argc != 3 ||
      (funct = (strcmp(strlower(argv[2]), "disable") == 0) ? NO  :
               (strcmp(argv[2], "enable") == 0) ? YES : -1) < 0 ) {
      fprintf(stderr, "Usage: %s %s disable|enable\n", argv[0], argv[1]);
      return 1;
   }
      
   if ( check_for_engine() != 0 ) {
      fprintf(stderr, "Heyu state engine is not running.\n");
      return 1;
   }

   send_x10state_command(ST_LAUNCH, (unsigned char)funct);
   return 0;
}
      

/*---------------------------------------------------------------------+
 | Test powerfail script                                               |
 +---------------------------------------------------------------------*/
int c_powerfailtest ( int argc, char *argv[] )
{
   extern int check_for_engine ( void );
   unsigned char bootflag = R_NOTATSTART;
     
   if ( argc < 3 ) {
      fprintf(stderr, "Parameter 'boot' or 'notboot' required.\n");
      return 1;
   }
   
   strlower(argv[2]);
   if ( strcmp(argv[2], "boot") == 0 )
      bootflag = R_ATSTART;
   else if ( strcmp(argv[2], "notboot") == 0 )
      bootflag = R_NOTATSTART;
   else {
      fprintf(stderr, "Parameter must be 'boot' or 'notboot'\n");
      return 1;
   }

   if ( check_for_engine() != 0 ) {
      fprintf(stderr, "State engine is not running.\n");
      return 1;
   }

   send_x10state_command(ST_PFAIL, bootflag);

   return 0;
}

/*---------------------------------------------------------------------+
 | Get security flags for home, away, min, max.                        |
 +---------------------------------------------------------------------*/
int get_armed_parms ( int argc, char *argv[], unsigned char *sflags, unsigned char mode )
{
   int           j;
   unsigned long lflags;
   char          msg[80];

#if 0
   if ( check_for_engine() != 0 ) {
      store_error_message("Engine is not running.");
      return 1;
   }
#endif

   lflags = GLOBSEC_ARMED;
   *sflags = (lflags >> GLOBSEC_SHIFT);

   for ( j = 1; j < argc; j++ ) {
      if ( strchr(argv[j], '$') && mode == CMD_VERIFY )
         continue;
      strlower(argv[j]);
      if ( strcmp(argv[j], "home") == 0 )
         lflags |= GLOBSEC_HOME;
      else if ( strcmp(argv[j], "away") == 0 )
         lflags &= ~GLOBSEC_HOME;
      else if ( strcmp(argv[j], "min") == 0 ) {
         lflags |= GLOBSEC_ARMED;
         lflags &= ~GLOBSEC_PENDING;
      }
      else if ( strcmp(argv[j], "max") == 0 ) {
         lflags &= ~GLOBSEC_ARMED;
         lflags |= GLOBSEC_PENDING;
      }
      else {
         sprintf(msg, "Invalid arm parameter '%s'\n", argv[j]);
         store_error_message(msg);
         return 1;
      }
   }

   *sflags = (lflags >> GLOBSEC_SHIFT);

   return 0;   
} 

/*---------------------------------------------------------------------+
 | Set the system flags for Armed or Disarmed - LEGACY                 |
 +---------------------------------------------------------------------*/
int c_arm ( int argc, char *argv[] )
{
   int j;
   unsigned long sflags;

   if ( argc < 2 ) {
      fprintf(stderr, "Too few parameters.\n");
      return 1;
   }

   if ( check_for_engine() != 0 ) {
      fprintf(stderr, "Engine is not running.\n");
      return 1;
   }


   if ( strcmp(argv[1], "_disarm") == 0 ) {
      send_x10state_command(ST_SECFLAGS, 0);
      return 0;
   }
   else if ( strcmp(argv[1], "_arm") != 0 ) {
      fprintf(stderr, "Invalid command '%s'\n", argv[1]);
      return 1;
   }

   sflags = GLOBSEC_ARMED;

   for ( j = 2; j < argc; j++ ) {
      strlower(argv[j]);
      if ( strcmp(argv[j], "home") == 0 )
         sflags |= GLOBSEC_HOME;
      else if ( strcmp(argv[j], "away") == 0 )
         continue;
      else if ( strcmp(argv[j], "min") == 0 ) {
         sflags |= GLOBSEC_ARMED;
         sflags &= ~GLOBSEC_PENDING;
      }
      else if ( strcmp(argv[j], "max") == 0 ) {
         sflags &= ~GLOBSEC_ARMED;
         sflags |= GLOBSEC_PENDING;
      }
      else {
         fprintf(stderr, "Invalid arm parameter '%s'\n", argv[j]);
         return 1;
      }
   }

   sflags = sflags >> GLOBSEC_SHIFT;

   send_x10state_command(ST_SECFLAGS, (unsigned char)sflags);

   return 0;
} 

/*---------------------------------------------------------------------+
 | Read ext3 code group as either absolute, e.g., "0", or relative,    |
 | e.g. "0.1").  Subgroups are entered and displayed as numbered 1-16  |
 | but converted to 0x80-0x8f to distinguish internal subgroup 0 from  |
 | absolute group 0                                                    |
 +---------------------------------------------------------------------*/
int parse_ext3_group ( char *token, unsigned char *group, unsigned char *subgroup )
{
   char errmsg[80];
   char *sp, *tp;
   int  igroup;

   igroup = (int)strtol(token, &sp, 10);

   if ( strchr(" ./t/n", *sp) == NULL || igroup < 0 || igroup > 3 ) {
      sprintf(errmsg, "Invalid group '%s'", token);
      store_error_message(errmsg);
      return 1;
   }
   else if ( *sp == '.' ) {
      *group = (unsigned char)igroup;
      tp = sp + 1;
      igroup = (int)strtol(tp, &sp, 10);
      if ( strchr(" /t/n", *sp) == NULL || igroup < 0 || igroup > 16 ) {
         sprintf(errmsg, "Invalid subgroup '%s'", tp);
         store_error_message(errmsg);
         return 1;
      }
      else if ( igroup == 0 ) {
         *subgroup = 0;
      }
      else {
         *subgroup = (unsigned char)(igroup - 1) | 0x80;
      }
   }
   else {
      *group = (unsigned char)igroup;
      *subgroup = 0;
   }

   return 0;
}


int restore_groups ( unsigned char hcode )
{

   int           group, lastgroup;
   int           tokc;
   char          **tokv = NULL;
   unsigned char ucode, xdata, gflag, grflag;
   int           unit, xgrc;
   unsigned int  bitmap;
   char          hc;
   char          cmdline[80];
   unsigned char grpmember[16], grpaddr[16], grplevel[16][4], xconfigmode;

   extern unsigned int modmask[][16];
   extern int fetch_x10state ( void );
   extern int copy_xmodule_info ( unsigned char, unsigned char [], unsigned char [],
              unsigned char [][4], unsigned char * );

   if ( fetch_x10state() != 0 )
      return 1;

   hc = code2hc(hcode);

   /* Copy xmodule info from x10state table to temp arrays */
   copy_xmodule_info(hcode, grpmember, grpaddr, grplevel, &xconfigmode);

   /* Determine if groups and group reference is used for any unit */
   grflag = gflag = 0;
   for ( ucode = 0; ucode < 16; ucode++ ) {
      if ( grpmember[ucode] & 0x0fu ) {
         gflag = 1;
         if ( grpaddr[ucode] & 0x80u ) {
            grflag = 1;
            break;
         }
      }
   }

   if ( gflag == 0 ) {
      /* No groups - restore xconfig mode if required and return */
      if ( modmask[Ext3StatusMask][hcode] & 0xffff ) {
         sprintf(cmdline, "%s 3b %c1 %02x", XARBNAME, hc, xconfigmode);
         tokenize(cmdline, " ", &tokc, &tokv);
         direct_command(tokc, tokv, CMD_RUN);
         free(tokv);
      }
      return 0;
   }

   /* Remove all units from all groups via xfunc 35 */
   sprintf(cmdline, "%s 35 %c1 ff", XARBNAME, hc);
   tokenize(cmdline, " ", &tokc, &tokv);
   direct_command(tokc, tokv, CMD_RUN);
   free(tokv); 

   if ( grflag && xconfigmode > 0 ) {
      /* Disable extended status ack */
      sprintf(cmdline, "%s 3b %c1 00", XARBNAME, hc);
      tokenize(cmdline, " ", &tokc, &tokv);
      direct_command(tokc, tokv, CMD_RUN);
      free(tokv);
   }

   for ( ucode = 0; ucode < 16; ucode++ ) {
      if ( (grpmember[ucode] & 0x0fu) == 0 )
         continue;

      unit = code2unit(ucode);
      bitmap = 1 << ucode;

      xgrc = (modmask[Ext3GrpRelT1Mask][hcode] & bitmap) ? 1 :
             (modmask[Ext3GrpRelT2Mask][hcode] & bitmap) ? 2 :
             (modmask[Ext3GrpRelT3Mask][hcode] & bitmap) ? 3 :
             (modmask[Ext3GrpRelT4Mask][hcode] & bitmap) ? 4 : 0;

      lastgroup = 4;
      for ( group = 0; group < 4; group++ ) {
         if ( (grpmember[ucode] & (1 << group)) == 0 )
            continue;

         if ( grpaddr[ucode] & 0x80u ) {
            /* Relative to group reference */
            /* Preset module to level with xfunc 31 */
            xdata = grplevel[ucode][group] & 0x3fu;
            sprintf(cmdline, "%s 31 %c%d %02x", XARBNAME, hc, unit, xdata);
            tokenize(cmdline, " ", &tokc, &tokv);
            if ( direct_command(tokc, tokv, CMD_RUN) != 0 )
               fprintf(stderr, "%s\n", error_message());
            free(tokv);
            /* Add to group with reference with xfunc 30 */
            xdata = (group << 6) | 0x20 | (grpaddr[ucode] & 0x0fu);
            sprintf(cmdline, "%s 30 %c%d %02x", XARBNAME, hc, unit, xdata);
            tokenize(cmdline, " ", &tokc, &tokv);
            if ( direct_command(tokc, tokv, CMD_RUN) != 0 )
               fprintf(stderr, "%s\n", error_message());
            free(tokv);
         }
         else {
            /* Set Absolute group with xfunc 32 */
            xdata = (group << 6) | (grplevel[ucode][group] & 0x3fu);
            sprintf(cmdline, "%s 32 %c%d %02x", XARBNAME, hc, unit, xdata);
            tokenize(cmdline, " ", &tokc, &tokv);
            if ( direct_command(tokc, tokv, CMD_RUN) != 0 )
               fprintf(stderr, "%s\n", error_message());
            free(tokv);
         } 
      }
   }

   /* Restore xstatus if required */
   if ( modmask[Ext3StatusMask][hcode] & 0xffff ) {
      sprintf(cmdline, "%s 3b %c1 %02x", XARBNAME, hc, xconfigmode);
      tokenize(cmdline, " ", &tokc, &tokv);
      direct_command(tokc, tokv, CMD_RUN);
      free(tokv);
   }

   return 0;
}

int c_restore_groups ( int argc, char *argv[] )
{
   char hc;

   for ( hc = 'A'; hc <= 'P'; hc++ ) {
      if ( restore_groups(hc2code(hc)) != 0 ) {
         fprintf(stderr, "restore_groups failed for housecode %c\n", hc);
         return 1;
      }
   }

   return 0;
}


#if 0
void display_help ( char *helplist[][3] )
{
   int  j;
   int  szlbl, szarg, sztot;
   char buffer[128];

   /* Get max string lengths for formatting output */
   szlbl = szarg = sztot = 0;
   j = 0;
   while ( helplist[j][0] != NULL ) {
      szlbl = max(szlbl, (int)strlen(helplist[j][0]));
      szarg = max(szarg, (int)strlen(helplist[j][1]));
      sztot = max(sztot, ((int)strlen(helplist[j][0]) + 
                               (int)strlen(helplist[j][1])));
      j++;
   }

   j = 0;
   while ( helplist[j][0] != NULL ) {
      #ifdef COMPACT_FORMAT
         /* Compact format */
         sprintf(buffer, "%s  %s", helplist[j][0], helplist[j][1]);    
         printf("  heyu  %-*s  %s\n", sztot + 2, buffer, helplist[j][2]);
      #else
         printf("  heyu  %-*s %-*s  %s\n", szlbl, helplist[j][0],
          szarg, helplist[j][1], helplist[j][2]);
      #endif  /* End ifdef */
      j++;
   }
   return;
}
#endif


#if 0
void display_dir_help ( unsigned long flags, unsigned long notflags )
{
   int  j;
   int  szlbl, szarg, sztot;
   char buffer[128];
   struct cmd_list *one = NULL, *two;

   /* Direct commands */
   /* Get max string lengths for formatting output */
   szlbl = szarg = sztot = 0;
   for ( j = 0; j < nx10cmds; j++ ) {
      if ( x10command[j].flags & F_HID ||
           x10command[j].flags & notflags ||
           !(x10command[j].flags & flags) )  {
         continue;
      }
      sztot = max(sztot, ((int)strlen(x10command[j].label) + 
                      (int)strlen(helparg[x10command[j].argtype])) );
   }
   for ( j = 0; j < nx10cmds; j++ ) {
      two = &x10command[j];
      if ( cmp_commands(one, two) ) {
         sprintf(buffer, "%s  %s", x10command[j].label, helparg[x10command[j].argtype]);
         printf("  heyu  %-*s  %s%s\n", sztot + 2, buffer,
             helpdirect[x10command[j].help],
             (x10command[j].flags & (F_NMA | F_NMAI)) ? " (*)" : "");
         one = two;
      }
   }
   return;
}    
#endif

#if 0
int c_logtail_new ( int argc, char *argv[] )
{
   int  lines;
   char cmdline[PATH_LEN + PATH_LEN + 20];
   char *sp;

   get_configuration(CONFIG_INIT);

   if ( strcmp(configp->logfile, "/dev/null") == 0 ) {
      fprintf(stderr, "LOG_DIR not defined in config file.\n");
      return 1;
   }

   if ( *configp->tailpath == '\0' ) {
      strcpy(cmdline, "tail");
   }
   else {
      strncpy2(cmdline, configp->tailpath, PATH_LEN);
   }

   if ( argc == 3 ) {
      if ( strcmp(argv[2], "-f") == 0 ) {
         sprintf(cmdline + strlen(cmdline), " -f %s", configp->logfile);
         system(cmdline);
         for (;;) {
            sleep(1);
         }
      }

      lines = (int)strtol(argv[2], &sp, 10);
      if ( strchr(" \t\r\n", *sp) == NULL ) {
         fprintf(stderr, "Invalid 'tail' lines '%s'\n", argv[2]);
         return 1;
      }
      lines = (lines < 0) ? -lines : lines;
      sprintf(cmdline + strlen(cmdline), " -%d", lines);
   }

   sprintf(cmdline + strlen(cmdline), " %s", configp->logfile);

   return system(cmdline);

}
#endif

int c_logtail ( int argc, char *argv[] )
{
   int  lines;
   char cmdline[PATH_LEN + PATH_LEN + 20];
   char *sp;

   get_configuration(CONFIG_INIT);

   if ( strcmp(configp->logfile, "/dev/null") == 0 ) {
      fprintf(stderr, "LOG_DIR not defined in config file.\n");
      return 1;
   }

   if ( *configp->tailpath == '\0' ) {
      strcpy(cmdline, "tail");
   }
   else {
      strcpy(cmdline, configp->tailpath);
   }

   if ( argc == 3 ) {
      lines = (int)strtol(argv[2], &sp, 10);
      if ( strchr(" \t\r\n", *sp) == NULL ) {
         fprintf(stderr, "Invalid 'tail' lines '%s'\n", argv[2]);
         return 1;
      }
      lines = (lines < 0) ? -lines : lines;
      sprintf(cmdline + strlen(cmdline), " -%d", lines);
   }

   sprintf(cmdline + strlen(cmdline), " %s", configp->logfile);

   return system(cmdline);

}


