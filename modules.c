

/*----------------------------------------------------------------------------+
 |                                                                            |
 |                  X10 Module Attributes for HEYU                            |
 |             Copyright 2004-2008 Charles W. Sullivan                        |
 |                                                                            |
 |                                                                            |
 | As used herein, HEYU is a trademark of Daniel B. Suthers.                  | 
 | X10, CM11A, and ActiveHome are trademarks of X-10 (USA) Inc.               |
 | SwitchLinc and LampLinc are trademarks of Smarthome, Inc.                  |
 | The author is not affiliated with any of these entities.                   |
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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#if defined(SYSV) || defined(FREEBSD) || defined(OPENBSD)
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <ctype.h>
#include "process.h"
#include "oregon.h"

/* Standard module attribute/response definitions for cflags */
/* Basic function codes 0-6 (maintain this order) */
#define  UNOFF     0x00000001     /* All Units Off */
#define  LION      0x00000002     /* All Lights ON */
#define  MON       0x00000004     /* Simple module ON */
#define  MOFF      0x00000008     /* Simple module OFF */
#define  DIM       0x00000010     /* Dim 1-22 */
#define  BRI       0x00000020     /* Bright 1-22 */
#define  LIOFF     0x00000040     /* All Lights OFF */
/* Other attributes */
#define  ALLON     0x00000080     /* All Units On */
#define  STDX10    0x00000100     /* Standard X10 */
#define  PRESET    0x00000200     /* Preset 1-32 */
#define  BRIB4DIM  0x00000400     /* If OFF, full Brightness before dimming */
#define  STAREQ    0x00000800     /* Responds to Status Request */
#define  STAON     0x00001000     /* Sends StatusOn ack */
#define  STAOFF    0x00002000     /* Sends StatusOff ack */
#define  LIONFULL  0x00004000     /* All Lights ON -> Full brightness */
#define  ONFULL    0x00008000     /* ON -> Full (or option) brightness */
#define  ONFULLOFF 0x00010000     /* ON -> Full brightness if Off */
#define  RESUME    0x00020000     /* ON resumes previous level */
#define  RESUMEDIM 0x00040000     /* Dim/Bright first resumes previous level */
#define  LIONUNAD  0x00080000     /* AllLightsOn unaddresses the module */
#define  LIOFFUNAD 0x00100000     /* AllLightsOff unaddresses the module */
#define  TARG      0x00200000     /* Is a target instead of a module */
#define  EXC16     0x00400000     /* Last unit turns On (all others turn off) */
#define  EXC8      0x00800000     /* Last unit turns On (others in group of 8 turn off) */
#define  EXC4      0x01000000     /* Last unit turns On and others in group of 4 turn off) */
#define  VDATA     0x02000000     /* Virtual data repository */
#define  PLCSENSOR 0x04000000     /* PLC Sensor */
#define  ADDRESSED 0x08000000     /* Has standard address */
#define  PHYS      0x10000000     /* Is a physical module */
#define  ONOFFUNAD 0x20000000     /* On or Off unaddresses the module (ACT bug) */

/* NOTE: Attributes STDX10, PRESET, and EXT3SW/EXT3DIM are */
/* mutually incompatible.                                  */

/* Extended code module attributes for xflags */
/* (Group reference characteristic types are mutually incompatible.) */
#define  XNON      0
#define  X3SW      0x00000001     /* Extended codes Type 3 (switch only) */
#define  X3DIM     0x00000002     /* Extended codes Type 3 with preset dim 0-63 */
#define  X0SH      0x00000004     /* Extended code Type 0 shutter control */
#define  X3GEXEC   0x00000008     /* Supports Extended Group Exec command */
#define  X3GRC1    0x00000010     /* Group reference characteristic Type 1 (LM14A, AM14A) */
#define  X3GRC2    0x00000020     /* Group reference characteristic Type 2 (LM465-1) */
#define  X3GRC3    0x00000040     /* Group reference characteristic Type 3 (WS467-1) */
#define  X3GRC4    0x00000080     /* Group reference characteristic Type 4 */
#define  X3GOFF    0x00000100     /* Extended Group Off */
#define  X3GOFFEX  0x00000200     /* Extended Group Off acts like Group Exec */
#define  X3GBD     0x00000400     /* Extended Group Bright/Dim */
#define  X3GBDFULL 0x00000800     /* Ext Grp Bri/Dim resumes, or brightens full if prev at level 0 */
#define  X3STAT    0x00001000     /* Extended code status, i.e., 2-way */
#define  X3GREM    0x00002000     /* Extended Group Remove works */
#define  X0PRESET  0x00004000     /* Extended code Type 0 shutter preset */

/* Virtual model attributes for vflags */
#define  VNON   0
#define  VSTD   0x00000001   /* X10 Standard */
#define  VENT   0x00000002   /* X10 Entertainment */
#define  VSEC   0x00000004   /* X10 Security */
#define  VRFXS  0x00000008   /* RFX Sensor */
#define  VRFXM  0x00000010   /* RFX Meter */
#define  VDMX   0x00000020   /* Digimax */
#define  VORE   0x00000040   /* Oregon */
#define  VKAKU  0x00000080   /* KAKU */

/* KaKu model attributes for kflags */
#define  KOFF     0x00000001   /* Off */
#define  KON      0x00000002   /* On */
#define  KGOFF    0x00000004   /* Group Off */
#define  KGON     0x00000008   /* Group On */
#define  KPRE     0x00000010   /* Preset */
#define  KGPRE    0x00000020   /* Group Preset */
#define  KRESUME  0x00000040   /* Resume */
#define  KPHYS    0x00000080   /* Physical module */

/* Module max dim levels */
#define  MXL0        0
#define  MXLS      210         /* Standard X10 modules 0-210 */
#define  MXLP       31         /* Preset modules 0-31 (zero-base) */
#define  MXLE       62         /* Extended code Type 3 dimmer modules 0-62 */
#define  MXLEA      63         /* Extended code Type 3 appliance modules 0,63 */
#define  MXLS0      25         /* Extended code Type 0 shutters 0-25 */
#define  MXLV      255         /* Virtual modules 0-255 */
#define  MXLK       15         /* KAKU modules 0-15 */

/* Standard module type attributes for cflags .  To add a new module type,  */
/* "OR" together its attributes here and add an entry in the modules */
/* table below. (Keep the table uncluttered, for future additions.) */ 
#define  NOMATT      0
#define  BASIC       (UNOFF | MON | MOFF | ALLON | ADDRESSED | PHYS)
#define  ACTBASIC    (STDX10 | MON | MOFF | ALLON | ADDRESSED | PHYS | STAON | STAOFF | STAREQ)
#define  ACTBUG      (ACTBASIC | ONOFFUNAD)

#define  STDAM       (BASIC | STDX10)
#define  STDLM       (STDAM | DIM | BRI | BRIB4DIM | LION | ONFULLOFF)
#define  STDWS       (STDLM | LIOFF)
#define  AMS         (STDAM | STAON | STAOFF | STAREQ)
#define  LMS         (STDLM | STAON | STAOFF | STAREQ)
#define  SIREN       (STDAM | LION )
#define  REM2        (STDX10 | TARG | MON | MOFF)
#define  REM3        (STDX10 | TARG | MOFF | BRI | DIM) 
#define  REM4        (STDX10 | TARG | MON | MOFF | BRI | DIM) 
#define  REM6        (STDX10 | TARG | MON | MOFF | BRI | DIM | LION | UNOFF) 

#define  LM15A       (STDAM | LION | LIONFULL | LIOFF | LIONUNAD | LIOFFUNAD)
#define  XPS3        (STDAM | LION | LIONFULL | LIOFF)
#define  XPD3        (STDLM | LIOFF)

#define  PR511       (STDAM | STAON | STAOFF | STAREQ | LION | LIONFULL | LIOFF)

#define  AM14A       (BASIC | ALLON | STAON | STAOFF | STAREQ)
#define  LM14A       (AM14A | DIM | BRI | LION | LIOFF | RESUME | RESUMEDIM | LIOFFUNAD)

#define  SL1AM       (BASIC)
#define  SL2AM       (SL1AM | STAREQ)

#define  SL1LM       (BASIC | PRESET | DIM | BRI | LION | LIOFF | ONFULL)
#define  SL2LM       (SL1LM | STAREQ)

#define  LL1LM       (BASIC | PRESET | DIM | BRI | LION | LIOFF | LIONFULL | ONFULL)
#define  LL2LM       (LL1LM | STAREQ)
#define  AMEXC16     (STDAM | EXC16)
#define  AMEXC8      (STDAM | EXC8 )
#define  AMEXC4      (STDAM | EXC4 )
#define  CAMEXC4     (STDX10 | EXC4 | MON | MOFF | ADDRESSED | PHYS)
#define  VIRT4       (TARG | MON | MOFF | BRI | DIM)
#define  SHUT0       (MON | MOFF | RESUME | PHYS)
#define  VIRTUAL     (TARG | VDATA)
#define  PALMPAD     (TARG | MON | MOFF | BRI | DIM)
#define  KEYCHAIN    (TARG | MON | MOFF)
#define  ONLYON      (TARG | MON)
#define  ONLYOFF     (TARG | MOFF)
#define  PLCSEN      (TARG | MON | MOFF | ADDRESSED | PLCSENSOR)
#define  MOTION      (TARG | MON | MOFF)

#define  LM_1        (BASIC | DIM | BRI | LION | LIOFF | RESUMEDIM | LIOFFUNAD)
#define  WS_1        (BASIC | DIM | BRI | LION | RESUME)

/* Extended module type attributes for xflags */

#define XAM14A        (X3SW | X3GEXEC | X3GRC1 | X3GOFFEX | X3STAT | X3GREM)
#define XLM14A        (X3SW | X3GEXEC | X3DIM | X3GRC1 | X3GOFFEX | X3STAT | X3GREM)
#define XLM_1         (X3SW | X3GEXEC | X3DIM | X3GRC2 | X3GOFF | X3GBD | X3GBDFULL)
#define XWS_1         (X3SW | X3GEXEC | X3DIM | X3GRC3 | X3GOFFEX | X3GREM)
#define XSHUT0        (X0SH)

/* KaKu module type attributes */
#define KAM           (KOFF | KON | KGOFF | KGON)
#define KLM           (KAM | KPRE | KGPRE | KRESUME)

/* Module option functions */
int opt_onlevel(), opt_sremote(), opt_kremote(), opt_sensor(), opt_svsensor(), opt_ds90(), opt_sd90(),
opt_ur81a(), opt_ux17a(), opt_guru(), opt_aux(), opt_rfxsensor(), opt_rfxold(), opt_act(), opt_defer(),
opt_rfxtemp(),
opt_rfxpulse(), opt_rfxcount(), opt_rfxpower(), opt_rfxwater(), opt_rfxgas(),
opt_sd10(), opt_plcsensor(), opt_gb10(), opt_jam(), opt_digimax(),
opt_oreTH1(), opt_oreTH2(), opt_oreTH3(), opt_oreTH4(), opt_oreTH5(), opt_oreTH6(),
opt_oreTemp1(), opt_oreTemp2(), opt_oreTemp3(), opt_oreTHB1(), opt_oreTHB2(), opt_oreWeight1(), 
opt_oreTemu(), opt_oreTHemu(), opt_oreTHBemu(), opt_x10std(), opt_oreignore(), opt_elsElec1(),
opt_bmb_sd18(),
opt_secignore(), opt_visonic(),
opt_oreWind1(), opt_oreWind2(), opt_oreWind3(),
opt_oreRain1(), opt_oreRain2(), opt_oreRain3(),
opt_oreUV1(), opt_oreUV2(), opt_kaku(), opt_owlElec2(), opt_owlElec2new(), opt_owlElec2rev();

/* Decoder functions for modules */
int fn_ds10a(), fn_ds90(), fn_ms10a(), fn_sh624(), fn_kr10a(), fn_ur81a(),
fn_guru(), fn_rfxsensor(),
fn_rfxpulse(), fn_rfxcount(), fn_rfxpower(), fn_rfxwater(), fn_rfxgas(),
fn_sd10(), fn_sd90(), fn_ms90(), fn_ds18(), fn_gb10(), fn_svdata(), fn_jam(),
fn_kr15a(), fn_kr18(), fn_dm10(), fn_bmb_sd18(), fn_visonic();

extern int ore_maxmin_temp ( ALIAS *, int, char **, int * );
extern int ore_maxmin_rh ( ALIAS *, int, char **, int * );
extern int ore_maxmin_bp ( ALIAS *, int, char **, int * );
extern double celsius2temp ( double, char, double);
int sensor_timeout ( ALIAS *, int, char **, int * );



struct modules_st {
  char           *label;  /* Case insensitive */
  int            maxlevel;
  unsigned long  vflags;  /* Virtual attributes */
  unsigned long  cflags;  /* Standard attributes */
  unsigned long  xflags;  /* Extended code attributes */
  int (*addopt_func)();
  int (*xlate_func)();
} modules[] = {
  {"NONE",        MXLS, VNON, NOMATT,     0, NULL, NULL  },  /* Has no attributes */
  {"StdAM",       MXLS, VNON, STDAM,      0, NULL, NULL  },  /* Standard X10 1-way Appliance Module */
  {"AM",          MXLS, VNON, STDAM,      0, NULL, NULL  },  /* Standard X10 1-way Appliance Module */
  {"AM486",       MXLS, VNON, STDAM,      0, NULL, NULL  },  /* Standard X10 1-way Appliance Module */
  {"AM12",        MXLS, VNON, STDAM,      0, NULL, NULL  },  /* Marmitek Standard X10 1-way Appliance Module */
  {"PAM01",       MXLS, VNON, STDAM,      0, NULL, NULL  },  /* Standard X10 1-way Appliance Module */
  {"AM466",       MXLS, VNON, STDAM,      0, NULL, NULL  },  /* Standard X10 1-way Appliance Module */
  {"PAM02",       MXLS, VNON, STDAM,      0, NULL, NULL  },  /* Standard X10 1-way Appliance Module */
  {"SR227",       MXLS, VNON, STDAM,      0, NULL, NULL  },  /* Standard X10 1-way Appliance Module */
  {"PA011",       MXLS, VNON, STDAM,      0, NULL, NULL  },  /* Standard X10 1-way Appliance Module */
  {"AMS",         MXLS, VNON, AMS,        0, opt_defer, NULL  },  /* 2-way Appliance Module */
  {"RAIN8II",     MXLS, VNON, AMS,        0, opt_defer, NULL  },  /* Rain8II 2-way Irrigation Module */
  {"RR501",       MXLS, VNON, AMS,        0, NULL, NULL  },  /* X10 Transceiver/Switch */
  {"PAT01",       MXLS, VNON, AMS,        0, NULL, NULL  },  /* X10 Transceiver/Switch */
  {"ACTAMS",      MXLS, VNON, ACTBASIC,   0, opt_act, NULL}, /* ACT programmable appliance module */
  {"RS114",       MXLS, VNON, ACTBASIC,   0, opt_act, NULL}, /* ACT programmable appliance module */
  {"ACTAMSBUG",   MXLS, VNON, ACTBUG,     0, opt_act, NULL}, /* ACT as above with On/Off unaddress bug */
  {"RF234",       MXLS, VNON, ACTBUG,     0, opt_act, NULL}, /* ACT as above with On/Off unaddress bug */
  {"StdLM",       MXLS, VNON, STDLM,      0, NULL, NULL  },  /* Standard X10 1-way Lamp Module */
  {"LM",          MXLS, VNON, STDLM,      0, NULL, NULL  },  /* Standard X10 1-way Lamp Module */
  {"LM465",       MXLS, VNON, STDLM,      0, NULL, NULL  },  /* Standard X10 1-way Lamp Module */
  {"LM12",        MXLS, VNON, STDLM,      0, NULL, NULL  },  /* Marmitek X10 1-way Lamp Module */
  {"LM465-1",     MXLE, VNON, LM_1,   XLM_1, NULL, NULL  },  /* Redesigned (2007) X10 1-way Lamp Module */
  {"LM-1",        MXLE, VNON, LM_1,   XLM_1, NULL, NULL  },  /* Redesigned (2007) X10 1-way Lamp Module */
  {"PLM03",       MXLS, VNON, STDLM,      0, NULL, NULL  },  /* Standard X10 1-way Lamp Module */
  {"PLM01",       MXLS, VNON, STDLM,      0, NULL, NULL  },  /* Standard X10 1-way Lamp Module */
  {"StdWS",       MXLS, VNON, STDWS,      0, NULL, NULL  },  /* Standard X10 1-way Lamp Wall Switch */
  {"WS",          MXLS, VNON, STDWS,      0, NULL, NULL  },  /* Standard X10 1-way Lamp Wall Switch */
  {"WS467",       MXLS, VNON, STDWS,      0, NULL, NULL  },  /* Standard X10 1-way Lamp Wall Switch */
  {"LW10U",       MXLS, VNON, STDWS,      0, NULL, NULL  },  /* Marmitek Standard X10 1-way Lamp Wall Switch */
  {"PLW01",       MXLS, VNON, STDWS,      0, NULL, NULL  },  /* Standard X10 1-way Lamp Wall Switch */
  {"WS477",       MXLS, VNON, STDWS,      0, NULL, NULL  },  /* Standard X10 3-way Lamp Wall Switch */
  {"PLW02",       MXLS, VNON, STDWS,      0, NULL, NULL  },  /* Standard X10 1-way Lamp Wall Switch */
  {"WS467-1",     MXLE, VNON,  WS_1,  XWS_1, NULL, NULL  },  /* Redesigned (2007) X10 1-way Lamp Wall Switch */
  {"WS-1",        MXLE, VNON,  WS_1,  XWS_1, NULL, NULL  },  /* Redesigned (2007) X10 1-way Lamp Wall Switch */
  {"WS12A",       MXLS, VNON, XPD3,       0, NULL, NULL  },  /* X10 1-way Lamp Wall Switch */
  {"XPD3",        MXLS, VNON, XPD3,       0, NULL, NULL  },  /* X10 Pro 1-way Lamp Wall Switch */
  {"WS13A",       MXLS, VNON, XPS3,       0, NULL, NULL  },  /* X10 1-way non-dimming Wall Switch */
  {"XPS3",        MXLS, VNON, XPS3,       0, NULL, NULL  },  /* X10 Pro 1-way non-dimming Wall Switch */
  {"LM15A",       MXLS, VNON, LM15A,      0, NULL, NULL  },  /* X10 LM15A Socket Rocket */
  {"LM15",        MXLS, VNON, LM15A,      0, NULL, NULL  },  /* Marmitek Socket Rocket */
  {"PSM04",       MXLS, VNON, LM15A,      0, NULL, NULL  },  /* X10 Pro Socket Rocket */
  {"LMS",         MXLS, VNON, LMS,        0, NULL, NULL  },  /* 2-way Lamp Module */
  {"PR511",       MXLS, VNON, PR511,      0, NULL, NULL  },  /* X10 2-way Motion Sensor floodlight */
  {"PHS01",       MXLS, VNON, PR511,      0, NULL, NULL  },  /* X10 Pro 2-way Motion Sensor floodlight */
  {"AM14A",       MXLEA, VNON, AM14A, XAM14A, NULL, NULL  },  /* X10 2-way Appliance Module, 2-pin (AM14A) */
  {"PAM21",       MXLEA, VNON, AM14A, XAM14A, NULL, NULL  },  /* X10 2-way Appliance Module, 2-pin (AM14A) */
  {"AM15A",       MXLEA, VNON, AM14A, XAM14A, NULL, NULL  },  /* X10 2-way Appliance Module, 3-pin (AM15A) */
  {"PAM22",       MXLEA, VNON, AM14A, XAM14A, NULL, NULL  },  /* X10 2-way Appliance Module, 3-pin (AM15A) */
  {"LM14A",       MXLE, VNON, LM14A, XLM14A, NULL, NULL  },  /* X10 2-way Lamp Module (LM14A) */
  {"PLM21",       MXLE, VNON, LM14A, XLM14A, NULL, NULL  },  /* X10 2-way Lamp Module (LM14A) */
  {"SL1AM",       MXLP, VNON, SL1AM,      0, NULL, NULL  },  /* SwitchLinc 1-way Switch */
  {"SL2AM",       MXLP, VNON, SL2AM,      0, NULL, NULL  },  /* SwitchLinc 2-way Switch */
  {"SL1LM",       MXLP, VNON, SL1LM,      0, opt_onlevel, NULL  },  /* SwitchLinc 1-way Lamp Module */
  {"SL2LM",       MXLP, VNON, SL2LM,      0, opt_onlevel, NULL  },  /* SwitchLinc 2-way Lamp Module */
  {"SL2380W",     MXLP, VNON, SL2LM,      0, opt_onlevel, NULL  },  /* SwitchLinc 2380W Dimmer */
  {"LL1LM",       MXLP, VNON, LL1LM,      0, opt_onlevel, NULL  },  /* LampLinc 1-way Dimmer */
  {"LL2LM",       MXLP, VNON, LL2LM,      0, opt_onlevel, NULL  },  /* LampLink 2-way Dimmer */
  {"LL2000STW",   MXLP, VNON, LL2LM,      0, opt_onlevel, NULL  },  /* LampLinc 2000STW Dimmer */
  {"REMOTE2",     MXLS, VNON, REM2,       0, NULL, NULL  },  /* Remote transmitter, 2 function */
  {"REMOTE3",     MXLS, VNON, REM3,       0, NULL, NULL  },  /* Remote transmitter, 3 function */
  {"REMOTE4",     MXLS, VNON, REM4,       0, NULL, NULL  },  /* Remote transmitter, 4 function */
  {"REMOTE6",     MXLS, VNON, REM6,       0, NULL, NULL  },  /* Remote transmitter, 6 function */
  {"REMOTEP",     MXLP, VNON, PRESET,     0, NULL, NULL  },  /* Remote transmitter, Preset 1-32 only */
  {"AMEXC",       MXLS, VNON, AMEXC16,    0, NULL, NULL  },  /* AM with exclusive-16 addressing */
  {"AMEXC16",     MXLS, VNON, AMEXC16,    0, NULL, NULL  },  /* AM with exclusive addressing */
  {"AMEXC8",      MXLS, VNON, AMEXC8,     0, NULL, NULL  },  /* AM with exclusive-8 addressing */
  {"RAIN8",       MXLS, VNON, AMEXC8,     0, NULL, NULL  },  /* WGL Rain8 irrigation controller */
  {"AMEXC4",      MXLS, VNON, AMEXC4,     0, NULL, NULL  },  /* AM with exclusive-4 addressing */
  {"XM10A",       MXLS, VNON, CAMEXC4,    0, NULL, NULL  },  /* X10 camera power supply */
  {"XM13A",       MXLS, VNON, CAMEXC4,    0, NULL, NULL  },  /* X10 camera power supply */
  {"XM14A",       MXLS, VNON, CAMEXC4,    0, NULL, NULL  },  /* X10 pan/tilt power supply */
  {"VIRT4",       MXLV, VSTD, VIRT4,      0, opt_onlevel, NULL  },  /* Virtual module, 4 function */
  {"VDATA",       MXLV, VSTD, VIRTUAL,    0, NULL, NULL  },  /* Virtual module data */
  {"PLCSENSOR",   MXLS, VSTD, PLCSEN,     0, opt_plcsensor, NULL  },  /* PLC Sensor target */
#ifdef HASEXT0
  {"SHUTTER",     MXLS0, VNON, SHUT0, XSHUT0, NULL, NULL  },  /* Extended code Type 0 shutter */
  {"SW10",        MXLS0, VNON, SHUT0, XSHUT0, NULL, NULL  },  /* Marmitek SW10 shutter control */
#endif

  {"DS10A",       MXLV, VSEC, VIRTUAL,    0, opt_sensor, fn_ds10a }, /* X-10 USA D/W sensor */
  {"DS10",        MXLV, VSEC, VIRTUAL,    0, opt_sensor, fn_ds10a }, /* Marmitek D/W sensor */
  {"DS10E",       MXLV, VSEC, VIRTUAL,    0, opt_sensor, fn_ds10a }, /* Marmitek D/W sensor */
  {"PDS01",       MXLV, VSEC, VIRTUAL,    0, opt_sensor, fn_ds10a }, /* X10 Pro D/W sensor */
  {"DS18",        MXLV, VSEC, VIRTUAL,    0, opt_sensor, fn_ds18 }, /* ElekHomica D/W sensor, old? */
  {"DS90",        MXLV, VSEC, VIRTUAL,    0, opt_ds90, fn_ds90 },   /* Marmitek D/W sensor */
  {"DS18-1",      MXLV, VSEC, VIRTUAL,    0, opt_ds90, fn_ds90 },   /* ElekHomica D/W sensor */

  {"MS10A",       MXLV, VSEC, VIRTUAL,    0, opt_sensor, fn_ms10a },
  {"MS90",        MXLV, VSEC, VIRTUAL,    0, opt_sensor, fn_ms90 }, /* Marmitek Motion sensor */
  {"MS18E",       MXLV, VSEC, VIRTUAL,    0, opt_sensor, fn_ms90 }, /* BMB Home Solutions Motion sensor */
  {"SD10",        MXLV, VSEC, VIRTUAL,    0, opt_sd10, fn_sd10 },
  {"BMB-SD18",    MXLV, VSEC, VIRTUAL,    0, opt_bmb_sd18, fn_bmb_sd18}, /* BMB Smoke Detector */
  {"EH-CWSD10",   MXLV, VSEC, VIRTUAL,    0, opt_sd10, fn_sd10 }, /* ElekHomica Smoke detector */
  {"EH-WD210",    MXLV, VSEC, VIRTUAL,    0, opt_sd10, fn_sd10 }, /* ElekHomica Water detector */
  {"GB10",        MXLV, VSEC, VIRTUAL,    0, opt_gb10, fn_gb10 }, /* Marmitek Glass Break detector */
  {"DM10",        MXLV, VSEC, VIRTUAL,    0, opt_gb10, fn_dm10 }, /* Marmitek DM10 Motion/Dawn/Dusk sensor */
  {"SD90",        MXLV, VSEC, VIRTUAL,    0, opt_sd90, fn_sd90 }, /* Marmitek Smoke detector */
  {"PMS01",       MXLV, VSEC, VIRTUAL,    0, opt_sensor, fn_ms10a },
  {"SH624",       MXLV, VSEC, VIRTUAL,    0, opt_sremote, fn_sh624 }, /* Full size remotes */
  {"PSR01",       MXLV, VSEC, VIRTUAL,    0, opt_sremote, fn_sh624 },
  {"KR18",        MXLV, VSEC, VIRTUAL,    0, opt_kremote, fn_kr18 }, /* Keyfob remotes */
  {"KR18E",       MXLV, VSEC, VIRTUAL,    0, opt_kremote, fn_kr18 },
  {"KR10A",       MXLV, VSEC, VIRTUAL,    0, opt_kremote, fn_kr10a },
  {"KR21",        MXLV, VSEC, VIRTUAL,    0, opt_kremote, fn_kr10a },
  {"PKR02",       MXLV, VSEC, VIRTUAL,    0, opt_kremote, fn_kr10a },
  {"KR15A",       MXLV, VSEC, VIRTUAL,    0, opt_sremote, fn_kr15a }, /* Big Red Button */
  {"SVDATA",      MXLV, VSEC, VIRTUAL,    0, opt_sremote, fn_svdata }, /* Generic security remote */
  {"SSVDATA",     MXLV, VSEC, VIRTUAL,    0, opt_svsensor, fn_svdata }, /* Generic security sensor */
  {"SEC_IGNORE",  MXLV, VSEC, VIRTUAL,    0, opt_secignore, NULL },
  {"UR81A",       MXLV, VENT, VIRTUAL,    0, opt_ur81a, fn_ur81a },
  {"UR51A",       MXLV, VENT, VIRTUAL,    0, opt_ur81a, fn_ur81a },
  {"UX17A",       MXLV, VENT, VIRTUAL,    0, opt_ux17a, NULL },
  {"UX23A",       MXLV, VENT, VIRTUAL,    0, opt_ux17a, NULL },
  {"GURU",        MXLV, VENT, VIRTUAL,    0, opt_guru, fn_guru },
  {"PALMPAD",     MXLS, VSTD, PALMPAD,    0, opt_aux, NULL},
  {"KR19A",       MXLS, VSTD, PALMPAD,    0, opt_aux, NULL},
  {"KR22",        MXLS, VSTD, PALMPAD,    0, opt_aux, NULL},
  {"KEYCHAIN",    MXLS, VSTD, KEYCHAIN,   0, opt_aux, NULL}, 
  {"ONLYON",      MXLS, VSTD, ONLYON,     0, opt_aux, NULL}, 
  {"ONLYOFF",     MXLS, VSTD, ONLYOFF,    0, opt_aux, NULL},
  {"MS12",        MXLS, VSTD, MOTION,     0, opt_x10std, NULL},
  {"MS12A",       MXLS, VSTD, MOTION,     0, opt_x10std, NULL},
  {"MS13",        MXLS, VSTD, MOTION,     0, opt_x10std, NULL},
  {"MS13A",       MXLS, VSTD, MOTION,     0, opt_x10std, NULL},
  {"MS14",        MXLS, VSTD, MOTION,     0, opt_x10std, NULL},
  {"MS14A",       MXLS, VSTD, MOTION,     0, opt_x10std, NULL},
  {"MS16",        MXLS, VSTD, MOTION,     0, opt_x10std, NULL},
  {"MS16A",       MXLS, VSTD, MOTION,     0, opt_x10std, NULL},

#ifdef HASORE
  {"ORE_TH1",     MXLV, VORE,  VIRTUAL,   0, opt_oreTH1, NULL},
  {"THGR122NX",   MXLV, VORE,  VIRTUAL,   0, opt_oreTH1, NULL},
  {"THGN123N",    MXLV, VORE,  VIRTUAL,   0, opt_oreTH1, NULL},
  {"THGR228N",    MXLV, VORE,  VIRTUAL,   0, opt_oreTH1, NULL},
  {"ORE_TH2",     MXLV, VORE,  VIRTUAL,   0, opt_oreTH2, NULL},
  {"THGN800",     MXLV, VORE,  VIRTUAL,   0, opt_oreTH2, NULL},
  {"THGR800",     MXLV, VORE,  VIRTUAL,   0, opt_oreTH2, NULL},
  {"THGR810",     MXLV, VORE,  VIRTUAL,   0, opt_oreTH2, NULL},
  {"ORE_TH3",     MXLV, VORE,  VIRTUAL,   0, opt_oreTH3, NULL},
  {"RTGN318",     MXLV, VORE,  VIRTUAL,   0, opt_oreTH3, NULL},
  {"RTGR328N",    MXLV, VORE,  VIRTUAL,   0, opt_oreTH3, NULL},
  {"ORE_TH4",     MXLV, VORE,  VIRTUAL,   0, opt_oreTH4, NULL},
  {"ORE_TH5",     MXLV, VORE,  VIRTUAL,   0, opt_oreTH5, NULL},
  {"ORE_TH6",     MXLV, VORE,  VIRTUAL,   0, opt_oreTH6, NULL},
  {"THGR918N",    MXLV, VORE,  VIRTUAL,   0, opt_oreTH6, NULL},
  {"ORE_T1",      MXLV, VORE,  VIRTUAL,   0, opt_oreTemp1, NULL},
  {"THR138",      MXLV, VORE,  VIRTUAL,   0, opt_oreTemp1, NULL},
  {"ORE_T2",      MXLV, VORE,  VIRTUAL,   0, opt_oreTemp2, NULL},
  {"THRN122N",    MXLV, VORE,  VIRTUAL,   0, opt_oreTemp2, NULL},
  {"THN122N",     MXLV, VORE,  VIRTUAL,   0, opt_oreTemp2, NULL},
  {"THN132N",     MXLV, VORE,  VIRTUAL,   0, opt_oreTemp2, NULL},
  {"ORE_T3",      MXLV, VORE,  VIRTUAL,   0, opt_oreTemp3, NULL},
  {"ORE_THB1",    MXLV, VORE,  VIRTUAL,   0, opt_oreTHB1, NULL},
  {"BTHR918",     MXLV, VORE,  VIRTUAL,   0, opt_oreTHB1, NULL},
  {"ORE_THB2",    MXLV, VORE,  VIRTUAL,   0, opt_oreTHB2, NULL},
  {"BTHR968",     MXLV, VORE,  VIRTUAL,   0, opt_oreTHB2, NULL},
  {"BTHR918N",    MXLV, VORE,  VIRTUAL,   0, opt_oreTHB2, NULL},
  {"ORE_WGT1",    MXLV, VORE,  VIRTUAL,   0, opt_oreWeight1, NULL},
  {"BWR102",      MXLV, VORE,  VIRTUAL,   0, opt_oreWeight1, NULL},
  {"ORE_TEMU",    MXLV, VORE,  VIRTUAL,   0, opt_oreTemu,   NULL},  /* Dummy */
  {"ORE_THEMU",   MXLV, VORE,  VIRTUAL,   0, opt_oreTHemu,  NULL},  /* Dummy */
  {"ORE_THBEMU",  MXLV, VORE,  VIRTUAL,   0, opt_oreTHBemu, NULL},  /* Dummy */
  {"ORE_IGNORE",  MXLV, VORE,  VIRTUAL,   0, opt_oreignore, NULL},
  {"ORE_WIND1",   MXLV, VORE,  VIRTUAL,   0, opt_oreWind1, NULL},
  {"ORE_WIND2",   MXLV, VORE,  VIRTUAL,   0, opt_oreWind2, NULL},
  {"WGR800",      MXLV, VORE,  VIRTUAL,   0, opt_oreWind2, NULL},
  {"ORE_WIND3",   MXLV, VORE,  VIRTUAL,   0, opt_oreWind3, NULL},
  {"WGR918N",     MXLV, VORE,  VIRTUAL,   0, opt_oreWind3, NULL},
  {"ORE_RAIN1",   MXLV, VORE,  VIRTUAL,   0, opt_oreRain1, NULL},
  {"PCR918N",     MXLV, VORE,  VIRTUAL,   0, opt_oreRain1, NULL},
  {"ORE_RAIN2",   MXLV, VORE,  VIRTUAL,   0, opt_oreRain2, NULL},
  {"PCR800",      MXLV, VORE,  VIRTUAL,   0, opt_oreRain2, NULL},
  {"ORE_RAIN3",   MXLV, VORE,  VIRTUAL,   0, opt_oreRain3, NULL},
  {"ELS_CM113",   MXLV, VORE,  VIRTUAL,   0, opt_elsElec1, NULL},
  {"ELS_ELEC1",   MXLV, VORE,  VIRTUAL,   0, opt_elsElec1, NULL},
  {"ORE_ELS",     MXLV, VORE,  VIRTUAL,   0, opt_elsElec1, NULL},
  {"ORE_UV1",     MXLV, VORE,  VIRTUAL,   0, opt_oreUV1, NULL},
  {"ORE_UV2",     MXLV, VORE,  VIRTUAL,   0, opt_oreUV2, NULL},
  {"OWL_ELEC2",   MXLV, VORE,  VIRTUAL,   0, opt_owlElec2new, NULL},
#endif /* HASORE */

#ifdef HASRFXS
  {"RFXSENSOR",   MXLV, VRFXS, VIRTUAL,   0, opt_rfxsensor, fn_rfxsensor},
#endif /* HASRFXS */

#ifdef HASRFXM
  {"RFXCOUNT",    MXLV, VRFXM, VIRTUAL,   0, opt_rfxcount, fn_rfxcount},
  {"RFXPOWER",    MXLV, VRFXM, VIRTUAL,   0, opt_rfxpower, fn_rfxpower},
  {"RFXWATER",    MXLV, VRFXM, VIRTUAL,   0, opt_rfxwater, fn_rfxwater},
  {"RFXGAS",      MXLV, VRFXM, VIRTUAL,   0, opt_rfxgas, fn_rfxgas},
  {"RFXPULSE",    MXLV, VRFXM, VIRTUAL,   0, opt_rfxpulse, fn_rfxpulse},
#endif /* HASRFXM */

#ifdef HASDMX
  {"DIGIMAX",     MXLV, VDMX,  VIRTUAL,   0, opt_digimax, NULL},
#endif /* HASDMX */

#ifdef HASKAKU
  {"KAKU_S",      MXLK, VKAKU, KAM,       0, opt_kaku, NULL},
  {"KAKU_P",      MXLK, VKAKU, KLM,       0, opt_kaku, NULL},
#endif /* HASKAKU */

  {"VISGEN",     MXLV, VSEC, VIRTUAL,    0, opt_visonic, fn_visonic }, /* Generic Visonic */

};
static int ntypes = (sizeof(modules)/sizeof(struct modules_st));

unsigned int modmask[NumModMasks][16];
unsigned int vmodmask[NumVmodMasks][16];
unsigned int kmodmask[NumKmodMasks][16];
unsigned char maxdimlevel[16][16];
unsigned char ondimlevel[16][16];

extern CONFIG config;
extern CONFIG *configp;


/*-------------------------------------------------------+
 | Return a pointer to a module xlate_func()             |
 +-------------------------------------------------------*/ 
int (*module_xlate_func(int index))()
{
   return modules[index].xlate_func;
}

/*-------------------------------------------------------+
 | Return the index in the module table for the argument |
 | name, or -1 if not found                              |
 | The comparison is case insensitive                    |
 +-------------------------------------------------------*/ 
int lookup_module_type ( char *modelname ) 
{
   char buffer[NAME_LEN + 1], label[NAME_LEN + 1];
   int  j;

   strncpy2(buffer, modelname, sizeof(buffer) - 1);
   strupper(buffer);

   for ( j = 0; j < ntypes; j++ ) {
      strncpy2(label, modules[j].label, sizeof(label) - 1);
      strupper(label);
      if ( strcmp(buffer, label) == 0 )
         return j;
   }

   return -1;
}

/*-------------------------------------------------------+
 | Pass back through the argument list the flags and     |
 | maxlevel values for module index 'module_type'        |
 +-------------------------------------------------------*/ 
void module_attributes ( int module_type,
      unsigned long *vflags, unsigned long *cflags,
      unsigned long *xflags, unsigned long *kflags, int *maxlevel )
{
   if ( module_type >= 0 ) {
      *vflags = modules[module_type].vflags;
      *cflags = modules[module_type].cflags;
      if ( *vflags & VKAKU ) {
         *kflags = *cflags;
         *cflags = 0;
      }
      else {
         *kflags = 0;
      }
      *xflags = modules[module_type].xflags;
      *maxlevel = modules[module_type].maxlevel;
   }
   else {
      *vflags = 0;
      *cflags = 0;
      *xflags = 0;
      *kflags = 0;
      *maxlevel = 0;
   }
   return;
}

/*-------------------------------------------------------+
 | Called by add_alias() to add options specified on the |
 | ALIAS line in the config file.                        |
 +-------------------------------------------------------*/ 
int add_module_options ( ALIAS *aliasp, int aliasindex,
                               char **tokens, int ntokens )
{
   int type;

   type = aliasp[aliasindex].modtype;

   if ( modules[type].addopt_func == NULL ) {
      if ( ntokens > 0 ) {
         store_error_message("No parameters are supported for this module type.");
         return 1;
      }
      return 0;
   }

   return modules[type].addopt_func(aliasp, aliasindex, tokens, &ntokens);
}
        

/*-------------------------------------------------------+
 | Display options specified for an ALIAS.               |
 +-------------------------------------------------------*/ 
char *display_module_options (int aliasindex )
{
   static   char buffer[128];
   ALIAS    *aliasp;
   long int optflags, optflags2;
   int      j, k;
   int      rh;
   double   tempc, bp;
   char     keystr[4], grpstr[4];
   long     secs;

   aliasp = configp->aliasp;

   if ( !aliasp )
      return "";


   optflags  = aliasp[aliasindex].optflags;
   optflags2 = aliasp[aliasindex].optflags2;

   buffer[0] = '\0';

   if ( optflags & MOPT_RESUME )
      sprintf(buffer, "ONLEVEL RESUME");
   else if ( optflags & MOPT_ONFULL && aliasp[aliasindex].flags & PRESET)
      sprintf(buffer, "ONLEVEL %d", aliasp[aliasindex].onlevel + 1);
   else if ( optflags & MOPT_ONFULL )
      sprintf(buffer, "ONLEVEL %d", aliasp[aliasindex].onlevel);

   if ( optflags & MOPT_SECURITY || optflags & MOPT_ENTERTAIN ) {
      buffer[0] = '\0';
      for ( j = 0; j < aliasp[aliasindex].nident; j++ )
         sprintf(buffer + strlen(buffer), "0x%02lx ", aliasp[aliasindex].ident[j]);
   }

   if ( optflags & MOPT_MAIN ) strcat(buffer, "MAIN ");
   if ( optflags & MOPT_AUX )  strcat(buffer, "AUX ");

   if ( optflags & MOPT_TRANSCEIVE ) strcat(buffer, "TRANSCEIVE ");
   if ( optflags & MOPT_RFFORWARD )  strcat(buffer, "RFFORWARD ");
   if ( optflags & MOPT_RFIGNORE )   strcat(buffer, "RFIGNORE ");
   if ( optflags & MOPT_REVERSE )    strcat(buffer, "REVERSE ");

   if ( optflags2 & MOPT2_DUMMY )    strcat(buffer, "DUMMY ");

   if ( optflags & MOPT_RFXSENSOR ) {
      sprintf(buffer + strlen(buffer), "0x%02lx ", aliasp[aliasindex].ident[0]);
      strcat(buffer, "T");
      if ( optflags & MOPT_RFXRH )       strcat(buffer, "H ");
      else if ( optflags & MOPT_RFXBP )  strcat(buffer, "B ");
      else if ( optflags & MOPT_RFXVAD ) strcat(buffer, "V ");
      else if ( optflags & MOPT_RFXPOT ) strcat(buffer, "P ");
      else if ( optflags & MOPT_RFXT2 )  strcat(buffer, "T ");
      else strcat(buffer, " ");
   }

   if ( optflags & MOPT_RFXMETER ) {
      sprintf(buffer + strlen(buffer), "0x%02lx ", aliasp[aliasindex].ident[0]);
   }

   if ( optflags & MOPT_KAKU ) {
      for ( j = 0; j < aliasp[aliasindex].nident; j++ ) {
         *keystr = '\0';
         for ( k = 0; k < 16; k++ ) {
            if ( aliasp[aliasindex].kaku_keymap[j] & (1 << k) ) {
               sprintf(keystr, "%d", k + 1);
               break;
            }
         }
         *grpstr = '\0';
         for ( k = 0; k < 16; k++ ) {
            if ( aliasp[aliasindex].kaku_grpmap[j] & (1 << k) ) {
               sprintf(grpstr, "%c", k + 'A');
               break;
            }
         }

         sprintf(buffer + strlen(buffer), "0x%07lx %s%s ",
           aliasp[aliasindex].ident[j], keystr, grpstr);
      }
   }

   if ( optflags2 & MOPT2_TMIN ) {
      tempc = (double)aliasp[aliasindex].tmin / 10.0;
      sprintf(buffer + strlen(buffer), "TMIN "FMT_ORET"%c ",
        celsius2temp(tempc, configp->ore_tscale, 0.0), configp->ore_tscale );
   }
   if ( optflags2 & MOPT2_TMAX ) {
      tempc = (double)aliasp[aliasindex].tmax / 10.0;
      sprintf(buffer + strlen(buffer), "TMAX "FMT_ORET"%c ",
        celsius2temp(tempc, configp->ore_tscale, 0.0), configp->ore_tscale );
   }
   if ( optflags2 & MOPT2_RHMIN ) {
      rh = aliasp[aliasindex].rhmin;
      sprintf(buffer + strlen(buffer), "RHMIN %d%% ", rh);
   }
   if ( optflags2 & MOPT2_RHMAX ) {
      rh = aliasp[aliasindex].rhmax;
      sprintf(buffer + strlen(buffer), "RHMAX %d%% ", rh);
   }
   if ( optflags2 & MOPT2_BPMIN ) {
      bp = (double)aliasp[aliasindex].bpmin;
      sprintf(buffer + strlen(buffer), "BPMIN "FMT_OREBP"%s ",
        (bp * configp->ore_bpscale) + configp->ore_bpoffset, configp->ore_bpunits );
   }
   if ( optflags2 & MOPT2_BPMAX ) {
      bp = (double)aliasp[aliasindex].bpmax;
      sprintf(buffer + strlen(buffer), "BPMAX "FMT_OREBP"%s ",
        (bp * configp->ore_bpscale) + configp->ore_bpoffset, configp->ore_bpunits );
   }

   if ( optflags2 & MOPT2_SWHOME ) {
      strcat(buffer + strlen(buffer), "SWHOME ");
   }
   if ( optflags2 & MOPT2_SWMAX ) {
      strcat(buffer + strlen(buffer), "SWMAX ");
   }
   if ( optflags2 & MOPT2_DUMMY ) {
      strcat(buffer + strlen(buffer), "DUMMY ");
   }

   /* Inactive timeout */
   if ( optflags2 & MOPT2_IATO ) {
      secs = aliasp[aliasindex].hb_timeout;
      sprintf(buffer + strlen(buffer), "IATO %ld:%02ld:%02ld ", (secs / 3600L), (secs % 3600L) / 60L, (secs % 60));
   }

   /* ACT module options */
   if ( optflags2 & MOPT2_AUF ) {
      sprintf(buffer + strlen(buffer), "AUF ");
   }
   if ( optflags2 & MOPT2_ALO ) {
      sprintf(buffer + strlen(buffer), "ALO ");
   }
   if ( optflags2 & MOPT2_ALF ) {
      sprintf(buffer + strlen(buffer), "ALF ");
   }

   /* Defer update for 2-way modules with auto status response */
   if ( optflags2 & MOPT2_DEFER ) {
      sprintf(buffer + strlen(buffer), "DEFER ");
   }

   return buffer;
} 
   

/*-------------------------------------------------------+
 | Return a pointer to the module name corresponding to  |
 | the argument module_type.                             | 
 +-------------------------------------------------------*/ 
char *lookup_module_name ( int module_type ) 
{

   if ( module_type >= 0 && module_type < ntypes )
      return modules[module_type].label;

   return "";
}

/*-------------------------------------------------------+
 | Create the state filter determined by characteristics |
 | of each module defined in the config file.            |
 +-------------------------------------------------------*/ 
void set_module_masks ( ALIAS *aliasp )
{
   int j, ucode;
   unsigned char hcode, maxlevel, onlevel;
   unsigned int  bitmap, vflags, cflags, xflags, kflags;
   unsigned int  defined[16];

   if ( configp->module_types == NO ) {
      for ( j = 0; j < NumModMasks; j++ ) {
         for ( hcode = 0; hcode < 16; hcode++ )
            modmask[j][hcode] = 0xffff;
      }
      return;
   }

   /* Default for undefined modules */
   vflags = modules[configp->default_module].vflags;
   cflags = modules[configp->default_module].cflags;
   if ( vflags & VKAKU ) {
      kflags = cflags;
      cflags = 0;
   }
   else {
      kflags = 0;
   }
   xflags = modules[configp->default_module].xflags;
   maxlevel = modules[configp->default_module].maxlevel;

   /* Record housecode|units with defined module type */
   for ( j = 0; j < 16; j++ ) {
      defined[j] = 0;
   }
   j = 0;
   while ( aliasp && aliasp[j].line_no > 0 ) {
      if ( aliasp[j].modtype >= 0 ) {
         /* Module is defined */
         hcode = hc2code(aliasp[j].housecode);
         defined[hcode] |= aliasp[j].unitbmap;
      }
      j++;
   }

   /* Initialize to zero */
   for ( j = 0; j < NumModMasks; j++ ) {
      for ( hcode = 0; hcode < 16; hcode++ )
         modmask[j][hcode] = 0;
   }
   for ( j = 0; j < NumVmodMasks; j++ ) {
      for ( hcode = 0; hcode < 16; hcode++ )
         vmodmask[j][hcode] = 0;
   }
   for ( j = 0; j < NumKmodMasks; j++ ) {
      for ( hcode = 0; hcode < 16; hcode++ )
         kmodmask[j][hcode] = 0;
   }

   /* Set the characteristic of undefined modules */
   for ( hcode = 0; hcode < 16; hcode++ ) {
      bitmap = ~defined[hcode];

      /* Mark units which respond */
      if ( cflags & ADDRESSED )
         modmask[AddrMask][hcode] = bitmap;
      if ( cflags & PHYS )
         modmask[PhysMask][hcode] = bitmap;   
      if ( cflags & UNOFF )
         modmask[AllOffMask][hcode] = bitmap;
      if ( cflags & LION )
         modmask[LightsOnMask][hcode] = bitmap;
      if ( cflags & MON )
         modmask[OnMask][hcode] = bitmap;
      if ( cflags & MOFF )
         modmask[OffMask][hcode] = bitmap;
      if ( cflags & DIM )
         modmask[DimMask][hcode] = bitmap;
      if ( cflags & BRI )
         modmask[BriMask][hcode] = bitmap;
      if ( cflags & LIOFF )
         modmask[LightsOffMask][hcode] = bitmap;
      if ( cflags & BRIB4DIM )
         modmask[BriDimMask][hcode] = bitmap;
      if ( cflags & STDX10 )
         modmask[StdMask][hcode] = bitmap;
      if ( cflags & PRESET )
         modmask[PresetMask][hcode] = bitmap;
      if ( cflags & STAREQ )
         modmask[StatusMask][hcode] = bitmap;
      if ( cflags & STAON )
         modmask[StatusOnMask][hcode] = bitmap;
      if ( cflags & STAOFF )
         modmask[StatusOffMask][hcode] = bitmap;
      if ( cflags & LIONFULL )
         modmask[LightsOnFullMask][hcode] = bitmap;
      if ( cflags & ONFULL )
         modmask[OnFullMask][hcode] = bitmap;
      if ( cflags & ONFULLOFF )
         modmask[OnFullOffMask][hcode] = bitmap;
      if ( cflags & ALLON )
         modmask[AllOnMask][hcode] = bitmap;
      if ( cflags & RESUME )
         modmask[ResumeMask][hcode] = bitmap;
      if ( cflags & TARG )
         modmask[TargMask][hcode] = bitmap;
      if ( cflags & EXC16 )
         modmask[Exc16Mask][hcode] = bitmap;
      if ( cflags & EXC8 )
         modmask[Exc8Mask][hcode] = bitmap;
      if ( cflags & EXC4 )
         modmask[Exc4Mask][hcode] = bitmap;
      if ( cflags & VDATA || vflags & VKAKU ) 
         modmask[VdataMask][hcode] = bitmap;
      if ( cflags & RESUMEDIM )
         modmask[ResumeDimMask][hcode] = bitmap;
      if ( cflags & LIONUNAD )
         modmask[LightsOnUnaddrMask][hcode] = bitmap;
      if ( cflags & LIOFFUNAD )
         modmask[LightsOffUnaddrMask][hcode] = bitmap;
      if ( cflags & PLCSENSOR )
         modmask[PlcSensorMask][hcode] = bitmap;
      if ( cflags & ONOFFUNAD )
         modmask[OnOffUnaddrMask][hcode] = bitmap;

      if ( xflags & X0SH ) 
         modmask[Ext0Mask][hcode] = bitmap;

      if ( xflags & X3SW ) {
         modmask[Ext3Mask][hcode] = bitmap;
         modmask[AllOnMask][hcode] = bitmap;
      }
      if ( xflags & X3DIM ) 
         modmask[Ext3DimMask][hcode] = bitmap;
      if ( xflags & X3GEXEC ) 
         modmask[Ext3GrpExecMask][hcode] = bitmap;
      if ( xflags & X3GRC1 ) 
         modmask[Ext3GrpRelT1Mask][hcode] = bitmap;
      if ( xflags & X3GRC2 ) 
         modmask[Ext3GrpRelT2Mask][hcode] = bitmap;
      if ( xflags & X3GRC3 ) 
         modmask[Ext3GrpRelT3Mask][hcode] = bitmap;
      if ( xflags & X3GRC4 ) 
         modmask[Ext3GrpRelT4Mask][hcode] = bitmap;
      if ( xflags & X3GOFF ) 
         modmask[Ext3GrpOffMask][hcode] = bitmap;
      if ( xflags & X3GOFFEX ) 
         modmask[Ext3GrpOffExecMask][hcode] = bitmap;
      if ( xflags & X3GBD ) 
         modmask[Ext3GrpBriDimMask][hcode] = bitmap;
      if ( xflags & X3GBDFULL ) 
         modmask[Ext3GrpBriDimFullMask][hcode] = bitmap;
      if ( xflags & X3STAT ) 
         modmask[Ext3StatusMask][hcode] = bitmap;
      if ( xflags & X3GREM )
         modmask[Ext3GrpRemMask][hcode] = bitmap;

      if ( vflags & VSTD )
         vmodmask[VstdMask][hcode] = bitmap;
      if ( vflags & VENT )
         vmodmask[VentMask][hcode] = bitmap;
      if ( vflags & VSEC )
         vmodmask[VsecMask][hcode] = bitmap;
      if ( vflags & VRFXS )
         vmodmask[VrfxsMask][hcode] = bitmap;
      if ( vflags & VRFXM )
         vmodmask[VrfxmMask][hcode] = bitmap;
      if ( vflags & VDMX )
         vmodmask[VdmxMask][hcode] = bitmap;
      if ( vflags & VORE )
         vmodmask[VoreMask][hcode] = bitmap;
      if ( vflags & VKAKU )
         vmodmask[VkakuMask][hcode] = bitmap;

      if ( kflags & KON )
         kmodmask[KonMask][hcode] = bitmap;
      if ( kflags & KOFF )
         kmodmask[KoffMask][hcode] = bitmap;
      if ( kflags & KGON )
         kmodmask[KGonMask][hcode] = bitmap;
      if ( kflags & KGOFF )
         kmodmask[KGoffMask][hcode] = bitmap;
      if ( kflags & KPRE )
         kmodmask[KpreMask][hcode] = bitmap;
      if ( kflags & KGPRE )
         kmodmask[KGpreMask][hcode] = bitmap;
      if ( kflags & KRESUME )
         kmodmask[KresumeMask][hcode] = bitmap;
      if ( kflags & KPHYS )
         kmodmask[KPhysMask][hcode] = bitmap;

      if ( !vflags || (vflags & VSTD) )
         vmodmask[VtstampMask][hcode] = bitmap;

      /* Set max and on dim levels for each unit */
      for ( ucode = 0; ucode < 16; ucode++ ) {
         if ( bitmap & (1 << ucode) ) {
            maxdimlevel[hcode][ucode] = maxlevel;
            ondimlevel[hcode][ucode] = maxlevel;
         }
      }
   }
   
   /* Now fill in the characteristics of defined modules */
   j = 0;
   while ( aliasp && aliasp[j].line_no > 0 ) {
      if ( aliasp[j].modtype < 0 ) {
         /* Module is undefined */
         j++;
         continue;
      }
      hcode  = hc2code(aliasp[j].housecode);
      bitmap = aliasp[j].unitbmap;
      cflags = aliasp[j].flags;
      vflags = aliasp[j].vflags;
      xflags = aliasp[j].xflags;
      kflags = aliasp[j].kflags;
      maxlevel = aliasp[j].maxlevel;
      onlevel = aliasp[j].onlevel;

      /* Mark units which respond */
      if ( cflags & ADDRESSED )
         modmask[AddrMask][hcode] |= bitmap;
      if ( cflags & PHYS )
         modmask[PhysMask][hcode] |= bitmap;   
      if ( cflags & UNOFF )
         modmask[AllOffMask][hcode] |= bitmap;
      if ( cflags & LION )
         modmask[LightsOnMask][hcode] |= bitmap;
      if ( cflags & MON )
         modmask[OnMask][hcode] |= bitmap;
      if ( cflags & MOFF )
         modmask[OffMask][hcode] |= bitmap;
      if ( cflags & DIM )
         modmask[DimMask][hcode] |= bitmap;
      if ( cflags & BRI )
         modmask[BriMask][hcode] |= bitmap;
      if ( cflags & LIOFF )
         modmask[LightsOffMask][hcode] |= bitmap;
      if ( cflags & BRIB4DIM )
         modmask[BriDimMask][hcode] |= bitmap;
      if ( cflags & STDX10 )
         modmask[StdMask][hcode] |= bitmap;
      if ( cflags & PRESET )
         modmask[PresetMask][hcode] |= bitmap;
      if ( cflags & STAREQ )
         modmask[StatusMask][hcode] |= bitmap;
      if ( cflags & STAON )
         modmask[StatusOnMask][hcode] |= bitmap;
      if ( cflags & STAOFF )
         modmask[StatusOffMask][hcode] |= bitmap;
      if ( cflags & LIONFULL )
         modmask[LightsOnFullMask][hcode] |= bitmap;
      if ( cflags & ONFULL )
         modmask[OnFullMask][hcode] |= bitmap;
      if ( cflags & ONFULLOFF )
         modmask[OnFullOffMask][hcode] |= bitmap;
      if ( cflags & ALLON )
         modmask[AllOnMask][hcode] |= bitmap;
      if ( cflags & RESUME )
         modmask[ResumeMask][hcode] |= bitmap;
      if ( cflags & TARG )
         modmask[TargMask][hcode] |= bitmap;
      if ( cflags & EXC16 )
         modmask[Exc16Mask][hcode] |= bitmap;
      if ( cflags & EXC8 )
         modmask[Exc8Mask][hcode] |= bitmap;
      if ( cflags & EXC4 )
         modmask[Exc4Mask][hcode] |= bitmap;
      if ( cflags & VDATA || vflags & VKAKU )
         modmask[VdataMask][hcode] |= bitmap;
      if ( cflags & RESUMEDIM )
         modmask[ResumeDimMask][hcode] |= bitmap;
      if ( cflags & LIONUNAD )
         modmask[LightsOnUnaddrMask][hcode] |= bitmap;
      if ( cflags & LIOFFUNAD )
         modmask[LightsOffUnaddrMask][hcode] |= bitmap;
      if ( cflags & PLCSENSOR )
         modmask[PlcSensorMask][hcode] |= bitmap;
      if ( cflags & ONOFFUNAD )
         modmask[OnOffUnaddrMask][hcode] |= bitmap;

      if ( xflags & X0SH ) 
         modmask[Ext0Mask][hcode] |= bitmap;

      if ( xflags & X3SW ) {
         modmask[Ext3Mask][hcode] |= bitmap;
         modmask[AllOnMask][hcode] |= bitmap;
      }
      if ( xflags & X3DIM ) 
         modmask[Ext3DimMask][hcode] |= bitmap;
      if ( xflags & X3GEXEC ) 
         modmask[Ext3GrpExecMask][hcode] |= bitmap;
      if ( xflags & X3GRC1 ) 
         modmask[Ext3GrpRelT1Mask][hcode] |= bitmap;
      if ( xflags & X3GRC2 ) 
         modmask[Ext3GrpRelT2Mask][hcode] |= bitmap;
      if ( xflags & X3GRC3 ) 
         modmask[Ext3GrpRelT3Mask][hcode] |= bitmap;
      if ( xflags & X3GRC4 ) 
         modmask[Ext3GrpRelT4Mask][hcode] |= bitmap;
      if ( xflags & X3GOFF ) 
         modmask[Ext3GrpOffMask][hcode] |= bitmap;
      if ( xflags & X3GOFFEX ) 
         modmask[Ext3GrpOffExecMask][hcode] |= bitmap;
      if ( xflags & X3GBD ) 
         modmask[Ext3GrpBriDimMask][hcode] |= bitmap;
      if ( xflags & X3GBDFULL ) 
         modmask[Ext3GrpBriDimFullMask][hcode] |= bitmap;
      if ( xflags & X3STAT ) 
         modmask[Ext3StatusMask][hcode] |= bitmap;
      if ( xflags & X3GREM )
         modmask[Ext3GrpRemMask][hcode] |= bitmap;

      if ( kflags & KON )
         kmodmask[KonMask][hcode] |= bitmap;
      if ( kflags & KOFF )
         kmodmask[KoffMask][hcode] |= bitmap;
      if ( kflags & KGON )
         kmodmask[KGonMask][hcode] |= bitmap;
      if ( kflags & KGOFF )
         kmodmask[KGoffMask][hcode] |= bitmap;
      if ( kflags & KPRE )
         kmodmask[KpreMask][hcode] |= bitmap;
      if ( kflags & KGPRE )
         kmodmask[KGpreMask][hcode] |= bitmap;
      if ( kflags & KRESUME )
         kmodmask[KresumeMask][hcode] |= bitmap;
      if ( kflags & KPHYS )
         kmodmask[KPhysMask][hcode] |= bitmap;

      if ( vflags & VSTD )
         vmodmask[VstdMask][hcode] |= bitmap;
      if ( vflags & VENT )
         vmodmask[VentMask][hcode] |= bitmap;
      if ( vflags & VSEC )
         vmodmask[VsecMask][hcode] |= bitmap;
      if ( vflags & VRFXS )
         vmodmask[VrfxsMask][hcode] |= bitmap;
      if ( vflags & VRFXM )
         vmodmask[VrfxmMask][hcode] |= bitmap;
      if ( vflags & VDMX )
         vmodmask[VdmxMask][hcode] |= bitmap;
      if ( vflags & VORE )
         vmodmask[VoreMask][hcode] |= bitmap;
      if ( vflags & VKAKU )
         vmodmask[VkakuMask][hcode] |= bitmap;
      if ( !vflags || (vflags & VSTD) )
         vmodmask[VtstampMask][hcode] |= bitmap;

      /* ACT module options */
      if ( aliasp[j].optflags2 & MOPT2_AUF )
         modmask[AllOffMask][hcode] |= bitmap;
      if ( aliasp[j].optflags2 & MOPT2_ALO ) {
         modmask[LightsOnMask][hcode] |= bitmap;
         modmask[OnFullMask][hcode] |= bitmap;
      }
      if ( aliasp[j].optflags2 & MOPT2_ALF )
         modmask[LightsOffMask][hcode] |= bitmap;

      if ( aliasp[j].optflags2 & MOPT2_DEFER )
         modmask[DeferMask][hcode] |= bitmap;


      /* Set max and on dim levels for each unit */
      for ( ucode = 0; ucode < 16; ucode++ ) {
         if ( bitmap & (1 << ucode) ) {
            maxdimlevel[hcode][ucode] = maxlevel;
            ondimlevel[hcode][ucode] = onlevel;
         }
      }

      j++;
   }

   return;
}


/*-------------------------------------------------------+
 | Display a table indicating the module attributes      |
 | (either defined or defaults) for each unit            |
 +-------------------------------------------------------*/ 
void show_module_mask ( unsigned char hcode )
{
   char hc, *chr = ".*x", *chr2 = ".*?";
   int  j, lw = 13;
   unsigned int exclusive, heartbeat = 0, lobat = 0, lobatunkn = 0;
   ALIAS *aliasp;

   aliasp = configp->aliasp;

   hc = code2hc(hcode);

   j = 0;
   while ( aliasp && aliasp[j].line_no > 0 ) {
     if ( aliasp[j].housecode == hc && (aliasp[j].optflags & MOPT_HEARTBEAT) ) {
        heartbeat |= aliasp[j].unitbmap;
     }
     if ( aliasp[j].housecode == hc && (aliasp[j].optflags & MOPT_LOBAT) ) {
        lobat |= aliasp[j].unitbmap;
     }
     if ( aliasp[j].housecode == hc && (aliasp[j].optflags & MOPT_LOBATUNKN) ) {
        lobatunkn |= aliasp[j].unitbmap;
     }
     j++;
   }
        

   printf("Module Attributes\n");
   printf("%*s %c\n", lw + 13, "Housecode", hc);
   printf("%*s  1..4...8.......16\n", lw, "Unit:");
   printf("%*s (%s)  %s\n", lw, "On", bmap2asc(modmask[OnMask][hcode], chr),
      "Supports On command");
   printf("%*s (%s)  %s\n", lw, "Off", bmap2asc(modmask[OffMask][hcode], chr),
      "Supports Off command");
   printf("%*s (%s)  %s\n", lw, "Dim", bmap2asc(modmask[DimMask][hcode], chr),
      "Supports Dim (1-22) commands");
   printf("%*s (%s)  %s\n", lw, "Bright", bmap2asc(modmask[BriMask][hcode], chr),
      "Supports Bright (1-22) commands");
   printf("%*s (%s)  %s\n", lw, "BrightB4Dim", bmap2asc(modmask[BriDimMask][hcode], chr),
      "Brightens to 100% before Dim/Bright if Off");
   printf("%*s (%s)  %s\n", lw, "ResumeB4Dim", bmap2asc(modmask[ResumeDimMask][hcode], chr),
      "Resumes prev. level before Dim/Bright if Off");
   printf("%*s (%s)  %s\n", lw, "LightsOn", bmap2asc(modmask[LightsOnMask][hcode], chr),
      "Supports LightsOn (AllLightsOn) command");
   printf("%*s (%s)  %s\n", lw, "LightsOnFull", bmap2asc(modmask[LightsOnFullMask][hcode], chr),
      "LightsOn command always brightens to 100%");
   printf("%*s (%s)  %s\n", lw, "OnFullIfOff", bmap2asc(modmask[OnFullOffMask][hcode], chr),
      "LightsOn brightens to 100% only if Off");
   exclusive = modmask[Exc16Mask][hcode] | modmask[Exc8Mask][hcode] | modmask[Exc4Mask][hcode];
   printf("%*s (%s)  %s\n", lw, "ExclusiveOn", bmap2asc(exclusive, chr),
      "Last unit turns On, other units in group Off");
   printf("%*s (%s)  %s\n", lw, "LightsOff", bmap2asc(modmask[LightsOffMask][hcode], chr),
      "Supports LightsOff command");
   printf("%*s (%s)  %s\n", lw, "LiOnUnAddr", bmap2asc(modmask[LightsOnUnaddrMask][hcode], chr),
      "Unaddressed by LightsOn command");
   printf("%*s (%s)  %s\n", lw, "LiOffUnAddr", bmap2asc(modmask[LightsOffUnaddrMask][hcode], chr),
      "Unaddressed by LightsOff command");
   
   printf("%*s (%s)  %s\n", lw, "OnOffUnAddr", bmap2asc(modmask[OnOffUnaddrMask][hcode], chr),
      "Unaddressed by On or Off command");

   printf("%*s (%s)  %s\n", lw, "AllOff", bmap2asc(modmask[AllOffMask][hcode], chr),
      "Supports AllOff (AllUnitsOff) command");
   printf("%*s (%s)  %s\n", lw, "StatusReq", bmap2asc(modmask[StatusMask][hcode], chr),
      "Supports StatusReq command");
   printf("%*s (%s)  %s\n", lw, "StatusOn", bmap2asc(modmask[StatusOnMask][hcode], chr),
      "Sends StatusOn in response to StatusReq");
   printf("%*s (%s)  %s\n", lw, "StatusOff", bmap2asc(modmask[StatusOffMask][hcode], chr),
      "Sends StatusOff in response to StatusReq");
   printf("%*s (%s)  %s\n", lw, "StatusDefer", bmap2asc(modmask[DeferMask][hcode], chr),
      "Defer update until StatusOn/Off response");
   printf("%*s (%s)  %s\n", lw, "Preset", bmap2asc(modmask[PresetMask][hcode], chr),
      "Supports Preset (1-32) commands");
   printf("%*s (%s)  %s\n", lw, "LevelFixed", bmap2asc(modmask[OnFullMask][hcode], chr),
      "On command sets to fixed brightness level");
   printf("%*s (%s)  %s\n", lw, "LevelResume", bmap2asc(modmask[ResumeMask][hcode], chr),
      "On command resumes previous brightness level");
   printf("%*s (%s)  %s\n", lw, "ExtSwitch", bmap2asc(modmask[Ext3Mask][hcode], chr),
      "Supports Extended code switch commands");
   printf("%*s (%s)  %s\n", lw, "ExtDimmer", bmap2asc(modmask[Ext3DimMask][hcode], chr),
      "Supports Extended code dimmer (0-63) commands");
   printf("%*s (%s)  %s\n", lw, "ExtStatus", bmap2asc(modmask[Ext3StatusMask][hcode], chr),
      "Supports Extended code StatusReq commands");
   printf("%*s (%s)  %s\n", lw, "ExtGrpExec", bmap2asc(modmask[Ext3GrpExecMask][hcode], chr),
      "Supports Extended code Group Execute");
   printf("%*s (%s)  %s\n", lw, "ExtGrpOff",
       bmap2asc2(modmask[Ext3GrpOffMask][hcode], modmask[Ext3GrpOffExecMask][hcode], chr),
      "Supports Extended code Group Off (x = bug)");
   printf("%*s (%s)  %s\n", lw, "ExtGrpBrDim", bmap2asc(modmask[Ext3GrpBriDimMask][hcode], chr),
      "Supports Extended code Group Bright/Dim");
   printf("%*s (%s)  %s\n", lw, "ExtGrpRem", bmap2asc(modmask[Ext3GrpRemMask][hcode], chr),
      "Supports Extended code Group Remove");
   printf("%*s (%s)  %s\n", lw, "ExtShutter", bmap2asc(modmask[Ext0Mask][hcode], chr),
      "Supports Extended code shutter (0-25) commands");
   printf("%*s (%s)  %s\n", lw, "PhysMod", bmap2asc((modmask[PhysMask][hcode] | kmodmask[KPhysMask][hcode]), chr),
      "Physical receiver module");
   printf("%*s (%s)  %s\n", lw, "Virtual", bmap2asc(modmask[VdataMask][hcode], chr),
      "Virtual Data module");
   printf("%*s (%s)  %s\n", lw, "PLCSensor", bmap2asc(modmask[PlcSensorMask][hcode], chr),
      "Sensor transmits X10 power line signals");
   printf("%*s (%s)  %s\n", lw, "Security", bmap2asc(vmodmask[VsecMask][hcode], chr),
      "X10 Security RF data");
#ifdef HASRFXS
   printf("%*s (%s)  %s\n", lw, "RFXSensor", bmap2asc(vmodmask[VrfxsMask][hcode], chr),
      "RFXSensor RF data");
#endif
#ifdef HASRFXM
   printf("%*s (%s)  %s\n", lw, "RFXMeter", bmap2asc(vmodmask[VrfxmMask][hcode], chr),
      "RFXMeter RF data");
#endif
#ifdef HASDMX
   printf("%*s (%s)  %s\n", lw, "Digimax", bmap2asc(vmodmask[VdmxMask][hcode], chr),
      "Digimax RF data");
#endif
#ifdef HASORE
   printf("%*s (%s)  %s\n", lw, "Oregon", bmap2asc(vmodmask[VoreMask][hcode], chr),
      "Oregon RF data");
#endif
   printf("%*s (%s)  %s\n", lw, "KaKu/HE", bmap2asc(vmodmask[VkakuMask][hcode], chr),
      "Supports KaKu/HomeEasy RF commands");

   printf("%*s (%s)  %s\n", lw, "Heartbeat", bmap2asc(heartbeat, chr),
      "Sensor transmits periodic heartbeat signal");
   printf("%*s (%s)  %s\n", lw, "LoBat",
       bmap2asc2(lobat, lobatunkn, chr2),
      "Sensor transmits low battery indication");

   printf("\n");

   return;
}

/*---------------------------------------------------------------------+
 | Count the RF options (There should be no more than one)             |
 +---------------------------------------------------------------------*/
int count_rf_options ( ALIAS *aliasp, int aliasindex )
{
   int count = 0;

   count += (aliasp[aliasindex].optflags & MOPT_TRANSCEIVE) ? 1 : 0;
   count += (aliasp[aliasindex].optflags & MOPT_RFFORWARD)  ? 1 : 0;
   count += (aliasp[aliasindex].optflags & MOPT_RFIGNORE)   ? 1 : 0;

   return count;
}
   
/*---------------------------------------------------------------------+
 | Set DUMMY option for security remotes (keyfob)                      |
 +---------------------------------------------------------------------*/
int remote_options ( ALIAS *aliasp, int aliasindex, char **tokens, int *ntokens)
{
   int  j, k, count = 0;
   char *tmptok;
   unsigned long flags = 0;

   for ( j = 0; j < *ntokens; j++ ) {
      strupper(tokens[j]);
      if ( strcmp(tokens[j], "DUMMY") == 0 ) {
         aliasp[aliasindex].optflags2 |= MOPT2_DUMMY;
         *tokens[j] = '\0';
         count++;
      }
      else if ( strcmp(tokens[j], "SWMAX") == 0 ) {
         flags |= SEC_MAX;
         aliasp[aliasindex].optflags2 |= MOPT2_SWMAX;
         *tokens[j] = '\0';
         count++;
      }
      else if ( strcmp(tokens[j], "SWHOME") == 0 ) {
         flags |= SEC_HOME;
         aliasp[aliasindex].optflags2 |= MOPT2_SWHOME;
         *tokens[j] = '\0';
         count++;
      }
      else if ( strcmp(tokens[j], "SWMIN") == 0 ) {
         flags |= SEC_MIN;
         aliasp[aliasindex].optflags2 &= ~MOPT2_SWMAX;
         *tokens[j] = '\0';
         count++;
      }
      else if ( strcmp(tokens[j], "SWAWAY") == 0 ) {
         flags |= SEC_AWAY;
         aliasp[aliasindex].optflags2 &= ~MOPT2_SWHOME;
         *tokens[j] = '\0';
         count++;
      }
   }

   if ( flags && (aliasp[aliasindex].optflags2 & MOPT2_DUMMY) ) {
      store_error_message("Parameter DUMMY incompatible with SWxxx parameters.");
      return 1;
   }
   if ( (flags & SEC_HOME) && (flags & SEC_AWAY) ) {
      store_error_message("Conflicting parameters SWHOME, SWAWAY.");
      return 1;
   }
   if ( (flags & SEC_MAX) && (flags & SEC_MIN) ) {
      store_error_message("Conflicting parameters SWMAX, SWMIN.");
      return 1;
   }


   if ( count == 0 )
      return 0;

   /* Compact to remove "gaps" where tokens were nulled out */

   for ( j = 0; j < *ntokens; j++ ) {
      if ( *tokens[j] == '\0' ) {
         for ( k = j + 1; k < *ntokens; k++ ) {
            if ( *tokens[k] != '\0' ) {
               tmptok = tokens[j];
               tokens[j] = tokens[k];
               tokens[k] = tmptok;
               *tmptok = '\0';
               break;
            }
         }
      }
   }

   *ntokens -= count;

   return 0;
}

/*---------------------------------------------------------------------+
 | Set REVERSE, MAIN, and AUX module options                           |
 +---------------------------------------------------------------------*/
int sensor_options ( ALIAS *aliasp, int aliasindex, char **tokens, int *ntokens)
{
   int  j, k, count = 0;
   char *tmptok;

   for ( j = 0; j < *ntokens; j++ ) {
      strupper(tokens[j]);
      if ( strcmp(tokens[j], "REVERSE") == 0 ) {
         aliasp[aliasindex].optflags |= MOPT_REVERSE;
         *tokens[j] = '\0';
         count++;
      }
      else if ( strcmp(tokens[j], "RFIGNORE") == 0 ) {
         aliasp[aliasindex].optflags |= MOPT_RFIGNORE;
         *tokens[j] = '\0';
         count++;
      }
      else if ( strcmp(tokens[j], "MAIN") == 0 ) {
         aliasp[aliasindex].optflags |= MOPT_MAIN;
         *tokens[j] = '\0';
         count++;
      }
      else if ( strcmp(tokens[j], "AUX") == 0 ) {
         aliasp[aliasindex].optflags |= MOPT_AUX;
         *tokens[j] = '\0';
         count++;
      }
   }

   /* Compact to remove "gaps" where tokens were nulled out */

   for ( j = 0; j < *ntokens; j++ ) {
      if ( *tokens[j] == '\0' ) {
         for ( k = j + 1; k < *ntokens; k++ ) {
            if ( *tokens[k] != '\0' ) {
               tmptok = tokens[j];
               tokens[j] = tokens[k];
               tokens[k] = tmptok;
               *tmptok = '\0';
               break;
            }
         }
      }
   }

   *ntokens -= count;

   return 0;
}

/*---------------------------------------------------------------------+
 | Options for PLC Sensor                                              |
 +---------------------------------------------------------------------*/
int opt_plcsensor ( ALIAS *aliasp, int aliasindex, char **tokens, int *ntokens)
{

   if ( sensor_timeout(aliasp, aliasindex, tokens, ntokens) != 0 )
      return 1;

   aliasp[aliasindex].vtype = 0;
   aliasp[aliasindex].optflags |= (MOPT_PLCSENSOR | MOPT_HEARTBEAT);
   return 0;
}

/*---------------------------------------------------------------------+
 | Options for second sensor in RFXSensor modules                      |
 +---------------------------------------------------------------------*/
int opt_rfxsensor ( ALIAS *aliasp, int aliasindex, char **tokens, int *ntokens)
{
   long ident;
   unsigned char rfxid /*, delta */;
   char *sp;

   if ( sensor_timeout(aliasp, aliasindex, tokens, ntokens) != 0 )
      return 1;

   if ( *ntokens < 2 ) {
      store_error_message("This module requires more parameters.");
      return 1;
   }

   if ( single_bmap_unit(aliasp[aliasindex].unitbmap) == 0xff ) {
      store_error_message("Multiple unit alias is invalid for sensors.");
      return 1;
   }

   ident = strtol(tokens[0], &sp, 16);
   if ( *sp == '\0' && ident >= 0 && ident <= 0xff ) {
      aliasp[aliasindex].ident[0] = (unsigned short)ident;
   }
   else {
      store_error_message("Invalid hexadecimal RFXSensor base address parameter");
      return 1;
   }

   rfxid = aliasp[aliasindex].ident[0];

   if ( (rfxid % 4) != 0 ) {
      store_error_message("RFXSensor base address must be a multiple of 0x04");
      return 1;
   }

   /* Add IDs for secondary and Supply Voltage sensors. */
   aliasp[aliasindex].ident[1] = aliasp[aliasindex].ident[0] + 1;
   aliasp[aliasindex].ident[2] = aliasp[aliasindex].ident[0] + 2;
   aliasp[aliasindex].nident = 3;

   aliasp[aliasindex].optflags |= MOPT_RFXVS;

   strupper(tokens[1]);
   if ( strcmp(tokens[1], "TH") == 0 ) {
      /* Temperature and Humidity */
      aliasp[aliasindex].optflags |= MOPT_RFXRH;
   }
   else if ( strcmp(tokens[1], "TB") == 0 ) {
      /* Temperature and Barometric Pressure */
      aliasp[aliasindex].optflags |= MOPT_RFXBP;
   }
   else if ( strcmp(tokens[1], "TV") == 0 ) {
      /* Temperature and Voltage */
      aliasp[aliasindex].optflags |= MOPT_RFXVAD;
   }
   else if ( strcmp(tokens[1], "TP") == 0 ) {
      /* Potentiometer */
      aliasp[aliasindex].optflags |= MOPT_RFXPOT;
   }
   else if ( strcmp(tokens[1], "TT") == 0 ) {
      /* Two Temperature probes */
      aliasp[aliasindex].optflags |= MOPT_RFXT2;
   }
   else if ( strcmp(tokens[1], "T") == 0 ) {
      /* Single Temperature probe */
      aliasp[aliasindex].optflags &= ~MOPT_RFXVS;
   }
   else {
      store_error_message("Invalid RFXSensor function.");
      return 1;
   }

   /* This is a sensor with periodic "heartbeat" signals */
   aliasp[aliasindex].vtype = RF_XSENSOR;
   aliasp[aliasindex].optflags |= (MOPT_RFXSENSOR | MOPT_HEARTBEAT | MOPT_LOBAT);

   return 0;
}

/*---------------------------------------------------------------------+
 | Options for RFXMeter modules                                        |
 +---------------------------------------------------------------------*/
int opt_rfxmeter ( ALIAS *aliasp, int aliasindex, char **tokens, int *ntokens)
{
   long ident;
   char *sp;

   if ( sensor_timeout(aliasp, aliasindex, tokens, ntokens) != 0 )
      return 1;

   if ( *ntokens < 1 ) {
      store_error_message("This module requires one ID parameter.");
      return 1;
   }
   else if ( *ntokens > 1 ) {
      store_error_message("This module accepts only one ID parameter.");
      return 1;
   }

   ident = strtol(tokens[0], &sp, 16);
   if ( *sp == '\0' && ident >= 0 && ident <= 0xff ) {
      aliasp[aliasindex].ident[0] = (unsigned short)ident;
   }
   else {
      store_error_message("Invalid hexadecimal RFXMeter ID parameter");
      return 1;
   }
   aliasp[aliasindex].nident = 1;

   aliasp[aliasindex].vtype = RF_XMETER;
   aliasp[aliasindex].optflags |= ((MOPT_SENSOR | MOPT_HEARTBEAT) & ~MOPT_LOBAT);

   return 0;
}

/*---------------------------------------------------------------------+
 | Options for RFXMeter Pulse modules                                  |
 +---------------------------------------------------------------------*/
int opt_rfxpulse ( ALIAS *aliasp, int aliasindex, char **tokens, int *ntokens) 
{
   if ( opt_rfxmeter(aliasp, aliasindex, tokens, ntokens) != 0 )
      return 1;

   aliasp[aliasindex].optflags |= (MOPT_RFXPULSE | MOPT_RFXCOUNT | MOPT_HEARTBEAT);
   return 0;
}

/*---------------------------------------------------------------------+
 | Options for RFXMeter Counter modules                                |
 +---------------------------------------------------------------------*/
int opt_rfxcount ( ALIAS *aliasp, int aliasindex, char **tokens, int *ntokens) 
{
   if ( opt_rfxmeter(aliasp, aliasindex, tokens, ntokens) != 0 )
      return 1;

   aliasp[aliasindex].optflags |= (MOPT_RFXCOUNT | MOPT_HEARTBEAT);
   return 0;
}

/*---------------------------------------------------------------------+
 | Options for RFXMeter Power modules                                  |
 +---------------------------------------------------------------------*/
int opt_rfxpower ( ALIAS *aliasp, int aliasindex, char **tokens, int *ntokens) 
{
   if ( opt_rfxmeter(aliasp, aliasindex, tokens, ntokens) != 0 )
      return 1;

   aliasp[aliasindex].optflags |= (MOPT_RFXPOWER | MOPT_RFXCOUNT | MOPT_HEARTBEAT);
   return 0;
}

/*---------------------------------------------------------------------+
 | Options for RFXMeter Water meter modules                            |
 +---------------------------------------------------------------------*/
int opt_rfxwater ( ALIAS *aliasp, int aliasindex, char **tokens, int *ntokens) 
{
   if ( opt_rfxmeter(aliasp, aliasindex, tokens, ntokens) != 0 )
      return 1;

   aliasp[aliasindex].optflags |= (MOPT_RFXWATER | MOPT_RFXCOUNT | MOPT_HEARTBEAT);
   return 0;
}

/*---------------------------------------------------------------------+
 | Options for RFXMeter Gas meter modules                              |
 +---------------------------------------------------------------------*/
int opt_rfxgas ( ALIAS *aliasp, int aliasindex, char **tokens, int *ntokens) 
{
   if ( opt_rfxmeter(aliasp, aliasindex, tokens, ntokens) != 0 )
      return 1;

   aliasp[aliasindex].optflags |= (MOPT_RFXGAS | MOPT_RFXCOUNT | MOPT_HEARTBEAT);
   return 0;
}

/*---------------------------------------------------------------------+
 | Options for Digimax 210                                             |
 +---------------------------------------------------------------------*/
int opt_digimax ( ALIAS *aliasp, int aliasindex, char **tokens, int *ntokens )
{
   long ident;
   char *sp;

   if ( *ntokens < 1 ) {
      store_error_message("This module requires an ID parameter");
      return 1;
   }

   if ( single_bmap_unit(aliasp[aliasindex].unitbmap) == 0xff ) {
      store_error_message("Multiple unit alias is invalid for sensors.");
      return 1;
   }

   ident = strtol(tokens[0], &sp, 16);
   if ( *sp == '\0' && ident >= 0 && ident <= 0xffff ) {
      aliasp[aliasindex].ident[0] = (unsigned short)ident;
      aliasp[aliasindex].nident = 1;
   }
   else {
      store_error_message("Invalid hexadecimal Digimax address parameter");
      return 1;
   }

   if ( sensor_timeout(aliasp, aliasindex, tokens, ntokens) != 0 )
      return 1;

   aliasp[aliasindex].vtype = RF_DIGIMAX;
   aliasp[aliasindex].storage_units = 1;

   aliasp[aliasindex].optflags = (MOPT_SECURITY | MOPT_SENSOR | MOPT_HEARTBEAT);

   return 0;
}

/*---------------------------------------------------------------------+
 | Options for ACT modules.                                            |
 +---------------------------------------------------------------------*/
int opt_act ( ALIAS *aliasp, int aliasindex, char **tokens, int *ntokens )
{
   int j;
   char message[80];

   for ( j = 0; j < *ntokens; j++ ) {
      strupper(tokens[j]);
      if ( strcmp(tokens[j], "AUF") == 0 )
         aliasp[aliasindex].optflags2 |= MOPT2_AUF;
      else if ( strcmp(tokens[j], "ALO") == 0 )
         aliasp[aliasindex].optflags2 |= MOPT2_ALO;
      else if ( strcmp(tokens[j], "ALF") == 0 )
         aliasp[aliasindex].optflags2 |= MOPT2_ALF;
      else {
         sprintf(message, "Invalid module option '%s'", tokens[j]);
         store_error_message(message);
         return 1;
      }
   }
   
   return 0;
}

/*---------------------------------------------------------------------+
 | Option for 2-way modules with automatic StatusOn/Off response       |
 +---------------------------------------------------------------------*/
int opt_defer ( ALIAS *aliasp, int aliasindex, char **tokens, int *ntokens )
{
   int j;
   char message[80];

   for ( j = 0; j < *ntokens; j++ ) {
      strupper(tokens[j]);
      if ( strcmp(tokens[j], "DEFER") == 0 )
         aliasp[aliasindex].optflags2 |= MOPT2_DEFER;
      else {
         sprintf(message, "Invalid module option '%s'", tokens[j]);
         store_error_message(message);
         return 1;
      }
   }
   
   return 0;
}


#ifdef HASORE
/*---------------------------------------------------------------------+
 | General options for Oregon sensors                                  |
 +---------------------------------------------------------------------*/
int opt_oregon ( ALIAS *aliasp, int aliasindex, char **tokens, int *ntokens )
{
   long ident;
   char *sp;

   if ( ore_maxmin_temp(aliasp, aliasindex, tokens, ntokens) != 0 )
      return 1;
   if ( ore_maxmin_rh(aliasp, aliasindex, tokens, ntokens) != 0 )
      return 1;
   if ( ore_maxmin_bp(aliasp, aliasindex, tokens, ntokens) != 0 )
      return 1;
   if ( sensor_timeout(aliasp, aliasindex, tokens, ntokens) != 0 )
      return 1;

   if ( *ntokens != 1 ) {
      store_error_message("Unknown parameters");
      return 1;
   }

   if ( single_bmap_unit(aliasp[aliasindex].unitbmap) == 0xff ) {
      store_error_message("Multiple unit alias is invalid for sensors.");
      return 1;
   }

   ident = strtol(tokens[0], &sp, 16);
   if ( *sp == '\0' && ident >= 0 && ident <= 0xfff ) {
      aliasp[aliasindex].ident[0] = (unsigned short)ident;
      aliasp[aliasindex].nident = 1;
   }
   else {
      store_error_message("Invalid hexadecimal Oregon address parameter");
      return 1;
   }

   aliasp[aliasindex].vtype = RF_OREGON;

   aliasp[aliasindex].optflags = (MOPT_SECURITY | MOPT_SENSOR | MOPT_HEARTBEAT | MOPT_LOBAT);

   return 0;
}

/*---------------------------------------------------------------------+
 | General options for Oregon sensors                                  |
 +---------------------------------------------------------------------*/
int opt_ore_dummy ( ALIAS *aliasp, int aliasindex, char **tokens, int *ntokens )
{

   if ( ore_maxmin_temp(aliasp, aliasindex, tokens, ntokens) != 0 )
      return 1;
   if ( ore_maxmin_rh(aliasp, aliasindex, tokens, ntokens) != 0 )
      return 1;
   if ( ore_maxmin_bp(aliasp, aliasindex, tokens, ntokens) != 0 )
      return 1;
   if ( sensor_timeout(aliasp, aliasindex, tokens, ntokens) != 0 )
      return 1;


   if ( *ntokens != 0 ) {
      store_error_message("Unknown parameters");
      return 1;
   }

   if ( single_bmap_unit(aliasp[aliasindex].unitbmap) == 0xff ) {
      store_error_message("Multiple unit alias is invalid for sensors.");
      return 1;
   }

   aliasp[aliasindex].nident = 0;

   aliasp[aliasindex].vtype = RF_OREGON;

   aliasp[aliasindex].optflags = (MOPT_SECURITY | MOPT_SENSOR | MOPT_HEARTBEAT);

   return 0;
}

/*---------------------------------------------------------------------+
 | Options for Oregon Temp1 sensors                                    |
 +---------------------------------------------------------------------*/
int opt_oreTemp1 ( ALIAS *aliasp, int aliasindex, char **tokens, int *ntokens )
{
   if ( opt_oregon(aliasp, aliasindex, tokens, ntokens) != 0 )
      return 1;

   aliasp[aliasindex].subtype = OreTemp1;
   aliasp[aliasindex].nvar = 1;
   aliasp[aliasindex].storage_units = 2 * aliasp[aliasindex].nvar;
   aliasp[aliasindex].funclist[0] = OreTempFunc;

   return 0;
}
   
/*---------------------------------------------------------------------+
 | Options for Oregon Temp2 sensors                                    |
 +---------------------------------------------------------------------*/
int opt_oreTemp2 ( ALIAS *aliasp, int aliasindex, char **tokens, int *ntokens )
{
   if ( opt_oregon(aliasp, aliasindex, tokens, ntokens) != 0 )
      return 1;

   aliasp[aliasindex].subtype = OreTemp2;
   aliasp[aliasindex].nvar = 1;
   aliasp[aliasindex].storage_units = 2 * aliasp[aliasindex].nvar;
   aliasp[aliasindex].funclist[0] = OreTempFunc;

   return 0;
}
   
/*---------------------------------------------------------------------+
 | Options for Oregon Temp3 sensors                                    |
 +---------------------------------------------------------------------*/
int opt_oreTemp3 ( ALIAS *aliasp, int aliasindex, char **tokens, int *ntokens )
{
   if ( opt_oregon(aliasp, aliasindex, tokens, ntokens) != 0 )
      return 1;

   aliasp[aliasindex].subtype = OreTemp3;
   aliasp[aliasindex].nvar = 1;
   aliasp[aliasindex].storage_units = 2 * aliasp[aliasindex].nvar;
   aliasp[aliasindex].funclist[0] = OreTempFunc;

   return 0;
}
   
/*---------------------------------------------------------------------+
 | Options for Oregon TH1 sensors                                      |
 +---------------------------------------------------------------------*/
int opt_oreTH1 ( ALIAS *aliasp, int aliasindex, char **tokens, int *ntokens )
{
   if ( opt_oregon(aliasp, aliasindex, tokens, ntokens) != 0 )
      return 1;

   aliasp[aliasindex].subtype = OreTH1;
   aliasp[aliasindex].nvar = 2;
   aliasp[aliasindex].storage_units = 2 * aliasp[aliasindex].nvar;
   aliasp[aliasindex].funclist[0] = OreTempFunc;
   aliasp[aliasindex].funclist[1] = OreHumidFunc;

   return 0;
}
   
/*---------------------------------------------------------------------+
 | Options for Oregon TH2 sensors                                      |
 +---------------------------------------------------------------------*/
int opt_oreTH2 ( ALIAS *aliasp, int aliasindex, char **tokens, int *ntokens )
{
   if ( opt_oregon(aliasp, aliasindex, tokens, ntokens) != 0 )
      return 1;

   aliasp[aliasindex].subtype = OreTH2;
   aliasp[aliasindex].nvar = 2;
   aliasp[aliasindex].storage_units = 2 * aliasp[aliasindex].nvar;
   aliasp[aliasindex].funclist[0] = OreTempFunc;
   aliasp[aliasindex].funclist[1] = OreHumidFunc;

   return 0;
}
   
/*---------------------------------------------------------------------+
 | Options for Oregon TH3 sensors                                      |
 +---------------------------------------------------------------------*/
int opt_oreTH3 ( ALIAS *aliasp, int aliasindex, char **tokens, int *ntokens )
{
   if ( opt_oregon(aliasp, aliasindex, tokens, ntokens) != 0 )
      return 1;

   aliasp[aliasindex].subtype = OreTH3;
   aliasp[aliasindex].nvar = 2;
   aliasp[aliasindex].storage_units = 2 * aliasp[aliasindex].nvar;
   aliasp[aliasindex].funclist[0] = OreTempFunc;
   aliasp[aliasindex].funclist[1] = OreHumidFunc;

   return 0;
}
   
/*---------------------------------------------------------------------+
 | Options for Oregon TH4 sensors                                      |
 +---------------------------------------------------------------------*/
int opt_oreTH4 ( ALIAS *aliasp, int aliasindex, char **tokens, int *ntokens )
{
   if ( opt_oregon(aliasp, aliasindex, tokens, ntokens) != 0 )
      return 1;

   aliasp[aliasindex].subtype = OreTH4;
   aliasp[aliasindex].nvar = 2;
   aliasp[aliasindex].storage_units = 2 * aliasp[aliasindex].nvar;
   aliasp[aliasindex].funclist[0] = OreTempFunc;
   aliasp[aliasindex].funclist[1] = OreHumidFunc;

   return 0;
}
   
/*---------------------------------------------------------------------+
 | Options for Oregon TH5 sensors                                      |
 +---------------------------------------------------------------------*/
int opt_oreTH5 ( ALIAS *aliasp, int aliasindex, char **tokens, int *ntokens )
{
   if ( opt_oregon(aliasp, aliasindex, tokens, ntokens) != 0 )
      return 1;

   aliasp[aliasindex].subtype = OreTH5;
   aliasp[aliasindex].nvar = 2;
   aliasp[aliasindex].storage_units = 2 * aliasp[aliasindex].nvar;
   aliasp[aliasindex].funclist[0] = OreTempFunc;
   aliasp[aliasindex].funclist[1] = OreHumidFunc;

   return 0;
}
   
/*---------------------------------------------------------------------+
 | Options for Oregon TH6 sensors                                      |
 +---------------------------------------------------------------------*/
int opt_oreTH6 ( ALIAS *aliasp, int aliasindex, char **tokens, int *ntokens )
{
   if ( opt_oregon(aliasp, aliasindex, tokens, ntokens) != 0 )
      return 1;

   aliasp[aliasindex].subtype = OreTH6;
   aliasp[aliasindex].nvar = 2;
   aliasp[aliasindex].storage_units = 2 * aliasp[aliasindex].nvar;
   aliasp[aliasindex].funclist[0] = OreTempFunc;
   aliasp[aliasindex].funclist[1] = OreHumidFunc;

   return 0;
}
   
/*---------------------------------------------------------------------+
 | Options for Oregon THB1 sensors                                     |
 +---------------------------------------------------------------------*/
int opt_oreTHB1 ( ALIAS *aliasp, int aliasindex, char **tokens, int *ntokens )
{
   if ( opt_oregon(aliasp, aliasindex, tokens, ntokens) != 0 )
      return 1;

   aliasp[aliasindex].subtype = OreTHB1;
   aliasp[aliasindex].nvar = 3;
   aliasp[aliasindex].storage_units = 2 * aliasp[aliasindex].nvar;
   aliasp[aliasindex].funclist[0] = OreTempFunc;
   aliasp[aliasindex].funclist[1] = OreHumidFunc;
   aliasp[aliasindex].funclist[2] = OreBaroFunc;

   return 0;
}
   
/*---------------------------------------------------------------------+
 | Options for Oregon THB2 sensors                                     |
 +---------------------------------------------------------------------*/
int opt_oreTHB2 ( ALIAS *aliasp, int aliasindex, char **tokens, int *ntokens )
{
   if ( opt_oregon(aliasp, aliasindex, tokens, ntokens) != 0 )
      return 1;

   aliasp[aliasindex].subtype = OreTHB2;
   aliasp[aliasindex].nvar = 3;
   aliasp[aliasindex].storage_units = 2 * aliasp[aliasindex].nvar;
   aliasp[aliasindex].funclist[0] = OreTempFunc;
   aliasp[aliasindex].funclist[1] = OreHumidFunc;
   aliasp[aliasindex].funclist[2] = OreBaroFunc;

   return 0;
}

/*---------------------------------------------------------------------+
 | Options for Oregon Wind1 sensors                                    |
 +---------------------------------------------------------------------*/
int opt_oreWind1 ( ALIAS *aliasp, int aliasindex, char **tokens, int *ntokens )
{
   if ( opt_oregon(aliasp, aliasindex, tokens, ntokens) != 0 )
      return 1;

   aliasp[aliasindex].subtype = OreWind1;
   aliasp[aliasindex].nvar = 3;
   aliasp[aliasindex].storage_units = 2 * aliasp[aliasindex].nvar;
   aliasp[aliasindex].funclist[0] = OreWindAvSpFunc;
   aliasp[aliasindex].funclist[1] = OreWindSpFunc;
   aliasp[aliasindex].funclist[2] = OreWindDirFunc;

   return 0;
}

/*---------------------------------------------------------------------+
 | Options for Oregon Wind2 sensors                                    |
 +---------------------------------------------------------------------*/
int opt_oreWind2 ( ALIAS *aliasp, int aliasindex, char **tokens, int *ntokens )
{
   if ( opt_oregon(aliasp, aliasindex, tokens, ntokens) != 0 )
      return 1;

   aliasp[aliasindex].subtype = OreWind2;
   aliasp[aliasindex].nvar = 3;
   aliasp[aliasindex].storage_units = 2 * aliasp[aliasindex].nvar;
   aliasp[aliasindex].funclist[0] = OreWindAvSpFunc;
   aliasp[aliasindex].funclist[1] = OreWindSpFunc;
   aliasp[aliasindex].funclist[2] = OreWindDirFunc;

   return 0;
}

/*---------------------------------------------------------------------+
 | Options for Oregon Wind3 sensors                                    |
 +---------------------------------------------------------------------*/
int opt_oreWind3 ( ALIAS *aliasp, int aliasindex, char **tokens, int *ntokens )
{
   if ( opt_oregon(aliasp, aliasindex, tokens, ntokens) != 0 )
      return 1;

   aliasp[aliasindex].subtype = OreWind3;
   aliasp[aliasindex].nvar = 3;
   aliasp[aliasindex].storage_units = 2 * aliasp[aliasindex].nvar;
   aliasp[aliasindex].funclist[0] = OreWindAvSpFunc;
   aliasp[aliasindex].funclist[1] = OreWindSpFunc;
   aliasp[aliasindex].funclist[2] = OreWindDirFunc;

   return 0;
}

/*---------------------------------------------------------------------+
 | Options for Oregon Rain1 sensors                                    |
 +---------------------------------------------------------------------*/
int opt_oreRain1 ( ALIAS *aliasp, int aliasindex, char **tokens, int *ntokens )
{
   if ( opt_oregon(aliasp, aliasindex, tokens, ntokens) != 0 )
      return 1;

   aliasp[aliasindex].subtype = OreRain1;
   aliasp[aliasindex].nvar = 2;
   /* Need extra storage locations for 32 bit data */
   aliasp[aliasindex].storage_units = 4 * aliasp[aliasindex].nvar;
   aliasp[aliasindex].funclist[0] = OreRainRateFunc;
   aliasp[aliasindex].funclist[1] = OreRainTotFunc;
   /* Override the default storage offsets */
   aliasp[aliasindex].statusoffset[0] = 0;
   aliasp[aliasindex].statusoffset[1] = 4;

   return 0;
}

/*---------------------------------------------------------------------+
 | Options for Oregon Rain2 sensors                                    |
 +---------------------------------------------------------------------*/
int opt_oreRain2 ( ALIAS *aliasp, int aliasindex, char **tokens, int *ntokens )
{
   if ( opt_oregon(aliasp, aliasindex, tokens, ntokens) != 0 )
      return 1;

   aliasp[aliasindex].subtype = OreRain2;
   aliasp[aliasindex].nvar = 2;
   /* Need extra storage locations for 32 bit data */
   aliasp[aliasindex].storage_units = 4 * aliasp[aliasindex].nvar;
   aliasp[aliasindex].funclist[0] = OreRainRateFunc;
   aliasp[aliasindex].funclist[1] = OreRainTotFunc;
   /* Override the default storage offsets */
   aliasp[aliasindex].statusoffset[0] = 0;
   aliasp[aliasindex].statusoffset[1] = 4;

   return 0;
}

/*---------------------------------------------------------------------+
 | Options for Oregon Rain3 sensors                                    |
 +---------------------------------------------------------------------*/
int opt_oreRain3 ( ALIAS *aliasp, int aliasindex, char **tokens, int *ntokens )
{
   if ( opt_oregon(aliasp, aliasindex, tokens, ntokens) != 0 )
      return 1;

   aliasp[aliasindex].subtype = OreRain3;
   aliasp[aliasindex].nvar = 2;
   /* Need extra storage locations for 32 bit data */
   aliasp[aliasindex].storage_units = 4 * aliasp[aliasindex].nvar;
   aliasp[aliasindex].funclist[0] = OreRainRateFunc;
   aliasp[aliasindex].funclist[1] = OreRainTotFunc;
   /* Override the default storage offsets */
   aliasp[aliasindex].statusoffset[0] = 0;
   aliasp[aliasindex].statusoffset[1] = 4;

   return 0;
}

/*---------------------------------------------------------------------+
 | Options for Electrisave sensors                                     |
 +---------------------------------------------------------------------*/
int opt_elsElec1 ( ALIAS *aliasp, int aliasindex, char **tokens, int *ntokens )
{
   if ( opt_oregon(aliasp, aliasindex, tokens, ntokens) != 0 )
      return 1;

   aliasp[aliasindex].vtype = RF_ELECSAVE;
   aliasp[aliasindex].subtype = OreElec1;
   aliasp[aliasindex].nvar = 1;
   aliasp[aliasindex].storage_units = 2 * aliasp[aliasindex].nvar;
   aliasp[aliasindex].funclist[0] = ElsCurrFunc;

   return 0;
}

/*---------------------------------------------------------------------+
 | Options for OWL CM119 sensors                                       |
 +---------------------------------------------------------------------*/
int opt_owlElec2 ( ALIAS *aliasp, int aliasindex, char **tokens, int *ntokens )
{
   if ( opt_oregon(aliasp, aliasindex, tokens, ntokens) != 0 )
      return 1;

   /* OWL needs 2 storage locations for power and 3 storage locations */
   /* for energy - all times 2 for last unchanged values also.        */

   aliasp[aliasindex].vtype = RF_OWL;
   aliasp[aliasindex].subtype = OreElec2;
   aliasp[aliasindex].nvar = 2;
   aliasp[aliasindex].storage_units = 10;
   aliasp[aliasindex].funclist[0] = OwlPowerFunc;
   aliasp[aliasindex].funclist[1] = OwlEnergyFunc;
   /* Override the default storage offsets */
   aliasp[aliasindex].statusoffset[0] = 0;
   aliasp[aliasindex].statusoffset[1] = 2;

   aliasp[aliasindex].optflags &= ~MOPT_LOBAT;
   aliasp[aliasindex].optflags |= MOPT_LOBATUNKN;

   return 0;
}

/*---------------------------------------------------------------------+
 | Options for OWL CM119 sensors                                       |
 +---------------------------------------------------------------------*/
int opt_owlElec2new ( ALIAS *aliasp, int aliasindex, char **tokens, int *ntokens )
{
   if ( opt_oregon(aliasp, aliasindex, tokens, ntokens) != 0 )
      return 1;

   /* OWL needs 2 storage locations for power and 3 storage locations */
   /* for energy - all times 2 for last unchanged values also.        */

   aliasp[aliasindex].vtype = RF_OWL;
   aliasp[aliasindex].subtype = OreElec2;
   aliasp[aliasindex].nvar = 2;
   aliasp[aliasindex].storage_units = 10;
   aliasp[aliasindex].funclist[0] = OwlPowerFunc;
   aliasp[aliasindex].funclist[1] = OwlEnergyFunc;
   /* Override the default storage offsets */
   aliasp[aliasindex].statusoffset[0] = 0;
   aliasp[aliasindex].statusoffset[1] = 4;

   aliasp[aliasindex].optflags &= ~MOPT_LOBAT;
   aliasp[aliasindex].optflags |= MOPT_LOBATUNKN;

   return 0;
}

/*---------------------------------------------------------------------+
 | Options for OWL CM119 sensors                                       |
 +---------------------------------------------------------------------*/
int opt_owlElec2rev ( ALIAS *aliasp, int aliasindex, char **tokens, int *ntokens )
{
   if ( opt_oregon(aliasp, aliasindex, tokens, ntokens) != 0 )
      return 1;

   /* OWL needs 2 storage locations for power and 3 storage locations */
   /* for energy - all times 2 for last unchanged values also.        */

   aliasp[aliasindex].vtype = RF_OWL;
   aliasp[aliasindex].subtype = OreElec2;
   aliasp[aliasindex].nvar = 2;
   aliasp[aliasindex].storage_units = 10;
   aliasp[aliasindex].funclist[0] = OwlEnergyFunc;
   aliasp[aliasindex].funclist[1] = OwlPowerFunc;
   /* Override the default storage offsets */
   aliasp[aliasindex].statusoffset[0] = 4;
   aliasp[aliasindex].statusoffset[1] = 0;

   aliasp[aliasindex].optflags &= ~MOPT_LOBAT;
   aliasp[aliasindex].optflags |= MOPT_LOBATUNKN;

   return 0;
}

/*---------------------------------------------------------------------+
 | Options for Oregon UV1 sensors                                      |
 +---------------------------------------------------------------------*/
int opt_oreUV1 ( ALIAS *aliasp, int aliasindex, char **tokens, int *ntokens )
{
   if ( opt_oregon(aliasp, aliasindex, tokens, ntokens) != 0 )
      return 1;

   aliasp[aliasindex].subtype = OreUV1;
   aliasp[aliasindex].nvar = 1;
   aliasp[aliasindex].storage_units = 2 * aliasp[aliasindex].nvar;
   aliasp[aliasindex].funclist[0] = OreUVFunc;

   return 0;
}

/*---------------------------------------------------------------------+
 | Options for Oregon UV2 sensors                                      |
 +---------------------------------------------------------------------*/
int opt_oreUV2 ( ALIAS *aliasp, int aliasindex, char **tokens, int *ntokens )
{
   if ( opt_oregon(aliasp, aliasindex, tokens, ntokens) != 0 )
      return 1;

   aliasp[aliasindex].subtype = OreUV2;
   aliasp[aliasindex].nvar = 1;
   aliasp[aliasindex].storage_units = 2 * aliasp[aliasindex].nvar;
   aliasp[aliasindex].funclist[0] = OreUVFunc;

   return 0;
}

/*---------------------------------------------------------------------+
 | Options for Oregon WEIGHT1 sensors                                  |
 +---------------------------------------------------------------------*/
int opt_oreWeight1 ( ALIAS *aliasp, int aliasindex, char **tokens, int *ntokens )
{
   long ident;
   char *sp;

   if ( *ntokens != 1 ) {
      store_error_message("Unknown parameters");
      return 1;
   }

   if ( single_bmap_unit(aliasp[aliasindex].unitbmap) == 0xff ) {
      store_error_message("Multiple unit alias is invalid for sensors.");
      return 1;
   }

   ident = strtol(tokens[0], &sp, 16);
   if ( *sp == '\0' && ident >= 0 && ident <= 0xfff ) {
      aliasp[aliasindex].ident[0] = (unsigned short)ident;
      aliasp[aliasindex].nident = 1;
   }
   else {
      store_error_message("Invalid hexadecimal Oregon address parameter");
      return 1;
   }

   aliasp[aliasindex].vtype = RF_OREGON;
   aliasp[aliasindex].subtype = OreWeight1;
   aliasp[aliasindex].optflags = (MOPT_SECURITY | MOPT_SENSOR);
   aliasp[aliasindex].nvar = 1;
   aliasp[aliasindex].storage_units = 2 * aliasp[aliasindex].nvar;
   aliasp[aliasindex].funclist[0] = OreWeightFunc;

   return 0;
}

/*---------------------------------------------------------------------+
 | Options for Oregon Dummy Temp sensors                                |
 +---------------------------------------------------------------------*/
int opt_oreTemu ( ALIAS *aliasp, int aliasindex, char **tokens, int *ntokens )
{
   if ( opt_ore_dummy(aliasp, aliasindex, tokens, ntokens) != 0 )
      return 1;

   aliasp[aliasindex].subtype = OreTemu;
   aliasp[aliasindex].nvar = 1;
   aliasp[aliasindex].storage_units = 2 * aliasp[aliasindex].nvar;
   aliasp[aliasindex].funclist[0] = OreTempFunc;

   return 0;
}

/*---------------------------------------------------------------------+
 | Options for Oregon Dummy Temp/Humidity sensors                      |
 +---------------------------------------------------------------------*/
int opt_oreTHemu ( ALIAS *aliasp, int aliasindex, char **tokens, int *ntokens )
{
   if ( opt_ore_dummy(aliasp, aliasindex, tokens, ntokens) != 0 )
      return 1;

   aliasp[aliasindex].subtype = OreTHemu;
   aliasp[aliasindex].nvar = 2;
   aliasp[aliasindex].storage_units = 2 * aliasp[aliasindex].nvar;
   aliasp[aliasindex].funclist[0] = OreTempFunc;
   aliasp[aliasindex].funclist[1] = OreHumidFunc;

   return 0;
}

/*---------------------------------------------------------------------+
 | Options for Oregon Dummy THB sensors                                |
 +---------------------------------------------------------------------*/
int opt_oreTHBemu ( ALIAS *aliasp, int aliasindex, char **tokens, int *ntokens )
{
   if ( opt_ore_dummy(aliasp, aliasindex, tokens, ntokens) != 0 )
      return 1;

   aliasp[aliasindex].subtype = OreTHBemu;
   aliasp[aliasindex].nvar = 3;
   aliasp[aliasindex].storage_units = 2 * aliasp[aliasindex].nvar;
   aliasp[aliasindex].funclist[0] = OreTempFunc;
   aliasp[aliasindex].funclist[1] = OreHumidFunc;
   aliasp[aliasindex].funclist[2] = OreBaroFunc;

   return 0;
}

/*-------------------------------------------------------+
 | Called for Oregon sensor IDs which are to be ignored  |
 +-------------------------------------------------------*/ 
int opt_oreignore ( ALIAS *aliasp, int aliasindex, char **tokens, int *ntokens )
{
   char   *sp;
   long   ident;
   int    j;

   /* First token is security ID */
   if ( *ntokens < 1 ) {
      store_error_message("Module ID(s) needed.");
      return 1;
   }
   if ( *ntokens > NIDENT ) {
      store_error_message("Too many parameters on line.");
      return 1;
   }

   for ( j = 0; j < *ntokens; j++ ) {
      ident = strtol(tokens[j], &sp, 16);
      if ( *sp == '\0' && ident >= 0 && ident <= 0xffff ) {  /* Was 0xaff ??? */
         aliasp[aliasindex].ident[j] = (unsigned short)ident;
      }
      else {
         store_error_message("Invalid hexadecimal ID parameter");
         return 1;
      }
   }
   aliasp[aliasindex].nident = j;
   
   /* This is a security RF transmitter */
   aliasp[aliasindex].vtype = RF_OREGON;
   aliasp[aliasindex].optflags = (MOPT_SECURITY | MOPT_RFIGNORE);

   return 0;
}

#endif   

/*--------------------------------------------------------+
 | Called for Security sensor IDs which are to be ignored |
 +--------------------------------------------------------*/ 
int opt_secignore ( ALIAS *aliasp, int aliasindex, char **tokens, int *ntokens )
{
   char   *sp;
   long   ident;
   int    j;

   /* First token is security ID */
   if ( *ntokens < 1 ) {
      store_error_message("Module ID(s) needed.");
      return 1;
   }
   if ( *ntokens > NIDENT ) {
      store_error_message("Too many parameters on line.");
      return 1;
   }

   for ( j = 0; j < *ntokens; j++ ) {
      ident = strtol(tokens[j], &sp, 16);
      if ( *sp == '\0' && ident >= 0 && ident <= 0xffff ) {
         aliasp[aliasindex].ident[j] = (unsigned short)ident;
      }
      else {
         store_error_message("Invalid hexadecimal ID parameter");
         return 1;
      }
   }
   aliasp[aliasindex].nident = j;
   
   /* This is a security RF transmitter */
   aliasp[aliasindex].vtype = RF_SEC;
   aliasp[aliasindex].optflags = (MOPT_SECURITY | MOPT_RFIGNORE);

   return 0;
}

/*-------------------------------------------------------+
 | Called by add_module_options() to add the identity of |
 | the X-10 UX17A transceiver to the module.             |
 +-------------------------------------------------------*/ 
int opt_ux17a ( ALIAS *aliasp, int aliasindex, char **tokens, int *ntokens )
{

   if ( *ntokens > 0 ) {
      store_error_message("This module takes no parameters.");
      return 1;
   }

   /* First byte of UX17A codeword is either 0x82 or 0x83 */
   aliasp[aliasindex].nident = 2;  
   aliasp[aliasindex].ident[0] = 0x82;
   aliasp[aliasindex].ident[1] = 0x83;
   
   /* This is an entertainment remote transmitter */
   aliasp[aliasindex].vtype = RF_ENT;

   return 0;
}

/*-------------------------------------------------------+
 | Called by add_module_options() to add the identity of |
 | the X-10 UR81A remote to the module.                  |
 +-------------------------------------------------------*/ 
int opt_ur81a ( ALIAS *aliasp, int aliasindex, char **tokens, int *ntokens )
{

   if ( *ntokens > 0 ) {
      store_error_message("This module takes no parameters.");
      return 1;
   }

   /* First byte of UR81A codeword is always 0xEE */
   aliasp[aliasindex].nident = 1;  
   aliasp[aliasindex].ident[0] = 0xEE;
   
   /* This is an entertainment remote transmitter */
   aliasp[aliasindex].vtype = RF_ENT;

   return 0;
}

/*-------------------------------------------------------+
 | Called by add_module_options() to add the IDs for a   |
 | generic entertainment remote.  Valid ID bytes for an  |
 | entertainment remote have bit 2 (1-8) set.            |
 +-------------------------------------------------------*/ 
int opt_guru ( ALIAS *aliasp, int aliasindex, char **tokens, int *ntokens )
{
   char   *sp;
   long   ident;
   int    j;

   /* Tokens are IDs */
   if ( *ntokens < 1 ) {
      store_error_message("At least one entertainment ID is needed.");
      return 1;
   }
   if ( *ntokens > NIDENT ) {
      store_error_message("Too many parameters on line.");
      return 1;
   }

   for ( j = 0; j < *ntokens; j++ ) {
      ident = strtol(tokens[j], &sp, 16);
      if ( *sp == '\0' && ident >= 0 && ident <= 0xff && (ident & 0x02) ) {
         aliasp[aliasindex].ident[j] = (unsigned short)ident;
      }
      else {
         store_error_message("Invalid hexadecimal entertainment ID parameter");
         return 1;
      }
   }
   aliasp[aliasindex].nident = j;
   
   /* This is an entertainment RF transmitter */
   aliasp[aliasindex].vtype = RF_ENT;
   aliasp[aliasindex].optflags |= MOPT_ENTERTAIN;

   return 0;
}




/*-------------------------------------------------------+
 | For security remotes with Arm/Disarm buttons          |
 | Called by add_module_options() to add the RF security |
 | ID from the ALIAS line in the config file.            |
 +-------------------------------------------------------*/ 
int opt_kremote ( ALIAS *aliasp, int aliasindex, char **tokens, int *ntokens )
{
   char   *sp;
   long   ident;
   int    j;

   if ( remote_options(aliasp, aliasindex, tokens, ntokens) != 0 )
      return 1;

   /* First token is security ID */
   if ( *ntokens < 1 ) {
      store_error_message("Module ID is needed.");
      return 1;
   }
   if ( *ntokens > NIDENT ) {
      store_error_message("Too many parameters on line.");
      return 1;
   }

   for ( j = 0; j < *ntokens; j++ ) {
      ident = strtol(tokens[j], &sp, 16);
      if ( *sp == '\0' && ident >= 0 && ident <= 0xffff ) {
         aliasp[aliasindex].ident[j] = (unsigned short)ident;
      }
      else {
         store_error_message("Invalid hexadecimal ID parameter");
         return 1;
      }
   }
   aliasp[aliasindex].nident = j;
   
   /* This is a security RF transmitter */
   aliasp[aliasindex].vtype = RF_SEC;
   aliasp[aliasindex].optflags |= MOPT_SECURITY;

   return 0;
}

/*-------------------------------------------------------+
 | Called by add_module_options() to add the RF security |
 | ID from the ALIAS line in the config file.            |
 +-------------------------------------------------------*/ 
int opt_sremote ( ALIAS *aliasp, int aliasindex, char **tokens, int *ntokens )
{
   char   *sp;
   long   ident;
   int    j;

   /* First token is security ID */
   if ( *ntokens < 1 ) {
      store_error_message("Module ID is needed.");
      return 1;
   }
   if ( *ntokens > NIDENT ) {
      store_error_message("Too many parameters on line.");
      return 1;
   }

   for ( j = 0; j < *ntokens; j++ ) {
      ident = strtol(tokens[j], &sp, 16);
      if ( *sp == '\0' && ident >= 0 && ident <= 0xffff ) {
         aliasp[aliasindex].ident[j] = (unsigned short)ident;
      }
      else {
         store_error_message("Invalid hexadecimal ID parameter");
         return 1;
      }
   }
   aliasp[aliasindex].nident = j;
   
   /* This is a security RF transmitter */
   aliasp[aliasindex].vtype = RF_SEC;
   aliasp[aliasindex].optflags |= MOPT_SECURITY;

   return 0;
}

/*-------------------------------------------------------+
 | Like opt_sremote by also identified as a sensor       |
 +-------------------------------------------------------*/
int opt_svsensor ( ALIAS *aliasp, int aliasindex, char **tokens, int *ntokens )
{
   if ( opt_sensor(aliasp, aliasindex, tokens, ntokens) != 0 )
      return 1;

   if ( aliasp[aliasindex].optflags & MOPT_REVERSE ) {
      store_error_message("Unsupported module option REVERSE");
      return 1;
   }

   aliasp[aliasindex].optflags &= ~(MOPT_HEARTBEAT | MOPT_LOBAT);

   return 0;
}

int opt_visonic ( ALIAS *aliasp, int aliasindex, char **tokens, int *ntokens )
{
   char   *sp;
   long   ident;
   int    j;

   /* First token is security ID */
   if ( *ntokens < 1 ) {
      store_error_message("Module ID is needed.");
      return 1;
   }
   if ( *ntokens > NIDENT ) {
      store_error_message("Too many parameters on line.");
      return 1;
   }

   for ( j = 0; j < *ntokens; j++ ) {
      ident = strtol(tokens[j], &sp, 16);
      if ( *sp == '\0' && ident >= 0 && ident <= 0xffffff ) {
         aliasp[aliasindex].ident[j] = ident;
      }
      else {
         store_error_message("Invalid hexadecimal ID parameter");
         return 1;
      }
   }
   aliasp[aliasindex].nident = j;
   
   /* This is a security RF transmitter */
   aliasp[aliasindex].vtype = RF_VISONIC;
   aliasp[aliasindex].optflags |= MOPT_SECURITY;

   return 0;
}

/*-------------------------------------------------------+
 | Called by add_module_options() to add the RF security |
 | sensor options from the ALIAS line in the config file.|
 +-------------------------------------------------------*/ 
int opt_sensor ( ALIAS *aliasp, int aliasindex, char **tokens, int *ntokens )
{
   sensor_options(aliasp, aliasindex, tokens, ntokens);

   if ( sensor_timeout(aliasp, aliasindex, tokens, ntokens) != 0 )
      return 1;

   if ( opt_sremote(aliasp, aliasindex, tokens, ntokens) != 0 )
      return 1;

   if ( single_bmap_unit(aliasp[aliasindex].unitbmap) == 0xff ) {
      store_error_message("Multiple unit alias is invalid for sensors.");
      return 1;
   }

   if ( aliasp[aliasindex].nident > 1 ) {
      store_error_message("Only one security ID is supported");
      return 1;
   }

   if ( aliasp[aliasindex].optflags & (MOPT_MAIN | MOPT_AUX) ) {
      store_error_message("Unsupported module option MAIN or AUX");
      return 1;
   }

   /* This is a sensor with periodic "heartbeat" signals and low battery flag */
   aliasp[aliasindex].optflags |= (MOPT_SENSOR | MOPT_HEARTBEAT | MOPT_LOBAT);

   return 0;
}

/*-------------------------------------------------------+
 | Called by add_module_options() to add the SD90 Smoke  |
 | Detector IDs from the ALIAS line in the config file.  |
 +-------------------------------------------------------*/ 
int opt_sd90 ( ALIAS *aliasp, int aliasindex, char **tokens, int *ntokens )
{
   unsigned short id0, id1;
   unsigned long  optflags = 0;

   sensor_options(aliasp, aliasindex, tokens, ntokens);

   if ( sensor_timeout(aliasp, aliasindex, tokens, ntokens) != 0 )
      return 1;

   if ( opt_sremote(aliasp, aliasindex, tokens, ntokens) != 0 )
      return 1;

   if ( single_bmap_unit(aliasp[aliasindex].unitbmap) == 0xff ) {
      store_error_message("Multiple unit alias is invalid.");
      return 1;
   }

   if ( aliasp[aliasindex].nident == 2 ) {
      id0 = aliasp[aliasindex].ident[0] & 0x00F0;
      id1 = aliasp[aliasindex].ident[1] & 0x00F0;    
      if ( (id0 == 0xC0 && id1 == 0xD0) ||
           (id0 == 0xD0 && id1 == 0xC0)    ) {
         optflags = (MOPT_SENSOR | MOPT_HEARTBEAT | MOPT_LOBAT);
      }
      else {
         store_error_message("Invalid ID value for the SD90");
         return 1;
      }
   }
   else if ( aliasp[aliasindex].nident == 1 ) {
      id0 = aliasp[aliasindex].ident[0] & 0x00F0;
      if ( id0 == 0xD0 ) {
         /* Sensor ID */
         optflags = (MOPT_SENSOR | MOPT_HEARTBEAT | MOPT_LOBAT);
      }
      else if ( id0 == 0xC0 ) {
         /* Emergency ID */
         optflags = 0;
      }
      else {
         store_error_message("Invalid ID value for the SD90");
         return 1;
      }
   }
   else {
      store_error_message("SD90 module type requires one or two IDs");
      return 1;
   }

   if ( aliasp[aliasindex].optflags & (MOPT_MAIN | MOPT_AUX | MOPT_REVERSE) ) {
      store_error_message("Unsupported module option");
      return 1;
   }

   /* This is a sensor */
   aliasp[aliasindex].optflags |= /* MOPT_SENSOR | */ optflags;

   return 0;
}

/*-------------------------------------------------------+
 | Called by add_module_options() for the Marmitek SD10  |
 | Smoke Detector which has no "heartbeat".              |
 +-------------------------------------------------------*/ 
int opt_sd10 ( ALIAS *aliasp, int aliasindex, char **tokens, int *ntokens )
{
   char   *sp;
   long   ident;

   /* Token is security ID */
   if ( single_bmap_unit(aliasp[aliasindex].unitbmap) == 0xff ) {
      store_error_message("Multiple unit alias is invalid for sensors.");
      return 1;
   }

   if ( *ntokens < 1 ) {
      store_error_message("Module ID is needed.");
      return 1;
   }
   else if ( *ntokens > 1 ) {
      store_error_message("Only one security ID is supported.");
      return 1;
   }

   ident = strtol(tokens[0], &sp, 16);
   if ( *sp == '\0' && ident >= 0 && ident <= 0xffff ) {
      aliasp[aliasindex].ident[0] = (unsigned short)ident;
   }
   else {
      store_error_message("Invalid hexadecimal ID parameter");
      return 1;
   }

   aliasp[aliasindex].nident = 1;
   
   /* This is a security RF transmitter */
   aliasp[aliasindex].vtype = RF_SEC;
   aliasp[aliasindex].optflags |= MOPT_SECURITY;

   return 0;
}

/*-------------------------------------------------------+
 | Called by add_module_options() for the BMB-SD18       |
 | Smoke Detector                                        |
 +-------------------------------------------------------*/ 
int opt_bmb_sd18 ( ALIAS *aliasp, int aliasindex, char **tokens, int *ntokens )
{
   char   *sp;
   long   ident;

   if ( sensor_timeout(aliasp, aliasindex, tokens, ntokens) != 0 )
      return 1;

   /* Token is security ID */
   if ( single_bmap_unit(aliasp[aliasindex].unitbmap) == 0xff ) {
      store_error_message("Multiple unit alias is invalid for sensors.");
      return 1;
   }

   if ( *ntokens < 1 ) {
      store_error_message("Module ID is needed.");
      return 1;
   }
   else if ( *ntokens > 1 ) {
      store_error_message("Only one security ID is supported.");
      return 1;
   }

   ident = strtol(tokens[0], &sp, 16);
   if ( *sp == '\0' && ident >= 0 && ident <= 0xffff ) {
      aliasp[aliasindex].ident[0] = (unsigned short)ident;
   }
   else {
      store_error_message("Invalid hexadecimal ID parameter");
      return 1;
   }

   aliasp[aliasindex].nident = 1;
   
   /* This is a security RF transmitter */
   aliasp[aliasindex].vtype = RF_SEC;
   aliasp[aliasindex].optflags |= (MOPT_SENSOR | MOPT_SECURITY | MOPT_HEARTBEAT | MOPT_LOBAT);

   return 0;
}

/*-------------------------------------------------------+
 | Called by add_module_options() for the Marmitek GB10  |
 | Glass Break sensor.                                   |
 +-------------------------------------------------------*/ 
int opt_gb10 ( ALIAS *aliasp, int aliasindex, char **tokens, int *ntokens )
{
   char   *sp;
   long   ident;

   if ( sensor_timeout(aliasp, aliasindex, tokens, ntokens) != 0 )
      return 1;

   /* Token is security ID */
   if ( single_bmap_unit(aliasp[aliasindex].unitbmap) == 0xff ) {
      store_error_message("Multiple unit alias is invalid for sensors.");
      return 1;
   }

   if ( *ntokens < 1 ) {
      store_error_message("Module ID is needed.");
      return 1;
   }
   else if ( *ntokens > 1 ) {
      store_error_message("Only one security ID is supported");
      return 1;
   }

   ident = strtol(tokens[0], &sp, 16);
   if ( *sp == '\0' && ident >= 0 && ident <= 0xffff ) {
      aliasp[aliasindex].ident[0] = (unsigned short)ident;
   }
   else {
      store_error_message("Invalid hexadecimal ID parameter");
      return 1;
   }

   aliasp[aliasindex].nident = 1;
   
   /* This is a security RF transmitter */
   aliasp[aliasindex].vtype = RF_SEC;
   aliasp[aliasindex].optflags |= ((MOPT_SENSOR | MOPT_SECURITY | MOPT_HEARTBEAT) & ~MOPT_LOBAT);

   return 0;
}

/*-------------------------------------------------------+
 | Called by add_module_options() for Standard X10       |
 | sensors, e.g., MS12,13,14,16 motion detectors.        |                 
 +-------------------------------------------------------*/ 
int opt_x10std ( ALIAS *aliasp, int aliasindex, char **tokens, int *ntokens )
{

   aliasp[aliasindex].optflags = 0;

   opt_aux(aliasp, aliasindex, tokens, ntokens);

#if 0
   aliasp[aliasindex].optflags = 0;

   for ( j = 0; j < *ntokens; j++ ) {
      strupper(tokens[j]);
      if ( strcmp(tokens[j], "TRANSCEIVE") == 0 ) {
         aliasp[aliasindex].optflags |= MOPT_TRANSCEIVE;
         *tokens[j] = '\0';
         count++;
      }
      else if ( strcmp(tokens[j], "RFFORWARD") == 0 ) {
         aliasp[aliasindex].optflags |= MOPT_RFFORWARD;
         *tokens[j] = '\0';
         count++;
      }
      else if ( strcmp(tokens[j], "RFIGNORE") == 0 ) {
         aliasp[aliasindex].optflags |= MOPT_RFIGNORE;
         *tokens[j] = '\0';
         count++;
      }
      else {
         store_error_message("Unknown RF option parameter.");
         return 1;
      }
   }

   if ( count > 1 ) {
      store_error_message("Conflicting RF options.");
      return 1;
   }

   /* Compact to remove "gaps" where tokens were nulled out */

   for ( j = 0; j < *ntokens; j++ ) {
      if ( *tokens[j] == '\0' ) {
         for ( k = j + 1; k < *ntokens; k++ ) {
            if ( *tokens[k] != '\0' ) {
               tmptok = tokens[j];
               tokens[j] = tokens[k];
               tokens[k] = tmptok;
               *tmptok = '\0';
               break;
            }
         }
      }
   }

   *ntokens -= count;
#endif

   aliasp[aliasindex].nident = 0;
   
   /* This is a Standard X10 RF sensor transmitter */
   aliasp[aliasindex].vtype = RF_STD;
   aliasp[aliasindex].optflags |= MOPT_SENSOR;

   return 0;
}

/*-------------------------------------------------------+
 | Called by add_module_options() to add the RFXTemp     |
 | ID from the ALIAS line in the config file.            |
 +-------------------------------------------------------*/ 
int opt_rfxtemp ( ALIAS *aliasp, int aliasindex, char **tokens, int *ntokens )
{
   sensor_options(aliasp, aliasindex, tokens, ntokens);

   if ( sensor_timeout(aliasp, aliasindex, tokens, ntokens) != 0 )
      return 1;

   if ( opt_sremote(aliasp, aliasindex, tokens, ntokens) != 0 )
      return 1;

   if ( aliasp[aliasindex].nident > 1 ) {
      store_error_message("Only one RFXTemp ID is supported");
      return 1;
   }

   /* This is a sensor with periodic "heartbeat" signals */
   aliasp[aliasindex].vtype = RF_XSENSOR;
   aliasp[aliasindex].optflags |= (MOPT_RFXSENSOR | MOPT_HEARTBEAT | MOPT_LOBAT);

   return 0;
}

/*-------------------------------------------------------+
 | Called by add_module_options() to add the RF security |
 | sensor options from the ALIAS line in the config file.|
 +-------------------------------------------------------*/ 
int opt_ds90 ( ALIAS *aliasp, int aliasindex, char **tokens, int *ntokens )
{
   int check_ds90_ids( unsigned /*short*/ long * );
   unsigned short comp_ds90_id( unsigned short, int );

   sensor_options(aliasp, aliasindex, tokens, ntokens);

   if ( sensor_timeout(aliasp, aliasindex, tokens, ntokens) != 0 )
      return 1;

   if ( opt_sremote(aliasp, aliasindex, tokens, ntokens) != 0 )
      return 1;

   if ( (aliasp[aliasindex].optflags & MOPT_MAIN) &&
        (aliasp[aliasindex].optflags & MOPT_AUX)     ) {
      store_error_message("Module options MAIN and AUX are incompatible");
      return 1;
   }

   if ( aliasp[aliasindex].nident > 2 ) {
      store_error_message("Too many security IDs for DS90");
      return 1;
   }
   else if ( aliasp[aliasindex].nident == 2 ) {
      /* Check main and aux ID and reverse order if necessary */
      if ( check_ds90_ids(aliasp[aliasindex].ident) != 0 ) {
         store_error_message("DS90 security IDs are incompatible");
         return 1;
      }
   }
   else if ( aliasp[aliasindex].optflags & MOPT_MAIN ) {
      aliasp[aliasindex].ident[1] =
            comp_ds90_id(aliasp[aliasindex].ident[0], COMPUTE_AUX);
      aliasp[aliasindex].nident = 2;
   }
   else if ( aliasp[aliasindex].optflags & MOPT_AUX ) {
      aliasp[aliasindex].ident[1] = aliasp[aliasindex].ident[0];
      aliasp[aliasindex].ident[0] =
            comp_ds90_id(aliasp[aliasindex].ident[1], COMPUTE_MAIN);
      aliasp[aliasindex].nident = 2;
   }

   /* This is a sensor with periodic "heartbeat" signals */
   aliasp[aliasindex].optflags |= (MOPT_SENSOR | MOPT_SECURITY | MOPT_HEARTBEAT | MOPT_LOBATUNKN);

   return 0;
}


/*-------------------------------------------------------+
 | Called by add_module_options() to add the RESUME or   |
 | ONFULL <level> options from the ALIAS line in the     |
 | config file.                                          |
 +-------------------------------------------------------*/ 
int opt_onlevel ( ALIAS *aliasp, int aliasindex, char **tokens, int *ntokens )
{
   int    j, type, val, maxval, valoffset;
   char   *sp;

   type = aliasp[aliasindex].modtype;
   maxval = modules[type].maxlevel;
   valoffset = (modules[type].cflags & PRESET) ? 1 : 0;

   for ( j = 0; j < *ntokens; j++ )
      strupper(tokens[j]);

   j = 0;
   while ( *ntokens > 0 ) {
      if ( !strcmp(tokens[j], "ONLEVEL") ) {
         if ( *ntokens > 1 &&
              (!strcmp(tokens[j + 1], "RESUME") ||
               !strcmp(tokens[j + 1], "0"))        ) {
            aliasp[aliasindex].flags |= RESUME;
            aliasp[aliasindex].flags &= ~ONFULL;
            aliasp[aliasindex].optflags |= MOPT_RESUME;
            aliasp[aliasindex].optflags &= ~MOPT_ONFULL;
            aliasp[aliasindex].onlevel = maxval;
            j += 2;
            *ntokens -= 2;
         }
         else if ( *ntokens > 1 &&
                 (val = strtol(tokens[j + 1], &sp, 10)) &&
                 *sp == '\0' && val > 0 &&
                  val <= (maxval + valoffset) ) {
            aliasp[aliasindex].flags &= ~RESUME;
            aliasp[aliasindex].flags |= ONFULL;
            aliasp[aliasindex].optflags &= ~MOPT_RESUME;
            aliasp[aliasindex].optflags |= MOPT_ONFULL;
            aliasp[aliasindex].onlevel = val - valoffset;
            j += 2;
            *ntokens -= 2;
         }
         else if ( *ntokens > 1 ) {
            store_error_message("Invalid ONLEVEL parameter");
            return 1;
         }
         else {
            store_error_message("Missing ONLEVEL parameter");
            return 1;
         }
      }
      else {          
         store_error_message("Module option not recognized");
         return 1;
      }
   }
   return 0;
}


/*---------------------------------------------------------------------+
 | Set TRANSCEIVE, RFFORWARD, or RFIGNORE module options               |
 +---------------------------------------------------------------------*/
int opt_aux ( ALIAS *aliasp, int aliasindex, char **tokens, int *ntokens)
{
   int  j, k, count = 0;
   char *tmptok;

   for ( j = 0; j < *ntokens; j++ ) {
      strupper(tokens[j]);
      if ( strcmp(tokens[j], "TRANSCEIVE") == 0 ) {
         aliasp[aliasindex].optflags |= MOPT_TRANSCEIVE;
         *tokens[j] = '\0';
         count++;
      }
      else if ( strcmp(tokens[j], "RFFORWARD") == 0 ) {
         aliasp[aliasindex].optflags |= MOPT_RFFORWARD;
         *tokens[j] = '\0';
         count++;
      }
      else if ( strcmp(tokens[j], "RFIGNORE") == 0 ) {
         aliasp[aliasindex].optflags |= MOPT_RFIGNORE;
         *tokens[j] = '\0';
         count++;
      }
      else {
         store_error_message("Unknown RF option parameter.");
         return 1;
      }
   }

   if ( count == 0 ) {
      store_error_message("RF option TRANSCEIVE, RFFORWARD, or RFIGNORE required.");
      return 1;
   }
   else if ( count > 1 ) {
      store_error_message("Conflicting RF options.");
      return 1;
   }

   /* Compact to remove "gaps" where tokens were nulled out */

   for ( j = 0; j < *ntokens; j++ ) {
      if ( *tokens[j] == '\0' ) {
         for ( k = j + 1; k < *ntokens; k++ ) {
            if ( *tokens[k] != '\0' ) {
               tmptok = tokens[j];
               tokens[j] = tokens[k];
               tokens[k] = tmptok;
               *tmptok = '\0';
               break;
            }
         }
      }
   }

   *ntokens -= count;

   return 0;
}

/*-------------------------------------------------------+
 | Called by add_module_options() to add the identity of |
 | a jamming signal from a (older) RFXCOM receiver       |
 +-------------------------------------------------------*/ 
int opt_jam ( ALIAS *aliasp, int aliasindex, char **tokens, int *ntokens )
{

   if ( *ntokens > 0 ) {
      store_error_message("This module takes no parameters.");
      return 1;
   }

   /* ID is either 0xFF (master) or 0x00 (slave) */
   aliasp[aliasindex].nident = 2;  
   aliasp[aliasindex].ident[0] = 0xFF;
   aliasp[aliasindex].ident[1] = 0x00;
   
   /* This is a security remote transmitter */
   aliasp[aliasindex].vtype = RF_SEC;

   return 0;
}

   
/*---------------------------------------------------------------------+
 | Process received vdata for GB10 Glass Break Sensor                  |
 +---------------------------------------------------------------------*/
int fn_gb10 ( struct xlate_vdata_st *xlate_vdata )
{
  unsigned int vdata;

  vdata = xlate_vdata->vdata;

  if ( vdata & ~(0xF4u) ) {
     /* Unknown code */
     xlate_vdata->func = VdataFunc;
     xlate_vdata->trig = VdataTrig;
     xlate_vdata->vflags = 0;
     return 0;
  }

  if ( vdata == 0xF0u ) {
     xlate_vdata->func = DuskFunc;
     xlate_vdata->trig = DuskTrig;
  }
  else if ( vdata & 0x80u ) {
     xlate_vdata->func = ClearFunc;
     xlate_vdata->trig = ClearTrig;
  }
  else {
     xlate_vdata->func = AlertFunc;
     xlate_vdata->trig = AlertTrig;
  }

#if 0
  xlate_vdata->vflags = (vdata & 0x01u) ? SEC_LOBAT : 0;
#endif  /* May be needed */

  return 0;
}


/*---------------------------------------------------------------------+
 | Process received vdata for (Euro) DM10 Motion/Dusk/Dawn sensor      |
 +---------------------------------------------------------------------*/
int fn_dm10 ( struct xlate_vdata_st *xlate_vdata )
{
  unsigned int vdata;

  vdata = xlate_vdata->vdata;

  if ( vdata == 0xE0u ) {
     xlate_vdata->func = AlertFunc;
     xlate_vdata->trig = AlertTrig;
  }
  else if ( vdata == 0xF0 ) {
     xlate_vdata->func = DuskFunc;
     xlate_vdata->trig = DuskTrig;
  }
  else if ( vdata == 0xF8 ) {
     xlate_vdata->func = DawnFunc;
     xlate_vdata->trig = DawnTrig;
  }
  else {
     /* Unknown code */
     xlate_vdata->func = VdataFunc;
     xlate_vdata->trig = VdataTrig;
  }
     
  xlate_vdata->vflags = 0;

#if 0
  xlate_vdata->vflags = (vdata & 0x01u) ? SEC_LOBAT : 0;
#endif  /* May be needed */

  return 0;
}


/*---------------------------------------------------------------------+
 | Process received vdata for DS10A Door/Window Sensor                 |
 +---------------------------------------------------------------------*/
int fn_ds10a ( struct xlate_vdata_st *xlate_vdata )
{
  unsigned int vdata;
  int          polarity;

  vdata = xlate_vdata->vdata;
  polarity = (xlate_vdata->optflags & MOPT_REVERSE) ? 0 : 1;

  xlate_vdata->vflags = 0;

#if 0
  if ( vdata == 0xffu ) {
     xlate_vdata->func = InactiveFunc;
     xlate_vdata->trig = InactiveTrig;
     return 0;
  }
#endif

  if ( vdata & ~(0x85u) ) {
     xlate_vdata->func = VdataFunc;
     xlate_vdata->trig = VdataTrig;
     return 0;
  }

  if ( vdata & 0x80u ) {
     xlate_vdata->func = polarity ? ClearFunc : AlertFunc;
     xlate_vdata->trig = polarity ? ClearTrig : AlertTrig;
  }
  else {
     xlate_vdata->func = polarity ? AlertFunc : ClearFunc;
     xlate_vdata->trig = polarity ? AlertTrig : ClearTrig;
  }

  if ( xlate_vdata->func == AlertFunc )
     xlate_vdata->vflags |= SEC_FAULT;

  xlate_vdata->vflags |= (vdata & 0x04u) ? SEC_MIN : SEC_MAX;
  xlate_vdata->vflags |= (vdata & 0x01u) ? SEC_LOBAT : 0;

  return 0;
}

/*---------------------------------------------------------------------+
 | Process received vdata for ElekHomica DS18 Door/Window Sensor       |
 +---------------------------------------------------------------------*/
int fn_ds18 ( struct xlate_vdata_st *xlate_vdata )
{
  unsigned int vdata;
  int          polarity;

  vdata = xlate_vdata->vdata;
  polarity = (xlate_vdata->optflags & MOPT_REVERSE) ? 0 : 1;

  xlate_vdata->vflags = 0;

  if ( vdata & ~(0x85u) ) {
     xlate_vdata->func = VdataFunc;
     xlate_vdata->trig = VdataTrig;
     return 0;
  }

  if ( vdata & 0x80 ) {
     xlate_vdata->func = polarity ? ClearFunc : AlertFunc;
     xlate_vdata->trig = polarity ? ClearTrig : AlertTrig;
  }
  else {
     xlate_vdata->func = polarity ? AlertFunc : ClearFunc;
     xlate_vdata->trig = polarity ? AlertTrig : ClearTrig;
  }

  if ( xlate_vdata->func == AlertFunc )
     xlate_vdata->vflags |= SEC_FAULT;

  xlate_vdata->vflags |= (vdata & 0x04) ? SEC_MIN : SEC_MAX;
  xlate_vdata->vflags |= (vdata & 0x01) ? SEC_LOBAT : 0;

  return 0;
}

/*---------------------------------------------------------------------+
 | Process received vdata for DS90 Door/Window Sensor                  |
 +---------------------------------------------------------------------*/
int fn_ds90 ( struct xlate_vdata_st *xlate_vdata )
{
  unsigned int   vdata;
  int            polarity;
//  unsigned short mask;
  unsigned long  mask;

  vdata = xlate_vdata->vdata;
  polarity = (xlate_vdata->optflags & MOPT_REVERSE) ? 0 : 1;

  mask = configp->securid_mask;

  xlate_vdata->func = VdataFunc;
  xlate_vdata->trig = VdataTrig;

  if ( vdata & ~(0xc5u) ) {
     xlate_vdata->func = VdataFunc;
     xlate_vdata->trig = VdataTrig;
     return 0;
  }

  if ( (xlate_vdata->optflags & MOPT_MAIN) && 
       ((xlate_vdata->ident & mask) == (xlate_vdata->identp[1] & mask)) ) {
     /* MAIN specified and signal on AUX, ignore */
     return 1;
  }
  if ( (xlate_vdata->optflags & MOPT_AUX) && 
       ((xlate_vdata->ident & mask) == (xlate_vdata->identp[0] & mask)) ) {
     /* AUX specified and signal on MAIN, ignore */
     return 1;
  }


#if 0
  if ( !(xlate_vdata->optflags & (MOPT_MAIN | MOPT_AUX))  &&
        (xlate_vdata->nident == 2) ) {
     /* Neither specified - add appropriate flag */
     xlate_vdata->vflags |=
         ((xlate_vdata->ident & mask) == (xlate_vdata->identp[0] & mask)) ? SEC_MAIN : SEC_AUX ;
  }
#endif


   if ( xlate_vdata->nident == 2 ) {
      if ( !(xlate_vdata->optflags & (MOPT_MAIN | MOPT_AUX)) ) {
         xlate_vdata->vflags |=
           ((xlate_vdata->ident & mask) == (xlate_vdata->identp[0] & mask)) ? SEC_MAIN : SEC_AUX ;
      }
      else if ( xlate_vdata->optflags & MOPT_MAIN ) {
         xlate_vdata->vflags |= SEC_MAIN;
      }
      else {
         xlate_vdata->vflags |= SEC_AUX;
      }
   }


  if ( vdata & 0x40 ) {
     xlate_vdata->func = TamperFunc;
     xlate_vdata->trig = TamperTrig;
//     xlate_vdata->vflags |= SEC_TAMPER;
  }
  else if ( vdata & 0x80 ) {
     xlate_vdata->func = polarity ? ClearFunc : AlertFunc;
     xlate_vdata->trig = polarity ? ClearTrig : AlertTrig;
  }
  else {
     xlate_vdata->func = polarity ? AlertFunc : ClearFunc;
     xlate_vdata->trig = polarity ? AlertTrig : ClearTrig;
  }

  if ( xlate_vdata->func == AlertFunc )
     xlate_vdata->vflags |= SEC_FAULT;

  xlate_vdata->vflags |= (vdata & 0x04) ? SEC_MIN : SEC_MAX;
  xlate_vdata->vflags |= (vdata & 0x01) ? SEC_LOBAT : 0;

  return 0;
}

/*---------------------------------------------------------------------+
 | Process received vdata for SD10 Security Smoke Detector             |
 +---------------------------------------------------------------------*/
int fn_sd10 ( struct xlate_vdata_st *xlate_vdata )
{

  unsigned int vdata;

  vdata = xlate_vdata->vdata;

  xlate_vdata->vflags = 0;

  if ( vdata == 0x26u ) {
     xlate_vdata->func = AlertFunc;
     xlate_vdata->trig = AlertTrig;
  }
  else {
     xlate_vdata->func = VdataFunc;
     xlate_vdata->trig = VdataTrig;
  }

  return 0;
}

/*---------------------------------------------------------------------+
 | Process received vdata for BMB-SD18 SD10 Security Smoke Detector    |
 +---------------------------------------------------------------------*/
int fn_bmb_sd18 ( struct xlate_vdata_st *xlate_vdata )
{

   unsigned int vdata;

   vdata = xlate_vdata->vdata;

   xlate_vdata->vflags = 0;

   xlate_vdata->vflags |= (vdata & 0x01) ? SEC_LOBAT : 0;

   switch ( vdata & ~(0x01u) ) {
      case 0x26u : /* "R" switch position */
         xlate_vdata->func = AlertFunc;
         xlate_vdata->trig = AlertTrig;
         break;
      case 0x66u : /* "R" switch position */
         xlate_vdata->func = ClearFunc;
         xlate_vdata->trig = ClearTrig;
         break;
      case 0x04u : /* "S" switch position */
         xlate_vdata->func = AlertFunc;
         xlate_vdata->trig = AlertTrig;
         break;
      case 0x80u : /* "S" switch position */
         xlate_vdata->func = ClearFunc;
         xlate_vdata->trig = ClearTrig;
         break;
      case 0x44u : /* Heartbeat, either "R" or "S" switch position */
         xlate_vdata->func = ClearFunc;
         xlate_vdata->trig = ClearTrig;
         break;
      default : /* Undefined codes */
         xlate_vdata->func = VdataFunc;
         xlate_vdata->trig = VdataTrig;
         break;
   }

   return 0;
}

/*---------------------------------------------------------------------+
 | Process received vdata for KR15A "Big Red Button"                   |
 +---------------------------------------------------------------------*/
int fn_kr15a ( struct xlate_vdata_st *xlate_vdata )
{

  unsigned int vdata;

  vdata = xlate_vdata->vdata;

  xlate_vdata->vflags = 0;

  if ( vdata == 0x03u ) {
     xlate_vdata->func = PanicFunc;
     xlate_vdata->trig = PanicTrig;
  }
  else {
     xlate_vdata->func = VdataFunc;
     xlate_vdata->trig = VdataTrig;
  }

  return 0;
}

/*---------------------------------------------------------------------+
 | Process received vdata for generic X10 security remote              |
 +---------------------------------------------------------------------*/
int fn_svdata ( struct xlate_vdata_st *xlate_vdata )
{

  xlate_vdata->vflags = 0;
  xlate_vdata->func = VdataFunc;
  xlate_vdata->trig = VdataTrig;

  return 0;
}

/*---------------------------------------------------------------------+
 | Process received vdata for SD90 Security Smoke Detector             |
 +---------------------------------------------------------------------*/
int fn_sd90 ( struct xlate_vdata_st *xlate_vdata )
{

  unsigned int vdata;

  vdata = xlate_vdata->vdata;

  xlate_vdata->vflags = 0;


  if ( vdata != 0x26u && vdata != 0x27u && (vdata & ~0x85u) ) {
     xlate_vdata->func = VdataFunc;
     xlate_vdata->trig = VdataTrig;
     return 0;
  }

  xlate_vdata->vflags |= (vdata & 0x01u) ? SEC_LOBAT : 0;

  if ( (vdata & 0x26u) == 0x26u ) {
     xlate_vdata->func = TestFunc;
     xlate_vdata->trig = TestTrig;
  }
  else if ( (vdata & 0x84u) == 0x84u ) {
     xlate_vdata->func = ClearFunc;
     xlate_vdata->trig = ClearTrig;
  }
  else {
     xlate_vdata->func = AlertFunc;
     xlate_vdata->trig = AlertTrig;
  }

  return 0;
}

/*---------------------------------------------------------------------+
 | Process received vdata for MS90 Security Motion Sensor              |
 +---------------------------------------------------------------------*/
int fn_ms90 ( struct xlate_vdata_st *xlate_vdata )
{

  unsigned int vdata;

  vdata = xlate_vdata->vdata;

  xlate_vdata->vflags = 0;


  if ( (vdata & 0x0Cu) != 0x0Cu || vdata & ~(0xCDu) ) {
     xlate_vdata->func = VdataFunc;
     xlate_vdata->trig = VdataTrig;
     return 0;
  }

  xlate_vdata->vflags |= (vdata & 0x01u) ? SEC_LOBAT : 0;
//  xlate_vdata->vflags |= (vdata & 0x40u) ? SEC_TAMPER : 0;

  if ( vdata & 0x40u ) {
     xlate_vdata->func = TamperFunc;
     xlate_vdata->trig = TamperTrig;
  }
  else if ( vdata & 0x80u ) {
     xlate_vdata->func = ClearFunc;
     xlate_vdata->trig = ClearTrig;
  }
  else {
     xlate_vdata->func = AlertFunc;
     xlate_vdata->trig = AlertTrig;
  }

  return 0;
}

/*---------------------------------------------------------------------+
 | Process received vdata for MS10A Security Motion Sensor             |
 +---------------------------------------------------------------------*/
int fn_ms10a ( struct xlate_vdata_st *xlate_vdata )
{

  unsigned int vdata;

  vdata = xlate_vdata->vdata;

  xlate_vdata->vflags = 0;


  if ( (vdata & 0x0cu) != 0x0cu || vdata & ~(0x8du) ) {
     xlate_vdata->func = VdataFunc;
     xlate_vdata->trig = VdataTrig;
     return 0;
  }

  xlate_vdata->vflags |= (vdata & 0x01u) ? SEC_LOBAT : 0;

  if ( vdata & 0x80u ) {
     xlate_vdata->func = ClearFunc;
     xlate_vdata->trig = ClearTrig;
  }
  else {
     xlate_vdata->func = AlertFunc;
     xlate_vdata->trig = AlertTrig;
  }

  return 0;
}

/*---------------------------------------------------------------------+
 | Process received vdata for SH624 Security Remote Control            |
 +---------------------------------------------------------------------*/
int fn_sh624 ( struct xlate_vdata_st *xlate_vdata )
{

  unsigned int vdata;

  vdata = xlate_vdata->vdata;

  xlate_vdata->vflags = 0;


   if ( !(vdata & 0x02) || (vdata & 0x11) ) {
      xlate_vdata->func = VdataFunc;
      xlate_vdata->trig = VdataTrig;
      return 0;
   }

   if ( vdata & 0x20 ) {
      xlate_vdata->func = PanicFunc;
      xlate_vdata->trig = PanicTrig;
   }
   else if ( vdata & 0x40 ) {
      if ( vdata & 0x80 ) {
         xlate_vdata->func = SecLightsOffFunc;
         xlate_vdata->trig = SecLightsOffTrig;
      }
      else {
         xlate_vdata->func = SecLightsOnFunc;
         xlate_vdata->trig = SecLightsOnTrig;
      }
   }
   else {
      if ( vdata & 0x80 ) {
         xlate_vdata->func = DisarmFunc;
         xlate_vdata->trig = DisarmTrig;
      }
      else {
         xlate_vdata->func = ArmFunc;
         xlate_vdata->trig = ArmTrig;
         xlate_vdata->vflags |= ((vdata & 0x08) ? SEC_HOME : SEC_AWAY);
         xlate_vdata->vflags |= ((vdata & 0x04) ? SEC_MIN : SEC_MAX);
      }
   }
   return 0;
}
      
/*---------------------------------------------------------------------+
 | Process received vdata for KR10A Security Keychain Remote Control   |
 +---------------------------------------------------------------------*/
int fn_kr10a ( struct xlate_vdata_st *xlate_vdata )
{

  unsigned int vdata;

  vdata = xlate_vdata->vdata;

  xlate_vdata->vflags = 0;


   if ( (vdata & 0x0f) != 0x06 || (vdata & 0x11) ) {
      xlate_vdata->func = VdataFunc;
      xlate_vdata->trig = VdataTrig;
      return 0;
   }

   if ( vdata & 0x20 ) {
      xlate_vdata->func = PanicFunc;
      xlate_vdata->trig = PanicTrig;
   }
   else if ( vdata & 0x40 ) {
      if ( vdata & 0x80 ) {
         xlate_vdata->func = SecLightsOffFunc;
         xlate_vdata->trig = SecLightsOffTrig;
      }
      else {
         xlate_vdata->func = SecLightsOnFunc;
         xlate_vdata->trig = SecLightsOnTrig;
      }
   }
   else {
      if ( vdata & 0x80 ) {
         if ( xlate_vdata->optflags2 & MOPT2_DUMMY ) {
            xlate_vdata->func = ClearFunc;
            xlate_vdata->trig = ClearTrig;
         }
         else {
            xlate_vdata->func = DisarmFunc;
            xlate_vdata->trig = DisarmTrig;
         }
      }
      else {
         if ( xlate_vdata->optflags2 & MOPT2_DUMMY ) {
            xlate_vdata->func = AlertFunc;
            xlate_vdata->trig = AlertTrig;
         }
         else {
            xlate_vdata->func = ArmFunc;
            xlate_vdata->trig = ArmTrig;
            xlate_vdata->vflags |= (xlate_vdata->optflags2 & MOPT2_SWHOME) ? SEC_HOME : SEC_AWAY;
            xlate_vdata->vflags |= (xlate_vdata->optflags2 & MOPT2_SWMAX) ? SEC_MAX : SEC_MIN;
         }
      }
   }
   return 0;
}
      
/*---------------------------------------------------------------------+
 | Process received vdata for Marmitek KR18 Security Keychain Remote   |
 +---------------------------------------------------------------------*/
int fn_kr18 ( struct xlate_vdata_st *xlate_vdata )
{
   xlate_vdata->vflags = 0;

   switch ( xlate_vdata-> vdata ) {
      case 0x26 :
         xlate_vdata->func = PanicFunc;
         xlate_vdata->trig = PanicTrig;
         break;
      case 0x06 :
         if ( xlate_vdata->optflags2 & MOPT2_DUMMY ) {
            xlate_vdata->func = AlertFunc;
            xlate_vdata->trig = AlertTrig;
         }
         else {
            xlate_vdata->func = ArmFunc;
            xlate_vdata->trig = ArmTrig;
            xlate_vdata->vflags |= (xlate_vdata->optflags2 & MOPT2_SWHOME) ? SEC_HOME : SEC_AWAY;
            xlate_vdata->vflags |= (xlate_vdata->optflags2 & MOPT2_SWMAX) ? SEC_MAX : SEC_MIN;
         }
         break;
      case 0x86 :
         if ( xlate_vdata->optflags2 & MOPT2_DUMMY ) {
            xlate_vdata->func = ClearFunc;
            xlate_vdata->trig = ClearTrig;
         }
         else {
            xlate_vdata->func = DisarmFunc;
            xlate_vdata->trig = DisarmTrig;
         }
         break;
      case 0x42 :
         xlate_vdata->func = AkeyOnFunc;
         xlate_vdata->trig = AkeyOnTrig;
         break;
      case 0xC2 :
         xlate_vdata->func = AkeyOffFunc;
         xlate_vdata->trig = AkeyOffTrig;
         break;
      case 0x46 :
         xlate_vdata->func = BkeyOnFunc;
         xlate_vdata->trig = BkeyOnTrig;
         break;
      case 0xC6 :
         xlate_vdata->func = BkeyOffFunc;
         xlate_vdata->trig = BkeyOffTrig;
         break;
      default :
         xlate_vdata->func = VdataFunc;
         xlate_vdata->trig = VdataTrig;
         break;
   }
   return 0;
}
      
/*---------------------------------------------------------------------+
 | Process received vdata for UR81A Entertainment Remote Control       |
 +---------------------------------------------------------------------*/
int fn_ur81a ( struct xlate_vdata_st *xlate_vdata )
{

   xlate_vdata->vflags = 0;
   xlate_vdata->func = VdataFunc;
   xlate_vdata->trig = VdataTrig;

   return 0;
}
      
/*---------------------------------------------------------------------+
 | Process received vdata for General Universal Remote Unit (GURU)     |
 +---------------------------------------------------------------------*/
int fn_guru ( struct xlate_vdata_st *xlate_vdata )
{

   xlate_vdata->vflags = 0;
   xlate_vdata->func = VdataFunc;
   xlate_vdata->trig = VdataTrig;

   return 0;
}

/*---------------------------------------------------------------------+
 | Process received vdata for RFXSensor                                |
 +---------------------------------------------------------------------*/
int fn_rfxsensor ( struct xlate_vdata_st *xlate_vdata )
{

   if ( (xlate_vdata->lobyte) & 0x10u && xlate_vdata->hibyte != 0x02u ) {
      xlate_vdata->func = RFXOtherFunc;
      xlate_vdata->trig = RFXOtherTrig;
      return 0;
   }

   switch ( xlate_vdata->ident % 0x04u ) {
      case 0 :
         xlate_vdata->func = RFXTempFunc;
         xlate_vdata->trig = RFXTempTrig;
         break;
      case 1 :
         if ( xlate_vdata->optflags & MOPT_RFXRH ) {
            xlate_vdata->func = RFXHumidFunc;
            xlate_vdata->trig = RFXHumidTrig;
         }
         else if ( xlate_vdata->optflags & MOPT_RFXBP ) {
            xlate_vdata->func = RFXPressFunc;
            xlate_vdata->trig = RFXPressTrig;
         }
         else if ( xlate_vdata->optflags & MOPT_RFXPOT ) {
            xlate_vdata->func = RFXPotFunc;
            xlate_vdata->trig = RFXPotTrig;
         }
         else {
            xlate_vdata->func = RFXVadFunc;
            xlate_vdata->trig = RFXVadTrig;
         }
         break;
      case 2 :
         if ( xlate_vdata->lobyte & 0x10u ) {
            xlate_vdata->func = RFXLoBatFunc;
            xlate_vdata->trig = RFXLoBatTrig;
         }
         else {
            xlate_vdata->func = RFXVsFunc;
            xlate_vdata->trig = RFXVsTrig;
         }
         break;
      default :
         break;
   }

   return 0;
}
   
/*---------------------------------------------------------------------+
 | Process received vdata for RFXMeter Pulse                           |
 +---------------------------------------------------------------------*/
int fn_rfxpulse ( struct xlate_vdata_st *xlate_vdata )
{
   xlate_vdata->vflags = 0;
   xlate_vdata->func = RFXPulseFunc;
   xlate_vdata->trig = RFXPulseTrig;

   return 0;
}

/*---------------------------------------------------------------------+
 | Process received vdata for RFXMeter Counter                         |
 +---------------------------------------------------------------------*/
int fn_rfxcount ( struct xlate_vdata_st *xlate_vdata )
{
   xlate_vdata->vflags = 0;
   xlate_vdata->func = RFXCountFunc;
   xlate_vdata->trig = RFXCountTrig;

   return 0;
}

/*---------------------------------------------------------------------+
 | Process received vdata for RFXMeter Power                           |
 +---------------------------------------------------------------------*/
int fn_rfxpower ( struct xlate_vdata_st *xlate_vdata )
{
   xlate_vdata->vflags = 0;
   xlate_vdata->func = RFXPowerFunc;
   xlate_vdata->trig = RFXPowerTrig;

   return 0;
}

/*---------------------------------------------------------------------+
 | Process received vdata for RFXMeter Water                           |
 +---------------------------------------------------------------------*/
int fn_rfxwater ( struct xlate_vdata_st *xlate_vdata )
{
   xlate_vdata->vflags = 0;
   xlate_vdata->func = RFXWaterFunc;
   xlate_vdata->trig = RFXWaterTrig;

   return 0;
}

/*---------------------------------------------------------------------+
 | Process received vdata for RFXMeter Pulse                           |
 +---------------------------------------------------------------------*/
int fn_rfxgas ( struct xlate_vdata_st *xlate_vdata )
{
   xlate_vdata->vflags = 0;
   xlate_vdata->func = RFXGasFunc;
   xlate_vdata->trig = RFXGasTrig;

   return 0;
}

int fn_visonic ( struct xlate_vdata_st *xlate_vdata )
{

   xlate_vdata->func = VdataFunc;
   xlate_vdata->trig = VdataTrig;
   xlate_vdata->vflags = 0;

   return 0;
}

/*---------------------------------------------------------------------+
 | Check the compatibility and order of DS90 IDs.  After bit reversal, |
 | each byte of the aux ID must be the corresponding byte of the       |
 | primary ID + 1.                                                     |
 | Reverse the order if necessary to get compatibility.                |
 | Return 0 if OK or 1 if the IDs don't have a valid relationship.     |
 +---------------------------------------------------------------------*/
int check_ds90_ids( unsigned /*short*/ long *identp )
{
   unsigned short mid, mid1, midlo, midhi, mask, temp;
   unsigned short aid, aid1, aidlo, aidhi;

   mask = (identp[0] > 0xffu || identp[1] > 0xffu) ? 0xffffu : 0x00ffu;

   midlo = rev_byte_bits( identp[0] & 0x00ffu);
   midhi = rev_byte_bits((identp[0] & 0xff00u & mask) >> 8) << 8;
   aidlo = rev_byte_bits( identp[1] & 0x00ffu);
   aidhi = rev_byte_bits((identp[1] & 0xff00u & mask) >> 8) << 8;

   mid = midhi | midlo;
   aid = aidhi | aidlo;

   mid1 = (((midhi + 0x0100u) & 0xff00u) | ((midlo + 0x0001u) & 0x00ffu)) & mask;
   aid1 = (((aidhi + 0x0100u) & 0xff00u) | ((aidlo + 0x0001u) & 0x00ffu)) & mask;

   if ( aid == mid1 ) {
      /* Order is OK */
      return 0;
   }
   
   if ( aid1 == mid ) {
      /* Reverse the order */
      temp = identp[0];
      identp[0] = identp[1];
      identp[1] = temp;
      return 0;
   }

   /* Invalid relationship */   
   return 1;
}

/*---------------------------------------------------------------------+
 | Determine the companion ID for a DS90 ID.  If func = COMPUTE_AUX,   |
 | find the aux ID for the argument main ID; otherwise find the main   |
 | ID for the argument aux ID.                                         |
 +---------------------------------------------------------------------*/
unsigned short comp_ds90_id ( unsigned short ident, int func )
{
   unsigned short mask, newid;
   unsigned char  idlo, idhi, id1lo, id1hi;

   mask = (ident > 0xffu) ? 0xffffu : 0x00ffu;

   idlo = rev_byte_bits(ident & 0x00ffu);
   idhi = rev_byte_bits((ident & 0xff00u & mask) >> 8);

   ident = rev_byte_bits(ident & 0xffu);

   if ( func == COMPUTE_AUX ) {
      /* Get aux corresponding to main */
      id1hi = ((idhi + 0x0001u) & 0x00ffu) & (mask >> 8);
      id1lo = ((idlo + 0x0001u) & 0x00ffu);
   }
   else {
      /* Get main corresponding to aux */
      id1hi = ((idhi - 0x0001u) & 0x00ffu) & (mask >> 8);
      id1lo = ((idlo - 0x0001u) & 0x00ffu);
   }
   newid = (rev_byte_bits(id1hi) << 8) | rev_byte_bits(id1lo);
   return newid;
}


int cmp_modules ( const void *m1, const void *m2 )
{
   struct modules_st *one, *two;
   
   one = (struct modules_st *)m1; two = (struct modules_st *)m2;

   return strcmp(one->label, two->label);
}

int c_modlist ( int argc, char *argv[] )
{
   int j;
   qsort(modules, ntypes, sizeof(struct modules_st), cmp_modules);

   for ( j = 0; j < ntypes; j++ )
      printf("%s\n", modules[j].label);
   printf("\n");

   return 0;
}


/*---------------------------------------------------------------------+
 | Get a temperature parameter from the tokenized ALIAS directive for  |
 | a sensor.                                                           |
 +---------------------------------------------------------------------*/
int temp_parm ( char **tokens, int *ntokens, char *tparmname, char *tscale,
                                             int *tflag, double *tvalue )
{
   char msg[80];
   int  j, k, count = 0;
   char *sp, *tmptok;

   *tflag = 0;

   j = 0;
   while ( j < *ntokens ) {
      strupper(tokens[j]);
      if ( strcmp(tokens[j], tparmname) == 0 ) {
         *tokens[j] = '\0';
         j++;
         if ( j >= *ntokens ) {
            sprintf(msg, "Missing %s value.", tparmname);
            store_error_message(msg);
            return 1;
         }
         strupper(tokens[j]);
         *tvalue = strtod(tokens[j], &sp);
         if ( strchr("CFKR", *sp) && strchr(" \t\n\r", *(sp + 1)) ) {
            *tscale = *sp;
         }
         else if ( !strchr(" \t\n\r", *sp) ) {
            sprintf(msg, "Invalid %s value or scale.", tparmname);
            store_error_message(msg);
            return 1;
         }
         *tokens[j] = '\0';
         *tflag = 1;
         count += 2;
      }
      j++;
   }

   if ( count == 0 )
      return 0;

   /* Compact to remove "gaps" where tokens were nulled out */

   for ( j = 0; j < *ntokens; j++ ) {
      if ( *tokens[j] == '\0' ) {
         for ( k = j + 1; k < *ntokens; k++ ) {
            if ( *tokens[k] != '\0' ) {
               tmptok = tokens[j];
               tokens[j] = tokens[k];
               tokens[k] = tmptok;
               *tmptok = '\0';
               break;
            }
         }
      }
   }
   *ntokens -= count;

   return 0;
}

/*---------------------------------------------------------------------+
 | Get a relative humidity parameter from the tokenized ALIAS          |
 | directive for a sensor.                                             |
 +---------------------------------------------------------------------*/
int rh_parm ( char **tokens, int *ntokens, char *rhparmname, char *rhscale,
                                             int *rhflag, double *rhvalue )
{
   char msg[80];
   int  j, k, count = 0;
   char *sp, *tmptok;

   *rhflag = 0;

   j = 0;
   while ( j < *ntokens ) {
      strupper(tokens[j]);
      if ( strcmp(tokens[j], rhparmname) == 0 ) {
         *tokens[j] = '\0';
         j++;
         if ( j >= *ntokens ) {
            sprintf(msg, "Missing %s value.", rhparmname);
            store_error_message(msg);
            return 1;
         }
         strupper(tokens[j]);
         *rhvalue = strtod(tokens[j], &sp);
         *rhscale = '%';
         if ( !strchr(" %\t\n\r", *sp) ) {
            sprintf(msg, "Invalid %s value.", rhparmname);
            store_error_message(msg);
            return 1;
         }
         *tokens[j] = '\0';
         *rhflag = 1;
         count += 2;
      }
      j++;
   }

   if ( count == 0 )
      return 0;

   /* Compact to remove "gaps" where tokens were nulled out */

   for ( j = 0; j < *ntokens; j++ ) {
      if ( *tokens[j] == '\0' ) {
         for ( k = j + 1; k < *ntokens; k++ ) {
            if ( *tokens[k] != '\0' ) {
               tmptok = tokens[j];
               tokens[j] = tokens[k];
               tokens[k] = tmptok;
               *tmptok = '\0';
               break;
            }
         }
      }
   }
   *ntokens -= count;

   return 0;
}

/*---------------------------------------------------------------------+
 | Get a barometric pressure parameter from the tokenized ALIAS        |
 | directive for a sensor.                                             |
 +---------------------------------------------------------------------*/
int bp_parm ( char **tokens, int *ntokens, char *bpparmname, char *bpunits,
                                             int *bpflag, double *bpvalue )
{
   char msg[80];
   int  j, k, count = 0;
   char *sp, *tmptok;

   *bpflag = 0;

   j = 0;
   while ( j < *ntokens ) {
      strupper(tokens[j]);
      if ( strcmp(tokens[j], bpparmname) == 0 ) {
         *tokens[j] = '\0';
         j++;
         if ( j >= *ntokens ) {
            sprintf(msg, "Missing %s value.", bpparmname);
            store_error_message(msg);
            return 1;
         }
         strupper(tokens[j]);
         *bpvalue = strtod(tokens[j], &sp);
         if ( strchr(" \t\n\r", *sp) )
            *bpunits = '\0';
         else
            strncpy(bpunits, sp, NAME_LEN);

         *tokens[j] = '\0';
         *bpflag = 1;
         count += 2;
      }
      j++;
   }

   if ( count == 0 )
      return 0;

   /* Compact to remove "gaps" where tokens were nulled out */

   for ( j = 0; j < *ntokens; j++ ) {
      if ( *tokens[j] == '\0' ) {
         for ( k = j + 1; k < *ntokens; k++ ) {
            if ( *tokens[k] != '\0' ) {
               tmptok = tokens[j];
               tokens[j] = tokens[k];
               tokens[k] = tmptok;
               *tmptok = '\0';
               break;
            }
         }
      }
   }
   *ntokens -= count;

   return 0;
}

/*---------------------------------------------------------------------+
 | Get inactive_timeout parameter from the tokenized ALIAS directive   |
 | for a sensor.                                                       |
 +---------------------------------------------------------------------*/
int timeout_parm ( char **tokens, int *ntokens, char *parmname,
                                             int *toflag, long *timeout )
{
   char msg[80];
   int  j, k, count = 0;
   char *totok;

   *toflag = 0;

   j = 0;
   while ( j < *ntokens ) {
      strupper(tokens[j]);
      if ( strcmp(tokens[j], parmname) == 0 ) {
         *tokens[j] = '\0';
         j++;
         if ( j >= *ntokens ) {
            sprintf(msg, "Missing %s hh:mm:ss value.", parmname);
            store_error_message(msg);
            return 1;
         }
         if ( (*timeout = parse_hhmmss(tokens[j], 3)) < 0 || *timeout > 86400L ) {
            sprintf(msg, "Invalid %s timeout '%s'.", parmname, tokens[j]);
            store_error_message(msg);
            return 1;
         }

         *tokens[j] = '\0';
         *toflag = 1;
         count += 2;
      }
      j++;
   }

   if ( count == 0 )
      return 0;

   /* Compact to remove "gaps" where tokens were nulled out */

   for ( j = 0; j < *ntokens; j++ ) {
      if ( *tokens[j] == '\0' ) {
         for ( k = j + 1; k < *ntokens; k++ ) {
            if ( *tokens[k] != '\0' ) {
               totok = tokens[j];
               tokens[j] = tokens[k];
               tokens[k] = totok;
               *totok = '\0';
               break;
            }
         }
      }
   }
   *ntokens -= count;

   return 0;
}


int sensor_timeout ( ALIAS *aliasp, int aliasindex, char **tokens, int *ntokens )
{
   long timeout;
   int  toflag;

   int timeout_parm ( char **, int *, char *, int *, long * );

   if ( timeout_parm(tokens, ntokens, "IATO", &toflag, &timeout) != 0 )
      return 1;

   if ( toflag ) {
      aliasp[aliasindex].optflags2 |= MOPT2_IATO;
      aliasp[aliasindex].hb_timeout = timeout;
   }

   return 0;
}



/*-------------------------------------------------------+
 | Called by add_module_options() to add the KAKU        |
 | sensor options from the ALIAS line in the config file.|
 +-------------------------------------------------------*/ 
int opt_kaku_orig ( ALIAS *aliasp, int aliasindex, char **tokens, int *ntokens )
{
   long ident;
   int  key, grp;
   char *sp;

   if ( *ntokens < 2 || *ntokens > 3 ) {
      store_error_message("Unknown parameters");
      return 1;
   }

   if ( single_bmap_unit(aliasp[aliasindex].unitbmap) == 0xff ) {
      store_error_message("Multiple unit alias is invalid for sensors.");
      return 1;
   }

   ident = strtol(tokens[0], &sp, 16);
   if ( *sp == '\0' && ident >= 0 ) {
      aliasp[aliasindex].ident[0] = ident;
      aliasp[aliasindex].nident = 1;
   }
   else {
      store_error_message("Invalid hexadecimal KAKU ID parameter");
      return 1;
   }

   key = strtol(tokens[1], &sp, 10);
   if ( *sp == '\0' && key > 0 && key <= 16 ) {
      aliasp[aliasindex].kaku_keymap[0] = 1 << (key - 1);
   }
   else {
      store_error_message("Invalid KAKU key parameter");
      return 1;
   }

   if ( *ntokens == 3 ) {
      grp = toupper((int)((unsigned char)(*tokens[2])));
      if ( strlen(tokens[2]) == 1 && grp >= 'A' && grp <= 'P' ) {
         aliasp[aliasindex].kaku_grpmap[0] = 1 << (grp - 'A');
      }
      else {
         store_error_message("Invalid KAKU group parameter");
         return 1;
      }
   }
      
   aliasp[aliasindex].vtype = RF_KAKU;

   aliasp[aliasindex].optflags = MOPT_KAKU;

   return 0;
}

/*-------------------------------------------------------+
 | Called by add_module_options() to add the KAKU        |
 | sensor options from the ALIAS line in the config file.|
 +-------------------------------------------------------*/ 
int opt_kaku ( ALIAS *aliasp, int aliasindex, char **tokens, int *ntokens )
{
   long ident;
   int  nident, key, grp;
   int  j;
   char *sp;

   if ( *ntokens < 2 ) {
      store_error_message("Too few parameters");
      return 1;
   }
   else if ( *ntokens > 12 ) {
      store_error_message("Too many parameters");
      return 1;
   }
   else if ( ((*ntokens) % 2) != 0 ) {
      store_error_message("Parameter syntax: <ID> <Key|Grp> [<ID> <Key|Grp> [...]]");
      return 1;
   }
      

   if ( single_bmap_unit(aliasp[aliasindex].unitbmap) == 0xff ) {
      store_error_message("Multiple unit alias is invalid for sensors.");
      return 1;
   }

   nident = 0;
   for ( j = 0; j < *ntokens; j += 2 ) {
      ident = strtol(tokens[j], &sp, 16);
      if ( strchr(" \t", *sp) && ident >= 0 ) {
         aliasp[aliasindex].ident[nident] = ident;
      }
      else {
         store_error_message("Invalid hexadecimal KAKU ID parameter");
         return 1;
      }

      if ( isdigit((int)((unsigned char)(*tokens[j+1]))) ) {
         key = strtol(tokens[j+1], &sp, 10);
         if ( key > 0 && key <= 16 ) {
            aliasp[aliasindex].kaku_keymap[nident] = 1 << (key - 1);
         }
         else {
            store_error_message("Invalid KAKU key parameter");
            return 1;
         }

         if ( strchr(" \t\r\n", *sp) ) {
            nident++;
            continue;
         }

         strupper(sp);
         if ( strlen(sp) == 1 && *sp >= 'A' && *sp <= 'P' ) {
            aliasp[aliasindex].kaku_grpmap[nident] = 1 << (*sp - 'A');
         }
         else {
            store_error_message("Invalid KAKU group parameter");
            return 1;
         }
      }
      else {
         strupper(tokens[j+1]);
         grp = *tokens[j+1];
         if ( strlen(tokens[j+1]) == 1 && grp >= 'A' && grp <= 'P' ) {
            aliasp[aliasindex].kaku_grpmap[nident] = 1 << (grp - 'A');
         }
         else {
            store_error_message("Invalid KAKU group parameter");
            return 1;
         }
      }

      nident++;
   }

   aliasp[aliasindex].nident = nident;

      
   aliasp[aliasindex].vtype = RF_KAKU;

   aliasp[aliasindex].optflags = MOPT_KAKU;

   return 0;
}
