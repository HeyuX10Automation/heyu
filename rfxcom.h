/*----------------------------------------------------------------------------+
 |                                                                            |
 |              RFXSensor and RFXMeter Support for HEYU                       |
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


/* Formats for displayed values of RFXSensor variables */
#define FMT_RFXT      "%.3f"  /* Temperature */
#define FMT_RFXRH     "%.2f"  /* Relative Humidity */
#define FMT_RFXBP     "%.4g"  /* Barometric Pressure */
#define FMT_RFXVAD    "%.2f"  /* A/D Voltage */

/* Formats for displayed values of RFXMeter variables */
#define FMT_RFXPOWER  "%.3f"  /* Power meter */
#define FMT_RFXWATER  "%.0f"  /* Water meter */
#define FMT_RFXGAS    "%.2f"  /* Gas meter */
#define FMT_RFXPULSE  "%.0f"  /* General pulse meter */

char *translate_kaku(unsigned char *, unsigned char *, int *);
char *translate_visonic(unsigned char *, unsigned char *, int *);
char *translate_rfxtype_message(unsigned char *);
char *translate_rfxsensor_ident(unsigned char *);

