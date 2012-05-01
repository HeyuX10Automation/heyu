/*----------------------------------------------------------------------------+
 | Sunrise/Sunset functions for Heyu.                                         |
 |                                                                            |
 | The solar computation functions below were programmed by Charles W.        |
 | Sullivan utilizing the techniques and astronomical constants published by  |
 | Roger W. Sinnott in the August 1994 issue of Sky & Telescope Magazine,     |
 | Page 84.                                                                   |
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

/* Return codes for function suntimes() */
#define NORMAL_SUN    0
#define DOWN_ALL_DAY  1
#define UP_ALL_DAY    2
#define NO_SUNRISE    4
#define NO_SUNSET     8

#define FMT_PORTRAIT  0
#define FMT_LANDSCAPE 1

#define TIMEMODE_STANDARD   0
#define TIMEMODE_CIVIL      1

int suntimes ( double, double, long int, long int, int, int,
              int *, int *, double *, double * );
long int greg2jd ( int, int, int );
int display_sun_table (FILE *, int, long, int, int, int, int, int, int, int);
int display_sun_table_wide (FILE *, int, long, int, int, int, int, int, int, int);
int local_dawndusk ( time_t, time_t *, time_t * );


