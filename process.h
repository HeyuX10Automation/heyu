/*----------------------------------------------------------------------------+
 |                                                                            |
 |      Enhanced HEYU Functionality for Uploaded Timers and Macros            |
 |        Copyright 2002,2003,2004,2005,2006 Charles W. Sullivan              |
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

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#include <time.h>

/* Some array length limits */
#define NAME_LEN         32   /* Alias label length */
#define MACRO_LEN        32   /* Macro label length */
#define SCENE_LEN        32   /* Scene label length */
#define PATH_LEN        127   /* File paths */
#define LINE_LEN       4096   /* Line length in input files */

#ifndef NUM_FLAG_BANKS
#define NUM_FLAG_BANKS    1   /* 32 flags per bank, maximum 32 banks (1024 flags) */
#endif
#ifndef NUM_COUNTER_BANKS
#define NUM_COUNTER_BANKS 1   /* 32 counters per bank, maximum 32 banks (1024 counters) */
#endif
#ifndef NUM_TIMER_BANKS
#define NUM_TIMER_BANKS   1   /* 32 user timers per bank, maximum 32 banks (1024 timers) */
#endif

#define NUM_USER_TIMERS  (32 * NUM_TIMER_BANKS)


/* Maximum number of unsigned long data storage locations */
/* in x10global state tables.                             */
#define MAXDATASTORAGE 128

/* Values for x10config variables DAWN_OPTION and DUSK_OPTION */
#define FIRST        1
#define EARLIEST     2
#define LATEST       3
#define AVERAGE      4
#define MEDIAN       5

/* Values for LAUNCH_SCRIPTS */
#define FIRST_MATCH  0
#define ALL_MATCH    1

/* Values for Launcher flow mode */
#define FM_CONTINUE  1
#define FM_BREAK     2

/* Bitmaps for x10config variables LAUNCH_SOURCE */
#define SNDC      1
#define SNDM      2
#define SNDP      4
#define SNDS      8
#define SNDT     16
#define RCVI     32
#define RCVT     64
#define SNDA    128
#define RCVA    256
#define XMTF    512
#define LSNONE 1024
#define LSALL  (SNDC | SNDM | SNDT | RCVI | RCVT | SNDA | RCVA | XMTF)

enum {SndC, SndM, SndP, SndS, SndT, RcvI, RcvT, SndA, RcvA, Xmtf};

/* Bitmaps for script options */
#define SCRIPT_HEYU     0  /* Generate heyu environment for scripts */
#define SCRIPT_XTEND    1  /* Generate xtend environment for scripts */
#define SCRIPT_RAWLEVEL 2  /* Heyu environ variables use raw levels */
#define SCRIPT_NOENV    4  /* Create no new environment variables */
#define SCRIPT_QUIET    8  /* Suppress display of command line in log file */
#define SCRIPT_QQUIET  16  /* Suppress display of launcher in log file */

/* Values for script_mode */
#define HEYU_SCRIPT     0  /* Use general heyu scripting */
#define HEYU_HELPER     1  /* Use heyuhelper */

/* Function Modes */
#define FUNC_ACTUAL   0
#define FUNC_GENERIC  1

/* Flags for alias options (Preset modules only) */
#define ALFLAG_RESUME  1     /* 'resume level' has been set */
#define ALFLAG_FIXED   2     /* 'fixed on level' has been set */

/* Values for x10config variabe RESOLVE_OVERLAP */
#define RES_OVLAP_SEPARATE  0
#define RES_OVLAP_COMBINED  1

/* Yes/No */
#define NO        0
#define YES       1
#define VERBOSE   3
#define IGNORE    4
#define NO_ANSWER 0xff

/* Check lockup */
#define CHECK_PORT  1
#define CHECK_CM11  2

#define DISABLE 0
#define ENABLE  1

#define NATIVE  0
#define SCALED  1

#define SC_SINGLE  0
#define SC_BITMAP  1

#define PCTMODE  0
#define RAWMODE  1

/* Module Option flags (alias[].optflags) */
#define MOPT_RESUME      0x00000001
#define MOPT_ONFULL      0x00000002
#define MOPT_SENSOR      0x00000004
#define MOPT_SECURITY    0x00000008
#define MOPT_ENTERTAIN   0x00000010
#define MOPT_NOPLC       0x00000020
#define MOPT_RFIGNORE    0x00000040
#define MOPT_TRANSCEIVE  0x00000080
#define MOPT_RFFORWARD   0x00000100
#define MOPT_REVERSE     0x00000200
#define MOPT_MAIN        0x00000400
#define MOPT_AUX         0x00000800
#define MOPT_RFXSENSOR   0x00001000
#define MOPT_RFXT2       0x00002000
#define MOPT_RFXVS       0x00004000
#define MOPT_RFXVAD      0x00008000
#define MOPT_RFXRH       0x00010000
#define MOPT_RFXBP       0x00020000
#define MOPT_RFXPOT      0x00040000
#define MOPT_RFXPULSE    0x00080000
#define MOPT_RFXPOWER    0x00100000
#define MOPT_RFXWATER    0x00200000
#define MOPT_RFXGAS      0x00400000
#define MOPT_RFXCOUNT    0x00800000
#define MOPT_HEARTBEAT   0x01000000
#define MOPT_PLCSENSOR   0x02000000
#define MOPT_LOBAT       0x04000000
#define MOPT_LOBATUNKN   0x08000000
#define MOPT_KAKU        0x10000000

#define MOPT_RFXMETER    (MOPT_RFXPULSE | MOPT_RFXPOWER | MOPT_RFXWATER | MOPT_RFXGAS | MOPT_RFXCOUNT)

/* More Module Option flags (alias[].optflags2) */
#define MOPT2_TMIN       0x00000001
#define MOPT2_TMAX       0x00000002
#define MOPT2_RHMIN      0x00000004
#define MOPT2_RHMAX      0x00000008
#define MOPT2_BPMIN      0x00000010
#define MOPT2_BPMAX      0x00000020
#define MOPT2_DUMMY      0x00000040
#define MOPT2_SWMAX      0x00000080
#define MOPT2_SWHOME     0x00000100
#define MOPT2_IATO       0x00000200 /* Inactive timeout */
#define MOPT2_AUF        0x00000400 /* ACT AllUnitsOff option */
#define MOPT2_ALO        0x00000800 /* ACT AllLightsOn option */
#define MOPT2_ALF        0x00001000 /* ACT AllLightsOff option */
#define MOPT2_DEFER      0x00002000 /* Defer update until StatusOn/Off */

/* Maximum number of security and RFXMeter sensors */
/* (door/window, motion sensor)       */
#define MAX_SEC_SENSORS 256
#define MAX_RFX_SENSORS 256

/* Old/New */
#define OLD     0
#define NEW     1

/* High/Low */
#define HIGH_STATE  1
#define LOW_STATE   0

/* Config option */
#define CONFIG_INIT     0
#define CONFIG_RESTART  1

/* Start/Restart */
#define E_START   0
#define E_RESTART 1

/* Manual or Automatic */
#define MANUAL    0
#define AUTOMATIC 1

/* For MIN/MAX DAWN/DUSK */
#define OFF    -1

/* Bitmaps for mode variables */
#define UNSPECIFIED  0
#define COMPATIBLE   1
#define HEYU_MODE    2
#define HAS_TIMERS  16

/* Values for variable CM11A_DAY_ZERO */
#define ZERO         0
#define TODAY        1
#define MIXED        2

/* Values involving the X10 Record File */
#define VALID_RECORD_FILE   0
#define SCHEDULE_EXPIRED   -1
#define NO_EXPIRATION      -2
#define NO_RECORD_FILE     -3
#define BAD_RECORD_FILE    -4

/* Prefixes for combined and delay-modified macro names */
#define COMB_MAC_PREFIX     "_co"
#define DELAY_MAC_PREFIX    "_de"
#define SECUR_MAC_PREFIX    "_se"
    
/* Prefix for subdirectories under xxxBASEDIR      */
/* (For optional use with command line parameters  */
/* -0 ... -9 when there is a CM11A interface on    */
/* more than one serial port.)                     */
#define HEYUSUB_PREFIX        ""

/* Files which must have a fixed name and be */
/* in a fixed location.                      */

#define CONFIG_FILE           "x10config"
#define CONFIG_FILE_ETC       "x10.conf"
#define RECORD_FILE           "x10record"
#define MACROXREF_FILE        "x10macroxref"
#define IMAGE_FILE            "x10image"
#define STATE_FILE            "x10state"
#define RESTART_RELAY_FILE    "__restart_relay"
#define RESTART_AUX_FILE      "__restart_aux"

/* Device types */
#define DEV_CM11A         0
#define DEV_CM10A         1
#define DEV_CM17A         2
#define DEV_W800RF32      4
#define DEV_MR26A         8
#define DEV_RFXCOM32     16
#define DEV_RFXCOMVL     32
#define DEV_DUMMY       256

/* TTY */
#define TTY_CLOSED  -1
#define TTY_DUMMY   -2

/* Date Formats */
#define YMD_ORDER  1
#define DMY_ORDER  2
#define MDY_ORDER  4

/* Aux daemon transceive/transcode defaults */
#define TR_NONE   0
#define TR_ALL    0xffff

/* X10 RF code types */
#define RF_STD          0       /* Standard X10 RF */
#define RF_ENT          1       /* Entertainment X10 RF */
#define RF_SEC          2       /* Security X10 RF */
#define RF_XSENSOR      3       /* RFXSensor data */
#define RF_XMETER       4       /* RFXMeter data */
#define RF_DUSK         5       /* Security Dusk RF */
#define RF_VISONIC      6       /* Visonic sensors and remotes */
#define RF_XJAM         7       /* RFXCOMVL jamming signal */
#define RF_XJAM32       8       /* RFXCOM32 jamming signal */
#define RF_SEC16        9       /* Security X10 RF 16-bit ID */
#define RF_DIGIMAX     10       /* Digimax */
#define RF_OREGON      11       /* Oregon sensors */
#define RF_ELECSAVE    12       /* Electrisave sensor */
#define RF_SECX        13       /* DM10 Alert */
#define RF_KAKU        14       /* KAKU/HomeEasy */
#define RF_OWL         15       /* OWL CM119 */
#define RF_VDATAM      16       /* VDATA for memory byte */

#define RF_XSENSORHDR  0x77    /* RFXSensor Initialization */
#define RF_RAWW800     0x78    /* Raw data from W800RF32A */
#define RF_RAWMR26     0x79    /* Raw data from MR26A */
#define RF_RAWVL       0x7a    /* Raw variable length */
#define RF_FLOOD       0x7b    /* Unceasing RF signals */
#define RF_TEST        0x7c
#define RF_NOISEW800   0x7d    /* Noise from W800RF32A */
#define RF_NOISEMR26   0x7e    /* Noise from MR26A */
#define RF_NOISEVL     0x7f    /* Variable length noise */

/* RF Raw display modes */
#define DISPMODE_RF_NONE     0
#define DISPMODE_RF_NORMAL   1
#define DISPMODE_RF_NOISE    2

/* Treatment of RF codes */
#define RF_IGNORE     0
#define RF_FORWARD    1
#define RF_TRANSCEIVE 2

/* RFXSensor ID offsets */
#define RFX_T  0  /* Temperature */
#define RFX_H  1  /* Humidity or Pressure A/D */
#define RFX_S  2  /* Supply Voltage A/D */

/* RFXSensor output bitmap */
#define RFXO_T   0x0001  /* Temperature */
#define RFXO_H   0x0002  /* Humidity */
#define RFXO_S   0x0004  /* Supply Voltage */
#define RFXO_V   0x0008  /* A/D Voltage */
#define RFXO_B   0x0010  /* Barometric Pressure */
#define RFXO_P   0x0020  /* Potentiometer */
#define RFXO_T2  0x0040  /* Second Temperature */
#define RFXO_LO  0x0080  /* Low battery */
#define RFXO_VI  0x0100  /* Internal A/D Voltage */
#define RFXO_ALL 0xffff

#define RAISE_DTR  1
#define RAISE_RTS  2

#define ADDR_STD  0x40 /* Includes a HU address */
#define ADDR_EXT  0x80 /* Virtual extended housecode Q-Z */

/* Wind Direction bitmap */
#define COMPASS_POINTS  1
#define COMPASS_ANGLE   2

/* Wind speed bitmap */
#define WINDSPEED  1
#define BEAUFORT   2

/* RFXCOM Receiver types and positions */
#define RFXX10    0
#define VISONIC   1
#define RFXMASTER 0
#define RFXSLAVE  1

/* The state engine lockfile */
#define STATE_LOCKFILE    "heyu.engine"

/* These files are written for 'heyu upload' or    */
/* 'heyu upload check' if the WRITE_CHECK_FILES    */
/* directive in the x10config file is YES.         */
#define RECORD_FILE_CHECK     "x10record.check"
#define MACROXREF_FILE_CHECK  "x10macroxref.check"
#define IMAGE_BIN_FILE        "x10image.bin"
#define IMAGE_FILE_CHECK      "x10image.check"
#define IMAGE_HEX_FILE        "x10image.hex"

/* Other files */
#define REPORT_FILE       "report.txt"
#define CRON_REPORT_FILE  "cronreport.txt"
#define SUN_TABLE_FILE    "sun_%d.txt"

enum {RiseSet, CivilTwi, NautTwi, AstroTwi, AngleOffset, };
enum {ArmLogicStrict, ArmLogicMedium, ArmLogicLoose};

#define RFXCOM_ARCTECH   0x00000001
#define RFXCOM_OREGON    0x00000002
#define RFXCOM_ATIWONDER 0x00000004
#define RFXCOM_X10       0x00000008
#define RFXCOM_VISONIC   0x00000010
#define RFXCOM_KOPPLA    0x00000020
#define RFXCOM_HE_UK     0x00000040
#define RFXCOM_HE_EU     0x00000080

/* Default values for X10 configuration file */

#define DEF_MODE                  COMPATIBLE
#define DEF_SCHEDFILE             "x10.sched"
#define DEF_PROGRAM_DAYS          366
#define DEF_COMBINE_EVENTS        YES
#define DEF_COMPRESS_MACROS       NO 
#define DEF_FEB_KLUGE             YES
#define DEF_HOUSECODE             'A'
#define DEF_DAWN_OPTION           FIRST
#define DEF_DUSK_OPTION           FIRST
#define DEF_DAWN_SUBSTITUTE       (6*60)
#define DEF_DUSK_SUBSTITUTE       (18*60)
#define DEF_MIN_DAWN              OFF
#define DEF_MAX_DAWN              OFF
#define DEF_MIN_DUSK              OFF
#define DEF_MAX_DUSK              OFF
#define DEF_CHECK_FILES           NO
#define DEF_TTY                   "/dev/ttyS0"
#define DEF_REPL_DELAYED_MACROS   YES
#define DEF_FORCE_ADDR            NO
#define DEF_ACK_HAILS             NO
#define DEF_TRIGGER_TAG           YES
#define DEF_XREF_APPEND           NO
#define DEF_RCS_TEMPERATURE       NO
#define DEF_MACTERM               YES
#define DEF_MAX_PPARMS            8
#define DEF_STATUS_TIMEOUT        2
#define DEF_SPF_TIMEOUT           3
#define DEF_DISPLAY_OFFSET        YES
#define DEF_RESERVED_TIMERS       0
#define DEF_DISPLAY_EXP_MACROS    YES
#define DEF_AUTO_CHAIN            NO
#define DEF_MODULE_TYPES          YES
#define DEF_LAUNCH_MODE           TMODE_SIGNAL
#define DEF_LAUNCH_SOURCE         RCVI
#define DEF_RES_OVERLAP           RES_OVLAP_SEPARATE
#define DEF_DEFAULT_MODULE        "LM"
#define DEF_SCRIPT_OPTION         SCRIPT_HEYU
#define DEF_SCRIPT_MODE           HEYU_SCRIPT
#define DEF_FUNCTION_MODE         FUNC_ACTUAL
#define DEF_LOGFILE               "/dev/null"
#define DEF_ISDARK_OFFSET         30
#define DEF_ENV_ALIAS_PREFIX      "x10"
#define DEF_DEVICE_TYPE           DEV_CM11A
#define DEF_SUNMODE               RiseSet
#define DEF_FIX_5A                YES
#define DEF_AUTOFETCH             YES
#define DEF_PFAIL_UPDATE          YES
#define DEF_CM17A_BIT_DELAY       500
#define DEF_RF_BURST_SPACING      110
#define DEF_RF_TIMER_TWEAK        20
#define DEF_RF_POST_DELAY         850
#define DEF_RF_FARB_DELAY         850
#define DEF_RF_FARW_DELAY         850
#define DEF_DISP_RF_XMIT          YES
#define DEF_RF_BURSTS              5
#define DEF_RESTRICT_DIMS         YES
#define DEF_STATE_FORMAT          NEW
#define DEF_CM11_POST_DELAY        80  /* 4 cycles @ 50 Hz */
#define DEF_START_ENGINE          MANUAL
#define DEF_RF_NOSWITCH           NO
#define DEF_RESPOOL_PERMS         01777
#define DEF_SPOOLFILE_MAX         1000000
#define DEF_CHECK_RI_LINE         YES
#define DEF_RING_CTRL             ENABLE
#define DEF_SEND_RETRIES           3
#define DEF_SCRIPT_CTRL           ENABLE
#define DEF_TRANSCEIVE            TR_ALL
#define DEF_RFFORWARD             TR_NONE
#define DEF_TRANS_DIMLEVEL        2
#define DEF_STATE_CTRL            SC_SINGLE
#define DEF_RF_FUNCMASK           0x7f
#define DEF_DISP_RF_NOISE         NO
#define DEF_AUX_MINCOUNT_W800     1
#define DEF_AUX_MINCOUNT_MR26     2
#define DEF_AUX_MINCOUNT_RFXCOM   1
#define DEF_AUX_MINCOUNT_RFX      1 /* RFXSensor data */
#define DEF_AUX_REPCOUNT          8
#define DEF_AUX_MAXCOUNT          200
#define DEF_AUX_FLOODREP          0
#define DEF_HIDE_UNCHANGED        NO
#define DEF_HIDE_UNCHANGED_INACTIVE   NO
#define DEF_SHOW_CHANGE           NO
#define DEF_ARM_MAX_DELAY         0
#define DEF_DISP_RAW_RF           DISPMODE_RF_NONE
#define DEF_ARM_LOGIC             ArmLogicMedium
#define DEF_INACTIVE_TIMEOUT      14400  /* 4 hours */
#define DEF_HEYU_UMASK            0000
#define DEF_LOG_UMASK             -1
#define DEF_AUTO_WAIT             0
#define DEF_FULL_BRIGHT           22
#define DEF_ENGINE_POLL           10000  /* microsends */
#define DEF_RFX_TSCALE            'C'
#define DEF_RFX_TOFFSET           0.0
#define DEF_RFX_VADSCALE          1.0
#define DEF_RFX_VADOFFSET         0.0
#define DEF_RFX_VADUNITS          "Volts"
#define DEF_RFX_BPSCALE           1.0
#define DEF_RFX_BPOFFSET          0.0
#define DEF_RFX_BPUNITS           "hPa"
#define DEF_RFXCOM_DTR_RTS        (RAISE_DTR | RAISE_RTS)
#define DEF_DEV_RFXCOM            DEV_RFXCOMVL
#define DEF_RFXCOM_HIBAUD         NO
#define DEF_RFX_POWERSCALE        0.001
#define DEF_RFX_POWERUNITS        "kWh"
#define DEF_RFX_WATERSCALE        1.0
#define DEF_RFX_WATERUNITS        ""
#define DEF_RFX_GASSCALE          1.0
#define DEF_RFX_GASUNITS          ""
#define DEF_RFX_PULSESCALE        1.0
#define DEF_RFX_PULSEUNITS        ""
#define DEF_RFXCOM_ENABLE         0
#define DEF_RFXCOM_DISABLE        0
#define DEF_LOCKUP_CHECK          0
#define DEF_TAILPATH              ""
#define DEF_SUPPRESS_RFXJAM       NO
#define DEF_DISPLAY_DMXTEMP       YES
#define DEF_SECURID_16            YES
#define DEF_SECURID_MASK          0xffffu
#define DEF_SECURID_PARITY        YES
#define DEF_LOGDATE_YEAR          NO
#define DEF_DMX_TSCALE            'C'
#define DEF_DMX_TOFFSET           0.0
#define DEF_ORE_TSCALE            'C'
#define DEF_ORE_TOFFSET           0.0
#define DEF_ORE_BPSCALE           1.0
#define DEF_ORE_BPOFFSET          0.0
#define DEF_ORE_BPUNITS           "hPa"
#define DEF_ORE_WGTSCALE          1.0
#define DEF_ORE_WGTUNITS          "kg"
#define DEF_ORE_LOWBATTERY        20
#define DEF_DISPLAY_ORE_ALL       NO
#define DEF_ORE_CHGBITS_T          1
#define DEF_ORE_CHGBITS_RH         1
#define DEF_ORE_CHGBITS_BP         1
#define DEF_ORE_CHGBITS_WGT        1
#define DEF_ORE_CHGBITS_DT         1
#define DEF_ORE_CHGBITS_WSP        1
#define DEF_ORE_CHGBITS_WAVSP      1
#define DEF_ORE_CHGBITS_WDIR       1
#define DEF_ORE_CHGBITS_RRATE      1
#define DEF_ORE_CHGBITS_RTOT       1
#define DEF_ORE_CHGBITS_UV         1
#define DEF_ORE_DATA_ENTRY         NATIVE
#define DEF_ORE_DISPLAY_CHAN       YES
#define DEF_ORE_DISPLAY_BATLVL     YES
#define DEF_OREID_16               NO
#define DEF_OREID_MASK             0xffffu
#define DEF_LOGDATE_UNIX           NO
#define DEF_INACTIVE_TIMEOUT_ORE   14400 /* 4 hours */
#define DEF_DISPLAY_SENSOR_INTV    NO
#define DEF_DATE_FORMAT            YMD_ORDER
#define DEF_DATE_SEPARATOR         '/'
#define DEF_LOCK_TIMEOUT           10
#define DEF_ORE_DISPLAY_FCAST      NO
#define DEF_CM11A_QUERY_DELAY      0
#define DEF_ELS_NUMBER             1
#define DEF_ELS_VOLTAGE            0.0
#define DEF_ELS_CHGBITS_CURR       1
#define DEF_ORE_WINDSCALE          1.0
#define DEF_ORE_WINDUNITS          "m/s"
#define DEF_ORE_WINDSENSORDIR      0
#define DEF_ORE_RAINRATESCALE      1.0
#define DEF_ORE_RAINRATEUNITS      ""
#define DEF_ORE_RAINTOTSCALE       1.0
#define DEF_ORE_RAINTOTUNITS       ""
#define DEF_ORE_WINDDIR_MODE       (COMPASS_POINTS | COMPASS_ANGLE)
#define DEF_ORE_DISPLAY_COUNT      NO
#define DEF_ORE_DISPLAY_BFT        NO
#define DEF_SCANMODE               FM_BREAK;
#define DEF_RFX_INLINE             NO
#define DEF_OWL_CHGBITS_POWER      1
#define DEF_OWL_CHGBITS_ENERGY     1
#define DEF_OWL_CALIB_POWER        1.0
#define DEF_OWL_CALIB_ENERGY       1.0
#define DEF_OWL_VOLTAGE            230.0
#define DEF_OWL_DISPLAY_COUNT      NO
#define DEF_ARM_REMOTE             AUTOMATIC
#define DEF_ACTIVE_CHANGE          NO
#define DEF_INACTIVE_HANDLING      NEW
#define DEF_PROCESS_XMIT           NO
#define DEF_SHOW_FLAGS_MODE        NEW
#define DEF_RFX_MASTER             RFXX10
#define DEF_RFX_SLAVE              RFXX10
#define DEF_LAUNCHPATH_APPEND      ""
#define DEF_LAUNCHPATH_PREFIX      ""
#define DEF_FIX_STOPSTART_ERROR    NO
#define DEF_CHKSUM_TIMEOUT         10

#define DTR_INIT  LOW_STATE
#define RTS_INIT  LOW_STATE

#define SPOOLFILE_ABSMIN  20
#define SPOOLFILE_ABSMAX  0x100000

/* Max errors to display before quitting when */
/* parsing configuration or schedule files.   */

#define MAX_ERRORS     15  

/* Bitmap flags for timers and tevents */
#define NO_EVENT           0
#define CLOCK_EVENT        1
#define DAWN_EVENT         2
#define DUSK_EVENT         4
#define SEC_EVENT          8

#define PRT_EVENT         16
#define COMB_EVENT        32
#define SUBST_EVENT       64
#define LOST_EVENT       128

#define TMP_EVENT        256
#define ACTIVE_EVENT     512
#define NOW_EVENT       1024
#define CHAIN_EVENT     2048
#define CANCEL_EVENT    4096
#define INVALID_EVENT   0xFFFF
#define TIME_EVENTS     (CLOCK_EVENT | DAWN_EVENT | DUSK_EVENT)
#define NOCOPY_EVENTS   (PRT_EVENT | LOST_EVENT)

/* Event cancellation flags */
#define MIN_DAWN_CANCEL   1
#define MAX_DAWN_CANCEL   2
#define MIN_DUSK_CANCEL   4
#define MAX_DUSK_CANCEL   8

/* Dawn/Dusk limit option flags */
#define DAWNGT   1
#define DAWNLT   2
#define DUSKGT   4
#define DUSKLT   8

/* Bitmap for shifted events */
#define SAME_DAY       0
#define PREV_DAY       1
#define NEXT_DAY       2

/* Bitmap used by function macro_index() */
#define MACRO_PARSER     1
#define TIMER_PARSER     2
#define TRIGGER_PARSER   4
#define CHAIN_PARSER     8
#define MACRO_DUPLICATE 16
#define DERIVED_MACRO   32

/* Macro modification bitmap */
#define UNMODIFIED    0
#define COMBINED      1
#define COMPRESSED    2
#define DELAY_MOD     4
#define SECURITY_MOD  8
#define TERMINATE    16
#define CHAINED      32

#define NULL_MACRO_INDEX  0
#define NULL_MACRO_OFFSET 0
#define NULL_TIME         (120 * 0x0f + 0x7f)  /* ( = 1927 minutes ) */

/* Flag values for valid Lat/Long. */
#define LATITUDE       1
#define LONGITUDE      2

/* Controls accepted by function get_freespace() */
#define ALL_TIMERS   1
#define CLK_TIMERS   2

/* Arguments for process_data() */
#define PROC_CHECK   1
#define PROC_UPLOAD  2

/* Arguments for direct_cmd() */
#define CMD_VERIFY  0
#define CMD_RUN     1
#define CMD_INT     2

/* Counter functions */
#define CNT_SET     0
#define CNT_INC     1
#define CNT_DEC     2
#define CNT_DSKPZ   3
#define CNT_DSKPNZ  4
#define CNT_DSKPNZIZ 5

/* Identifies macros which are used over the programmed interval */
#define NOTUSED   0
#define USED      1

#define ACTIVE_START 1
#define ACTIVE_STOP  2

/* Values for Trigger Off/On mode in schedule */
#define TRIGGER_OFF  0
#define TRIGGER_ON   1

/* Value for Trigger Tag */
#define TRIGGER_TAG  0x40
#define TIMER_EXEC    0
#define TRIGGER_EXEC  1

/* Values for CM11a security mode bits and offset adjustment */
#define SECURITY_OFF  0x00
#define SECURITY_ON   0x02
#define SECURITY_OFFSET_ADJUST 30

/* Bitmap flags for "as-of" dates and times */
#define ASIF_NONE  0
#define ASIF_DATE  1
#define ASIF_TIME  2

/* Controls for time_adjust() */
#define LGL2STD    0
#define STD2LGL    1

/* Bitmap flags for SCENE/USERSYN */
#define F_SCENE    1
#define F_USYN     2

/* How to interpret undefined legal times, e.g., in the */
/* USA, times between 02:00 and 02:59 on the day when   */
/* Daylight Time goes into effect.                      */
#define STD_TIME      0
#define DST_TIME      1
#define UNDEF_TIME    STD_TIME

/* An env variable name for use by the relay */
#define CLOCKSTAMP    "HEYUCLOCKSTAMP"

/* Delay after setclock, reset, purge, or clear */
#define SETCLOCK_DELAY    2000  /* milliseconds */

/* Flags for parse_addr() function */
#define A_VALID   1
#define A_HCODE   2
#define A_BMAP    4
#define A_PLUS    8
#define A_MINUS  16
#define A_ALIAS  32
#define A_DUMMY  64
#define A_MULT  128
#define A_STAR  256

/* CM17A RF Timing modes */
#define RF_SLOW  0
#define RF_FAST  1

/* Origin of call to parse_config_tail() */
#define SRC_CONFIG    1
#define SRC_SCHED     2
#define SRC_ENVIRON   4
#define SRC_STOP      8
#define SRC_HELP     16
#define SRC_SYSCHECK 32
#define SRC_MULT     64

/* Phase synch modes */
#define NO_SYNC      0
#define RISE_SYNC    1
#define FALL_SYNC    2

/* RFXmitter frequency */
#define MHZ310       1
#define MHZ433       2

/* State engine command functions  */
/* 3 bytes: header, function, data */
#define ST_COMMAND       0x7b  /* header */
#define ST_INIT_ALL      0
#define ST_INIT          1
#define ST_WRITE         2
#define ST_LAUNCH        3
#define ST_LAUNCH_GLOBAL 4
#define ST_CHKSUM        5
#define ST_APPEND        6
#define ST_EXIT          7
#define ST_MSG           8
#define ST_XMITRF        9
#define ST_RESETRF      10
#define ST_REWIND       11
#define ST_INIT_OTHERS  12
#define ST_BUSYWAIT     13
#define ST_SOURCE       14
#define ST_FLAGS        15
#define ST_PFAIL        16
#define ST_SHOWBYTE     17
#define ST_CLRSTATUS    18
#define ST_VDATA        19
#define ST_RESTART      20
#define ST_SECFLAGS     21
#define ST_LONGVDATA    22
#define ST_RFFLOOD      23
#define ST_AUX_ADDR     24
#define ST_AUX_FUNC     25
#define ST_SETTIMER     26
#define ST_CLRTIMERS    27
#define ST_CLRTAMPER    28
#define ST_RFXTYPE      29
#define ST_RFXSERIAL    30
#define ST_RFXNOISEVL   31
#define ST_RFXRAWVL     32
#define ST_VARIABLE_LEN 33
#define ST_LOCKUP       34
#define ST_COUNTER      35
#define ST_GENLONG      36
#define ST_ORE_EMU      37
#define ST_SCRIPT       38
#define ST_SECURITY     39
#define ST_FLAGS32      40
#define ST_TESTPOINT    41
#define ST_SHOWBUFFER   42

/* Launch trigger modes */
#define TMODE_SIGNAL  0
#define TMODE_MODULE  1

/* Launcher types */
#define L_NORMAL       0x0001
#define L_POWERFAIL    0x0002
#define L_ADDRESS      0x0004
#define L_RFFLOOD      0x0008
#define L_TIMEOUT      0x0010
#define L_SENSORFAIL   0x0020
#define L_LOCKUP       0x0040
#define L_RFXJAM       0x0080
#define L_EXEC         0x0100
#define L_HOURLY       0x0200
#define L_ANY         (L_NORMAL|L_POWERFAIL|L_ADDRESS|L_RFFLOOD|L_TIMEOUT|L_SENSORFAIL|L_LOCKUP|L_RFXJAM|L_EXEC|L_HOURLY)

/* Flags for powerfail time relative to relay startup */
#define R_ATSTART     1
#define R_NOTATSTART  2
/* Time in seconds defining R_ATSTART versus R_NOTATSTART */
#define ATSTART_DELAY 10

/* Daemon which launches script */
#define D_CMDLINE   0
#define D_ENGINE    1
#define D_RELAY     2
#define D_AUXDEV    3
#define D_AUXRCV    4

/* Flag modification modes */
#define CLRFLAG  0
#define SETFLAG  1

/* Sensor activity modes */
#define S_ACTIVE   0
#define S_INACTIVE 1

/* Bitmap definitions for Heyu script environment variables */
#define HEYUMAP_LEVEL     0x000000ff  /* Lowest byte is level 0-100 */
#define HEYUMAP_APPL      0x00000100  /* Appliance (non-dimmable) module */
#define HEYUMAP_SPEND     0x00000200  /* Status pending state */
#define HEYUMAP_OFF       0x00000400  /* Off state */
#define HEYUMAP_ADDR      0x00000800  /* Addressed state */
#define HEYUMAP_CHG       0x00001000  /* Changed state */
#define HEYUMAP_DIM       0x00002000  /* Dimmed state */
#define HEYUMAP_SIGNAL    0x00004000  /* Signal state */
#define HEYUMAP_CLEAR     0x00008000  /* Clear state */
#define HEYUMAP_ALERT     0x00010000  /* Alert state */
#define HEYUMAP_AUXCLEAR  0x00020000  /* AuxClear state */
#define HEYUMAP_AUXALERT  0x00040000  /* AuxAlert state */
#define HEYUMAP_ACTIVE    0x00080000  /* Active */
#define HEYUMAP_INACTIVE  0x00100000  /* Inactive */
#define HEYUMAP_MODCHG    0x00200000  /* Module Change */
#define HEYUMAP_ON        0x00400000  /* On state */

#define FLAGMAP_LOBAT    0x00000001  /* Low battery */
#define FLAGMAP_ROLLOVER 0x00000002  /* Rollover flag */
#define FLAGMAP_SWMIN    0x00000004  /* Min switch */
#define FLAGMAP_SWMAX    0x00000008  /* Max switch */
#define FLAGMAP_MAIN     0x00000010  /* Main channel */
#define FLAGMAP_AUX      0x00000020  /* Aux channel */
#define FLAGMAP_TAMPER   0x00000040  /* Tamper flag */
#define FLAGMAP_TMIN     0x00000080  /* Tmin flag */
#define FLAGMAP_TMAX     0x00000100  /* Tmax flag */
#define FLAGMAP_RHMIN    0x00000200  /* RHmin flag */
#define FLAGMAP_RHMAX    0x00000400  /* RHmax flag */
#define FLAGMAP_BPMIN    0x00000800  /* BPmin flag */
#define FLAGMAP_BPMAX    0x00001000  /* BPmin flag */
#define FLAGMAP_DMXSET   0x00002000  /* DMX Set flag */
#define FLAGMAP_DMXHEAT  0x00004000  /* DMX Heat flag */
#define FLAGMAP_DMXINIT  0x00008000  /* DMX Init flag */
#define FLAGMAP_DMXTEMP  0x00010000  /* DMX Temp flag */

/* Bitmap definitions for Xtend script environment variables */
#define XTMAP_APPL  32   /* Appliance module */
#define XTMAP_ADDR  64   /* Addressed state */
#define XTMAP_ON    128  /* On state */

/* Night and Dark flags share space with 16 common flags */
#define NIGHT_FLAG        (0x01 << 16)
#define DARK_FLAG         (0x02 << 16)

/* Global security flags share space with 16 common flags plus */
/* night and dark flags so need to be shifted by 18 bits.      */
#define GLOBSEC_SHIFT     18
#define GLOBSEC_ARMED     (0x01 << GLOBSEC_SHIFT)
#define GLOBSEC_HOME      (0x02 << GLOBSEC_SHIFT)
#define GLOBSEC_PENDING   (0x04 << GLOBSEC_SHIFT)
#define GLOBSEC_TAMPER    (0x08 << GLOBSEC_SHIFT)
#define GLOBSEC_SLIGHTS   (0x10 << GLOBSEC_SHIFT)
#define GLOBSEC_FLOOD     (0x20 << GLOBSEC_SHIFT)
#define GLOBSEC_RFXJAM    (0x40 << GLOBSEC_SHIFT)

/* Unit Security and RFXMeter vflags */
#define SEC_LOBAT     0x00000001
#define SEC_HOME      0x00000002
#define SEC_AWAY      0x00000004
#define SEC_MIN       0x00000008
#define SEC_MAX       0x00000010
#define SEC_FLOOD     0x00000020
#define SEC_TAMPER    0x00000040
#define SEC_MAIN      0x00000080
#define SEC_AUX       0x00000100
#define RFX_ROLLOVER  0x00000200
#define RFX_JAM       0x00000400
#define DMX_HEAT      0x00000800
#define DMX_SET       0x00001000
#define DMX_INIT      0x00002000
#define ORE_TMIN      0x00004000
#define ORE_TMAX      0x00008000
#define ORE_RHMIN     0x00010000
#define ORE_RHMAX     0x00020000
#define ORE_BPMIN     0x00040000
#define ORE_BPMAX     0x00080000
#define SEC_FAULT     0x00100000
#define DMX_TEMP      0x00200000

/* Used for DS90 sensor model ID determination */
#define COMPUTE_AUX   1
#define COMPUTE_MAIN -1

/* Virtual data packet lengths */
#define LEN_VDATA     6
#define LEN_LONGVDATA 8

/* Types of data written to the spoolfile by heyu */
enum {SENT_WRMI, SENT_ADDR, SENT_FUNC, SENT_EXTFUNC, SENT_STCMD, SENT_OTHER,
      SENT_MESSAGE, SENT_RF, SENT_FLAGS, SENT_PFAIL, SENT_CLRSTATUS,
      SENT_VDATA, SENT_LONGVDATA, SENT_RFFLOOD, SENT_LOCKUP, SENT_SETTIMER, SENT_RFXTYPE,
      SENT_RFXSERIAL, SENT_RFVARIABLE, SENT_COUNTER, SENT_GENLONG, SENT_ORE_EMU, SENT_SCRIPT,
      SENT_KAKU, SENT_FLAGS32, SENT_INITSTATE, SENT_VISONIC, SENT_SHOWBUFFER};
  
/* X10 generic signal types */
enum {
  AllOffSignal, LightsOnSignal, OnSignal, OffSignal,
  DimSignal, BriSignal, LightsOffSignal, Ext3Signal, Ext3DimSignal,
  AllOnSignal, PresetSignal, StatusSignal, StatusOnSignal, StatusOffSignal
};

/* Module masks */
enum {
   AllOffMask, LightsOnMask, OnMask, OffMask, DimMask, BriMask,
   BriDimMask, LightsOffMask, StdMask, Ext3Mask, Ext3DimMask, AllOnMask,
   PresetMask, StatusMask, StatusOnMask, StatusOffMask,
   LightsOnFullMask, OnFullMask, OnFullOffMask, ResumeMask,
   TargMask, Exc16Mask, Exc8Mask, Exc4Mask, Ext0Mask, VdataMask,
   ResumeDimMask, LightsOnUnaddrMask, LightsOffUnaddrMask, Ext3GrpExecMask,
   Ext3GrpRelMask, Ext3GrpOffMask, Ext3GrpOffExecMask, Ext3GrpBriDimFullMask,
   Ext3GrpBriDimMask, Ext3GrpRelT1Mask, Ext3GrpRelT2Mask, Ext3GrpRelT3Mask,
   Ext3GrpRelT4Mask, Ext3StatusMask, Ext3GrpRemMask, PlcSensorMask, AddrMask,
   PhysMask, DeferMask, OnOffUnaddrMask, NumModMasks
};

/* Virtual Module masks */
enum {VstdMask, VentMask, VsecMask, VrfxsMask, VrfxmMask, VdmxMask, VoreMask,
VtstampMask, VkakuMask, NumVmodMasks
};

/* KaKu Module masks */
enum {KoffMask, KonMask, KGoffMask, KGonMask, KpreMask, KGpreMask, KresumeMask,
KPhysMask, NumKmodMasks
};

/* State bitmap indexes */
enum {OnState, DimState, ModChgState, ChgState, LightsOnState, AlertState, ClearState,
   AuxAlertState, AuxClearState, ActiveState, InactiveState, LoBatState,
   ValidState, NullState, ActiveChgState, SpendState, TamperState,
   AddrState, /* SwMinState, SwMaxState,*/ NumStates};

/* Launch trigger types */
enum {
   AllOffTrig, LightsOnTrig, OnTrig, OffTrig, DimTrig, BriTrig,
   LightsOffTrig, ExtendedTrig, HailReqTrig, HailAckTrig,
   PresetTrig, PresetTrig2, DataXferTrig, StatusOnTrig, StatusOffTrig,
   StatusReqTrig, AllOnTrig, ExtPowerUpTrig, VdataTrig, VdataMTrig, NumTriggers
};

/* Enum security trigger types */
enum {
 PanicTrig, ArmTrig, DisarmTrig, AlertTrig, ClearTrig, TestTrig,
 SecLightsOnTrig, SecLightsOffTrig, TamperTrig, DuskTrig, DawnTrig,
 AkeyOnTrig, AkeyOffTrig, BkeyOnTrig, BkeyOffTrig, InactiveTrig, NumSecTriggers
};

/* Launch RFXSensor, RFXMeter, Digimax, Oregon trigger types */
enum {
   RFXTempTrig, RFXTemp2Trig, RFXHumidTrig, RFXPressTrig, RFXVadTrig,
   RFXPotTrig, RFXVsTrig, RFXLoBatTrig, RFXOtherTrig, RFXPulseTrig,
   RFXPowerTrig, RFXWaterTrig, RFXGasTrig, RFXCountTrig,
   DmxTempTrig, DmxOnTrig, DmxOffTrig, DmxSetPtTrig, NumRFXTriggers
};

/* Launch Oregon trigger types */
enum {
   OreTempTrig, OreHumidTrig, OreBaroTrig, OreWeightTrig,
   OreWindSpTrig, OreWindAvSpTrig, OreWindDirTrig,
   OreRainRateTrig, OreRainTotTrig, ElsCurrTrig, OreUVTrig,
   OwlPowerTrig, OwlEnergyTrig, OreDTTrig,
   NumOreTriggers
};

/* KaKu trigger types */
enum {
   KakuOffTrig, KakuOnTrig, KakuGrpOffTrig, KakuGrpOnTrig, KakuUnkTrig,
   KakuPreTrig, KakuGrpPreTrig, KakuUnkPreTrig, NumKakuTriggers
};

/* X10 functions (must align with funclabel[] in cmd.c) */
enum {
   AllOffFunc, LightsOnFunc, OnFunc, OffFunc, DimFunc,
   BrightFunc, LightsOffFunc, ExtendedFunc, HailFunc,
   HailAckFunc, Preset1Func, Preset2Func, DataXferFunc,
   StatusOnFunc, StatusOffFunc, StatusReqFunc, AllOnFunc,
   ExtPowerUpFunc, VdataFunc, VdataMFunc, PanicFunc, ArmFunc, DisarmFunc,
   AlertFunc, ClearFunc, TestFunc, SecLightsOnFunc, SecLightsOffFunc,
   TamperFunc, DuskFunc, DawnFunc, AkeyOnFunc, AkeyOffFunc,
   BkeyOnFunc, BkeyOffFunc,
   RFXTempFunc, RFXTemp2Func, RFXHumidFunc, RFXPressFunc,
   RFXVadFunc, RFXPotFunc, RFXVsFunc, RFXLoBatFunc, RFXOtherFunc,
   RFXPulseFunc, RFXPowerFunc, RFXWaterFunc, RFXGasFunc, RFXCountFunc,
   DmxTempFunc, DmxOnFunc, DmxOffFunc, DmxSetPtFunc,
   OreTempFunc, OreHumidFunc, OreBaroFunc, OreWeightFunc,
   OreWindSpFunc, OreWindAvSpFunc, OreWindDirFunc,
   OreRainRateFunc, OreRainTotFunc,  ElsCurrFunc, OreUVFunc,
   KakuOffFunc, KakuOnFunc, KakuGrpOffFunc, KakuGrpOnFunc, KakuUnkFunc,
   KakuPreFunc, KakuGrpPreFunc, KakuUnkPreFunc,
   OwlPowerFunc, OwlEnergyFunc, InactiveFunc, OreDTFunc,
   InvalidFunc
};

#if 0
/* X10 functions (must align with funclabel[] in cmd.c) */
enum {
   AllOffFunc, LightsOnFunc, OnFunc, OffFunc, DimFunc,
   BrightFunc, LightsOffFunc, ExtendedFunc, HailFunc,
   HailAckFunc, Preset1Func, Preset2Func, DataXferFunc,
   StatusOnFunc, StatusOffFunc, StatusReqFunc, AllOnFunc,
   ExtPowerUpFunc, VdataFunc, PanicFunc, ArmFunc, DisarmFunc,
   AlertFunc, ClearFunc, SecLightsOnFunc, SecLightsOffFunc,
   TamperFunc, RFXSensorFunc, InvalidFunc
};

/* RFXSensor functions (must align with rfxfunclabel[] in cmd.c) */
enum {
   RFXTempFunc, RFXTemp2Func, RFXHumidFunc, RFXPressFunc, RFXVadFunc,
   RFXVsFunc, RFXLoBatFunc, RFXOtherFunc, RFXPulseFunc, RFXPowerFunc,
   RFXWaterFunc, RFXGasFunc
};
#endif

/* RFXCOM disable modes */
struct rfx_disable_st {
   unsigned long bitmap;
   unsigned short code;
   char *label;
   int minlabel;
};


/* Place to store an alias from user's x10config file */
#define NIDENT 16  /* Max number of ident per alias */
#define NOFFSET 2  /* Max number of offset values per alias */
#define NFUNCLIST 8 /* Max number of functions per alias */
typedef struct {
  int           line_no;
  char          label[NAME_LEN + 1];
  char          housecode;
  unsigned char hcode;
  unsigned int  unitbmap;
  unsigned int  ext0links;
  int           modtype;
  unsigned long vflags;
  unsigned long flags;
  unsigned long xflags;
  unsigned long kflags;
  unsigned long optflags;
  unsigned long optflags2;
  int           tmin;
  int           tmax;
  int           rhmin;
  int           rhmax;
  int           bpmin;
  int           bpmax;
  double        offset[NOFFSET];
  int           onlevel;
  int           maxlevel;
  unsigned char vtype;
  unsigned char subtype;
  unsigned char nident;
  unsigned long ident[NIDENT];
  int           nvar;
  int           storage_index;
  int           storage_units;
  unsigned char funclist[NFUNCLIST];
  unsigned char statusoffset[NFUNCLIST];   
  int           (*xlate_func)();
//  long          timeout;
  long          hb_timeout;
  int           hb_index;
  unsigned int  kaku_keymap[NIDENT];  
  unsigned int  kaku_grpmap[NIDENT];  
} ALIAS;

/* Place to store scene definition from config file */
typedef struct {
  int          line_no;
  int          nparms;
  unsigned int type;
  char         label[SCENE_LEN + 1];
  char         *body;
} SCENE;

/* Place to store script info from config file */
typedef struct {
   int line_no;
   char label[NAME_LEN + 1];
   unsigned char script_option;
   unsigned char done;
   int num_launchers;
   char *cmdline;
} SCRIPT;
 
typedef struct {
    int           stindex;
    unsigned char hcode;
    unsigned int  bmap;
} STATEFLAG;

typedef struct {
    int           vfindex;
    unsigned char hcode;
    unsigned int  ucode;
} VIRTFLAG;

typedef struct {
   int            line_no;
   char           label[NAME_LEN + 1];
   int            scriptnum;
   int            launchernum;
   unsigned int   type;
   unsigned char  matched;
   unsigned char  scanmode;
   unsigned char  oksofar;
   unsigned int   source;
   unsigned int   nosource;
   unsigned char  hcode;
   unsigned long  afuncmap;
   unsigned long  gfuncmap;
   unsigned long  xfuncmap;
   unsigned long  sfuncmap;
   unsigned long  ofuncmap;
   unsigned long  kfuncmap;
   unsigned int   signal;
   unsigned int   module;
   unsigned int   bmaptrig;
   unsigned int   bmaptrigemu;
   unsigned int   chgtrig;
   unsigned long  flags[NUM_FLAG_BANKS];
   unsigned long  notflags[NUM_FLAG_BANKS];
   unsigned long  sflags;
   unsigned long  notsflags;
   unsigned long  vflags;
   unsigned long  notvflags;
   unsigned long  czflags[NUM_COUNTER_BANKS];
   unsigned long  notczflags[NUM_COUNTER_BANKS];
   unsigned long  tzflags[NUM_TIMER_BANKS];
   unsigned long  nottzflags[NUM_TIMER_BANKS];
   unsigned char  bootflag;
   unsigned char  trigemuflag;
   unsigned char  unitstar;
   unsigned char  timer;
   int            sensor;

   unsigned char  genfunc;
   unsigned char  actfunc;
   unsigned char  xfunc;
   unsigned char  level;
   unsigned char  rawdim;
   unsigned int   bmaplaunch;
   unsigned int   actsource;
   int            num_stflags;
   int            num_virtflags;
   STATEFLAG      stlist[32];
   VIRTFLAG       virtlist[32];
} LAUNCHER;
 
/* Place to store info from the user's x10config file */
/* and timezone from the system clock/calendar.       */

typedef struct {
  unsigned char read_flag;        /* Indicates config file has been processed */
  unsigned char mode;             /* HEYU or COMPATIBLE (with ActiveHome (tm)) */
  char          schedfile[PATH_LEN + 1];   /* Pathname of x10 schedule file */
  long int      asif_date;        /* Assumed program date (yyyymmdd) instead of today */
  int           asif_time;        /* Assumed minutes after 00:00 Legal time  */
  int           program_days_in;  /* Number of days to program, beginning today */
  int           program_days;     /* Number of days to program, beginning today */
  int           lat_d;            /* Latitude degrees */
  int           lat_m;            /* Latitude minutes */
  int           lon_d;            /* Longitude degrees */
  int           lon_m;            /* Longitude minutes */
  double        latitude;         /* Latitude degrees as double */
  double        longitude;        /* Longitude degrees as double */
  unsigned char loc_flag;         /* Indicates valid Lat and Lon */
  unsigned char feb_kluge;        /* Use FEB_KLUGE config option */
  unsigned char dawn_option;      /* Use First, Average, Median, etc.*/
  unsigned char dusk_option;      /* Use First, Average, Median, etc.*/
  int           sunmode;          /* Definition of Dawn/Dusk */
  int           sunmode_offset;   /* Custom Dawn/Dusk angle offset */
  int           dawn_substitute;  /* For days when there's no dawn */
  int           dusk_substitute;  /* For days when there's no dusk */
  int           min_dawn;         /* Lower bound on dawn */
  int           max_dawn;         /* Upper bound on dawn */
  int           min_dusk;         /* Lower bound on dusk */
  int           max_dusk;         /* Upper bound on dusk */
  unsigned char combine_events;   /* Similar timed events should be combined */
  unsigned char compress_macros;  /* Merge unit codes in uploaded macro commands */
  unsigned char repl_delay;       /* Flag: Replace delayed macros */
  unsigned char display_offset;   /* Display offsets in report file */
  char          tty[PATH_LEN + 1];   /* Serial port to use */
  char          suffix[PATH_LEN + 1]; /* Suffix for file locks */
  char          ttyaux[PATH_LEN + 1]; /* Auxiliary input serial port */
#ifdef HAVE_FEATURE_RFXLAN
  char          auxhost[NAME_LEN + 1]; /* Auxiliary input network host address or name */
  char          auxport[NAME_LEN + 1]; /* Auxiliary input network port number or name */
#endif
  char          suffixaux[PATH_LEN + 1]; /* Suffix for aux file lock */
  char          ttyrfxmit[PATH_LEN + 1]; /* RFXmitter serial port */
  unsigned char rfxmit_freq;      /* RFXmitter frequency */
  char          housecode;        /* Base housecode */
  char          force_addr;       /* Send an address byte for all commands */
  unsigned int  device_type;      /* Identify non-CM11A interfaces */
  unsigned int  auxdev;           /* Auxilliary device type */
  unsigned char newformat;        /* NEWFORMAT flag */
  unsigned char checkfiles;       /* Write .check, .bin, and .hex files? */
  long int      tzone;            /* Timezone */
  unsigned char isdst;            /* Indicates Daylight Time is in effect */
                                  /*   at some time during the year. (Don't */
                                  /*   confuse with CALEND.isdst)           */
  int           dstminutes;       /* Minutes to add to standard time to get DST  */
  int           alias_size;       /* Size of array of ALIAS structures */
  ALIAS         *aliasp;          /* Pointer to array of ALIAS structures */
  SCENE         *scenep;          /* Pointer to array of SCENE structures */
  SCRIPT        *scriptp;         /* Pointer to array of SCRIPT structures */
  LAUNCHER      *launcherp;       /* Pointer to array of LAUNCHER structures */
  int           max_pparms;       /* Maximum number of positional parameters */
  int           trigger_tag;      /* Tag trigger events. */
  unsigned int  rcs_temperature;  /* Housecode bitmap for translation via RCS table */
  int           xref_append;      /* Append helper events to x10macroxref entries */
  int           ack_hails;        /* Send hail_ack when a hail is received */
  int           macterm;          /* Terminate each macro */
  int           status_timeout;   /* Loop count for status timeout */
  int           spf_timeout;      /* Loop count for special func timeout */
  int           reserved_timers;  /* Number of timers to hold in reserve */
  int           disp_exp_mac;     /* Display commands in macro in monitor */
  int           auto_chain;       /* Use chained macros for events *WIP* */
  int           module_types;     /* Customize behavior per module type */
  int           launch_mode;      /* Launch on signal or module characteristic */
  int           function_mode;    /* Launch on actual or generic function types */
  unsigned int  launch_source;    /* Signal sources allowed to launch a script */
  unsigned char res_overlap;      /* Resolve overlaps separately or combined */
  int           default_module;   /* Default module type */
  char          script_shell[PATH_LEN + 1]; /* Default shell for scripts */
  char          logfile[PATH_LEN + 1]; /* Log file for state engine output */
  unsigned char logcommon;        /* Use common log file */
  unsigned char disp_subdir;      /* Display subdir 0-9 in log file and monitor */
  unsigned char script_mode;      /* Use heyuhelper vs general scripts */
  int           isdark_offset;    /* Minutes after Dusk and before Dawn */
  char          env_alias_prefix[8]; /* Prefix for alias env variables */
  unsigned char fix_5a;           /* Modify 5A checksum if possible */
  unsigned char autofetch;        /* Rewrite state file at every state cmd */
  unsigned char pfail_update;     /* Set clock after power restored */
  int           cm11_post_delay;  /* Delay between direct commands */
  int           cm17a_bit_delay;  /* Intra-bit delay for CM17A commands (us) */
  int           rf_burst_spacing; /* Time between successive CM17A RF bursts */
  int           rf_timer_tweak;   /* Correct for millisleep low resolution */
  int           rf_post_delay;    /* Post-delay for normal CM17A commands (ms) */
  int           rf_farb_delay;    /* Post delay for farb CM17A command (ms) */
  int           rf_farw_delay;    /* Post delay for farw CM17A command (ms) */
  unsigned char disp_rf_xmit;     /* Display CM17A transmissions in monitor */
  unsigned char def_rf_bursts;    /* Nominal CM17A RF bursts */
  unsigned char rf_bursts[12];    /* CM17A RF bursts for each CM11A function */
  unsigned long timer_loopcount;  /* 1 second countdown base for timing loops */
  unsigned char restrict_dims;    /* Restrict dims/brights to range 1-22 */
  unsigned char state_format;     /* New or old (heyuhelper-style) state format */
  char          *pfail_script;    /* Script command line run by relay after power fail */
  unsigned char start_engine;     /* Start engine automatically or manually */
  unsigned char rf_noswitch;      /* Suppress switch in TM751 and RR501 Xceivers */
  unsigned int  respool_perms;    /* Permissions for re-creating SPOOLDIR */
  unsigned long spool_max;        /* Spool file size when rewind is attempted. */
  unsigned char check_RI_line;    /* Check RI serial line before sending command. */
  unsigned char ring_ctrl;        /* Run with Ring Indicator line disabled */
  int           send_retries;     /* Number of times to try sending a command */
  unsigned char script_ctrl;      /* Enable/Disable launching of scripts */
  unsigned int  transceive;       /* Aux transceive options */
  unsigned int  rfforward;        /* Aux RF forward options */
  unsigned char trans_dim;        /* Transceived dim level */
  unsigned char state_ctrl;       /* Single or Bitmap for state commands */
  int           heyu_umask;       /* umask for creating all Heyu files */
  int           log_umask;        /* umask for creating the Heyu log file */
  unsigned char rf_funcmask;      /* Mask for RF functions to be transceived */
  int           aux_repcounts[4]; /* Aux mincount, repcount, maxcount, floodrep */
  int           aux_mincount_rfx; /* Mincount for RFXSensor with RFXCOM receiver. */
  unsigned char hide_unchanged;   /* Hide periodic sensor check-in reports */
  unsigned char hide_unchanged_inactive; /*  Override above "Hide" for Inactive reports */
  unsigned char show_change;      /* (Debug) append CHG|NOCHG to monitor display */
  unsigned char disp_rf_noise;    /* Display Aux RF Noise reception */
  long          arm_max_delay;    /* Delay time for Armed Max */
  unsigned char disp_raw_rf;      /* Display raw RF characters received by Aux */
  int           arm_logic;        /* Logic function (1-3) to use for arming */
  long          inactive_timeout; /* Time when sensors are considered inactive */
  int           auto_wait;        /* busywait() timeout before each sent command. */
  unsigned char full_bright;      /* Level to achieve full brightness */
  long          engine_poll;      /* Engine/Monitor poll delay (microseconds) */
  char          dmx_tscale;       /* Digimax sensor temperature scale (F, C, K, or R) */
  double        dmx_toffset;      /* Digimax sensor temperature offset */
  unsigned char ore_lobat;        /* Low battery percent */
  char          ore_tscale;       /* Oregon sensor temperature scale (F, C, K, or R) */
  double        ore_toffset;      /* Oregon sensor temperature offset */
  double        ore_bpscale;      /* Oregon sensor barometric pressure unit scale factor */
  double        ore_bpoffset;     /* Oregon sensor barometric pressure offset */
  char          ore_bpunits[NAME_LEN];  /* Oregon sensor barometric pressure unit name */
  double        ore_wgtscale;     /* Oregon sensor weight unit scale factor */
  char          ore_wgtunits[NAME_LEN]; /* Oregon sensor weight unit name */
  double        ore_windscale;     /* Oregon sensor wind speed unit scale factor */
  char          ore_windunits[NAME_LEN]; /* Oregon sensor wind speed unit name */
  int           ore_windsensordir; /* Oregon wind direction offset angle in decidegrees*/
  double        ore_rainratescale;     /* Oregon rain rate scale factor */
  char          ore_rainrateunits[NAME_LEN]; /* Oregon rain rate units */
  double        ore_raintotscale;     /* Oregon total rain scale factor */
  char          ore_raintotunits[NAME_LEN]; /* Oregon total rain units */
  unsigned char ore_winddir_mode; /* Display Points, Angle, or Both */
  char          rfx_tscale;       /* RFXSensor temperature scale (F, C, K, or R) */
  double        rfx_toffset;      /* RFX_Sensor temperature offset */
  double        rfx_vadscale;     /* RFXSensor A/D voltage scale factor */
  double        rfx_vadoffset;    /* RFXSEnsor A/D voltage offset */
  char          rfx_vadunits[NAME_LEN];  /* RFXSensor A/D voltage unit name */
  double        rfx_bpscale;      /* RFXSensor barometric pressure unit scale factor */
  double        rfx_bpoffset;     /* RFXSensor barometric pressure offset */
  char          rfx_bpunits[NAME_LEN];  /* RFXSensor barometric pressure unit name */
  unsigned char rfxcom_dtr_rts;   /* DTR and RTS line state for RFXCOM receiver */
  unsigned char rfxcom_hibaud;    /* Use high baud rate */
  unsigned long rfxcom_enable;    /* Enable bitmap for certain RF protocols */
  unsigned long rfxcom_disable;   /* Disable bitmap for certain RF protocols */
  double        rfx_powerscale;   /* RFXMeter Power meter scale factor */
  char          rfx_powerunits[NAME_LEN]; /* RFXMeter Power (energy) units */
  double        rfx_waterscale;   /* RFXMeter Water meter scale factor */
  char          rfx_waterunits[NAME_LEN]; /* RFXMeter Water meter units */
  double        rfx_gasscale;     /* RFXMeter Gas meter scale factor */
  char          rfx_gasunits[NAME_LEN]; /* RFXMeter Gas meter units */
  double        rfx_pulsescale;   /* RFXMeter Pulse meter scale factor */
  char          rfx_pulseunits[NAME_LEN]; /* RFXMeter Pulse meter units */
  unsigned char lockup_check;     /* Relay checks for port and/or CM11A lockup */
  char          tailpath[PATH_LEN + 1]; /* Pathspec of 'tail' command */
  unsigned char suppress_rfxjam;  /* Suppress jamming signals from older RFXCOM */
  unsigned char display_dmxtemp;  /* Display Digimax Temperature signals */
  unsigned char securid_16 ;      /* Use 16 bit Security ID */
  unsigned long securid_mask;     /* Mask for Security ID */
  unsigned char securid_parity;   /* Check parity for 16 bit security RF */
  unsigned char logdate_year;     /* Include year in log/monitor dates */
  unsigned char disp_ore_all;     /* Display both supported and unsupported Oregon types */
  unsigned char ore_chgbits_t;    /* Change in least bits for temperature changed state */
  unsigned char ore_chgbits_rh;   /* Change in least bits for humidity changed state */
  unsigned char ore_chgbits_bp;   /* Change in least bits for baro pressure changed state */
  unsigned char ore_chgbits_wgt;  /* Change in least bits for weight changed state */
  unsigned int  ore_chgbits_dt;   /* Change in least bits for date/time changed state */
  unsigned int  ore_chgbits_wsp;  /* Change in least bits for wind speed changed state */
  unsigned int  ore_chgbits_wavsp; /* Change in least bits for wind avg speed changed state */
  unsigned int  ore_chgbits_wdir;  /* Change in least bits for wind direction changed state */
  unsigned int  ore_chgbits_rrate; /* Change in least bits for rain rate changed state */
  unsigned int  ore_chgbits_rtot; /* Change in least bits for total rain changed state */
  unsigned int  ore_chgbits_uv;   /* Change in least bit for UV factor changed state */
  unsigned char ore_data_entry;   /* Enter data in Native or Scaled units */
  unsigned char ore_display_chan; /* Display Oregon channel number */
  unsigned char ore_display_batlvl; /* Display Oregon battery level (0-100%) */
  unsigned char ore_display_count;  /* Display raw count from wind and rain sensors */
  unsigned char ore_display_bft;  /* Display Beaufort wind scale */
  unsigned char oreid_16;         /* Use 16 bit Oregon ID */
  unsigned short oreid_mask;      /* Mask for Oregon ID */
  unsigned char logdate_unix;     /* Display log dates as seconds from 1/1/1970 epoch */
  long          inactive_timeout_ore; /* Time when Oregon sensors are considered inactive */
  unsigned char display_sensor_intv;  /* Display elapsed time since previous transmission */
  unsigned char date_format;      /* Date format for entry and display */
  char          date_separator;   /* Separator character for date, e.g., '/', '-', or '.' */
  int           lock_timeout;     /* Timeout for acquiring lock file */
  unsigned char ore_display_fcast; /* Display forecast from Oregon THB sensors */
  int           cm11a_query_delay; /* Delay between receiving 0x5A and sending 0xC3 */
  unsigned char els_number;        /* Number of ElectriSave current probes (1-3) */
  double        els_voltage;       /* Nominal voltage for ElectriSave power calculation */
  unsigned int  els_chgbits_curr;  /* Change in least bits for current changed state */
  unsigned char scanmode;          /* Launcher continue or break */
  unsigned char rfx_inline;        /* Display rfxmeter setup in normal monitor */
  double        owl_voltage;       /* Voltage to use with OWL CM119 */
  double        owl_calib_power;   /* Owl power calibration */
  double        owl_calib_energy;  /* Owl energy calibration */
  unsigned long owl_chgbits_power; /* Change in least bits for power changed state */
  unsigned long owl_chgbits_energy; /* Change in least bits for energu changed state */
  unsigned char owl_display_count;  /* Display raw OWL Power and Energy counts */
  unsigned char arm_remote;        /* Auto or Manual arming from remote signal */
  unsigned char active_change;     /* Active <==> Inactive sets "changed" flag for sensor */
  unsigned char inactive_handling; /* New or old method */
  unsigned char process_xmit;      /* Process xmit signal as if actual */
  unsigned char show_flags_mode;   /* Display flags in old (16 flags) or new format */
  unsigned char rfx_master;        /* RFXCOM Master receiver type */
  unsigned char rfx_slave;         /* RFXCOM Slave receiver type */
  char          launchpath_append[PATH_LEN + 1]; /* Append this to normal search PATH */
  char          launchpath_prefix[PATH_LEN + 1]; /* Prefix this to normal search PATH */
  unsigned char fix_stopstart_error; /* Workaround for Stop time = Start time internal error */
  int           chksum_timeout;    /* Wait for checksum timeout */
  
} CONFIG;


/* Place to store timed events from timers */
typedef struct {
   int            line_no;          /* Line number in input file */
   unsigned char  pos;              /* Start (1) or stop (2) position in input timer. */
   int            link;             /* Index to next tevent in linked list */
   int            plink;            /* Backlink for printing */
   int            chain_len;        /* Length of chain of macros */
   int            timer;            /* Index of input timer */
   int            trig;             /* Index to trigger, if any */
   unsigned char  generation;       /* Parent or Child */
   unsigned char  done;             /* Has been processed. */
   unsigned char  combined;         /* Number of combined tevents */
   unsigned char  cancel;           /* Min/Max Dawn/Dusk cancel flags */
   int            print;            /* Number of headers to be printed */
   unsigned char  dow_bmap;         /* Days of Week bitmap */
   int            sched_beg;        /* Begin date as integer mmdd */
   int            sched_end;        /* End date as integer mmdd */
   unsigned char  spflag;           /* (Reserved) */
   int            spindex;          /* (Reserved) */
   int            *spdata;          /* (Reserved) */
   int            notify;           /* Notify days before expire */
   int            resolv_beg;       /* Resolved day of year to begin */
   int            resolv_end;       /* Resolved day of year to end */
   unsigned int   flag;             /* Clock, Dawn, or Dusk, plus other flags */
   unsigned int   flag2;            /* Auxilliary flag */                       
   int            offset;           /* Minutes from Midnight or Dawn/Dusk */
   unsigned char  delay;            /* Macro delay, 0-240 min */
   int            security;         /* Security event offset */
   unsigned char  ddoptions;        /* Dawn/Dusk option flags */
   int            dawnlt;           /* DAWNLT time */
   int            dawngt;           /* DAWNGT time */
   int            dusklt;           /* DUSKLT time */
   int            duskgt;           /* DUSKGT time */
   int            lostday;          /* Day shifted out of program period */
   int            macro;            /* Index into MACRO array */
   int            *ptr;             /* Pointer used in resolve_times() */
   int            intv;             /* Index into array of subintervals */
   int            nintv;            /* Number of subintervals */
} TEVENT;

/* Place to store info about a timer command */
typedef struct {
   int            line_no;          /* Line number in input file */
   int            line1;            /* Start event original timer */
   int            line2;            /* Stop event original timer */
   unsigned char  pos1;             /* Start event original timer start or stop */
   unsigned char  pos2;             /* Stop event original timer start or stop */
   int            link;             /* Index of next timer in linked list */
   int            tevent_start;     /* Index of ancestor tevent for start */
   int            tevent_stop;      /* Index of ancestor tevent for stop */
   unsigned char  generation;       /* Parent or Child */
   unsigned char  dow_bmap;         /* Days of Week bitmap */
   int            sched_beg;        /* Begin date as integer mmdd */
   int            sched_end;        /* End date as integer mmdd */
   unsigned char  spflag;           /* (Reserved) */
   int            spindex;          /* (Reserved) */
   int            *spdata;          /* (Reserved) */
   int            notify;           /* Notify days before expire. */
   int            resolv_beg;       /* Resolved day of year to begin */
   int            resolv_end;       /* Resolved day of year to end */
   unsigned int   flag_start;       /* Start based on Clock, Dawn, or Dusk */
   unsigned int   flag_stop;        /* Stop based on Clock, Dawn, or Dusk */
   unsigned int   flag_combined;    /* Above flags or'd together */
   int            dawnlt;           /* DAWNLT option - minutes from midnight */
   int            dawngt;           /* DAWNGT option - minutes from midnight */
   int            dusklt;           /* DUSKLT option - minutes from midnight */
   int            duskgt;           /* DUSKGT option - minutes from midnight */
   unsigned char  ddoptions;        /* Flags for above options */
   unsigned char  cancel;           /* Min/Max Dawn/Dusk cancel flags */
   int            offset_start;     /* Minutes from Midnight or Dawn/Dusk */
   int            offset_stop;      /* Minutes from Midnight or Dawn/Dusk */
   unsigned char  delay_start;      /* Macro delay, 0-240 min */
   unsigned char  delay_stop;       /* Macro delay, 0-240 min */
   int            security_start;   /* Security mode flag for start time */
   int            security_stop;    /* Security mode flag for stop time */
   int            macro_start;      /* Index into MACRO array */
   int            macro_stop;       /* Index into MACRO array */
   int            error_start;      /* Max Dawn/Dusk error in start time */
   int            error_stop;       /* Max Dawn/Dusk error in stop time */
   int            *ptr_start;       /* Pointer used in resolve_sun_times() */
   int            *ptr_stop;        /* Pointer used in resolve_sun_times() */
   int            num_ptr;          /* Number of above pointers actually used */
   int            intv;             /* Index into array of subintervals */
   int            nintv;            /* Number of subintervals */
   int            memloc;           /* Offset of timer in Cm11a memory image */
} TIMER;

/* Place to store info about a trigger command */
typedef struct {
   int           line_no;          /* Line number in input file */
   unsigned char housecode;        /* x10 code for housecode */
   unsigned char unitcode;         /* x10 code for unit code */
   unsigned char command;          /* Command: ON = 1, OFF = 0 */
   int           macro;            /* Index into MACRO array */
   unsigned char tag;              /* Sequence number 1-7 for triggers to above macro */
   int           memloc;           /* Offset of trigger in Cm11a memory image */
} TRIGGER;

/* Place to store info about a macro */
typedef struct {
   int           line_no;          /* Line number in input file */
   int           link;             /* Index of next macro in linked list */
   int           rlink;            /* Index of previous macro in linked list */
   char          label[MACRO_LEN + 1]; /* Name of macro             */
   unsigned char refer;            /* Indicates parsers which reference the macro */
   int           trig;             /* Trigger index, if any */
   unsigned char use;              /* Use macro in current program */
   unsigned char flag;             /* Macro flags */
   unsigned char modflag;          /* Macro modification flags */
   unsigned char delay;            /* Delay time in seconds 0-240 */
   int           offset;           /* Offset of macro in CM11a memory image */
   int           element;          /* Index into list of macro elements. */
   int           nelem;            /* Number of elements. */
   int           total;            /* Total size of element code (bytes) */
   unsigned char isnull;           /* Indicates if this is the null macro */
} MACRO;

/* Place to store calendar information */
typedef struct {
  int           year;        /* year */
  int           month;       /* month (1-12) */
  int           mday;        /* day of month (1-31) */
  int           yday;        /* day of year (0-365) */
  int           create_day;  /* day of year (0-365) */
  int           minutes;     /* minutes from 0:00 hours Legal time */
  int           day_zero;    /* day of year corresponding to CM11a day 0 */
  long          jan1day;     /* Day number on 1 Jan counted from 1 Jan 1970 */
  long          today;       /* Day number today counted from 1 Jan 1970 */
  unsigned char isdst;       /* Indicates daylight time is in effect this day */
                             /* (Don't confuse with CONFIG.isdst)             */
  unsigned char valid;       /* Indicates the above info has been filled in */
  unsigned char asif_flag;   /* 0 indicates today's date; 1 indicates noon on */
                             /*  ASIF_DATE in configuration file. */
} CALEND;

/* Structure used by heyu_getopt() */
struct opt_st {
   char *configp;
   char *schedp;
   char *subp;
   char dispsub[NAME_LEN];
   int  verbose;
   int  linesync;
};

/* Structure used as argument to pass data by xlate_func() */
struct xlate_vdata_st {
   unsigned char  vdata;
   unsigned char  hcode;
   unsigned char  ucode;
   unsigned long  optflags;
   unsigned long  optflags2;
   unsigned long  ident;
   int            nident;
   unsigned long *identp;
   unsigned char  func;
   unsigned char  xfunc;
   unsigned long  vflags;
   unsigned char  hibyte;
   unsigned char  lobyte;
   int            trig;
}; 
   
/* Contains info written to or read from the x10record file */
struct record_info {
   unsigned char isready;
   long int      dayset;
   int           yday;
   int           day_zero;
   long int      tzone;
   unsigned int  flags;
   int           dstminutes;
   int           program_days;
};


/* Used by function write_macroxref() */
struct macindx {
   int  index;
   int  offset;
};

/* Used by function display_events() */
struct ev_s {
   int tevent;
   int timer;
   int line;
   int pos;
   int tpos;
   int beg;
   int flag;
};

#ifndef min
#define min( a, b )  ((a) < (b) ? (a) : (b))
#endif

#ifndef max
#define max( a, b )  ((a) > (b) ? (a) : (b))
#endif

#ifndef abs
#define abs( a )   ((a) < 0 ? -(a) : (a))
#endif

extern CONFIG *configp;
extern const unsigned int source_type[];
extern const char *const source_name[];

int code2unit ( unsigned char );
int bmap2wday ( unsigned char );
int isleapyear ( int ); 
int get_dst_info ( int );
int isdst_test ( int, int );
int parse_latitude ( char * );
int parse_longitude ( char * );
int parse_config ( FILE *, unsigned char );
int get_alias ( ALIAS *, char *, char *, unsigned int * );
int alias_lookup ( char *, char *, unsigned int * );
int alias_lookup_index ( char, unsigned int, unsigned char );
int add_alias ( ALIAS **, char *, int, char, char *, int );
int parse_time_token ( char *, int * );
int spawn_child_tevent ( TEVENT **, int );
int spawn_child_timer ( TIMER **, int );
int timer_index ( TIMER ** );
int tevent_index ( TEVENT ** );
int update_current_timer_generation ( void );
int update_current_tevent_generation ( void );
int update_current_timer_generation_anyway ( void );
int trigger_index ( TRIGGER ** );
int macro_element_index ( unsigned char **, int );
int macro_index ( MACRO **, char *, unsigned char );
int intv_index ( int **, unsigned int * );
int parse_sched ( FILE *, TIMER **, TRIGGER **, MACRO **, unsigned char ** );
int parse_timer ( TIMER **, MACRO **, char * );
int parse_trigger ( TRIGGER **, MACRO **, char * );
int parse_macro ( MACRO **, unsigned char **, char * );
int is_compatible ( TEVENT *, TEVENT * );
int reconstruct_timers ( TEVENT *, TIMER ** );
int iter_mgr ( int, long *, long, int * );
int set_suntime ( int *, int, int, unsigned char, int * );
int resolve_sun_times ( TIMER **, CALEND *, int, int *, int * );
int get_freespace ( int, TIMER *, TRIGGER *, MACRO * );
int write_image_bin ( char *, unsigned char * );
int write_image_hex ( char *, unsigned char * );
int write_record_file ( char *, CALEND * );
int read_record_file ( void );
int get_upload_expire ( void ); 
int compmac ( struct macindx *, struct macindx * );
int write_macroxref ( char *, MACRO *, unsigned char *, int );
int process_data ( int );
int crontest ( void );
int write_sun_table ( int, int, int, int, int );
int comp_events ( struct ev_s *, struct ev_s * );
int display_events ( FILE *, TIMER *, TEVENT *, MACRO *, CALEND * );
int display_timers ( FILE *, TIMER *, TEVENT *, MACRO * );
int display_macros ( FILE *, unsigned char, MACRO *, unsigned char * );
int final_report ( char *, CALEND *, TIMER *, TEVENT *, 
                        TRIGGER *, MACRO *, unsigned char *, int );
int c_upload ( int, char ** );
int c_utility ( int argc, char ** );
int is_tevent_similar ( int, int, TEVENT * );
int compress_elements ( unsigned char *, int *, int * );
int verify_tevent_links ( TEVENT * );
int verify_timer_links ( TIMER * );
int macro_element_length ( unsigned char );

void tp ( int );
void bcx ( char *, int, int ); 
void dump_tevents_raw ( FILE *, TEVENT * );
void dump_tevents ( FILE *, TEVENT * );
void dump_timer_generation ( FILE *, TIMER *, int );
void display_sys_calendar ( void );
void calendar_today ( CALEND * );
void advance_calendar ( CALEND *, int );
void save_timer_config ( TIMER * );
void save_tevent_config ( TEVENT * );
void save_state ( TIMER *, TEVENT * );
void restore_timer_config ( TIMER * );
void restore_tevent_config ( TEVENT * );
void restore_state ( TIMER *, TEVENT * );
void increment_tevent_generation ( TEVENT *, int );
void increment_timer_generation ( TIMER *, int );
void resolve_dates ( TEVENT **, CALEND *, int );
void security_adjust_legal_old ( TEVENT * );
void security_adjust_legal ( TEVENT **, MACRO **, unsigned char ** );
void split_timers ( TIMER *, TEVENT ** );
void associate_tevent_triggers ( TEVENT *, MACRO * );
void identify_macros_in_use ( MACRO *, TEVENT * );
void resolve_tevent_overlap ( TEVENT ** );
void create_memory_image_high ( unsigned char *, TIMER *, TRIGGER *, MACRO *, unsigned char * );
void create_memory_image_low ( unsigned char *, TIMER *, TRIGGER *, MACRO *, unsigned char * );
void store_record_info ( CALEND * );
void remove_record_file ( void );
void display_cm11a_status ( int );
void display_status_message ( int );
int get_configuration ( unsigned char );
void yday2date ( long, int, int *, int *, int *, int * );
void find_heyu_path ( void );
void combine_similar_tevents ( TEVENT **, MACRO **, unsigned char ** );
void compress_macros ( MACRO *, unsigned char * );
void fix_february ( int, int, int, int *, int * );
void display_syn( void );
long day_count ( int, int, int, int );
long int daycount2JD ( long int ); 
char *strtrim ( char * );
char *strlower ( char * );
char *strupper ( char * );
char *strncpy2 ( char *, char *, int );
char *get_token ( char *, char **, char *, int );
char code2hc ( unsigned char );
char *bmap2dow ( unsigned char );
char *bmap2ascdow ( unsigned char );
char *bmap2units ( unsigned int );
char *bmap2asc ( unsigned int, char *);
char *bmap2asc2 ( unsigned int, unsigned int, char *);
char *bmap2rasc ( unsigned int, char *);
char *legal_time_string ( void );
char *pathspec ( char * );
char *unique_macro_name ( int *, int, MACRO *, char * ) ;
char flag_def ( unsigned int );
unsigned char unit2code ( int );
unsigned char dow2bmap ( char * );
unsigned char rev_low_nybble ( unsigned char ); 
unsigned char rev_byte_bits ( unsigned char ); 
unsigned char lrotbmap ( unsigned char );
unsigned char rrotbmap ( unsigned char );
unsigned char single_bmap_unit( unsigned int );
unsigned char checksum( unsigned char *, int );
unsigned int units2bmap ( char * );
unsigned int flags2bmap ( char * );
unsigned long flags2longmap ( char * );
int lookup_scene ( SCENE *, char * );
void free_scenes ( SCENE ** );
void free_scripts ( SCRIPT ** );
void free_launchers ( LAUNCHER ** );
void free_aliases ( ALIAS ** );
int tokenize2 ( char *, char *, int *, char ** );
int tokenize ( char *, char *, int *, char *** );
int is_admin_cmd ( char * );
int is_direct_cmd ( char * );
int is_heyu_cmd ( char * );
int verify_scene ( char * );
int macro_command( int, char **, int *, int *, unsigned char * );
void clear_error_message( void );
void store_error_message ( char * );
void add_error_prefix ( char * );
void add_error_suffix ( char * );
char *error_message ( void );
unsigned char hc2code ( char );
int max_parms ( int, char ** );
int lookup_macro ( int, char *, int * );
int lookup_alias_mult ( char, unsigned int, int * );
char *lookup_label_mult ( char, unsigned int, int * );
char *lookup_label ( char, unsigned int );
char *alias_rev_lookup_mult ( char, unsigned int, int * );
char *alias_rev_lookup ( char, unsigned int );
int parse_units ( char *, unsigned int * );
void replace_dummy_parms ( int, char ***, char ** );
int image_chksum ( unsigned char * );
int macro_dupe ( MACRO **, int, unsigned char **, char * );
unsigned int parse_addr ( char *, char *, unsigned int * );
int add_scene ( SCENE **, char *, int, char *, unsigned int );
void get_std_timezone ( void );
int parse_config_tail ( char *, unsigned char );
void display_config_overrides ( FILE * );
int finalize_config ( unsigned char );
int environment_config( void );
void millisleep ( long );
void microsleep ( long );
void write_x10state_file ( void );
void x10state_update_sticky_addr ( unsigned char );
void x10state_update_addr ( unsigned char, int * );
void x10state_update_func ( unsigned char *, int * );
void x10state_update_ext3func ( unsigned char *, int * );
void x10state_update_ext0func ( unsigned char *, int * );
void x10state_update_extotherfunc ( unsigned char *, int * );
int x10state_update_virtual ( unsigned char *, int, int * );
void x10state_init_all ( void );
void x10state_init_others ( void );
void update_flags ( unsigned char * );
void update_flags_32 ( unsigned char * );
int  send_clear_statusflags ( unsigned char, unsigned int );
void clear_statusflags ( unsigned char, unsigned int );
void x10state_init_old ( unsigned char );
void x10state_init ( unsigned char, unsigned int );
void x10state_show ( unsigned char );
void x10state_update_bitmap ( unsigned char, unsigned int );
int load_image ( int * );
int loadcheck_image ( int );
void display_trigger( char *, unsigned int, unsigned char );
int expand_macro( char *, unsigned int, int );
char *translate_sent ( unsigned char *, int, int * );
char *translate_virtual ( unsigned char *, int, unsigned char *, int * );
char *translate_long_virtual ( unsigned char *, unsigned char *, int * );
void x10state_reset_all_triggers ( void );
void x10state_reset_all_modmasks ( void );
int parse_launch ( char * );
int launch_scripts ( int *);
void send_sptty_x10state_command ( int, unsigned char, unsigned char );
void send_x10state_command ( unsigned char, unsigned char );
void set_module_masks ( ALIAS * );
int lookup_module_type ( char * );
int (*module_xlate_func(int))();
void finalize_launch ( void );
char *lookup_module_name ( int );
int absdims( int );
unsigned char level2dims( unsigned char, char **, char ** );
void show_hc_dimlevel ( unsigned char );
void show_all_dimlevels ( void );
int add_script( SCRIPT **, LAUNCHER **, int, char * );
int add_launchers ( LAUNCHER **, int, char * );
int send_address ( unsigned char, unsigned int, int );
int send_command ( unsigned char *, int, unsigned int, int );
int heyu_getopt( int, char **, struct opt_st * );
int alias_count ( void );
int launch_heyuhelper ( unsigned char, unsigned int, unsigned char );
void module_attributes ( int, unsigned long *, unsigned long *, unsigned long *, unsigned long *, int * );
int add_module_options ( ALIAS *, int, char **, int );
char *display_module_options ( int );
long int systime_now ( void );
int dawndusk_today ( long *, long * );
int unlock_state_engine( void );
void free_all_arrays ( CONFIG * );
int display_x10state_message( char * );
int c_cm10a_init( int, char ** );
int invalidate_for_cm10a ( void );
void wait_next_tick ( void );
void send_virtual_data    ( unsigned char, unsigned char, unsigned char, unsigned char, unsigned char, unsigned char, unsigned char );
int send_virtual_aux_data ( unsigned char, unsigned char, unsigned char, unsigned char, unsigned char, unsigned char, unsigned char );
int send_virtual_cmd_data ( unsigned char, unsigned char, unsigned char, unsigned char, unsigned char, unsigned char, unsigned char );
int reread_config ( void );
int check_for_engine ( void );
int check_for_aux ( void );
int alias_rev_index ( char, unsigned int, unsigned char, unsigned long );
void read_minimal_config ( unsigned char, unsigned int );
void set_user_timer_countdown ( int, long );
void display_settimer_setting ( int, long );
void create_lock_suffix ( char *, char * );
long parse_hhmmss ( char *, int );
int  busywait ( int );
char *translate_settimer_message( unsigned char * );
void reset_user_timers( void );
int  set_global_dawndusk ( time_t );
int  update_global_nightdark_flags ( time_t );
int  engine_local_setup ( int );
void midnight_tick ( time_t );
void hour_tick ( time_t );
void minute_tick ( time_t );
void second_tick ( time_t, long );
void set_global_tomorrow ( time_t );
int  is_tomorrow ( time_t );
unsigned long list2linmap ( char *, int, int );
char *linmap2list( unsigned long );
int decode_rfxsensor_data ( ALIAS *, int, unsigned int *, unsigned int *, double *, double *, double *, double *);
int rfxsensor_count ( void );
void set_macro_timestamp ( time_t );
time_t get_macro_timestamp ( void );
void create_rfxpower_panels( void );
int check_dir_rw ( char *, char * );
void gendate ( char *, time_t, unsigned char, unsigned char );
int warn_zone_faults ( FILE *, char * );
double hilo2dbl ( unsigned long, unsigned long );
int is_unmatched_flags ( LAUNCHER * );
void setup_countdown_timers ( void );
int update_activity_states ( unsigned char, unsigned int, unsigned char );
int proc_type_std(unsigned char hcode, unsigned char ucode,
		  unsigned char fcode);
char *datstrf(void);
void configure_rf_tables(void);
int direct_command(int, char **, int);

/*
 * Process signals received from sources other than the Heyu spool file.
 * buf: a buffer with a signal in the Heyu internal (spool file) format,
 * len: length, in bytes, of the signal representation in the buffer,
 * src: source of the signal, can be any value that heyu_parent can take.
 */
int process_received(unsigned char *buf, int len, int src);

#define for_each_unit(bitmap, unit) \
		for (unit = 0; unit < 16; unit++) \
			if ((bitmap) & (1 << unit))

#define alt_parent_call(function, parent, ...)	(\
{						 \
	int save_parent = heyu_parent;		 \
	int ret;				 \
						 \
	heyu_parent = parent;			 \
	ret = function(__VA_ARGS__);		 \
	heyu_parent = save_parent;		 \
						 \
	ret;					 \
})

/*
 * Find a single unit alias matching the module type and ID.
 * Returns an index to the alias table entry, or -1 if not found.
 * Additionally, stores the retrieved alias unit code under ucodep.
 */
int id2index(unsigned char type, unsigned long id, unsigned char *ucodep);
