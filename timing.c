/*----------------------------------------------------------------------------+
 |                                                                            |
 |                   HEYU Timing Loop Support                                 |
 |               Copyright 2005 Charles W. Sullivan                           |
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

#include <time.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
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
