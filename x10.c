/*
 * Changes for use with CM11A copyright 1996, 1997 Daniel B. Suthers,
 * Pleasanton Ca, 94588 USA
 * E-mail dbs@tanj.com
 */
/*
 * Copyright 1986 by Larry Campbell, 73 Concord Street, Maynard MA 01754 USA
 * (maynard!campbell).
 *
 * John Chmielewski (tesla!jlc until 9/1/86, then rogue!jlc) assisted
 * by doing the System V port and adding some nice features.  Thanks!
 * 
 */

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
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#include <time.h>
#include <signal.h>
#include "x10.h"
#include "version.h"
#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#endif
#include "process.h" 

/* msf - added for glibc/rh 5.0 */
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

extern int tty;
extern int sptty;
extern int usage();
extern int start_relay(),setup_sp_tty(), xread(), unit2int();
extern void error(), quit();
extern int lock_for_write();
extern int munlock();
extern CONFIG config;  /* CWS - Structure with all configuration data */
extern CONFIG *configp;

int x10_housecode;
int verbose;
int i_am_relay = 0;	/* flag to differentiate the relay process */
int port_locked = 0;	/* flag to show that the tty port is locked */
int i_am_monitor = 0;
int i_am_state = 0;
int i_am_aux = 0;
int i_am_rfxmeter = 0;
int heyu_parent = D_CMDLINE;  

FILE *fdsout;
FILE *fdserr;
FILE *fprfxo;
FILE *fprfxe;

void init();
int check4poll();

char heyuprogname[PATH_LEN + 1 + 10];

struct opt_st options;
struct opt_st *optptr = &options;

extern char *wday_name[7];
extern char *month_name[12];


/* Table for mapping units (1-16) or house codes (a-p) to 4 bit nibbles.
 * element 0 equates to unit 1 or code a
 */
unsigned char cm11map[]=
{  06, 016, 02, 012, 01, 011, 05, 015,
   07, 017, 03, 013, 0, 010, 04, 014
};

/* Table for mapping an x10 format bitmap to a unit number.  It's
 * the reverse of the one above.
 * Note that the unit numbers are 1-16, but the array is 0-15.
 */
unsigned char map2cm11[]=
{
    13, 5, 3, 11, 15, 7, 1, 9, 14, 6, 4, 12, 16, 8, 2, 10
};

int timeout = TIMEOUT;


extern int
 c_date(),  c_info(), c_monitor(), c_reset(),  c_setclock(),
 c_erase(),  c_upload(), c_utility(), c_setclockraw(), c_command(),
 is_heyu_cmd(), c_qerase(),
 c_set_status(), c_show1(), c_show2(), c_x10state(), c_ping(), c_busywait(),
 c_catchup(), c_macro(), c_start_engine(), c_logmsg(), c_readclock(),
 c_flagstate(), c_cm10a_ident(), c_turn_rts_off(), c_turn_rts_on(),
 c_trigger(), c_status_state(), c_powerfailtest(),
 c_show_modem_lines(), c_ri_disable(), c_ri_enable(), c_port_line_test(),
 c_start_aux(), c_start(), c_restart(), c_stop(), c_script_ctrl(),
 c_arm(), c_sendarbst(), c_sensorfault(), c_armedstate(), c_settimer(),
 c_sunstate(), c_restore_groups(), c_rfxcmds(), c_timer_times(), c_webhook(),
 c_counter(), c_logtail(), c_modlist(), c_dmxcmds(), c_orecmds(), c_ore_emu(),
 c_launch(), c_rts_pulser(), c_conflist(), c_sec_emu(), c_stateflaglist(),
 c_masklist();

void init ( char * );
int read_config( unsigned char );

extern void create_alerts ( void );
extern int  quick_ports_check ( void );

int c_list(), c_version();

struct cmdentry {
    char *cmd_name;
    int (*cmd_routine) ();
    int lock_needed;
    int internal_cmd;
} cmdtab[] = {
    {"___", c_command,1,1},
    {"date", c_date,1,0} ,
    {"erase", c_erase,1,0},
    {"info", c_info,1,1},
    {"list", c_list,0,1},
    {"monitor", c_monitor,0,1},
    {"cmd", c_command,1,1},
    {"reset", c_reset,1,0},
    {"setclock", c_setclock,1,0},
    {"readclock", c_readclock,1,0},
    {"setclockraw", c_setclockraw, 1,0}, /* undocumented, see source - CWS */
    {"upload", c_upload, 1, 0},  /* (added by CWS) */
    {"utility", c_utility, 0, 1}, /* (added by CWS) */
    {"newbattery", c_set_status, 1, 0}, 
    {"purge", c_set_status, 1, 0},
    {"clear", c_set_status, 1, 0},
    {"reserved", c_set_status, 1, 0}, /* undocumented, see source - CWS */
    {"version", c_version,0, 1},
    {"qerase", c_qerase,1, 0},  /* hidden feature */
    {"enginestate", c_x10state,1, 1},
    {"initstate", c_x10state,1, 1},
    {"initstateold", c_x10state, 1, 1},
    {"initothers", c_x10state,1, 1},
    {"fetchstate", c_x10state,1, 1},
    {"onstate", c_x10state, 1, 1},
    {"offstate", c_x10state, 1, 1},
    {"dimstate", c_x10state, 1, 1},
    {"fullonstate", c_x10state, 1, 1},
    {"chgstate", c_x10state, 1, 1},
    {"modchgstate", c_x10state, 1, 1},
    {"alertstate", c_x10state, 1, 1},
    {"clearstate", c_x10state, 1, 1},
    {"auxalertstate", c_x10state, 1, 1},
    {"auxclearstate", c_x10state, 1, 1},
    {"addrstate", c_x10state, 1, 1},
    {"lobatstate", c_x10state, 1, 1},
    {"inactivestate", c_x10state, 1, 1},
    {"activestate", c_x10state, 1, 1},
    {"activechgstate", c_x10state, 1, 1},
    {"validstate", c_x10state, 1, 1},
    {"tamperstate", c_x10state, 1, 1},
    {"dimlevel", c_x10state, 1, 1},
    {"rawlevel", c_x10state, 1, 1},
    {"memlevel", c_x10state, 1, 1},
    {"rcstemp", c_x10state, 1, 1},
    {"rawmemlevel", c_x10state, 1, 1},
    {"vident", c_x10state, 1, 1},
    {"sincelast", c_x10state, 1, 1},
    {"statestr", c_x10state, 1, 1},
    {"heyu_state", c_x10state, 1, 1},
    {"heyu_vflagstate", c_x10state, 1, 1},
    {"heyu_rawstate", c_x10state, 1, 1},
    {"verbose_rawstate", c_x10state, 1, 1},
    {"xtend_state", c_x10state, 1, 1},
    {"catchup", c_catchup, 1, 0},
    {"macro", c_macro, 1, 0},
    {"trigger", c_trigger, 1, 0},
    {"ping", c_ping, 1, 0},
    {"show", c_show2, 1, 1},
    {"engine", c_start_engine, 1, 1},
    {"logmsg", c_logmsg, 1, 1},
    {"wait", c_busywait, 1, 0},
    {"flagstate", c_flagstate, 1, 1},
    {"statusstate", c_status_state, 1, 1},
    {"spendstate", c_status_state, 1, 1},
    {"cm10a_init", c_cm10a_init, 1, 0},
    {"cm10a_ident", c_cm10a_ident, 1, 0},
    {"turn_rts_off", c_turn_rts_off, 1, 0},    
    {"turn_rts_on", c_turn_rts_on, 1, 0},
    {"show_modem_lines", c_show_modem_lines, 1, 0},
    {"powerfailtest", c_powerfailtest, 1, 1},
    {"ri_disable", c_ri_disable, 1, 0},
    {"ri_enable", c_ri_enable, 1, 0},
    {"port_line_test", c_port_line_test, 1, 0},
    {"rts_pulser", c_rts_pulser, 1, 1},
    {"aux", c_start_aux, 1, 1},
    {"start", c_start, 1, 1},
    {"restart", c_restart, 1, 1},
    {"stop", c_stop,0, 1},
    {"script_ctrl", c_script_ctrl, 1, 1},
    {"_arm", c_arm, 1, 1},
    {"_disarm", c_arm, 1, 1},
    {"sendarbst", c_sendarbst, 1, 0},
    {"sensorfault", c_sensorfault, 1, 1},
    {"armedstate", c_armedstate, 1, 1},
    {"sunstate", c_sunstate, 1, 1},
    {"nightstate", c_sunstate, 1, 1},
    {"darkstate", c_sunstate, 1, 1},
    {"restore_groups", c_restore_groups, 1, 0},
#ifdef HAVE_FEATURE_RFXS
    {"rfxflag_state", c_x10state, 1, 1},
    {"rfxtemp", c_rfxcmds, 1, 1},
    {"rfxrh", c_rfxcmds, 1, 1},
    {"rfxbp", c_rfxcmds, 1, 1},
    {"rfxvs", c_rfxcmds, 1, 1},
    {"rfxvad", c_rfxcmds, 1, 1},
    {"rfxvadi", c_rfxcmds, 1, 1},
    {"rfxpot", c_rfxcmds, 1, 1},
    {"rfxtemp2", c_rfxcmds, 1, 1},
    {"rfxlobat", c_rfxcmds, 1, 1},
#endif
#ifdef HAVE_FEATURE_RFXM
    {"rfxflag_state", c_x10state, 1, 1},
    {"rfxpower", c_rfxcmds, 1, 1},
    {"rfxwater", c_rfxcmds, 1, 1},
    {"rfxgas", c_rfxcmds, 1, 1},
    {"rfxpulse", c_rfxcmds, 1, 1},
    {"rfxcount", c_rfxcmds, 1, 1},
    {"rfxpanel", c_rfxcmds, 1, 1},
#endif
#ifdef HAVE_FEATURE_DMX
    {"dmxflag_state", c_x10state, 1, 1},
    {"dmxtemp", c_dmxcmds, 1, 1},
    {"dmxsetpoint", c_dmxcmds, 1, 1},
    {"dmxstatus", c_dmxcmds, 1, 1},
    {"dmxmode", c_dmxcmds, 1, 1},
#endif /* HAVE_FEATURE_DMX */
#ifdef HAVE_FEATURE_ORE
    {"oreflag_state", c_x10state, 1, 1},
    {"oretemp", c_orecmds, 1, 1},
    {"orerh", c_orecmds, 1, 1},
    {"orebp", c_orecmds, 1, 1},
    {"orewgt", c_orecmds, 1, 1},
    {"elscurr", c_orecmds, 1, 1},
    {"orewindsp", c_orecmds, 1, 1},
    {"orewindavsp", c_orecmds, 1, 1},
    {"orewinddir", c_orecmds, 1, 1},
    {"orerainrate", c_orecmds, 1, 1},
    {"oreraintot", c_orecmds, 1, 1},
    {"oreuv", c_orecmds, 1, 1},
    {"owlpower", c_orecmds, 1, 1},
    {"owlenergy", c_orecmds, 1, 1},
    {"ore_emu", c_ore_emu, 1, 1},
#endif /* HAVE_FEATURE_ORE */
    {"timer_times", c_timer_times, 1, 1},
    {"webhook", c_webhook, 1, 1},
    {"counter", c_counter, 1, 1},
    {"logtail", c_logtail, 1, 1},
//    {"modlist", c_modlist, 0, 1},
//    {"conflist", c_conflist, 0, 1},
    {"launch",  c_launch, 1, 1},
    {"sec_emu", c_sec_emu, 1, 1},
    {"", NULL , 0, 0}
};

char *argptr;

/*-------------------------------------------------------------------+
 |  Main                                                             |
 +-------------------------------------------------------------------*/
int main ( int argc, char *argv[] )
{
    register int i;
    int (*rtn) ();
    struct cmdentry *c;
    int retcode;
    int ntokens;
    char writefilename[PATH_LEN + 1];
    char *cptr;
    int start_engine_main();
    int check_for_engine();
    int check_dir_rw ( char *, char * );

    configp = &config;
	
    strncpy2(heyuprogname, argv[0], sizeof(heyuprogname) - 10);

    /* Record if heyu was executed downstream of the heyu state engine or relay */
    heyu_parent = D_CMDLINE;
    if ( (cptr = getenv("HEYU_PARENT")) != NULL ) {
       if ( strcmp(cptr, "RELAY") == 0 ) {
          heyu_parent = D_RELAY;
          i_am_state = 0;
       }    
       else if ( strcmp(cptr, "ENGINE") == 0 ) {
          heyu_parent = D_ENGINE;
          i_am_state = 0;
       }
       else if ( strcmp(cptr, "AUXDEV") == 0 ) {
          heyu_parent = D_AUXDEV;
          i_am_state = 0;
       }
    }

    fdsout = fprfxo = stdout;
    fdserr = fprfxe = stderr;

    rtn = NULL;


    /* Check for and store options in options structure */
    /* Return number of tokens used for options, or -1  */
    /* if a usage error.                                */
    ntokens = heyu_getopt(argc, argv, optptr);

    if ( ntokens < 0 || (argc - ntokens) < 2 ) {
       fprintf(stderr, "Heyu version %s\n", VERSION );
       fprintf(stderr, "X10 Automation for Linux, Unix, and Mac OS X\n");
       fprintf(stderr, "Copyright Charles W. Sullivan and Daniel B. Suthers\n");
       fprintf(stderr, 
          "Usage: heyu [options] <command>  (Enter 'heyu help' for commands.)\n");
       return 1;
    }

    verbose = optptr->verbose;
    if( verbose )
	printf( "Version:%4s\n", VERSION );

    /* Remove the tokens used for options from the argument list */
    for ( i = 1; i < (argc - ntokens); i++ ) {
       argv[i] = argv[i + ntokens];
    }
    argc -= ntokens;

    if ( strcmp(argv[1], "list") == 0 ) {
       c_list();
       return 0;
    } 


    if ( strcmp(argv[1], "help") == 0 ||
         strcmp(argv[1], "syn") == 0  ||
         strcmp(argv[1], "linewidth") == 0  ) {
       c_command(argc, argv);
       return 0;
    }

    if ( strcmp(argv[1], "utility") == 0 ) {
          c_utility(argc, argv);
          return 0;
    }

    if ( strcmp(argv[1], "modlist") == 0 ) {
       return c_modlist(argc, argv);
    }
    else if ( strcmp(argv[1], "conflist") == 0 ) {
       return c_conflist(argc, argv);
    }
    else if ( strcmp(argv[1], "stateflaglist") == 0 ) {
       return c_stateflaglist(argc, argv);
    }
    else if ( strcmp(argv[1], "masklist") == 0 ) {
       return c_masklist(argc, argv);
    }

#if 0
    if ( strcmp(argv[1], "webhook") == 0 ) {
       c_webhook(argc, argv);
       return 0;
    }
#endif

    if ( strcmp(argv[1], "show") == 0 ) {
       retcode = c_show1(argc, argv);
       if ( retcode == 0 ) {
          free_all_arrays(configp);
          return 0;
       }
    }

    /* Commands other than those handled above require */
    /* a configuration file and file locking.          */

    if ( strcmp(argv[1], "stop") == 0 )
       read_minimal_config ( CONFIG_INIT, SRC_STOP );
    else
       read_config(CONFIG_INIT);
  
    if ( is_heyu_cmd(argv[1]) ) {
         /* This is a direct command */
         c = cmdtab;
         rtn = c->cmd_routine;
    }
    else {
        /* This is an administrative or state command */
        for (c = cmdtab + 1; c->cmd_routine != NULL; c++) {
	    if (strcmp(argv[1], c->cmd_name) == 0) {
                if ( !(configp->device_type & DEV_DUMMY) || c->internal_cmd ) {
	           rtn = c->cmd_routine;
                   break;
                }
                else {
                   fprintf(stderr,
                      "Command '%s' is not valid for TTY dummy\n", c->cmd_name);
                   return 1;
                }
	    }
        }
    }

    if ( rtn == NULL )  {
       fprintf(stderr, 
          "Usage: heyu [options] <command>  (Enter 'heyu help' for commands.)\n");
       return 1;
    }


    if ( (strcmp("stop", c->cmd_name) == 0 ) || 
         (strcmp("version", c->cmd_name) == 0) ||
         (strcmp("help", c->cmd_name) == 0) )     {
       retcode = (*rtn)();		/* exits */
       free_all_arrays(configp);
       return retcode;
    }

    /* Check read/write permissions for spoolfile directory */
    if ( check_dir_rw(SPOOLDIR, "SPOOL") != 0 ) {
       fprintf(stderr, "%s\n", error_message());
       return 1;
    }

    if ( quick_ports_check() != 0 ) {
       fprintf(stderr, "Serial port %s is in use by another program.\n", configp->ttyaux);
       return 1;
    }

    if ( c->lock_needed == 1 ) {
       if ( lock_for_write() < 0 )
          error("Program exiting.\n");
       port_locked = 1;
    }

    argptr = argv[0];

    /* Setup alert command arrays */
    create_alerts();

    /* Check for and start relay if not already running */
    start_relay(configp->tty);
    if ( ! i_am_relay ) {
      setup_sp_tty();
    }

    if ( strcmp("start", c->cmd_name) == 0 &&
       check_for_engine() != 0 &&
       configp->start_engine == AUTOMATIC )  {
       if ( heyu_parent == D_CMDLINE )
          printf("starting heyu_engine\n");
       init("engine");
       c_start_engine(argc, argv);
    }

    if ( strcmp("start", c->cmd_name) == 0 &&
       check_for_aux() != 0 &&
       configp->ttyaux[0] != '\0' ) {
          if ( heyu_parent == D_CMDLINE )
             printf("starting heyu_aux\n");
          init("aux");
          c_start_aux(argc, argv);
    }


#ifdef MINIEXCH
    mxconnect(MINIXPORT);
#endif

    init(c->cmd_name);

    retcode = (*rtn) (argc, argv);
    if ( port_locked == 1 ) {
       sprintf(writefilename, "%s%s", WRITEFILE, configp->suffix);
       munlock(writefilename);
    } 
    free_all_arrays(configp);
    return retcode;
}

/*-------------------------------------------------------------------+
 |  Init                                                             |
 +-------------------------------------------------------------------*/

void init ( char *cmd_name )
{

    timeout = TIMEOUT;
    if( strcmp(cmd_name, "monitor") == 0 )
    {
	check4poll(1, 2);
    }
    else if ( strcmp(cmd_name, "engine") == 0 )
        check4poll(0, 0);
    else if ( strcmp(cmd_name, "info") == 0 )
        check4poll(0, 0) ; 
    else
	check4poll(0, 0);

    /* Now check the status of the interface. */
    /* if( get_status() < 1)
        error("could not get the interface status");
    */
}



/*
 * Convert X10-style day of week (bit map, bit 0=monday, 6=sunday)
 * to UNIX localtime(3) style day of week (integer, 0=sunday)
 */
/* DBS  NOTE:  This is valid for CP290, not for CM11A.
 * The CM11A uses unix style.
 */


int dowX2U(b)
register char b;
{
    register int n;

    for (n = 1; (!(b & 1)) && n < 8; n++, b = b >> 1)
    	;
    if (n == 7)
	n = 0;
    if (n == 8)
	n = 7;
    return (n);
}



int dowU2X(d)
int d;
{
    if (d == 0)
	d = 7;
    return (1 << (d - 1));
}



int read_config( unsigned char mode ) 
{
   int retcode; 

   /* Read the user's Heyu configuration file and store in */
   /* global CONFIG structure 'config'.  Transfer some of  */
   /* the data into existing variables.                    */
  
   retcode = get_configuration(mode);

   x10_housecode = hc2code(configp->housecode);
    
   return retcode;
}


/* Takes the bits in CM11A format (as used in the reply to a status) 
 * and returns a day string.
 * The b parameter is for bitmap */
char *bits2day(b)
int b;
{
   
   switch(b)
   {
       case 1:
	   return("Sun");
       case 2:
	   return("Mon");
       case 4:
	   return("Tue");
       case 8:
	   return("Wed");
       case 16:
	   return("Thu");
       case 32:
	   return("Fri");
       case 64:
	   return("Sat");
    }
    return("Error");
}

int c_version()
{
    printf( "Version:%4s\n", VERSION );
    return(0);
}

void display(RCCS)
char *RCCS;
{

    /* This is not used much so far */
    if( verbose > 1)
        printf( "%s\n", RCCS);
}


/* List the variables as built into the program at compile time */
int c_list ()
{
   if ( verbose ) {
      printf( "Version: %s\n", VERSION );
      printf( "The SPOOLDIR is %s\n", SPOOLDIR);
      printf( "The LOCKDIR is %s\n", LOCKDIR);
      printf( "The SYSBASEDIR is %s\n", SYSBASEDIR);
      printf( "Number of Flags = %d\n", (32 * NUM_FLAG_BANKS));
      printf( "Number of Counters = %d\n", (32 * NUM_COUNTER_BANKS));
      printf( "Number of Timers = %d\n", (32 * NUM_TIMER_BANKS));
   }
   else {
      printf( "Version=%s\n", VERSION );
      printf( "SPOOLDIR=%s\n", SPOOLDIR);
      printf( "LOCKDIR=%s\n", LOCKDIR);
      printf( "SYSBASEDIR=%s\n", SYSBASEDIR);
      printf( "Flags=%d\n", (32 * NUM_FLAG_BANKS));
      printf( "Counters=%d\n", (32 * NUM_COUNTER_BANKS));
      printf( "Timers=%d\n", (32 * NUM_TIMER_BANKS));
   }

   return 0;
}

/* Start command does nothing by itself */
/* but allows daemons to start.         */
int c_start( int argc, char *argv[] )
{
   return 0;
}

/* Reread configuration file */
int reread_config ( void )
{
   config.read_flag = 0;

   /* Null out the option pointers so a restart will use */
   /* the config file defined by env variable X10CONFIG  */
   if ( i_am_state || i_am_relay || i_am_aux || i_am_monitor ) { 
      optptr->configp = NULL;
      optptr->subp = NULL;
   }

   return read_config(CONFIG_RESTART);

}

/* Check existance and read/write ability of a directory */
int check_dir_rw ( char *pathspec, char *label )
{
   struct stat statb;
#ifdef GETGROUPS_T
   GETGROUPS_T grps[100];
#else
   gid_t       grps[100];
#endif
   int         ngrps, j;
   char        errmsg[256];
   int         msgl = sizeof(errmsg) - 1;

   clear_error_message();

   if ( stat(pathspec, &statb) != 0 ) {
      snprintf(errmsg, msgl, "%s directory %s does not exist", label, pathspec);
      store_error_message(errmsg);
      return 1;
   }

#ifdef S_ISDIR
   if ( S_ISDIR(statb.st_mode) == 0 ) {
#else
   if ( (statb.st_mode & S_IFDIR) != S_IFDIR ) {
#endif
      snprintf(errmsg, msgl, "%s %s is not a directory", label, pathspec);
      store_error_message(errmsg);
      return 1;
   }

   if ( getuid() == 0 )
      return 0;

   if ( (statb.st_mode & S_IRWXU) == S_IRWXU &&   
        (statb.st_uid == getuid() || statb.st_uid == geteuid()) ) {
      return 0;
   }

   if ( (statb.st_mode & S_IRWXG) == S_IRWXG ) {
      if ( (ngrps = getgroups((sizeof(grps)/sizeof(typeof(*grps))) - 1, grps)) < 0 ) {
         snprintf(errmsg, msgl, "Internal error - getgroups()");
         store_error_message(errmsg);
         return 1;
      }
      grps[ngrps++] = getegid();
      for ( j = 0; j < ngrps; j++ ) {
         if ( statb.st_gid == grps[j] )
            return 0;
      }
   }

   if ( ((statb.st_mode & S_IRWXO) == S_IRWXO) )
      return 0;

   snprintf(errmsg, msgl, "Insufficient r/w permission for %s directory %s", label, pathspec);
   store_error_message(errmsg);
   return 1;
}

