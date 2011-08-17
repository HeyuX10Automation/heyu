
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#endif
#include <ctype.h>
#if defined(SYSV) || defined(FREEBSD) || defined(OPENBSD)
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif

#include "x10.h"
#include "process.h"
#include "x10state.h"
#include "digimax.h"

extern int sptty;
extern int i_am_state, i_am_aux, i_am_relay, i_am_rfxmeter;
extern char *funclabel[];
extern void create_flagslist ( unsigned char, unsigned long, char * );
void update_activity_timeout ( ALIAS *, int );

extern CONFIG config;
extern CONFIG *configp;

extern struct x10global_st x10global;
extern x10hcode_t *x10state;

extern unsigned int modmask[NumModMasks][16];
extern int lock_for_write(), munlock();


/*------------------------------------------------------------------------+
 | Interpret Digimax data string, update the state, and test whether      |
 | any launch condition is satisfied.                                     |
 | The data packet has been forwarded 3 times in succession so it can be  |
 | split here into 3 separate Heyu functions. Parameter 'seq' numbers the |
 | packets 1, 2, 3.                                                       |
 | buffer references:                                                     |
 |  ST_COMMAND, ST_LONGVDATA, vtype, seq, vidhi, vidlo, nbytes, bytes 1-N |
 +------------------------------------------------------------------------*/
char *translate_digimax ( unsigned char *buf, unsigned char *sunchanged, int *launchp )
{

#ifdef HASDMX
   static char    outbuf[160];
   char           flagslist[80], unknown[32];
   ALIAS          *aliasp;
   LAUNCHER       *launcherp;
   extern unsigned int signal_source;
   unsigned char  func, *vdatap, vtype, seq, vdata = 0;
   unsigned short vident;
   int            unit, loc, tempc;
   unsigned char  status, settempc;
   unsigned long  longvdata, prevdata, currdata, heatmode, onoff;
   char           hc;

   unsigned char  actfunc, xactfunc;
   unsigned int   trigaddr, mask, active, trigactive;
   unsigned int   modchgstate, changestate;
   unsigned int   bmaplaunch, launched;
   unsigned long  afuncmap, xfuncmap;

   int            j, k, found, index = -1;
   unsigned char  hcode, ucode, trig;
   unsigned int   bitmap = 0, vflags = 0;

   static unsigned int  startupstate;

   static char *typename[] = {"Std", "Ent", "Sec", "RFXSensor", "RFXMeter",
     "?", "??", "???", "????", "DigiMax", "Noise", "Noise2"};

   launcherp = configp->launcherp;
   aliasp = configp->aliasp;

   *launchp = -1;
   *sunchanged = 0;
   flagslist[0] = '\0';
   unknown[0] = '\0';
   *outbuf = '\0';
   changestate = modchgstate = 0;


   vtype  = buf[2];
   seq =    buf[3];
   vident = (buf[4] << 8) | buf[5];
   vdatap  = buf + 9;
   func   = VdataFunc;

   x10global.longvdata = 0;
   x10global.lastvtype = vtype;

   hcode = ucode = 0;
   found = 0;
   j = 0;
   /* Look up the alias, if any */
   while ( !found && aliasp && aliasp[j].line_no > 0 ) {
      if ( aliasp[j].vtype == vtype && 
           (bitmap = aliasp[j].unitbmap) > 0 &&
           (ucode = single_bmap_unit(aliasp[j].unitbmap)) != 0xff ) {
         for ( k = 0; k < aliasp[j].nident; k++ ) {
            if ( aliasp[j].ident[k] == vident ) {
               index = j;
               found = 1;
               break;
            }
         }
      }
      j++;
   }

   if ( !found || !aliasp ) {
      if ( seq == 1 ) {
         sprintf(outbuf, "func %12s : Type %s ID 0x%04X Data 0x%02X%02X%02X%02X",
           "RFdata", typename[vtype], vident, vdatap[0], vdatap[1], vdatap[2], vdatap[3]);
      }
      return outbuf;
   }
   
   hc = aliasp[index].housecode;
   hcode = hc2code(hc);
   unit = code2unit(ucode);

   loc = aliasp[index].storage_index;
   prevdata = x10global.data_storage[loc];
   currdata = (vdatap[0] << 24) | (vdatap[1] << 16) | (vdatap[2] << 8) | vdatap[3];
   /* Blank out parity bits */
   currdata &= ~PARMASK;

   switch ( seq ) {
      case 1 : /* Process current temperature */

         /* Create a special message if any of the undefined status bits are set */
         /* Hopefully a user seeing this message will report it so we can figure */
         /* out their meaning (low battery perhaps).                             */
         if ( currdata & UNDEFMASK ) {
            sprintf(unknown, "unknown = 0x%08lx ", currdata & UNDEFMASK);
         }

         status = (currdata & STATMASK) >> STATSHIFT;

         longvdata = (prevdata & (TSETPMASK | SETPFLAG | ONOFFMASK)) |
                     (currdata & (STATMASK | TCURRMASK | HEATMASK)) | VALMASK;

         if ( status == 0 || status == 3 ) {
            longvdata &= ~ONOFFMASK;
         }
         if ( status == 0 )
            longvdata &= ~(TSETPMASK | SETPFLAG);
 
         vflags = (longvdata & HEATMASK) ? 0 : DMX_HEAT;
         vflags |= (status == 3) ? DMX_INIT : 0;
         vflags |= (longvdata & SETPFLAG) ? DMX_SET : 0;
         vflags |= DMX_TEMP;

         tempc = (longvdata & TCURRMASK) >> TCURRSHIFT;
         tempc = (tempc & 0x80) ? 0x80 - tempc : tempc;

         modchgstate = ((prevdata ^ currdata) & TCURRMASK) ? bitmap : 0;

         /* Update the storage location */
         x10global.data_storage[loc] = longvdata;
         x10global.longvdata = longvdata;

         func = DmxTempFunc;
         trig = DmxTempTrig;

         create_flagslist (vtype, vflags, flagslist);
         sprintf(outbuf, "func %12s : hu %c%-2d Temp %dC %s %s(%s)",
            funclabel[func], hc, unit, tempc, flagslist, unknown, aliasp[index].label);
         break;

      case 2 :

         status = (currdata & STATMASK) >> STATSHIFT;

         longvdata = (prevdata & (TCURRMASK | HEATMASK | ONOFFMASK)) |
                     (currdata & (STATMASK | TSETPMASK)) | VALMASK;

         if ( status == 0 || status == 3 )
            longvdata &= ~ONOFFMASK;

         if ( status == 0 )
            longvdata &= ~(TSETPMASK | SETPFLAG);
         else
            longvdata |= SETPFLAG;

         modchgstate = ((prevdata ^ longvdata) & TSETPMASK) ? bitmap : 0;

         settempc = (longvdata & TSETPMASK) >> TSETPSHIFT;

         vflags =  (longvdata & HEATMASK) ? 0 : DMX_HEAT;
         vflags |= (longvdata & SETPFLAG) ? DMX_SET : 0;
         vflags |= (status == 3) ? DMX_INIT : 0;
         vflags |= DMX_TEMP;

         /* Update stored data */
         x10global.data_storage[loc] = longvdata;
         x10global.longvdata = longvdata;


         if ( status == 0 )
            return "";
            
         func = DmxSetPtFunc;
         trig = DmxSetPtTrig;

         create_flagslist(vtype, vflags, flagslist);

         sprintf(outbuf, "func %12s : hu %c%-2d Setpoint %dC %s (%s)",
            funclabel[func], hc, unit, settempc, flagslist, aliasp[index].label);

         break;

      case 3 : /* Process Heat On/Off */

         status = (currdata & STATMASK) >> STATSHIFT;

         longvdata = ((prevdata & (TCURRMASK | TSETPMASK | HEATMASK | SETPFLAG)) | 
                     ((currdata & STATMASK)) ) | VALMASK;

         if ( status == 1 || status == 2 ) {
            longvdata |= (status << ONOFFSHIFT);
         }

         x10global.data_storage[loc] = longvdata;
         x10global.longvdata = longvdata;
         
         modchgstate = ((prevdata ^ longvdata) & ONOFFMASK) ? bitmap : 0;
         
         vflags = (longvdata & HEATMASK) ? 0 : DMX_HEAT;
         vflags |= (status != 0) ? DMX_SET : 0;
         vflags |= (status == 3) ? DMX_INIT : 0;
         vflags |= DMX_TEMP;

         /* Update stored data */
         x10global.data_storage[loc] = longvdata;
         x10global.longvdata = longvdata;


         onoff = (longvdata & ONOFFMASK) >> ONOFFSHIFT;
         if ( onoff == 0 )
            return "";

         heatmode = (longvdata & HEATMASK) >> HEATSHIFT;

         x10state[hcode].state[DimState] &= ~bitmap;

         /* Not sure if On/Off needs to be swapped in "cool" mode */
         /* (which requires cutting a jumper wire in the thermostat) */
         /* or if the DigiMax or it's base station will handle this */
         /* automatically. (DMXCOOLSWAP YES|NO is defined in digimax.h) */
         if ( DMXCOOLSWAP == YES )
            onoff = (heatmode == 0) ? onoff : ((onoff + 2) % 2) + 1;

         if ( onoff == 1 ) {
            func = DmxOnFunc;
            trig = DmxOnTrig;
            x10state[hcode].state[OnState] |= bitmap;
            x10state[hcode].dimlevel[ucode] = vdata = 0xff;
         }
         else {
            func = DmxOffFunc;
            trig = DmxOffTrig;
            x10state[hcode].state[OnState] &= ~bitmap;
            x10state[hcode].dimlevel[ucode] = vdata = 0;
         }

         create_flagslist (vtype, vflags, flagslist);
         sprintf(outbuf, "func %12s : hu %c%-2d %s (%s)",
            funclabel[func], hc, unit, flagslist, aliasp[index].label);
         break;

      default :
         return "";
   }

   x10state[hcode].vaddress = bitmap;
   x10state[hcode].lastcmd = func;
   x10state[hcode].lastunit = unit;
   x10state[hcode].vident[ucode] = vident;
   x10state[hcode].vflags[ucode] = vflags;
   x10state[hcode].timestamp[ucode] = time(NULL);
   x10global.lasthc = hcode;
   x10global.lastaddr = 0;

   if ( vflags & SEC_LOBAT ) {
      x10state[hcode].state[LoBatState] |= (1 << ucode);
   }
   else {
      x10state[hcode].state[LoBatState] &= ~(1 << ucode);
   }

   actfunc = 0;
   afuncmap = 0;
   xactfunc = func;
   xfuncmap = (1 << trig);
   trigaddr = bitmap;

   mask = modmask[VdataMask][hcode];
   active = bitmap & mask;

   if ( seq == 1 ) {
      startupstate = ~x10state[hcode].state[ValidState] & active;
      x10state[hcode].state[ValidState] |= active;
      /* Update activity timeout for sensor */
      update_activity_timeout(aliasp, index);
      update_activity_states(hcode, aliasp[index].unitbmap, S_ACTIVE);
   }

   x10state[hcode].state[ModChgState] = modchgstate;

   x10state[hcode].state[ChgState] = x10state[hcode].state[ModChgState] |
      (x10state[hcode].state[ActiveChgState] & active) |
      (startupstate & ~modmask[PhysMask][hcode]);

   changestate = x10state[hcode].state[ChgState];

   *sunchanged = (changestate & active) ? 0 : 1;

   if ( configp->show_change == YES ) {
      snprintf(outbuf + strlen(outbuf), sizeof(outbuf), "%s", ((changestate & active) ? " Chg" : " UnChg"));
   }

   if ( i_am_state )
      write_x10state_file();

   /* Heyuhelper, if applicable */
   if ( i_am_state && signal_source & RCVI && configp->script_mode & HEYU_HELPER ) {
      launch_heyuhelper(hcode, trigaddr, func);
      return outbuf;
   }

   bmaplaunch = 0;
   launched = 0;
   j = 0;
   while ( launcherp && launcherp[j].line_no > 0 ) {
      if ( launcherp[j].type != L_NORMAL ||
           launcherp[j].hcode != hcode ||
           is_unmatched_flags(&launcherp[j]) ||
           (launcherp[j].vflags & x10state[hcode].vflags[ucode]) != launcherp[j].vflags ||
	   (launcherp[j].notvflags & ~x10state[hcode].vflags[ucode]) != launcherp[j].notvflags ) {
         j++;
	 continue;
      }

      if ( launcherp[j].xfuncmap & xfuncmap &&
           launcherp[j].source & signal_source ) {
         trigactive = trigaddr & (mask | launcherp[j].signal);
         if ( (bmaplaunch = (trigactive & launcherp[j].bmaptrig) &
              (changestate | ~launcherp[j].chgtrig)) ||
              (launcherp[j].unitstar && !trigaddr)) {
            launcherp[j].matched = YES;
            *launchp = j;
            launcherp[j].actfunc = xactfunc;
            launcherp[j].genfunc = 0;
	    launcherp[j].xfunc = xactfunc;
	    launcherp[j].level = x10state[hcode].dimlevel[ucode];
            launcherp[j].bmaplaunch = bmaplaunch;
            launcherp[j].actsource = signal_source;
            launched |= bmaplaunch;
            if ( launcherp[j].scanmode & FM_BREAK )
               break;
         }
      }
      j++;
   }

   x10state[hcode].launched = launched;

   return outbuf;
#else
   return "";
#endif /* HASDMX */
}


/*---------------------------------------------------------------------+
 | Display stored data for Digimax thermostat                          |
 +---------------------------------------------------------------------*/
int show_digimax ( void )
{

#if HASDMX
   ALIAS         *aliasp;
   char          hc;
   int           unit, index, temp, count = 0, maxlabel = 0;
   unsigned char hcode, ucode, status, onoff;
   unsigned long longvdata;
   static char *onoff_status[] = {"---", "On", "Off", ""};

   if ( !(aliasp = configp->aliasp) ) 
      return 0;

   /* Get maximum lengths of module name and alias label */
   index = 0;
   while ( aliasp[index].line_no > 0 ) {
      if ( aliasp[index].vtype == RF_DIGIMAX ) {
         count++;
         maxlabel = max(maxlabel, (int)strlen(aliasp[index].label));
      }
      index++;
   }

   if ( count == 0 )
      return 0;

   index = 0;
   while ( aliasp[index].line_no > 0 ) {
      if ( aliasp[index].vtype != RF_DIGIMAX ) {
         index++;
         continue;
      }
      ucode = single_bmap_unit(aliasp[index].unitbmap);
      unit = code2unit(ucode);
      hc = aliasp[index].housecode;
      hcode = hc2code(hc);

      longvdata = x10global.data_storage[aliasp[index].storage_index];

      if ( longvdata == 0 ) {
         printf("%c%-2d %-*s  Temp ---  Setpoint ---  mode: ----  status: ---\n",
            hc, unit, maxlabel, aliasp[index].label);
         index++;
         continue;
      }

      status = (longvdata & STATMASK) >> STATSHIFT;

      printf("%c%-2d %-*s", hc, unit, maxlabel, aliasp[index].label);

      temp = (longvdata & TCURRMASK) >> TCURRSHIFT;
      if ( temp & 0x80 )
         temp = 0x80 - temp;
      printf("  Temp %2dC", temp);

      temp = (longvdata & TSETPMASK) >> TSETPSHIFT;
      if ( (longvdata & SETPFLAG) == 0 ) 
         printf("  Setpoint ---");
      else
         printf("  Setpoint %2dC", temp);

      printf("  mode: %s", ((longvdata & HEATMASK) ? "Cool" : "Heat"));

      onoff = (longvdata & ONOFFMASK) >> ONOFFSHIFT;
      printf("  status: %s", onoff_status[onoff]);

      printf("\n");

      index++;
   }
#endif /* HASDMX */

   return 0;
}

#ifdef HASDMX 
/*---------------------------------------------------------------------+
 | Display a Digimax data value stored in the x10state structure.      |
 +---------------------------------------------------------------------*/
int c_dmxcmds ( int argc, char *argv[] )
{

   ALIAS          *aliasp;
   unsigned char  hcode, ucode, mode;
   unsigned long  aflags;
   unsigned long  longvdata;
   char           hc;
   unsigned int   bitmap;
   int            unit, index, tempc;

   int read_x10state_file ( void );

   if ( argc < 3 ) {
      fprintf(stderr, "Usage: %s %s Hu\n", argv[0], argv[1]);
      return 1;
   }

   if ( check_for_engine() != 0 ) {
      fprintf(stderr, "State engine is not running.\n");
      return 1;
   }
   if ( read_x10state_file() != 0 ) {
      fprintf(stderr, "Unable to read state file.\n");
      return 1;
   }

   if ( (aliasp = configp->aliasp) == NULL )
      return 1;

      
   aflags = parse_addr(argv[2], &hc, &bitmap);
   if ( !(aflags & A_VALID) || aflags & (A_DUMMY | A_PLUS | A_MINUS) || bitmap == 0 ) {
      fprintf(stderr, "Invalid Hu address '%s'\n", argv[2]);
      return 1;
   }
   if ( aflags & A_MULT ) {
      fprintf(stderr, "Only a single unit address is valid.\n");
      return 1;
   }
   hcode = hc2code(hc);
   ucode = single_bmap_unit(bitmap);
   unit  = code2unit(ucode);

   if ( (index = alias_lookup_index(hc, bitmap, RF_DIGIMAX)) < 0 ) {
      fprintf(stderr, "Address %c%d is not configured as a Digimax\n", hc, unit);
      return 1;
   }

   longvdata = x10global.data_storage[aliasp[index].storage_index];

   if ( (longvdata & 0x01) == 0 ) {
      fprintf(stderr, "Not ready\n");
      return 1;
   }

   if ( strcmp(argv[1], "dmxtemp") == 0 ) {
      tempc = (longvdata & TCURRMASK) >> TCURRSHIFT;
      tempc = (tempc & 0x80) ? (0x80 - tempc) : tempc;
      printf("%d\n", tempc);
   }
   else if ( strcmp(argv[1], "dmxsetpoint") == 0 ) {
      if ( longvdata & SETPFLAG ) {
         tempc = (longvdata & TSETPMASK) >> TSETPSHIFT;
         printf("%d\n", tempc);
      }
      else {
         fprintf(stderr, "Not ready\n");
         return 1;
      }
   }
   else if ( strcmp(argv[1], "dmxstatus") == 0 ) {
      mode = (longvdata & ONOFFMASK) >> ONOFFSHIFT;
      if ( mode > 0 ) {      
         printf("%d\n", ((mode == 1) ? 1 : 0));
      }
      else {
         fprintf(stderr, "Not ready\n");
         return 1;
      }
   }
   else if ( strcmp(argv[1], "dmxmode") == 0 ) {
      printf("%d\n", ((longvdata & HEATMASK) ? 0 : 1));
   }

   return 0;
}
#endif /* HASDMX */
