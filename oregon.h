/*----------------------------------------------------------------------------+
 |                                                                            |
 |                  Oregon Sensor Support for HEYU                            |
 |                Copyright 2008 Charles W. Sullivan                          |
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

/* Formats for Oregon data */
#define  FMT_ORET       "%.1f"  /* Temperature */
#define  FMT_OREBP      "%.4g"  /* Barometric Pressure */
#define  FMT_OREWGT     "%.1f"  /* Weight */
#define  FMT_OREWSP     "%.1f"  /* Wind Speed */
#define  FMT_OREWDIR    "%.1f"  /* Wind Direction */
#define  FMT_ORERTOT    "%.2f"  /* Total Rainfall */
#define  FMT_ORERRATE   "%.2f"  /* Rainfall Rate */

/* Scale parameters for OWL CM119 */
#define OWLPSC  1.0       /* Power */
#define OWLESC  0.044843  /* Energy */
#define OWLVREF 230.0     /* Ref Voltage */

/* Don't change anything below */
#define ORE_VALID   0x00000001u
#define ORE_WGTNUM  0x00000006u  /* Shared */
#define ORE_FCAST   0x0000000eu  /* Shared */
#define ORE_NEGTEMP 0x00000020u
#define ORE_BATLVL  0x00000040u
#define ORE_LOBAT   0x00000080u
#define ORE_DATAMSK 0x00ffff00u
#define ORE_CHANMSK 0xf0000000u
#define ORE_BATMSK  0x0f000000u

#define ORE_DATASHFT   8
#define ORE_BATSHFT   24
#define ORE_CHANSHFT  28
#define ORE_WGTNUMSHFT 1
#define ORE_FCASTSHFT  1

/* Oregon subtypes */
enum {
   OreTemp1, OreTemp2, OreTemp3,
   OreTH1, OreTH2, OreTH3, OreTH4, OreTH5, OreTH6,
   OreTHB1, OreTHB2,
   OreTemu, OreTHemu, OreTHBemu,
   OreRain1, OreRain2, OreRain3,
   OreWind1, OreWind2, OreWind3,
   OreUV1, OreUV2,
   OreDT1, OreElec1, OreElec2,
   OreWeight1, OreWeight2
};

unsigned char oretype ( unsigned char *, unsigned char *, unsigned char *, unsigned char *, unsigned long *, int * );
char *translate_gen_longdata ( unsigned char *, unsigned char *, int * );
char *translate_ore_emu ( unsigned char *, unsigned char *, int * );
char *translate_oregon( unsigned char *, unsigned char *, int * );
int is_ore_ignored ( unsigned int );
static inline long long c_oredt(unsigned long *data_storage)
{
	return *(time_t *)(data_storage + 1);
}
