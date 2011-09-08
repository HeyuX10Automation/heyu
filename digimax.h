/*----------------------------------------------------------------------------+
 |                                                                            |
 |                    Digimax Support for HEYU                                |
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

/* Bitfields for DigiMax 210 */

#define PARMASK     0x0f0000ff   /* Parity bits (to be discarded) */
#define STATMASK    0x30000000   /* Input Status field */
#define TCURRMASK   0x00ff0000   /* Current Temperature field */
#define TSETPMASK   0x00003f00   /* Temperature Setpoint field */
#define HEATMASK    0x00004000   /* Heat mode bit (0 = heat, 1 = cool) */
#define ONOFFMASK   0x03000000   /* Stored On/Off Status field */
#define UNDEFMASK   0xc0008000   /* Undefined Status bits */
#define VALMASK     0x00000001   /* Valid stored data (temp) bit */
#define SETPFLAG    0x00000002   /* Valid stored setpoint bit */
#define TCURRSHIFT  16
#define TSETPSHIFT   8
#define STATSHIFT   28
#define ONOFFSHIFT  24
#define HEATSHIFT   14

/* Is it necessary to swap On/Off when the DigiMax is operated  */
/* in cool mode (for agreement with On/Off PLC code transmitted */
/* by the base station) ?  YES|NO                               */
#define DMXCOOLSWAP    YES

