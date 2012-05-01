/*----------------------------------------------------------------------------+
 |                                                                            |
 |                   HEYU Timing Loop Support                                 |
 |               Copyright 2005 Charles W. Sullivan                           |
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
#include <unistd.h>
#include <signal.h>

/*----------------------------------------------------------------------------+
 | Timing loop calibration for one second.                                    |
 +----------------------------------------------------------------------------*/
static volatile int alarmflag;
static volatile unsigned long countup;
extern void wait_next_tick(void);

void loop_alarm_isr ( int signo )
{
   signo = signo;
   alarmflag = 0;
   return;
}

unsigned long loop_calibrate ( void )
{
   
   alarmflag = 1;
   countup = 0;
   signal(SIGALRM, loop_alarm_isr);
   wait_next_tick();
   alarm(1);
   while ( alarmflag && ++countup );
   alarm(0);

   return countup;
}   
