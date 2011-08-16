/*----------------------------------------------------------------------------+
 |                                                                            |
 |                       HEYU Configuration                                   |
 |       Copyright 2002,2003,2004-2008 Charles W. Sullivan                    |
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
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#include <ctype.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif

#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#endif
#include <time.h>

#include "x10.h"
#include "process.h"
#include "sun.h"
#include "x10state.h"

extern int line_no, verbose;
extern int i_am_relay, i_am_aux, i_am_state, i_am_monitor;
extern char heyu_path[PATH_LEN + 1];
extern char alt_path[PATH_LEN + 1];
extern char heyu_script[PATH_LEN + 1];
extern long std_tzone;
extern char default_housecode;
extern char heyu_config[PATH_LEN + 1];
 
extern int  fetch_x10state ( void );
extern int  fetch_x10state_old ( void );
extern struct x10global_st x10global;

char        statefile[PATH_LEN + 1];
char        enginelockfile[PATH_LEN + 1];
char        auxfile[PATH_LEN + 1];

unsigned int transceive_list[16];

unsigned int   ore_ignore[32];
int            ore_ignore_size;

unsigned int   sec_ignore[32];
int            sec_ignore_size;

CONFIG config;
CONFIG *configp = NULL;

enum {
   OldAlias, Alias, UserSyn, Launcher, Script, Scene, RFBursts,
   /* Directives with multiple instances allowed */
   /* must be before 'Separator'.                */ 
   Separator,  
   Tty, TtyAux, HouseCode, ForceAddr,
   NewFormat, ScheduleFile, MaxPParms, RcsTemp, StatusTimeout,
   SpfTimeout, DispExpMac, AckHails, Mode, ProgramDays,
   AsIfDate, AsIfTime, CombineEvents, CompressMacros, FebKluge,
   Latitude, Longitude, DawnOption, DuskOption, DawnSubstitute,
   DuskSubstitute, MinDawn, MaxDawn, MinDusk, MaxDusk, CheckFiles,
   ReportPath, ReplDelay, Ignored, ResvTimers, TrigTag, XrefApp,
   MacTerm, AutoChain, ResOverlap, ModuleTypes, LaunchMode,
   FunctionMode, LaunchSrc, DefaultModule, ScriptShell, ScriptMode,
   LogDir, IsDarkOffset, EnvAliasPrefix, DevType, SunMode, Fix5A,
   AutoFetch, PfailUpdate, BitDelay, DefRFBursts, BurstSpacing,
   TimerTweak, RFPostDelay, RFFarbDelay, RFFarwDelay, DispRFX, LoopCount,
   RestrictDims, StateFmt, /* StateDir, */PfailScript, CM11PostDelay,
   StartEngine, IgnoreSilent, NoSwitch, RespoolPerms, SpoolMax,
   CheckRILine, RIdisable, SendRetries, ScriptCtrl, Transceive, TransMissp,
   RFForward, TransDim, StateCtrl, RFFuncMask, RingCtrl, AuxRepcounts,
   AuxMincountRFX, HideUnchg, HideUnchgInactive, ShowChange, DispRFNoise, ArmMaxDelay, DispRawRF, ArmLogic,
   InactiveTimeout, HeyuUmask, AutoWait, FullBright, EnginePoll,
   RfxTscale, RfxVadscale, RfxBPscale, DispSubdir, RfxComDtrRts, RfxHiBaud,
   RfxPowerScale, RfxWaterScale, RfxGasScale, RfxPulseScale, RfxComEnable, RfxComDisable,
   LockupCheck, TailPath, RfxJam, DispDmxTemp, SecID16, SecIDPar, LogDateYr,
   DmxTscale, OreTscale, OreBPscale, OreWgtscale, OreLowBatt, DispOreAll,
   OreChgBitsT, OreChgBitsRH, OreChgBitsBP, OreChgBitsWgt, OreDataEntry, OreDispChan, OreID16,
   OreDispFcast, LogDateUnix, InactiveTimeoutOre, DispSensorIntv, DateFormat, LockTimeout,
   CM11QueryDelay, ElsNumber, ElsVoltage, ElsChgBitsCurr, OreWindscale, OreWindSensorDir,
   OreRainRatescale, OreRainTotscale, OreDispBatLvl, OreWindDirMode, OreDispCount,
   OreDispBft, OreChgBitsWsp, OreChgBitsWavsp, OreChgBitsWdir, OreChgBitsRrate, OreChgBitsRtot,
   OreChgBitsUV, ScanMode, RfxInline,
   OwlVoltage, OwlCalibPower, OwlCalibEnergy, OwlChgBitsPower, OwlChgBitsEnergy, OwlDispCount,
   ArmRemote, ActiveChange, InactiveHandling, ProcessXmit, ShowFlagsMode,
   LaunchPathAppend, LaunchPathPrefix, FixStopStartError, TtyRFXmit, ChkSumTimeout
   
};

#define CSTP   1    /* Minimal config for stop command */
#define CCAL   2    /* Minimal config for syscheck command */
#define CMLT   4    /* Multiple instances allowed */
#define COVR   8    /* Allow override in sched file */
#define CHLP  16    /* Minimal config for help command */
#define CIGN  32    /* Ignored (usually obsolete) */
#define CHID  64    /* Don't display */

static struct conf {
   char          *name;
   int           mintoken;
   int           maxtoken;
   unsigned char isparsed;
   unsigned char override;  /* Allow override in sched file */
   unsigned int  flags;
   unsigned int  switchval;
} command[] = {
   {"(_alias_)",            3, 3, 0, 0, CMLT,    OldAlias },
   {"ALIAS",                3,16, 0, 0, CMLT,    Alias },
   {"LAUNCHER",             2,-1, 0, 0, CMLT,    Launcher},
   {"SCRIPT",               2,-1, 0, 0, CMLT,    Script },
   {"USERSYN",              2,-1, 0, 0, CMLT,    UserSyn },
   {"SCENE",                2,-1, 0, 0, CMLT,    Scene },
   {"SECTION",              1,-1, 0, 0, COVR|CMLT, IgnoreSilent},
   /* Multiple instances OK for switchval above */
   {"TTY",                  2, 4, 0, 0, CSTP,    Tty },
   {"TTY_AUX",              3, 5, 0, 0, CSTP,    TtyAux},
   {"TTY_RFXMIT",           3, 3, 0, 0, CSTP,    TtyRFXmit},
   {"HOUSECODE",            2, 2, 0, 0, 0,       HouseCode },
   {"FORCE_ADDR",           2, 2, 0, 0, 0,       ForceAddr },
   {"NEWFORMAT",            1, 1, 0, 0, CIGN,    Ignored },
   {"MACROXREF",            2, 2, 0, 0, CIGN,    Ignored },
   {"SCHEDULE_FILE",        2, 2, 0, 0, 0,       ScheduleFile },
   {"MAX_PPARMS",           2, 2, 0, 0, 0,       MaxPParms },
   {"STATUS_TIMEOUT",       2, 2, 0, 0, 0,       StatusTimeout },
   {"SPF_TIMEOUT",          2, 2, 0, 0, 0,       SpfTimeout },
   {"RCS_TEMPERATURE",      2, 2, 0, 0, 0,       RcsTemp },
   {"RCS_DECODE",           2, 2, 0, 0, 0,       RcsTemp },
   {"DISPLAY_EXP_MACROS",   2, 2, 0, 0, CIGN,    Ignored }, 
   {"ACK_HAILS",            2, 2, 0, 0, 0,       AckHails }, 
   {"MODE",                 2, 2, 0, 1, COVR,    Mode },
   {"PROGRAM_DAYS",         2, 2, 0, 1, COVR,    ProgramDays },
   {"ASIF_DATE",            2, 2, 0, 1, COVR,    AsIfDate },
   {"ASIF_TIME",            2, 2, 0, 1, COVR,    AsIfTime },
   {"COMBINE_EVENTS",       2, 2, 0, 1, COVR,    CombineEvents },
   {"COMPRESS_MACROS",      2, 2, 0, 1, COVR,    CompressMacros },
   {"FEB_KLUGE",            2, 2, 0, 1, COVR,    FebKluge },
   {"LATITUDE",             2, 2, 0, 1, COVR,    Latitude },
   {"LONGITUDE",            2, 2, 0, 1, COVR,    Longitude },
   {"DAWN_OPTION",          2, 2, 0, 1, COVR,    DawnOption },
   {"DUSK_OPTION",          2, 2, 0, 1, COVR,    DuskOption },
   {"DAWN_SUBSTITUTE",      2, 2, 0, 1, COVR|CIGN, Ignored },
   {"DUSK_SUBSTITUTE",      2, 2, 0, 1, COVR|CIGN, Ignored },
   {"MIN_DAWN",             2, 2, 0, 1, COVR,    MinDawn },
   {"MAX_DAWN",             2, 2, 0, 1, COVR,    MaxDawn },
   {"MIN_DUSK",             2, 2, 0, 1, COVR,    MinDusk },
   {"MAX_DUSK",             2, 2, 0, 1, COVR,    MaxDusk },
   {"WRITE_CHECK_FILES",    2, 2, 0, 1, COVR,    CheckFiles },
   {"REPORT_PATH",          2, 2, 0, 1, COVR,    ReportPath },
   {"REPL_DELAYED_MACROS",  2, 2, 0, 1, COVR,    ReplDelay },
   {"RESERVED_TIMERS",      2, 2, 0, 1, COVR|CHID, ResvTimers }, /* WIP */
   {"TRIGGER_TAG",          2, 2, 0, 1, COVR|CIGN, Ignored }, 
   {"XREF_APPEND",          2, 2, 0, 1, COVR|CIGN, Ignored }, 
   {"MACTERM",              2, 2, 0, 1, COVR,    MacTerm }, /* WIP */
   {"RESOLVE_OVERLAP",      2, 2, 0, 1, COVR,    ResOverlap },
   {"DAWNDUSK_DEF",         2, 3, 0, 1, COVR,    SunMode },
   {"SCRIPT_MODE",          2, 2, 0, 0, 0,       ScriptMode },
   {"SCRIPT_SHELL",         2, 2, 0, 0, 0,       ScriptShell },
   {"LAUNCH_SOURCE",        2, 6, 0, 0, 0,       LaunchSrc },
   {"DEFAULT_MODULE",       2, 2, 0, 0, 0,       DefaultModule },
   {"LOG_DIR",              2, 3, 0, 0, 0,       LogDir },
   {"DISPLAY_SUBDIR",       2, 2, 0, 0, 0,       DispSubdir },
   {"ISDARK_OFFSET",        2, 2, 0, 0, 0,       IsDarkOffset },
   {"ENV_ALIAS_PREFIX",     2, 2, 0, 0, 0,       EnvAliasPrefix },
   {"FIX_5A",               2, 2, 0, 0, CHID,    Fix5A },
   {"AUTOFETCH",            2, 2, 0, 0, 0,       AutoFetch },
   {"POWERFAIL_UPDATE",     2, 2, 0, 0, 0,       PfailUpdate },
   {"CM17A_BIT_DELAY",      2, 2, 0, 0, 0,       BitDelay },
   {"DEF_RF_BURSTS",        2, 2, 0, 0, 0,       DefRFBursts },
   {"RF_BURSTS",            3,21, 0, 0, 0,       RFBursts},
   {"TIMER_LOOPCOUNT",      2, 2, 0, 0, 0,       LoopCount },
   {"RF_BURST_SPACING",     2, 2, 0, 0, 0,       BurstSpacing },
   {"RF_TIMER_TWEAK",       2, 2, 0, 0, 0,       TimerTweak },
   {"RF_POST_DELAY",        2, 2, 0, 0, 0,       RFPostDelay },
   {"RF_FARB_DELAY",        2, 2, 0, 0, 0,       RFFarbDelay },
   {"RF_FARW_DELAY",        2, 2, 0, 0, 0,       RFFarwDelay },
   {"DISPLAY_RF_XMIT",      2, 2, 0, 0, 0,       DispRFX },
   {"RESTRICT_DIMS",        2, 2, 0, 0, 0,       RestrictDims },
   {"STATE_FORMAT",         2, 2, 0, 0, 0,       StateFmt },
   {"RELAY_POWERFAIL_SCRIPT", 2,-1, 0, 0, CHID,   PfailScript},
   {"CM11_POST_DELAY",      2, 2, 0, 0, 0,       CM11PostDelay},
   {"START_ENGINE",         2, 2, 0, 0, 0,       StartEngine},
   {"RF_NOSWITCH",          2, 2, 0, 0, 0,       NoSwitch},
   {"RESPOOL_PERMISSIONS",  2, 2, 0, 0, CHID,    RespoolPerms},
   {"SPOOLFILE_MAX",        2, 2, 0, 0, 0,       SpoolMax},
   {"CHECK_RI_LINE",        2, 2, 0, 0, 0,       CheckRILine},
   {"RI_DISABLE",           2, 2, 0, 0, 0,       RIdisable},
   {"SEND_RETRIES",         2, 2, 0, 0, 0,       SendRetries},
   {"TRANSCEIVE",           2, 3, 0, 0, 0,       Transceive},
   {"TRANSCIEVE",           2, 3, 0, 0, CHID,    TransMissp},
   {"RFFORWARD",            2, 3, 0, 0, 0,       RFForward},
   {"TRANS_DIMLEVEL",       2, 2, 0, 0, 0,       TransDim},
   {"SCRIPT_CTRL",          2, 2, 0, 0, 0,       ScriptCtrl},
   {"STATE_CTRL",           2, 2, 0, 0, 0,       StateCtrl},
   {"RING_CTRL",            2, 2, 0, 0, 0,       RingCtrl},
   {"RF_FUNCMASK",          2,-1, 0, 0, 0,       RFFuncMask},
   {"AUX_REPCOUNTS",        4, 4, 0, 0, 0,       AuxRepcounts},
   {"AUX_MINCOUNT_RFX",     2, 2, 0, 0, 0,       AuxMincountRFX},
   {"HIDE_UNCHANGED",       2, 2, 0, 0, 0,       HideUnchg},
   {"HIDE_UNCHANGED_INACTIVE", 2, 2, 0, 0, 0,    HideUnchgInactive},
   {"SHOW_CHANGE",          2, 2, 0, 0, 0,       ShowChange},
   {"DISPLAY_RF_NOISE",     2, 2, 0, 0, 0,       DispRFNoise},
   {"ARM_MAX_DELAY",        2, 2, 0, 0, 0,       ArmMaxDelay},
   {"DISPLAY_RAW_RF",       2, 2, 0, 0, 0,       DispRawRF},
   {"ARM_LOGIC",            2, 2, 0, 0, 0,       ArmLogic},
   {"INACTIVE_TIMEOUT",     2, 2, 0, 0, 0,       InactiveTimeout},
   {"HEYU_UMASK",           2, 2, 0, 0, 0,       HeyuUmask},
   {"AUTO_WAIT",            2, 2, 0, 0, 0,       AutoWait},
   {"FULL_BRIGHT",          2, 2, 0, 0, 0,       FullBright},
   {"ENGINE_POLL",          2, 2, 0, 0, 0,       EnginePoll},
   {"DMX_TSCALE",           2, 3, 0, 0, 0,       DmxTscale},
   {"ORE_LOWBATTERY",       2, 2, 0, 0, 0,       OreLowBatt},
   {"ORE_TSCALE",           2, 3, 0, 0, 0,       OreTscale},
   {"ORE_BPSCALE",          3, 4, 0, 0, 0,       OreBPscale},
   {"ORE_WGTSCALE",         3, 3, 0, 0, 0,       OreWgtscale},
   {"ORE_WINDSCALE",        3, 3, 0, 0, 0,       OreWindscale},
   {"ORE_WINDSENSORDIR",    2, 2, 0, 0, 0,       OreWindSensorDir},
   {"ORE_WINDDIR_MODE",     2, 2, 0, 0, 0,       OreWindDirMode},
   {"ORE_RAINRATESCALE",    3, 3, 0, 0, 0,       OreRainRatescale},
   {"ORE_RAINTOTSCALE",     3, 3, 0, 0, 0,       OreRainTotscale},
   {"RFX_TSCALE",           2, 3, 0, 0, 0,       RfxTscale},
   {"RFX_VADSCALE",         3, 4, 0, 0, 0,       RfxVadscale},
   {"RFX_BPSCALE",          3, 4, 0, 0, 0,       RfxBPscale},
   {"RFXCOM_DTR_RTS",       2, 2, 0, 0, 0,       RfxComDtrRts},
   {"RFXCOM_HIBAUD",        2, 2, 0, 0, CHID,    RfxHiBaud},
   {"RFX_POWERSCALE",       3, 3, 0, 0, 0,       RfxPowerScale},
   {"RFX_WATERSCALE",       3, 3, 0, 0, 0,       RfxWaterScale},
   {"RFX_GASSCALE",         3, 3, 0, 0, 0,       RfxGasScale},
   {"RFX_PULSESCALE",       3, 3, 0, 0, 0,       RfxPulseScale},
   {"RFXCOM_ENABLE",        2, 4, 0, 0, 0,       RfxComEnable},
   {"RFXCOM_DISABLE",       2,16, 0, 0, 0,       RfxComDisable},
   {"LOCKUP_CHECK",         2, 2, 0, 0, CHID,    LockupCheck},
   {"TAILPATH",             2, 2, 0, 0, 0,       TailPath},
   {"SUPPRESS_RFXJAM",      2, 2, 0, 0, 0,       RfxJam},
   {"DISPLAY_DMXTEMP",      2, 2, 0, 0, 0,       DispDmxTemp},
   {"SECURID_16",           2, 2, 0, 0, 0,       SecID16},
   {"SECURID_PARITY",       2, 2, 0, 0, 0,       SecIDPar},
   {"LOGDATE_YEAR",         2, 2, 0, 0, 0,       LogDateYr},
   {"DISPLAY_ORE_ALL",      2, 2, 0, 0, 0,       DispOreAll},
   {"ORE_CHGBITS_T",        2, 2, 0, 0, 0,       OreChgBitsT},
   {"ORE_CHGBITS_RH",       2, 2, 0, 0, 0,       OreChgBitsRH},
   {"ORE_CHGBITS_BP",       2, 2, 0, 0, 0,       OreChgBitsBP},
   {"ORE_CHGBITS_WGT",      2, 2, 0, 0, 0,       OreChgBitsWgt},
   {"ORE_CHGBITS_WSP",      2, 2, 0, 0, CIGN,    Ignored},
   {"ORE_CHGBITS_WINDSP",   2, 2, 0, 0, 0,       OreChgBitsWsp},
   {"ORE_CHGBITS_WAVSP",    2, 2, 0, 0, CIGN,    Ignored},
   {"ORE_CHGBITS_WINDAVSP", 2, 2, 0, 0, 0,       OreChgBitsWavsp},
   {"ORE_CHGBITS_WDIR",     2, 2, 0, 0, CIGN,    Ignored},
   {"ORE_CHGBITS_WINDDIR",  2, 2, 0, 0, 0,       OreChgBitsWdir},
   {"ORE_CHGBITS_RRATE",    2, 2, 0, 0, CIGN,    Ignored},
   {"ORE_CHGBITS_RAINRATE", 2, 2, 0, 0, 0,       OreChgBitsRrate},
   {"ORE_CHGBITS_RTOT",     2, 2, 0, 0, CIGN,    Ignored},
   {"ORE_CHGBITS_RAINTOT",  2, 2, 0, 0, 0,       OreChgBitsRtot},
   {"ORE_CHGBITS_UV",       2, 2, 0, 0, 0,       OreChgBitsUV},
   {"ORE_DATA_ENTRY",       2, 2, 0, 0, 0,       OreDataEntry},
   {"ORE_DISPLAY_CHAN",     2, 2, 0, 0, 0,       OreDispChan},
   {"ORE_DISPLAY_BATLVL",   2, 2, 0, 0, 0,       OreDispBatLvl},
   {"ORE_DISPLAY_RAW",      2, 2, 0, 0, CIGN,    Ignored},
   {"ORE_DISPLAY_COUNT",    2, 2, 0, 0, 0,       OreDispCount},
   {"ORE_DISPLAY_BEAUFORT", 2, 2, 0, 0, 0,       OreDispBft},
   {"ORE_ID_16",            2, 2, 0, 0, 0,       OreID16},
   {"ORE_DISPLAY_FCAST",    2, 2, 0, 0, 0,       OreDispFcast},
   {"LOGDATE_UNIX",         2, 2, 0, 0, 0,       LogDateUnix},
   {"DISPLAY_SENSOR_INTV",  2, 2, 0, 0, 0,       DispSensorIntv},
   {"DATE_FORMAT",          2, 3, 0, 1, COVR,    DateFormat},
   {"LOCK_TIMEOUT",         2, 2, 0, 0, 0,       LockTimeout},
   {"CM11A_QUERY_DELAY",    2, 2, 0, 0, 0,       CM11QueryDelay},
   {"ELS_VOLTAGE",          2, 2, 0, 0, 0,       ElsVoltage},
   {"ELS_CHGBITS_CURR",     2, 2, 0, 0, 0,       ElsChgBitsCurr},
   {"LAUNCHER_SCANMODE",    2, 2, 0, 0, 0,       ScanMode},
   {"RFXMETER_SETUP_INLINE", 2, 2, 0, 0, 0,      RfxInline},
   {"OWL_VOLTAGE",          2, 2, 0, 0, 0,       OwlVoltage},
   {"OWL_CALIB_POWER",      2, 2, 0, 0, 0,       OwlCalibPower},
   {"OWL_CALIB_ENERGY",     2, 2, 0, 0, 0,       OwlCalibEnergy},
   {"OWL_CHGBITS_POWER",    2, 2, 0, 0, 0,       OwlChgBitsPower},
   {"OWL_CHGBITS_ENERGY",   2, 2, 0, 0, 0,       OwlChgBitsEnergy},
   {"OWL_DISPLAY_COUNT",    2, 2, 0, 0, 0,       OwlDispCount},
   {"ARM_REMOTE",           2, 2, 0, 0, 0,       ArmRemote},
//   {"ACTIVE_CHANGE",        2, 2, 0, 0, CIGN,    Ignored},
   {"INACTIVE_HANDLING",    2, 2, 0, 0, 0,       InactiveHandling},
   {"PROCESS_XMIT",         2, 2, 0, 0, 0,       ProcessXmit},
   {"SHOW_FLAGS_MODE",      2, 2, 0, 0, 0,       ShowFlagsMode},
   {"LAUNCHPATH_APPEND",    2, 2, 2, 0, 0,       LaunchPathAppend},
   {"LAUNCHPATH_PREFIX",    2, 2, 2, 0, 0,       LaunchPathPrefix},
   {"FIX_STOPSTART_ERROR",  2, 2, 0, 1, COVR,    FixStopStartError},
//   {"CHKSUM_TIMEOUT",       2, 2, 0, 0, 0,       ChkSumTimeout},
   
#if 0
   {"ELS_NUMBER",           2, 2, 0, 0, 0,       ElsNumber},
#endif
#if 0
   {"INACTIVE_TIMEOUT_ORE", 2, 2, 0, 0, 0,       InactiveTimeoutOre},  /* WIP */
#endif
};
int ncommands = ( sizeof(command)/sizeof(struct conf) );

/* Dawn/Dusk options */
static struct ddopt {
   char     *label;
   unsigned char value;
} dd_option[] = {
   {"FIRST",    FIRST },
   {"EARLIEST", EARLIEST },
   {"LATEST",   LATEST },
   {"AVERAGE",  AVERAGE },
   {"MEDIAN",   MEDIAN },
};
int nddopt = ( sizeof(dd_option)/sizeof(struct ddopt) );

/* Launch source options - set default sources of signal */
/* permitted to launch a script.                         */
static struct lsopt {
   char      *label;
   unsigned int value;
} ls_option[] = {
   {"SNDC",     SNDC   }, /* Sent from command line */
   {"SNDM",     SNDM   }, /* Sent from macro executed by Timer */
   {"SNDT",     SNDT   }, /* Sent from macro executed by Trigger */
   {"SNDP",     SNDP   }, /* Sent from a relay powerfail script */
   {"SNDS",     SNDS   }, /* Sent from a script */
   {"RCVI",     RCVI   }, /* Received from the interface */
   {"RCVT",     RCVT   }, /* Trigger which executed a macro */
   {"SNDA",     SNDA   }, /* Transceived by aux daemon */
   {"RCVA",     RCVA   }, /* Received via sptty from aux daemon */
   {"ANYSRC",   LSALL  }, /* Any of the above (SNDS and SNDP not included!) */
   {"NOSRC",    LSNONE }, /* No sources are the default */
};
int nlsopt = ( sizeof(ls_option)/sizeof(struct lsopt) );

/* CM17A function labels.  Note: CONFIG.rf_bursts[] in    */
/* process.h must be at least the size of nrflabels below */
static struct cm17a_label {
   char          *label;
   unsigned char subcode;
} rf_label[] = {
  {"falloff",    0 },
  {"flightson",  1 },
  {"fon",        2 },
  {"foff",       3 },
  {"fdim",       4 },
  {"fbright",    5 },
  {"flightsoff", 6 },
  {"_",          7 },
  {"_",          8 },
  {"farb",       9 },
  {"farw",      10 },
};
int nrflabels = ( sizeof(rf_label)/sizeof(struct cm17a_label) );

/*---------------------------------------------------------------------+
 | Reset the isparsed flags in the conf struct array.                  |
 +---------------------------------------------------------------------*/
void reset_isparsed_flags ( void )
{
   int j;

   for ( j = 0; j < ncommands; j++ )
      command[j].isparsed = 0;

   return;
}

/*---------------------------------------------------------------------+
 | Return 1 if all valid characters within alias label, otherwise 0    |
 +---------------------------------------------------------------------*/
int is_valid_alias_label ( char *label, char **sp )
{
   *sp = label;
   while ( **sp ) {
      if ( isalnum((int)(**sp)) || strchr("-_.", **sp) ) {
         (*sp)++;
	 continue;
      }
      return 0;
   }
   return 1;
}   
	      
/*---------------------------------------------------------------------+
 | Initialize the CONFIG structure with default values.                |
 +---------------------------------------------------------------------*/
void initialize_config ( void )
{
   char *sp1;
   int  j;

   /* Load default configuration values */
   configp->read_flag = 0;
   configp->mode = DEF_MODE;
   (void) strncpy2(configp->schedfile, DEF_SCHEDFILE, sizeof(config.schedfile) - 1);
   configp->asif_date = -1;
   configp->asif_time = -1;
   configp->program_days_in = DEF_PROGRAM_DAYS;
   configp->program_days = DEF_PROGRAM_DAYS;
   configp->combine_events = DEF_COMBINE_EVENTS;
   configp->compress_macros = DEF_COMPRESS_MACROS;
   configp->feb_kluge = DEF_FEB_KLUGE;
   configp->housecode = DEF_HOUSECODE;
   configp->force_addr = DEF_FORCE_ADDR;
   configp->loc_flag = 0;
   configp->dawn_option = DEF_DAWN_OPTION;
   configp->dusk_option = DEF_DUSK_OPTION;
   configp->dawn_substitute = DEF_DAWN_SUBSTITUTE;
   configp->dusk_substitute = DEF_DUSK_SUBSTITUTE;
   configp->min_dawn = DEF_MIN_DAWN;
   configp->max_dawn = DEF_MAX_DAWN;
   configp->min_dusk = DEF_MIN_DUSK;
   configp->max_dusk = DEF_MAX_DUSK;
   strncpy2(configp->tty, DEF_TTY, sizeof(config.tty) - 1);
   configp->ttyaux[0] = '\0';
   configp->auxhost[0] = '\0';
   configp->auxport[0] = '\0';
   configp->suffixaux[0] = '\0';
   configp->auxdev = 0;
   configp->newformat = 0;
   configp->checkfiles = DEF_CHECK_FILES;
   configp->repl_delay = DEF_REPL_DELAYED_MACROS;
   configp->reserved_timers = DEF_RESERVED_TIMERS;
   configp->alias_size = 0;
   configp->aliasp = NULL;
   configp->scenep = NULL;
   configp->scriptp = NULL;
   configp->launcherp = NULL;
   configp->pfail_script = NULL;
   configp->max_pparms = DEF_MAX_PPARMS;
   configp->rcs_temperature = DEF_RCS_TEMPERATURE;
   configp->trigger_tag = DEF_TRIGGER_TAG;
   configp->display_offset = DEF_DISPLAY_OFFSET;
   configp->xref_append = DEF_XREF_APPEND;
   configp->ack_hails = DEF_ACK_HAILS;
   configp->macterm = DEF_MACTERM;
   configp->status_timeout = DEF_STATUS_TIMEOUT;
   configp->spf_timeout = DEF_SPF_TIMEOUT;
   configp->disp_exp_mac = DEF_DISPLAY_EXP_MACROS;
   configp->module_types = DEF_MODULE_TYPES;
   configp->launch_mode = DEF_LAUNCH_MODE;
   configp->function_mode = DEF_FUNCTION_MODE;
   configp->launch_source = DEF_LAUNCH_SOURCE;
   configp->res_overlap = DEF_RES_OVERLAP;
   configp->default_module = lookup_module_type(DEF_DEFAULT_MODULE);
   configp->script_mode = DEF_SCRIPT_MODE;
   if ( (sp1 = getenv("SHELL")) != NULL )
      strncpy2(configp->script_shell, sp1, sizeof(config.script_shell) - 1);
   else
      strncpy2(configp->script_shell, "/bin/sh", sizeof(config.script_shell) - 1);
   *configp->logfile = '\0';
   configp->logcommon = NO;
   configp->disp_subdir = NO_ANSWER;
   configp->isdark_offset = DEF_ISDARK_OFFSET;
   strncpy2(configp->env_alias_prefix, DEF_ENV_ALIAS_PREFIX, sizeof(config.env_alias_prefix) - 1);
   configp->sunmode = DEF_SUNMODE;
   configp->fix_5a = DEF_FIX_5A;
   configp->cm11_post_delay = DEF_CM11_POST_DELAY;
   configp->pfail_update = DEF_PFAIL_UPDATE;
   configp->cm17a_bit_delay = DEF_CM17A_BIT_DELAY;
   configp->rf_burst_spacing = DEF_RF_BURST_SPACING;
   configp->rf_timer_tweak = DEF_RF_TIMER_TWEAK;
   configp->rf_post_delay = DEF_RF_POST_DELAY;
   configp->rf_farb_delay = DEF_RF_FARB_DELAY;
   configp->rf_farw_delay = DEF_RF_FARW_DELAY;
   configp->disp_rf_xmit = DEF_DISP_RF_XMIT;
   configp->def_rf_bursts = DEF_RF_BURSTS;
   for ( j = 0; j < nrflabels; j++ )
      configp->rf_bursts[j] = DEF_RF_BURSTS;
   configp->timer_loopcount = 0;
   configp->restrict_dims = DEF_RESTRICT_DIMS;
   configp->state_format = DEF_STATE_FORMAT;
   configp->start_engine = DEF_START_ENGINE;
   configp->rf_noswitch = DEF_RF_NOSWITCH;
   configp->respool_perms = DEF_RESPOOL_PERMS;
   configp->spool_max = DEF_SPOOLFILE_MAX;
   configp->check_RI_line = DEF_CHECK_RI_LINE;
   configp->send_retries = DEF_SEND_RETRIES;
   configp->script_ctrl = DEF_SCRIPT_CTRL;
   configp->transceive = DEF_TRANSCEIVE;
   configp->rfforward = DEF_RFFORWARD;
   configp->trans_dim = DEF_TRANS_DIMLEVEL;
   configp->state_ctrl = DEF_STATE_CTRL;
   configp->ring_ctrl = DEF_RING_CTRL;
   configp->heyu_umask = DEF_HEYU_UMASK;
   configp->log_umask = DEF_LOG_UMASK;
   configp->rf_funcmask = DEF_RF_FUNCMASK;
   configp->aux_repcounts[0] = 0;  /* Value set at finalize_config() */
   configp->aux_repcounts[1] = DEF_AUX_REPCOUNT;
   configp->aux_repcounts[2] = DEF_AUX_MAXCOUNT;
   configp->aux_repcounts[3] = DEF_AUX_FLOODREP;
   configp->aux_mincount_rfx = 0; /* Value set at finalize_config() */
   configp->hide_unchanged = DEF_HIDE_UNCHANGED;
   configp->hide_unchanged_inactive = DEF_HIDE_UNCHANGED_INACTIVE;
   configp->show_change = DEF_SHOW_CHANGE;
   configp->disp_rf_noise = DEF_DISP_RF_NOISE;
   configp->arm_max_delay = DEF_ARM_MAX_DELAY;
   configp->disp_raw_rf = DEF_DISP_RAW_RF;
   configp->arm_logic = DEF_ARM_LOGIC;
   configp->inactive_timeout = DEF_INACTIVE_TIMEOUT;
   configp->device_type = DEF_DEVICE_TYPE;
   configp->auto_wait = DEF_AUTO_WAIT;
   configp->full_bright = DEF_FULL_BRIGHT;
   configp->engine_poll = DEF_ENGINE_POLL;
   configp->dmx_tscale = DEF_DMX_TSCALE;
   configp->dmx_toffset = DEF_DMX_TOFFSET;
   configp->ore_lobat = DEF_ORE_LOWBATTERY;
   configp->ore_tscale = DEF_ORE_TSCALE;
   configp->ore_toffset = DEF_ORE_TOFFSET;
   configp->ore_bpscale = DEF_ORE_BPSCALE;
   configp->ore_bpoffset = DEF_ORE_BPOFFSET;
   strncpy2(configp->ore_bpunits, DEF_ORE_BPUNITS, NAME_LEN - 1);
   configp->ore_wgtscale = DEF_ORE_WGTSCALE;
   strncpy2(configp->ore_wgtunits, DEF_ORE_WGTUNITS, NAME_LEN - 1);
   configp->rfx_tscale = DEF_RFX_TSCALE;
   configp->rfx_toffset = DEF_RFX_TOFFSET;
   configp->rfx_vadscale = DEF_RFX_VADSCALE;
   configp->rfx_vadoffset = DEF_RFX_VADOFFSET;
   strncpy2(configp->rfx_vadunits, DEF_RFX_VADUNITS, NAME_LEN - 1);
   configp->rfx_bpscale = DEF_RFX_BPSCALE;
   configp->rfx_bpoffset = DEF_RFX_BPOFFSET;
   strncpy2(configp->rfx_bpunits, DEF_RFX_BPUNITS, NAME_LEN - 1);
   configp->rfxcom_dtr_rts = DEF_RFXCOM_DTR_RTS;
   configp->rfxcom_hibaud = DEF_RFXCOM_HIBAUD;
   configp->rfx_powerscale = DEF_RFX_POWERSCALE;
   strncpy2(configp->rfx_powerunits, DEF_RFX_POWERUNITS, NAME_LEN - 1);
   configp->rfx_waterscale = DEF_RFX_WATERSCALE;
   strncpy2(configp->rfx_waterunits, DEF_RFX_WATERUNITS, NAME_LEN - 1);
   configp->rfx_gasscale = DEF_RFX_GASSCALE;
   strncpy2(configp->rfx_gasunits, DEF_RFX_GASUNITS, NAME_LEN - 1);
   configp->rfx_pulsescale = DEF_RFX_PULSESCALE;
   strncpy2(configp->rfx_pulseunits, DEF_RFX_PULSEUNITS, NAME_LEN - 1);
   configp->rfxcom_enable = DEF_RFXCOM_ENABLE;
   configp->rfxcom_disable = DEF_RFXCOM_DISABLE;
   configp->lockup_check = DEF_LOCKUP_CHECK;
   strncpy2(configp->tailpath, DEF_TAILPATH, sizeof(config.tailpath) - 1);
   configp->suppress_rfxjam = DEF_SUPPRESS_RFXJAM;
   configp->display_dmxtemp = DEF_DISPLAY_DMXTEMP;
   configp->securid_16 = DEF_SECURID_16;
   configp->securid_mask = DEF_SECURID_MASK;
   configp->securid_parity = DEF_SECURID_PARITY;
   configp->logdate_year = DEF_LOGDATE_YEAR;
   configp->disp_ore_all = DEF_DISPLAY_ORE_ALL;
   configp->ore_chgbits_t = DEF_ORE_CHGBITS_T;
   configp->ore_chgbits_rh = DEF_ORE_CHGBITS_RH;
   configp->ore_chgbits_bp = DEF_ORE_CHGBITS_BP;
   configp->ore_chgbits_wgt = DEF_ORE_CHGBITS_WGT;
   configp->ore_chgbits_wsp = DEF_ORE_CHGBITS_WSP;
   configp->ore_chgbits_wavsp = DEF_ORE_CHGBITS_WAVSP;
   configp->ore_chgbits_wdir = DEF_ORE_CHGBITS_WDIR;
   configp->ore_chgbits_rrate = DEF_ORE_CHGBITS_RRATE;
   configp->ore_chgbits_rtot = DEF_ORE_CHGBITS_RTOT;
   configp->ore_chgbits_uv = DEF_ORE_CHGBITS_UV;
   configp->ore_data_entry = DEF_ORE_DATA_ENTRY;
   configp->ore_display_chan = DEF_ORE_DISPLAY_CHAN;
   configp->ore_display_batlvl = DEF_ORE_DISPLAY_BATLVL;
   configp->ore_display_count = DEF_ORE_DISPLAY_COUNT;
   configp->oreid_16 = DEF_OREID_16;
   configp->ore_display_fcast = DEF_ORE_DISPLAY_FCAST;
   configp->oreid_mask = DEF_OREID_MASK;
   configp->logdate_unix = DEF_LOGDATE_UNIX;
   configp->inactive_timeout_ore = DEF_INACTIVE_TIMEOUT_ORE;
   configp->display_sensor_intv = DEF_DISPLAY_SENSOR_INTV;
   configp->date_format = DEF_DATE_FORMAT;
   configp->date_separator = DEF_DATE_SEPARATOR;
   configp->lock_timeout = DEF_LOCK_TIMEOUT;
   configp->cm11a_query_delay = DEF_CM11A_QUERY_DELAY;
   configp->els_number = DEF_ELS_NUMBER;
   configp->els_voltage = DEF_ELS_VOLTAGE;
   configp->els_chgbits_curr = DEF_ELS_CHGBITS_CURR;
   configp->ore_windscale = DEF_ORE_WINDSCALE;
   strcpy(configp->ore_windunits, DEF_ORE_WINDUNITS);
   configp->ore_windsensordir = DEF_ORE_WINDSENSORDIR;
   configp->ore_winddir_mode = DEF_ORE_WINDDIR_MODE;
   configp->ore_rainratescale = DEF_ORE_RAINRATESCALE;
   strcpy(configp->ore_rainrateunits, DEF_ORE_RAINRATEUNITS);
   configp->ore_raintotscale = DEF_ORE_RAINTOTSCALE;
   strcpy(configp->ore_raintotunits, DEF_ORE_RAINTOTUNITS);
   configp->scanmode = DEF_SCANMODE;
   configp->rfx_inline = DEF_RFX_INLINE;
   configp->owl_voltage = DEF_OWL_VOLTAGE;
   configp->owl_calib_power = DEF_OWL_CALIB_POWER;
   configp->owl_calib_energy = DEF_OWL_CALIB_ENERGY;
   configp->owl_chgbits_power = DEF_OWL_CHGBITS_POWER;
   configp->owl_chgbits_energy = DEF_OWL_CHGBITS_ENERGY;
   configp->owl_display_count = DEF_OWL_DISPLAY_COUNT;
   configp->arm_remote = DEF_ARM_REMOTE;
   configp->active_change = DEF_ACTIVE_CHANGE;
   configp->inactive_handling = DEF_INACTIVE_HANDLING;
   configp->process_xmit = DEF_PROCESS_XMIT;
   configp->show_flags_mode = DEF_SHOW_FLAGS_MODE;
   configp->rfx_master = DEF_RFX_MASTER;
   configp->rfx_slave = DEF_RFX_SLAVE;
   strcpy(configp->launchpath_append, DEF_LAUNCHPATH_APPEND);
   strcpy(configp->launchpath_prefix, DEF_LAUNCHPATH_PREFIX);
   configp->fix_stopstart_error = DEF_FIX_STOPSTART_ERROR;
   configp->ttyrfxmit[0] = '\0';
   configp->rfxmit_freq = 0;
   configp->chksum_timeout = DEF_CHKSUM_TIMEOUT;

   return;
}

/*---------------------------------------------------------------------+
 | Parse the users configuration file and save info in CONFIG struct   |
 | for only the minimal directives required for some specific commands.|
 | E.g., the 'heyu stop' command needs only the TTY directive.         |
 +---------------------------------------------------------------------*/
int parse_minimal_config ( FILE *fd_conf, unsigned char mode, unsigned char source )
{
   char   buffer[LINE_LEN];
   char   *sp1;
   int    err, errors;
   CONFIG configtmp;

   configp = &configtmp;

   /* Make sure the isparsed flags are reset in the command list */
   reset_isparsed_flags();

   /* Load some default configuration values */
   initialize_config();

   line_no = 0;
   errors = 0;
   while ( fgets(buffer, LINE_LEN, fd_conf) != NULL ) {
      line_no++ ;
      buffer[LINE_LEN - 1] = '\0';

      /* Get rid of comments and blank lines */
      if ( (sp1 = strchr(buffer, '#')) != NULL )
         *sp1 = '\0';
      (void) strtrim(buffer);
      if ( buffer[0] == '\0' )
         continue;

      err = parse_config_tail(buffer, source);
      if ( err || *error_message() != '\0' ) { 
         fprintf(stderr, "Config Line %02d: %s\n", line_no, error_message());
         clear_error_message();
      }

      errors += err;
      if ( errors > MAX_ERRORS )
         return 1;
   }

   /* Determine and load the user's timezone */
   get_std_timezone();
   configp->tzone = std_tzone;

   /* Determine if Daylight Time is ever in effect and if so,    */
   /* the minutes after midnight it goes into and out of effect. */
   configp->isdst = get_dst_info(0);

   /* Add configuration items from environment */
   errors += environment_config();

   errors += finalize_config(mode);

   if ( *error_message() != '\0' ) {
      fprintf(stderr, "%s\n", error_message());
      clear_error_message();
   }

   /* Free the allocated memory in the original config structure */
   free_all_arrays(&config);

   /* Copy the temporary config into the global structure */
   memcpy(&config, configp, sizeof(config));

   configp = &config;

   /* Done with config file */
   line_no = 0; 
  
   return errors;
}
         

/*---------------------------------------------------------------------+
 | Parse the users configuration file and save info in CONFIG struct.  |
 | First check to see if it's already been read.                       |
 +---------------------------------------------------------------------*/
int parse_config ( FILE *fd_conf, unsigned char mode )
{
   char   buffer[LINE_LEN];
   char   *sp1;
   int    err, errors;
   int    j;
   SCENE  *scenep;
   extern char *typename[];
   CONFIG configtmp;


   /* If the configuration file has already been read into memory, */
   /* just return.                                                 */
   if ( config.read_flag != 0 ) {
      return 0;
  }

   configp = &configtmp;

   /* Make sure the isparsed flags are reset in the command list */
   reset_isparsed_flags();

   /* Load some default configuration values */
   initialize_config();

   line_no = 0;
   errors = 0;
   while ( fgets(buffer, LINE_LEN, fd_conf) != NULL ) {
      line_no++ ;
      buffer[LINE_LEN - 1] = '\0';

      /* Get rid of comments and blank lines */
      if ( (sp1 = strchr(buffer, '#')) != NULL )
         *sp1 = '\0';
      (void) strtrim(buffer);
      if ( buffer[0] == '\0' )
         continue;

      err = parse_config_tail(buffer, SRC_CONFIG);
      if ( err || *error_message() != '\0') {
         if ( !i_am_relay && !i_am_aux && !i_am_monitor ) 
            fprintf(stderr, "Config Line %02d: %s\n", line_no, error_message());
         clear_error_message();
      }

      errors += err;
      if ( errors > MAX_ERRORS )
         return 1;
   }

   /* Everything has now been read from the config file and stored */

   /* Verify the syntax of user-defined scenes/usersyns */
   scenep = configp->scenep;
   j = 0;
   while ( scenep && scenep[j].line_no > 0 ) {
      if ( verify_scene(scenep[j].body) != 0 ) {
         fprintf(stderr, "Config Line %02d: %s '%s': %s\n",
            scenep[j].line_no, typename[scenep[j].type], scenep[j].label,
            error_message());
         clear_error_message();
         if ( ++errors > MAX_ERRORS )
            return errors;
      }
      else if ( *error_message() != '\0' ) {
         /* Check warning messages */
         fprintf(stderr, "Config Line %02d: %s '%s': %s\n",
            scenep[j].line_no, typename[scenep[j].type], scenep[j].label,
            error_message());
         clear_error_message();
      }
      j++;
   }

   /* Configure module masks */
   set_module_masks( configp->aliasp );

   /* Use the Heyu path if the user hasn't provided an alternate */
   if ( !(*alt_path) )
      (void)strncpy2(alt_path, heyu_path, sizeof(alt_path) - 1);

   /* Determine and load the user's timezone */
   get_std_timezone();
   configp->tzone = std_tzone;

   /* Determine if Daylight Time is ever in effect and if so,    */
   /* the minutes after midnight it goes into and out of effect. */
   configp->isdst = get_dst_info(0);

   /* Add configuration items from environment */
   errors += environment_config();

   errors += finalize_config(mode);
   if ( *error_message() != '\0' ) {
      fprintf(stderr, "%s\n", error_message());
      clear_error_message();
   }
   configp->read_flag = (errors == 0) ? 1 : 0;

   if ( errors > 0 )
      return errors;

   /* Free the allocated memory in the original config structure */
   free_all_arrays(&config);

   /* Copy the temporary config into the global structure */
   memcpy(&config, configp, sizeof(config));

   configp = &config;

   /* Done with config file */
   line_no = 0; 
  
   return errors;
}
         

/*---------------------------------------------------------------------+
 | Parse the config line in buffer.  Argument 'source' can be          |
 | SRC_CONFIG if called from parse_config() or SRC_SCHED if called     |
 | from parse_sched(), for configuration items which may be overridden |
 | in the latter.                                                      |
 +---------------------------------------------------------------------*/
int parse_config_tail ( char *buffer, unsigned char source ) 
{  
   char   errbuffer[80];
   char   searchstr[128];
   char   directive[128];
   char   label[(NAME_LEN + SCENE_LEN + MACRO_LEN + 1)];
   char   token[50];
   char   hc;
   char   *bufp, *sp;
   int    errors = 0;
   int    num, value, hour, minut, len, modtype;
   long   longvalue;
   int    bursts;
   int    j, k, commj, switchval;
   int    tokc;
   char   **tokv = NULL;
   int    perms;
   double dblvalue;

   extern struct rfx_disable_st rfx_disable[];
   extern int nrfxdisable;

      bufp = buffer;
      get_token(directive, &bufp, " \t", sizeof(directive)/sizeof(char));
      strtrim(bufp);
      strncpy2(searchstr, directive, sizeof(searchstr) - 1);
      strupper(searchstr);


      /* Search list of configuration commands starting */
      /* past "oldalias".                               */

      commj = 0; switchval = OldAlias;
      for ( j = 1; j < ncommands; j++ ) {
         if ( !strcmp(searchstr, command[j].name) ) {
            switchval = command[j].switchval;
            commj = j;
            break;
         }
      }

      /* See if override in schedule file is permitted */
      if ( (source == SRC_SCHED || source == SRC_ENVIRON) &&
               (command[commj].flags & COVR) == 0 ) {
         if ( commj )
            sprintf(errbuffer, "Configuration directive %s not allowed here.",
                command[commj].name);
         else
            sprintf(errbuffer, "Invalid configuration directive");
         store_error_message(errbuffer);
         return 1;
      }

      /* Minimal configurations for some commands */
      if ( source == SRC_STOP && (command[commj].flags & CSTP) == 0 )
         return 0;
      if ( source == SRC_HELP && (command[commj].flags & CHLP) == 0 )
         return 0;
      if ( source == SRC_SYSCHECK && (command[commj].flags & CCAL) == 0 )
         return 0;


      /* If the directive has a fixed number of tokens (as denoted */
      /* by maxtokens > 0) or _might_ be an old-style alias,       */
      /* tokenize the tail immediately.                            */

      if ( commj == 0 || command[j].maxtoken > 0 ) {
         tokenize(bufp, " \t", &tokc, &tokv);
      }
      else {
         tokc = (*bufp == '\0') ? 0 : 1 ;
      }


      /* If not found in list and it doesn't have the correct */
      /* number of items for an alias, reject it.             */
      if ( commj == 0  && tokc != 2 ) {
         sprintf(errbuffer, "Invalid Directive '%s'", directive);
         store_error_message(errbuffer);
         return 1;
      }


      /* Unless allowed, check to see it's not a duplicate  */
      /* of an earlier directive.                           */
      if ( command[commj].isparsed & source && (command[commj].flags & CMLT) == 0) {
         sprintf(errbuffer, 
            "Directive '%s' appears more than once in the file", searchstr);
         store_error_message(errbuffer);
         return 1;
      }
      else {
         command[commj].isparsed |= source;
      }

      /* Check that commands found on the list have the correct */
      /* number of tokens on the line.                          */
      if ( (tokc + 1) < command[commj].mintoken ) {
         store_error_message("Too few items on line.");
         return 1;
      }
      else if ( (command[commj].maxtoken != -1) && ((tokc + 1) > command[commj].maxtoken) ) {
         store_error_message("Too many items on line.");
         return 1;
      }

      errors = 0;
      switch ( switchval ) {
         case Ignored :
            sprintf(errbuffer,
               "Directive %s is obsolete and is being ignored.", searchstr);
            store_error_message(errbuffer);
            break;

         case Mode :
            (void) strupper(tokv[0]);
            if ( !strcmp(tokv[0], "COMPATIBLE") )
               configp->mode = COMPATIBLE;
            else if ( !strcmp(tokv[0], "HEYU") )
               configp->mode = HEYU_MODE;
            else {
               store_error_message("MODE must be COMPATIBLE or HEYU");
               errors++;
            }
            break;

         case OldAlias :  /* Possible alias, else invalid command */
            /* Check if it matches the form of an alias */
            if ( tokc != 2 || strlen(tokv[0]) != 1 ||
                  (hc = toupper((int)(*tokv[0]))) < 'A' || hc > 'P' ) {
               store_error_message("Invalid Directive.");
               errors++;
               break;
            }
	    if ( !is_valid_alias_label(directive, &sp) ) {
	       sprintf(errbuffer, "Invalid character '%c' in alias label.", *sp);
	       store_error_message(errbuffer);
	       errors++;
	       break;
	    }
	       
            if ( strcmp(tokv[0], "macro") == 0 ) {
               store_error_message("An alias label may not be the word \"macro\".");
               errors++;
               break;
            }

            if ( strchr("_-", *directive ) ) {
               store_error_message("Alias labels may not begin with '_' or '-'");
               errors++;
               break;
            }

            if ( add_alias(&configp->aliasp, directive, line_no,
                                                 hc, tokv[1], -1) < 0 ) {
               errors++;
            }
            break;

         case Alias :  /* New alias format using ALIAS directive */
            /* Expects housecode and unit code(s) to be concatenated, */
            /* the same as they are for the command line or macro.    */

	    if ( !is_valid_alias_label(tokv[0], &sp) ) {
	       sprintf(errbuffer, "Invalid character '%c' in alias label.", *sp);
	       store_error_message(errbuffer);
	       errors++;
	       break;
	    }

            if ( strcmp(tokv[0], "macro") == 0 ) {
               store_error_message("An alias label may not be the word \"macro\".");
               errors++;
               break;
            }

            if ( strchr("_-", *tokv[0] ) ) {
               store_error_message("Alias labels may not begin with '_' or '-'");
               errors++;
               break;
            }

            hc = toupper((int)(*tokv[1]));
            *tokv[1] = ' ';
            strtrim(tokv[1]);

            modtype = -1;
            if ( tokc > 2 ) {
               if ( (modtype = lookup_module_type(tokv[2])) < 0 ) {
                  sprintf(errbuffer, "Invalid module model '%s'", tokv[2]);
                  store_error_message(errbuffer);
                  errors++;
                  break;
               }
            }

            if ( (j = add_alias(&configp->aliasp, tokv[0], line_no,
                                hc, tokv[1], modtype)) < 0 ) {
               errors++;
               break;
            }

            if ( modtype >= 0 && tokc >= 3 ) {
               if ( add_module_options(configp->aliasp, j, tokv + 3, tokc - 3) != 0 ) {
                  errors++;
                  break;
               }
            }

            break;

         case UserSyn :
            get_token(label, &bufp, " \t", sizeof(label)/sizeof(char));
            if ( add_scene(&configp->scenep, label, line_no, bufp, F_USYN) < 0 ) {
               errors++;
            }
            break;

         case Scene :
            get_token(label, &bufp, " \t", sizeof(label)/sizeof(char));
            if ( add_scene(&configp->scenep, label, line_no, bufp, F_SCENE) < 0 ) {
               errors++;
            }
            break;

         case AsIfDate :
            configp->asif_date = strtol(tokv[0], &sp, 10);
            if ( !strchr(" \t\n", *sp) ||
	         configp->asif_date < 19700101 ||
                 configp->asif_date > 20380101 )  {
               store_error_message("ASIF_DATE must be yyyymmdd between 19700101 and 20380101");
               errors++;
            }
            break;

         case AsIfTime :
            num = sscanf(tokv[0], "%d:%d", &hour, &minut) ;
            value = 60 * hour + minut ;
            configp->asif_time = value;
            if ( num != 2 || value < 0 || value > 1439 ) {
               store_error_message("ASIF_TIME must be hh:mm between 00:00-23:59");
               errors++;
            }
            break;

         case ScheduleFile :
            strncpy2(configp->schedfile, tokv[0], sizeof(config.schedfile) - 1);
            break;

         case ProgramDays :
            strupper(tokv[0]);
            configp->program_days_in = (int)strtol(tokv[0], &sp, 10);

            if ( !strchr(" \t\n", *sp) ||
	       configp->program_days_in < 1 ||
	       configp->program_days_in > 366 ) {
               store_error_message("PROGRAM_DAYS outside range 1-366");
               errors++;
            }
            break;

         case CombineEvents :
            strupper(tokv[0]);
            if ( !strcmp(tokv[0], "YES") )
               configp->combine_events = YES;
            else if ( !strcmp(tokv[0], "NO") )
               configp->combine_events = NO;
            else {
               store_error_message("COMBINE_EVENTS must be YES or NO");
               errors++;
            }
            break;

         case CompressMacros :
            (void) strupper(tokv[0]);
            if ( !strcmp(tokv[0], "YES") )
               configp->compress_macros = YES;
            else if ( !strcmp(tokv[0], "NO") )
               configp->compress_macros = NO;
            else {
               store_error_message("COMPRESS_MACROS must be YES or NO");
               errors++;
            }
            break;

         case FebKluge :
            (void) strupper(tokv[0]);
            if ( !strcmp(tokv[0], "YES") )
               configp->feb_kluge = YES;
            else if ( !strcmp(tokv[0], "NO") )
               configp->feb_kluge = NO;
            else {
               store_error_message("FEB_KLUGE must be YES or NO");
               errors++;
            }
            break;

         case Latitude :            
            errors += parse_latitude(tokv[0]);
            break;

         case Longitude :
            errors += parse_longitude(tokv[0]);
            break;

         case DawnOption :
            (void) strupper(tokv[0]);
            for ( j = 0; j < nddopt; j++ ) {
               if ( !strcmp(tokv[0], dd_option[j].label) )
                  break;
            }
            if ( j == nddopt ) {
               store_error_message("Invalid DAWN_OPTION");
               errors++;
               break;
            }
            configp->dawn_option = dd_option[j].value;
            break;

         case DuskOption :
            (void) strupper(tokv[0]);
            for ( j = 0; j < nddopt; j++ ) {
               if ( !strcmp(tokv[0], dd_option[j].label) )
                  break;
            }
            if ( j == nddopt ) {
               store_error_message("Invalid DUSK_OPTION");
               errors++;
               break;
            }
            configp->dusk_option = dd_option[j].value;
            break;

         case DawnSubstitute :
            num = sscanf(tokv[0], "%d:%d", &hour, &minut) ;
            value = 60 * hour + minut ;
            if ( num != 2 || value < 0 || value > 1439 ) {
               store_error_message("DAWN_SUBSTITUTE - must be 00:00-23:59 (hh:mm)");
               errors++;
               break;
            }
            configp->dawn_substitute = value;
            break;

         case DuskSubstitute :
            num = sscanf(tokv[0], "%d:%d", &hour, &minut) ;
            value = 60 * hour + minut ;
            if ( num != 2 || value < 0 || value > 1439 ) {
               store_error_message("DUSK_SUBSTITUTE must be 00:00-23:59 (hh:mm)");
               errors++;
               break;
            }
            configp->dusk_substitute = value;
            break;

         case MinDawn :
            if ( strcmp(strupper(tokv[0]), "OFF") == 0 ) {
               configp->min_dawn = OFF;
               break;
            }
            num = sscanf(tokv[0], "%d:%d", &hour, &minut);
            value = 60 * hour + minut ;
            if ( num != 2 || value < 0 || value > 1439 ) {
               store_error_message("MIN_DAWN must be 00:00-23:59 (hh:mm) or OFF");
               errors++;
               break;
            }
            configp->min_dawn = value;
            break;

         case MaxDawn :
            if ( strcmp( strupper(tokv[0]), "OFF") == 0 ) {
               configp->max_dawn = OFF;
               break;
            }
            num = sscanf(tokv[0], "%d:%d", &hour, &minut);
            value = 60 * hour + minut ;
            if ( num != 2 || value < 0 || value > 1439 ) {
               store_error_message("MAX_DAWN must be 00:00-23:59 (hh:mm) or OFF");
               errors++;
               break;
            }
            configp->max_dawn = value;
            break;

         case MinDusk :
            if ( strcmp( strupper(tokv[0]), "OFF") == 0 ) {
               configp->min_dusk = OFF;
               break;
            }
            num = sscanf(tokv[0], "%d:%d", &hour, &minut);
            value = 60 * hour + minut ;
            if ( num != 2 || value < 0 || value > 1439 ) {
               store_error_message("MIN_DUSK must be 00:00-23:59 (hh:mm) or OFF");
               errors++;
               break;
            }
            configp->min_dusk = value;
            break;

         case MaxDusk :
            if ( strcmp( strupper(tokv[0]), "OFF") == 0 ) {
               configp->max_dusk = OFF;
               break;
            }
            num = sscanf(tokv[0], "%d:%d", &hour, &minut);
            value = 60 * hour + minut ;
            if ( num != 2 || value < 0 || value > 1439 ) {
               store_error_message("MAX_DUSK must be 00:00-23:59 (hh:mm) or OFF");
               errors++;
               break;
            }
            configp->max_dusk = value;
            break;

         case Tty :
            (void) strncpy2(configp->tty, tokv[0], sizeof(config.tty) - 1);

	    for ( j = 1; j < tokc; j++) {
	       strupper(tokv[j]);
               if ( strncmp(tokv[j], "CM10A", 4) == 0 ) {
	          configp->device_type |= DEV_CM10A;
	       }
	       else if ( strncmp(tokv[j], "CM17A", 4) == 0 ) {
                  configp->device_type |= DEV_CM17A;
	       }
	       else if ( !(strncmp(tokv[j], "CM11A", 4) == 0 ||
			   strncmp(tokv[j], "CM12U", 4) == 0)    ) {
	          sprintf(errbuffer, "Unsupported interface type '%s'", tokv[j]);
		  store_error_message(errbuffer);
	          errors++;
		  break;
	       }
	    }
            break;

         case TtyAux :
            (void) strncpy2(configp->ttyaux, tokv[0], sizeof(config.ttyaux) - 1);

	    strupper(tokv[1]);
            if ( strncmp(tokv[1], "W800RF32", 8) == 0 ) {
	       configp->auxdev = DEV_W800RF32;
	    }
	    else if ( strncmp(tokv[1], "MR26A", 4) == 0 ) {
               configp->auxdev = DEV_MR26A;
	    }
	    else if ( strcmp(tokv[1], "RFXCOM32") == 0 ) {
               configp->auxdev = DEV_RFXCOM32;
	    }
	    else if ( strcmp(tokv[1], "RFXCOM") == 0 ) {
               configp->auxdev = DEF_DEV_RFXCOM;
	    }
	    else if ( strcmp(tokv[1], "RFXCOMVL") == 0 ) {
               configp->auxdev = DEV_RFXCOMVL;
	    }
	    else {
	       sprintf(errbuffer, "Unsupported aux device '%s'", tokv[1]);
               store_error_message(errbuffer);
	       errors++;
               break;
	    }

	    if ( ( configp->auxdev == DEV_RFXCOM32 || configp->auxdev == DEV_RFXCOMVL ) && *configp->ttyaux != '/' ) {
	       sp = strchr(tokv[0], ':');

	       if ( sp ) {
	          j = sp++ - tokv[0];
                  if ( j > sizeof(config.auxhost) - 1 )
	             j = sizeof(config.auxhost) - 1;

                  (void) strncpy2(configp->auxhost, tokv[0], j);
                  (void) strncpy2(configp->auxport, sp, sizeof(config.auxport) - 1);
	       }
	    }

            if ( configp->auxdev == DEV_RFXCOMVL && tokc > 2 ) {
               strupper(tokv[2]);
               if ( strcmp(tokv[2], "VISONIC") == 0 )
                  configp->rfx_master = VISONIC;
               else if ( strcmp(tokv[2], "X10") == 0 )
                  configp->rfx_master = RFXX10;
               else {
                  sprintf(errbuffer, "Invalid RFXCOM receiver type '%s'", tokv[2]);
                  store_error_message(errbuffer);
                  errors++;
                  break;
               }
               if ( tokc > 3 ) {
                  strupper(tokv[3]);
                  if ( strcmp(tokv[3], "VISONIC") == 0 )
                     configp->rfx_slave = VISONIC;
                  else if ( strcmp(tokv[3], "X10") == 0 )
                     configp->rfx_slave = RFXX10;
                  else {
                     sprintf(errbuffer, "Invalid RFXCOM receiver type '%s'", tokv[3]);
                     store_error_message(errbuffer);
                     errors++;
                     break;
                  }
               }
            }
                  
            break;

         case TtyRFXmit :
            strncpy2(configp->ttyrfxmit, tokv[0], sizeof(config.ttyrfxmit) - 1);
            if ( strncmp(tokv[1], "310", 3) == 0 )
               configp->rfxmit_freq = MHZ310;
            else if ( strncmp(tokv[1], "433", 3) == 0 )
               configp->rfxmit_freq = MHZ433;
            else {
               store_error_message("TTY_RFXMIT frequency parameter must be 310 or 433");
               errors++;
               break;
            }

            break;

         case HouseCode : /* Base housecode */
            hc = toupper((int)(*tokv[0]));
            if ( (int)strlen(tokv[0]) > 1 || hc < 'A' || hc > 'Z' ) {
               store_error_message("Invalid HOUSECODE - must be A though P");
               errors++;
               break;
            }
            configp->housecode = hc;
            default_housecode = hc;
            break;

         case ForceAddr :
            (void) strupper(tokv[0]);
            if ( !strcmp(tokv[0], "YES") )
               configp->force_addr = YES;
            else if ( !strcmp(tokv[0], "NO") )
               configp->force_addr = NO;
            else {
               store_error_message("FORCE_ADDR must be YES or NO");
               errors++;
            }
            break;

         case NewFormat : /* New Format - allow for additional choices in future */
            if ( (tokc + 1) == 1 )
               configp->newformat = 1;
            else
               configp->newformat = (unsigned char)strtol(tokv[0], NULL, 10);
            break;

         case CheckFiles :
            (void) strupper(tokv[0]);
            if ( !strcmp(tokv[0], "YES") )
               configp->checkfiles = YES;
            else if ( !strcmp(tokv[0], "NO") )
               configp->checkfiles = NO;
            else {
               store_error_message("WRITE_CHECK_FILES must be YES or NO");
               errors++;
            }
            break;

         case ReportPath : /* Alternate path for report files */
            (void)strncpy2(alt_path, tokv[0], sizeof(alt_path) - 1);
            if ( alt_path[strlen(alt_path) - 1] != '/' )
               (void)strncat(alt_path, "/", sizeof(alt_path) - 1 - strlen(alt_path));
            break;

         case ReplDelay :
            (void) strupper(tokv[0]);
            if ( !strcmp(tokv[0], "YES") )
               configp->repl_delay = YES;
            else if ( !strcmp(tokv[0], "NO") )
               configp->repl_delay = NO;
            else {
               store_error_message("REPL_DELAYED_MACROS must be YES or NO");
               errors++;
            }
            break;

         case ResvTimers :
            (void) strupper(tokv[0]);
            configp->reserved_timers = (int)strtol(tokv[0], &sp, 10);

            if ( !strchr(" \t\n", *sp) || 
	       configp->reserved_timers < 0 || configp->reserved_timers > 50 ) {
               store_error_message("RESERVED_TIMERS outside range 0-50");
               errors++;
            }
            break;

         case TrigTag :
            (void) strupper(tokv[0]);
            if ( !strcmp(tokv[0], "YES") )
               configp->trigger_tag = YES;
            else if ( !strcmp(tokv[0], "NO") )
               configp->trigger_tag = NO;
            else {
               store_error_message("TRIGGER_TAG must be YES or NO");
               errors++;
            }
            break;

         case MaxPParms :
            (void) strupper(tokv[0]);
            configp->max_pparms = (int)strtol(tokv[0], &sp, 10);

            if ( !strchr(" \t\n", *sp) ||
	       configp->max_pparms < 1 || configp->max_pparms > 999 ) {
               store_error_message("MAX_PPARMS outside range 1-999");
               errors++;
            }
            break;

         case RcsTemp :
            /* Housecodes for which Preset commands received from   */
            /* thermostats are to be decoded into temperature.      */
            /* Information is stored as a housecode bitmap.         */
            sp = tokv[0];
            (void) strlower(sp);
            configp->rcs_temperature = 0;
            if ( !strcmp(sp, "off") || !strcmp(sp, "none") ) {
               break;
            }
            if ( !strcmp(sp, "all") || !strcmp(sp, "yes") ) {
               configp->rcs_temperature = 0xffff;
               break;
            }

            len = strlen(sp);
            if ( *sp == '[' && *(sp + len - 1) == ']' ) {
               /* Create the housecode bitmap */               
               for ( j = 1; j < len - 1; j++ ) {
                  /* Ignore spaces and commas */
                  if ( sp[j] == ' ' || sp[j] == ',' )
                     continue;
                  if ( sp[j] < 'a' || sp[j] > 'p' )
                     break;
                  configp->rcs_temperature |= (1 << hc2code(sp[j]));
               }
               if ( j != len - 1 ) {
                  store_error_message("Invalid character in RCS_DECODE housecode list");
                  errors++;
                  break;
               }
               break;
            }
            store_error_message("RCS_DECODE must be OFF or [<housecode list>] or ALL");
            errors++;
            break;

         case TransMissp :
            store_error_message("TRANSCEIVE misspelled");
         case Transceive :
            value = 0;
            /* Housecodes to be transceived by the aux daemon */
            sp = tokv[0];
            (void) strlower(sp);
            if ( !strcmp(sp, "all") ) {
               configp->transceive = TR_ALL;
               break;
            }

            if ( !strcmp(sp, "none") ) {
               configp->transceive = TR_NONE;
               break;
            }

            if ( !strcmp(sp, "allexcept") ) {
               if ( tokc < 2 ) {
                  store_error_message("Too few parameters for TRANSCEIVE ALLEXCEPT");
                  errors++;
                  break;
               }
               sp = tokv[1];
               strlower(sp);
               value = 1;
            }

            len = strlen(sp);
            if ( *sp == '[' && *(sp + len - 1) == ']' ) {
               /* Create the housecode bitmap */
               configp->transceive = 0; 
               for ( j = 1; j < len - 1; j++ ) {
                  /* Ignore periods and commas */
                  if ( sp[j] == '.' || sp[j] == ',' )
                     continue;
                  if ( sp[j] < 'a' || sp[j] > 'p' ) {
                     store_error_message("Invalid character in TRANSCEIVE housecode list");
                     errors++;
                     break;
                  }
                  configp->transceive |= (1 << hc2code(sp[j]));
               }
               if ( value == 1 )
                  configp->transceive = 0xffff & ~configp->transceive;

               break;
            }
            store_error_message("TRANSCEIVE must be ALL or NONE or ALLEXCEPT [<housecode list>]");
            errors++;
            break;

         case RFForward :
            value = 0;
            /* Housecodes to be transceived by the aux daemon */
            sp = tokv[0];
            (void) strlower(sp);
            if ( !strcmp(sp, "all") ) {
               configp->rfforward = TR_ALL;
               break;
            }

            if ( !strcmp(sp, "none") ) {
               configp->rfforward = TR_NONE;
               break;
            }

            if ( !strcmp(sp, "allexcept") ) {
               if ( tokc < 2 ) {
                  store_error_message("Too few parameters for RFFORWARD ALLEXCEPT");
                  errors++;
                  break;
               }
               sp = tokv[1];
               strlower(sp);
               value = 1;
            }

            len = strlen(sp);
            if ( *sp == '[' && *(sp + len - 1) == ']' ) {
               /* Create the housecode bitmap */
               configp->rfforward = 0; 
               for ( j = 1; j < len - 1; j++ ) {
                  /* Ignore periods and commas */
                  if ( sp[j] == '.' || sp[j] == ',' )
                     continue;
                  if ( sp[j] < 'a' || sp[j] > 'p' ) {
                     store_error_message("Invalid character in RFFORWARD housecode list");
                     errors++;
                     break;
                  }
                  configp->rfforward |= (1 << hc2code(sp[j]));
               }
               if ( value == 1 )
                  configp->rfforward = 0xffff & ~configp->rfforward;

               break;
            }
            store_error_message("RFFORWARD must be ALL or NONE or ALLEXCEPT [<housecode list>]");
            errors++;
            break;


         case TransDim :
            configp->trans_dim = (unsigned char)strtol(tokv[0], &sp, 10);

            if ( !strchr(" \t\n", *sp) ||
	       configp->trans_dim < 1 || configp->trans_dim > 22 ) {
               store_error_message("TRANS_DIMLEVEL outside range 1-22");
               errors++;
            }
            break;

         case StatusTimeout :
            configp->status_timeout = (int)strtol(tokv[0], &sp, 10);

            if ( !strchr(" \t\n", *sp) ||
	       configp->status_timeout < 1 || configp->status_timeout > 5 ) {
               store_error_message("STATUS_TIMEOUT outside range 1-5");
               errors++;
            }
            break;


         case SpfTimeout :
            configp->spf_timeout = (int)strtol(tokv[0], &sp, 10);

            if ( !strchr(" \t\n", *sp) ||
	       configp->spf_timeout < 1 || configp->spf_timeout > 10 ) {
               store_error_message("SPF_TIMEOUT outside range 1-10");
               errors++;
            }
            break;

         case XrefApp :
            (void) strupper(tokv[0]);
            if ( !strcmp(tokv[0], "YES") )
               configp->xref_append = YES;
            else if ( !strcmp(tokv[0], "NO") )
               configp->xref_append = NO;
            else {
               store_error_message("XREF_APPEND must be YES or NO");
               errors++;
            }
            break;

         case AckHails :
            (void) strupper(tokv[0]);
            if ( !strcmp(tokv[0], "YES") )
               configp->ack_hails = YES;
            else if ( !strcmp(tokv[0], "NO") )
               configp->ack_hails = NO;
            else {
               store_error_message("ACK_HAILS must be YES or NO");
               errors++;
            }
            break;

         case DispExpMac :
            (void) strupper(tokv[0]);
            if ( !strcmp(tokv[0], "YES") )
               configp->disp_exp_mac = YES;
            else if ( !strcmp(tokv[0], "NO") )
               configp->disp_exp_mac = NO;
            else {
               store_error_message("DISPLAY_EXP_MACROS must be YES or NO");
               errors++;
            }
            break;

         case MacTerm :
            (void) strupper(tokv[0]);
            if ( !strcmp(tokv[0], "YES") )
               configp->macterm = YES;
            else if ( !strcmp(tokv[0], "NO") )
               configp->macterm = NO;
            else {
               store_error_message("MACTERM must be YES or NO");
               errors++;
            }
            break;

         case AutoChain :
            (void) strupper(tokv[0]);
            if ( !strcmp(tokv[0], "YES") )
               configp->auto_chain = YES;
            else if ( !strcmp(tokv[0], "NO") )
               configp->auto_chain = NO;
            else {
               store_error_message("AUTO_CHAIN must be YES or NO");
               errors++;
            }
            break;

         case Launcher :
            if ( add_launchers(&configp->launcherp, line_no, bufp) < 0 )
               errors++;
            break;

         case Script :
            if ( add_script(&configp->scriptp, &configp->launcherp, line_no, bufp) < 0 )
               errors++;
            break;

         case ModuleTypes :
            (void) strupper(tokv[0]);
            if ( !strcmp(tokv[0], "YES") )
               configp->module_types = YES;
            else if ( !strcmp(tokv[0], "NO") )
               configp->module_types = NO;
            else {
               store_error_message("MODULE_TYPES must be YES or NO");
               errors++;
            }
            break;

         case FunctionMode :
            (void) strupper(tokv[0]);
            if ( !strcmp(tokv[0], "ACTUAL") )
               configp->function_mode = FUNC_ACTUAL;
            else if ( !strcmp(tokv[0], "GENERIC") )
               configp->function_mode = FUNC_GENERIC;
            else {
               store_error_message("FUNCTION_MODE must be ACTUAL or GENERIC");
               errors++;
            }
            break;

         case LaunchMode :
            (void) strupper(tokv[0]);
            if ( !strcmp(tokv[0], "SIGNAL") )
               configp->launch_mode = TMODE_SIGNAL;
            else if ( !strcmp(tokv[0], "MODULE") )
               configp->launch_mode = TMODE_MODULE;
            else {
               store_error_message("LAUNCH_MODE must be SIGNAL or MODULE");
               errors++;
            }
            break;

         case LaunchSrc :
            value = 0;
            for ( j = 0; j < tokc; j++ ) {
               strncpy2(token, tokv[j], sizeof(token));
               strupper(token);
               if ( strcmp(token, "SNDS") == 0 ) {
                  sprintf(errbuffer, "LAUNCH_SOURCE '%s' not allowed as a default.", tokv[j]);
                  store_error_message(errbuffer);
                  errors++;
                  break;
               }
               for ( k = 0; k < nlsopt; k++ ) {
                  if ( strcmp(token, ls_option[k].label) == 0 ) { 
                     value |= ls_option[k].value;
                     break;
                  }
               }
               if ( k >= nlsopt ) {
                  sprintf(errbuffer, "Invalid LAUNCH_SOURCE option '%s'", tokv[j]);
                  store_error_message(errbuffer);
                  errors++;
                  break;
               }
            }
            if ( value & LSNONE ) {
               if ( value & ~LSNONE ) 
                  store_error_message("Warning: LAUNCH_SOURCE 'nosrc' cancels all others on line.");
               configp->launch_source = 0;
            }
            else
               configp->launch_source = (unsigned int)value;
            break;

         case ResOverlap :
            (void) strupper(tokv[0]);
            if ( !strcmp(tokv[0], "OLD") )
               configp->res_overlap = RES_OVLAP_COMBINED;
            else if ( !strcmp(tokv[0], "NEW") )
               configp->res_overlap = RES_OVLAP_SEPARATE;
            else {
               store_error_message("RESOLVE_OVERLAP must be OLD or NEW");
               errors++;
            }
            break;

         case DefaultModule :
            if ( (configp->default_module = lookup_module_type(tokv[0])) < 0 ) {
               sprintf(errbuffer, "Module type '%s' is unknown.", tokv[0]);
               store_error_message(errbuffer);
               errors++;
            }
            break;
        
         case ScriptShell :
            if ( access(tokv[0], X_OK) == 0 ) {
              strncpy2(configp->script_shell, tokv[0], sizeof(config.script_shell) - 1);
            }
            else {
              sprintf(errbuffer,
                 "An executable shell '%s' is not found.", tokv[0]);
              store_error_message(errbuffer);
              errors++;
            }
            break;
         
         case ScriptMode :
            (void) strupper(tokv[0]);
            if ( !strcmp(tokv[0], "HEYUHELPER") )
               configp->script_mode = HEYU_HELPER;
            else if ( !strncmp(tokv[0], "SCRIPT", 6) )
               configp->script_mode = HEYU_SCRIPT;
            else {
               store_error_message("SCRIPT_MODE must be HEYUHELPER or SCRIPT");
               errors++;
            }
            break;

         case LogDir :
            strncpy2(token, tokv[0], sizeof(token) - 1);
            strupper(token);
            if ( strcmp(token, "NONE") == 0 ) {
               *configp->logfile = '\0';
               break;
            }

            strncpy2(token, tokv[0], sizeof(token) - 1);
            strcat(token, "/");
            if ( check_dir_rw(token, "LOG_DIR") != 0 ) {
#if 0
               store_error_message(
                  "LOG_DIR does not exist or is not writable.");
#endif
               errors++;
	    }
	    else {     
               sprintf(configp->logfile, "%s/%s",
		 tokv[0], LOGFILE);
	    }

            if ( tokc == 2 ) {
               strncpy2(token, tokv[1], sizeof(token) - 1);
               strupper(token);
               if ( strcmp(token, "COMMON") == 0 ) {
                  configp->logcommon = YES;
               }
               else {
                  sprintf(errbuffer,
                    "Token '%s' not recognized. Do you mean COMMON ?", tokv[1]);
                  store_error_message(errbuffer);
                  errors++;
               }
            }
               
            break;

         case DispSubdir :
            strupper(tokv[0]);
            if ( !strcmp(tokv[0], "YES") )
               configp->disp_subdir = YES;
            else if ( !strcmp(tokv[0], "NO") )
               configp->disp_subdir = NO;
            else {
               store_error_message("DISPLAY_SUBDIR must be YES or NO");
               errors++;
            }
            break;


         case IsDarkOffset :
            configp->isdark_offset = (int)strtol(tokv[0], &sp, 10);

            if ( !strchr(" \t\n", *sp) ||
	         configp->isdark_offset < -360 || configp->isdark_offset > 360 ) {
               store_error_message("ISDARK_OFFSET outside range +/-360 minutes");
               errors++;
            }
            break;

	 case EnvAliasPrefix :
	    strupper(tokv[0]);
	    if ( strcmp(tokv[0], "UC") == 0 )
               strncpy2(configp->env_alias_prefix, "X10", sizeof(config.env_alias_prefix) - 1);
	    else if ( strcmp(tokv[0], "LC") == 0 )
	       strncpy2(configp->env_alias_prefix, "x10", sizeof(config.env_alias_prefix) - 1);
	    else {
	       store_error_message("ENV_ALIAS_PREFIX must be UC or LC");
	       errors++;
	    }
	    break;

	 case SunMode :
	    strupper(tokv[0]);
            if ( tokc == 1 ) {
	       if ( strncmp(tokv[0], "RISESET", 1) == 0 ) {
	          configp->sunmode = RiseSet;
		  break;
               }
	       else if ( strncmp(tokv[0], "CIVILTWI", 1) == 0 ) {
	          configp->sunmode = CivilTwi;
		  break;
               }
	       else if ( strncmp(tokv[0], "NAUTTWI", 1) == 0 ) {
	          configp->sunmode = NautTwi;
		  break;
               }
	       else if ( strncmp(tokv[0], "ASTROTWI", 1) == 0 ) {
	          configp->sunmode = AstroTwi;
		  break;
               }
	    }
            else {
	       sp = NULL;
	       if ( strncmp(tokv[0], "OFFSET", 1) == 0 ) {
	          configp->sunmode = AngleOffset;
                  configp->sunmode_offset = (int)strtol(tokv[1], &sp, 10);
               }
               if ( sp && strchr(" \t\n", *sp) )
	          break;
            }
	    store_error_message("DAWNDUSK_DEF must be R, C, N, A or O <int>");
	    errors++;
	    break;

         case Fix5A :
            (void) strupper(tokv[0]);
            if ( !strcmp(tokv[0], "YES") )
               configp->fix_5a = YES;
            else if ( !strcmp(tokv[0], "NO") )
               configp->fix_5a = NO;
            else {
               store_error_message("FIX_5A must be YES or NO");
               errors++;
            }
            break;

	 case CM11PostDelay :
	    configp->cm11_post_delay = (int)strtol(tokv[0], &sp, 10);
	    if ( !strchr(" \t\n", *sp) ||
                 configp->cm11_post_delay < 0 ||
                 configp->cm11_post_delay > 1000 ) {
	       store_error_message("CM11_POST_DELAY must be 0-1000");
	       errors++;
	    }
	    break;
	    
         case AutoFetch :
            (void) strupper(tokv[0]);
            if ( !strcmp(tokv[0], "YES") )
               configp->autofetch = YES;
            else if ( !strcmp(tokv[0], "NO") )
               configp->autofetch = NO;
            else {
               store_error_message("AUTOFETCH must be YES or NO");
               errors++;
            }
            break;

         case PfailUpdate :
            (void) strupper(tokv[0]);
            if ( !strcmp(tokv[0], "YES") )
               configp->pfail_update = YES;
            else if ( !strcmp(tokv[0], "NO") )
               configp->pfail_update = NO;
            else {
               store_error_message("POWERFAIL_UPDATE must be YES or NO");
               errors++;
            }
            break;

	 case BitDelay :
	    configp->cm17a_bit_delay = (int)strtol(tokv[0], &sp, 10);
	    if ( !strchr(" \t\n", *sp) ||
                 configp->cm17a_bit_delay < 100 ||
                 configp->cm17a_bit_delay > 10000 ) {
	       store_error_message("CM17A_BIT_DELAY must be 100-10000");
	       errors++;
	    }
	    break;

         case BurstSpacing :
            configp->rf_burst_spacing = (int)strtol(tokv[0], &sp, 10);
	    if ( !strchr(" \t\n", *sp) ||
                 configp->rf_burst_spacing < 80 ||
                 configp->rf_burst_spacing > 160 ) {
	       store_error_message("RF_BURST_SPACING must be 80-160");
	       errors++;
	    }
	    break;

         case TimerTweak :
            configp->rf_timer_tweak = (int)strtol(tokv[0], &sp, 10);
	    if ( !strchr(" \t\n", *sp) ||
                 configp->rf_timer_tweak < 0 ||
                 configp->rf_timer_tweak > 50 ) {
	       store_error_message("RF_TIMER_TWEAK must be 0-50");
	       errors++;
	    }
	    break;
            	    
	 case RFPostDelay :
	    configp->rf_post_delay = (int)strtol(tokv[0], &sp, 10);
	    if ( !strchr(" \t\n", *sp) ||
                 configp->rf_post_delay < 0 ||
                 configp->rf_post_delay > 10000 ) {
	       store_error_message("RF_POST_DELAY must be 0-10000");
	       errors++;
	    }
	    break;
	    
	 case RFFarbDelay :
	    configp->rf_farb_delay = (int)strtol(tokv[0], &sp, 10);
	    if ( !strchr(" \t\n", *sp) ||
                 configp->rf_farb_delay < 0 ||
                 configp->rf_farb_delay > 10000 ) {
	       store_error_message("RF_FARB_DELAY must be 0-10000");
	       errors++;
	    }
	    break;
	    
	 case RFFarwDelay :
	    configp->rf_farw_delay = (int)strtol(tokv[0], &sp, 10);
	    if ( !strchr(" \t\n", *sp) ||
                 configp->rf_farw_delay < 0 ||
                 configp->rf_farw_delay > 10000 ) {
	       store_error_message("RF_FARW_DELAY must be 0-10000");
	       errors++;
	    }
	    break;
	    
         case DispRFX :
            (void) strupper(tokv[0]);
            if ( !strcmp(tokv[0], "YES") )
               configp->disp_rf_xmit = YES;
            else if ( !strcmp(tokv[0], "NO") )
               configp->disp_rf_xmit = NO;
	    else if ( !strcmp(tokv[0], "VERBOSE") )
	       configp->disp_rf_xmit = VERBOSE;
            else {
               store_error_message("DISPLAY_RF_XMIT must be NO or YES or VERBOSE");
               errors++;
            }
            break;

	 case DefRFBursts :
	    configp->def_rf_bursts = (int)strtol(tokv[0], &sp, 10);
	    if ( !strchr(" \t\n", *sp) ||
                 configp->def_rf_bursts < 5 ||
                 configp->def_rf_bursts > 6 ) {
	       store_error_message("DEF_RF_BURSTS must be 5 or 6");
	       errors++;
	    }
	    break;

	 case RFBursts :
            if ( tokc % 2 ) {
               store_error_message("Missing RF_BURSTS parameter");            
	       errors++;
	       break;
	    }

            for ( j = 0; j < tokc; j+=2 ) {
  	       bursts = strtol(tokv[j + 1], &sp, 10);
	       if ( !strchr(" \t\r\n", *sp) || bursts < 1 ) {
	          sprintf(errbuffer, "Invalid RF_BURSTS bursts '%s'", tokv[j + 1]);
	          store_error_message(errbuffer);
	          errors++;
	          break;
	       }
               strlower(tokv[j]);
               for ( k = 0; k < nrflabels; k++ ) {
                  if ( !strcmp(tokv[j], rf_label[k].label) ) {
                     configp->rf_bursts[rf_label[k].subcode] = bursts;
                     break;
                  }
               }
               if ( k >= nrflabels ) {
	          sprintf(errbuffer, "Unknown CM17A function '%s'", tokv[j]);
	          store_error_message(errbuffer);
	          errors++;
                  break;
	       }
            }

	    break;

#if 0
	 case RFBursts :
	    bursts = strtol(tokv[1], &sp, 10);
	    if ( !strchr(" \t\n", *sp) || bursts < 1 ) {
	       store_error_message("Invalid RF_BURSTS");
	       errors++;
	       break;
	    }
	    (void) strlower(tokv[0]);
	    for ( j = 0; j < nrflabels; j++ ) {
	       if ( !strcmp(tokv[0], rf_label[j].label) ) { 
	          configp->rf_bursts[rf_label[j].subcode] = bursts;
		  break;
	       }
	    }
	    if ( j >= nrflabels ) {
	       sprintf(errbuffer, "Unknown CM17A function '%s'", tokv[0]);
	       store_error_message(errbuffer);
	       errors++;
	    }
	    break;
#endif

	 case LoopCount :
	    configp->timer_loopcount = (unsigned long)strtol(tokv[0], &sp, 10);
	    if ( !strchr(" \t\n", *sp) ) {
	       store_error_message("Invalid TIMER_LOOPCOUNT");
	       errors++;
	       break;
	    }
	    break;

	 case RestrictDims :
            (void) strupper(tokv[0]);
            if ( !strcmp(tokv[0], "YES") )
               configp->restrict_dims = YES;
            else if ( !strcmp(tokv[0], "NO") )
               configp->restrict_dims = NO;
            else {
               store_error_message("RESTRICT_DIMS must be YES or NO");
               errors++;
            }
            break;

	 case StateFmt :
            (void) strupper(tokv[0]);
            if ( !strcmp(tokv[0], "NEW") )
               configp->state_format = NEW;
            else if ( !strcmp(tokv[0], "OLD") )
               configp->state_format = OLD;
            else {
               store_error_message("STATE_FORMAT must be NEW or OLD");
               errors++;
            }
            break;

         case PfailScript :
            if ( !(configp->pfail_script = strdup(bufp)) ) {
               store_error_message("Memory allocation error - out of memory");
               errors++;
            }
            break;
	    
	 case StartEngine :
            (void) strupper(tokv[0]);
            if ( !strncmp(tokv[0], "MANUAL", 3) )
               configp->start_engine = MANUAL;
            else if ( !strncmp(tokv[0], "AUTOMATIC", 4) )
               configp->start_engine = AUTOMATIC;
            else {
               store_error_message("START_ENGINE must be MANUAL or AUTO");
               errors++;
            }
            break;

	 case IgnoreSilent :
            break;

	 case NoSwitch :
            (void) strupper(tokv[0]);
            if ( !strcmp(tokv[0], "YES") )
               configp->rf_noswitch = YES;
            else if ( !strcmp(tokv[0], "NO") )
               configp->rf_noswitch = NO;
            else {
               store_error_message("RF_NOSWITCH must be YES or NO");
               errors++;
            }
            break;

         case RespoolPerms :
#ifndef RESPOOL
            store_error_message(
              "The RESPOOL_PERMISSIONS directive is invalid for this operating system");
            errors++;
            break;
#endif
            perms = (int)strtol(tokv[0], &sp, 8);
            if ( !strchr(" \t\n", *sp) || perms < 0 || perms > 07777) {
               store_error_message("RESPOOL_PERMISSIONS - invalid octal number");
               errors++;
            }
            configp->respool_perms = (unsigned int)perms;
            break;

	 case SpoolMax :
	    configp->spool_max = strtol(tokv[0], &sp, 10);
	    if ( !strchr(" \t\n", *sp) ||
                 configp->spool_max < SPOOLFILE_ABSMIN ||
                 configp->spool_max > SPOOLFILE_ABSMAX ) {
               sprintf(errbuffer, "SPOOLFILE_MAX must be between %ul and %ul",
                 SPOOLFILE_ABSMIN,  SPOOLFILE_ABSMAX);
	       store_error_message(errbuffer);
	       errors++;
	    }
	    break;
	    
	 case CheckRILine :
            (void) strupper(tokv[0]);
            if ( !strcmp(tokv[0], "YES") )
               configp->check_RI_line = YES;
            else if ( !strcmp(tokv[0], "NO") )
               configp->check_RI_line = NO;
            else {
               store_error_message("CHECK_RI_LINE must be YES or NO");
               errors++;
            }
            break;

	 case RIdisable :
            (void) strupper(tokv[0]);
            if ( !strcmp(tokv[0], "YES") )
               store_error_message("Obsolete - use RING_CTRL DISABLE");
            else if ( !strcmp(tokv[0], "NO") )
               store_error_message("Obsolete - use RING_CTRL ENABLE");
            else {
               store_error_message("Obsolete - Use RING_CTRL ENABLE or DISABLE");
            }
            errors++;
            break;

	 case RingCtrl :
            (void) strupper(tokv[0]);
            if ( !strcmp(tokv[0], "ENABLE") )
               configp->ring_ctrl = ENABLE;
            else if ( !strcmp(tokv[0], "DISABLE") )
               configp->ring_ctrl = DISABLE;
            else {
               store_error_message("RING_CTRL must be ENABLE or DISABLE");
               errors++;
            }
            break;

         case SendRetries :
	    configp->send_retries = (int)strtol(tokv[0], &sp, 10);
	    if ( !strchr(" \t\n", *sp) ||
                 configp->send_retries < 0 ) {
	       store_error_message("SEND_RETRIES must be > 0");
	       errors++;
	    }
	    break;

	 case ScriptCtrl :
            (void) strupper(tokv[0]);
            if ( !strcmp(tokv[0], "ENABLE") )
               configp->script_ctrl = ENABLE;
            else if ( !strcmp(tokv[0], "DISABLE") )
               configp->script_ctrl = DISABLE;
            else {
               store_error_message("SCRIPT_CTRL must be ENABLE or DISABLE");
               errors++;
            }
            break;

	 case StateCtrl :
            (void) strupper(tokv[0]);
            if ( !strcmp(tokv[0], "SINGLE") )
               configp->state_ctrl = SC_SINGLE;
            else if ( !strcmp(tokv[0], "BITMAP") )
               configp->state_ctrl = SC_BITMAP;
            else {
               store_error_message("STATE_CTRL must be SINGLE or BITMAP");
               errors++;
            }
            break;

         case RFFuncMask :
            store_error_message("RF_FUNCMASK not yet implemented");
            errors++;
            break;

         case HideUnchg :
            (void) strupper(tokv[0]);
            if ( !strcmp(tokv[0], "YES") )
               configp->hide_unchanged = YES;
            else if ( !strcmp(tokv[0], "NO") )
               configp->hide_unchanged = NO;
            else {
               store_error_message("HIDE_UNCHANGED must be YES or NO");
               errors++;
            }
            break;

         case HideUnchgInactive :
            (void) strupper(tokv[0]);
            if ( !strcmp(tokv[0], "YES") )
               configp->hide_unchanged_inactive = YES;
            else if ( !strcmp(tokv[0], "NO") )
               configp->hide_unchanged_inactive = NO;
            else {
               store_error_message("HIDE_UNCHANGED_INACTIVE must be YES or NO");
               errors++;
            }
            break;

         case ShowChange :
            (void) strupper(tokv[0]);
            if ( !strcmp(tokv[0], "YES") )
               configp->show_change = YES;
            else if ( !strcmp(tokv[0], "NO") )
               configp->show_change = NO;
            else {
               store_error_message("SHOW_CHANGE must be YES or NO");
               errors++;
            }
            break;

         case AuxRepcounts :
            for ( j = 0; j < 3; j++ ) {
               value = (int)strtol(tokv[j], &sp, 10);
               if ( strchr(" \t\n", *sp) == NULL || value < 0 ) {
                  store_error_message("Invalid AUX_REPCOUNTS");
                  errors++;
                  break;
               }
               configp->aux_repcounts[j] = value;
            }
            if ( configp->aux_repcounts[0] < 1 ) {
               store_error_message("AUX_REPCOUNTS <min count> value must be greater than zero");
               errors++;
               break;
            }
            break;

         case AuxMincountRFX :
            value = (int)strtol(tokv[0], &sp, 10);
            if ( strchr(" \t\n", *sp) == NULL || value < 1 || value > 3 ) {
               store_error_message("AUX_MINCOUNT_RFX out of range 1-3");
               errors++;
               break;
            }
            configp->aux_mincount_rfx = value;
            break;

         case DispRFNoise :
            (void) strupper(tokv[0]);
            if ( !strcmp(tokv[0], "YES") )
               configp->disp_rf_noise = YES;
            else if ( !strcmp(tokv[0], "NO") )
               configp->disp_rf_noise = NO;
            else {
               store_error_message("DISPLAY_RF_NOISE must be YES or NO");
               errors++;
            }
            break;

         case ArmMaxDelay :
            if ( (longvalue = parse_hhmmss(tokv[0], 3)) >= 0 && longvalue < 43200 ) {
               configp->arm_max_delay = (int)longvalue;
            }
            else {
               store_error_message("Invalid ARM_MAX_DELAY");
               errors++;
            }
	    break;

          case ArmLogic :
            strupper(tokv[0]);
            configp->arm_logic =
               (strcmp(tokv[0], "STRICT") == 0) ?  ArmLogicStrict :
               (strcmp(tokv[0], "MEDIUM") == 0) ?  ArmLogicMedium :
               (strcmp(tokv[0], "LOOSE")  == 0) ?  ArmLogicLoose  : 0;

            if ( configp->arm_logic == 0 ) {
               store_error_message("ARM_LOGIC must be STRICT, MEDIUM or LOOSE");
               errors++;
            }
	    break;

         case InactiveTimeout :
            if ( (longvalue = parse_hhmmss(tokv[0], 3)) >= 0 && longvalue < 86400 ) {
               configp->inactive_timeout = longvalue;
            }
            else {
               store_error_message("Invalid INACTIVE_TIMEOUT");
               errors++;
            }
	    break;

         case InactiveTimeoutOre :
            if ( (longvalue = parse_hhmmss(tokv[0], 3)) >= 0 && longvalue < 86400 ) {
               configp->inactive_timeout_ore = longvalue;
            }
            else {
               store_error_message("Invalid INACTIVE_TIMEOUT_ORE");
               errors++;
            }
	    break;

         case DispRawRF :
            (void) strupper(tokv[0]);

            if ( !strcmp(tokv[0], "NONE") )
               configp->disp_raw_rf = DISPMODE_RF_NONE;
            else if ( !strcmp(tokv[0], "NOISE") )
               configp->disp_raw_rf = DISPMODE_RF_NOISE;
            else if ( !strcmp(tokv[0], "ALL") )
               configp->disp_raw_rf = (DISPMODE_RF_NORMAL | DISPMODE_RF_NOISE);
            else {
               store_error_message("DISPLAY_RAW_RF must be NONE or NOISE or ALL");
               errors++;
            }
            break;

         case HeyuUmask :
            if ( strcmp(strupper(tokv[0]), "OFF") == 0 ) {
               configp->heyu_umask = -1;
            }
            else {
               configp->heyu_umask = (int)strtol(tokv[0], &sp, 8);
               if ( !strchr(" \t\r\n", *sp) || configp->heyu_umask < 0 ) {
                  store_error_message("Invalid HEYU_UMASK value.");
                  errors++;
               }
            }
            break;

         case AutoWait :
            configp->auto_wait = (int)strtol(tokv[0], &sp, 10);
            if ( !strchr(" \t\n\r", *sp) ||
                 configp->auto_wait < 0 || configp->auto_wait > 300 ) {
               store_error_message("AUTO_WAIT timeout must be 0-300 seconds.");
               errors++;
            }
            break;

         case FullBright :
            value = (int)strtol(tokv[0], &sp, 10);
            if ( !strchr(" \t\n\r", *sp) || value < 16 || value > 31 ) {
               store_error_message("FULL_BRIGHT level must be 16 though 31");
               errors++;
            }
            configp->full_bright = (unsigned char)value;
            break;

         case EnginePoll :
            configp->engine_poll = strtol(tokv[0], &sp, 10);
            if ( !strchr(" \t\n\r", *sp) ||
                 configp->engine_poll < 100L ||
                 configp->engine_poll > 1000000L ) {
               store_error_message("ENGINE_POLL must be 100 though 1000000");
               errors++;
            }
            break;

         case DmxTscale :
            strupper(tokv[0]);
            if ( strncmp(tokv[0], "FAHRENHEIT", 1) == 0 )
               configp->dmx_tscale = 'F';
            else if ( strncmp(tokv[0], "CELSIUS", 1) == 0 )
               configp->dmx_tscale = 'C';
            else if ( strncmp(tokv[0], "KELVIN", 1) == 0 )
               configp->dmx_tscale = 'K';
            else if ( strncmp(tokv[0], "RANKINE", 1) == 0 )
               configp->dmx_tscale = 'R';
            else {
               store_error_message("DMX_TSCALE must be C, F, K, or R");
               errors++;
            }
            if ( tokc > 1 ) {
               configp->dmx_toffset = strtod(tokv[1], &sp);
               if ( !strchr(" \t\n", *sp) ) {
                  store_error_message("Invalid DMX_TSCALE offset.");
                  errors++;
               }
            }
            break;

         case OreLowBatt :
            value = (int)strtol(tokv[0], &sp, 10);
            if ( !strchr(" %\t\n\r", *sp) || value < 10 || value > 90 ) {
               store_error_message("ORE_LOWBATTERY level must be 10 though 90 percent");
               errors++;
            }
            configp->ore_lobat = (unsigned char)value;
            break;
            

         case OreTscale :
            strupper(tokv[0]);
            if ( strncmp(tokv[0], "FAHRENHEIT", 1) == 0 )
               configp->ore_tscale = 'F';
            else if ( strncmp(tokv[0], "CELSIUS", 1) == 0 )
               configp->ore_tscale = 'C';
            else if ( strncmp(tokv[0], "KELVIN", 1) == 0 )
               configp->ore_tscale = 'K';
            else if ( strncmp(tokv[0], "RANKINE", 1) == 0 )
               configp->ore_tscale = 'R';
            else {
               store_error_message("ORE_TSCALE must be C, F, K, or R");
               errors++;
            }
            if ( tokc > 1 ) {
               configp->ore_toffset = strtod(tokv[1], &sp);
               if ( !strchr(" \t\n", *sp) ) {
                  store_error_message("Invalid ORE_TSCALE offset.");
                  errors++;
               }
            }
            break;

         case OreBPscale :
            strncpy2(configp->ore_bpunits, tokv[0], NAME_LEN - 1);
            configp->ore_bpscale = strtod(tokv[1], &sp);
            if ( !strchr(" \t\n", *sp) ) {
               store_error_message("Invalid ORE_BPSCALE scale factor.");
               errors++;
               break;
            }
            if ( tokc > 2 ) {
               configp->ore_bpoffset = strtod(tokv[2], &sp);
               if ( !strchr(" \t\n", *sp) ) {
                  store_error_message("Invalid ORE_BPSCALE offset.");
                  errors++;
                  break;
               }
            }
            break;

         case OreWgtscale :
            strncpy2(configp->ore_wgtunits, tokv[0], NAME_LEN - 1);
            configp->ore_wgtscale = strtod(tokv[1], &sp);
            if ( !strchr(" \t\n", *sp) ) {
               store_error_message("Invalid ORE_WGTSCALE scale factor.");
               errors++;
            }
            break;

         case OreWindscale :
            strncpy2(configp->ore_windunits, tokv[0], NAME_LEN - 1);
            configp->ore_windscale = strtod(tokv[1], &sp);
            if ( !strchr(" \t\r\n", *sp) ) {
               store_error_message("Invalid ORE_WINDSCALE scale factor.");
               errors++;
            }
            break;

         case OreWindSensorDir :
            dblvalue = strtod(tokv[0], &sp);
            if ( !strchr(" \t\r\n", *sp) || dblvalue < -359.6 || dblvalue > 359.9 ) {
               store_error_message("Invalid ORE_WINDSENSORDIR angle.");
               errors++;
            }
            /* Store as integer decidegrees */
            configp->ore_windsensordir = (int)(dblvalue * 10.0);
            break;

         case OreWindDirMode :
            strupper(tokv[0]);
            if ( !strcmp(tokv[0], "POINTS") )
               configp->ore_winddir_mode = COMPASS_POINTS;
            else if ( !strcmp(tokv[0], "ANGLE") )
               configp->ore_winddir_mode = COMPASS_ANGLE;
            else if ( !strcmp(tokv[0], "BOTH") )
               configp->ore_winddir_mode = (COMPASS_POINTS|COMPASS_ANGLE);
            else {
               store_error_message("ORE_WINDDIR_MODE must be POINTS, ANGLE, or BOTH.");
               errors++;
            }
            break;

         case OreRainRatescale :
            strncpy2(configp->ore_rainrateunits, tokv[0], NAME_LEN - 1);
            configp->ore_rainratescale = strtod(tokv[1], &sp);
            if ( !strchr(" \t\r\n", *sp) ) {
               store_error_message("Invalid ORE_RAINRATESCALE scale factor.");
               errors++;
            }
            break;

         case OreRainTotscale :
            strncpy2(configp->ore_raintotunits, tokv[0], NAME_LEN - 1);
            configp->ore_raintotscale = strtod(tokv[1], &sp);
            if ( !strchr(" \t\r\n", *sp) ) {
               store_error_message("Invalid ORE_RAINTOTSCALE scale factor.");
               errors++;
            }
            break;

         case RfxTscale :
            strupper(tokv[0]);
            if ( strncmp(tokv[0], "FAHRENHEIT", 1) == 0 )
               configp->rfx_tscale = 'F';
            else if ( strncmp(tokv[0], "CELSIUS", 1) == 0 )
               configp->rfx_tscale = 'C';
            else if ( strncmp(tokv[0], "KELVIN", 1) == 0 )
               configp->rfx_tscale = 'K';
            else if ( strncmp(tokv[0], "RANKINE", 1) == 0 )
               configp->rfx_tscale = 'R';
            else {
               store_error_message("RFX_TSCALE must be C, F, K, or R");
               errors++;
            }
            if ( tokc > 1 ) {
               configp->rfx_toffset = strtod(tokv[1], &sp);
               if ( !strchr(" \t\n", *sp) ) {
                  store_error_message("Invalid RFX_TSCALE offset.");
                  errors++;
               }
            }
            break;

        case RfxVadscale :
            strncpy2(configp->rfx_vadunits, tokv[0], NAME_LEN - 1);
            configp->rfx_vadscale = strtod(tokv[1], &sp);
            if ( !strchr(" \t\n", *sp) ) {
               store_error_message("Invalid RFX_VADSCALE scale factor.");
               errors++;
               break;
            }
            if ( tokc > 2 ) {
               configp->rfx_vadoffset = strtod(tokv[2], &sp);
               if ( !strchr(" \t\n", *sp) ) {
                  store_error_message("Invalid RFX_VADSCALE offset.");
                  errors++;
                  break;
               }
            }
            break;
            
        case RfxBPscale :
            strncpy2(configp->rfx_bpunits, tokv[0], NAME_LEN - 1);
            configp->rfx_bpscale = strtod(tokv[1], &sp);
            if ( !strchr(" \t\n", *sp) ) {
               store_error_message("Invalid RFX_BPSCALE scale factor.");
               errors++;
               break;
            }
            if ( tokc > 2 ) {
               configp->rfx_bpoffset = strtod(tokv[2], &sp);
               if ( !strchr(" \t\n", *sp) ) {
                  store_error_message("Invalid RFX_BPSCALE offset.");
                  errors++;
                  break;
               }
            }
            break;

         case RfxComDtrRts :
            value = (int)strtol(tokv[0], &sp, 10);
            if ( !strchr(" \t\n\r", *sp) || value < 0 || value > 3 ) {
               store_error_message("RFXCOM_DTR_RTS must be 0 though 3");
               errors++;
            }
            configp->rfxcom_dtr_rts = (unsigned char)value;
            break;


            break;
            
         case RfxHiBaud :
            (void) strupper(tokv[0]);
            if ( !strcmp(tokv[0], "YES") )
               configp->rfxcom_hibaud = YES;
            else if ( !strcmp(tokv[0], "NO") )
               configp->rfxcom_hibaud = NO;
            else {
               store_error_message("RFXCOM_HIBAUD must be YES or NO");
               errors++;
            }
            break;

        case RfxPowerScale :
            strncpy2(configp->rfx_powerunits, tokv[0], NAME_LEN - 1);
            configp->rfx_powerscale = strtod(tokv[1], &sp);
            if ( !strchr(" \t\n", *sp) ) {
               store_error_message("Invalid RFX_POWERSCALE scale factor.");
               errors++;
               break;
            }
            break;

        case RfxWaterScale :
            strncpy2(configp->rfx_waterunits, tokv[0], NAME_LEN - 1);
            configp->rfx_waterscale = strtod(tokv[1], &sp);
            if ( !strchr(" \t\n", *sp) ) {
               store_error_message("Invalid RFX_WATERSCALE scale factor.");
               errors++;
               break;
            }
            break;

        case RfxGasScale :
            strncpy2(configp->rfx_gasunits, tokv[0], NAME_LEN - 1);
            configp->rfx_gasscale = strtod(tokv[1], &sp);
            if ( !strchr(" \t\n", *sp) ) {
               store_error_message("Invalid RFX_GASSCALE scale factor.");
               errors++;
               break;
            }
            break;

        case RfxPulseScale :
            strncpy2(configp->rfx_pulseunits, tokv[0], NAME_LEN - 1);
            configp->rfx_pulsescale = strtod(tokv[1], &sp);
            if ( !strchr(" \t\n", *sp) ) {
               store_error_message("Invalid RFX_PULSESCALE scale factor.");
               errors++;
               break;
            }
            break;

         case RfxComEnable :
            store_error_message("RFXCOM_ENABLE is obsolete; see RFXCOM_DISABLE");
            break;

#if 0
         case RfxComDisable :
            for ( j = 0; j < tokc; j++ ) {
               strupper(tokv[j]);
               if ( strncmp(tokv[j], "ARCTECH", 3) == 0 ) 
                  configp->rfxcom_disable |= RFXCOM_ARCTECH;
               else if ( strncmp(tokv[j], "OREGON", 3) == 0 )
                  configp->rfxcom_disable |= RFXCOM_OREGON;
               else if ( strncmp(tokv[j], "ATIWONDER", 3) == 0 )
                  configp->rfxcom_disable |= RFXCOM_ATIWONDER;
               else if ( strncmp(tokv[j], "X10", 3) == 0 )
                  configp->rfxcom_disable |= RFXCOM_X10;
               else if ( strncmp(tokv[j], "VISONIC", 3) == 0 )
                  configp->rfxcom_disable |= RFXCOM_VISONIC;
               else if ( strncmp(tokv[j], "KOPPLA", 3) == 0 )
                  configp->rfxcom_disable |= RFXCOM_KOPPLA;
               else if ( strcmp(tokv[j], "HE_UK") == 0 )
                  configp->rfxcom_disable |= RFXCOM_HE_UK;
               else if ( strcmp(tokv[j], "HE_EU") == 0 )
                  configp->rfxcom_disable |= RFXCOM_HE_EU;
               else {
                  sprintf(errbuffer, "Invalid RFXCOM_DISABLE protocol %s", tokv[j]);
                  store_error_message(errbuffer);
                  errors++;
                  break;
               }
            }
            break;
#endif

         case RfxComDisable :
            configp->rfxcom_disable = 0;
            for ( j = 0; j < tokc; j++ ) {
               strupper(tokv[j]);
               for ( k = 0; k < nrfxdisable; k++ ) {
                  if ( strncmp(tokv[j], rfx_disable[k].label, rfx_disable[k].minlabel) == 0 ) {
                     configp->rfxcom_disable |= rfx_disable[k].bitmap;
                     *tokv[j] = '\0';
                     break;
                  }
               }
               if ( *tokv[j] ) {
                  sprintf(errbuffer, "Invalid RFXCOM_DISABLE protocol %s", tokv[j]);
                  store_error_message(errbuffer);
                  errors++;
                  break;
               }
            }
            break;

         case LockupCheck :
            for ( j = 0; j < tokc; j++ ) {
               strupper(tokv[j]);
               if ( strcmp(tokv[j], "YES") == 0 )
                  configp->lockup_check = (CHECK_PORT | CHECK_CM11);
               else if ( strcmp(tokv[j], "PORT") == 0 )
                  configp->lockup_check = CHECK_PORT;
               else if ( strncmp(tokv[j], "CM11A", 4) == 0 )
                  configp->lockup_check = CHECK_CM11;
               else if ( strcmp(tokv[j], "NO") == 0 )
                  configp->lockup_check = 0;
               else {
                  sprintf(errbuffer, "Invalid LOCKUP_CHECK parameter %s", tokv[j]);
                  store_error_message(errbuffer);
                  errors++;
                  break;
               }
            }
            break;

         case TailPath :
            strncpy2(configp->tailpath, tokv[0], sizeof(config.tailpath));
            break;

         case RfxJam :
            (void) strupper(tokv[0]);
            if ( !strcmp(tokv[0], "YES") )
               configp->suppress_rfxjam = YES;
            else if ( !strcmp(tokv[0], "NO") )
               configp->suppress_rfxjam = NO;
            else {
               store_error_message("SUPPRESS_RFXJAM must be YES or NO");
               errors++;
            }
            break;

         case DispDmxTemp :
            (void) strupper(tokv[0]);
            if ( !strcmp(tokv[0], "YES") )
               configp->display_dmxtemp = YES;
            else if ( !strcmp(tokv[0], "NO") )
               configp->display_dmxtemp = NO;
            else {
               store_error_message("DISPLAY_DMXTEMP must be YES or NO");
               errors++;
            }
            break;

         case SecID16 :
            (void) strupper(tokv[0]);
            if ( !strcmp(tokv[0], "YES") )
               configp->securid_16 = YES;
            else if ( !strcmp(tokv[0], "NO") )
               configp->securid_16 = NO;
            else {
               store_error_message("SECURID_16 must be YES or NO");
               errors++;
            }
            break;

         case SecIDPar :
            (void) strupper(tokv[0]);
            if ( !strcmp(tokv[0], "YES") )
               configp->securid_parity = YES;
            else if ( !strcmp(tokv[0], "NO") )
               configp->securid_parity = NO;
            else {
               store_error_message("SECURID_PARITY must be YES or NO");
               errors++;
            }
            break;

         case LogDateYr :
            (void) strupper(tokv[0]);
            if ( !strcmp(tokv[0], "YES") )
               configp->logdate_year = YES;
            else if ( !strcmp(tokv[0], "NO") )
               configp->logdate_year = NO;
            else {
               store_error_message("LOGDATE_YEAR must be YES or NO");
               errors++;
            }
            break;

         case LogDateUnix :
            (void) strupper(tokv[0]);
            if ( !strcmp(tokv[0], "YES") )
               configp->logdate_unix = YES;
            else if ( !strcmp(tokv[0], "NO") )
               configp->logdate_unix = NO;
            else {
               store_error_message("LOGDATE_UNIX must be YES or NO");
               errors++;
            }
            break;

         case DispOreAll :
            (void) strupper(tokv[0]);
            if ( !strcmp(tokv[0], "YES") )
               configp->disp_ore_all = YES;
            else if ( !strcmp(tokv[0], "NO") )
               configp->disp_ore_all = NO;
            else {
               store_error_message("DISPLAY_ORE_ALL must be YES or NO");
               errors++;
            }
            break;

         case OreDispFcast :
            (void) strupper(tokv[0]);
            if ( !strcmp(tokv[0], "YES") )
               configp->ore_display_fcast = YES;
            else if ( !strcmp(tokv[0], "NO") )
               configp->ore_display_fcast = NO;
            else {
               store_error_message("ORE_DISPLAY_FCAST must be YES or NO");
               errors++;
            }
            break;

         case OreChgBitsT :
            value = (int)strtol(tokv[0], &sp, 10);
            if ( !strchr(" \t\n\r", *sp) || value < 1 || value > 255 ) {
               store_error_message("ORE_CHGBITS_T must be 1 though 255");
               errors++;
            }
            configp->ore_chgbits_t = (unsigned char)value;
            break;
            
         case OreChgBitsRH :
            value = (int)strtol(tokv[0], &sp, 10);
            if ( !strchr(" \t\n\r", *sp) || value < 1 || value > 255 ) {
               store_error_message("ORE_CHGBITS_RH must be 1 though 255");
               errors++;
            }
            configp->ore_chgbits_rh = (unsigned char)value;
            break;
            
         case OreChgBitsBP :
            value = (int)strtol(tokv[0], &sp, 10);
            if ( !strchr(" \t\n\r", *sp) || value < 1 || value > 255 ) {
               store_error_message("ORE_CHGBITS_BP must be 1 though 255");
               errors++;
            }
            configp->ore_chgbits_bp = (unsigned char)value;
            break;
            
         case OreChgBitsWgt :
            value = (int)strtol(tokv[0], &sp, 10);
            if ( !strchr(" \t\n\r", *sp) || value < 1 || value > 255 ) {
               store_error_message("ORE_CHGBITS_WGT must be 1 though 255");
               errors++;
            }
            configp->ore_chgbits_wgt = (unsigned char)value;
            break;
            
         case OreChgBitsWsp :
            longvalue = strtol(tokv[0], &sp, 10);
            if ( !strchr(" \t\n\r", *sp) || longvalue < 1 || longvalue > 65535 ) {
               store_error_message("ORE_CHGBITS_WINDSP must be 1 though 65535");
               errors++;
            }
            configp->ore_chgbits_wsp = (unsigned int)longvalue;
            break;
            
         case OreChgBitsWavsp :
            longvalue = strtol(tokv[0], &sp, 10);
            if ( !strchr(" \t\n\r", *sp) || longvalue < 1 || longvalue > 65535 ) {
               store_error_message("ORE_CHGBITS_WINDAVSP must be 1 though 65535");
               errors++;
            }
            configp->ore_chgbits_wavsp = (unsigned int)longvalue;
            break;
            
         case OreChgBitsWdir :
            longvalue = strtol(tokv[0], &sp, 10);
            if ( !strchr(" \t\n\r", *sp) || longvalue < 1 || longvalue > 720 ) {
               store_error_message("ORE_CHGBITS_WINDDIR must be 1 though 720");
               errors++;
            }
            configp->ore_chgbits_wdir = (unsigned int)longvalue;
            break;
            
         case OreChgBitsRrate :
            longvalue = strtol(tokv[0], &sp, 10);
            if ( !strchr(" \t\n\r", *sp) || longvalue < 1 || longvalue > 65535 ) {
               store_error_message("ORE_CHGBITS_RAINRATE must be 1 though 65535");
               errors++;
            }
            configp->ore_chgbits_rrate = (unsigned int)longvalue;
            break;
            
         case OreChgBitsRtot :
            longvalue = strtol(tokv[0], &sp, 10);
            if ( !strchr(" \t\n\r", *sp) || longvalue < 1 || longvalue > 65535 ) {
               store_error_message("ORE_CHGBITS_RAINTOT must be 1 though 65535");
               errors++;
            }
            configp->ore_chgbits_rtot = (unsigned int)longvalue;
            break;
            
         case OreChgBitsUV :
            value = (int)strtol(tokv[0], &sp, 10);
            if ( !strchr(" \t\n\r", *sp) || value < 1 || value > 255 ) {
               store_error_message("ORE_CHGBITS_UV must be 1 though 255");
               errors++;
            }
            configp->ore_chgbits_uv = (unsigned char)value;
            break;
            
         case OreDataEntry :
            (void) strupper(tokv[0]);
            if ( !strcmp(tokv[0], "NATIVE") )
               configp->ore_data_entry = NATIVE;
            else if ( !strcmp(tokv[0], "SCALED") )
               configp->ore_data_entry = SCALED;
            else {
               store_error_message("ORE_DATA_ENTRY must be NATIVE or SCALED");
               errors++;
            }
            break;

         case OreDispChan :
            (void) strupper(tokv[0]);
            if ( !strcmp(tokv[0], "YES") )
               configp->ore_display_chan = YES;
            else if ( !strcmp(tokv[0], "NO") )
               configp->ore_display_chan = NO;
            else {
               store_error_message("ORE_DISPLAY_CHAN must be YES or NO");
               errors++;
            }
            break;

         case OreDispBatLvl :
            (void) strupper(tokv[0]);
            if ( !strcmp(tokv[0], "YES") )
               configp->ore_display_batlvl = YES;
            else if ( !strcmp(tokv[0], "NO") )
               configp->ore_display_batlvl = NO;
            else {
               store_error_message("ORE_DISPLAY_BATLVL must be YES or NO");
               errors++;
            }
            break;

         case OreDispCount :
            (void) strupper(tokv[0]);
            if ( !strcmp(tokv[0], "YES") )
               configp->ore_display_count = YES;
            else if ( !strcmp(tokv[0], "NO") )
               configp->ore_display_count = NO;
            else {
               store_error_message("ORE_DISPLAY_COUNT must be YES or NO");
               errors++;
            }
            break;

         case OreDispBft :
            (void) strupper(tokv[0]);
            if ( !strcmp(tokv[0], "YES") )
               configp->ore_display_bft = YES;
            else if ( !strcmp(tokv[0], "NO") )
               configp->ore_display_bft = NO;
            else {
               store_error_message("ORE_DISPLAY_BEAUFORT must be YES or NO");
               errors++;
            }
            break;

         case OreID16 :
            (void) strupper(tokv[0]);
            if ( !strcmp(tokv[0], "YES") )
               configp->oreid_16 = YES;
            else if ( !strcmp(tokv[0], "NO") )
               configp->oreid_16 = NO;
            else {
               store_error_message("ORE_ID_16 must be YES or NO");
               errors++;
            }
            break;

         case DispSensorIntv :
            (void) strupper(tokv[0]);
            if ( !strcmp(tokv[0], "YES") )
               configp->display_sensor_intv = YES;
            else if ( !strcmp(tokv[0], "NO") )
               configp->display_sensor_intv = NO;
            else {
               store_error_message("DISPLAY_SENSOR_INTV must be YES or NO");
               errors++;
            }
            break;

         case DateFormat :
            (void) strupper(tokv[0]);
            if ( !strcmp(tokv[0], "YMD") )
               configp->date_format = YMD_ORDER;
            else if ( !strcmp(tokv[0], "DMY") )
               configp->date_format = DMY_ORDER;
            else if ( !strcmp(tokv[0], "MDY") )
               configp->date_format = MDY_ORDER;
            else {
               store_error_message("DATE_FORMAT order must be YMD, DMY, or MDY");
               errors++;
               break;
            }
           
            if ( tokc == 2 ) {
               if ( !strcmp(tokv[1], "'/'") || !strcmp(tokv[1], "\"/\"") || !strcmp(tokv[1], "/") )
                  configp->date_separator = '/';
               else if ( !strcmp(tokv[1], "'-'") || !strcmp(tokv[1], "\"-\"") || !strcmp(tokv[1], "-") )
                  configp->date_separator = '-';
               else if ( !strcmp(tokv[1], "'.'") || !strcmp(tokv[1], "\".\"") || !strcmp(tokv[1], ".") )
                  configp->date_separator = '.';
               else {
                  store_error_message("DATE_FORMAT separator must be '/', '-', or '.'");
                  errors++;
               }
            }
            break;

         case LockTimeout :
            value = (int)strtol(tokv[0], &sp, 10);
            if ( !strchr(" \t\n\r", *sp) || value < 5 || value > 60 ) {
               store_error_message("LOCK_TIMEOUT range is 5 through 60 seconds");
               errors++;
            }
            configp->lock_timeout = value;
            break;
            
         case CM11QueryDelay :
            value = (int)strtol(tokv[0], &sp, 10);
            if ( !strchr(" \t\n\r", *sp) || value < 0 || value > 100 ) {
               store_error_message("CM11_QUERY_DELAY range is 0 through 100 milliseconds");
               errors++;
            }
            configp->cm11a_query_delay = value;
            break;
            
         case ElsNumber :
            value = (int)strtol(tokv[0], &sp, 10);
            if ( !strchr(" \t\n\r", *sp) || value < 1 || value > 3 ) {
               store_error_message("ELS_NUMBER range is 1 through 3");
               errors++;
            }
            configp->els_number = value;
            break;
            
         case ElsVoltage :
            dblvalue = strtod(tokv[0], &sp);
            if ( !strchr(" \t\n\r", *sp) || dblvalue < 0.0 ) {
               store_error_message("ELS_VOLTAGE is invalid or negative");
               errors++;
            }
            configp->els_voltage = dblvalue;
            break;

         case ElsChgBitsCurr :
            value = (int)strtol(tokv[0], &sp, 10);
            if ( !strchr(" \t\n\r", *sp) || value < 1 || value > 1000 ) {
               store_error_message("ELS_CHGBITS_CURR is 1 through 1000");
               errors++;
            }
            configp->els_chgbits_curr = value;
            break;
            
         case ScanMode :
            (void) strupper(tokv[0]);
            if ( !strcmp(tokv[0], "CONTINUE") )
               configp->scanmode = FM_CONTINUE;
            else if ( !strcmp(tokv[0], "BREAK") )
               configp->scanmode = FM_BREAK;
            else {
               store_error_message("LAUNCHER_SCANMODE must be CONTINUE or BREAK");
               errors++;
            }
            break;

         case RfxInline :
            (void) strupper(tokv[0]);
            if ( !strcmp(tokv[0], "YES") )
               configp->rfx_inline = YES;
            else if ( !strcmp(tokv[0], "NO") )
               configp->rfx_inline = NO;
            else {
               store_error_message("RFXMETER_SETUP_INLINE must be YES or NO");
               errors++;
            }
            break;

         case OwlVoltage :
            dblvalue = strtod(tokv[0], &sp);
            if ( !strchr(" \t\n\r", *sp) || dblvalue < 0.0 ) {
               store_error_message("OWL_VOLTAGE is invalid or negative");
               errors++;
            }
            configp->owl_voltage = dblvalue;
            break;

         case OwlCalibPower :
            dblvalue = strtod(tokv[0], &sp);
            if ( !strchr(" \t\n\r", *sp) || dblvalue < 0.0 ) {
               store_error_message("OWL_CALIB_POWER is invalid or negative");
               errors++;
            }
            configp->owl_calib_power = dblvalue;
            break;

         case OwlCalibEnergy :
            dblvalue = strtod(tokv[0], &sp);
            if ( !strchr(" \t\n\r", *sp) || dblvalue < 0.0 ) {
               store_error_message("OWL_CALIB_ENERGY is invalid or negative");
               errors++;
            }
            configp->owl_calib_energy = dblvalue;
            break;

         case OwlChgBitsPower :
            longvalue = strtol(tokv[0], &sp, 10);
            if ( !strchr(" \t\n\r", *sp) || longvalue < 1L || longvalue > 10000L ) {
               store_error_message("OWL_CHGBITS_POWER is 1 through 10000");
               errors++;
            }
            configp->owl_chgbits_power = longvalue;
            break;
            
         case OwlChgBitsEnergy :
            longvalue = strtol(tokv[0], &sp, 10);
            if ( !strchr(" \t\n\r", *sp) || longvalue < 1L || longvalue > 100000L ) {
               store_error_message("OWL_CHGBITS_ENERGY is 1 through 100000");
               errors++;
            }
            configp->owl_chgbits_energy = longvalue;
            break;
            
         case OwlDispCount :
            (void) strupper(tokv[0]);
            if ( !strcmp(tokv[0], "YES") )
               configp->owl_display_count = YES;
            else if ( !strcmp(tokv[0], "NO") )
               configp->owl_display_count = NO;
            else {
               store_error_message("OWL_DISPLAY_COUNT must be YES or NO");
               errors++;
            }
            break;

	 case ArmRemote :
            (void) strupper(tokv[0]);
            if ( !strncmp(tokv[0], "MANUAL", 3) )
               configp->arm_remote = MANUAL;
            else if ( !strncmp(tokv[0], "AUTOMATIC", 4) )
               configp->arm_remote = AUTOMATIC;
            else {
               store_error_message("ARM_REMOTE must be MANUAL or AUTO");
               errors++;
            }
            break;

         case ActiveChange :
            (void) strupper(tokv[0]);
            if ( !strcmp(tokv[0], "YES") )
               configp->active_change = YES;
            else if ( !strcmp(tokv[0], "NO") )
               configp->active_change = NO;
            else {
               store_error_message("ACTIVE_CHANGE must be YES or NO");
               errors++;
            }
            break;

         case InactiveHandling :
            (void) strupper(tokv[0]);
            if ( !strcmp(tokv[0], "NEW") )
               configp->inactive_handling = NEW;
            else if ( !strcmp(tokv[0], "OLD") )
               configp->inactive_handling = OLD;
            else {
               store_error_message("INACTIVE_HANDLING must be NEW or OLD");
               errors++;
            }
            break;

         case ProcessXmit :
            (void) strupper(tokv[0]);
            if ( !strcmp(tokv[0], "YES") )
               configp->process_xmit = YES;
            else if ( !strcmp(tokv[0], "NO") )
               configp->process_xmit = NO;
            else {
               store_error_message("PROCESS_XMIT must be YES or NO");
               errors++;
            }
            break;

         case ShowFlagsMode :
            (void) strupper(tokv[0]);
            if ( !strcmp(tokv[0], "NEW") )
               configp->show_flags_mode = NEW;
            else if ( !strcmp(tokv[0], "OLD") )
               configp->show_flags_mode = OLD;
            else {
               store_error_message("SHOW_FLAGS_MODE must be NEW or OLD");
               errors++;
            }
            break;

         case LaunchPathAppend :
            strtrim(tokv[0]);
            if ( strlen(tokv[0]) > PATH_LEN ) {
               store_error_message("LAUNCHPATH_APPEND too long");
               errors++;
            }
            else {
               strcpy(configp->launchpath_append, tokv[0]);
            }
            break;

         case LaunchPathPrefix :
            strtrim(tokv[0]);
            if ( strlen(tokv[0]) > PATH_LEN ) {
               store_error_message("LAUNCHPATH_PREFIX too long");
               errors++;
            }
            else {
               strcpy(configp->launchpath_prefix, tokv[0]);
            }
            break;

         case FixStopStartError :
            (void) strupper(tokv[0]);
            if ( !strcmp(tokv[0], "YES") )
               configp->fix_stopstart_error = YES;
            else if ( !strcmp(tokv[0], "NO") )
               configp->fix_stopstart_error = NO;
            else {
               store_error_message("FIX_STOPSTART_ERROR must be YES or NO");
               errors++;
            }
            break;

         case ChkSumTimeout :
            longvalue = strtol(tokv[0], &sp, 10);
            if ( !strchr(" \t\n\r", *sp) || longvalue < 1L || longvalue > 20L ) {
               store_error_message("CHKSUM_TIMEOUT must be 1 through 20 seconds");
               errors++;
               break;
            }
            configp->chksum_timeout = longvalue;
            break;

         default :
            store_error_message("Unsupported config directive");
            errors++;
            break;

      }
      free( tokv );

      return errors;
}

/*---------------------------------------------------------------------+
 | Get certain configuration items from environment.                   |
 +---------------------------------------------------------------------*/
int environment_config ( void )
{
   int  j, retcode, errors = 0;
   char *sp;
   char buffer[32];

   static char *envars[] = {
     "LATITUDE",
     "LONGITUDE",
     "ASIF_DATE",
     "ASIF_TIME",
   };

   reset_isparsed_flags();

   for ( j = 0; j < (int)(sizeof(envars)/sizeof(char *)); j++ ) {
      if ( (sp = getenv(envars[j])) != NULL ) {
         sprintf(buffer, "%s %s", envars[j], sp);
         retcode = parse_config_tail(buffer, SRC_ENVIRON);
         errors += retcode;
         if ( retcode != 0 || *error_message() != '\0' ) {
            fprintf(stderr, "Environment variable %s.\n", error_message());
            clear_error_message();
         }
      }
   }
   return errors;
}


/*---------------------------------------------------------------------+
 | Display configuration file directives that have been overridden,    |
 | either by an environment variable or by a 'config' command in the   |
 | schedule file, as indicated by the isparsed flag.                   |
 +---------------------------------------------------------------------*/
void display_config_overrides ( FILE *fd )
{
   int  j, count = 0;
   char delim = ' ';

   fprintf(fd, "Configuration overrides:");

   for ( j = 0; j < ncommands; j++ ) {
      if ( command[j].isparsed & (SRC_ENVIRON | SRC_SCHED)) {
         count++;
         fprintf(fd, "%c %s", delim, command[j].name);
         delim = ',';
      }
   }
   if ( !count )
      fprintf(fd, " -- None --");

   fprintf(fd, "\n\n");

   return;
}

/*---------------------------------------------------------------------+
 | Open the user's X10 configuration file and read only the minimal    |
 | directives for specific commands, like 'heyu stop'.                 |
 +---------------------------------------------------------------------*/
void read_minimal_config ( unsigned char mode, unsigned int source )
{

   FILE        *fd ;
   int         error_count;
   char        confp[PATH_LEN + 1];
   extern char heyu_config[PATH_LEN + 1];
   extern int  verbose;

   find_heyu_path();

   strncpy2(confp, pathspec(heyu_config), sizeof(confp) - 1);

   if ( verbose ) 
      (void) fprintf(stdout, "Opening Heyu configuration file '%s'\n", confp); 
 
   if ( !(fd = fopen(confp, "r")) ) {
      if ( !i_am_relay )
         fprintf(stderr, "Unable to find (or open) Heyu configuration file '%s'\n", confp);
      else
         syslog(LOG_ERR, "Unable to find (or open) Heyu configuration file '%s'\n", confp);

      return;
   }

   error_count = parse_minimal_config( fd, mode, source );

   if ( error_count != 0 ) {
      if ( !i_am_relay )
         fprintf(stderr, "Quitting due to errors in configuration file '%s'\n", confp);
      else
         syslog(LOG_ERR, "Quitting due to errors in configuration file '%s'\n", confp);
      exit(1);
   }
 
   (void) fclose( fd );

   return;
}


/*---------------------------------------------------------------------+
 | Return the index in the array of SCRIPT structures for the SCRIPT   |
 | having the argument 'label', or -1 if not found.                    |
 +---------------------------------------------------------------------*/
int lookup_script ( SCRIPT *scriptp, char *label )
{
   int    j = 0;

   while ( scriptp && scriptp[j].line_no > 0 ) {
      if ( strcmp(label, scriptp[j].label) == 0 )
         return j;
      j++;
   }
   return -1;
}

/*---------------------------------------------------------------------+
 | Return the index in the array of LAUNCHER structures for the first  |
 | LAUNCHER having the argument 'label', or -1 if not found.           |
 +---------------------------------------------------------------------*/
int lookup_launcher ( LAUNCHER *launcherp, char *label )
{
   int   j = 0;

   while ( launcherp && launcherp[j].line_no > 0 ) {
      if ( strcmp(label, launcherp[j].label) == 0 )
         return j;
      j++;
   }
   return -1;
}


/*---------------------------------------------------------------------+
 | Create list of Oregon sensor IDs to be ignored and classified as    |
 | noise.                                                              |
 +---------------------------------------------------------------------*/
int create_oregon_ignore_list ( void )
{
#ifdef HAVE_FEATURE_ORE
   ALIAS *aliasp;
   int   j, k;

   if ( (aliasp = configp->aliasp) == NULL )
      return 0;

   ore_ignore_size = 0;
   j = 0;
   while ( aliasp[j].line_no > 0 ) {
      if ( aliasp[j].vtype == RF_OREGON && (aliasp[j].optflags & MOPT_RFIGNORE) ) {
         k = 0;
         while ( k < aliasp[j].nident && ore_ignore_size < (sizeof(ore_ignore)/sizeof(unsigned int)) ) {
            ore_ignore[ore_ignore_size++] = aliasp[j].ident[k];
            k++;
         }
      }
      j++;
   }
#endif /* HAVE_FEATURE_ORE */
   return 0;
}

/*---------------------------------------------------------------------+
 | Create list of Security sensor IDs to be ignored and classified as  |
 | noise.                                                              |
 +---------------------------------------------------------------------*/
int create_security_ignore_list ( void )
{
   ALIAS *aliasp;
   int   j, k;

   if ( (aliasp = configp->aliasp) == NULL )
      return 0;

   sec_ignore_size = 0;
   j = 0;
   while ( aliasp[j].line_no > 0 ) {
      if ( aliasp[j].vtype == RF_SEC && (aliasp[j].optflags & MOPT_RFIGNORE) ) {
         k = 0;
         while ( k < aliasp[j].nident && sec_ignore_size < (sizeof(sec_ignore)/sizeof(unsigned short)) ) {
            sec_ignore[sec_ignore_size++] = aliasp[j].ident[k];
            k++;
         }
      }
      j++;
   }
   return 0;
}

int is_sec_ignored ( unsigned int saddr )
{
   int j = 0;

   while ( j < sec_ignore_size ) {
      if ( saddr == sec_ignore[j] ) {
         return 1;
      }
      j++;
   }
   return 0;
}

/*---------------------------------------------------------------------+
 | Resolve interrelated configuration items.                           |
 +---------------------------------------------------------------------*/
int finalize_config ( unsigned char mode )
{
   int      finalize_launchers(void);
   int      create_file_paths(void);
   mode_t   heyu_umask;
   int      verify_unique_ids(unsigned char);
   int      assign_data_storage ( void );
   int      set_elec1_nvar ( int );

   ALIAS    *aliasp;

   char errmsg[80];
   char *sp;
   int  j;


   /* Count and configure aliases */
   aliasp = configp->aliasp;
   j = 0;
   while ( aliasp && aliasp[j].line_no > 0 ) {
      if ( (aliasp[j].optflags & MOPT_HEARTBEAT) && !(aliasp[j].optflags2 & MOPT2_IATO) ) {
         aliasp[j].hb_timeout = configp->inactive_timeout;
      }
      j++;
   }
   configp->alias_size = j;
   
   if ( configp->heyu_umask >= 0 )
      umask((mode_t)configp->heyu_umask);

   heyu_umask = umask(0);
   umask(heyu_umask);

   if ( configp->log_umask < 0 )
      configp->log_umask = (int)heyu_umask;

   if ( configp->mode == COMPATIBLE ) {
      configp->program_days = 366;
   }
   else {
      configp->program_days = configp->program_days_in;
   }

   if ( configp->min_dusk != OFF && configp->max_dusk != OFF && 
        configp->min_dusk >= configp->max_dusk ) {
      store_error_message("MIN_DUSK must be less than MAX_DUSK");
      return 1;
   }

   if ( configp->min_dawn != OFF && configp->max_dawn != OFF && 
        configp->min_dawn >= configp->max_dawn ) {
      store_error_message("MIN_DAWN must be less than MAX_DAWN");
      return 1;
   }

   if ( configp->rfxcom_enable && configp->rfxcom_disable ) {
      store_error_message("RFXCOM_ENABLE (Deprecated) and RFXCOM_DISABLE are incompatible.");
      return 1;
   }
   if ( configp->rfxcom_disable )
      configp->rfxcom_enable = 0xFFFF;
   else if ( configp->rfxcom_enable )
      store_error_message("Directive RFXCOM_ENABLE is deprecated; see RFXCOM_DISABLE");

   if ( configp->securid_16 == NO || configp->auxdev != DEV_RFXCOMVL ) {
      configp->securid_mask = 0x00ffu;
   }
   else {
      configp->securid_mask = 0xffffu;
   }

   if ( configp->oreid_16 == NO || configp->auxdev != DEV_RFXCOMVL ) {
      configp->oreid_mask = 0x00ffu;
   }
   else {
      configp->oreid_mask = 0xffffu;
   }

   if ( verify_unique_ids(RF_ENT) != 0     ||
        verify_unique_ids(RF_SEC) != 0     ||
        verify_unique_ids(RF_XSENSOR) != 0 ||
        verify_unique_ids(RF_XMETER) != 0  ||
        verify_unique_ids(RF_DIGIMAX) != 0 ||
        verify_unique_ids(RF_OREGON) != 0    ) {
      return 1;
   }

   if ( strcmp(configp->tty, "dummy") == 0 ) {
      configp->device_type |= DEV_DUMMY;
   }

   /* Create suffix for lock files, e.g., ".ttyS0" */

   if ( (sp = strrchr(configp->tty, '/')) != NULL ) {
      strncpy2(configp->suffix, sp, sizeof(config.suffix) - 1);
      *(configp->suffix) = '.';
   }
   else {	   
      strncpy2(configp->suffix, ".", sizeof(config.suffix) - 1);
      strncat(configp->suffix, configp->tty,
                          sizeof(config.suffix) - 1 - strlen(configp->suffix));
   }

   if ( configp->auxdev ) {
      if ( (sp = strrchr(configp->ttyaux, '/')) != NULL ) {
         strncpy2(configp->suffixaux, sp, sizeof(config.suffixaux) - 1);
         *(configp->suffixaux) = '.';
      }
      else {	   
         strncpy2(configp->suffixaux, ".", sizeof(config.suffixaux) - 1);
         strncat(configp->suffixaux, configp->ttyaux,
                            sizeof(config.suffixaux) - 1 - strlen(configp->suffixaux));
      }
   }

   if ( configp->aux_repcounts[0] == 0 ) {
      configp->aux_repcounts[0] =
         (configp->auxdev == DEV_RFXCOM32  ||
          configp->auxdev == DEV_RFXCOMVL     ) ?
            DEF_AUX_MINCOUNT_RFXCOM :
         (configp->auxdev == DEV_MR26A) ? DEF_AUX_MINCOUNT_MR26 : DEF_AUX_MINCOUNT_W800;
   }

   if ( configp->aux_mincount_rfx == 0 ) {
      if (configp->auxdev == DEV_RFXCOM32  ||
          configp->auxdev == DEV_RFXCOMVL     ) {
         configp->aux_mincount_rfx = max(DEF_AUX_MINCOUNT_RFX, configp->aux_repcounts[0]);
      }
      else {
         configp->aux_mincount_rfx = configp->aux_repcounts[0];
      }
   }

   if ( *configp->logfile == '\0' ) {
      strncpy2(configp->logfile, DEF_LOGFILE, sizeof(config.logfile) - 1);
   }
   else if ( configp->logcommon == YES ) {
      strncat(configp->logfile, ".common",
                     sizeof(config.logfile) - 1 - strlen(configp->logfile));
      if ( configp->disp_subdir == NO_ANSWER )
         configp->disp_subdir = YES;
   }
   else {
      strncat(configp->logfile, configp->suffix,
                     sizeof(config.logfile) - 1 - strlen(configp->logfile));
   }

   /* If not set above */
   if ( configp->disp_subdir == NO_ANSWER )
      configp->disp_subdir = NO;

   if ( configp->hide_unchanged_inactive == NO_ANSWER )
      configp->hide_unchanged_inactive = configp->hide_unchanged;

   create_file_paths();  

   if ( access(configp->logfile, F_OK) == 0 &&
        access(configp->logfile, W_OK) != 0 ) {
      sprintf(errmsg, "Log file '%s' is not writable - check permissions.",
	 configp->logfile);
      store_error_message(errmsg);
      return 1;
   }

   if ( access(statefile, F_OK) == 0 &&
        access(statefile, W_OK) != 0 ) {
      sprintf(errmsg, "State file '%s' is not writable - check permissions.",
	 statefile);
      store_error_message(errmsg);
      return 1;
   }

   for ( j = 0; j < nrflabels; j++ ) {
      if ( configp->rf_bursts[j] > configp->def_rf_bursts ) {
         sprintf(errmsg, "RF_BURSTS exceeds %d\n", configp->def_rf_bursts);
	 store_error_message(errmsg);
	 return 1;
      }
   }


   if ( configp->transceive & configp->rfforward ) {
      store_error_message("Housecode conflict in TRANSCEIVE and RFFORWARD");
      return 1;
   }

   if ( configp->device_type == DEV_DUMMY ) 
      configp->lockup_check = 0;

   assign_data_storage();

   create_oregon_ignore_list();
   create_security_ignore_list();

   set_elec1_nvar(configp->els_number);


#ifdef HAVE_FEATURE_RFXM
   create_rfxpower_panels();
#endif

   if ( finalize_launchers() > 0 )
      return 1;

   return 0;
}


/*---------------------------------------------------------------------+
 | Add an alias to the array of ALIAS structures and return the index  |
 | of the new member.                                                  |
 +---------------------------------------------------------------------*/
int add_alias ( ALIAS **aliaspp, char *label, int line_no, 
                              char housecode, char *units, int modtype )
{
   static int    size, max_size;
   static int    strucsize = sizeof(ALIAS);
   int           j, k, maxlevel;
   int           blksize = 10;
   char          hc;
   unsigned int  bmap;
   unsigned long vflags, flags, xflags, kflags;
   char          errmsg[128];
   int           (*module_xlate_func(int index))();

   clear_error_message();

   /* Allocate initial block of memory */
   if ( *aliaspp == NULL ) {
      *aliaspp = (ALIAS *) calloc(blksize, strucsize );
      if ( *aliaspp == NULL ) {
         (void) fprintf(stderr, "Unable to allocate memory for Alias.\n");
         exit(1);
      }
      max_size = blksize;
      size = 0;
      /* Initialize it where necessary */
      for ( j = 0; j < max_size; j++ ) {
         (*aliaspp)[j].label[0] = '\0';
         (*aliaspp)[j].modtype = -1;
         (*aliaspp)[j].line_no = 0;
         (*aliaspp)[j].vflags = 0;
         (*aliaspp)[j].flags = 0;
         (*aliaspp)[j].xflags = 0;
         (*aliaspp)[j].kflags = 0;
         (*aliaspp)[j].optflags = 0;
         (*aliaspp)[j].optflags2 = 0;
         (*aliaspp)[j].tmin = 0;
         (*aliaspp)[j].tmax = 0;
         (*aliaspp)[j].rhmin = 0;
         (*aliaspp)[j].rhmax = 0;
         for ( k = 0; k < NOFFSET; k++ )
            (*aliaspp)[j].offset[k] = 0.0;
         (*aliaspp)[j].vtype = 0;
         (*aliaspp)[j].subtype = 0;
         (*aliaspp)[j].nident = 0;
         for ( k = 0; k < NIDENT; k++ ) {
            (*aliaspp)[j].ident[k] = 0;
            (*aliaspp)[j].kaku_keymap[k] = 0;
            (*aliaspp)[j].kaku_grpmap[k] = 0;
         }
         (*aliaspp)[j].xlate_func = NULL;
//         (*aliaspp)[j].timeout = 0;
         (*aliaspp)[j].hb_timeout = -1L;
         (*aliaspp)[j].hb_index = -1;
         (*aliaspp)[j].nvar = 0;
         (*aliaspp)[j].storage_index = -1;
         (*aliaspp)[j].storage_units = 0;
         for ( k = 0; k < NFUNCLIST; k++ ) {
            (*aliaspp)[j].funclist[k] = 0xff;
            (*aliaspp)[j].statusoffset[k] = k;
         }
         (*aliaspp)[j].ext0links = 0;
      }
   }

   /* Check for a valid label length */
   if ( (int)strlen(label) > NAME_LEN ) {
      sprintf(errmsg, 
         "Alias label '%s' too long - maximum %d characters", label, NAME_LEN);
      store_error_message(errmsg);
      return -1;
   }

   /* See if the alias label is already in the list.     */
   /* If so, it's an error.                              */
   if ( (j = get_alias(*aliaspp, label, &hc, &bmap)) >= 0 ) {
      (void) sprintf(errmsg, "Duplicate alias label '%s'", label);
      store_error_message(errmsg);
      return -1;
   }

   /* Verify that the label is not 'macro' */
   if ( strcmp(label, "macro") == 0 ) {
      store_error_message("An alias may not have the label 'macro'\n");
      return -1;
   }

   /* Check the housecode */
   if ( housecode == '_' ) {
      (void) sprintf(errmsg, 
         "Alias '%s': Default housecode symbol ('_') is invalid in an alias",
              label);
      store_error_message(errmsg);
      return -1;
   }
   if ( (hc = toupper((int)housecode)) < 'A' || hc > 'P' ) {
      (void) sprintf(errmsg, "Alias '%s': Housecode '%c' outside range A-P",
               label, hc);
      store_error_message(errmsg);
      return -1;
   }

   /* Check the units list */
   if ( parse_units( units, &bmap ) != 0 ) {
      (void) sprintf(errmsg, "Alias '%s': ", label);
      add_error_prefix(errmsg);
      return -1;
   }

   /* Check to see that the module type (if any) specified for */
   /* this housecode|unit address doesn't conflict with that   */
   /* in a previously defined alias.                           */

   if ( modtype >= 0 ) {
      j = 0;
      while ( (*aliaspp)[j].line_no > 0 ) {
         if ( (*aliaspp)[j].housecode == hc && (*aliaspp)[j].unitbmap & bmap &&
              (*aliaspp)[j].modtype >= 0 ) {
            if ( (*aliaspp)[j].modtype != modtype ) {
               sprintf(errmsg,
                  "Module type conflicts with that defined for %c%s on Line %d",
                       hc, bmap2units((*aliaspp)[j].unitbmap & bmap),
                          (*aliaspp)[j].line_no);
               store_error_message(errmsg);
               return -1;
            }
         }
         j++;
      }
   }

#if 0   
   if ( modtype >= 0 ) {
      j = 0;
      while ( (*aliaspp)[j].line_no > 0 ) {
         if ( (*aliaspp)[j].housecode == hc && (*aliaspp)[j].unitbmap & bmap &&
              (*aliaspp)[j].modtype >= 0 && (*aliaspp)[j].modtype != modtype ) {
            sprintf(errmsg,
               "Module type conflicts with that defined for %c%s on Line %d",
                    hc, bmap2units((*aliaspp)[j].unitbmap & bmap),
                       (*aliaspp)[j].line_no);
            store_error_message(errmsg);
            return -1;
         }
         j++;
      }
   }
#endif

   /* Check to see if there's an available location          */
   /* If not, increase the size of the memory allocation.    */
   /* (Always leave room for a final termination indicator.) */
   if ( size == (max_size - 1)) {
      max_size += blksize ;
      *aliaspp = (ALIAS *) realloc(*aliaspp, max_size * strucsize );
      if ( *aliaspp == NULL ) {
         (void) fprintf(stderr, "Unable to increase size of Alias list.\n");
         exit(1);
      }

      /* Initialize the new memory allocation */
      for ( j = size; j < max_size; j++ ) {
         (*aliaspp)[j].label[0] = '\0';
         (*aliaspp)[j].modtype = -1;
         (*aliaspp)[j].line_no = 0;
         (*aliaspp)[j].vflags = 0;
         (*aliaspp)[j].flags = 0;
         (*aliaspp)[j].xflags = 0;
         (*aliaspp)[j].kflags = 0;
         (*aliaspp)[j].optflags = 0;
         (*aliaspp)[j].optflags2 = 0;
         (*aliaspp)[j].tmin = 0;
         (*aliaspp)[j].tmax = 0;
         (*aliaspp)[j].rhmin = 0;
         (*aliaspp)[j].rhmax = 0;
         for ( k = 0; k < NOFFSET; k++ )
            (*aliaspp)[j].offset[k] = 0.0;
         (*aliaspp)[j].vtype = 0;
         (*aliaspp)[j].subtype = 0;
         (*aliaspp)[j].nident = 0;
         for ( k = 0; k < NIDENT; k++ ) {
            (*aliaspp)[j].ident[k] = 0;
            (*aliaspp)[j].kaku_keymap[k] = 0;
            (*aliaspp)[j].kaku_grpmap[k] = 0;
         }
         (*aliaspp)[j].xlate_func = NULL;
//         (*aliaspp)[j].timeout = 0;
         (*aliaspp)[j].hb_timeout = -1L;
         (*aliaspp)[j].hb_index = -1;
         (*aliaspp)[j].nvar = 0;
         (*aliaspp)[j].storage_index = -1;
         (*aliaspp)[j].storage_units = 0;
         for ( k = 0; k < NFUNCLIST; k++ ) {
            (*aliaspp)[j].funclist[k] = 0xff;
            (*aliaspp)[j].statusoffset[k] = k;
         }
         (*aliaspp)[j].ext0links = 0;
      }
   }

   j = size;
   size += 1;

   (void) strncpy2((*aliaspp)[j].label, label, NAME_LEN);
   (*aliaspp)[j].line_no = line_no;
   (*aliaspp)[j].housecode = toupper((int)housecode);
   (*aliaspp)[j].hcode = hc2code(housecode);
   (*aliaspp)[j].unitbmap = bmap;
   (*aliaspp)[j].modtype = modtype;
   module_attributes(modtype, &vflags, &flags, &xflags, &kflags, &maxlevel);
   (*aliaspp)[j].vflags = vflags;
   (*aliaspp)[j].flags = flags;
   (*aliaspp)[j].xflags = xflags;
   (*aliaspp)[j].kflags = kflags;
   (*aliaspp)[j].maxlevel = maxlevel;
   (*aliaspp)[j].onlevel = maxlevel;
   (*aliaspp)[j].xlate_func = module_xlate_func(modtype);

   return j;
}

/*---------------------------------------------------------------------+
 | Add a scene or usersyn to the array of SCENE structures and return  |
 | the index of the new member.                                        |
 | Argument 'type' may be 1 for a scene or 2 for a usersyn             |
 +---------------------------------------------------------------------*/
int add_scene ( SCENE **scenepp, char *label, 
                 int line_no, char *body, unsigned int type )
{
   static int    size, max_size ;
   static int    strucsize = sizeof(SCENE);
   int           j;
   char          *sp;
   int           cmdc, nparms;
   char          **cmdv;
   int           blksize = 10;
   char          errmsg[128];
   extern char   *typename[];

   /* Allocate initial block of memory */
   if ( *scenepp == NULL ) {
      *scenepp = calloc(blksize, strucsize );
      if ( *scenepp == NULL ) {
         fprintf(stderr, "Unable to allocate memory for Scene/Usersyn.\n");
         exit(1);
      }
      max_size = blksize;
      size = 0;
      /* Initialize it */
      for ( j = 0; j < max_size; j++ ) {
         (*scenepp)[j].label[0] = '\0';
         (*scenepp)[j].line_no = -1;
         (*scenepp)[j].nparms = 0;
         (*scenepp)[j].type = 0;
         (*scenepp)[j].body = NULL;
      }
   }

   /* Check for a valid scene label */
   if ( (int)strlen(label) > SCENE_LEN ) {
      sprintf(errmsg, 
         "%s label too long - maximum %d characters", typename[type], SCENE_LEN);
      store_error_message(errmsg);
      return -1;
   }
   if ( strchr("+_-", *label) != NULL ) {
      sprintf(errmsg, "%s label may not may not begin with '+', '-' or '_'", typename[type]);
      store_error_message(errmsg);
      return -1;
   }
   if ( strchr(label, '$') != NULL ) {
      sprintf(errmsg, "%s label may not contain the '$' character", typename[type]);
      store_error_message(errmsg);
      return -1;
   }
   if ( strchr(label, ';') != NULL ) {
      sprintf(errmsg, "%s label may not contain the ';' character", typename[type]);
      store_error_message(errmsg);
      return -1;
   }

   /* See if the scene label is already in the list.     */
   /* If so, it's an error.                              */
   if ( (j = lookup_scene(*scenepp, label)) >= 0 ) {
      sprintf(errmsg, 
         "%s label '%s' previously defined as a %s on line %d",
          typename[type], label, typename[(*scenepp)[j].type], (*scenepp)[j].line_no);
      store_error_message(errmsg);
      return -1;
   }

   if ( is_admin_cmd(label) || is_direct_cmd(label) ) {
      sprintf(errmsg, "%s label '%s' conflicts with heyu command", typename[type], label);
      store_error_message(errmsg);
      return -1;
   }

   /* Check to see if there's an available location          */
   /* If not, increase the size of the memory allocation.    */
   /* (Always leave room for a final termination indicator.) */
   if ( size == (max_size - 1)) {
      max_size += blksize ;
      *scenepp = realloc(*scenepp, max_size * strucsize );
      if ( *scenepp == NULL ) {
         fprintf(stderr, "Unable to increase size of Scene/Usersyn list.\n");
         exit(1);
      }

      /* Initialize the new memory allocation */
      for ( j = size; j < max_size; j++ ) {
         (*scenepp)[j].label[0] = '\0';
         (*scenepp)[j].line_no = -1;
         (*scenepp)[j].nparms = 0;
         (*scenepp)[j].type = 0;
         (*scenepp)[j].body = NULL;
      }
   }

   j = size;
   size += 1;

   /* Determine the number of replaceable parameters */
   sp = strdup(body);
   tokenize(sp, " \t;", &cmdc, &cmdv);
   nparms = max_parms(cmdc, cmdv);
   free(sp);
   free(cmdv);
   if ( *error_message() != '\0' ) {
      sprintf(errmsg, "%s '%s': ", typename[type], label);
      add_error_prefix(errmsg);
   }
   if ( nparms < 0 )
      return -1;

   sp = strdup(body);
   if ( sp == NULL ) {
      fprintf(stderr,
        "Unable to allocate memory for body of Scene/Usersyn '%s'.\n", label);
         exit(1);
   }
   strtrim(sp);
  
   (void) strncpy2((*scenepp)[j].label, label, SCENE_LEN);
   (*scenepp)[j].line_no = line_no;
   (*scenepp)[j].nparms = nparms;
   (*scenepp)[j].type = type;
   (*scenepp)[j].body = sp;

   return j;
}

/*---------------------------------------------------------------------+
 | Parse Latitude string [NS+-]ddd:mm and store in struct config.      |
 | line_no <= 0 indicates string from environment, otherwise line in   |
 | configuration file.                                                 | 
 +---------------------------------------------------------------------*/
int parse_latitude ( char *string )
{
   char tmpbuff[128];
   char *sp1, *sp2;
   int  sign;

   if ( string == NULL )
      return 0;

   (void)strncpy2(tmpbuff, string, sizeof(tmpbuff) - 1);
   sp1 = tmpbuff;

   /* For compatibility with Heyu 1 */
   if ( isdigit((int)(*sp1)) ) {
      (void)strcpy(tmpbuff, "N");
      (void)strncpy2(tmpbuff + 1, string, sizeof(tmpbuff) - 2);
   }
   else if ( *sp1 == '+' ) {
      *sp1 = 'N';
   }
   else if ( *sp1 == '-' ) {
      *sp1 = 'S';
   }

   switch ( toupper((int)(*sp1)) ) {
      case 'N' :
         sign = 1;
         break;
      case 'S' :
         sign = -1;
         break;
      default :
         sign = -2;   
         break;
   }
   sp1++;

   if ( sign < -1 || !(sp2 = strchr(sp1, ':')) ) {
         store_error_message("LATITUDE invalid - must be [NS+-]dd:mm");
         configp->loc_flag &= ~(LATITUDE) ;
         return 1;
   }

   configp->lat_m = (int)strtol(sp2 + 1, NULL, 10);
   *sp2 = '\0';
   configp->lat_d = (int)strtol(sp1, NULL, 10);
   if ( configp->lat_d == 0 )
      configp->lat_m *= sign ;
   else
      configp->lat_d *= sign ;

   configp->latitude = ( configp->lat_d < 0 ) ?
         (double)configp->lat_d - (double)configp->lat_m/60. :
               (double)configp->lat_d + (double)configp->lat_m/60.;

   if ( configp->latitude < -89. || configp->latitude > 89. ) {
         store_error_message("LATITUDE outside range -89 to +89 degrees");
         configp->loc_flag &= ~(LATITUDE);
         return 1;
   }

   configp->loc_flag |= LATITUDE ;

   return 0;
}

/*---------------------------------------------------------------------+
 | Parse Longitude string [EW+-]ddd:mm and store in struct config.     |
 | line_no <= 0 indicates string from environment, otherwise line in   |
 | configuration file.                                                 | 
 +---------------------------------------------------------------------*/
int parse_longitude ( char *string )
{
   char tmpbuff[128];
   char *sp1, *sp2;
   int  sign;

   if ( string == NULL )
      return 0;

   (void)strncpy2(tmpbuff, string, sizeof(tmpbuff) - 1);
   sp1 = tmpbuff;

   /* For compatibility with Heyu 1, where positive longitude assumed West */
   if ( isdigit((int)(*sp1)) ) {
      (void)strcpy(tmpbuff, "W");
      (void)strncpy2(tmpbuff + 1, string, sizeof(tmpbuff) - 2);
   }
   else if ( *sp1 == '+' ) {
      *sp1 = 'W';
   }
   else if ( *sp1 == '-' ) {
      *sp1 = 'E';
   }

   switch ( toupper((int)(*sp1)) ) {
      case 'E' :
         sign = 1;
         break;
      case 'W' :
         sign = -1;
         break;
      default :
         sign = -2;     
         break;
   }
   sp1++;

   if ( sign < -1 || !(sp2 = strchr(sp1, ':')) ) {
         store_error_message("LONGITUDE invalid - must be [EW+-]dd:mm");
         configp->loc_flag &= ~(LONGITUDE) ;
         return 1;
   }

   configp->lon_m = (int)strtol(sp2 + 1, NULL, 10);
   *sp2 = '\0';
   configp->lon_d = (int)strtol(sp1, NULL, 10);
   if ( configp->lon_d == 0 )
      configp->lon_m *= sign ;
   else
      configp->lon_d *= sign ;

   configp->longitude = ( configp->lon_d < 0 ) ?
      (double)configp->lon_d - (double)configp->lon_m/60. :
               (double)configp->lon_d + (double)configp->lon_m/60.;

   if ( configp->longitude < -180. || configp->longitude > 180. ) {
         store_error_message("LONGITUDE outside range -180 to +180 degrees");
         configp->loc_flag &= ~(LONGITUDE);
         return 1;
   }

   configp->loc_flag |= LONGITUDE ;

   return 0;
}

/*---------------------------------------------------------------------+
 | Check executability of a file on user's PATH                        |
 +---------------------------------------------------------------------*/
int is_executable ( char *pathname )
{
   char pathbuffer[1024];
   char buffer[1024];
   char *sp;
   int  j, tokc;
   char **tokv;

   if ( access(pathname, X_OK) == 0 )
      return 1;

   if ( (sp = getenv("PATH")) == NULL ) 
      return 0;

   strncpy2(pathbuffer, sp, sizeof(pathbuffer) - 1);

   tokenize(buffer, ":", &tokc, &tokv);

   for ( j = 0; j < tokc; j++ ) {
      strncpy2(buffer, tokv[j], sizeof(buffer) - 1);
      strncat(buffer, "/", sizeof(buffer) - 1 - strlen(buffer));
      strncat(buffer, pathname, sizeof(buffer) - 1 - strlen(buffer));
      if ( access(buffer, X_OK) == 0 ) {
         free(tokv);
         return 1;
      }
   }
   free(tokv);
   return 0;
}

/*---------------------------------------------------------------------+
 | Free memory allocated for SCRIPT structure and contents.            |
 +---------------------------------------------------------------------*/
void free_scripts ( SCRIPT **scriptpp )
{
   int j = 0;

   if ( *scriptpp == NULL )
      return;

   while ( (*scriptpp)[j].line_no > 0 ) {
     if ( (*scriptpp)[j].cmdline ) {
        free((*scriptpp)[j].cmdline);
     }
     j++;
   }
   free(*scriptpp);
   *scriptpp = NULL;

   return;
}  

/*---------------------------------------------------------------------+
 |  Free the array of SCENEs and the scene bodies therein.             |
 +---------------------------------------------------------------------*/
void free_scenes ( SCENE **scenepp )
{
   int j = 0;

   if ( *scenepp == NULL )
      return;

   while ( (*scenepp)[j].line_no > 0 ) {
     if ( (*scenepp)[j].body != NULL ) {
        free((*scenepp)[j].body);
     }
     j++;
   }

   free((*scenepp));
   *scenepp = NULL;
   return;
}

/*---------------------------------------------------------------------+
 |  Free the array of ALIASES.                                         |
 +---------------------------------------------------------------------*/
void free_aliases ( ALIAS **aliaspp )
{
   if ( *aliaspp == NULL )
      return;

   free((*aliaspp));
   *aliaspp = NULL;
   return;
}

/*---------------------------------------------------------------------+
 |  Free the array of LAUNCHERS.                                       |
 +---------------------------------------------------------------------*/
void free_launchers ( LAUNCHER **launcherpp )
{
   if ( *launcherpp == NULL )
      return;

   free((*launcherpp));
   *launcherpp = NULL;
   return;
}

/*---------------------------------------------------------------------+
 |  Free the relay powerfail script.                                   |
 +---------------------------------------------------------------------*/
void free_relay_powerfail_script ( char **pfail_script )
{
   if ( *pfail_script == NULL )
      return;

   free(*pfail_script);
   *pfail_script = NULL;
   return;
}

/*---------------------------------------------------------------------+
 |  Free arrays of ALIASes, SCENEs, SCRIPTs, and LAUNCHERs             |
 +---------------------------------------------------------------------*/
void free_all_arrays ( CONFIG *configp )
{        
   free_aliases(&configp->aliasp);
   free_scenes(&configp->scenep);
   free_scripts(&configp->scriptp);
   free_launchers(&configp->launcherp);
   free_relay_powerfail_script(&configp->pfail_script);

   return;
}

/*---------------------------------------------------------------------+
 |  Create the file pathspecs for many files.                          |
 +---------------------------------------------------------------------*/
int create_file_paths ( void )
{
    sprintf(statefile, "%s", pathspec(STATE_FILE));
    sprintf(enginelockfile, "%s/LCK..%s%s", LOCKDIR, STATE_LOCKFILE, configp->suffix);
    
#if 0 /* future */
    sprintf(spoolfile, "%s/%s%s", SPOOLDIR, SPOOLFILE, configp->suffix);
    sprintf(relaylockfile, "%s/LCK..%s%s", LOCKDIR, RELAYFILE, configp->suffix);
    sprintf(writelockfile, "%s/LCK..%s%s", LOCKDIR, WRITEFILE, configp->suffix);
    sprintf(ttylockfile, "%s/LCK.%s", LOCKDIR, configp->suffix);
#endif /* future */

    return 0;
}

/*---------------------------------------------------------------------+
 | Open the user's X10 configuration file and call parse_config() to   |
 | parse it and fill in global structure config.  exit(1) is called    |
 | if the file cannot be found or read, or if it contains errors.      | 
 +---------------------------------------------------------------------*/
int get_configuration ( unsigned char mode )
{

   FILE       *fd ;
   int        error_count;
   char       confp[PATH_LEN + 1];
   CONFIG     configtmp;

   /* Return if the configuration file has already been read into memory */
   if ( config.read_flag != 0 ) {
      return 0;
   }

   configp = &configtmp;

   find_heyu_path();

   strncpy2(confp, pathspec(heyu_config), sizeof(confp) - 1);

   if ( verbose ) 
      (void) fprintf(stdout,
          "Reading Heyu configuration file '%s'\n", confp); 
 
   if ( !(fd = fopen(confp, "r")) ) {
      if ( !i_am_relay )
         fprintf(stderr, "Unable to find (or open) Heyu configuration file '%s'\n", confp);
      else
         syslog(LOG_ERR, "Unable to find (or open) Heyu configuration file '%s'\n", confp);
    
       exit(1);
   }

   setup_countdown_timers();

   error_count = parse_config( fd, mode );

   (void) fclose( fd );

   if ( error_count != 0 ) {
      if ( mode == CONFIG_RESTART && (i_am_aux || i_am_relay) ) {
         return error_count;
      }
      else {
         (void)fprintf(stderr, 
                 "Quitting due to errors in configuration file '%s'\n", confp);
         exit(1);
      }
   }
 
   return 0;
}

/*---------------------------------------------------------------------+
 | Parse a time string X:Y:Z and return the total time represented by  |
 | 3600*X + 60*Y + Z, or -1 if error. With X:Y:Z = hh:mm:ss the value  |
 | returned will typically represent seconds.  With just Y:Z the value |
 | returned can be interpreted as either hh:mm or mm:ss depending on   |
 | the context.  A null field or empty field, i.e., "::" or ": :" is   |
 | flagged as an error, as is a trailing ':'.                          |
 +---------------------------------------------------------------------*/
long parse_hhmmss ( char *hhmmss, int maxfields )
{
   int  j, tokc;
   long value, seconds = 0;
   char **tokv;
   char *sp;
   char buf[32];

   if ( strstr(hhmmss, "::") != NULL ||
        *(hhmmss + strlen(hhmmss) - 1) == ':' ||
        strlen(hhmmss) > (sizeof(buf) - 1) ) {
      return -1;
   }

   strcpy(buf, hhmmss);

   tokenize(buf, ":", &tokc, &tokv );

   if ( tokc > maxfields ) {
      free(tokv);
      return -1;
   }

   for ( j = 0; j < tokc; j++ ) {
      if ( *strtrim(tokv[j]) == '\0' ) {
         free(tokv);
         return -1;
      }
      else {
         value = strtol(tokv[j], &sp, 10);
         if ( strchr(" \t\n", *sp) == NULL ) {
            free(tokv);
            return -1;
         }
      }
      seconds = 60 * seconds + value;
   }
   free(tokv);
   return seconds;
}

/*---------------------------------------------------------------------+
 | Verify unique IDs for Sec, Ent or RFXSensor modules                 |
 +---------------------------------------------------------------------*/
int verify_unique_ids ( unsigned char vtype )
{
   ALIAS          *aliasp;
   unsigned short ident, mask;
   int            j, k;
   char           errmsg[160];
   int            ntable, dupes;

   struct idtable_st {
      unsigned short ident;
      int            index;
   } idtable[512];

   if ( !(aliasp = configp->aliasp) )
      return 0;

   mask = (vtype == RF_SEC) ? configp->securid_mask :
          (vtype == RF_OREGON) ? configp->oreid_mask : 0xffffu;

   strcpy(errmsg, "The same ID appears in ALIASes:");

   ntable = 0;
   j = 0;
   while ( aliasp[j].line_no > 0 ) {
      if ( aliasp[j].vtype == vtype ) {
         for ( k = 0; k < aliasp[j].nident; k++ ) {
            idtable[ntable].ident = aliasp[j].ident[k] & mask;
            idtable[ntable++].index = j;
         }
      }
      j++;
   }

   dupes = 0;
   for ( j = 0; j < ntable; j++ ) {
      ident = idtable[j].ident;
      for ( k = j + 1; k < ntable; k++ ) {
         if ( idtable[k].ident == ident ) {
            dupes++;
            snprintf(errmsg + strlen(errmsg), sizeof(errmsg) - 1, " (%s %s)",
               aliasp[idtable[j].index].label, aliasp[idtable[k].index].label);
         }
      }
   }

   if ( dupes > 0 ) {
      store_error_message(errmsg);
      return 1;
   }

   return 0;
}

/*---------------------------------------------------------------------+
 | Display configuration file stripped of comments and blank lines.    |
 +---------------------------------------------------------------------*/
void show_configuration ( void )
{
   FILE       *fd ;
   char       confp[PATH_LEN + 1];
   char       buffer[LINE_LEN];
   char       *sp;

   get_configuration(CONFIG_INIT);

   find_heyu_path();

   strncpy2(confp, pathspec(heyu_config), sizeof(confp) - 1);
 
   if ( !(fd = fopen(confp, "r")) ) {
       exit(1);
   }

   while ( fgets(buffer, sizeof(buffer), fd) != NULL ) {      
      if ( (sp = strchr(buffer, '#')) != NULL )
         *sp = '\0';
      strtrim(buffer);
      if ( *buffer )
         printf("%s\n", buffer);
   }

   (void) fclose( fd );
   return;
}

void display_webhook_usage ( void )
{
   printf("Usage: heyu webhook <option> -L[fmt] -d|D[fmt] -m|M[fmt] -b[fmt] -ce<list> -nb<repl> [category]\n");
   printf("options:\n");
   printf("  fileinfo      Deprecated - use pathinfo\n");
   printf("  menuinfo      Deprecated - use helpinfo\n");
   printf("  pathinfo      Display pathspecs for Heyu config and log files.\n");
   printf("  flaginfo      Display flags, czflags, or tzflags as long ASCII bitmap.\n");
   printf("  flagbankinfo  Display flags, czflags, or tzflags as multiple banks of 32.\n");
   printf("  helpinfo      Display available help menu commands\n");
   printf("  config_dump   Display display configuration file directives.\n");
   printf("  maskinfo      Display masks for environment variable $X10_Hu\n");
   printf("  flagmaskinfo  Display masks for environment variable $X10_Hu_vFlags\n");
   printf("switches:\n");
   printf("  -L        Prefix with formatted line number in configuration file.\n");
   printf("  -d |-D    Display formatted directive label in lower | upper case.\n");
   printf("  -m |-M    Format multiple directive label numbering beginning with 0 | 1\n");
   printf("  -b        Format body of directive.\n");
   printf("  -ce       Characters in <list> are escaped in script command lines.\n");
   printf("  -nb       Null body is replaced by <repl>.\n");
   printf("The format specification %%V must appear in each format string ([fmt]) where\n");
   printf("it is replaced as appropriate by the line number, directive label, label number,\n");
   printf("or directive body.\n");
   printf("\n");
   printf("categories for config_dump:\n");
   printf("  alias\n");
   printf("  usersyn\n");
   printf("  scene\n");
   printf("  script\n");
   printf("  other    (Everything other than the above directives.)\n");
   printf("  <blank>  (Everything.)\n");
   printf("categories for pathinfo:\n");
   printf("  conf     Configuration file pathspec.\n");
   printf("  log      Logfile pathspec.\n");
   printf("  <blank>  Both the above.\n");
   printf("categories for flaginfo and flagbankinfo:\n");
   printf("  flags    Common flags.\n");
   printf("  czflags  Counter-zero flags.\n");
   printf("  tzflags  Timer-zero flags.\n");
   printf("Example:\n");
   printf("  heyu webhook config_dump -L\"%%V: \" -dheyu_%%V -m\\(%%V\\) -b=\\\"%%V\\\" alias\n");
   printf("yields:\n");
   printf(" 13: heyu_alias(2)=\"porch_light A7 StdWS\"\n");
   printf("\n");
   return;
}

int flags2asciimap ( char *asciimap, unsigned long flags )
{
   int j;
   
   for ( j = 0; j < 32; j++ ) {
      asciimap[j] = (flags & (1 << j)) ? '1' : '0';
   }
   asciimap[32] = '\0';

   return 0;
}

int allflags2asciimap ( char *asciimap, unsigned long *flags, int num_banks )
{
   int j, k, bank;

   k = 0;
   for ( bank = 0; bank < num_banks; bank++ ) {
      for ( j = 0; j < 32; j++ ) {
         asciimap[k++] = (flags[bank] & (1 << j)) ? '1' : '0';
      }
   }
   asciimap[k++] = '\0';

   return 0;
}

      
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++
 |    START WEBHOOK SECTION                             |
 +++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

#define WF_LINE        0x00000001
#define WF_UPPER       0x00000002
#define WF_MULT        0x00000004
#define WF_ESC         0x00000008
#define WF_ALIAS       0x00000010
#define WF_USYN        0x00000020
#define WF_SCENE       0x00000040
#define WF_SCRIPT      0x00000080
#define WF_OTHER       0x00000100
#define WF_CONF        0x00000200
#define WF_LOG         0x00000400
#define WF_NULLBODY    0x00000800
#define WF_FLAGS       0x00001000
#define WF_CZFLAGS     0x00002000
#define WF_TZFLAGS     0x00004000
#define WF_MASKS       0x00008000

#define WF_ALLCAT (WF_ALIAS | WF_USYN | WF_SCENE | WF_SCRIPT | WF_OTHER)

typedef struct {
   char label[NAME_LEN + 1];
   int  index;
} MULTI;

typedef struct {
   char label[NAME_LEN + 1];
} IGNLIST;

static struct cat_st {
   char *label;
   unsigned int flag;
} catlist[] = {
   {"ALIAS",   WF_ALIAS },
   {"USERSYN", WF_USYN  },
   {"SCENE",   WF_SCENE },
   {"SCRIPT",  WF_SCRIPT},
   {"OTHER",   WF_OTHER },
};
#define NCATEGORY (sizeof(catlist)/sizeof(struct cat_st))

struct webhook_st {
   char line_format[NAME_LEN + 1];
   char label_format[NAME_LEN + 1];
   char mult_format[NAME_LEN + 1];
   char body_format[NAME_LEN + 1];
   char escape_list[NAME_LEN + 1];
   char null_body[PATH_LEN + 1];
   unsigned long flags;
   int index_start;
   int nmult;
   int nignore;
   MULTI mtable[sizeof(command)/sizeof(struct conf)];
   IGNLIST igntable[sizeof(command)/sizeof(struct conf)];
}; 

unsigned int category_flag ( char *category )
{
   int j;

   for ( j = 0; j < NCATEGORY; j++ ) {
      if ( strcmp(category, catlist[j].label) == 0 ) {
         return catlist[j].flag;
      }
   }
   return 0;
}

unsigned int directive_flag ( char *directive )
{
   int j;

   for ( j = 0; j < NCATEGORY; j++ ) {
      if ( strcmp(directive, catlist[j].label) == 0 ) {
         return catlist[j].flag;
      }
   }
   return WF_OTHER;
}

void init_hooks ( struct webhook_st *hookp )
{

   strcpy(hookp->line_format, "%d: ");
   strcpy(hookp->label_format, "%s");
   strcpy(hookp->mult_format, "[%d]");
   strcpy(hookp->body_format, " %s");
   strcpy(hookp->escape_list, "");
   strcpy(hookp->null_body, "");
   hookp->flags = 0;
   hookp->index_start = 0;
   hookp->nmult = 0;
   hookp->nignore = 0;

   return;
}

int set_format ( char *fmt_str, char *fmt )
{
   char *sp;

   if ( strlen(fmt_str) > NAME_LEN ) {
      fprintf(stderr, "Format too long.\n");
      return 1;
   }

   if ( (sp = strstr(fmt_str, "%V")) == NULL ) {
      fprintf(stderr, "No '%%V' in format string.\n");
      return 1;
   }
   strncpy(sp, "..", 2);

   if ( strchr(fmt_str, '%') != NULL ) {
      fprintf(stderr, "Only '%%V' may appear in format string.\n");
      return 1;
   }
   strncpy(sp, fmt, 2);

   return 0;
}

int create_mult_ign_tables ( struct webhook_st *hookp )
{
   int i, j, m;
   MULTI  *mtable;
   IGNLIST *igntable;

   mtable = hookp->mtable;
   igntable = hookp->igntable;

   i = m = 0;
   for ( j = 1; j < ncommands; j++ ) {
      if ( command[j].flags & CMLT ) {
         strcpy(mtable[m].label, command[j].name);
         mtable[m].index = hookp->index_start;
         m++;
      }
      else if ( command[j].flags & CIGN ) {
         strcpy(igntable[i].label, command[j].name);
         i++;
      }
   }
   hookp->nmult = m;
   *(mtable[m].label) = '\0';
   hookp->nignore = i;
   *(igntable[i].label) = '\0';

   return m;
}

int if_ignore ( char *directive, struct webhook_st *hookp )
{
   int j;
   IGNLIST *igntable;

   igntable = hookp->igntable;

   for ( j = 0; j < hookp->nignore; j++ ) {
      if ( strcmp(directive, igntable[j].label) == 0 )
         return 1;
   }
   return 0;
}

void init_mult_table ( struct webhook_st *hookp )
{
   int k = 0;
   MULTI *mtable;

   mtable = hookp->mtable;

   for ( k = 0; k < hookp->nmult; k++ )
      mtable[k].index = hookp->index_start;

   return;
}


int mult_index( MULTI *multp, char *label )
{
   int index, k = 0;

   while ( *multp[k].label ) {
      if ( strcmp(multp[k].label, label) == 0 ) {
         index = multp[k].index;
         multp[k].index += 1;
         return index;
      }
      k++;
   }
   return -1;
}


int parse_switches ( int argc, char *argv[], struct webhook_st *hookp )
{
   int j;

   if ( argc < 4 )
      return 0;

   for ( j = 3; j < argc; j++ ) {
      if ( *argv[j] == '-' ) {
         switch ( *(argv[j] + 1) ) {
            case 'L' :
               hookp->flags |= WF_LINE;
               if ( *(argv[j] + 2) != '\0' ) {
                  if ( set_format(argv[j] + 2, "%d") != 0 )
                     return 1;
                  strcpy(hookp->line_format, (argv[j] + 2));
               }
               break;
            case 'D' :
               hookp->flags |= WF_UPPER;
            case 'd' :
               if ( *(argv[j] + 2) != '\0' ) {
                  if ( set_format(argv[j] + 2, "%s") != 0 )
                     return 1;
                  strcpy(hookp->label_format, (argv[j] + 2));
               }
               break;
            case 'M' :
               hookp->index_start = 1;
            case 'm' :
               hookp->flags |= WF_MULT;
               if ( *(argv[j] + 2) != '\0' ) {
                  if ( set_format(argv[j] + 2, "%d") != 0 )
                     return 1;
                  strcpy(hookp->mult_format, (argv[j] + 2));
               }
               break;
            case 'b' :
               if ( *(argv[j] + 2) != '\0' ) {
                  if ( set_format(argv[j] + 2, "%s") != 0 )
                     return 1;
                  strcpy(hookp->body_format, (argv[j] + 2));
               }
               break;
            case 'c' :
               if ( *(argv[j] + 2) == 'e' ) {
                  hookp->flags |= WF_ESC;
                  if ( *(argv[j] + 3) ) {
                     strcpy(hookp->escape_list, (argv[j] + 3));
                  }
               }
               else {
                  fprintf(stderr, "Switch '%s' invalid.\n", argv[j]);
                  return 1;
               }
               break;
            case 'n' :
               if ( *(argv[j] + 2) == 'b' ) {
                  hookp->flags |= WF_NULLBODY;
                  if ( *(argv[j] + 3) ) {
                     strcpy(hookp->null_body, (argv[j] + 3));
                  }
               }
               else {
                  fprintf(stderr, "Switch '%s' invalid.\n", argv[j]);
                  return 1;
               }
               break;

            default :
               fprintf(stderr, "Switch '%s' not recognized.\n", argv[j]);
               return 1;
               break;
         }
      }
   }
   return 0;
}

int webhook_script_cmdline ( char *cmdptr, struct webhook_st *hookp )
{
   char buffer[LINE_LEN];
   char *sp, *dp;

   if ( !(hookp->flags & WF_ESC) )
      return 0;

   strcpy(buffer, cmdptr);
   sp = buffer;
   dp = cmdptr;
   while ( *sp ) {
      if ( strchr(hookp->escape_list, *sp) )
         *dp++ = '\\';
      *dp++ = *sp++;
   }
   *dp = '\0';

   return 0;
}

void config_dump_category ( FILE *fd, unsigned int wf_flag, struct webhook_st *hookp )
{
   char buffer[LINE_LEN];
   char outbuf[LINE_LEN];
   char directive[NAME_LEN + 1];
   char *sp, *cmdptr;
   int  j, tokc, line_no, index;
   unsigned int flags, dflag;
   char **tokv = NULL;
   MULTI *mtable;
   IGNLIST *igntable;

   rewind(fd);

   flags = hookp->flags;
   mtable = hookp->mtable;
   igntable = hookp->igntable;
   
   line_no = 0;
   while ( fgets(buffer, sizeof(buffer), fd) != NULL ) { 
      line_no++;     
      if ( (sp = strchr(buffer, '#')) != NULL )
         *sp = '\0';
      strtrim(buffer);
      if ( *buffer == '\0' ) 
         continue;
      sp = buffer;
      get_token(directive, &sp, " \t\r\n", NAME_LEN + 1);
      strupper(directive);

      if ( if_ignore(directive, hookp) )
         continue;

      dflag = directive_flag(directive);
      if ( !(dflag & wf_flag) )
         continue;

      index = mult_index(mtable, directive);
      if ( !(flags & WF_UPPER) )
         strlower(directive);
      if ( flags & WF_LINE )
         printf(hookp->line_format, line_no);
      printf(hookp->label_format, directive);
      if ( (flags & WF_MULT) && (index >= 0) )
         printf(hookp->mult_format, index);

      *outbuf = '\0';
      if ( dflag & WF_SCRIPT ) {
         cmdptr = strstr(sp, "::");
         *cmdptr = '\0';
         tokenize(sp, " \t\r\n", &tokc, &tokv);
         for ( j = 0; j < tokc; j++ ) {
            strcat(outbuf, tokv[j]);
            strcat(outbuf, " ");
         }
         *cmdptr = ':';
         webhook_script_cmdline(cmdptr + 2, hookp);
         strcat(outbuf, cmdptr);
      }
      else {
         tokenize(sp, " \t\r\n", &tokc, &tokv);
         for ( j = 0; j < tokc; j++ ) {
            strcat(outbuf, tokv[j]);
            strcat(outbuf, " ");
         }
         *(outbuf + strlen(outbuf) - 1) = '\0';
      }

      printf(hookp->body_format, outbuf);
      printf("\n");
      free(tokv);
   }

   return;
}

int webhook_config_dump ( int argc, char *argv[] )
{
   char category[NAME_LEN + 1];
   struct webhook_st hooks;
   MULTI *mtable;
   FILE       *fp;
   char       confp[PATH_LEN + 1];
   int        j;

   init_hooks(&hooks);

   if ( argc > 3 && *argv[argc - 1] != '-' ) {
      strncpy2(category, argv[argc - 1], NAME_LEN);
      strupper(category);

      if ( (hooks.flags = category_flag(category)) == 0 ) {
         fprintf(stderr, "Unsupported category '%s'\n", argv[argc - 1]);
         return 1;
      }
   }
   else {
      hooks.flags = 0;
      for ( j = 0; j < NCATEGORY; j++ ) {
         hooks.flags |= catlist[j].flag;
      }
   }

   if ( parse_switches(argc, argv, &hooks) != 0 )
      return 1;

   create_mult_ign_tables(&hooks);
   init_mult_table(&hooks);

   mtable = hooks.mtable;

   get_configuration(CONFIG_INIT);

   find_heyu_path();

   strncpy2(confp, pathspec(heyu_config), sizeof(confp) - 1);
 
   if ( !(fp = fopen(confp, "r")) ) {
       exit(1);
   }

   config_dump_category(fp, hooks.flags, &hooks);

   fclose( fp );

   return 0;
}
   
int webhook_pathinfo ( int argc, char *argv[] )
{
   char *configlabel[] = {"conf", "CONF"};
   char *loglabel[] = {"log", "LOG"};
   char category[NAME_LEN + 1];
   char *label;
   struct webhook_st hooks;

   init_hooks(&hooks);

   if ( argc > 3 && *argv[argc - 1] != '-' ) {
      strncpy2(category, argv[argc - 1], NAME_LEN);
      strupper(category);

      if ( strcmp(category, "CONF") == 0 )
         hooks.flags |= WF_CONF;
      else if ( strcmp(category, "LOG") == 0 )
         hooks.flags |= WF_LOG;
      else {
         fprintf(stderr, "Unsupported category '%s'\n", argv[argc - 1]);
         return 1;
      }
   }
   else {
      hooks.flags |= (WF_CONF | WF_LOG);
   }

   if ( parse_switches(argc, argv, &hooks) != 0 )
      return 1;

   get_configuration(CONFIG_INIT);

   if ( hooks.flags & WF_CONF ) {
      find_heyu_path();
      label = (hooks.flags & WF_UPPER) ? configlabel[1] : configlabel[0];
      printf(hooks.label_format, label);
      printf(hooks.body_format, pathspec(heyu_config));
      printf("\n");
   }

   if ( hooks.flags & WF_LOG ) {
      label = (hooks.flags & WF_UPPER) ? loglabel[1] : loglabel[0];
      printf(hooks.label_format, label);
      if ( strcmp(configp->logfile, "/dev/null") == 0 && (hooks.flags & WF_NULLBODY) )
         printf(hooks.body_format, hooks.null_body);
      else
         printf(hooks.body_format, configp->logfile);
      printf("\n");
   }

   return 0;
}

void get_help_topics ( char **topic, int *ntopic )
{

   *ntopic = 0;

   topic[(*ntopic)++] = "admin";
   topic[(*ntopic)++] = "direct";
   topic[(*ntopic)++] = "state";
   topic[(*ntopic)++] = "internal";
#ifdef HAVE_FEATURE_CM17A
   topic[(*ntopic)++] = "cm17a";
#endif
#ifdef HAVE_FEATURE_EXT0
   topic[(*ntopic)++] = "shutter";
#endif
#ifdef HAVE_FEATURE_RFXS
   topic[(*ntopic)++] = "rfxsensor";
#endif
#ifdef HAVE_FEATURE_RFXM
   topic[(*ntopic)++] = "rfxmeter";
#endif
#ifdef HAVE_FEATURE_DMX
   topic[(*ntopic)++] = "digimax";
#endif
#ifdef HAVE_FEATURE_ORE
   topic[(*ntopic)++] = "oregon";
#endif

  return;

}

int webhook_menuinfo ( int argc, char *argv[] )
{
   char *topic[32];
   int  j, ntopic;

   get_help_topics (topic, &ntopic);

   for ( j = 0; j < ntopic; j++ )
      printf("$help_menu[%d]=\"/heyu/help %s\";\n", j, topic[j]);
   return 0;
}


int webhook_fileinfo ( int argc, char *argv[] )
{

   get_configuration(CONFIG_INIT);
   find_heyu_path();
   printf("$heyu_conf=\"%s\";\n", pathspec(heyu_config));
   if ( strcmp(configp->logfile, "/dev/null") == 0 )
      printf("$heyu_log=\"/tmp/\";\n");
   else
      printf("$heyu_log=\"%s\";\n", configp->logfile);
   return 0;

}

int webhook_helpinfo ( int argc, char *argv[] )
{
   char *label;
   struct webhook_st hooks;
   int j, k, ntopic;
   char outformat[80];
   char *topic[32];

   init_hooks(&hooks);

   if ( parse_switches(argc, argv, &hooks) != 0 )
      return 1;

   label = (hooks.flags & WF_UPPER) ? "HELP" : "help" ;

   /* Get list of available help topics */
   get_help_topics(topic, &ntopic);

   if ( hooks.flags & WF_MULT ) {
      sprintf(outformat, "%s%s%s\n", hooks.label_format, hooks.mult_format, hooks.body_format);
      j = hooks.index_start;
      for ( k = 0; k < ntopic; k++ )
         printf(outformat, label, j++, topic[k]);
   }
   else {
      sprintf(outformat, "%s%s\n", hooks.label_format, hooks.body_format);
      for ( k = 0; k < ntopic; k++ )
         printf(outformat, label, topic[k]);
   }      

   return 0;
}


int webhook_flaginfo ( int argc, char *argv[] )
{
   char *flaglabel[] = {"flags", "FLAGS"};
   char *czflaglabel[] = {"czflags", "CZFLAGS"};
   char *tzflaglabel[] = {"tzflags", "TZFLAGS"};

   char category[NAME_LEN + 1];
   char *label;
   struct webhook_st hooks;
   int  read_x10state_file ( void );

   char asciimap[1030];

   get_configuration(CONFIG_INIT);

   init_hooks(&hooks);

   if ( argc > 3 && *argv[argc - 1] != '-' ) {
      strncpy2(category, argv[argc - 1], NAME_LEN);
      strupper(category);

      if ( strcmp(category, "FLAGS") == 0 )
         hooks.flags |= WF_FLAGS;
      else if ( strcmp(category, "CZFLAGS") == 0 )
         hooks.flags |= WF_CZFLAGS;
      else if ( strcmp(category, "TZFLAGS") == 0 )
         hooks.flags |= WF_TZFLAGS;
      else {
         fprintf(stderr, "Unsupported flaginfo category '%s'\n", argv[argc - 1]);
         return 1;
      }
   }
   else {
      fprintf(stderr, "Missing category.  It must be FLAGS, CZFLAGS, or TZFLAGS\n");
      return 1;
   }

   if ( check_for_engine() != 0 ) {
      fprintf(stderr, "State engine is not running.\n");
      return 1;
   }

   if ( fetch_x10state() != 0 ) {
      fprintf(stderr, "Webhook unable to read state file.\n");
      return 1;
   }

   if ( parse_switches(argc, argv, &hooks) != 0 )
      return 1;

   if ( hooks.flags & WF_FLAGS ) {
      allflags2asciimap(asciimap, x10global.flags, NUM_FLAG_BANKS);
      label = (hooks.flags & WF_UPPER) ? flaglabel[1] : flaglabel[0];
   }
   else if ( hooks.flags & WF_CZFLAGS ) {
      allflags2asciimap(asciimap, x10global.czflags, NUM_COUNTER_BANKS);
      label = (hooks.flags & WF_UPPER) ? czflaglabel[1] : czflaglabel[0];
   }
   else if ( hooks.flags & WF_TZFLAGS ) {
      allflags2asciimap(asciimap, x10global.tzflags, NUM_TIMER_BANKS);
      label = (hooks.flags & WF_UPPER) ? tzflaglabel[1] : tzflaglabel[0];
   }
   else {
      return 1;
   }

   printf(hooks.label_format, label);
   printf(hooks.body_format, asciimap);
   printf("\n");

   return 0;
}

int webhook_flagbankinfo ( int argc, char *argv[] )
{

   char *flaglabel[] = {"flagbank", "FLAGBANK"};
   char *czflaglabel[] = {"czflagbank", "CZFLAGBANK"};
   char *tzflaglabel[] = {"tzflagbank", "TZFLAGBANK"};

   char category[NAME_LEN + 1];
   char *label;
   struct webhook_st hooks;
   int  read_x10state_file ( void );

   char asciimap[34];
   int  j;

   get_configuration(CONFIG_INIT);

   init_hooks(&hooks);

   if ( argc > 3 && *argv[argc - 1] != '-' ) {
      strncpy2(category, argv[argc - 1], NAME_LEN);
      strupper(category);

      if ( strcmp(category, "FLAGS") == 0 )
         hooks.flags |= WF_FLAGS;
      else if ( strcmp(category, "CZFLAGS") == 0 )
         hooks.flags |= WF_CZFLAGS;
      else if ( strcmp(category, "TZFLAGS") == 0 )
         hooks.flags |= WF_TZFLAGS;
      else {
         fprintf(stderr, "Unsupported flagbankinfo category '%s'\n", argv[argc - 1]);
         return 1;
      }
   }
   else {
      fprintf(stderr, "Missing category.  It must be FLAGS, CZFLAGS, or TZFLAGS\n");
      return 1;
   }

   if ( check_for_engine() != 0 ) {
      fprintf(stderr, "State engine is not running.\n");
      return 1;
   }

   if ( fetch_x10state() != 0 ) {
      fprintf(stderr, "Webhook unable to read x10state file.\n");
      return 1;
   }

   if ( parse_switches(argc, argv, &hooks) != 0 )
      return 1;

   if ( hooks.flags & WF_FLAGS ) {
      label = (hooks.flags & WF_UPPER) ? flaglabel[1] : flaglabel[0];
      for ( j = 0; j < NUM_FLAG_BANKS; j++ ) {
         flags2asciimap(asciimap, x10global.flags[j]);
         printf(hooks.label_format, label);
         printf(hooks.mult_format, hooks.index_start + j);
         printf(hooks.body_format, asciimap);
         printf("\n");
      }
   }
   else if ( hooks.flags & WF_CZFLAGS ) {
      label = (hooks.flags & WF_UPPER) ? czflaglabel[1] : czflaglabel[0];
      for ( j = 0; j < NUM_COUNTER_BANKS; j++ ) {
         flags2asciimap(asciimap, x10global.czflags[j]);
         printf(hooks.label_format, label);
         printf(hooks.mult_format, hooks.index_start + j);
         printf(hooks.body_format, asciimap);
         printf("\n");
      }
   }
   else if ( hooks.flags & WF_TZFLAGS ) {
      label = (hooks.flags & WF_UPPER) ? tzflaglabel[1] : tzflaglabel[0];
      for ( j = 0; j < NUM_TIMER_BANKS; j++ ) {
         flags2asciimap(asciimap, x10global.tzflags[j]);
         printf(hooks.label_format, label);
         printf(hooks.mult_format, hooks.index_start + j);
         printf(hooks.body_format, asciimap);
         printf("\n");
      }
   }
   else {
      return 1;
   }

   return 0;
}

int webhook_maskinfo ( int argc, char *argv[] )
{
   char *label;
   int j;
   char minibuf[32];
   unsigned int mask;
   struct webhook_st hooks;
   extern int get_env_funcmask ( int, char **, unsigned int * );
   extern int get_env_flagmask ( int, char **, unsigned int * ); 

   init_hooks(&hooks);

   hooks.flags |= WF_MASKS;

   if ( parse_switches(argc, argv, &hooks) != 0 )
      return 1;

   j = 0;
   while ( get_env_funcmask(j, &label, &mask) == 0 ) {
      printf(hooks.label_format, label);
      sprintf(minibuf, "%d", mask);
      printf(hooks.body_format, minibuf);
      printf("\n");
      j++;
   }

   return 0;
}

int webhook_flagmaskinfo ( int argc, char *argv[] )
{
   char *label;
   int j;
   char minibuf[32];
   unsigned int mask;
   struct webhook_st hooks;
   extern int get_env_funcmask ( int, char **, unsigned int * );
   extern int get_env_flagmask ( int, char **, unsigned int * ); 

   init_hooks(&hooks);

   hooks.flags |= WF_MASKS;

   if ( parse_switches(argc, argv, &hooks) != 0 )
      return 1;

   j = 0;
   while ( get_env_flagmask(j, &label, &mask) == 0 ) {
      printf(hooks.label_format, label);
      sprintf(minibuf, "%d", mask);
      printf(hooks.body_format, minibuf);
      printf("\n");
      j++;
   }   

   return 0;
}


int c_webhook ( int argc, char *argv[] )
{
   if ( argc < 3 ) {
      display_webhook_usage();
      return 0;
   }

   if ( strcmp(argv[2], "config_dump") == 0 )
      return webhook_config_dump(argc, argv);
   else if ( strcmp(argv[2], "pathinfo") == 0 )
      return webhook_pathinfo(argc, argv);
   else if ( strcmp(argv[2], "helpinfo") == 0 )
      return webhook_helpinfo(argc, argv);
   else if ( strcmp(argv[2], "flaginfo") == 0 )
      return webhook_flaginfo(argc, argv);
   else if ( strcmp(argv[2], "flagbankinfo") == 0 )
      return webhook_flagbankinfo(argc, argv);
   else if ( strcmp(argv[2], "maskinfo") == 0 )
      return webhook_maskinfo(argc, argv);
   else if ( strcmp(argv[2], "flagmaskinfo") == 0 )
      return webhook_flagmaskinfo(argc, argv);
   else if ( strcmp(argv[2], "fileinfo") == 0 )
      return webhook_fileinfo(argc, argv);      /* deprecated */
   else if ( strcmp(argv[2], "menuinfo") == 0 )
      return webhook_menuinfo(argc, argv);      /* deprecated */
   else {
      fprintf(stderr, "Invalid webhook command '%s'\n", argv[2]);
      return 1;
   }
   return 0;
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++
 |    END WEBHOOK SECTION                               |
 +++++++++++++++++++++++++++++++++++++++++++++++++++++++*/



int cmp_directives ( const void *m1, const void *m2 )
{
   struct conf *one, *two;
   
   one = (struct conf *)m1; two = (struct conf *)m2;

   return strcmp(one->name, two->name);
}

int c_conflist ( int argc, char *argv[] )
{
   int j;
   qsort(command + 1, ncommands - 1, sizeof(struct conf), cmp_directives);

   for ( j = 1; j < ncommands - 1; j++ )
      if ( !(command[j].flags & (CIGN | CHID)) )
         printf("%s\n", command[j].name);
   printf("\n");

   return 0;
}

/*---------------------------------------------------------------------+
 | Compare arrays of ALIAS structures to see if any have been changed. |
 | Return 1 if changed or 0 if unchanged.                              |
 +---------------------------------------------------------------------*/
int is_alias_changed ( ALIAS *aliasp1, int size1, ALIAS *aliasp2, int size2 )
{
   ALIAS *aliaspp1, *aliaspp2;
   int j, k, dif;
   
   if ( size1 != size2 || size1 == 0 )
      return 1;   

   for ( j = 0; j < size1; j++ ) {
      aliaspp1 = &aliasp1[j]; aliaspp2 = &aliasp2[j];
      dif = (strcmp(aliaspp1->label, aliaspp2->label))   ? 1 :
            (aliaspp1->housecode != aliaspp2->housecode) ? 1 :
            (aliaspp1->hcode     != aliaspp2->hcode)     ? 1 :
            (aliaspp1->unitbmap  != aliaspp2->unitbmap)  ? 1 :
            (aliaspp1->ext0links != aliaspp2->ext0links) ? 1 :
            (aliaspp1->modtype   != aliaspp2->modtype)   ? 1 :
            (aliaspp1->vflags    != aliaspp2->vflags)    ? 1 :
            (aliaspp1->flags     != aliaspp2->flags)     ? 1 :
            (aliaspp1->xflags    != aliaspp2->xflags)    ? 1 :
            (aliaspp1->optflags  != aliaspp2->optflags)  ? 1 :
            (aliaspp1->optflags2 != aliaspp2->optflags2) ? 1 :
            (aliaspp1->tmin      != aliaspp2->tmin)      ? 1 :
            (aliaspp1->tmax      != aliaspp2->tmax)      ? 1 :
            (aliaspp1->rhmin     != aliaspp2->rhmin)     ? 1 :
            (aliaspp1->rhmax     != aliaspp2->rhmax)     ? 1 :
            (aliaspp1->onlevel   != aliaspp2->onlevel)   ? 1 :
            (aliaspp1->maxlevel  != aliaspp2->maxlevel)  ? 1 :
            (aliaspp1->vtype     != aliaspp2->vtype)     ? 1 :
            (aliaspp1->subtype   != aliaspp2->subtype)   ? 1 :
            (aliaspp1->nident    != aliaspp2->nident)    ? 1 :
//            (aliaspp1->timeout   != aliaspp2->timeout)   ? 1 : 
            (aliaspp1->hb_timeout != aliaspp2->hb_timeout)   ? 1 : 
            (aliaspp1->storage_index != aliaspp2->storage_index) ? 1 :
            (aliaspp1->storage_units != aliaspp2->storage_units) ? 1 : 0;

      if ( dif )
         return 1;

      for ( k = 0; k < aliaspp1->nident; k++ ) {
         if ( aliaspp1->ident[k] != aliaspp2->ident[k] )
            return 1;
      }
      for ( k = 0; k < NFUNCLIST; k++ ) {
         if ( aliaspp1->funclist[k] != aliaspp2->funclist[k] )
            return 1;
      }
      for ( k = 0; k < NOFFSET; k++ ) {
         if ( aliaspp1->offset[k] != aliaspp2->offset[k] )
            return 1;
      }         
   }

  return 0;
}  



