/*----------------------------------------------------------------------------+
 |                                                                            |
 |                    Enhanced HEYU Functionality                             |
 |                                                                            |
 |         Enhancements copyright 2004-2008 Charles W. Sullivan               |
 |                      All Rights Reserved                                   |
 |                                                                            |
 | Changes for use with CM11A copyright 1996, 1997 Daniel B. Suthers,         |
 | Pleasanton Ca, 94588 USA                                                   |
 |                                                                            |
 | Copyright 1986 by Larry Campbell, 73 Concord Street, Maynard MA 01754 USA  |
 | (maynard!campbell).                                                        |
 |                                                                            |
 | John Chmielewski (tesla!jlc until 9/1/86, then rogue!jlc) assisted         |
 | by doing the System V port and adding some nice features.  Thanks!         |
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
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <ctype.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#include <time.h>
#include <signal.h>
#include "x10.h"
#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#endif
#include "process.h"
#include "x10state.h"
#include "oregon.h"

/* msf - added for glibc/rh 5.0 */
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

extern int sptty;

extern CONFIG config;
extern CONFIG *configp;

extern struct x10global_st x10global;
extern x10hcode_t *x10state;
 
extern int i_am_monitor, i_am_state, i_am_rfxmeter;
extern int verbose;

extern int xread(int, unsigned char *, int, int);

extern int is_ring ( void );
extern int reread_config ( void );

unsigned char bootflag = R_NOTATSTART;
	
/*----------------------------------------------------------------------------+
 | Determine the maximum number of bytes for a received function available in |
 | the buffer.  Arguments lenbyte and fmapbyte are the first two bytes from   |
 | the buffer, representing the length of the buffer and a bitmapped function |
 | indicator.  The return value is determined by the lesser the length of the |
 | buffer or the distance to the next indicated function from the function    |
 | indicator byte.                                                            |
 +----------------------------------------------------------------------------*/
int max_funclen ( unsigned char lenbyte, unsigned char fmapbyte, int pos )
{
   int k, maxlen;

   if ( !(fmapbyte & (1 << pos)) || lenbyte > 9 )
      return 0;
   
   maxlen = 1;
   for ( k = pos + 1; k < (int)(lenbyte - 1); k++ ) {
      if ( !(fmapbyte & (1 << k)) )
         maxlen++;
      else
         break;
   }
   return maxlen;
}


/* 
 *  The CM11A sends data back to the computer whenever it sees a command
 *  come in over the AC buss.  This should (theoretically) allow the compuer
 *  to track the status of all modules.  Upon startup, this program should
 *  check for a poll before anything else.
 *
 *  Check for a poll (0x5a) from the CM11A, If we get one within a
 *  second, we should send 0xc3 to tell it that we are ready to read
 *  it's output.
 *
 *  If the showdata flag is set, we print.  Otherwise we just eat the output.
 */


extern char *datstrf(void);
unsigned int signal_source;
extern FILE *fdsout;
extern FILE *fdserr;
extern FILE *fprfxo;
extern FILE *fprfxe;

int check4poll( int showdata, int timeout )
{
    static int lastunit;
    static char lasthc = '_';
    int temperat, fdata;
    int n, i, j;
    int to_read;
    char hc;
    unsigned char predim, xcmd, xtype, xdata, subunit;
    int unit;
    int macro_report;
    char *func;
    int funcbits;
    static int wasflag = 0;
    unsigned char buf[128];
    char outbuf[256];
    unsigned char snochange;
    extern char *b2s();
    off_t f_offset;
    unsigned int mac_addr, bitmap;
    extern void acknowledge_hail();
    extern int special_func;
    extern int display_expanded_macro();
    int ichksum;
    int identify_sent(unsigned char *, int, unsigned char *);
    char *translate_other(unsigned char *, int, unsigned char *);
    extern char *translate_sent(unsigned char *, int, int *);
    extern char *translate_rf_sent(unsigned char *, int *);
#if 0
    extern int find_powerfail_launcher( unsigned char );
    extern int find_rfflood_launcher( void );
    extern int find_lockup_launcher( void );
#endif
    extern int find_powerfail_scripts ( unsigned char );
    extern int find_rfflood_scripts ( void );
    extern int find_lockup_scripts ( void );

    extern int set_globsec_flags( unsigned char );
    extern char *display_armed_status ( void );
    extern int clear_tamper_flags ( void );
    extern int engine_local_setup ( int );
    extern char *translate_rfxtype_message ( unsigned char * );
    extern char *translate_rfxsensor_ident ( unsigned char * );
    extern char *display_variable_aux_data ( unsigned char * );
    extern int set_counter ( unsigned char, unsigned short, unsigned char );
    extern char *translate_counter_action ( unsigned char * );
    extern char *translate_kaku ( unsigned char *, unsigned char *, int * );
    extern char *translate_visonic ( unsigned char *, unsigned char *, int * );
    extern char *display_binbuffer ( unsigned char * );
    int launch_script_cmd ( unsigned char * );
    int launchp = -1;
    char maclabel[MACRO_LEN + 1];
    int macfound;
    unsigned int squelch;
    unsigned char hcode;

    static int chksum_alert = -1;
    extern char *funclabel[18];
    static unsigned char newbuf[8], hexaddr = 0; 
    static char *send_prefix = "sndc";
    static unsigned char waitflag = 0;

    int sent_type;
    unsigned char tag;
    int maxlen;
    unsigned char level;
    static unsigned char defer_dim;
    char minibuf[32];
    char *transp;
    mode_t oldumask;
    char *sp;


  
    static char *rcs_status[] = {
       "System_mode = OFF", "System_mode = HEAT", "System_mode = COOL", "System_mode = AUTO",
       "Fan = ON", "Fan = OFF", "Setback = ON", "Setback = OFF", "Temp Change", "Setpoint Change",
       "Outside Temp", "Heat Setpoint", "Cooling Setpoint",
    };
    static unsigned int source_type[] = {SNDC, SNDS, SNDP, SNDA, RCVA};
    static char *source_name[] = {"sndc", "snds", "sndp", "snda", "rcva"};

    if ( i_am_state ) {
       /* Redirect output to the Heyu log file */
       oldumask = umask((mode_t)configp->log_umask);
       fdsout = freopen(configp->logfile, "a", stdout);
       fdserr = freopen(configp->logfile, "a", stderr);
       umask(oldumask);
    }
 
    /* Note: When invoked as the normal 'heyu monitor' (i_am_monitor), */
    /* fdsout and fdserr will have been set to stdout and stderr.      */
    /* But when invoked as 'heyu monitor rfxmeter' (i_am_rfxmeter),    */
    /* they will have been redirected to /dev/null so the only output  */
    /* to that xterm will be RFXMeter data via file pointer fprfxo.    */

    unit = -1;
    hc = '\0';
    func =  "" ;

    if ( timeout == 0 )	 {	/* only do a read if there is data */
	if( sptty < 0 ) {
	    return 0;
        }
        f_offset = lseek(sptty, 0, SEEK_CUR);
	if ( f_offset == lseek(sptty, 0, SEEK_END) ) {
	    return 0;
        }
	lseek(sptty, f_offset, SEEK_SET);
	timeout = 1;
    }


    n = xread(sptty, buf, 1, timeout);
    if ( n != 0 ) {
	/* Three 0xff bytes in a row indicate that I sent a transmission to
	 * the CM11, I.E a command.  The byte following the 3 0xFF bytes
	 * indicates how long the sent command was.  We need to suck these
	 * off the stack if we are monitoring.
	 */
	while ( buf[0] == 0xff )  {  /* CM11A may be responding to a transmission  */
	    if ( ++wasflag == 3 ) {
		wasflag = 0;
		n = xread(sptty, buf, 1, timeout);	/* length of xmit */
		if( n != 1 ) {
		     return(0);
                }
                if ( buf[0] == 0xff && chksum_alert == 0xff ) {
		   /*
		    * We have received four 0xff bytes in a row,
		    * looks like the first one was the checksum expected,
		    * so drop one of them and loop one more time.
		    */
		   chksum_alert = -1;
		   wasflag = 2;
		   continue;
		}
		n = buf[0];

                if ( n < 127 ) {
		   unsigned char chksum;

                   n = xread(sptty, buf, n, timeout);

                   /* Identify the type of sent command from the leading */
                   /* byte and the length of the buffer.                 */
                   sent_type = identify_sent(buf, n, &chksum);

                   if ( sent_type == SENT_STCMD ) {
                      /* A command for the monitor/state process */
		      if ( buf[1] == ST_SOURCE ) {
                         signal_source = source_type[buf[2]];
                         send_prefix = source_name[buf[2]];
		      }
                      else if ( buf[1] == ST_TESTPOINT && i_am_monitor ) {
                         fprintf(fdsout, "%s Testpoint %d\n", datstrf(), buf[2]);
                      } 
                      else if ( buf[1] == ST_LAUNCH && i_am_state ) {
                         configp->script_ctrl = buf[2];
                         fprintf(fdsout, "%s Scripts %s\n", datstrf(), (buf[2] == ENABLE ? "enabled" : "disabled"));
                      }
                      else if ( buf[1] == ST_INIT_ALL && i_am_state ) {
                         x10state_init_all();
                         write_x10state_file();
                      }
                      else if ( buf[1] == ST_INIT && i_am_state ) {
                         x10state_init_old(buf[2]);
                         write_x10state_file();
                      }
                      else if ( buf[1] == ST_INIT_OTHERS && i_am_state ) {
                         x10state_init_others();
                         write_x10state_file();
                      }
                      else if ( buf[1] == ST_WRITE && i_am_state ) {
                         write_x10state_file();
                      }
                      else if ( buf[1] == ST_EXIT && i_am_state ) {
                         write_x10state_file();
                         unlock_state_engine();
                         exit(0);
                      }
                      else if ( buf[1] == ST_RESETRF && (i_am_state || i_am_monitor) ) {
			 fprintf(fdsout, "%s Reset CM17A\n", datstrf());
                      }
		      else if ( buf[1] == ST_BUSYWAIT && (i_am_state || i_am_monitor) ) {
			 waitflag = buf[2];
                         if ( configp->auto_wait == 0 ) {
			    if ( waitflag > 0 )
			       fprintf(fdsout, "%s Wait macro completion.\n", datstrf());
			    else
			       fprintf(fdsout, "%s Wait timeout.\n", datstrf());
                         }
		      }
                      else if ( buf[1] == ST_CHKSUM ) {
                         chksum_alert = buf[2];
                      }
                      else if ( buf[1] == ST_REWIND ) {
                         for ( j = 0; j < 10; j++ ) {
                            if ( lseek(sptty, (off_t)0, SEEK_END) <= (off_t)(SPOOLFILE_ABSMIN/2) )
                               break;
                            sleep(1);
                         }
                         fprintf(fdsout, "%s Spoolfile exceeded max size and was rewound.\n", datstrf());
                      }
                      else if ( buf[1] == ST_SHOWBYTE && (i_am_state || i_am_monitor) ) {
                         fprintf(fdsout, "%s Byte value = 0x%02x\n", datstrf(), buf[2]);
                      }
                      else if ( buf[1] == ST_RESTART && (i_am_state || i_am_monitor || i_am_rfxmeter) ) {
                         reread_config();
                         if ( i_am_state ) {
                            syslog(LOG_ERR, "engine reconfiguration-\n");
                            engine_local_setup(E_RESTART);
                            fprintf(fdsout, "%s Engine reconfiguration\n", datstrf());
                            fflush(fdsout);
//                            syslog(LOG_ERR, "engine reconfiguration-\n");
                         }
                         else if ( i_am_rfxmeter ) {
                            fprintf(fprfxo, "RFXMeter monitor reconfiguration\n");
                            fflush(fprfxo);
                         }
                         else {
                            fprintf(fdsout, "%s Monitor reconfiguration\n", datstrf());
                            fflush(fdsout);
                         }
                      }
                      else if ( buf[1] == ST_SECFLAGS && (i_am_state || i_am_monitor)) {
                         if ( buf[2] != 0 ) {
                            warn_zone_faults(fdsout, datstrf());
                         }
                         set_globsec_flags(buf[2]);
                         write_x10state_file();
                         fprintf(fdsout, "%s %s\n", datstrf(), display_armed_status());
                         fflush(fdsout);
                      }
                      else if ( buf[1] == ST_CLRTIMERS && (i_am_state || i_am_monitor)) {
                         reset_user_timers();
                         write_x10state_file();
                         fprintf(fdsout, "%s Reset timers 1-16\n", datstrf());
                         fflush(fdsout);
                      }
                      else if ( buf[1] == ST_CLRTAMPER && (i_am_state || i_am_monitor)) {
                         clear_tamper_flags();
                         write_x10state_file();
                         fprintf(fdsout, "%s Clear tamper flags\n", datstrf());
                         fflush(fdsout);
                      }
                   } 
                   else if ( sent_type == SENT_WRMI ) {
                      /* WRMI - Ignore */
                   }
                   else if ( sent_type == SENT_ADDR ) {
                      /* An address */
                      if ( *(transp = translate_sent(buf, n, &launchp)) )
                         fprintf(fdsout, "%s %s %s\n", datstrf(), send_prefix, transp);
		      chksum_alert = chksum;
                      if ( launchp >= 0 && i_am_state) {
                         launch_scripts(&launchp);
                      }
                   }
                   else if ( sent_type == SENT_FUNC ) {
                      /* Standard function */
                      if ( *(transp = translate_sent(buf, n, &launchp)) )
                         fprintf(fdsout, "%s %s %s\n", datstrf(), send_prefix, transp);
		      chksum_alert = chksum;
                      if ( launchp >= 0 && i_am_state) {
                         launch_scripts(&launchp);
                      }
                   }
                   else if ( sent_type == SENT_EXTFUNC ) {
                      /* Extended function */
                      if ( *(transp = translate_sent(buf, n, &launchp)) )
                         fprintf(fdsout,"%s %s %s\n", datstrf(), send_prefix, transp);
		      chksum_alert = chksum;
                      if ( launchp >= 0 && i_am_state) {
                         launch_scripts(&launchp);
                      }
                   }
                   else if ( sent_type == SENT_RF ) {
                      /* CM17A command */
                      fprintf(fdsout,"%s %s %s\n", datstrf(), "xmtf", translate_rf_sent(buf, &launchp));
                      if ( launchp >= 0 && i_am_state ) {
                         launch_scripts(&launchp);
                      }
                   }
                   else if ( sent_type == SENT_FLAGS && i_am_state ) {
                      update_flags(buf);
                      write_x10state_file();
                   }
                   else if ( sent_type == SENT_FLAGS32 && i_am_state ) {
                      update_flags_32(buf);
                      write_x10state_file();
                   }
                   else if ( sent_type == SENT_VDATA ) {
                      strcpy(outbuf, translate_virtual(buf, 9 /*8*/, &snochange, &launchp));
                      if ( *outbuf && !(snochange && config.hide_unchanged == YES && launchp < 0) )
                         fprintf(fdsout,"%s %s %s\n", datstrf(), send_prefix, outbuf);
                      if ( i_am_state && launchp >= 0 ) {
                         launch_scripts(&launchp);
                      }
                   }
                   else if ( sent_type == SENT_GENLONG ) {
                      strcpy(outbuf, translate_gen_longdata(buf, &snochange, &launchp));
                      if ( *outbuf && !(snochange && config.hide_unchanged == YES && launchp < 0) )
                         fprintf(fdsout,"%s %s %s\n", datstrf(), send_prefix, outbuf);
                      if ( i_am_state && launchp >= 0 ) {
                         launch_scripts(&launchp);
                      }
                   }
                   else if ( sent_type == SENT_ORE_EMU ) {
                      strcpy(outbuf, translate_ore_emu(buf, &snochange, &launchp));
                      if ( *outbuf && !(snochange && config.hide_unchanged == YES && launchp < 0) )
                         fprintf(fdsout,"%s %s %s\n", datstrf(), send_prefix, outbuf);
                      if ( i_am_state && launchp >= 0 ) {
                         launch_scripts(&launchp);
                      }
                   }
                   else if ( sent_type == SENT_KAKU ) {
                      strcpy(outbuf, translate_kaku(buf, &snochange, &launchp));
                      if ( *outbuf && !(snochange && config.hide_unchanged == YES && launchp < 0) )
                         fprintf(fdsout,"%s", outbuf);
                      if ( i_am_state && launchp >= 0 ) {
                         launch_scripts(&launchp);
                      }
                   }
                   else if ( sent_type == SENT_VISONIC ) {
                      strcpy(outbuf, translate_visonic(buf, &snochange, &launchp));
                      if ( *outbuf && !(snochange && config.hide_unchanged == YES && launchp < 0) )
                         fprintf(fdsout,"%s", outbuf);
                      if ( i_am_state && launchp >= 0 ) {
                         launch_scripts(&launchp);
                      }
                   }
                   else if ( sent_type == SENT_LONGVDATA ) {
                      if ( i_am_rfxmeter ) {
                         /* This and only this data gets sent to the special */
                         /* 'heyu monitor rfxmeter' window */
                         strcpy(outbuf, translate_long_virtual(buf, &snochange, &launchp) );
                         if ( (sp = strchr(outbuf, '\n')) != NULL )
                            *sp = '\0';
                         fprintf(fprfxo, "%s\n", outbuf);
                         fflush(fprfxo);
                      }
                      else {
                         /* This and all other data are not sent to the rfxmeter window */
                         strcpy(outbuf, translate_long_virtual(buf, &snochange, &launchp) );
                         if ( *outbuf && !(snochange && config.hide_unchanged == YES && launchp < 0) ) {
                            if ( (sp = strchr(outbuf, '\n')) != NULL ) {
                               *sp = '\0';
                               fprintf(fdsout,"%s %s %s\n", datstrf(), send_prefix, outbuf);
                               fprintf(fdsout,"%s %s\n", datstrf(), sp + 1);
                            }
                            else {
                               fprintf(fdsout,"%s %s %s\n", datstrf(), send_prefix, outbuf);
                            }
                         }
                         if ( i_am_state && launchp >= 0 ) {
                            launch_scripts(&launchp);
                         }
                      }
                   }
                   else if ( sent_type == SENT_COUNTER && (i_am_state || i_am_monitor)) {
                      set_counter(buf[2] | (buf[3] << 8), buf[4] | (buf[5] << 8), buf[6]);
                      write_x10state_file();
                      fprintf(fdsout, "%s %s\n", datstrf(), translate_counter_action(buf));
                      fflush(fdsout);
                   }
                   else if ( sent_type == SENT_CLRSTATUS && i_am_state ) {
                      clear_statusflags(buf[2], (buf[3] << 8 | buf[4]));
                      write_x10state_file();
                   }
		   else if ( sent_type == SENT_MESSAGE && (i_am_monitor || i_am_state) ) {
		      fprintf(fdsout, "%s %s\n", datstrf(), buf + 3);
	           }	      		   
                   else if ( sent_type == SENT_PFAIL && (i_am_state || i_am_monitor) ) {
                      bootflag = buf[2];
                      if ( bootflag & R_ATSTART ) 
                         fprintf(fdsout, "%s Powerfail signal received at startup.\n", datstrf());
                      else
                         fprintf(fdsout, "%s Powerfail signal received.\n", datstrf());
                      if ( i_am_state && (launchp = find_powerfail_scripts(bootflag)) >= 0 )
                         launch_scripts(&launchp);
                   }
                   else if ( sent_type == SENT_RFFLOOD && (i_am_state || i_am_monitor) ) {
                      fprintf(fdsout, "%s RF_Flood signal received.\n", datstrf());
                      if ( i_am_state && (launchp = find_rfflood_scripts()) >= 0 )
                         launch_scripts(&launchp);
                   }
                   else if ( sent_type == SENT_LOCKUP && (i_am_state || i_am_monitor) ) {
                      fprintf(fdsout, "%s Interface lockup signal received, reason %02x.\n", datstrf(), buf[2]);
                      if ( i_am_state && (launchp = find_lockup_scripts()) >= 0 )
                         launch_scripts(&launchp);
                   }
                   else if ( sent_type == SENT_SETTIMER && (i_am_state || i_am_monitor) ) {
                      fprintf(fdsout, "%s %s\n", datstrf(), translate_settimer_message(buf));
//                      if ( i_am_state )
//                         write_x10state_file();
                   }
                   else if ( sent_type == SENT_RFXTYPE && (i_am_state || i_am_monitor) ) {
                      fprintf(fdsout, "%s %s %s\n", datstrf(), send_prefix, translate_rfxtype_message(buf));
                   } 
                   else if ( sent_type == SENT_RFXSERIAL && (i_am_state || i_am_monitor) ) {
                      fprintf(fdsout, "%s %s %s\n", datstrf(), send_prefix, translate_rfxsensor_ident(buf));
                   } 
                   else if ( sent_type == SENT_RFVARIABLE && (i_am_state || i_am_monitor) ) {
                      fprintf(fdsout, "%s %s %s\n", datstrf(), send_prefix, display_variable_aux_data(buf));
                   }
                   else if ( sent_type == SENT_SCRIPT && (i_am_state) ) {
                       launch_script_cmd(buf);
                   }
                   else if ( sent_type == SENT_INITSTATE && (i_am_state) ) {
                        x10state_init(buf[2], buf[3] | (buf[4] << 8));
                   }
                   else if ( sent_type == SENT_SHOWBUFFER && i_am_monitor ) {
                      fprintf(fdsout, "%s %s\n", datstrf(), display_binbuffer(buf + 2));
                   }
                   else if ( sent_type == SENT_OTHER ) {
                      /* Other command */
                      if ( *(transp = translate_other(buf, n, &chksum)) )
                         fprintf(fdsout, "%s %s\n", datstrf(), transp);
		      chksum_alert = chksum;
                      launchp = -1;
                   }

                }
                else {
                   n = xread(sptty, buf, n - 127, timeout);
                }
                
	        fflush(fdsout);
	        return(0);
	    }
	    else
		n = xread(sptty, buf, 1, timeout);

	    if( n != 1 ) {
	        return(0);
            }

	}   /* end of while */


	/* print out the saved 0xff from the buffer */
	if ( buf[0] != 0xff && wasflag > 0 )  {
	    if (chksum_alert == 0xff) {
	    	/* a single 0xff was apparently a checksum */
		chksum_alert = -1;
		wasflag--;
	    }
	    n = buf[0];
	    for(i = 0; i < wasflag;i++)
		buf[i] = 0xff;
	    buf[i] = n;
	    n = wasflag + 1;
	    wasflag = 0;
	}
	    
        if ( buf[0] == chksum_alert ) {
	    /* this is the checksum expected, just drop it */
            chksum_alert = -1;
	    return 0;
        }

	if (chksum_alert >= 0) {
	    /* 
	     * Instead of the checksum expected, we've received something else.
	     * Don't expect that checksum to appear any longer. Either the
	     * interface didn't respond with a checksum, busy with an incomming
	     * transmission or a macro, or we are completely out of sync.
	     */
	    chksum_alert = -1;
	}

	macro_report = 0;
        if ( buf[0] == 0x5a )  {
            /* CM11A has polling info for me */
	    /* The xread is executed a second time on failure because the
	       dim commands may be tieing up the CM11.
	    */
	    n = xread(sptty, buf, 1, 2);  /* get the buffer size */
	    if ( n == 0)
		n = xread(sptty, buf, 1, 2);  /* get the buffer size */

	    if ( n > 0 )  {
                /* We have a byte count */
	        to_read = buf[0];	/* number of bytes to read */
		if ( to_read == 0x5a ) {
                    /* Darn.  Another polling indicator */
		    timeout = 2;
		    return ( check4poll(showdata, timeout) );
		}
		else if ( to_read == 0x5b )  { /* Macro report coming in */
		    to_read = 2;
		    macro_report = 1;
		}

		if ( to_read > (int)sizeof(buf) ) {
		    if( verbose )
			fprintf(fdserr, "Polling read size exceeds buffer");
		    return(-1);
		}

		n = xread(sptty, buf, to_read , timeout);
		if ( n != to_read ) {
		    fprintf(fdsout, "Poll only got %d of %d bytes\n",
		            n, to_read);
		    fflush(fdsout);
		    return 0;
		}

                if ( showdata > 0 && (verbose != 0) )
		    fprintf(fdsout, "I received a poll %d bytes long.\n", n);

		if ( macro_report == 1 )  { /* CM11A is reporting a macro execution.*/
		    if ( showdata ) {
                       mac_addr = ((buf[0] & 0x07u) << 8) + buf[1];
                       /* Get macro label (or "Unknown") and saved image checksum */ 
                       macfound = lookup_macro(mac_addr, maclabel, &ichksum);

                       if ( buf[0] & 0x70u )  {
                          /* Macro was executed by a Trigger */
                          if ( macfound == 1 && ichksum != -1 && loadcheck_image(ichksum) ) {

                             /* Determine and display the inferred trigger. */
                             /* Launch a script if warranted.               */
                             signal_source = RCVT;
                             tag = (buf[0] & 0x70u) >> 4;
                             display_trigger(datstrf(), mac_addr, tag);

                             /* Announce the execution of the macro */
                             fprintf(fdsout, "%s Trigger executed macro : %s, address 0x%03x\n",
                                   datstrf(), maclabel, mac_addr);

                             /* Decipher and display macro elements from image file. */
                             /* Launch script(s) when warranted.                     */
                             signal_source = SNDT;
                             expand_macro(datstrf(), mac_addr, TRIGGER_EXEC);
                          }
                          else {
                             fprintf(fdsout, "%s Trigger executed macro : %s, address 0x%03x\n",
                                    datstrf(), maclabel, mac_addr);
                          }
                       }
                       else {
                          /* Macro was executed by a Timer */
                          set_macro_timestamp(time(NULL));
                          if ( macfound == 1 && ichksum != -1 && loadcheck_image(ichksum) ) {
                             fprintf(fdsout, "%s Timer executed macro   : %s, address 0x%03x\n",
                                       datstrf(), maclabel, mac_addr);
                             /* Decipher and display macro elements from image file. */
                             /* Launch script(s) when warranted.                     */
                             signal_source = SNDM;
                             expand_macro(datstrf(), mac_addr, TIMER_EXEC);
                          }
                          else {
                             fprintf(fdsout, "%s Timer executed macro   : %s, address 0x%03x\n",
                                       datstrf(), maclabel, mac_addr);
                          }
                       }
		    }
		}
		else {
                    signal_source = RCVI;
#if 0   			
                    if ( showdata ) {
                        for ( i = 1; i < to_read; i++)
                            fprintf(fdsout, " %02x", buf[i]);
                        fprintf(fdsout, "\n");
                    }
#endif
                       
			    
		    for ( i = 1; i < to_read; i++) {
                        xcmd = xdata = subunit = predim = level = 0;

			if ( defer_dim &&
                            ( (funcbits = (newbuf[0] & 0x0Fu)) == DIM || funcbits == BRIGHT) ) {
                            /* Finish handling a deferred Dim or Bright function */
                            newbuf[1] = buf[i];
                            signal_source = RCVI;
                            x10state_update_func(newbuf, &launchp);
                            hc = code2hc((newbuf[0] & 0xF0u) >> 4);
                            level = buf[i];
                            defer_dim = 0;
                            newbuf[0] = 0;                            
			    if( showdata ) {
                               fprintf(fdsout, "%s rcvi func %12s : hc %c %s %%%02.0f [%d]\n",
                                  datstrf(), funclabel[funcbits], hc, 
                                  ((funcbits == DIM) ? "dim" : "bright"),
                                  100.*(float)level/210., level);
                            }
			}
			else if ( (buf[0] & (0x01 << (i-1))) != 0) {
                            /* A function has been received */
                            signal_source = RCVI;

                            /* Determine the maximum number of bytes    */
                            /* available in the buffer for the function */
                            maxlen = max_funclen(to_read, buf[0], (i-1));

			    hc = code2hc( ((buf[i] & 0xF0u) >> 4) );
			    funcbits = buf[i] & 0x0Fu;
                            func = funclabel[buf[i] & 0x0Fu];

			    if ( funcbits == DIM || funcbits == BRIGHT ) {
                               if ( maxlen > 1 ) {
                                  /* We have the dim level - handle it here */
                                  x10state_update_func(buf + i, &launchp);
                                  level = buf[i+1];
                                  defer_dim = 0;
                                  i++;
                               }
			       else {
                                  /* Otherwise, defer until we've got the dim level */
                                  newbuf[0] = buf[i];
                                  defer_dim = 1;
                               }
                            }
			    else if ( funcbits == PRESET1 || funcbits == PRESET2)  {
			        func = "Preset";
                                x10state_update_func(buf + i, &launchp);

                                predim = rev_low_nybble((buf[i] & 0xF0u) >> 4) + 1;

			        if ( funcbits == PRESET2 )
			           predim += 16;

                                fdata = predim;
			    }
                            else if ( funcbits == EXTCODE )  {
                                if ( maxlen >= 4 )  {
                                    /* Buffer looks OK. (Sometimes it gets  */
                                    /* garbled when ext code functions are  */
                                    /* received by a firmware 1 interface.) */
                                    unit = code2unit(buf[i+1] & 0x0fu);
				    subunit = (buf[i+1] & 0xf0u) >> 4;
                                    bitmap = 1 << (buf[i+1] & 0x0fu);
                                    xdata = buf[i+2];
                                    xcmd = buf[i+3];
                                    xtype = (xcmd & 0xf0u) >> 4;
                                    if ( xtype == 3 ) {
                                       x10state_update_ext3func(buf + i, &launchp);
                                    }
#ifdef HAVE_FEATURE_EXT0
                                    else if ( xtype == 0 ) {
                                       x10state_update_ext0func(buf + i, &launchp);
                                    }
#endif
                                    else {
                                       x10state_update_extotherfunc(buf + i, &launchp);
                                    }
                                    i += 3;
                                }
                                else  {
                                    /* Garbled buffer - dump it */
                                    unit = 0;
                                    bitmap = 0;
                                    xdata = 0;
                                    xcmd = 0xff;
                                    i = to_read;
                                }
                            }
                            else {
                                x10state_update_func(buf + i, &launchp);
                            }
                                
				
			    if ( showdata )  {

                                hcode = hc2code(hc);
                                squelch = x10state[hcode].squelch;
                                if ( squelch != 0 && (x10state[hcode].lastactive & squelch) == 0 ) {
                                   fprintf(fdsout, "%s rcvi addr unit     %3d : hu %c%-2d (%s)\n",
                                      datstrf(), code2unit(single_bmap_unit(squelch)),
                                      hc, code2unit(single_bmap_unit(squelch)), lookup_label(hc, squelch) );
                                   x10state[hcode].squelch = squelch = 0;
                                }
                                else if ( squelch != 0 && ((squelch & x10state[hcode].state[ChgState]) || launchp >= 0) ) {
                                   fprintf(fdsout, "%s rcvi addr unit     %3d : hu %c%-2d (%s)\n",
                                      datstrf(), code2unit(single_bmap_unit(squelch)),
                                      hc, code2unit(single_bmap_unit(squelch)), lookup_label(hc, squelch) );
                                   x10state[hcode].squelch = squelch = 0;
                                }



				if ( (funcbits == DIM || funcbits == BRIGHT) && squelch == 0 ) {
                                    if ( !defer_dim )
                                        fprintf(fdsout, "%s rcvi func %12s : hc %c %s %%%02.0f [%d]\n",
                                           datstrf(), funclabel[funcbits], toupper((int)hc), 
                                           ((funcbits == DIM) ? "dim" : "bright"),
                                           100.*(float)level/210., level);
                                }
				else if ( (funcbits == PRESET1 || funcbits == PRESET2) && squelch == 0 ) {
				    fprintf(fdsout, "%s rcvi func %12s : level %d\n",
					    datstrf(), func, predim);
                                    if ( i_am_monitor &&
                                         configp->rcs_temperature & (1 << hc2code(lasthc)) ) {
                                       /* Special funcs for RCS compatible thermometers */
                                       if ( lastunit > 10 )  {
                                          temperat = -60 + (predim - 1) + 32 * (lastunit - 11);
					  sprintf(minibuf, "Temperature = %d", temperat);
                                          fprintf(fdsout, "%s %-22s : hu %c0  (%s)\n",
                                            datstrf(), minibuf, toupper((int)lasthc), lookup_label(lasthc, 0));
                                       }
                                       else if ( lastunit == 6 && predim < sizeof(rcs_status) / sizeof(*rcs_status) + 1 ) {
                                          fprintf(fdsout, "%s %-22s : hu %c0  (%s)\n",
                                            datstrf(), rcs_status[predim - 1], toupper((int)lasthc),
                                                                   lookup_label(lasthc, 0));
                                       }
                                    }
				}                                
                                else if ( funcbits == EXTCODE && squelch == 0 )  {
			            char tmp[10], stmp[16];
			            sprintf(tmp, "%d", unit);
                                    bitmap = 1 << unit2code(unit);
				    if ( subunit == 0 )
				       stmp[0] = '\0';
				    else
				       sprintf(stmp, " subunit %02d", subunit);

                                    if ( xcmd == 0x31 ) {
                                       if ( xdata & 0xc0u ) {
                                          fprintf(fdsout,
                                            "%s rcvi func      xPreset : hu %c%-2d%s level %d ramp %d (%s)\n",
                                            datstrf(), hc, unit, stmp, xdata & 0x3f, (xdata & 0xc0) >> 6,
                                            lookup_label(hc, bitmap));
                                       }
                                       else {
                                          fprintf(fdsout,
                                            "%s rcvi func      xPreset : hu %c%-2d%s level %d (%s)\n",
                                            datstrf(), hc, unit, stmp, xdata, lookup_label(hc, bitmap));
                                       }
                                    }
				    else if ( xcmd == 0x3b )
                                       fprintf(fdsout, "%s rcvi func      xConfig : hc %c mode=%d\n",
                                           datstrf(), hc, xdata);
                                    else if ( xcmd == 0x33 )
                                       fprintf(fdsout, "%s rcvi func       xAllOn : hc %c\n",
                                           datstrf(), hc);
                                    else if ( xcmd == 0x34 )
                                       fprintf(fdsout, "%s rcvi func      xAllOff : hc %c\n",
                                           datstrf(), hc);
                                    else if ( xcmd == 0x37 ) {
                                       if ( (xdata & 0x30u) == 0 ) {
                                          fprintf(fdsout, "%s rcvi func   xStatusReq : hu %c%-2d%s (%s)\n",
                                              datstrf(), hc, unit, stmp, lookup_label(hc, bitmap));
                                       }
                                       else if ( (xdata & 0x30u) == 0x10 ) {
                                          fprintf(fdsout, "%s rcvi func     xPowerUp : hu %c%-2d%s (%s)\n",
                                              datstrf(), hc, unit, stmp, lookup_label(hc, bitmap));
                                       }
                                       else if ( (xdata & 0x30u) == 0x20 ) {
                                          fprintf(fdsout, "%s rcvi func   xGrpStatus : hu %c%-2d%s group %d (%s)\n",
                                              datstrf(), hc, unit, stmp, (xdata & 0xc0u) >> 6, lookup_label(hc, bitmap));
                                       }
                                       else { 
                                          fprintf(fdsout, "%s rcvi func   xGrpStatus : hu %c%-2d%s group %d.%-2d (%s)\n",
                                              datstrf(), hc, unit, stmp, (xdata & 0xc0u) >> 6, (xdata & 0x0fu) + 1, lookup_label(hc, bitmap));
                                       }
                                    }
                                    else if ( xcmd == 0x38 ) {
                                       if ( xdata & 0x40u )
                                           fprintf(fdsout, "%s rcvi func   xStatusAck : hu %c%-2d%s Switch %-3s %s (%s)\n",
                                               datstrf(), hc, unit, stmp, ((xdata & 0x3fu) ? "On " : "Off"),
                                               ((xdata & 0x80u) ? "LoadOK" : "NoLoad"), 
                                               lookup_label(hc, bitmap));
                                       else
                                           fprintf(fdsout, "%s rcvi func   xStatusAck : hu %c%-2d%s Lamp level %d, %s (%s)\n",
                                               datstrf(), hc, unit, stmp, xdata & 0x3fu, 
                                               ((xdata & 0x80u) ? "BulbOK" : "NoBulb"), 
                                               lookup_label(hc, bitmap));
                                    }
                                    else if ( xcmd == 0x30 ) {
                                       if ( (xdata & 0x20u) == 0 ) {
                                          fprintf(fdsout, "%s rcvi func      xGrpAdd : hu %c%-2d%s group %d (%s)\n",
                                             datstrf(), hc, unit, stmp, (xdata & 0xc0u) >> 6, lookup_label(hc, bitmap));

                                       }
                                       else {
                                          fprintf(fdsout, "%s rcvi func      xGrpAdd : hu %c%-2d%s group %d.%-2d (%s)\n",
                                             datstrf(), hc, unit, stmp, (xdata & 0xc0u) >> 6, (xdata & 0x0fu) + 1, lookup_label(hc, bitmap));
                                       }
                                    }
                                    else if ( xcmd == 0x32 ) {
                                       fprintf(fdsout, "%s rcvi func   xGrpAddLvl : hu %c%-2d%s group %d level %d (%s)\n",
                                          datstrf(), hc, unit, stmp, (xdata & 0xc0u) >> 6, xdata & 0x3fu, lookup_label(hc, bitmap));
                                    }
                                    else if ( xcmd == 0x35 ) {
                                       if ( (xdata & 0x30) == 0 ) {
                                          fprintf(fdsout, "%s rcvi func      xGrpRem : hu %c%-2d%s group(s) %s (%s)\n",
                                             datstrf(), hc, unit, stmp, linmap2list(xdata & 0x0f), lookup_label(hc, bitmap));
                                       }
                                       else {
                                          fprintf(fdsout, "%s rcvi func   xGrpRemAll : hc %c group(s) %s\n",
                                             datstrf(), hc, linmap2list(xdata & 0x0f));

                                       }
                                    }
                                    else if ( xcmd == 0x36 ) {
                                       if ( (xdata & 0x30u) == 0 ) {
                                          fprintf(fdsout, "%s rcvi func     xGrpExec : hc %c group %d\n",
                                             datstrf(), hc, (xdata & 0xc0u) >> 6);
                                       }
                                       else if ( (xdata & 0x30u) == 0x20u ) {
                                          fprintf(fdsout, "%s rcvi func     xGrpExec : hc %c group %d.%-2d\n",
                                             datstrf(), hc, (xdata & 0xc0u) >> 6, (xdata & 0x0fu) + 1);
                                       }
                                       else if ( (xdata & 0x30u) == 0x10u ) {
                                          fprintf(fdsout, "%s rcvi func      xGrpOff : hc %c group %d\n",
                                             datstrf(), hc, (xdata & 0xc0u) >> 6);
                                       }
                                       else {
                                          fprintf(fdsout, "%s rcvi func      xGrpOff : hc %c group %d.%-2d\n",
                                             datstrf(), hc, (xdata & 0xc0u) >> 6, (xdata & 0x0fu) + 1);
                                       }
                                    }
                                    else if ( xcmd == 0x39 ) {
                                       fprintf(fdsout, "%s rcvi func      xGrpAck : hu %c%-2d%s group %d level %d (%s)\n",
                                          datstrf(), hc, unit, stmp, (xdata & 0xc0) >> 6, xdata & 0x3fu, lookup_label(hc, bitmap));
                                    }
                                    else if ( xcmd == 0x3A ) {
                                       fprintf(fdsout, "%s rcvi func     xGrpNack : hu %c%-2d%s group %d (%s)\n",
                                          datstrf(), hc, unit, stmp, (xdata & 0xc0) >> 6, lookup_label(hc, bitmap));
                                    }
                                    else if ( xcmd == 0x3C ) {
                                       if ( (xdata & 0x30u) == 0 ) {
                                          fprintf(fdsout, "%s rcvi func      xGrpDim : hc %c group %d\n",
                                             datstrf(), hc, (xdata & 0xc0u) >> 6);
                                       }
                                       else if ( (xdata & 0x30u) == 0x10 ) {
                                          fprintf(fdsout, "%s rcvi func   xGrpBright : hc %c group %d\n",
                                             datstrf(), hc, (xdata & 0xc0u) >> 6);
                                       }
                                       else if ( (xdata & 0x30u) == 0x20 ) {
                                          fprintf(fdsout, "%s rcvi func      xGrpDim : hc %c group %d.%-2d\n",
                                             datstrf(), hc, (xdata & 0xc0u) >> 6, (xdata & 0x0fu) + 1);
                                       }
                                       else {
                                          fprintf(fdsout, "%s rcvi func   xGrpBright : hc %c group %d.%-2d\n",
                                             datstrf(), hc, (xdata & 0xc0u) >> 6, (xdata & 0x0fu) + 1);
                                       }

                                    }
                                    else if ( xcmd == 0x01 )
                                       fprintf(fdsout, "%s rcvi func    shOpenLim : hu %c%-2d%s level %d (%s)\n",
                                           datstrf(), hc, unit, stmp, xdata, lookup_label(hc, bitmap));
				    else if ( xcmd == 0x03 )
                                       fprintf(fdsout, "%s rcvi func       shOpen : hu %c%-2d%s level %d (%s)\n",
                                           datstrf(), hc, unit, stmp, xdata, lookup_label(hc, bitmap));
				    else if ( xcmd == 0x02 )
                                       fprintf(fdsout, "%s rcvi func     shSetLim : hu %c%-2d%s level %d (%s)\n",
                                           datstrf(), hc, unit, stmp, xdata, lookup_label(hc, bitmap));
                                    else if ( xcmd == 0x04 )
                                       fprintf(fdsout, "%s rcvi func    shOpenAll : hc %c\n",
                                           datstrf(), hc);
                                    else if ( xcmd == 0x0B )
                                       fprintf(fdsout, "%s rcvi func   shCloseAll : hc %c\n",
                                           datstrf(), hc);

                                    else if ( xcmd == 0xff )
                                       fprintf(fdsout, "%s rcvi func      ExtCode : Incomplete xcode in buffer.\n",
                                               datstrf());
                                    else  {
                                       fprintf(fdsout, "%s rcvi func     xFunc %02x : hu %c%-2d%s data=0x%02x (%s)\n",
                                           datstrf(), xcmd, hc, unit, stmp, xdata, lookup_label(hc, bitmap));
                                    }
                                }
                                else {
                                   hcode = hc2code(hc);
                                   squelch = x10state[hcode].squelch;
                                   if ( squelch == 0 ) {
                                      fprintf(fdsout, "%s rcvi func %12s : hc %c\n",
					 datstrf(), func, hc);
                                   }

                                   if ( funcbits == 8 && i_am_state && configp->ack_hails == YES ) {
                                      acknowledge_hail();
                                   }
				}
                                x10state[hcode].squelch = 0;

			    }

                            if ( special_func > 0 ) {
                               if( (funcbits == PRESET1 || funcbits == PRESET2) ) {
                                  if ( lastunit > 10 ) {
                                     temperat = -60 + (predim - 1) + 32 * (lastunit - 11);
                                     fprintf(fdsout, "%s Temperature = %-3d   : hu %c0  (%s)\n",
                                          datstrf(), temperat, lasthc, lookup_label(lasthc, 0));
                                  }
                                  else if ( lastunit == 6 && predim < 9 ) {
                                     fprintf(fdsout, "%s %s : hu %c0  (%s)\n",
                                         datstrf(), rcs_status[predim - 1], toupper((int)lasthc),
                                                                   lookup_label(lasthc, 0));
                                  }
                               }
                            }
                        }
			else {		
                            /* an address byte was received */
                            signal_source = RCVI;
			    hcode = (buf[i] & 0xF0u) >> 4;
                            hc = code2hc(hcode);
			    unit = code2unit( buf[i] & 0x0Fu) ;
	                    hexaddr = buf[i];
                            bitmap = 1 << (buf[i] & 0x0fu);
                            lasthc = hc;
                            lastunit = unit;
                            x10state_update_addr(buf[i], &launchp);
			    x10state_update_sticky_addr(buf[i]);


                            if ( showdata ) {
                               squelch = x10state[hcode].squelch;
                               if ( squelch == 0 ) {
                                  fprintf(fdsout, "%s rcvi addr unit     %3d : hu %c%-2d (%s)\n",
				     datstrf(), unit, hc, unit, lookup_label(hc, bitmap) );
                               }
                               else if ( squelch != 0  && squelch != bitmap ) {
                                  /* Different address - display deferred squelch address and cancel it */
                                  x10state[hcode].squelch = 0;
                                  fprintf(fdsout, "%s rcvi addr unit     %3d : hu %c%-2d (%s)\n",
                                      datstrf(), code2unit(single_bmap_unit(squelch)), hc, code2unit(single_bmap_unit(squelch)), lookup_label(hc, squelch) );
                                  fprintf(fdsout, "%s rcvi addr unit     %3d : hu %c%-2d (%s)\n",
				      datstrf(), unit, hc, unit, lookup_label(hc, bitmap) );
                               }
                            }
			}

			fflush(fdsout);

                        if ( launchp >= 0 && i_am_state) {
                            launch_scripts(&launchp);
                        }
		    }
		    fflush(fdsout);
		}
	    }
	    else {
		if (verbose)
                   fprintf(fdsout, "%s Bytes received = %d; the interface didn't answer a getinfo response.\n", datstrf(), n);
                else
                   fprintf(fdsout, "%s The interface didn't answer a getinfo response.\n", datstrf());
	    }
	}
        else if ( buf[0] == 0xa5 && showdata )  {
            /* Device experienced an AC power interruption */
            if ( configp->device_type & DEV_CM10A )
               fprintf(fdsout, "%s CM10A experienced an AC power interruption. %s",
                    datstrf(), "It requested a restart.\n");
            else
	       fprintf(fdsout, "%s CM11A experienced an AC power interruption. %s",
	            datstrf(), "It requested a time update.\n");
	}
	else if ( buf[0] == 0x5b ) {  
           /* The CM11A is reporting a macro execution */
	   to_read = 2;
	   n = xread(sptty, buf, to_read, timeout);
	   if ( showdata ) {
              mac_addr = ((buf[0] & 0x07u) << 8) + buf[1];
              /* Get macro label (or "Unknown") and saved image checksum */ 
              macfound = lookup_macro(mac_addr, maclabel, &ichksum);

              if ( buf[0] & 0x70u ) {
                 /* The macro was executed by a Trigger */
                 if ( macfound == 1 && ichksum != -1 && loadcheck_image(ichksum) ) {

                    /* Determine and display the inferred trigger. */
                    /* Launch script if warranted.                 */
                    tag = (buf[0] & 0x70u) >> 4;
                    signal_source = RCVT;
                    display_trigger(datstrf(), mac_addr, tag);

                    /* Announce the execution of the macro */
                    fprintf(fdsout, "%s Trigger executed macro : %s, address 0x%03x\n",
                                       datstrf(), maclabel, mac_addr);

                    /* Decipher and display macro elements from image file. */
                    /* Launch script(s) if warranted.                       */
                    signal_source = SNDT;
                    expand_macro(datstrf(), mac_addr, TRIGGER_EXEC);
                 }
                 else {
                    fprintf(fdsout, "%s Trigger executed macro : %s, address 0x%03x\n",
                                       datstrf(), maclabel, mac_addr);
                 }
              }  
              else  {
                 /* The macro was executed by a Timer */
                 if ( macfound == 1 && ichksum != -1 && loadcheck_image(ichksum) ) {

                    /* Announce the execution of the macro */
                    fprintf(fdsout, "%s Timer executed macro   : %s, address 0x%03x\n",
                                 datstrf(), maclabel, mac_addr);

                    /* Decipher and display macro elements from image file */
                    /* and launch script(s) if warranted.                  */
                    signal_source = SNDM;
                    expand_macro(datstrf(), mac_addr, TIMER_EXEC);
                 }
                 else {
                    fprintf(fdsout, "%s Timer executed macro   : %s, address 0x%03x\n",
                                       datstrf(), maclabel, mac_addr);
                 }
              }
           }
	}
	else if ( buf[0] == ((configp->ring_ctrl == DISABLE) ? 0xdb : 0xeb) && waitflag ) {
	   waitflag = 0;
	}
	else if ( !(buf[0] == 0x55 && n == 1) )  {
            /* There's some sort of timing problem with the 0x55 ("ready to  */
            /* receive") signal from the interface after a long bright/dim   */
            /* or extended command is sent.  Ignore it, but alert to other   */
            /* unknown values.                                               */
	    fprintf(fdsout, 
	        "%s Poll received unknown value (%d bytes), leading byte = %0x\n",
		datstrf(), n, buf[0]);
        }

        fflush(fdsout);
    }
    return 0;
}

