
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

#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#if defined(SYSV) || defined(FREEBSD) || defined(OPENBSD)
#include <string.h>
#else
#include <strings.h>
#endif
#include <time.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "x10.h"
#include "sun.h"
#include "process.h"
#include "version.h"
#include "local.h"

#define NDSTINTV 6
struct dststruct {
  long  elapsed;   /* Elapsed minutes from 00:00 hours legal time Jan 1st */
  int   offset;    /* Minutes to add to ST to get Legal Time  during each */
                   /*  interval.                                          */
} lgls[NDSTINTV], dsts[NDSTINTV], stds[NDSTINTV], stdr[NDSTINTV];

#if 0
#define NDSTINTV (int)(sizeof(lgls)/sizeof(struct dststruct))
#endif

/* Contains info written to or read from the x10record file */
struct record_info x10record = {0, 0, 0, 0, 0, 0, 0, 0};
struct record_info *x10recordp = &x10record;

#define PROMSIZE 1024

/* Global variables */

int    line_no;
int    timer_size     = 0;
int    timer_maxsize  = 0;
int    timer_savesize = 0;
int    tevent_size    = 0;
int    tevent_savesize = 0;
int    tevent_maxsize = 0;
int    current_timer_generation = 0;
int    save_timer_generation = 0;
int    timer_generation_delta = 0;
int    current_tevent_generation = 0;
int    save_tevent_generation = 0;
int    tevent_generation_delta = 0;
char   default_housecode = 'A';

long int std_tzone;    /* Timezone in seconds West of Greenwich */
char   *heyu_tzname[2];

/* Directory (terminated with /) containing the critical */
/* heyu files x10config, x10record, and x10macroxref     */
char   heyu_path[PATH_LEN + 128];

char   schedfile[PATH_LEN + 1];
char   heyu_script[PATH_LEN + 1];

/* State file */
extern char   statefile[];

/* Alternate optional directory (specified in x10config) */
/* for report and non-critical files                     */
char   alt_path[PATH_LEN + 1]; 

char   heyu_config[PATH_LEN + 1]; /* Filename of Heyu configuration file */
int    is_writable;              /* Flag: Heyu directory is writable */

/* External variables */

extern int verbose, i_am_relay;
extern CONFIG config;
extern CONFIG *configp;

extern struct opt_st *optptr;

char *wday_name[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

char *month_name[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                      "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

/* Extended error management functions - these allow stringing together */
/* error messages from a tree of function calls into a (hopefully)      */
/* meaningful message for the user.                                     */

/*----------------------------------------------------------------------------+
 | Pass back pointer and size of error message buffer - for internal use by   |
 | functions below.                                                           |
 +----------------------------------------------------------------------------*/
void error_area ( char **pointer, int *space )
{
   static char error_buffer[1024];

   *pointer = error_buffer;
   *space = (sizeof(error_buffer)/sizeof(char));
   return;
}

/*----------------------------------------------------------------------------+
 | Save an error message in error message buffer                              |
 +----------------------------------------------------------------------------*/
void store_error_message ( char *message ) 
{
   char *buffp = NULL;
   int  space;

   error_area(&buffp, &space);

   if ( space < (int)strlen(message) + 1 ) {
      fprintf(stderr, "Internal error: no space - store_error_message()\n");
      exit(1);
   }
   strncpy2(buffp, message, space - 1);
   return;
}

/*----------------------------------------------------------------------------+
 | Prepend a message to an existing error message in buffer                   |
 +----------------------------------------------------------------------------*/
void add_error_prefix ( char *prefix )
{
   char        *buffp = NULL;
   char        *oldmsg = NULL;
   int         space;

   error_area(&buffp, &space);

   if ( space < ((int)strlen(prefix) + (int)strlen(buffp) + 1) ) {
      fprintf(stderr, "Internal error: no space - add_error_prefix()\n");
      exit(1);
   }
   if ( (oldmsg = strdup(buffp)) == NULL ) {
      fprintf(stderr, "Unable to allocate memory for error message\n");
      exit(1);
   }
   strncpy2(buffp, prefix, space - 1);
   strncat(buffp, oldmsg, space - 1 - strlen(buffp));
   free(oldmsg);

   return;
}

/*----------------------------------------------------------------------------+
 | Append a message to an existing error message in buffer                    |
 +----------------------------------------------------------------------------*/
void add_error_suffix ( char *suffix )
{
   char  *buffp = NULL;
   int   space;

   error_area(&buffp, &space);

   if ( space < ((int)strlen(buffp) + (int)strlen(suffix) + 1) ) {
      fprintf(stderr, "Internal error: no space - add_error_suffix()\n");
      exit(1);
   }

   strncat(buffp, suffix, space - 1 - strlen(buffp));
 
   return;
}

/*----------------------------------------------------------------------------+
 | Get pointer to error message buffer                                        |
 +----------------------------------------------------------------------------*/
char *error_message ( void )
{
   char *buffp = NULL;
   int  space;

   error_area(&buffp, &space);

   return buffp;
}

/*----------------------------------------------------------------------------+
 | Clear the error message buffer                                             |
 +----------------------------------------------------------------------------*/
void clear_error_message( void )
{
   char *buffp = NULL;
   int  space;

   error_area(&buffp, &space);

   *buffp = '\0';
   return;
}

/* Debugging functions */
void tp ( int point )
{
   printf("testpoint %d\n", point);
   fflush(stdout);
   return ;
}

int show_timer_links( TIMER *timerp ) 
{
   int j;
   for ( j = 0; j < timer_maxsize; j++ )
      printf("%3d  %3d\n", j, timerp[j].link);
   return 0;
}

/*----------------------------------------------------------------------------+
 | Display tokens                                                             |
 +----------------------------------------------------------------------------*/
void display_tokens ( int tokc, char *tokv[] ) 
{
   int j;

   for ( j = 0; j < tokc; j++ ) {
      printf(" \"%s\"", tokv[j]);
   }
   printf("\n");
   return;
}


/*----------------------------------------------------------------------------+
 | Verify that the start and stop times in any timer are not the same.        |
 | (If they are, CM11A will not launch the stop macro.)                       |
 | Return the number of timers failing the test.                              |
 +----------------------------------------------------------------------------*/
int check_timer_start_stop ( TIMER *timerp )
{
   int j, count;

   if ( !timerp )
      return 0;

   j = 0;
   count = 0;
   while ( timerp[j].line_no > 0 ) {
      if ( timerp[j].generation != current_timer_generation ||
           timerp[j].flag_start == NO_EVENT ||
           timerp[j].flag_stop  == NO_EVENT     ) {
         j++;
         continue;
      }
      if ( timerp[j].offset_stop == timerp[j].offset_start ) 
         count++;
      j++;
   }
   return count;
}

/*----------------------------------------------------------------------------+
 | Verify an unbroken chain of TEVENT links.                                  |
 +----------------------------------------------------------------------------*/
int verify_tevent_links ( TEVENT *teventp )
{
   int  *linker, *linkee;
   int  j, k, start, size, retcode;
   static int sizeint = sizeof(int);

   if ( !teventp )
      return 0;

   size = 0;
   while ( teventp[size].line_no > 0 ) 
      size++;

   linker = calloc( size, sizeint );
   if ( linker == NULL ) {
      (void)fprintf(stderr, "verify_tevent_links() - Unable to allocate memory 1\n");
      return -1;
   }
   linkee = calloc( size, sizeint );
   if ( linkee == NULL ) {
      (void)fprintf(stderr, "verify_tevent_links() - Unable to allocate memory 2\n");
      return -1;
   }

   /* Find the start of the chain, i.e., the tevent not linked to by any other */
   for ( j = 0; j < size; j++ ) { 
      linker[j] = 0;
   }
   for ( j = 0; j < size; j++ ) {
      k = teventp[j].link;
      if ( k > (size - 1) ) {
         (void)fprintf(stderr, 
           "verify_tevent_links() - link %d out of bound %d at index %d\n", k, size - 1, j);
         return -1;
      }
      if ( k >= 0 )
         linker[k] = 1;
   }
   for ( start = 0; start < size; start++ ) {
      if ( linker[start] == 0 )
         break;
   }   


   for ( j = 0; j < size; j++ ) {
      linkee[j] = 0;
      linker[j] = -1;
   }
  
   k = start;
   for ( j = 0; j < size; j++ ) {
      k = teventp[k].link;
      if ( k < 0 && j < (size - 1) ) {
         (void)fprintf(stderr, 
           "verify_tevent_links() - premature end of %d length chain at index %d\n", size, j);
         return -1;
      }
      if ( k > (size - 1) ) {
         (void)fprintf(stderr, 
           "verify_tevent_links() - link %d out of bound %d at index %d\n", k, size - 1, j);
         return -1;
      }

      if ( k >= 0 ) {
         linkee[k] += 1;
         linker[k] = j; 
      }
   }

   retcode = 0;
   for ( k = 1; k < size; k++ ) {
      if ( linkee[k] == 0 ) { 
         (void)fprintf(stderr, "teventp[linkee[%d]] is not in chain.\n", k);
         retcode = 1;  
      }
      if ( linkee[k] > 1 ) {
         (void)fprintf(stderr, "teventp[linkee[%d]] is multiply linked.\n", k);
         retcode = 1;
      }
      if ( linkee[k] == -1 ) {
         (void)fprintf(stderr, "End of chain at index %d\n", linker[k]);
         retcode = 1;
      } 
   }

   free(linker);
   free(linkee);

   return retcode;
}


/*----------------------------------------------------------------------------+
 | Verify an unbroken chain of TIMER links.                                   |
 +----------------------------------------------------------------------------*/
int verify_timer_links ( TIMER *timerp )
{
   int  *linker, *linkee;
   int  j, k, start, size, retcode;
   static int sizeint = sizeof(int);

   if ( !timerp )
      return 0;

   size = 0;
   while ( timerp[size].line_no > 0 ) 
      size++;

   linker = calloc( size, sizeint );
   if ( linker == NULL ) {
      (void)fprintf(stderr, "verify_timer_links() - Unable to allocate memory 1\n");
      return -1;
   }
   linkee = calloc( size, sizeint );
   if ( linkee == NULL ) {
      (void)fprintf(stderr, "verify_timer_links() - Unable to allocate memory 2\n");
      return -1;
   }

   /* Find the start of the chain, i.e., the timer not linked to by any other */
   for ( j = 0; j < size; j++ ) { 
      linker[j] = 0;
   }
   for ( j = 0; j < size; j++ ) {
      k = timerp[j].link;
      if ( k > (size - 1) ) {
         (void)fprintf(stderr, 
           "verify_timer_links() - link %d out of bound %d at index %d\n", k, size - 1, j);
         return -1;
      }
      if ( k >= 0 )
         linker[k] = 1;
   }
   for ( start = 0; start < size; start++ ) {
      if ( linker[start] == 0 )
         break;
   }   


   for ( j = 0; j < size; j++ ) {
      linkee[j] = 0;
      linker[j] = -1;
   }


   for ( j = 0; j < size; j++ ) {
      linkee[j] = 0;
      linker[j] = -1;
   }
  
   k = start;
   for ( j = 0; j < size; j++ ) {
      k = timerp[k].link;
      if ( k < 0 && j < (size - 1) ) {
         (void)fprintf(stderr, 
           "verify_timerp_links() - premature end of %d length chain at index %d\n", size, j);
         return -1;
      }
      if ( k > (size - 1) ) {
         (void)fprintf(stderr, 
           "verify_timerp_links() - link %d out of bound %d at index %d\n", k, size, j);
         return -1;
      }

      if ( k >= 0 ) {
         linkee[k] += 1;
         linker[k] = j; 
      }
   }

   retcode = 0;
   for ( k = 1; k < size; k++ ) {
      if ( linkee[k] == 0 ) { 
         (void)fprintf(stderr, "timerp[linkee[%d]] is not in chain.\n", k);
         retcode = 1;  
      }
      if ( linkee[k] > 1 ) {
         (void)fprintf(stderr, "timerp[linkee[%d]] is multiply linked.\n", k);
         retcode = 1;
      }
      if ( linkee[k] == -1 ) {
         (void)fprintf(stderr, "End of chain at index %d\n", linker[k]);
         retcode = 1;
      } 
   }

   free( linker );
   free( linkee );

   return retcode;
}


/* String/array manipulation functions */

/*----------------------------------------------------------------------------+
 | Trim leading and trailing whitespace from argument string and return       |
 | pointer to string. Argument string itself is modified.                     |
 +----------------------------------------------------------------------------*/
char *strtrim( char *string )
{
   char *ss ;
   char *sd ;

   ss = sd = string ;
   /* Move pointer to first non-whitespace character */
   while ( *ss == ' ' || *ss == '\t' || *ss == '\n' || *ss == '\r')
      ss++ ;

   /* Close up leading whitespace */
   while ( (*sd++ = *ss++) != '\0' )
      ;

   /* Back up pointer to character before terminating NULL */
   sd -= 2 ;
   /* Replace trailing whitespace with NULLs */
   while( sd >= string
         && ( *sd == ' ' || *sd == '\t' || *sd == '\n' || *sd == '\r' ) )
      *sd-- = '\0' ;

   return string ;
}

/*----------------------------------------------------------------------------+
 | Convert a string to lower case.  Return pointer to string.                 |
 +----------------------------------------------------------------------------*/
char *strlower ( char *string )
{
   char *sp = string ;

   while ( *sp ) {
      *sp = tolower((int)(*sp));
      sp++ ;
   }
   return string;
}

/*----------------------------------------------------------------------------+
 | Convert a string to Upper case.  Return pointer to string.                 |
 +----------------------------------------------------------------------------*/
char *strupper ( char *string )
{
   char *sp = string ;

   while ( *sp ) {
      *sp = toupper((int)(*sp));
      sp++ ;
   }
   return string;
}

/*----------------------------------------------------------------------------+
 | Copy n characters from source to target string and append a trailing null. |
 | Return a pointer to the target.  (Length of target string must be n+1).    |
 +----------------------------------------------------------------------------*/
char *strncpy2 ( char *target, char *source, int n )
{
   char *sp, *tp;
   int  count;

   sp = source; tp = target; count = n;
   while ( count-- > 0 && *sp ) {
      *tp++ = *sp++ ;
   }
   *tp = '\0';

   return target;
}

/*----------------------------------------------------------------------------+
 | Convert high and low ulongs of a ulonglong to a double.                    |
 +----------------------------------------------------------------------------*/
double hilo2dbl ( unsigned long high, unsigned long low )
{
#ifdef HASULL
   unsigned long long ull;

   ull = (unsigned long long)high << 32 | (unsigned long long)low;
   return (double)ull;
#else
   return (double)high * 4294967296.0 + (double)low;
#endif
}


/*--------------------------------------------------------------------+
 | Break up the string 'str' into tokens delimited by characters in   |
 | 'delim' and create the list 'tokv' of pointers to these tokens.    |
 | The original string is modified.  Free 'tokv' after use.           |
 +--------------------------------------------------------------------*/
int tokenize ( char *str, char *delim, int *tokc, char ***tokv )
{
   char *sp, *tp;
   static int sizchptr = sizeof(char *);

   *tokc = 0;
   *tokv = NULL;
   sp = str;
   while ( *sp != '\0' ) {
      /* Bypass leading delimiters */
      while ( *sp != '\0' && strchr(delim, *sp) != NULL )
         sp++;
      if ( *sp == '\0' )
         return 0;
      tp = sp;
      /* Advance to the next delimiter */
      while ( *sp != '\0' && strchr(delim, *sp) == NULL )
         sp++;
      /* Terminate the token if not already at end of string */
      if ( *sp != '\0' )
         *sp++ = '\0';
      /* Allocate space for the pointer */
      if ( *tokc == 0 )
         *tokv = calloc(1, sizchptr);
      else
         *tokv = realloc(*tokv, (*tokc + 1) * sizchptr);
      if ( *tokv == NULL ) {
         fprintf(stderr, "Unable to allocate memory in tokenize()\n");
         exit(1);
      }
      (*tokv)[(*tokc)++] = tp;
   }

   /* Add a terminating NULL */
   if ( *tokc == 0 )
      *tokv = calloc(1, sizchptr);
   else
      *tokv = realloc(*tokv, (*tokc + 1) * sizchptr);
   if ( *tokv == NULL ) {
      fprintf(stderr, "Unable to allocate memory in tokenize()\n");
      exit(1);
   }
   (*tokv)[(*tokc)] = (char *)NULL;
   
   return 0;
}

/*--------------------------------------------------------------------+
 | Copy maxlen-1 characters to target of the token delimited by delim |
 | from the char string *nextpp.  On return, *nextpp points to the    |
 | character following the token (or the part copied thereof).        |
 | The original string is unchanged.                                  |
 +--------------------------------------------------------------------*/
char *get_token( char *target, char **nextpp, char *delim, int maxlen )
{
   char *sp, *tp;
   int  count = 0;

   sp = *nextpp;
   tp = target;

   /* Bypass leading delimiters */
   while ( *sp && strchr(delim, *sp) ) {
      sp++ ;
   }

   /* Transfer up to maxlen-1 characters */
   while ( *sp && !strchr(delim, *sp) && count < maxlen ) {
      *tp++ = *sp++ ;
      count++ ;
   }

   *nextpp = sp;

   /* Terminate with a null character */
   *tp = '\0';

   return target;
}

/*---------------------------------------------------------------------+
 | Left rotate the contents of an array.                               |
 +---------------------------------------------------------------------*/
void lrotarray ( unsigned char *array, int length )
{
   unsigned char hold;
   int           j;

   hold = array[0];
   for ( j = 0; j < length - 1; j++ ) 
      array[j] = array[j + 1];

   array[length - 1] = hold;

   return;
}

/*---------------------------------------------------------------------+
 | Right rotate the contents of an array.                              |
 +---------------------------------------------------------------------*/
void rrotarray ( unsigned char *array, int length )
{
   unsigned char hold;
   int           j;

   hold = array[length - 1];
   for ( j = length - 1; j > 0; j-- ) 
      array[j] = array[j - 1];

   array[0] = hold;

   return;
}


/* X10 Encoding/Decoding functions */

/*---------------------------------------------------------------------+
 | Convert housecode letter to x10 code.                               |
 +---------------------------------------------------------------------*/
unsigned char hc2code ( char hc )
{
   /* X10 codes for housecode letters A through P */
   static unsigned char code[] =
                 {6,14,2,10,1,9,5,13,7,15,3,11,0,8,4,12};

   return code[(toupper((int)hc) - 'A') & 0x0f];
}

/*---------------------------------------------------------------------+
 | Convert x10 code to housecode letter.                               |
 +---------------------------------------------------------------------*/
char code2hc ( unsigned char code )
{
   char *hcode = "MECKOGAINFDLPHBJ";

   return hcode[code & 0x0fu];
}

/*---------------------------------------------------------------------+
 | Convert X10 unit number (1-16) to x10 code.                         |
 +---------------------------------------------------------------------*/
unsigned char unit2code ( int unit )
{
   /* X10 codes for unit 1 through 16 */
   static unsigned char code[] =
                 {6,14,2,10,1,9,5,13,7,15,3,11,0,8,4,12};

   return (unit > 0 && unit < 17) ? code[unit - 1] : 0xff;
}

/*---------------------------------------------------------------------+
 | Convert X10 code to X10 unit number (1-16).                         |
 +---------------------------------------------------------------------*/
int code2unit ( unsigned char code )
{
   static int units[] = {13,5,3,11,15,7,1,9,14,6,4,12,16,8,2,10};

   return units[code & 0x0fu];
}

/*---------------------------------------------------------------------+
 | Convert bit position (0-15) in X10 bitmap to unit number.           |
 +---------------------------------------------------------------------*/
int bitpos2unit ( int bitpos )
{
   static int units[] = {13,5,3,11,15,7,1,9,14,6,4,12,16,8,2,10};
   
   return units[bitpos & 0x0f];
}
   
/*---------------------------------------------------------------------+
 | Return X10 bitmap for Days of Week string.                          |
 +---------------------------------------------------------------------*/
unsigned char dow2bmap ( char *dow )
{
   char *pattern = "smtwtfs";
   unsigned char bmap = 0;
   unsigned char mask = 0x01;
   char buffer[16];
   int  j;

   if ( strlen(dow) != 7 )
      return 0xff;

   strncpy2(buffer, dow, sizeof(buffer) - 1);
   strlower(buffer);

   for ( j = 0; j < 7; j++ ) {
      if ( buffer[j] == pattern[j] ) {
         bmap |= mask;
         mask = mask << 1 ;
      }
      else if ( buffer[j] == '.' )
         mask = mask << 1 ;
      else
         return 0xff ;
   }
   return bmap;
}

/*---------------------------------------------------------+
 | Return Days of Week string for X10 bitmap argument      |
 +---------------------------------------------------------*/
char *bmap2dow( unsigned char bmap )
{
   static char buff[10];
   int    j;
   char   *days = "smtwtfs";
   char   *err  = "-error-";
   unsigned char mask = 0x01;

   if ( bmap > 127 )
      return err;

   for ( j = 0; j < 7; j++ ) {
      buff[j] = (mask & bmap) ? days[j] : '.' ;
      mask = mask << 1;
   }
   buff[7] = '\0';
   return buff;
}

/*---------------------------------------------------------+
 | Return Linux tm_wday (Sun = 0) for X10 bitmap.          |
 | (Assumes only one wday represented in the bitmap.)      |
 +---------------------------------------------------------*/
int bmap2wday ( unsigned char bmap )
{
   int j;

   j = 0;
   while ( (bmap = (bmap >> 1)) != 0 )
      j++;

   return j;
}   

/*---------------------------------------------------------+
 | Return ASCII weekday name for X10 DOW bitmap.           |
 | (Assumes only one wday represented in the bitmap.)      |
 +---------------------------------------------------------*/
char *bmap2ascdow ( unsigned char bmap )
{
   return wday_name[bmap2wday(bmap)];
}   

/*---------------------------------------------------------------------+
 | Reverse the order of the lower 4 bits, e.g., 1 -> 8 and 8 -> 1      |
 | while leaving the upper bits unchanged.                             |
 +---------------------------------------------------------------------*/
unsigned char rev_low_nybble ( unsigned char input ) 
{
   static unsigned char rev_table[] = 
     {0, 8, 4, 12, 2, 10, 6, 14, 1, 9, 5, 13, 3, 11, 7, 15};

   int  j;

   j = (int)(input & 0x0fu);
   
   return (input & 0xf0u) | rev_table[j];
}

/*---------------------------------------------------------------------+
 | Reverse the order of the bits in a byte.                            |
 +---------------------------------------------------------------------*/
unsigned char rev_byte_bits ( unsigned char input )
{
   int           j;
   unsigned char output = 0;

   for ( j = 0; j < 8; j++ ) {
      if ( input & (1 << j) )
         output |= (0x80 >> j);
   }
   return output;
}
  
/*---------------------------------------------------------------------+
 | Rotate the x10 Day of Week bitmap one bit to the left.  This has    |
 | the effect of moving the day of an event into the following day,    |
 | e.g., bmap(".mt..fs") ==> bmap("s.tw..s")                           |
 +---------------------------------------------------------------------*/
unsigned char lrotbmap ( unsigned char bmap )
{
   return  ( ((bmap & 0x40u) >> 6) | ( (bmap & 0x3fu) << 1 ) );
}

/*---------------------------------------------------------------------+
 | Rotate the x10 Day of Week bitmap one bit to the right.  This has   |
 | the effect of moving the day of an event into the preceding day,    |
 | e.g., bmap(".mt..fs") ==> bmap("sm..tf.")                           |
 +---------------------------------------------------------------------*/
unsigned char rrotbmap ( unsigned char bmap )
{
   return  ( ((bmap & 0x01u) << 6) | ( (bmap & 0x7fu) >> 1 ) );
}

/*---------------------------------------------------------------------+
 | Return the code for a unit in a bitmap which is presumed to contain |
 | only a single unit or 0.  If the bitmap contains more than a single |
 | unit, an error value of 0xff is returned.                           |
 +---------------------------------------------------------------------*/
unsigned char single_bmap_unit( unsigned int bitmap )
{
   unsigned int   invmap[] = {6,14,2,10,1,9,5,13,7,15,3,11,0,8,4,12};
   unsigned int   mask = 1;
   unsigned char  units[17], nunits;
   int            j;

   nunits = 0; units[0] = 0;
   for ( j = 0; j < 16; j++ ) {
      mask = 0x01 << invmap[j] ;
      if ( bitmap & mask ) {
         units[nunits++] = invmap[j];
      }
   }
   
   return (nunits <= 1) ? units[0] : 0xff;
} 


/*---------------------------------------------------------------------+
 | Parse the units list and return an X10 unit bitmap containing only  |
 | the first unit in the units list, with unit 0 acceptable.           |
 +---------------------------------------------------------------------*/
unsigned int units2single ( char *str )
{
   static int   umap[] = {6,14,2,10,1,9,5,13,7,15,3,11,0,8,4,12};
   int          unit;
   char         buffer[256];
   char         errmsg[64];
   char         *tail, *sp;

   tail = str;
   if ( *get_token(buffer, &tail, "-,", 255) == '\0' ) 
      return 0;

   unit = (int)strtol(buffer, &sp, 10);
   if ( *sp ) {
      sprintf(errmsg, 
         "Warning: Invalid character '%c' in units list (ignored).\n", *sp);
      store_error_message(errmsg);
      return 0;
   }
   if ( unit < 0 || unit > 16 ) {
      sprintf(errmsg, "Warning: Unit number %d outside range 0-16 (ignored).\n", unit);
      store_error_message(errmsg);
      return 0;
   }

   if ( unit == 0 ) 
      return 0;

   return ( 1 << umap[unit - 1] );
}
   
/*---------------------------------------------------------------------+
 | Parse the flags list and return a long bitmap, with bit 0 = flag 1, |
 | bit 1 = flag 2, etc.                                                |
 +---------------------------------------------------------------------*/
unsigned long flags2longmap ( char *str )
{

   char          buffer[256];
   char          errmsg[80];
   char          *tail, *sp;
   int           flagmax = 32;

   int           flist[32];
   int           j, ustart, flag;
   unsigned long longmap;

   if ( strchr(str, '*') ) {
      longmap = 0xffffffff;
      return longmap;
   }

   for ( j = 0; j < 32; j++ )
      flist[j] = 0;

   ustart = 0; tail = str;
   while ( *(get_token(buffer, &tail, "-,", 255)) ) {
      flag = (int)strtol(buffer, &sp, 10);
      if ( *sp ) {
         sprintf(errmsg, "Invalid char '%c' in flags list.", *sp);
         store_error_message(errmsg);
         return 0;
      }
      if ( flag < 1 || flag > flagmax ) {
         sprintf(errmsg, "Flag number %d outside range 1-%d.", flag, flagmax);
         store_error_message(errmsg);
         return 0;
      }

      if ( *tail == ',' || *tail == '\0' ) {
         if ( ustart ) {
            for ( j = ustart; j <= flag; j++ )
               flist[j-1] = 1;
            ustart = 0;
            continue;
         }
         else {
            flist[flag-1] = 1;
            continue;
         }
      }
      else {
         ustart = flag;
      }
   }

   longmap = 0;
   for ( j = 0; j < 32; j++ )
      longmap |= (flist[j]) << j;

   return longmap;
}



/*---------------------------------------------------------------------+
 | Parse the flags list and return a bitmap, with bit 0 = flag 1,      |
 | bit 1 = flag 2, etc.                                                |
 +---------------------------------------------------------------------*/
unsigned int flags2bmap ( char *str )
{

   char         buffer[256];
   char         errmsg[80];
   char         *tail, *sp;

   int          flist[16];
   int          j, ustart, flag;
   unsigned int bmap;

   if ( strchr(str, '*') ) {
      bmap = 0xffff;
      return bmap;
   }

   for ( j = 0; j < 16; j++ )
      flist[j] = 0;

   ustart = 0; tail = str;
   while ( *(get_token(buffer, &tail, "-,", 255)) ) {
      flag = (int)strtol(buffer, &sp, 10);
      if ( *sp ) {
         sprintf(errmsg, "Invalid char '%c' in flags list.", *sp);
         store_error_message(errmsg);
         return 0;
      }
      if ( flag < 1 || flag > 16 ) {
         sprintf(errmsg, "Flag number %d outside range 1-16.", flag);
         store_error_message(errmsg);
         return 0;
      }

      if ( *tail == ',' || *tail == '\0' ) {
         if ( ustart ) {
            for ( j = ustart; j <= flag; j++ )
               flist[j-1] = 1;
            ustart = 0;
            continue;
         }
         else {
            flist[flag-1] = 1;
            continue;
         }
      }
      else {
         ustart = flag;
      }
   }

   bmap = 0;
   for ( j = 0; j < 16; j++ )
      bmap |= (flist[j]) << j;

   return bmap;
}

/*---------------------------------------------------------------------+
 | Parse the units list and return the X10 unit bitmap.                |
 +---------------------------------------------------------------------*/
unsigned int units2bmap ( char *str )
{

   static int   umap[] = {6,14,2,10,1,9,5,13,7,15,3,11,0,8,4,12};
   char         buffer[256];
   char         errmsg[80];
   char         *tail, *sp;

   int          ulist[16];
   int          j, ustart, unit;
   unsigned int bmap;

   for ( j = 0; j < 16; j++ )
      ulist[j] = 0;

   ustart = 0; tail = str;
   while ( *(get_token(buffer, &tail, "-,", 255)) ) {
      unit = (int)strtol(buffer, &sp, 10);
      if ( *sp ) {
         sprintf(errmsg, "Invalid char '%c' in units list.", *sp);
         store_error_message(errmsg);
         return 0;
      }
      if ( unit < 1 || unit > 16 ) {
         sprintf(errmsg, "Unit number %d outside range 1-16.", unit);
         store_error_message(errmsg);
         return 0;
      }

      if ( *tail == ',' || *tail == '\0' ) {
         if ( ustart ) {
            for ( j = ustart; j <= unit; j++ )
               ulist[j-1] = 1;
            ustart = 0;
            continue;
         }
         else {
            ulist[unit-1] = 1;
            continue;
         }
      }
      else {
         ustart = unit;
      }
   }

   bmap = 0;
   for ( j = 0; j < 16; j++ )
      bmap |= (ulist[j]) << umap[j];

   return bmap;
}

/*---------------------------------------------------------------------+
 | Return string listing X10 units for X10 bitmap argument.            |
 +---------------------------------------------------------------------*/
char *bmap2units ( unsigned int bmap )
{
   static int   invmap[] = {6,14,2,10,1,9,5,13,7,15,3,11,0,8,4,12};
   static char  buffer[128];
   char         minbuf[8];
   unsigned int mask = 0x01;
   int          j, count, nunits;
   int          units[17];
   int          first;

   if ( bmap == 0 ) {
      (void)strncpy2(buffer, "0", sizeof(buffer) - 1);
      return buffer;
   }

   nunits = 0;
   for ( j = 0; j < 16; j++ ) {
      mask = 0x01 << invmap[j] ;
      if ( bmap & mask ) {
         units[nunits++] = j + 1;
      }
   }
   units[nunits++] = 0;

   buffer[0] = buffer[1] = '\0';
   first = units[0];
   count = 1;
   for (j=1; j < nunits; j++) {
      if ( units[j] == units[j-1] + 1) {
         count++;
         continue;
      }

      switch ( count ) {
         case 1 :
            sprintf(minbuf, ",%d", first);
            break;
         case 2 :
            sprintf(minbuf, ",%d,%d", first, units[j-1]);
            break;
         default :
            sprintf(minbuf, ",%d-%d", first, units[j-1]);
            break;
      }

      (void) strncat(buffer, minbuf, sizeof(buffer) - 1 - strlen(buffer));
      first = units[j];
      count = 1;
   }

   return buffer + 1;
}

/*---------------------------------------------------------------------+
 | Return a 16 character ASCII string displaying in descending order   |
 | an X10 unit bitmap, i.e., char[0] -> unit 16, char[15] -> unit 1.   |
 | The argument chrs is a two-character string, the 1st character of   |
 | represents 'unset' units and the 2nd character the 'set' bits.      |
 | Example: With chrs = "01", a bitmap for units 1,5,6                 |
 | (bitmap 0x0242) will be represented as "0000000000110001".          |
 +---------------------------------------------------------------------*/
char *bmap2rasc ( unsigned int bitmap, char *chrs )
{
   int j;
   static char outbuf[17];

   for ( j = 0; j < 16; j++ ) {
      if ( bitmap & (1 << j) )
         outbuf[16 - code2unit(j)] = chrs[1];
      else
         outbuf[16 - code2unit(j)] = chrs[0];
   }
   outbuf[16] = '\0';
   return outbuf;
}

/*---------------------------------------------------------------------+
 | Return a 16 character ASCII string displaying in ascending order    |
 | an X10 unit bitmap, i.e., char[0] -> unit 1, char[15] -> unit 16.   |
 | The argument chrs is a two-character string, the 1st character of   |
 | represents 'unset' units and the 2nd character the 'set' bits.      |
 | Example: With chrs = "01", a bitmap for units 1,5,6                 |
 | (bitmap 0x0242) will be represented as "1000110000000000".          |
 +---------------------------------------------------------------------*/
char *bmap2asc ( unsigned int bitmap, char *chrs )
{
   int j;
   static char outbuf[17];

   for ( j = 0; j < 16; j++ ) {
      if ( bitmap & (1 << j) )
         outbuf[code2unit(j) - 1] = chrs[1];
      else
         outbuf[code2unit(j) - 1] = chrs[0];
   }
   outbuf[16] = '\0';
   return outbuf;
}

/*---------------------------------------------------------------------+
 | Parse the list of integers and return a linear bitmap.  The list    |
 | is comprised of comma-separated positive integers or ranges of      |
 | integers, e.g., 3,2,4-7,11  The integers are restricted to the      |
 | values minval through maxval (within the range 0-31).               |
 +---------------------------------------------------------------------*/
unsigned long list2linmap ( char *str, int minval, int maxval )
{

   char          buffer[256];
   char          errmsg[80];
   char          *tail, *sp;

   int           ulist[32];
   int           j, ustart, unit, temp;
   unsigned long linmap;

   for ( j = 0; j < 32; j++ )
      ulist[j] = 0;

   ustart = -1; tail = str;
   while ( *(get_token(buffer, &tail, "-,", 255)) ) {
      unit = (int)strtol(buffer, &sp, 10);
      if ( *sp ) {
         sprintf(errmsg, "Invalid char '%c' in the list.", *sp);
         store_error_message(errmsg);
         return 0;
      }
      if ( unit < minval || unit > maxval ) {
         sprintf(errmsg, "outside range %d-%d", minval, maxval);
         store_error_message(errmsg);
         return 0;
      }

      if ( *tail == ',' || *tail == '\0' ) {
         if ( ustart >= 0 ) {
            if ( unit < ustart ) {
               temp = ustart;
               ustart = unit;
               unit = temp;
            }
            for ( j = ustart; j <= unit; j++ )
               ulist[j] = 1;
            ustart = 0;
            continue;
         }
         else {
            ulist[unit] = 1;
            continue;
         }
      }
      else {
         ustart = unit;
      }
   }

   linmap = 0;
   for ( j = 0; j < 32; j++ )
      linmap |= (ulist[j]) << j;

   return linmap;
}

/*---------------------------------------------------------------------+
 | Return a comma-separated list of integers and ranges of integers    |
 | represented by the linear bitmap argument, where bit0 -> 0, etc.    |
 +---------------------------------------------------------------------*/
char *linmap2list ( unsigned long linmap )
{
   static char   buffer[128];
   char          minbuf[8];
   unsigned long mask = 1;
   int           j, count, nunits;
   int           units[32];
   int           first;

   buffer[0] = buffer[1] = '\0';
   if ( linmap == 0 ) {
      return buffer;
   }

   nunits = 0;
   for ( j = 0; j < 31; j++ ) {
      mask = 1 << j ;
      if ( linmap & mask ) {
         units[nunits++] = j;
      }
   }
   units[nunits++] = 0;

   first = units[0];
   count = 1;
   for (j = 1; j < nunits; j++) {
      if ( units[j] == units[j-1] + 1) {
         count++;
         continue;
      }

      switch ( count ) {
         case 1 :
            sprintf(minbuf, ",%d", first);
            break;
         case 2 :
            sprintf(minbuf, ",%d,%d", first, units[j-1]);
            break;
         default :
            sprintf(minbuf, ",%d-%d", first, units[j-1]);
            break;
      }

      (void) strncat(buffer, minbuf, sizeof(buffer) - 1 - strlen(buffer));
      first = units[j];
      count = 1;
   }

   return buffer + 1;
}

/* Date and Calendar functions */

/*------------------------------------------------------------------+
 | Determine the user's standard timezone, defined here as the      |
 | offset in seconds of local Standard Time from GMT, with West of  |
 | Greenwich positive, and store in global variable std_tzone.      |
 | Some C libraries provide this directly as a global variable;     |
 | for others it must be determined from struct member tm_gmtoff,   |
 | which provides the offset of local Legal Time and has the        |
 | opposite sign.                                                   |
 +------------------------------------------------------------------*/
#ifdef HASTZ
void get_std_timezone ( void )
{
   struct tm  *tmp;
   time_t     now;

   /* Fill in the tm structure for the current date */
   time(&now);
   tmp = localtime(&now);

   /* The library includes the global variable "timezone" */
   
   std_tzone = (long)timezone;
   return;
}
#else
void get_std_timezone ( void )
{
   struct tm  *tmp;
   time_t     now;
   long int   jan_off, jul_off;

   /* Fill in the tm structure for the current date */
   time(&now);
   tmp = localtime(&now);

   /* struct tm includes the element tm_gmtoff  */

   /* Get the GMT offset for January */
   tmp->tm_mon = 0;
   tmp->tm_mday = 1;
   tmp->tm_hour = 12;
   tmp->tm_min = tmp->tm_sec = 0;
   tmp->tm_isdst = -1;
   mktime(tmp);

   jan_off = tmp->tm_gmtoff;

   /* Get the GMT offset for July */
   tmp->tm_mon = 6;
   tmp->tm_mday = 1;
   tmp->tm_hour = 12;
   tmp->tm_min = tmp->tm_sec = 0;
   tmp->tm_isdst = -1;
   mktime(tmp);

   jul_off = tmp->tm_gmtoff;

   /* The lesser value corresponds to Standard Time.*/
   /* Change sign to make West of Greenwich positive */

   std_tzone = -min(jan_off, jul_off);
   return;
}
#endif   /* End of #ifdef */

/*------------------------------------------------------------------+ 
 | For some places in the world, e.g., Australia, there's no        |  
 | distinction in the current Linux timezone files between TZ names | 
 | for Standard and Daylight time.  We gerry-rig that here, at      |  
 | least for Heyu's purposes.                                       |
 +------------------------------------------------------------------*/
void fix_tznames ( void )
{
   time_t      now;
   extern char *tzname[], *heyu_tzname[];
   static char std[16], dst[16];

   /* Get current date and time */
   time(&now) ;
   (void)localtime(&now);

   (void)strncpy2(std, tzname[0], sizeof(std) - 1);
   (void)strncpy2(dst, tzname[1], sizeof(dst) - 1);
   if ( strcmp(tzname[0], tzname[1]) == 0 )
      (void)strncat(dst, " (DST)", sizeof(dst) - 1 - strlen(dst));
   
   heyu_tzname[0] = std;
   heyu_tzname[1] = dst;
   
   return;
}

/*-----------------------------------------------------------------+
 | Return pointer to string containing "asif" date and time.       |
 +-----------------------------------------------------------------*/
char *asif_time_string ( void )
{
   time_t      now;
   struct tm   *tms;
   static char buffer[32];
   extern char *heyu_tzname[];

   fix_tznames();

   time(&now);
   tms = localtime(&now);

   if ( configp->asif_date > 0 ) {
      tms->tm_year  = (int)(configp->asif_date / 10000L) - 1900;
      tms->tm_mon   = (int)(configp->asif_date % 10000L) / 100 - 1;
      tms->tm_mday  = (int)(configp->asif_date % 100L);
   }
   if ( configp->asif_time >= 0 ) {
      tms->tm_hour  = 0;
      tms->tm_min   = configp->asif_time;
      tms->tm_sec   = 0;
   }
   tms->tm_isdst = -1;
   
   (void)mktime(tms);
    
   (void)sprintf(buffer, "%s %s %02d %4d %02d:%02d:%02d %s",
     wday_name[tms->tm_wday], month_name[tms->tm_mon], tms->tm_mday, tms->tm_year + 1900,
        tms->tm_hour, tms->tm_min, tms->tm_sec, heyu_tzname[tms->tm_isdst]);

   return buffer;
}


/*-----------------------------------------------------------------+
 | Return pointer to string containing current system Legal Time.  |
 +-----------------------------------------------------------------*/
char *legal_time_string ( void )
{
   time_t      now;
   struct tm   *tms;
   static char buffer[32];
   extern char *heyu_tzname[];

   fix_tznames();

   time(&now);
   tms = localtime(&now);

#if 0    
   (void)sprintf(buffer, "%s %s %02d %4d %02d:%02d:%02d %s",
     wday_name[tms->tm_wday], month_name[tms->tm_mon], tms->tm_mday, tms->tm_year + 1900,
        tms->tm_hour, tms->tm_min, tms->tm_sec, heyu_tzname[tms->tm_isdst]);
#endif
   (void)sprintf(buffer, "%s %02d %s %4d %02d:%02d:%02d %s",
     wday_name[tms->tm_wday], tms->tm_mday, month_name[tms->tm_mon], tms->tm_year + 1900,
        tms->tm_hour, tms->tm_min, tms->tm_sec, heyu_tzname[tms->tm_isdst]);


   return buffer;
}

/*-----------------------------------------------------------------+
 | Return pointer to tm structure with local Standard Time.        |
 +-----------------------------------------------------------------*/
struct tm *stdtime( const time_t *timep )
{
   time_t          now;

   /* Set the local timezone variable */

   get_std_timezone();

   now = *timep - (time_t)std_tzone;

   return gmtime(&now);
}

/*-----------------------------------------------------------------+
 | Get count of days for argument date at minutes after 00:00      |
 | time, counted from 1 Jan 1970 00:00 hours Standard time in the  |
 | local timezone.                                                 |
 +-----------------------------------------------------------------*/
long day_count ( int year, int month, int mday, int minutes )
{
   time_t       now;
   struct tm    mytm, *tms;

   tms = &mytm;
   tms->tm_year = year - 1900;
   tms->tm_mon  = month - 1;
   tms->tm_mday = mday;
   tms->tm_hour = 0;
   tms->tm_min  = minutes;
   tms->tm_sec = 0;
   tms->tm_isdst = -1;

   now = mktime(tms);

   return ((long)now - std_tzone) / 86400L;

}

/*-----------------------------------------------------------------+
 | Given the count of days from 1 Jan 1970 at 00:00:00 GMT,        |
 | return the Julian Day corresponding to Noon on that day.        |
 +-----------------------------------------------------------------*/
long int daycount2JD ( long int daycount ) 
{
   return (daycount + 2440588L);
}

/*-----------------------------------------------------------------+
 | Return 1 if the argument year is a leap year; 0 otherwise.      |
 +-----------------------------------------------------------------*/
int isleapyear ( int year ) 
{
   return ((year % 400) == 0) ? 1 :
          ((year % 100) == 0) ? 0 :
          ((year % 4 )  == 0) ? 1 : 0 ;
}

#if 0
/*-----------------------------------------------------------------+
 | Determine elapsed minutes from 0:00 hrs Standard Time on Jan 1  |
 | of the current year until the next NDSTINTV changes between     |
 | Standard/Daylight Time.  Store results in global struct         |
 | dststruct array lgls[] and in struct config.                    |
 +-----------------------------------------------------------------*/
int get_dst_info_old ( int year )
{
   time_t      now, seconds, startsec, jan1sec, jul1sec, delta ;
   struct tm   jultms, *tmjan, *tmjul, *tms;
   int         indx, nintv, val, startval, dstminutes;
   int         iter, result = -1, restart;
   int         offset[2];

   /* Get current date and time */
   time(&now) ;
   tmjan = localtime(&now);

   if ( year >= 1970 )
      tmjan->tm_year = year - 1900;

   /* Get calendar seconds at 0:00 hours Legal Time on Jan 1st of this year */
   tmjan->tm_mon = 0;
   tmjan->tm_mday = 1;
   tmjan->tm_hour = 0;
   tmjan->tm_min = tmjan->tm_sec = 0;
   tmjan->tm_isdst = -1;

   jan1sec = mktime(tmjan);

   tmjul = &jultms;
   memcpy( (void *)tmjul, (void *)tmjan, sizeof(struct tm) );

   /* Calendar seconds at same legal time on July 1st */
   tmjul->tm_mon = 6;
   tmjul->tm_mday = 1;
   tmjul->tm_hour = 0;
   tmjul->tm_min = tmjul->tm_sec = 0;
   tmjul->tm_isdst = -1;

   jul1sec = mktime(tmjul);

   /* Reduce difference by full days of 86400 seconds */
   dstminutes = (int)((jul1sec - jan1sec) % (time_t)86400 / (time_t)60);
   dstminutes = min( dstminutes, 1440 - dstminutes );
   configp->dstminutes = dstminutes;

   offset[0] = 0;
   offset[1] = dstminutes;

   /* Reduce to seconds at 0:00 hours Standard Time */
   jan1sec = ((jan1sec - std_tzone) / (time_t)86400) * (time_t)86400 + std_tzone;

   if ( (val = tmjan->tm_isdst) > 0 ) {
      /* Daylight time in Southern hemisphere */
      configp->isdst = 1;
      indx = 1;
      startval = val;
   }
   else if ( (val = tmjul->tm_isdst) > 0 ) {
      /* Daylight time in Northern hemisphere */
      configp->isdst = 1;
      indx = 0;
      startval = 0;
   }
   else {
      /* Daylight time never in effect */
      configp->isdst = 0;
      for ( nintv = 0; nintv < NDSTINTV; nintv++ )
         lgls[nintv].elapsed = -1;
         stds[nintv].elapsed = -1;
      return 0;
   }

   nintv = 0;
   startsec = jan1sec;
   while ( nintv < NDSTINTV ) {
      iter = 0;
      result = -1;
      restart = 1;
      while ( iter < 1000 && !iter_mgr(result, (long *)(&delta), 30*86400L, &restart) ) {
         iter++;
         seconds = startsec + delta;
         tms = localtime(&seconds);
         result = (tms->tm_isdst == startval) ? -1 : 1 ;
      }
      if ( iter > 999 ) {
         (void) fprintf(stderr, "convergence error in get_dst_info()\n");
         exit(1);
      }

      /* Store as elapsed minutes from 0:00 hours Jan 1 Standard Time */
      /* adjusted for changeover from daylight to standard time and   */
      /* for changeover from standard to daylight time.               */

      lgls[nintv].elapsed = (long)(seconds - jan1sec)/60L + offset[indx];
      lgls[nintv].offset = offset[indx];
      if ( UNDEF_TIME == DST_TIME )
         stds[nintv].elapsed = lgls[nintv].elapsed;
      else 
         stds[nintv].elapsed = (long)(seconds - jan1sec)/60L + dstminutes;

      stds[nintv].offset = offset[indx];
      stdr[nintv].elapsed = (long)(seconds - jan1sec)/60L;
      stdr[nintv].offset = offset[indx]; 
      nintv++;

      indx = (indx + 1) % 2;
      startval = (startval == val) ? 0 : val;
      startsec = seconds + (time_t)86400 ;

   }

   return nintv;
}
#endif

/*-----------------------------------------------------------------+
 | Determine whether DST is in effect on a given yday & minutes    |
 | (measured from midnight) by comparison with the data in struct  |
 | dststruct dsts[], which must previously have been loaded by a   |
 | call to function get_dst_info().  The number of minutes to add  |
 | to Standard to get Legal Time is returned.                      |
 +-----------------------------------------------------------------*/
int isdst_test ( int yday, int minutes )
{
   int   j;
   long  elapsed;

   elapsed = (long)1440 * (long)yday + (long) minutes;

   for ( j = 0; j < NDSTINTV; j++ ) {
      if ( elapsed < lgls[j].elapsed )
         return lgls[j].offset;
   }

   return 0;
}



/*-----------------------------------------------------------------+
 | Return the appropriate time adjustment for periods of Standard  |
 | and Daylight Time.                                              |
 +-----------------------------------------------------------------*/
int time_adjust ( int yday, int minutes, unsigned char mode )
{
   int    j, offset = 0;
   long   elapsed;


   elapsed = (long)1440 * (long)yday + (long) minutes;

   if ( mode == LGL2STD ) {
      for ( j = 0; j < NDSTINTV; j++ ) {
         if ( elapsed < stds[j].elapsed ) {
            offset = stds[j].offset;
            break;
         }
      }
   }
   else {
      for ( j = 0; j < NDSTINTV; j++ ) {
         if ( elapsed < stdr[j].elapsed ) {
            offset = stdr[j].offset;
            break;
         }
      }
   }

   return offset;
}

   
/*---------------------------------------------------------+
 | Display system calendar configuration:                  |
 |   Current date and time.                                |
 |   Timezone.                                             |
 |   Begin and end of Daylight Time for the current year.  |
 +---------------------------------------------------------*/
void display_sys_calendar ( void )
{
   extern  char *heyu_tzname[];

   /* Months beginning with 1 */
   static  char *m_names[] = {
     "", "Jan", "Feb", "Mar", "Apr", "May", "Jun",
         "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
   };


   static  char    *display[] = {
     "Daylight Time begins  yday: %-4d %s %02d %s %4d at %02d:%02d %s\n",
     "Standard Time resumes yday: %-4d %s %02d %s %4d at %02d:%02d %s\n"
   };

   time_t    now;
   struct tm *tms ;
   int       j, k;
   int       month, day, yday, wday, minute, year, year0;
   long      jan1day;

   /* Determine the user's standard timezone */
   get_std_timezone();

   /* Create distinctive TZ names if necessary */
   fix_tznames();

   /* Get current date and time */
   time(&now) ;
   tms = localtime(&now);

   /* Display current date and time */
   (void) printf("\nDate: %s %02d %s %04d    yday: %-3d\n",
        wday_name[tms->tm_wday], tms->tm_mday, month_name[tms->tm_mon],
        1900+tms->tm_year, tms->tm_yday);
   (void) printf("Time: %02d:%02d:%02d  %s\n",
        tms->tm_hour, tms->tm_min, tms->tm_sec,
        heyu_tzname[tms->tm_isdst == 0 ? 0 : 1] );

   year0 = tms->tm_year + 1900;

   /* Display Timezone */
   if ( timezone < 0L )
      (void) printf("Standard Time Zone: %.2f hours East of Greenwich\n",
                   -(double)std_tzone / 3600.);
   else
      (void) printf("Standard Time Zone: %.2f hours West of Greenwich\n",
                    (double)std_tzone / 3600.);

   if ( !get_dst_info(0) ) {
      (void) printf("Daylight time is not in effect at any time during the year.\n");
      return;
   }
   else {
      /* Get the starting day for the year */
      tms->tm_year = year0 - 1900;
      tms->tm_mon = 0;
      tms->tm_mday = 1;
      tms->tm_hour = 0;
      tms->tm_min = tms->tm_sec = 0;
      tms->tm_isdst = -1;
      now = mktime(tms);
      jan1day = (long)((now - std_tzone) / (time_t)86400);

      /* Display the dates in the order they occur during the year. */
      for ( j = 0; j < NDSTINTV; j++ ) {
         if ( lgls[j].elapsed < 0 )
            continue;
         k = lgls[j].offset == 0 ? 0 : 1 ;
         yday = lgls[j].elapsed / 1440;
         minute = lgls[j].elapsed % 1440;
         yday2date( jan1day, yday, &year, &month, &day, &wday );

         (void)printf(display[k], yday, wday_name[wday], day,
               m_names[month], year, minute/60, minute%60, heyu_tzname[k] );
      }
   }
   (void) printf("\n");

   return;
}

/*---------------------------------------------------------------------+
 | Get today's date info and save in CALEND structure.                 |
 +---------------------------------------------------------------------*/
void calendar_today ( CALEND *calendp )
{
   time_t      now;
   struct tm   tmstr, *tmp, *tms;
   int         year, month, mday, minutes;

   if ( !configp->read_flag ) {
      (void) fprintf(stderr,
      "Function calendar_today() says: Configuration file has not yet been read.\n");
      exit(1);
   }

   fix_tznames();

   tmp = &tmstr;

   time(&now);
   tms = localtime(&now);

   calendp->asif_flag = ASIF_NONE;

   if ( configp->asif_date > 0 ) {
      tms->tm_year  = (int)(configp->asif_date / 10000L) - 1900;
      tms->tm_mon   = (int)(configp->asif_date % 10000L) / 100 - 1;
      tms->tm_mday  = (int)(configp->asif_date % 100L);
      calendp->asif_flag |= ASIF_DATE;
   }
   if ( configp->asif_time >= 0 ) {
      tms->tm_hour  = 0;
      tms->tm_min   = configp->asif_time;
      tms->tm_sec   = 0;
      calendp->asif_flag |= ASIF_TIME;
   }
   tms->tm_isdst = -1;
   
   now = mktime(tms);

   calendp->year    = year    = tms->tm_year + 1900;
   calendp->month   = month   = tms->tm_mon + 1;
   calendp->mday    = mday    = tms->tm_mday;
   calendp->minutes = minutes = 60 * tms->tm_hour + tms->tm_min;

   calendp->jan1day = day_count( year, 1, 1, 0 );
   calendp->create_day = day_count( year, month, mday, minutes )
                                - calendp->jan1day; 

   if ( configp->mode == COMPATIBLE ) {
      calendp->today = calendp->jan1day;
      calendp->yday  = 0;
      calendp->day_zero = 0;
   }
   else {
      calendp->today = day_count( year, month, mday, minutes );
      calendp->yday = calendp->today - calendp->jan1day;
      calendp->day_zero = calendp->yday;
   } 

   get_dst_info(year); 

   calendp->valid = 1;

   return;
}


/*---------------------------------------------------------------------+
 | Advance the date info in a CALEND structure by argument ndays.      |
 +---------------------------------------------------------------------*/
void advance_calendar ( CALEND *calendp, int ndays )
{
   struct tm *tms, mytm;

   if ( !calendp || calendp->valid != 1 ) {
      (void) fprintf(stderr,
         "advance_calendar() : No valid existing CALEND structure to advance.\n");
      exit(1);
   }

   tms = &mytm;
   tms->tm_year = calendp->year - 1900;
   tms->tm_mon  = calendp->month - 1;
   tms->tm_mday = calendp->mday + ndays;
   tms->tm_min  = calendp->minutes;
   tms->tm_hour = tms->tm_sec = 0;
   tms->tm_isdst = -1;

   (void) mktime(tms);

   calendp->year    = tms->tm_year + 1900;
   calendp->month   = tms->tm_mon + 1;
   calendp->mday    = tms->tm_mday;
   
   calendp->minutes = (configp->asif_time > 0) ? configp->asif_time :
	                        60 * tms->tm_hour + tms->tm_min;
   calendp->isdst   = tms->tm_isdst;

   calendp->today = day_count( calendp->year, calendp->month,
                         calendp->mday, calendp->minutes);

   calendp->jan1day = day_count( calendp->year, 1, 1, 0);

   calendp->yday    = calendp->today - calendp->jan1day;

   calendp->day_zero = (configp->mode == COMPATIBLE) ?  0 : calendp->yday;

   /* Turn off the asif_date flag */
   calendp->asif_flag &= ~ASIF_DATE;

   return;
}

/* Other functions */

/*---------------------------------------------------------------------+
 | Return string containing full pathname to a Heyu file.              | 
 +---------------------------------------------------------------------*/
char *pathspec ( char *filename ) 
{
   static char full_path[PATH_LEN + 1];

   (void)strncpy2(full_path, heyu_path, sizeof(full_path) - 1);
   (void)strncat(full_path, filename, sizeof(full_path) - 1 - strlen(full_path));
   
   return full_path;
}

/*---------------------------------------------------------------------+
 | Return string containing full pathname to a Heyu report file.       | 
 +---------------------------------------------------------------------*/
char *altpathspec ( char *filename ) 
{
   extern char alt_path[];
   static char full_path[PATH_LEN + 1];

   (void)strncpy2(full_path, alt_path, sizeof(full_path) - 1);
   (void)strncat(full_path, filename, sizeof(full_path) - 1 - strlen(full_path));
   
   return full_path;
}

/*---------------------------------------------------------------------+
 | Search the array of SCENEs for label.  Return the index in the      |
 | array if found, otherwise return -1.                                |
 +---------------------------------------------------------------------*/
int lookup_scene ( SCENE *scenep, char *label )
{
   int j = 0;

   while ( scenep && scenep[j].line_no > 0 ) {
      if ( strcmp(label, scenep[j].label) == 0 ) {
         return j;
      }
      j++;
   }

   return -1;
}




/*---------------------------------------------------------------------+
 | Search the array of ALIAS structures for the argument alias label.  |
 | If found, pass the encoded housecode and unit bitmap back through   |
 | the argument list and return the index in the array.  Otherwise     |
 | return -1.                                                          |
 +---------------------------------------------------------------------*/
int get_alias ( ALIAS *aliasp, char *label, char *hc, unsigned int *bmap )
{
   int  j;

   j = 0;
   while ( aliasp && aliasp[j].line_no > 0 ) {
      if ( !strcmp(aliasp[j].label, label) ) {
         *hc    = aliasp[j].housecode;
         *bmap  = aliasp[j].unitbmap;
         return j;
      }
      j++ ;
   }
   *hc    = '_';
   *bmap  = 0;

   return -1;
}

/*---------------------------------------------------------------------+
 | Search the array of ALIAS structures for multiple instances of      |
 | aliases having the argument housecode and bitmap.  Return each      |
 | alias index as found and update the index pointer to the following  |
 | address.  Return -1 when no more aliases are found.                 |
 +---------------------------------------------------------------------*/
int lookup_alias_mult ( char hc, unsigned int bitmap, int *indxp )
{
   ALIAS *aliasp;
   int   j;

   aliasp = configp->aliasp;
   hc = toupper((int)hc);

   j = *indxp;
   while ( aliasp && aliasp[j].line_no > 0 ) {
      if ( hc == aliasp[j].housecode &&
             bitmap == aliasp[j].unitbmap  ) {
         *indxp = j + 1;
         return j;
      }
      j++;
   }
   return -1;
}

/*---------------------------------------------------------------------+
 | Search the array of ALIAS structures for multiple instances of      |
 | aliases having the argument housecode and bitmap.  Return each      |
 | alias label as found and update the index pointer to the following  |
 | address.  Return NULL when no more aliases are found.               |
 +---------------------------------------------------------------------*/
char *lookup_label_mult ( char hc, unsigned int bitmap, int *indxp )
{
   ALIAS *aliasp;
   int   j;

   aliasp = configp->aliasp;
   hc = toupper((int)hc);

   j = *indxp;
   while ( aliasp && aliasp[j].line_no > 0 ) {
      if ( hc == aliasp[j].housecode &&
             bitmap == aliasp[j].unitbmap  ) {
         *indxp = j + 1;
         return aliasp[j].label;
      }
      j++;
   }
   return (char *)NULL;
}

/*---------------------------------------------------------------------+
 | Search the array of ALIAS structures for multiple instances of      |
 | aliases having the argument housecode and bitmap.  Return each      |
 | alias label as found and update the index pointer to the following  |
 | address.  Return NULL when no more aliases are found.               |
 +---------------------------------------------------------------------*/
char *alias_rev_lookup_mult ( char hc, unsigned int bitmap, int *indxp )
{
   ALIAS *aliasp;
   int   j;

   aliasp = configp->aliasp;
   hc = toupper((int)hc);

   j = *indxp;
   while ( aliasp && aliasp[j].line_no > 0 ) {
      if ( hc == aliasp[j].housecode &&
             bitmap == aliasp[j].unitbmap  ) {
         *indxp = j + 1;
         return aliasp[j].label;
      }
      j++;
   }
   return (char *)NULL;
}

#if 0
/*---------------------------------------------------------------------+
 | Search the array of ALIAS structures for an alias with optflag      |
 | MOPT_SENSOR and having the argument housecode and bitmap.  Return   |
 | the timestamp if found, otherwise 0.                                |
 +---------------------------------------------------------------------*/
long get_sensor_timestamp ( unsigned char hcode, unsigned char ucode )
{
   char         hc;
   unsigned int bitmap;
   int          j;

   if ( !(aliasp->configp->aliasp) )
      return 0;

   hc = hc2code(hcode);
   bitmap = (1 << ucode);

   j = 0;
   while ( aliasp && aliasp[j].line_no > 0 ) {
      if ( hc == aliasp[j].housecode && 
           bitmap == aliasp[j].unitbmap &&
           aliasp[j].optflag & MOPT_SENSOR ) {
         return aliasp[j].timestamp;
      }
      j++;
   }
   return 0;
}
#endif

/*---------------------------------------------------------------------+
 | Search the array of ALIAS structures for an alias having the        |
 | argument housecode and bitmap.  Return the alias label if found,    |
 | otherwise "_no_alias_".                                             |
 +---------------------------------------------------------------------*/
char *lookup_label ( char hc, unsigned int bitmap )
{
   ALIAS *aliasp;
   int   j;

   aliasp = configp->aliasp;
   hc = toupper((int)hc);

   j = 0;
   while ( aliasp && aliasp[j].line_no > 0 ) {
      if ( hc == aliasp[j].housecode && 
           bitmap == aliasp[j].unitbmap  ) {
         return aliasp[j].label;
      }
      j++;
   }
   return "_no_alias_";
}

/*---------------------------------------------------------------------+
 | Search the array of ALIAS structures for an alias having the        |
 | argument housecode, bitmap and ident.  Return the alias index if    |
 | found, otherwise -1.                                                |
 +---------------------------------------------------------------------*/
int alias_rev_index ( char hc, unsigned int bitmap, unsigned char vtype, unsigned long ident )
{
   ALIAS *aliasp;
   int   j, k;
   unsigned long mask;

   aliasp = configp->aliasp;
   hc = toupper((int)hc);

   mask = (vtype == RF_SEC) ? configp->securid_mask : 0xffffffffu;

   ident &= mask;

   j = 0;
   while ( aliasp && aliasp[j].line_no > 0 ) {
      if ( hc == aliasp[j].housecode &&
           bitmap == aliasp[j].unitbmap &&
           vtype == aliasp[j].vtype ) {
         if ( vtype == 0 )
            return j;
         for ( k = 0; k < aliasp[j].nident; k++ ) {
            if ( ident == (aliasp[j].ident[k] & mask) ) {
               return j;
            }
         }
      }
      j++;
   }
   return -1;
}

/*---------------------------------------------------------------------+
 | Search the array of ALIAS structures for an alias having the        |
 | argument housecode and bitmap.  Return the alias label if found,    |
 | otherwise "_no_alias_".                                             |
 +---------------------------------------------------------------------*/
char *alias_rev_lookup ( char hc, unsigned int bitmap )
{
   ALIAS *aliasp;
   int   j;

   aliasp = configp->aliasp;
   hc = toupper((int)hc);

   j = 0;
   while ( aliasp && aliasp[j].line_no > 0 ) {
      if ( hc == aliasp[j].housecode && 
           bitmap == aliasp[j].unitbmap  ) {
         return aliasp[j].label;
      }
      j++;
   }
   return "_no_alias_";
}

/*---------------------------------------------------------------------+
 | Search the array of ALIAS structures for the argument alias label.  |
 | If found, pass the encoded housecode and unit bitmap back through   |
 | the argument list and return the index in the array.  Otherwise     |
 | return -1.                                                          |
 +---------------------------------------------------------------------*/
int alias_lookup ( char *label, char *hc, unsigned int *bmap )
{
   return get_alias( configp->aliasp, label, hc, bmap );
}   

/*---------------------------------------------------------------------+
 | Return a count of the total number of defined aliases.              |
 +---------------------------------------------------------------------*/
int alias_count ( void )
{
   ALIAS *aliasp;
   int   count = 0;

   aliasp = configp->aliasp;

   while ( aliasp && aliasp[count].line_no > 0 )
      count++;

   return count; 
}   

/*---------------------------------------------------------------------+
 | Return a count of the total number of defined RFXSensors.           |
 +---------------------------------------------------------------------*/
int rfxsensor_count ( void )
{
   ALIAS *aliasp;
   int   j, count = 0;

   aliasp = configp->aliasp;

   j = 0;
   while ( aliasp && aliasp[j].line_no > 0 ) {
      if ( aliasp[j].vtype == RF_XSENSOR )
         count++;
      j++;
   }

   return count; 
}   

/*---------------------------------------------------------------------+
 | Return a count of the total number of defined RFXMeters.            |
 +---------------------------------------------------------------------*/
int rfxmeter_count ( void )
{
   ALIAS *aliasp;
   int   j, count = 0;

   aliasp = configp->aliasp;

   j = 0;
   while ( aliasp && aliasp[j].line_no > 0 ) {
      if ( aliasp[j].vtype == RF_XMETER )
         count++;
      j++;
   }

   return count; 
}   

/*---------------------------------------------------------------------+
 | Return a count of aliases with a specific vtype                     |
 +---------------------------------------------------------------------*/
int alias_vtype_count ( unsigned char vtype )
{
   ALIAS *aliasp;
   int   j, count = 0;

   aliasp = configp->aliasp;

   j = 0;
   while ( aliasp && aliasp[j].line_no > 0 ) {
      if ( aliasp[j].vtype == vtype )
         count++;
      j++;
   }

   return count; 
}   


/*---------------------------------------------------------------------+
 | Return the macro execution time of the earliest event whose time    |
 | is defined by "now+NN" in the schedule file.  The time includes the |
 | macro delay time, if any, and is in minutes after 00:00 Legal Time. |
 | If no such event defined, return -1                                 |
 | (This function must be called before the input time has been        |
 | converted from Legal to Standard Time.)                             |
 +---------------------------------------------------------------------*/
int get_first_now_time ( TEVENT *teventp )
{
   int  j;
   int  exec_time, earliest;

   j = 0;
   earliest = 9999;
   while ( teventp && teventp[j].line_no > 0 ) {
      if ( teventp[j].generation == current_tevent_generation &&
           (teventp[j].flag & NOW_EVENT) )  {
         exec_time = teventp[j].offset + teventp[j].delay;
         earliest = min(earliest, exec_time);
      }
      j++;
   }
   if ( earliest < 9999 )
      return earliest;

   return -1;
}


/*---------------------------------------------------------------------+
 | Interpret the time token from a timer command as either the         |
 | clock time in minutes or the offset from dawn or dusk in minutes.   |
 | Return a flag indicating whether Clock or Dawn or Dusk relative.    |
 | The time token may also be "now[+offset]" for quick testing.        |
 +---------------------------------------------------------------------*/
int parse_time_token ( char *str, int *stime )
{
   int       hour, min, flag = 0;
   char      buf[64];
   char      *sp;
   struct tm *tms;
   time_t    now;

   (void) strncpy2(buf, str, 63);
   (void) strlower(strtrim(buf));

   if ( *(sp = buf + strlen(buf) - 1) == 's' ) {
      flag |= SEC_EVENT;
      *sp = '\0';
   }

   if ( !strncmp(buf, "dawn", 4) ) {
      flag |= DAWN_EVENT ;
      *stime = (*(buf+4)) ? (int)strtol(buf+4, NULL, 10) : 0 ;
   }
   else if ( !strncmp(buf, "dusk", 4) ) {
      flag |= DUSK_EVENT ;
      *stime = (*(buf+4)) ? (int)strtol(buf+4, NULL, 10) : 0 ;
   }
   else if ( !strncmp(buf, "now", 3) ) {
      /* (For quick testing timers) */
      *stime = (*(buf+3)) ? (int)strtol(buf+3, NULL, 10) : 0 ;
      if ( *stime < 0 || flag & SEC_EVENT ) {
         flag = INVALID_EVENT;
         return flag;
      }
      time(&now);
      tms = localtime(&now);
      /* Round up, allowing at least 15 seconds for uploading */
      *stime += 60 * tms->tm_hour + tms->tm_min + (tms->tm_sec + 75)/60;
      if ( *stime < 0 || *stime > 1439 ) {
         flag = INVALID_EVENT;
         return flag;
      }
      else
         flag |= (CLOCK_EVENT | NOW_EVENT);
   }      
   else if ( (sp = strchr(buf, ':')) != NULL ) {
      min = (int)strtol(sp+1, NULL, 10);
      *sp = '\0';
      hour = ( *buf ) ? (int)strtol(buf, NULL, 10) : 0 ;
      *stime = 60 * hour + min ;
      if ( *stime < 0 || *stime > 1439 )
         flag = INVALID_EVENT;
      else
         flag |= CLOCK_EVENT;
   }
   else {
      flag = INVALID_EVENT ;
   }

   return flag;
}


/*-----------------------------------------------------------+
 | Return the index of a child tevent which is a duplicate   |
 | of the parent except that it's one generation higher and  |
 | the links of both parent and child are updated to insert  |
 | the child into the linked list after the parent.          |
 +-----------------------------------------------------------*/
int spawn_child_tevent ( TEVENT **teventpp, int parent_index )
{

   int        cindx;

   /* Get a new tevent */

   cindx = tevent_index( teventpp );

   /* Copy all fields from the parent tevent */
   (void) memcpy( (void *)(*teventpp + cindx),
            (void *)(*teventpp + parent_index), sizeof(TEVENT) );

   /* Reset the flags defined as "don't copy" */
   (*teventpp)[cindx].flag &= ~(NOCOPY_EVENTS);

   /* Set the new generation */
   increment_tevent_generation( *teventpp, cindx );

   /* Insert the child into the linked list */
   (*teventpp)[cindx].link = (*teventpp)[parent_index].link;
   (*teventpp)[parent_index].link = cindx;

   /* Set the reverse link */
   (*teventpp)[cindx].plink = parent_index;

   return cindx;
}

/*-----------------------------------------------------------+
 | Return the index of a child timer which is a duplicate    |
 | of the parent except that it's one generation higher and  |
 | the links of both parent and child are updated to insert  |
 | the child into the linked list after the parent.          |
 +-----------------------------------------------------------*/
int spawn_child_timer ( TIMER **timerpp, int parent_index )
{

   int        cindx;

   /* Get a new timer */

   cindx = timer_index( timerpp );

   /* Copy all fields from the parent timer */
   (void) memcpy( (void *)(*timerpp + cindx),
            (void *)(*timerpp + parent_index), sizeof(TIMER) );

   /* Set the new generation */
   increment_timer_generation( *timerpp, cindx );

   /* Insert the child into the linked list */
   (*timerpp)[cindx].link = (*timerpp)[parent_index].link;
   (*timerpp)[parent_index].link = cindx;

   return cindx;
}


/*---------------------------------------------------------------------+
 | Return next available index in array of TIMER structures            |
 +---------------------------------------------------------------------*/
int timer_index ( TIMER **timerpp )
{
   extern int timer_size, timer_maxsize ;
   int        j = 0;
   int        blksize = 20;
   static int siztimer = sizeof(TIMER);

   /* Allocate initial block of memory */
   if ( *timerpp == NULL ) {
      *timerpp = calloc(blksize, siztimer );
      if ( *timerpp == NULL ) {
         (void) fprintf(stderr, "Unable to allocate memory for Timer.\n");
         exit(1);
      }
      timer_maxsize = blksize;
      timer_size = 0;
      for ( j = 0; j < timer_maxsize; j++ ) {
         (*timerpp)[j].line_no = -1 ;
         (*timerpp)[j].link = j + 1;
         (*timerpp)[j].cancel = 0;
      }
   }

   /* Check to see if there's an available location          */
   /* If not, increase the size of the memory allocation.    */
   /* (Always leave room for a final termination indicator.) */
   if ( timer_size == (timer_maxsize - 1)) {
      timer_maxsize += blksize ;
      *timerpp = realloc(*timerpp, timer_maxsize * siztimer );
      if ( *timerpp == NULL ) {
         (void) fprintf(stderr, "Unable to increase size of Timer list.\n");
         exit(1);
      }

      /* Initialize the new memory allocation */
      for ( j = timer_size; j < timer_maxsize; j++ ) {
         (*timerpp)[j].line_no = -1;
         (*timerpp)[j].link = j + 1;
         (*timerpp)[j].cancel = 0;
      }
   }

   j = timer_size;
   timer_size += 1;

   return j;
}

/*---------------------------------------------------------------------+
 | Return next available index in array of TEVENT structures           |
 +---------------------------------------------------------------------*/
int tevent_index ( TEVENT **teventpp )
{
   extern int tevent_size, tevent_maxsize ;
   int        j;
   int        blksize = 20;
   static int siztevent = sizeof(TEVENT);

   /* Allocate initial block of memory */
   if ( *teventpp == NULL ) {
      *teventpp = calloc(blksize, siztevent );
      if ( *teventpp == NULL ) {
         (void) fprintf(stderr, "Unable to allocate memory for tevent.\n");
         exit(1);
      }
      tevent_maxsize = blksize;
      tevent_size = 0;
      for ( j = 0; j < tevent_maxsize; j++ ) {
         (*teventpp)[j].line_no = -1 ;
         (*teventpp)[j].combined = 0;
         (*teventpp)[j].plink = -1;
         (*teventpp)[j].chain_len = 1;
         (*teventpp)[j].link = j + 1;
      }
   }

   /* Check to see if there's an available location          */
   /* If not, increase the size of the memory allocation.    */
   /* (Always leave room for a final termination indicator.) */
   if ( tevent_size == (tevent_maxsize - 1)) {
      tevent_maxsize += blksize ;
      *teventpp = realloc(*teventpp, tevent_maxsize * siztevent );
      if ( *teventpp == NULL ) {
         (void) fprintf(stderr, "Unable to increase size of tevent list.\n");
         exit(1);
      }

      /* Initialize the new memory allocation */
      for ( j = tevent_size; j < tevent_maxsize; j++ ) {
         (*teventpp)[j].line_no = -1;
         (*teventpp)[j].combined = 0;
         (*teventpp)[j].plink = -1;
         (*teventpp)[j].chain_len = 1;
         (*teventpp)[j].link = j + 1;
      }
   }

   j = tevent_size;
   tevent_size += 1;

   return j;
}

/*---------------------------------------------------------------------+
 | Save the state of TIMERs and TEVENTs                                |
 +---------------------------------------------------------------------*/
void save_state ( TIMER *timerp, TEVENT *teventp )
{
   extern int timer_size, timer_savesize, save_timer_generation;
   extern int tevent_size, tevent_savesize, save_tevent_generation;

   if ( timerp ) {
       timer_savesize = timer_size;
       save_timer_generation = current_timer_generation;
   }

   if ( teventp ) {
      tevent_savesize = tevent_size;
      save_tevent_generation = current_tevent_generation;
   }

   return;
}   

/*---------------------------------------------------------------------+
 | Save the initial TIMER configuration                                |
 +---------------------------------------------------------------------*/
void save_timer_config ( TIMER *timerp )
{
    extern int timer_size, timer_savesize, save_timer_generation;

    if ( timerp ) {
       timer_savesize = timer_size;
       save_timer_generation = current_timer_generation;
    }

    return;
 }

/*---------------------------------------------------------------------+
 | Save the initial TEVENT configuration                               |
 +---------------------------------------------------------------------*/
void save_tevent_config ( TEVENT *teventp )
{
   extern int tevent_size, tevent_savesize, save_tevent_generation;

   if ( teventp ) {
      tevent_savesize = tevent_size;
      save_tevent_generation = current_tevent_generation;
   }

   return;
}

/*---------------------------------------------------------------------+
 | Restore TIMER and TEVENT configuration to saved state.              |
 +---------------------------------------------------------------------*/
void restore_state ( TIMER *timerp, TEVENT *teventp )
{
   extern int timer_size, timer_savesize, timer_maxsize;
   extern int save_timer_generation;
   extern int tevent_size, tevent_savesize, tevent_maxsize;
   extern int save_tevent_generation;
   int        j;

   if ( timerp ) {
      timer_size = timer_savesize;
      for ( j = timer_size; j < timer_maxsize; j++ ) {
         timerp[j].line_no = -1;
         timerp[j].link = j + 1;
      }
      current_timer_generation = save_timer_generation;
   }

   if ( teventp ) {
      tevent_size = tevent_savesize;
      for ( j = tevent_size; j < tevent_maxsize; j++ ) {
         teventp[j].line_no = -1;
         teventp[j].link = j + 1;
         teventp[j].trig = -1;
      }
      current_tevent_generation = save_tevent_generation;
   }
   return;
}

/*---------------------------------------------------------------------+
 | Restore TIMER configuration to saved state.                         |
 +---------------------------------------------------------------------*/
void restore_timer_config ( TIMER *timerp )
{
    extern int timer_size, timer_savesize, timer_maxsize;
    extern int save_timer_generation;
    int        j;

    if ( timerp ) {
       timer_size = timer_savesize;
       for ( j = timer_size; j < timer_maxsize; j++ ) {
          timerp[j].line_no = -1;
          timerp[j].link = j + 1;
       }
       current_timer_generation = save_timer_generation;
    }
    return ;
}

/*---------------------------------------------------------------------+
 | Restore TEVENT configuration to saved state.                        |
 +---------------------------------------------------------------------*/
void restore_tevent_config ( TEVENT *teventp )
{
   extern int tevent_size, tevent_savesize, tevent_maxsize;
   extern int save_tevent_generation;
   int        j;

   if ( teventp ) {
      tevent_size = tevent_savesize;
      for ( j = tevent_size; j < tevent_maxsize; j++ ) {
         teventp[j].line_no = -1;
         teventp[j].link = j + 1;
         teventp[j].trig = -1;
      }
      current_tevent_generation = save_tevent_generation;
   }
   return;
}

/*---------------------------------------------------------------------+
 | Update the timer generation counter and return the new generation.  |
 +---------------------------------------------------------------------*/
int update_current_timer_generation ( void )
{
   extern int current_timer_generation, timer_generation_delta;

   current_timer_generation += timer_generation_delta;
   timer_generation_delta = 0;

   return current_timer_generation;
}

/*---------------------------------------------------------------------+
 | Update the tevent generation counter and return the new generation. |
 +---------------------------------------------------------------------*/
int update_current_tevent_generation ( void )
{
   extern int current_tevent_generation, tevent_generation_delta;

   current_tevent_generation += tevent_generation_delta;
   tevent_generation_delta = 0;

   return current_tevent_generation;
}

/*---------------------------------------------------------------------+
 | Increment the current tevent generation by at least one.            | 
 +---------------------------------------------------------------------*/
int update_current_tevent_generation_anyway ( void )
{
   extern int tevent_generation_delta;

   if ( tevent_generation_delta == 0 )
      tevent_generation_delta = 1;

   return update_current_tevent_generation();
}

/*---------------------------------------------------------------------+
 | Increment the generation of a tevent (without changing the current  |
 | generation).                                                        |
 +---------------------------------------------------------------------*/
void increment_tevent_generation ( TEVENT *teventp, int index )
{
   extern int tevent_generation_delta;

   teventp[index].generation += 1;
   tevent_generation_delta = 1;

   return;
}

/*---------------------------------------------------------------------+
 | Increment the generation of a timer (without changing the current   |
 | generation).                                                        |
 +---------------------------------------------------------------------*/
void increment_timer_generation ( TIMER *timerp, int index )
{
   extern int timer_generation_delta;

   if ( timerp )
      timerp[index].generation += 1;
   
   timer_generation_delta = 1;

   return;
}

/*---------------------------------------------------------------------+
 | Return next available index in array of TRIGGER structures          |
 +---------------------------------------------------------------------*/
int trigger_index ( TRIGGER **triggerpp )
{
   static int size, max_size ;
   int   j;
   int   blksize = 5;
   static int strucsize = sizeof(TRIGGER);

   /* Allocate initial block of memory */
   if ( *triggerpp == NULL ) {
      *triggerpp = calloc(blksize, strucsize );
      if ( *triggerpp == NULL ) {
         (void) fprintf(stderr, "Unable to allocate memory for Trigger.\n");
         exit(1);
      }
      max_size = blksize;
      size = 0;
      for ( j = 0; j < max_size; j++ ) {
         (*triggerpp)[j].line_no = -1 ;
      }
   }

   /* Check to see if there's an available location          */
   /* If not, increase the size of the memory allocation.    */
   /* (Always leave room for a final termination indicator.) */
   if ( size == (max_size - 1)) {
      max_size += blksize ;
      *triggerpp = realloc(*triggerpp, max_size * strucsize );
      if ( *triggerpp == NULL ) {
         (void) fprintf(stderr, "Unable to increase size of Trigger list.\n");
         exit(1);
      }

      /* Initialize the new memory allocation */
      for ( j = size; j < max_size; j++ ) {
         (*triggerpp)[j].line_no = -1;
      }
   }

   j = size;
   size += 1;

   return j;
}


/*---------------------------------------------------------------------+
 | Return an index to the list of macro elements with sufficient       |
 | room for the argument number of bytes.                              |
 +---------------------------------------------------------------------*/
int macro_element_index ( unsigned char **elementpp, int nbytes )
{
   static int size, max_size ;
   static int ucsize = sizeof(unsigned char);
   int   j;
   int   blksize = 50;   /* Must be at least greater than the largest */
                          /* sized macro element (currently 6 bytes).  */

   /* Allocate initial block of memory to begin with */
   if ( *elementpp == NULL ) {
      max_size = max(blksize, nbytes);
      *elementpp = calloc(max_size, ucsize );
      if ( *elementpp == NULL ) {
         (void) fprintf(stderr, "Unable to allocate memory for macro element.\n");
         exit(1);
      }
      size = 0;
      for ( j = 0; j < max_size; j++ ) {
         (*elementpp)[j] = 0 ;
      }
   }

   /* Check to see if there's an available location with enough room. */
   /* If not, increase the size of the memory allocation.             */
   if ( (size + nbytes) > max_size ) {
      max_size += max(blksize, nbytes) ;
      *elementpp = realloc(*elementpp, (max_size * ucsize) );
      if ( *elementpp == NULL ) {
         (void) fprintf(stderr, "Unable to increase size of macro element list.\n");
         exit(1);
      }
      for ( j = size; j < max_size; j++ ) {
         (*elementpp)[j] = 0 ;
      }
   }

   j = size;
   size += nbytes;

   return j;
}

/*---------------------------------------------------------------------+
 | Return the index in array MACRO to the argument macro_name.         |
 | If not found, create the entry.                                     |
 | Also mark the flag in MACRO with the type of command which          |
 | referenced or defined it, i.e., timer, trigger, or macro.           |
 | If defined more than once by a macro command, mark it as a          |
 | duplicate.                                                          |
 +---------------------------------------------------------------------*/
int macro_index ( MACRO **macropp, char *macro_label,
                                                   unsigned char refer )
{
   static int size, max_size ;
   int    j;
   int    blksize = 10;
   static int sizmacro = sizeof(MACRO);

   /* Allocate initial block of memory */
   if ( *macropp == NULL ) {
      *macropp = calloc(blksize, sizmacro );
      if ( *macropp == NULL ) {
         (void) fprintf(stderr,
            "Unable to allocate memory for Macro list.\n");
         exit(1);
      }
      max_size = blksize;
      size = 0;
      for ( j = 0; j < max_size; j++ ) {
         (*macropp)[j].line_no = -1;
         (*macropp)[j].refer = 0 ;
         (*macropp)[j].label[0] = '\0' ;
         (*macropp)[j].use = USED ;
         (*macropp)[j].modflag = UNMODIFIED;
         (*macropp)[j].isnull = 0;
         (*macropp)[j].trig = -1;
         (*macropp)[j].nelem = 0;
         (*macropp)[j].total = 0;
      }
      /* Make the first macro in the list the null macro */
      /* and give it the label "null".                     */
      (*macropp)[0].line_no = 9999;
      (*macropp)[0].isnull = 1;
      (*macropp)[0].refer = MACRO_PARSER;
      (void) strncpy2((*macropp)[0].label, "null", MACRO_LEN);
      size = 1;
   }

   /* See if the macro name is already in the list.      */
   /* If so, add parser's identity and return its index. */
   /* However if this and the existing entry were made   */
   /* by the macro parser, also flag it as a duplicate.  */

   for ( j = 0; j < size; j++ ) {
      if ( !strcmp(macro_label, (*macropp)[j].label) ) {
         if ( (*macropp)[j].refer & (refer & MACRO_PARSER) )
            refer |= MACRO_DUPLICATE ;
         (*macropp)[j].refer |= refer ;
         return j;
      }
   }

   /* Check to see if there's room for the new macro name. */
   /* If not, increase the size of the memory allocation. */
   /* (Always leave room for a final terminating NULL.)   */

   if ( size == (max_size - 1)) {
      max_size += blksize ;
      *macropp = realloc(*macropp, max_size * sizmacro );
      if ( *macropp == NULL ) {
         (void) fprintf(stderr, "Unable to increase size of Macro list.\n");
         exit(1);
      }

      /* Initialize the new memory allocation */
      for ( j = size; j < max_size; j++ ) {
         (*macropp)[j].line_no = -1;
         (*macropp)[j].refer = 0;
         (*macropp)[j].label[0] = '\0' ;
         (*macropp)[j].use = USED ;
         (*macropp)[j].modflag = UNMODIFIED;
         (*macropp)[j].isnull = 0;
         (*macropp)[j].trig = -1;
         (*macropp)[j].nelem = 0;
         (*macropp)[j].total = 0;
      }
   }

   /* Now add the new macro label, mark caller's identity in flag, */
   /* and return the index to its position in the list */

   j = size ;
   (void) strncpy2((*macropp)[j].label, macro_label, MACRO_LEN);
   (*macropp)[j].refer |= refer ;
   size += 1 ;

   return j ;
}

/*---------------------------------------------------------------------+
 | Return next available index in array of intervals.                  |
 +---------------------------------------------------------------------*/
int intv_index ( int **intvp , unsigned int *size )
{
   static int max_size ;
   static int sizeint = sizeof(int);
   int   j;
   int   blksize = 100;

   /* Allocate initial block of memory */
   if ( *intvp == NULL ) {
      *intvp = calloc(blksize, sizeint );
      if ( *intvp == NULL ) {
         (void) fprintf(stderr,
            "Unable to allocate memory for intervals list.\n");
         exit(1);
      }
      max_size = blksize;
      *size = 0;
   }

   /* Check to see if there's an available location          */
   /* If not, increase the size of the memory allocation.    */
   /* (Always leave room for a final termination indicator.) */
   if ( *size == (unsigned int)(max_size - 2)) {
      max_size += blksize ;
      *intvp = realloc(*intvp, max_size * sizeint );
      if ( *intvp == NULL ) {
         (void) fprintf(stderr,
            "Unable to increase size of intervals list.\n");
         exit(1);
      }
   }

   j = *size;
   *size += 1;

   return j;
}

/*---------------------------------------------------------+
 | Trace a substitute event to the current generation.     |
 +---------------------------------------------------------*/
int find_substitute ( TEVENT *teventp, int subindex )
{
   int  j;

   j = subindex;
   while ( teventp[j].generation < (unsigned char)current_tevent_generation )
      j = teventp[j].link;

   return j;
}


/*---------------------------------------------------------+
 | Fix the begin or end date for February if necessary.    |
 +---------------------------------------------------------*/
void fix_february ( int dsbegin, int dsend, int year, 
                                  int *dbeginp, int *dendp ) 
{
   /* Pass back dates as-is in the event there's no change */
   *dbeginp = dsbegin;
   *dendp   = dsend;

   /* If the user has set FEB_KLUGE=YES in the x10config      */
   /* file, interpret either Feb 28 or Feb 29 as the last day */
   /* of that month.                                          */
   if ( configp->feb_kluge == YES ) {    
      /* Fix end of February date for the year */
      if ( dsbegin == 228 || dsbegin == 229 )
         *dbeginp = isleapyear(year) ? 229 : 228 ;
      if ( dsend   == 228 || dsend   == 229 )
         *dendp   = isleapyear(year) ? 229 : 228 ;

      /* Same thing, if the user has specified reverse dates. */
      /* ( parse_sched() will have added 12 months to the end */
      /* date, to push it into the following year.)           */

      if ( dsend   == 1428 || dsend == 1429 )
         *dendp   = isleapyear(year + 1) ? 1429 : 1428;
   }

   /* Fix Feb 30th for the year, regardless */
   if ( dsbegin == 230 )
      *dbeginp = isleapyear(year) ? 229 : 228;
   if ( dsend   == 230 )
      *dendp   = isleapyear(year) ? 229 : 228;

   /* Same thing, if user has specified a reversed date range */
   if ( dsend   == 1430 )
      *dendp   = isleapyear(year + 1) ? 1429 : 1428;

   return;
}

/*---------------------------------------------------------+
 | Determine new tevent begin/end dates, filtering the     |
 | date ranges specified in the x10sched file by the       |
 | interval of N days (1-366) beginning today for which    |
 | the CM11a is to be programmed.                          |
 +---------------------------------------------------------*/
void resolve_dates ( TEVENT **teventpp, CALEND *calendp, int ndays )
{
   extern int    current_tevent_generation;
   int           j, k, cindx, today, year, day_min, day_max;
   int           offset, minutes, dayminute, delay, sec_adjust, sec_nomin;
   int           dsbegin, dsend, dbegin, dend, beg, end;
   int           notify, valb, vald, size;
   int           day_zero, arrmax, day_adj;
   int           bday[32], eday[32], keep[32], shday[32];

   unsigned char dow_bmap, bmap;
   unsigned int  flag;

   unsigned char breaker[734]; /* 366 + 366 + 2 */
   int           dst[734];

   /* Nothing to do if no tevents */
   if ( !(*teventpp) )
      return;

   if ( verbose )
      (void)printf("Entering resolve_dates() at generation %d\n",
                              current_tevent_generation);

   year       = calendp->year;
   today      = calendp->yday;
   day_zero   = calendp->day_zero;

   arrmax    = (isleapyear(year) ? 366 : 365) +
               (isleapyear(year + 1) ? 366 : 365) + 1; 
 
   j = 0;
   while ( (*teventpp)[j].line_no > 0 ) {
      if ( (*teventpp)[j].generation != current_tevent_generation ) {
         j++;
         continue;
      }

      /* Get dates (which are stored as mmdd = 100*mm + dd) */
      dsbegin    = (*teventpp)[j].sched_beg;
      dsend      = (*teventpp)[j].sched_end;

      flag       = (*teventpp)[j].flag;
      dow_bmap   = (*teventpp)[j].dow_bmap;
      notify     = min((*teventpp)[j].notify, ndays);
      sec_adjust = (*teventpp)[j].security;
      delay      = (*teventpp)[j].delay;
      minutes    = (*teventpp)[j].offset;

      day_adj = 0;

      /* Adjustment for security mode if necessary */
      if ( flag & CLOCK_EVENT ) {
         minutes = minutes - sec_adjust;
         if ( minutes < 0 ) {
            /* Event occurs on the previous day */
            minutes += 1440;
            day_adj = -1;
            dow_bmap = rrotbmap( dow_bmap );
         }
         if ( minutes > 1439 ) {
            /* Event occurs on the following day */
            minutes -= 1440;
            day_adj = 1;
            dow_bmap = lrotbmap( dow_bmap );
         }
      }
      else {
         minutes = (*teventpp)[j].offset - sec_adjust;
      }
      
      for ( k = 0; k < 32; k++ )
         shday[k] = day_adj;

      
      /* Initialize breaker and DST breaker arrays */
      for ( k = 0; k < 734; k++ ) {
         breaker[k] = 0;
         dst[k] = 0;
      }
 
      /* Determine the day numbering as of Noon */
      dayminute = 12 * 60;

      /* Fix February dates for this year if necessary */
      fix_february( dsbegin, dsend, year, &dbegin, &dend );

      /* Get day counts from Jan 1 of this year, offsetting by 1 day so we */
      /* can keep track of events which are shifted into the previous day. */

      beg = 1 + day_count( year, dbegin/100, dbegin % 100, dayminute ) - calendp->jan1day;
      end = 1 + day_count( year, dend/100, dend % 100, dayminute ) - calendp->jan1day;

      /* Limit the upper end */
      end = min(end, arrmax - 1);
 
      if ( beg < 0 || end < 0 || beg > arrmax - 1 || end > arrmax - 1 ) {
         (void)fprintf(stderr, 
           "Line %2d-%d: resolve_dates(): Begin/End Error 1: Begin = %d, End = %d\n",
           (*teventpp)[j].line_no, (*teventpp)[j].pos, beg, end);
         exit(1);
      }

      for ( k = beg + day_adj; k <= end + day_adj; k++ ) {	      
         breaker[k] = 1;
      }


      /* If the user has specified reverse dates, account for the */
      /* beginning of year interval for this year carried over    */
      /* from last year.                                          */
      if ( dsend > 1231 ) {
         fix_february( dsbegin, dsend, year - 1, &dbegin, &dend );
         beg = 1 + day_count( year, 1, 1, dayminute ) - calendp->jan1day;
         end = 1 + day_count( year, (dend - 1200)/100, (dend - 1200) % 100, dayminute ) - calendp->jan1day;

         for ( k = beg + day_adj; k <= end + day_adj; k++ ) {
            breaker[k] = 1;
         }
      }

      /* Fix February dates for next year if necessary */
      fix_february( dsbegin, dsend, year + 1, &dbegin, &dend );

      /* Get day counts for the same dates next year, but still */
      /* counting from Jan 1 of this year.                      */

      beg = 1 + day_count( year, dbegin/100 + 12, dbegin % 100, dayminute ) - calendp->jan1day;
      end = 1 + day_count( year, dend/100 + 12, dend % 100, dayminute ) - calendp->jan1day;

      /* Limit the upper end */
      end = min(end, arrmax - 1);

      if ( beg < 0 || end < 0 ||  beg > arrmax - 1 || end > arrmax - 1 ) {
         (void)fprintf(stderr, 
            "Line %d-%d: resolve_dates(): Begin/End Error 2: Begin = %d, End = %d\n",
           (*teventpp)[j].line_no, (*teventpp)[j].pos, beg, end);
         exit(1);
      }

      for ( k = beg + day_adj; k <= end + day_adj; k++ ) {	      
         breaker[k] = 1;
      }


      sec_nomin  = (flag & SEC_EVENT) ? SECURITY_OFFSET_ADJUST : 0;
               
      /* If the event is a clock event, set up the dst array to */
      /* indicate periods of Standard and Daylight Time         */

      if ( flag & CLOCK_EVENT ) {
         for ( k = 0; k < 733; k++ ) {
            dst[k + 1] = time_adjust(k, minutes + delay + sec_nomin, LGL2STD);
	 }         
         dst[0] = dst[arrmax];
      }


      /* Here we create a list of interval begin and end days     */
      /* for the specified date range and for periods of Standard */
      /* and Daylight Time.                                       */

      size = 0;
      valb = breaker[0];
      vald = dst[0];
      bday[size] = 0;
      for ( k = 1; k < arrmax; k++ ) {
         if ( breaker[k] != valb || dst[k] != vald ) {
            if ( !valb ) {
               bday[size] = k;
               valb = breaker[k];
               vald = dst[k];
               continue;
            }
            eday[size] = k - 1;
            bday[++size] = k;
            valb = breaker[k];
            vald = dst[k];
         }
      }
      if ( valb )
         eday[size++] = k - 1;


      /* Store the date ranges in the tevent structure */

      for ( k = 0; k < size; k++ ) {
         beg = bday[k];
         end = eday[k];

         offset = minutes;
         bmap = dow_bmap;

         /* Adjust for DST as required */
         if ( flag & CLOCK_EVENT ) {
            offset -= dst[beg];
            if ( offset < 0 ) {
               /* Event occurs on previous day */
               offset += 1440;
               bmap = rrotbmap(bmap);
               shday[k] = -1;
               bday[k] = beg -= 1;
               eday[k] = end -= 1;
            }
            if ( offset > 1439 ) {
               /* Event occurs on following day */
               offset -= 1440;
               bmap = lrotbmap(bmap);
               shday[k] = 1;
               bday[k] = beg += 1;
               eday[k] = end += 1;
            }
         }


         /* Now discard/clip the intervals outside the program days */
         /* range and create a new generation of tevents for those  */  
         /* which remain.                                           */

         day_min = today + 1;
         day_max = (today + ndays - 1) + 1;

         /* For substitute events */
         if ( flag & SUBST_EVENT ) {
            if ( beg <= day_min && end >= day_min ) {
               beg = end = day_min;
               flag &= ~TMP_EVENT;
            }
            else {
               continue;
            }
         }
         
         /* For "expire-dd" events only */ 
         if ( notify >= 0 )
             day_min = day_max - notify; 

         /* Discard intervals entirely outside the program days range */
         if ( end < day_min || beg > day_max ) {
            keep[k] = 0;
            continue;
         }
         keep[k] = 1;
        
         beg = max( beg, day_min );
         end = min( end, day_max );

         cindx = spawn_child_tevent( teventpp, j );

         /* Store the resolved begin and end days in the new tevent */
         /* structure, removing the one-day offset added earlier.   */
         /* In HEYU mode, the intervals are offset such that today  */
         /* is day 0 in the CM11a clock.                            */

         (*teventpp)[cindx].resolv_beg = (beg - 1 - day_zero);
         (*teventpp)[cindx].resolv_end = (end - 1 - day_zero);
         (*teventpp)[cindx].offset = offset;
         (*teventpp)[cindx].dow_bmap = bmap;
         (*teventpp)[cindx].flag = flag & ~(NOCOPY_EVENTS);

         /* Internal check */
         if ( beg - 1 - day_zero  < 0 || end < beg ) {
            (void)fprintf(stderr, 
                "Internal error: Line %d: resolve_dates(): beg = %d, end = %d  day_zero = %d\n",
                (*teventpp)[j].line_no, beg, end, day_zero);
         }             
      }     

      /* Go back and check the discarded interval so as to note any event */
      /* which was "lost" by being shifted outside the program-days range */
      /* due to security or DST day adjustments.                          */

      for ( k = 0; k < size; k++ ) {
         if ( keep[k] || shday[k] == 0 )
            continue;
         if ( shday[k] == -1 && eday[k] == (today + 1) - 1 ) {
           (*teventpp)[j].lostday = today - day_zero;
           (*teventpp)[j].flag |= LOST_EVENT;
         }
         if ( shday[k] ==  1 && bday[k] == (today + 1) + 1 ) {
            (*teventpp)[j].lostday = today + ndays - 1 - day_zero;
            (*teventpp)[j].flag |= LOST_EVENT;
         }
      }
      j++ ;
   }


   /* Disable all (unused) temporary events in the child generations */
   j = 0;
   while ( (*teventpp)[j].line_no > 0 ) {
      if ( (*teventpp)[j].generation == (current_tevent_generation + 1) &&
           (*teventpp)[j].flag & TMP_EVENT ) {
         (*teventpp)[j].flag = NO_EVENT;
      }
      j++;
   }


   /* Update the generation regardless of whether */
   /* or not there are any active tevents.        */

   (void) update_current_tevent_generation_anyway();


   return;
}

/*---------------------------------------------------------+
 | Resolve Timer options DAWNGT, DAWNLT, DUSKGT, DUSKLT    |
 +---------------------------------------------------------*/
void resolve_dawndusk_options ( TEVENT **teventpp, CALEND *calendp )
{
   extern int    current_tevent_generation;

   unsigned char ddoptions;
   int           j, k, cindx, beg, end, val, size;
   int           breaker[366];
   int           scode[366], dawn[366], dusk[366];
   int           intv[32];
   int           day_zero, opttime, stime;

   double        latitude, longitude;
   time_t        tzone;

   long          julianday;

   /* See if we need to do anything here */

   if ( *teventpp == NULL )
      return;

   ddoptions = 0;
   j = 0;
   while ( (*teventpp)[j].line_no > 0 ) {
      if ( (*teventpp)[j].generation == current_tevent_generation &&
           (*teventpp)[j].flag != NO_EVENT ) {
         ddoptions |= (*teventpp)[j].ddoptions;
      }
      j++;
   }
   if ( ddoptions == 0 )
      return;

   /* Load arrays with Dawn and Dusk values */

   /* Get geographic location and timezone from   */
   /* that stored in the global CONFIG structure. */

   if ( configp->loc_flag != (LATITUDE | LONGITUDE) ) {
      (void)fprintf(stderr, 
         "LATITUDE and/or LONGITUDE not specified in %s\n",
	    pathspec(CONFIG_FILE));
      exit(1);
   }

   latitude  = configp->latitude;
   longitude = configp->longitude;
   tzone     = configp->tzone;

   /* Calculate sunrise and sunset for 366 days from today's */
   /* date and store in arrays.                              */

   /* Get today's Julian Day - the big number, not the day   */
   /* of the year.                                           */

   julianday = daycount2JD(calendp->today);

   for ( j = 0; j < 366; j++ ) {
      scode[j] = suntimes( latitude, longitude, tzone, julianday,
         configp->sunmode, configp->sunmode_offset, &dawn[j], &dusk[j], NULL, NULL );
      julianday++ ;
   }

   /* For Arctic/Antarctic regions - Substitute a dawn and/or  */
   /* dusk time for days when the sun is continually above or  */
   /* below the horizon, or no sunrise or sunset.              */

   for ( j = 0; j < 366; j++ ) {
      if ( scode[j] & UP_ALL_DAY ) {
         dawn[j] = 1;     /* 00:01 */
	 dusk[j] = 1438;  /* 23:58 */
      }
      else if ( scode[j] & DOWN_ALL_DAY ) {
	 dawn[j] = 1438;
	 dusk[j] = 1;
      }
      else if ( scode[j] & NO_SUNRISE ) {
	 dawn[j] = 1;
      }
      else if ( scode[j] & NO_SUNSET ) {
	 dusk[j] = 1438;
      }
   }


   j = 0;
   while ( (*teventpp)[j].line_no > 0 ) {
      if ( (*teventpp)[j].generation != current_tevent_generation ||
           (*teventpp)[j].flag == NO_EVENT ) {
         j++;
         continue;
      }
      if ( (ddoptions = (*teventpp)[j].ddoptions) == 0 ) {
         increment_tevent_generation( *teventpp, j );
         j++;
         continue;
      }

      for ( k = 0; k < 366; k++ )
         breaker[k] = 1;

      day_zero = calendp->day_zero;

      if ( ddoptions & DAWNGT ) {
         opttime = (*teventpp)[j].dawngt;
         for ( k = 0; k < 366; k++ ) {
            stime = opttime - time_adjust(k + day_zero, opttime, LGL2STD);
            if ( dawn[k] <= stime )
               breaker[k] = 0;
         }
      }
      if ( ddoptions & DAWNLT ) {
         opttime = (*teventpp)[j].dawnlt;
         for ( k = 0; k < 366; k++ ) {
            stime = opttime - time_adjust(k + day_zero, opttime, LGL2STD);
            if ( dawn[k] >= stime )
               breaker[k] = 0;
         }
      }
      if ( ddoptions & DUSKGT ) {
         opttime = (*teventpp)[j].duskgt;
         for ( k = 0; k < 366; k++ ) {
            stime = opttime - time_adjust(k + day_zero, opttime, LGL2STD);
            if ( dusk[k] <= stime )
               breaker[k] = 0;
         }
      }
      if ( ddoptions & DUSKLT ) {
         opttime = (*teventpp)[j].dusklt;
         for ( k = 0; k < 366; k++ ) {
            stime = opttime - time_adjust(k + day_zero, opttime, LGL2STD);
            if ( dusk[k] >= stime )
               breaker[k] = 0;
         }
      }

      beg = (*teventpp)[j].resolv_beg;
      end = (*teventpp)[j].resolv_end;
      val = breaker[beg];

      size = 0;
      intv[size++] = beg;
      for ( k = beg; k <= end; k++ ) {
         if ( val != breaker[k] ) {
            if ( k > beg ) {
               intv[size++] = k - 1;
               intv[size++] = k;
            }
            val = breaker[k];
         }
      }
      intv[size++] = end;

      for ( k = 0; k < size; k += 2 ) {
         beg = intv[k];
         end = intv[k + 1];

         cindx = spawn_child_tevent( teventpp, j );

         (*teventpp)[cindx].resolv_beg = beg;
         (*teventpp)[cindx].resolv_end = end;
         if ( breaker[beg] == 0 )
            (*teventpp)[cindx].flag |= CANCEL_EVENT;
      }

      j++;
   }
   (void) update_current_tevent_generation();


   return;
}


/*---------------------------------------------------------------------+
 | Duplicate a macro and its elements, giving it a new label (which    |
 | must be unique), and return the index to it.                        |
 +---------------------------------------------------------------------*/
int macro_dupe ( MACRO **macropp, int macindex, 
                               unsigned char **elementp, char *newlabel )
{
   int  j, total, eindx;

   /* Verify the new label is unique */
   j = 0;
   while( (*macropp)[j].line_no > 0 )  {
      if ( strcmp(newlabel, (*macropp)[j].label) == 0 ) {
         (void)fprintf(stderr, "Internal error: New macro label is not unique.\n");
         exit(1);
      }
      j++;
   }

   j = macro_index( macropp, newlabel, DERIVED_MACRO );

   memcpy((void *)(*macropp + j), (void *)(*macropp + macindex), sizeof(MACRO));

   (void)strncpy2((*macropp)[j].label, newlabel, MACRO_LEN);
   (*macropp)[j].line_no = 9999;
   (*macropp)[j].modflag |= DELAY_MOD; 

   /* Copy the macro elements */      
   total = (*macropp)[macindex].total;
   eindx = macro_element_index(elementp, total);
   memcpy((void *)(*elementp + eindx), 
           (void *)(*elementp + (*macropp)[macindex].element), 
                 total * sizeof(unsigned char));
   (*macropp)[j].element = eindx;

   return j;
}   


/*---------------------------------------------------------------------+
 | Special duplicate macro.                                            |
 | Check to see whether a macro with the original label and zero       |
 | delay already exists.  If so, return its index.  If not, create a   |
 | duplicate with a '_' prefixed label and zero delay.                 |
 +---------------------------------------------------------------------*/
int macro_dupe_special ( MACRO **macropp, int macindex, 
                                              unsigned char **elementp )
{
   int  j, total, eindx;
   char *label;
   char labuf[MACRO_LEN + 2];

   label = (*macropp)[macindex].label;
   labuf[0] = '_';
   (void)strncpy2(labuf + 1, label, sizeof(labuf) - 2);
   labuf[MACRO_LEN] = '\0';

   j = 0;
   while( (*macropp)[j].line_no > 0 )  {
      if ( (strcmp(label, (*macropp)[j].label) == 0  ||
            strcmp(labuf, (*macropp)[j].label) == 0)  &&
           (*macropp)[j].delay == 0 ) {
         return j;
      }
      j++;
   }

   j = macro_index( macropp, labuf, DERIVED_MACRO );

   memcpy((void *)(*macropp + j), (void *)(*macropp + macindex), sizeof(MACRO));

   (void)strncpy2((*macropp)[j].label, labuf, MACRO_LEN);
   (*macropp)[j].line_no = 9999;
   (*macropp)[j].delay = 0;
   (*macropp)[j].modflag |= DELAY_MOD; 

   /* Copy the macro elements */      
   total = (*macropp)[macindex].total;
   eindx = macro_element_index(elementp, total);
   memcpy((void *)(*elementp + eindx), 
           (void *)(*elementp + (*macropp)[macindex].element), 
                 total * sizeof(unsigned char));
   (*macropp)[j].element = eindx;

   return j;
}   

/*---------------------------------------------------------------------+
 | Clear tevent tree of disabled events by spawning another generation |
 | without them                                                        |
 +---------------------------------------------------------------------*/
void clear_disabled_events ( TEVENT **teventpp )
{
   int j;

   if ( *teventpp == NULL )
      return;
      
   if ( verbose )
      (void)printf("Entering clear_disabled_events() at generation %d\n",
                              current_tevent_generation);

   j = 0;
   while ( (*teventpp)[j].line_no > 0 ) {
      if ( (*teventpp)[j].generation == current_tevent_generation &&
           (*teventpp)[j].flag != NO_EVENT &&
           ((*teventpp)[j].flag & CANCEL_EVENT) == 0 ) {
         (void) spawn_child_tevent( teventpp, j );
      }
      j++;
   }

   (void) update_current_tevent_generation_anyway();

   return;
}

/*---------------------------------------------------------------------+
 | Generate a pseudo random variation +/- 30 min for an offset         |
 | but remaining within the range 0-1439                               |
 +---------------------------------------------------------------------*/
int randomize_offset ( int offset )
{
   /* Balanced pseudo-randoms */
   static int harrand[24] = {6,18,19,28,21,12,10,11,23,6,11,4,21,
                         15,8,22,26,22,24,15,12,25,4,17};
   time_t   now;
   int      r;
   long int day;

   time(&now);
   day = ((long)now - configp->tzone)/ 86400L;
   r = (day % 2L) ? harrand[day % 23L] : -harrand[day % 23L];
   if ( (offset + r) < 0 || (offset + r) > 1439 )
      return (offset - r);
   return (offset + r);
}

/*---------------------------------------------------------------------+
 | For events where security mode is set, 30 minutes are to be         |
 | subtracted from the event time so the "random" variation will be    |
 | +/- 30 minutes when the CM11a adds the variable time 0-60 minutes   |
 | in this mode.                                                       |
 | However since the CM11a can not wrap times past 23:59 into the next |
 | day, the adjusted time cannot be earlier than 0:00 or later than    |
 | 22:59.  If such would otherwise be the case, we use the delayed     |
 | macro capability of the CM11a to correct the situation.             |
 |                                                                     |
 | This function does not actually change the timer offsets, but       |
 | determines the amount of the adjustment, leaving the actual         |
 | adjustment to function resolve_dates().                             |
 +---------------------------------------------------------------------*/
void security_adjust_legal ( TEVENT **teventpp, MACRO **macropp, 
                                              unsigned char **elementp )
{
   int           j, count, modify;
   int           macro, mindx, submac, cindx, sindx;
   int           good1, good2, offset, delay, event, delta;
   char          *label;
   unsigned int  flag;

   if ( !(*teventpp) )
      return;

   if ( verbose )
      (void)printf("Entering security_adjust_legal() at generation %d\n",
                              current_tevent_generation);

   /* These are the lower and upper bounds of the region where     */
   /* the event time can simply be adjusted without problems, i.e. */
   /* when after subtracting 30 minutes the resulting time falls   */
   /* between 0:00 and 22:59 at any time of year.                  */

   good1 = SECURITY_OFFSET_ADJUST + configp->dstminutes;
   good2 = 1440 - SECURITY_OFFSET_ADJUST - 1;

   /* See if there are any security clock events that will require */
   /* modifying the timer and/or macro.                            */
   j = 0; 
   modify = NO;
   while ( (*teventpp)[j].line_no > 0 ) {
      count++;

      if ( (*teventpp)[j].generation != current_tevent_generation ||
           (flag = (*teventpp)[j].flag) == NO_EVENT ||
           (flag & SEC_EVENT) == 0 ||
           (flag & (DAWN_EVENT | DUSK_EVENT)) != 0 ) {
         j++;
         continue;
      }

      event = (*teventpp)[j].offset + (*teventpp)[j].delay;

      if ( event % 1440 >= good1 && event % 1440 <= good2 ) {
         j++;
         continue;
      }
      else {
         modify = YES;
         j++;
         continue;
      }
   }
   count = j;

   /* Simple situation - store the adjustment to be made later. */
   if ( modify == NO ) {
      for ( j = 0; j < count; j++ ) {
         if ( (*teventpp)[j].generation != current_tevent_generation ||
              (*teventpp)[j].flag == NO_EVENT ) {
            continue;
	 }
	 if ( (*teventpp)[j].flag & SEC_EVENT )
            (*teventpp)[j].security = SECURITY_OFFSET_ADJUST;
	 else
            (*teventpp)[j].security = 0;
      }	 
      return ;
   }

   /* More complex case - create a new timer and possibly a delayed */ 
   /* macro if necessary.                                           */
   j = 0;
   while ( (*teventpp)[j].line_no > 0 ) {
      if ( (*teventpp)[j].generation != current_tevent_generation ||
           (*teventpp)[j].flag == NO_EVENT ) {
         j++;
         continue;
      }	 

      cindx = spawn_child_tevent( teventpp, j );

      flag   = (*teventpp)[cindx].flag;
      offset = (*teventpp)[cindx].offset;
      delay  = (*teventpp)[cindx].delay;
      event  = offset + delay;
      
      if ( !(flag & SEC_EVENT) ) {
         (*teventpp)[cindx].security = 0;
         j++;
         continue;
      }
    
      if ( flag & (DAWN_EVENT | DUSK_EVENT) ||
           (event >= good1 && event <= good2) ) {
         (*teventpp)[cindx].security = SECURITY_OFFSET_ADJUST;
         j++;
         continue;
      }

      /* Create a new macro duplicating the original */

      macro = (*teventpp)[j].macro;
      label = unique_macro_name( &macro, 1, *macropp, SECUR_MAC_PREFIX );

      mindx = macro_dupe ( macropp, macro, elementp, label );

      (*macropp)[mindx].line_no = 9999;
      (*macropp)[mindx].delay = delay = 0;
      (*macropp)[mindx].modflag |= DELAY_MOD;     

      (*teventpp)[cindx].offset = offset = event;
      (*teventpp)[cindx].macro = mindx;
      (*teventpp)[cindx].flag |= (PRT_EVENT);
      (*teventpp)[cindx].print += 1;
      (*teventpp)[cindx].plink = j;


      if ( offset < good1 ) {
         delta = good1 - offset;
         if ( delay >= delta ) {
            /* Delay can be decreased, keeping the event in the same day */
            delay -= delta;
            (*teventpp)[cindx].security = SECURITY_OFFSET_ADJUST - delta;
         }
         else {
            /* Delay must be increased, shifting event into the previous day */
            delta = 1440 + offset - good2;
            delay += delta;
            (*teventpp)[cindx].security = SECURITY_OFFSET_ADJUST + delta;

            /* Create a non-security substitute event for use as first day if needed */
            sindx = spawn_child_tevent( teventpp, j );
            submac = macro_dupe_special(macropp, (*teventpp)[j].macro, elementp);
 
            event = (*teventpp)[j].offset + (*teventpp)[j].delay;
            (*teventpp)[sindx].offset = randomize_offset(event % 1440);
            (*teventpp)[sindx].delay = 0;
            (*teventpp)[sindx].macro = submac;
            (*teventpp)[sindx].flag |= (SUBST_EVENT | TMP_EVENT);
            (*teventpp)[sindx].plink = j;
            if ( submac != (*teventpp)[j].macro ) {
               (*teventpp)[sindx].flag |= PRT_EVENT;
               (*teventpp)[sindx].print += 1;
            }
            (*teventpp)[sindx].flag &= ~SEC_EVENT;
                      
         }
      }
      else  {
         /* Offset > good2 */
         delta = offset - good2;
         if ( delay < 240 - delta ) {
            /* Delay can be increased - keeping event in the same day */
            delay += delta;
            (*teventpp)[cindx].security = SECURITY_OFFSET_ADJUST + delta;
         }
         else {
            /* Delay can only be decreased, shifting event into the following day */
            delta = 1440 + good1 - offset;
            delay -= delta;
            (*teventpp)[cindx].security = SECURITY_OFFSET_ADJUST - delta;
            /* Create a non-security substitute event for use as last day if needed */
            sindx = spawn_child_tevent( teventpp, j );
            submac = macro_dupe_special(macropp, (*teventpp)[j].macro, elementp);

            event = (*teventpp)[j].offset + (*teventpp)[j].delay;
            (*teventpp)[sindx].offset = randomize_offset(event % 1440);
            (*teventpp)[sindx].delay = 0;
            (*teventpp)[sindx].macro = submac;
            (*teventpp)[sindx].flag |= (SUBST_EVENT | TMP_EVENT);
            (*teventpp)[sindx].flag &= ~SEC_EVENT;                       
         }
      }

      (*macropp)[mindx].delay = delay;
      (*teventpp)[cindx].delay = delay;

      j++;
   }

   (void)update_current_tevent_generation();

   return;
}

/*---------------------------------------------------------------------+
 | Warn of duplicate timer in schedule file                            |
 +---------------------------------------------------------------------*/
int warn_duplicate_timer ( TIMER *timerp )
{
   int  j, k, count;
   int  *arrp;
   static int sizint = sizeof(int);

   if ( !timerp )
      return 0;

   j = 0;
   while ( timerp[j].line_no > 0 )
      j++; 

   count = j;
   if ( (arrp = calloc(count, sizint)) == NULL ) {
      fprintf(stderr, "warn_duplicate_timer() - Unable to allocate memory.\n");
      exit(1);
   }

   for ( j = 0; j < count; j++ )
      arrp[j] = 1;

   for ( j = 0; j < count; j++ ) {
         if ( arrp[j] == 0 )
            continue;
      for ( k = j + 1; k < count; k++ ) {
         if ( arrp[k] == 0 )
            continue;
         if ( timerp[j].generation   == timerp[k].generation   && 
              timerp[j].dow_bmap     == timerp[k].dow_bmap     &&
              timerp[j].sched_beg    == timerp[k].sched_beg    &&
              timerp[j].sched_end    == timerp[k].sched_end    &&
              timerp[j].offset_start == timerp[k].offset_start &&
              timerp[j].offset_stop  == timerp[k].offset_stop  &&
              timerp[j].flag_start   == timerp[k].flag_start   &&
              timerp[j].flag_stop    == timerp[k].flag_stop    &&
              timerp[j].notify       == timerp[k].notify       &&
              timerp[j].macro_start  == timerp[k].macro_start  &&
              timerp[j].macro_stop   == timerp[k].macro_stop     ) {
            fprintf(stderr, "Lines %02d,%02d: Warning - Duplicate timer\n",
               timerp[j].line_no, timerp[k].line_no);
            arrp[k] = 0;
         }
      }
   }
   free(arrp);
   return 0;
}

/*---------------------------------------------------------------------+
 | Warn of duplicate timed event in input timers.                      |
 +---------------------------------------------------------------------*/
int warn_duplicate_tevent ( TEVENT *teventp )
{
   int  j, k, count;
   int  *arrp;
   static int sizint = sizeof(int);

   if ( !teventp )
      return 0;

   j = 0;
   while ( teventp[j].line_no > 0 )
      j++; 

   count = j;
   if ( (arrp = calloc(count, sizint)) == NULL ) {
      fprintf(stderr, "warn_duplicate_tevent() - Unable to allocate memory.\n");
      exit(1);
   }

   for ( j = 0; j < count; j++ )
      arrp[j] = 1;

   for ( j = 0; j < count; j++ ) {
         if ( arrp[j] == 0 )
            continue;
      for ( k = j + 1; k < count; k++ ) {
         if ( arrp[k] == 0 )
            continue;
         if ( teventp[j].generation == teventp[k].generation   &&
              teventp[j].dow_bmap   == teventp[k].dow_bmap     &&
              teventp[j].sched_beg  == teventp[k].sched_beg    &&
              teventp[j].sched_end  == teventp[k].sched_end    &&
              teventp[j].offset     == teventp[k].offset       &&
              teventp[j].flag       == teventp[k].flag         &&
              teventp[j].notify     == teventp[k].notify       &&
              teventp[j].macro      == teventp[k].macro           ) {
            fprintf(stderr, 
               "Lines %02d,%02d: Warning - Duplicate Timed Event\n",
                  teventp[j].line_no, teventp[k].line_no);
            arrp[k] = 0;
         }
      }
   }
   free(arrp);
   return 0;
}

/*---------------------------------------------------------------------+
 | Warn of duplicate trigger initiator in schedule file                |
 +---------------------------------------------------------------------*/
int warn_duplicate_trigger ( TRIGGER *triggerp )
{
   int  j, k, count;
   int  *arrp;
   static char *offon[] = {"off", "on"};
   static int sizint = sizeof(int);

   if ( !triggerp )
      return 0;

   j = 0;
   while ( triggerp[j].line_no > 0 )
      j++; 

   count = j;
   if ( (arrp = calloc(count, sizint)) == NULL ) {
      fprintf(stderr, "warn_duplicate_trigger() - Unable to allocate memory.\n");
      exit(1);
   }

   for ( j = 0; j < count; j++ )
      arrp[j] = 1;

   for ( j = 0; j < count; j++ ) {
         if ( arrp[j] == 0 )
            continue;
      for ( k = j + 1; k < count; k++ ) {
         if ( arrp[k] == 0 )
            continue;
         if ( triggerp[j].housecode == triggerp[k].housecode &&
              triggerp[j].unitcode  == triggerp[k].unitcode  &&
              triggerp[j].command   == triggerp[k].command      ) {
            fprintf(stderr, 
               "Lines %02d,%02d: Warning - Duplicate trigger '%c%d %s'\n",
                  triggerp[j].line_no, triggerp[k].line_no, 
                  code2hc(triggerp[j].housecode),
                  code2unit(triggerp[j].unitcode), 
                  offon[triggerp[j].command] );
            arrp[k] = 0;
         }
      }
   }
   free(arrp);
   return 0;
}


/*---------------------------------------------------------------------+
 | Parse schedule file.  Store parameters for timers, triggers, and    |
 | macros in their respective structure arrays.                        |
 +---------------------------------------------------------------------*/
int parse_sched ( FILE *fd_sched, TIMER **timerpp, TRIGGER **triggerpp,
                              MACRO **macropp, unsigned char **elementp )
{
   extern int  line_no, current_timer_generation, timer_generation_delta;

   int         j;
   int         index;
   int         err, errors;

   char        buffer[LINE_LEN];
   char        cmdbuf[256];
   char        cmdbufl[256];
   char        *sp, *tail;

   /* Make two passes through the schedule file, the first */
   /* concerned only with configuration overrides.         */

   line_no = 0;
   errors = 0;
   while ( fgets(buffer, LINE_LEN, fd_sched) != NULL ) {
      line_no++ ;

      /* Make sure buffer is properly terminated */
      buffer[LINE_LEN - 1] = '\0';
      
      /* Get rid of comment strings */
      if ( (sp = strchr(buffer, '#')) != NULL )
         *sp = '\0';

      /* Verify the entire line fits into the buffer */
      if ( strlen(buffer) == (LINE_LEN - 2) ) {
         (void) fprintf(stderr,
           "Line %02d: Line too long.\n", line_no);
         if ( ++errors > MAX_ERRORS )
            return errors;
         continue;
      }

      /* Remove leading and trailing whitespace */
      (void) strtrim(buffer);

      if ( *buffer == '\0')
         continue;

      tail = buffer;

      /* Get the first token on the line. */
      (void) get_token(cmdbuf, &tail, " \t", 255);

      strncpy2(cmdbufl, cmdbuf, sizeof(cmdbufl) - 1);
      strlower(cmdbufl);

      if ( strcmp(cmdbufl, "config") == 0 )  {
         err = parse_config_tail(tail, SRC_SCHED);
         if ( err || *error_message() != '\0' ) {
            fprintf(stderr, 
              "Line %02d: %s\n", line_no, error_message());
            clear_error_message();
         }
         errors += err;
         if ( errors > MAX_ERRORS )
            return errors;
      }
   }
   finalize_config(CONFIG_INIT);


   /* Second pass */
   rewind(fd_sched);

   line_no = 0;
   while ( fgets(buffer, LINE_LEN, fd_sched) != NULL ) {
      line_no++ ;

      /* Make sure buffer is properly terminated */
      buffer[LINE_LEN - 1] = '\0';
      
      /* Get rid of comment strings */
      if ( (sp = strchr(buffer, '#')) != NULL )
         *sp = '\0';

      /* Verify the entire line fit into the buffer */
      if ( strlen(buffer) == (LINE_LEN - 2) ) {
         (void) fprintf(stderr,
           "Line %02d: Line too long.\n", line_no);
         if ( ++errors > MAX_ERRORS )
            return errors;
         continue;
      }

      /* Remove leading and trailing whitespace */
      (void) strtrim(buffer);

      if ( *buffer == '\0')
         continue;

      tail = buffer;

      /* Get the first token on the line. */
      (void) get_token(cmdbuf, &tail, " \t", 255);

      strncpy2(cmdbufl, cmdbuf, sizeof(cmdbufl) - 1);
      strlower(cmdbufl);

      /* Pass the tail of the command line to the appropriate */
      /* parsing function.                                    */

      if ( strcmp(cmdbufl, "timer") == 0 )  {
         errors += parse_timer(timerpp, macropp, tail);
      }
      else if ( strcmp(cmdbufl, "trigger") == 0 ) {
         errors += parse_trigger(triggerpp, macropp, tail);
      }
      else if ( strcmp(cmdbufl, "macro") == 0 ) {
         errors += parse_macro(macropp, elementp, tail);
      }
      else if ( strcmp(cmdbufl, "config") == 0 ) {
         /* Ignore config lines in this pass */
         continue;
      }
      else if ( strcmp(cmdbufl, "section") == 0 ) {
         continue;
      }
      else {
         (void) fprintf(stderr,
           "Line %02d: Unrecognized keyword '%s' in schedule file.\n", line_no, cmdbuf);
         errors++ ;
      }
      if ( errors > MAX_ERRORS )
         return errors;
   }


   /* Verify that all macros referenced by timers and triggers have */
   /* been defined, and that macros have not been multiply defined. */
   /* Warn if a macro is unused by timers or triggers.              */
   /* Also, link the timers together in a linked list and set the   */
   /* generation as generation 0.                                   */
   /* Also store the macro delay times in the timer.                */

   current_timer_generation = 0;
   timer_generation_delta = 0;

   if (*timerpp) {
      j = 0;
      while ( (*timerpp)[j].line_no > 0 ) {
         (*timerpp)[j].link = j + 1;
         (*timerpp)[j].generation = 0;
         index = (*timerpp)[j].macro_start ;
         if ( !((*macropp)[index].refer & MACRO_PARSER) ) {
            (void) fprintf(stderr, "Line %02d: Macro '%s' is not defined.\n",
                      (*timerpp)[j].line_no, (*macropp)[index].label );
            if ( ++errors > MAX_ERRORS )
               return errors;
         }
         (*timerpp)[j].delay_start = (*macropp)[index].delay;


         index = (*timerpp)[j].macro_stop ;
         if ( !((*macropp)[index].refer & MACRO_PARSER) ) {
            (void) fprintf(stderr, "Line %02d: Macro '%s' is not defined.\n",
                      (*timerpp)[j].line_no, (*macropp)[index].label );
            if ( ++errors > MAX_ERRORS )
               return errors;
         }
         (*timerpp)[j].delay_stop = (*macropp)[index].delay;

         j++;
      }
      /* Terminate the linked list with a -1 */
      (*timerpp)[j - 1].link = -1;

      warn_duplicate_timer(*timerpp);
   }

   if (*triggerpp) {
      j = 0;
      while ( (*triggerpp)[j].line_no > 0 ) {
         index = (*triggerpp)[j].macro ;
         if ( !((*macropp)[index].refer & MACRO_PARSER) ) {
            (void) fprintf(stderr, "Line %02d: Macro '%s' is not defined.\n",
                      (*triggerpp)[j].line_no, (*macropp)[index].label );
            errors++;
         }
         if ( errors > MAX_ERRORS )
            return errors;

         j++;
      }

      if ( configp->macterm == YES )
         warn_duplicate_trigger(*triggerpp);
   }

   if (*macropp) {	    
      /* Start with j = 1, ignoring the null macro */
      j = 1; 
      while ( (*macropp)[j].line_no > 0 ) {
         if ( (*macropp)[j].refer & MACRO_DUPLICATE ) {
            (void) fprintf(stderr, "Line %02d: Macro '%s' is multiply defined.\n",
                      (*macropp)[j].line_no, (*macropp)[j].label );
            if ( ++errors > MAX_ERRORS )
               return errors;
         }
         if ( !((*macropp)[j].refer & (TIMER_PARSER | TRIGGER_PARSER)) ) {
            (void) fprintf(stderr,
               "Line %02d: Warning: Macro '%s' is not called by a timer or trigger and will be ignored.\n",
                      (*macropp)[j].line_no, (*macropp)[j].label );
         }
         j++;
      }
   }
   line_no = 0;
   return errors;
}

/*---------------------------------------------------------------------+
 | Parse the tail of a timer command.                                  |
 +---------------------------------------------------------------------*/
int parse_timer_old_old( TIMER **timerpp, MACRO **macropp, char *tail)
{
   extern   int line_no;

   int      mdays[] = {0,31,30,31,30,31,30,31,31,30,31,30,31};

   unsigned int    flag;
   int      count;
   int      time;
   int      index, macindex;
   int      month, day;

   char     weekdays[16];
   char     daterange[16];
   char     datebegin[16], dateend[16];
   char     tstart[20], tstop[20];
   char     macstart[40], macstop[40];

   count = sscanf(tail, "%10s %15s %16s %16s %37s %37s",
            weekdays, daterange, tstart, tstop,
            macstart, macstop);
   if ( count != 6 ) {
      (void) fprintf(stderr, "Line %02d: Invalid timer command.\n", line_no);
      return 1;
   }

   /* If both start and stop macros are null macros, just return */
   if ( !strcmp(macstart, "null") && !strcmp(macstop, "null") )
      return 0;

   /* Get the index in array timerp to store the timer parameters. */
   index = timer_index( timerpp );

   (*timerpp)[index].line_no = line_no;
   (*timerpp)[index].line1 = (*timerpp)[index].line2 = line_no;
   (*timerpp)[index].pos1 = 1;
   (*timerpp)[index].pos2 = 2;

   if ( ((*timerpp)[index].dow_bmap = dow2bmap(weekdays)) == 0xff ) {
      (void) fprintf(stderr, "Line %02d: Timer contains invalid weekdays string.\n", line_no);
      return 1;
   }

   /* Determine whether the command contains clock or dawn or dusk relative */
   /* events and get time offset from either midnight or dawn/dusk.         */

   if ( (flag = parse_time_token(tstart, &time)) == INVALID_EVENT ) {
      (void) fprintf(stderr, "Line %d: Timer contains invalid start time token.\n", line_no);
      return 1;
   }
   else {
      (*timerpp)[index].offset_start = time;
      (*timerpp)[index].flag_start = flag ;
   }

   if ( (flag = parse_time_token(tstop, &time)) == INVALID_EVENT ) {
      (void) fprintf(stderr, "Line %d: Timer contains invalid stop time token.\n", line_no);
      return 1;
   }
   else {
      (*timerpp)[index].offset_stop = time;
      (*timerpp)[index].flag_stop = flag ;
   }

   (void) strtrim(daterange);

   /* Check for the special case "expire-ddd" */
   (*timerpp)[index].notify = -1;
   if ( !strncmp(daterange, "expire", 6) ) {
      /* Fill in full year, store days, and adjust later */  
      (void)strncpy2(datebegin, "01/01", sizeof(datebegin) - 1);
      (void)strncpy2(dateend, "12/31", sizeof(dateend) - 1);

      if ( sscanf(daterange, "expire-%d", &day) == 1 )  
         (*timerpp)[index].notify = day;
      else
         (*timerpp)[index].notify = 0;
   }
   /* Otherwise look for the standard "mm/dd-mm/dd" string */ 
   else if ( sscanf(daterange, "%5s-%5s", datebegin, dateend) != 2 ) {
      (void)fprintf(stderr,
         "Line %02d: Invalid timer date range %s.\n", line_no, daterange);
      return 1;
   }

   if ( sscanf(datebegin, "%d/%d", &month, &day) != 2 ||
          month < 1 || month > 12 || day < 1 || day > mdays[month] ) {
      (void)fprintf(stderr,
         "Line %02d: Invalid timer mm/dd begin date.\n", line_no);
      return 1;
   }
   (*timerpp)[index].sched_beg = 100*month + day;  /* Save as mmdd */

   if ( sscanf(dateend, "%d/%d", &month, &day) != 2 ||
          month < 1 || month > 12 || day < 1 || day > mdays[month] ) {
      (void)fprintf(stderr,
         "Line %02d: Invalid timer mm/dd end date.\n", line_no);
      return 1;
   }
   (*timerpp)[index].sched_end = 100*month + day;  /* Save as mmdd */

   /* If the user has specified reversed dates, move the end date */
   /* into the following year by adding 12 months.                */
   if ( (*timerpp)[index].sched_end < (*timerpp)[index].sched_beg )
      (*timerpp)[index].sched_end += 1200;

   if ( (int)strlen(macstart) > MACRO_LEN || (int)strlen(macstop) > MACRO_LEN )  {
      (void) fprintf(stderr,
         "Line %02d: Macro name exceeds %d characters.\n", line_no, MACRO_LEN);
      return 1;
   }

   /* Register the macro labels and store the indexes thereto. */
   /* If the macro is the null macro, correct the timer flag.  */

   (*timerpp)[index].macro_start = macindex
            = macro_index(macropp, macstart, TIMER_PARSER);
   if ( macindex == NULL_MACRO_INDEX )
      (*timerpp)[index].flag_start = NO_EVENT;

   (*timerpp)[index].macro_stop  = macindex
            = macro_index(macropp, macstop,  TIMER_PARSER);
   if ( macindex == NULL_MACRO_INDEX )
      (*timerpp)[index].flag_stop = NO_EVENT;

   /* For convenience, store the combined start and stop flags */
   (*timerpp)[index].flag_combined =
       (*timerpp)[index].flag_start | (*timerpp)[index].flag_stop;

   return 0;
}


/*---------------------------------------------------------------------+
 | Parse the tail of a timer command, with DAWNLT,...etc options.      |
 +---------------------------------------------------------------------*/
int parse_timer_old ( TIMER **timerpp, MACRO **macropp, char *tail)
{
   extern   int line_no;

   int      mdays[] = {0,31,30,31,30,31,30,31,31,30,31,30,31};

   unsigned int  flag;
   int      time;
   int      index, macindex;
   int      month, day;
   int      j, tokc;
   char     **tokv = NULL;
   char     datebegin[16], dateend[16];
   char     minibuf[32];

   tokenize(tail, " \t\n", &tokc, &tokv);

   /* Required tokens are:
        [0] Weekday map
        [1] Date range
        [2] Start time
        [3] Stop time
        [4] Start macro
        [5] Stop macro
      Optional tokens:
        Keyword 'option' and keywords for supported options.
   */

   if ( tokc < 6 ) {
      fprintf(stderr, "Line %02d: Invalid timer command.\n", line_no);
      free(tokv);
      return 1;
   }

   /* If both start and stop macros are null macros, just return */
   if ( !strcmp(tokv[4], "null") && !strcmp(tokv[5], "null") ) {
      free(tokv);
      return 0;
   }

   /* Get the index in array timerp to store the timer parameters. */
   index = timer_index( timerpp );

   (*timerpp)[index].line_no = line_no;
   (*timerpp)[index].line1 = (*timerpp)[index].line2 = line_no;
   (*timerpp)[index].pos1 = 1;
   (*timerpp)[index].pos2 = 2;

   if ( ((*timerpp)[index].dow_bmap = dow2bmap(tokv[0])) == 0xff ) {
      (void) fprintf(stderr, "Line %02d: Timer contains invalid weekdays string.\n", line_no);
      free(tokv);
      return 1;
   }

   /* Determine whether the command contains clock or dawn or dusk relative */
   /* events and get time offset from either midnight or dawn/dusk.         */

   if ( (flag = parse_time_token(tokv[2], &time)) == INVALID_EVENT ) {
      (void) fprintf(stderr, "Line %d: Timer contains invalid start time token.\n", line_no);
      free(tokv);
      return 1;
   }
   else {
      (*timerpp)[index].offset_start = time;
      (*timerpp)[index].flag_start = flag ;
   }

   if ( (flag = parse_time_token(tokv[3], &time)) == INVALID_EVENT ) {
      (void) fprintf(stderr, "Line %d: Timer contains invalid stop time token.\n", line_no);
      free(tokv);
      return 1;
   }
   else {
      (*timerpp)[index].offset_stop = time;
      (*timerpp)[index].flag_stop = flag ;
   }

   (void) strtrim(tokv[1]);

   /* Check for the special case "expire-ddd" */
   (*timerpp)[index].notify = -1;
   if ( !strncmp(tokv[1], "expire", 6) ) {
      /* Fill in full year, store days, and adjust later */  
      (void)strncpy2(datebegin, "01/01", sizeof(datebegin) - 1);
      (void)strncpy2(dateend, "12/31", sizeof(dateend) - 1);

      if ( sscanf(tokv[1], "expire-%d", &day) == 1 )  
         (*timerpp)[index].notify = day;
      else
         (*timerpp)[index].notify = 0;
   }
   /* Otherwise look for the standard "mm/dd-mm/dd" string */ 
   else if ( sscanf(tokv[1], "%5s-%5s", datebegin, dateend) != 2 ) {
      (void)fprintf(stderr,
         "Line %02d: Invalid timer date range %s.\n", line_no, tokv[1]);
      free(tokv);
      return 1;
   }

   if ( sscanf(datebegin, "%d/%d", &month, &day) != 2 ||
          month < 1 || month > 12 || day < 1 || day > mdays[month] ) {
      (void)fprintf(stderr,
         "Line %02d: Invalid timer mm/dd begin date.\n", line_no);
      free(tokv);
      return 1;
   }
   (*timerpp)[index].sched_beg = 100*month + day;  /* Save as mmdd */

   if ( sscanf(dateend, "%d/%d", &month, &day) != 2 ||
          month < 1 || month > 12 || day < 1 || day > mdays[month] ) {
      (void)fprintf(stderr,
         "Line %02d: Invalid timer mm/dd end date.\n", line_no);
      free(tokv);
      return 1;
   }
   (*timerpp)[index].sched_end = 100*month + day;  /* Save as mmdd */

   /* If the user has specified reversed dates, move the end date */
   /* into the following year by adding 12 months.                */
   if ( (*timerpp)[index].sched_end < (*timerpp)[index].sched_beg )
      (*timerpp)[index].sched_end += 1200;

   if ( (int)strlen(tokv[4]) > MACRO_LEN || (int)strlen(tokv[5]) > MACRO_LEN )  {
      (void) fprintf(stderr,
         "Line %02d: Macro name exceeds %d characters.\n", line_no, MACRO_LEN);
      free(tokv);
      return 1;
   }

   /* Register the macro labels and store the indexes thereto. */
   /* If the macro is the null macro, correct the timer flag.  */

   (*timerpp)[index].macro_start = macindex
            = macro_index(macropp, tokv[4], TIMER_PARSER);
   if ( macindex == NULL_MACRO_INDEX )
      (*timerpp)[index].flag_start = NO_EVENT;

   (*timerpp)[index].macro_stop  = macindex
            = macro_index(macropp, tokv[5],  TIMER_PARSER);
   if ( macindex == NULL_MACRO_INDEX )
      (*timerpp)[index].flag_stop = NO_EVENT;

   /* For convenience, store the combined start and stop flags */
   (*timerpp)[index].flag_combined =
       (*timerpp)[index].flag_start | (*timerpp)[index].flag_stop;

   /* Look for timer Dawn/Dusk options */
   
   (*timerpp)[index].ddoptions = 0;
   (*timerpp)[index].dawnlt = -1;
   (*timerpp)[index].dawngt = -1;
   (*timerpp)[index].dusklt = -1;
   (*timerpp)[index].duskgt = -1;

   for ( j = 6; j < tokc; j++ ) {
      strncpy2(minibuf, tokv[j], 31);
      strupper(minibuf);
      if ( strcmp("DAWNGT", minibuf) == 0 ) {
         if ( (*timerpp)[index].ddoptions & DAWNGT ) {
            fprintf(stderr, "Line %02d: Duplicate DAWNGT option in Timer.\n", line_no);
            free(tokv);
            return 1;
         }
         if ( tokc < j + 2 ) {
            fprintf(stderr, "Line %02d: Missing hh:mm following DAWNGT option\n", line_no);
            free(tokv);
            return 1;
         }
         if ( (flag = parse_time_token(tokv[j + 1], &time)) != CLOCK_EVENT ) {
            fprintf(stderr, "Line %d: Invalid DAWNGT time token '%s'.\n", line_no, tokv[j + 1]);
            free(tokv);
            return 1;
         }
  
         (*timerpp)[index].dawngt = time;
         (*timerpp)[index].ddoptions |= DAWNGT;
         j++;
      }
      else if ( strcmp("DAWNLT", minibuf) == 0 ) {
         if ( (*timerpp)[index].ddoptions & DAWNLT ) {
            fprintf(stderr, "Line %02d: Duplicate DAWNLT option in Timer.\n", line_no);
            free(tokv);
            return 1;
         }
         if ( tokc < j + 2 ) {
            fprintf(stderr, "Line %02d: Missing hh:mm following DAWNLT option\n", line_no);
            free(tokv);
            return 1;
         }
         if ( (flag = parse_time_token(tokv[j + 1], &time)) != CLOCK_EVENT ) {
            fprintf(stderr, "Line %d: Invalid DAWNLT time token '%s'.\n", line_no, tokv[j + 1]);
            free(tokv);
            return 1;
         }
  
         (*timerpp)[index].dawnlt = time;
         (*timerpp)[index].ddoptions |= DAWNLT;
         j++;
      }
      else if ( strcmp("DUSKGT", minibuf) == 0 ) {
         if ( (*timerpp)[index].ddoptions & DUSKGT ) {
            fprintf(stderr, "Line %02d: Duplicate DUSKGT option in Timer.\n", line_no);
            free(tokv);
            return 1;
         }
         if ( tokc < j + 2 ) {
            fprintf(stderr, "Line %02d: Missing hh:mm following DUSKGT option\n", line_no);
            free(tokv);
            return 1;
         }
         if ( (flag = parse_time_token(tokv[j + 1], &time)) != CLOCK_EVENT ) {
            fprintf(stderr, "Line %d: Invalid DUSKGT hh:mm token '%s'.\n", line_no, tokv[j + 1]);
            free(tokv);
            return 1;
         }
  
         (*timerpp)[index].duskgt = time;
         (*timerpp)[index].ddoptions |= DUSKGT;
         j++;
      }
      else if ( strcmp("DUSKLT", minibuf) == 0 ) {
         if ( (*timerpp)[index].ddoptions & DUSKLT ) {
            fprintf(stderr, "Line %02d: Duplicate DUSKLT option in Timer.\n", line_no);
            free(tokv);
            return 1;
         }
         if ( tokc < j + 2 ) {
            fprintf(stderr, "Line %02d: Missing hh:mm following DUSKLT option\n", line_no);
            free(tokv);
            return 1;
         }
         if ( (flag = parse_time_token(tokv[j + 1], &time)) != CLOCK_EVENT ) {
            fprintf(stderr, "Line %d: Invalid DUSKLT hh:mm token '%s'.\n", line_no, tokv[j + 1]);
            free(tokv);
            return 1;
         }
  
         (*timerpp)[index].dusklt = time;
         (*timerpp)[index].ddoptions |= DUSKLT;
         j++;
      }
      else {
         fprintf(stderr, "Line %02d: Invalid Timer option '%s'.\n", line_no, tokv[j]);
         free(tokv);
         return 1;
      }
   }

   free(tokv);
   return 0;
}


/*---------------------------------------------------------------------+
 | Parse the timer date range, observing the user's DATE_FORMAT for    |
 | the order in which month and day are entered.                       |
 | For any format, the separator between month and day may be any of   |
 | '/', '-', or '.'.                                                   |
 | The separator between the two dates may be '-' or ':'.              |
 +---------------------------------------------------------------------*/
int parse_date_range ( char *range, int *mmdd_begin, int *mmdd_end )
{
   char *sp, *rp;
   int  j;
   int  val[4], mon1, day1, mon2, day2;
   char msg[80];
   
   static int  mdays[] = {0,31,30,31,30,31,30,31,31,30,31,30,31};
   static char *sepstr[] = {"/-.", "-:", "/-.", " /t/n/r"};


   rp = range;
   for ( j = 0; j < 4; j++ ) {
      val[j] = (int)strtol(rp, &sp, 10);
      if ( !strchr(sepstr[j], *sp) ) {
         snprintf(msg, sizeof(msg) - 1, "Invalid timer date range '%s'.", range);
         store_error_message(msg);
         return 1;
      }
      rp = sp + 1;
   }

   if ( configp->date_format == DMY_ORDER ) {
      day1 = val[0]; mon1 = val[1];
      day2 = val[2]; mon2 = val[3];
   }
   else {
      mon1 = val[0]; day1 = val[1];
      mon2 = val[2]; day2 = val[3];
   }

   if ( mon1 < 1 || mon1 > 12 || day1 < 1 || day1 > mdays[mon1] ) {
      store_error_message("Invalid timer begin date.");
      return 1;
   }

   if ( mon2 < 1 || mon2 > 12 || day2 < 1 || day2 > mdays[mon2] ) {
      store_error_message("Invalid timer end date.");
      return 1;
   }

   *mmdd_begin = 100 * mon1 + day1;
   *mmdd_end   = 100 * mon2 + day2;

   /* If the user has specified reversed dates, move the end date */
   /* into the following year by adding 12 months.                */
   if ( *mmdd_end < *mmdd_begin )
      *mmdd_end += 1200;

   return 0;
}   
  
/*---------------------------------------------------------------------+
 | Parse the tail of a timer command, with DAWNLT,...etc options.      |
 +---------------------------------------------------------------------*/
int parse_timer ( TIMER **timerpp, MACRO **macropp, char *tail)
{
   extern   int line_no;

//   int      mdays[] = {0,31,30,31,30,31,30,31,31,30,31,30,31};

   unsigned int  flag;
   int      time;
   int      index, macindex;
//   int      month, day;
   int      day;
   int      j, tokc;
   char     **tokv = NULL;
//   char     datebegin[16], dateend[16];
   char     minibuf[32];
   int      mmdd_begin, mmdd_end;

   tokenize(tail, " \t\n", &tokc, &tokv);

   /* Required tokens are:
        [0] Weekday map
        [1] Date range
        [2] Start time
        [3] Stop time
        [4] Start macro
        [5] Stop macro
      Optional tokens:
        Keyword 'option' and keywords for supported options.
   */

   if ( tokc < 6 ) {
      fprintf(stderr, "Line %02d: Invalid timer command.\n", line_no);
      free(tokv);
      return 1;
   }

   /* If both start and stop macros are null macros, just return */
   if ( !strcmp(tokv[4], "null") && !strcmp(tokv[5], "null") ) {
      free(tokv);
      return 0;
   }

   /* Get the index in array timerp to store the timer parameters. */
   index = timer_index( timerpp );

   (*timerpp)[index].line_no = line_no;
   (*timerpp)[index].line1 = (*timerpp)[index].line2 = line_no;
   (*timerpp)[index].pos1 = 1;
   (*timerpp)[index].pos2 = 2;

   if ( ((*timerpp)[index].dow_bmap = dow2bmap(tokv[0])) == 0xff ) {
      (void) fprintf(stderr, "Line %02d: Timer contains invalid weekdays string.\n", line_no);
      free(tokv);
      return 1;
   }

   /* Determine whether the command contains clock or dawn or dusk relative */
   /* events and get time offset from either midnight or dawn/dusk.         */

   if ( (flag = parse_time_token(tokv[2], &time)) == INVALID_EVENT ) {
      (void) fprintf(stderr, "Line %d: Timer contains invalid start time token.\n", line_no);
      free(tokv);
      return 1;
   }
   else {
      (*timerpp)[index].offset_start = time;
      (*timerpp)[index].flag_start = flag ;
   }

   if ( (flag = parse_time_token(tokv[3], &time)) == INVALID_EVENT ) {
      (void) fprintf(stderr, "Line %d: Timer contains invalid stop time token.\n", line_no);
      free(tokv);
      return 1;
   }
   else {
      (*timerpp)[index].offset_stop = time;
      (*timerpp)[index].flag_stop = flag ;
   }

   (void) strtrim(tokv[1]);

   /* Check for the special case "expire-ddd" */
   (*timerpp)[index].notify = -1;
   if ( !strncmp(tokv[1], "expire", 6) ) {
      /* Fill in full year, store days, and adjust later */  
      mmdd_begin = 101;
      mmdd_end = 1231;

      if ( sscanf(tokv[1], "expire-%d", &day) == 1 )  
         (*timerpp)[index].notify = day;
      else
         (*timerpp)[index].notify = 0;
   }
   /* Otherwise look for the standard "mm/dd-mm/dd" string */
   /* or its equivalent in the user's date format          */
   else if ( parse_date_range(tokv[1], &mmdd_begin, &mmdd_end) != 0 ) {
      fprintf(stderr, "Line %02d: %s\n", line_no, error_message());
      free(tokv);
      return 1;
   }
   (*timerpp)[index].sched_beg = mmdd_begin;
   (*timerpp)[index].sched_end = mmdd_end;


   if ( (int)strlen(tokv[4]) > MACRO_LEN || (int)strlen(tokv[5]) > MACRO_LEN )  {
      (void) fprintf(stderr,
         "Line %02d: Macro name exceeds %d characters.\n", line_no, MACRO_LEN);
      free(tokv);
      return 1;
   }

   /* Register the macro labels and store the indexes thereto. */
   /* If the macro is the null macro, correct the timer flag.  */

   (*timerpp)[index].macro_start = macindex
            = macro_index(macropp, tokv[4], TIMER_PARSER);
   if ( macindex == NULL_MACRO_INDEX )
      (*timerpp)[index].flag_start = NO_EVENT;

   (*timerpp)[index].macro_stop  = macindex
            = macro_index(macropp, tokv[5],  TIMER_PARSER);
   if ( macindex == NULL_MACRO_INDEX )
      (*timerpp)[index].flag_stop = NO_EVENT;

   /* For convenience, store the combined start and stop flags */
   (*timerpp)[index].flag_combined =
       (*timerpp)[index].flag_start | (*timerpp)[index].flag_stop;

   /* Look for timer Dawn/Dusk options */
   
   (*timerpp)[index].ddoptions = 0;
   (*timerpp)[index].dawnlt = -1;
   (*timerpp)[index].dawngt = -1;
   (*timerpp)[index].dusklt = -1;
   (*timerpp)[index].duskgt = -1;

   for ( j = 6; j < tokc; j++ ) {
      strncpy2(minibuf, tokv[j], 31);
      strupper(minibuf);
      if ( strcmp("DAWNGT", minibuf) == 0 ) {
         if ( (*timerpp)[index].ddoptions & DAWNGT ) {
            fprintf(stderr, "Line %02d: Duplicate DAWNGT option in Timer.\n", line_no);
            free(tokv);
            return 1;
         }
         if ( tokc < j + 2 ) {
            fprintf(stderr, "Line %02d: Missing hh:mm following DAWNGT option\n", line_no);
            free(tokv);
            return 1;
         }
         if ( (flag = parse_time_token(tokv[j + 1], &time)) != CLOCK_EVENT ) {
            fprintf(stderr, "Line %d: Invalid DAWNGT time token '%s'.\n", line_no, tokv[j + 1]);
            free(tokv);
            return 1;
         }
  
         (*timerpp)[index].dawngt = time;
         (*timerpp)[index].ddoptions |= DAWNGT;
         j++;
      }
      else if ( strcmp("DAWNLT", minibuf) == 0 ) {
         if ( (*timerpp)[index].ddoptions & DAWNLT ) {
            fprintf(stderr, "Line %02d: Duplicate DAWNLT option in Timer.\n", line_no);
            free(tokv);
            return 1;
         }
         if ( tokc < j + 2 ) {
            fprintf(stderr, "Line %02d: Missing hh:mm following DAWNLT option\n", line_no);
            free(tokv);
            return 1;
         }
         if ( (flag = parse_time_token(tokv[j + 1], &time)) != CLOCK_EVENT ) {
            fprintf(stderr, "Line %d: Invalid DAWNLT time token '%s'.\n", line_no, tokv[j + 1]);
            free(tokv);
            return 1;
         }
  
         (*timerpp)[index].dawnlt = time;
         (*timerpp)[index].ddoptions |= DAWNLT;
         j++;
      }
      else if ( strcmp("DUSKGT", minibuf) == 0 ) {
         if ( (*timerpp)[index].ddoptions & DUSKGT ) {
            fprintf(stderr, "Line %02d: Duplicate DUSKGT option in Timer.\n", line_no);
            free(tokv);
            return 1;
         }
         if ( tokc < j + 2 ) {
            fprintf(stderr, "Line %02d: Missing hh:mm following DUSKGT option\n", line_no);
            free(tokv);
            return 1;
         }
         if ( (flag = parse_time_token(tokv[j + 1], &time)) != CLOCK_EVENT ) {
            fprintf(stderr, "Line %d: Invalid DUSKGT hh:mm token '%s'.\n", line_no, tokv[j + 1]);
            free(tokv);
            return 1;
         }
  
         (*timerpp)[index].duskgt = time;
         (*timerpp)[index].ddoptions |= DUSKGT;
         j++;
      }
      else if ( strcmp("DUSKLT", minibuf) == 0 ) {
         if ( (*timerpp)[index].ddoptions & DUSKLT ) {
            fprintf(stderr, "Line %02d: Duplicate DUSKLT option in Timer.\n", line_no);
            free(tokv);
            return 1;
         }
         if ( tokc < j + 2 ) {
            fprintf(stderr, "Line %02d: Missing hh:mm following DUSKLT option\n", line_no);
            free(tokv);
            return 1;
         }
         if ( (flag = parse_time_token(tokv[j + 1], &time)) != CLOCK_EVENT ) {
            fprintf(stderr, "Line %d: Invalid DUSKLT hh:mm token '%s'.\n", line_no, tokv[j + 1]);
            free(tokv);
            return 1;
         }
  
         (*timerpp)[index].dusklt = time;
         (*timerpp)[index].ddoptions |= DUSKLT;
         j++;
      }
      else {
         fprintf(stderr, "Line %02d: Invalid Timer option '%s'.\n", line_no, tokv[j]);
         free(tokv);
         return 1;
      }
   }

   free(tokv);
   return 0;
}

/*---------------------------------------------------------------------+
 | Parse the tail of a trigger command.                                |
 +---------------------------------------------------------------------*/
int parse_trigger( TRIGGER **triggerpp, MACRO **macropp, char *tail)
{
   extern int line_no;

   int          index, macindex;
   char         hc;
   int          tokc;
   char         **tokv;
   unsigned int bitmap, aflags;

   tokenize(tail, " \t\n", &tokc, &tokv);

   /* Expected tokens are:       */
   /*   tokv[0] = Housecode|Unit */
   /*   tokv[1] = "on" or "off"  */
   /*   tokv[2] = macro label    */

   if ( tokc < 3 ) {
      (void) fprintf(stderr,
         "Line %02d: Invalid trigger (too few fields).\n", line_no);
      free(tokv);
      return 1;
   }
   if ( tokc > 3 ) {
      (void) fprintf(stderr,
         "Line %02d: Invalid trigger (too many fields).\n", line_no);
      free(tokv);
      return 1;
   }

   index = trigger_index( triggerpp );
   (*triggerpp)[index].line_no = line_no;

   aflags = parse_addr(tokv[0], &hc, &bitmap);

   if ( !(aflags & A_VALID) ) {
      fprintf(stderr, "Line %02d: %s\n", line_no, error_message());
      clear_error_message();
      free(tokv);
      return 1;
   }
   if ( !(aflags & A_HCODE) || !(aflags & A_BMAP) ||
                    aflags & (A_PLUS | A_MINUS | A_DUMMY | A_MULT) ) {
      fprintf(stderr,
         "Line %02d: Invalid trigger Housecode|Unit '%s'\n",
             line_no, tokv[0]);
      free(tokv);
      return 1;
   }

   (*triggerpp)[index].housecode = hc2code(hc);
   (*triggerpp)[index].unitcode = single_bmap_unit(bitmap);

   if ( strcmp(tokv[1], "on") == 0 )
      (*triggerpp)[index].command = TRIGGER_ON;
   else if ( strcmp(tokv[1], "off") == 0 )
      (*triggerpp)[index].command = TRIGGER_OFF;
   else {
      (void) fprintf(stderr,
         "Line %02d: Trigger command not 'on' or 'off'.\n", line_no);
      free(tokv);
      return 1;
   }

   if ( (int)strlen(tokv[2]) > MACRO_LEN ) {
      (void) fprintf(stderr,
          "Line %02d: Macro label exceeds %d characters.\n",
              line_no, MACRO_LEN);
      free(tokv);
      return 1;
   }

   if ( !strcmp(tokv[2], "null") ) {
      (void) fprintf(stderr,
         "Line %02d: Macro label \"null\" is not valid in a trigger.\n",
         line_no);
      free(tokv);
      return 1;
   }

   /* Register the macro label and store the index thereto */
   macindex =  macro_index(macropp, tokv[2], TRIGGER_PARSER);
   (*triggerpp)[index].macro = macindex;

   /* Cross-reference the macro with the trigger */
   (*macropp)[macindex].trig = index;

   free(tokv);
   return 0;
}


/*---------------------------------------------------------------------+
 | Parse the tail of a macro command in the upload schedule.           |
 +---------------------------------------------------------------------*/
int parse_macro ( MACRO **macropp, unsigned char **elementp, char *tail)
{
   extern int    line_no;
   extern char   *typename[];

   int           i, j, k, m, n, index, loc, retcode = 0;
   int           delay, nelem, nbytes;
   char          buffer[1024];
   unsigned char elembuff[256];
   int           macc, cmdc, tokc, scmdc;
   char          **macv, **cmdv, **scmdv, **tokv;
   char          *sp, *scp, *tailsave;
   SCENE         *scenep;

   scenep = configp->scenep;

   /* First token is macro label */
   if ( *(get_token(buffer, &tail, " \t", 255)) == '\0') {
       fprintf(stderr, "Line %02d: Too few fields.\n", line_no);
       return 1;
   }

   if ( (int)strlen(buffer) > MACRO_LEN ) {
      fprintf(stderr, "Line %02d: Macro label '%s' exceeds %d characters.\n",
         line_no, buffer, MACRO_LEN);
      return 1;
   }

   if ( !strcmp(buffer, "null") ) {
      fprintf(stderr, "Line %02d: null macro may not be re-defined.\n", line_no);
      return 1;
   }

   if ( strchr("_+-$", *buffer) != NULL ) {
      fprintf(stderr, 
         "Line %02d: Macro labels beginning with '_', '+', '-', '$' are reserved.\n", line_no);
      return 1;
   }

   /* Get index in array of MACRO structures */
   index = macro_index(macropp, buffer, MACRO_PARSER);
   (*macropp)[index].line_no = line_no;

   /* Get the index into the element array where we will start to */
   /* store the elements.                                         */
   (*macropp)[index].element = macro_element_index( elementp, 0 );

   /* Second token is delay time (optional if zero) */
   tailsave = tail;
   if ( *(get_token(buffer, &tail, " \t", 255)) == '\0') {
       fprintf(stderr, "Line %02d: Too few fields.\n", line_no);
       return 1;
   }

   delay = (int)strtol(buffer, &sp, 10);

   if ( !strchr(" \t", *sp) ) {
      /* Non-numeric, so use default zero delay and backup tail pointer */
      delay = 0;
      tail = tailsave;
   }

   if ( delay < 0 || delay > 240 ) {
      fprintf(stderr,
        "Line %02d: Macro delay is outside range 0 - 240 minutes.\n", line_no);
      return 1;
   }

   (*macropp)[index].delay = (unsigned char)delay;

   /* Divide the remainder of the tail into ';' delimited x10 commands */
   tokenize(tail, ";", &macc, &macv);

   for ( i = 0; i < macc; i++ ) {
      strtrim(macv[i]);
      if ( *macv[i] == '\0' )
         continue;
      /* Divide commands into tokens */
      tokenize(macv[i], " \t", &cmdc, &cmdv); 
      /* See if it's a user-defined scene from the config file */
      if ( (k = lookup_scene(scenep, cmdv[0])) >= 0 ) {
         /* Check if needed number of parameters are supplied */
         if ( cmdc < scenep[k].nparms + 1 ) {
            fprintf(stderr,
              "Line %02d: (Config %02d: %s '%s'): Too few parameters - %d required.\n",
                 line_no, scenep[k].line_no, typename[scenep[k].type], scenep[k].label,
                 scenep[k].nparms);
            free(cmdv);
            free(macv);
            return 1;
         }
         else if ( cmdc > scenep[k].nparms + 1 ) {
            fprintf(stderr,
              "Line %02d: (Config %02d: %s '%s'): Too many parameters - only %d accepted.\n",
                 line_no, scenep[k].line_no, typename[scenep[k].type], scenep[k].label,
                 scenep[k].nparms);
            free(cmdv);
            free(macv);
            return 1;
         }

         /* Tokenize individual commands in scene */
         scp = strdup(scenep[k].body);
         tokenize(scp, ";", &scmdc, &scmdv);
         for ( m = 0; m < scmdc; m++ ) {
            strtrim(scmdv[m]);
	    if ( *scmdv[m] == '\0' )
               continue;
            /* Break up commands into tokens */
            tokenize(scmdv[m], " \t", &tokc, &tokv);
            /* Substitute for any dummy parameters */
            replace_dummy_parms(tokc, &tokv, cmdv);

            retcode = macro_command(tokc, tokv, &nelem, &nbytes, elembuff);

            /* Free each member of tokv, then tokv itself */ 
            for ( j = 0; j < tokc; j++ )
               free(tokv[j]);
            free(tokv);

            if ( retcode != 0 ) {
               /* Display stacked error message from macro_command() */
               fprintf(stderr, "Line %02d: (Config %02d: %s '%s'): %s.\n",
                  line_no, scenep[k].line_no, typename[scenep[k].type],
                  scenep[k].label, error_message());
               clear_error_message();
               free(scmdv);
               free(scp);
               free(cmdv);
               free(macv);
               return 1;
            }
            else if ( *error_message() ) {
               /* Display any warning message */
               fprintf(stderr, "Line %02d: %s.\n", line_no, error_message());
               clear_error_message();
            }

            (*macropp)[index].nelem += nelem;
            (*macropp)[index].total += nbytes;

            /* Make space for the element and copy it there */
            loc = macro_element_index( elementp, nbytes );
            for ( n = 0; n < nbytes; n++ ) {
               (*elementp)[loc + n] = elembuff[n];
            }
         }
         free(scmdv);
         free(scp);
      }
      else {
         /* It's an individual command */
         retcode = macro_command(cmdc, cmdv, &nelem, &nbytes, elembuff);
         free(cmdv);
         if ( retcode != 0 ) {
            /* Display stacked error message from macro_command() */
            fprintf(stderr, "Line %02d: %s.\n", line_no, error_message());
            free(macv);
            return 1;
         }
         (*macropp)[index].nelem += nelem;
         (*macropp)[index].total += nbytes;

         /* Make space for the element and copy it there */
         loc = macro_element_index( elementp, nbytes );
         for ( n = 0; n < nbytes; n++ )
            (*elementp)[loc + n] = elembuff[n];
      }
   }
   free(macv);

   /* Check that the macro has at least one element */
   if ( (*macropp)[index].nelem < 1 ) {
      fprintf(stderr, "Line %02d: macro contains no commands.\n", line_no);
      return 1;
   }

   return 0;
}
         

/*----------------------------------------------------------------------------+
 | Split timers into individual tevents                                       |
 +----------------------------------------------------------------------------*/
void split_timers ( TIMER *timerp, TEVENT **teventpp )
{
   int        j, indx = 0;

   if ( !timerp )
      return;

   j = 0;
   while( timerp[j].line_no > 0 ) {
      if ( timerp[j].flag_start != NO_EVENT ) {
         indx = tevent_index( teventpp );
         (*teventpp)[indx].line_no     = timerp[j].line_no;
         (*teventpp)[indx].pos         = 1;
         (*teventpp)[indx].link        = indx + 1;
         (*teventpp)[indx].timer       = j;
         (*teventpp)[indx].generation  = 0;
         (*teventpp)[indx].dow_bmap    = timerp[j].dow_bmap;
         (*teventpp)[indx].sched_beg   = timerp[j].sched_beg;
         (*teventpp)[indx].sched_end   = timerp[j].sched_end;
         (*teventpp)[indx].notify      = timerp[j].notify;
         (*teventpp)[indx].resolv_beg  = timerp[j].resolv_beg;
         (*teventpp)[indx].resolv_end  = timerp[j].resolv_end;
         (*teventpp)[indx].flag        = timerp[j].flag_start | PRT_EVENT;
         (*teventpp)[indx].print       = 1;
         (*teventpp)[indx].flag2       = 0;
         (*teventpp)[indx].cancel      = timerp[j].cancel;
         (*teventpp)[indx].offset      = timerp[j].offset_start;
         (*teventpp)[indx].delay       = timerp[j].delay_start;
         (*teventpp)[indx].security    = timerp[j].security_start;
         (*teventpp)[indx].ddoptions   = timerp[j].ddoptions;
         (*teventpp)[indx].dawnlt      = timerp[j].dawnlt;
         (*teventpp)[indx].dawngt      = timerp[j].dawngt;
         (*teventpp)[indx].dusklt      = timerp[j].dusklt;
         (*teventpp)[indx].duskgt      = timerp[j].duskgt;
         (*teventpp)[indx].macro       = timerp[j].macro_start;
         (*teventpp)[indx].ptr         = timerp[j].ptr_start;
         (*teventpp)[indx].intv	       = timerp[j].intv;
         (*teventpp)[indx].nintv       = timerp[j].nintv;
      }
      if ( timerp[j].flag_stop  != NO_EVENT ) {
         indx = tevent_index( teventpp );
         (*teventpp)[indx].line_no     = timerp[j].line_no;
         (*teventpp)[indx].pos         = 2;
         (*teventpp)[indx].link        = indx + 1;
         (*teventpp)[indx].timer       = j;
         (*teventpp)[indx].generation  = 0;
         (*teventpp)[indx].dow_bmap    = timerp[j].dow_bmap;
         (*teventpp)[indx].sched_beg   = timerp[j].sched_beg;
         (*teventpp)[indx].sched_end   = timerp[j].sched_end;
         (*teventpp)[indx].notify      = timerp[j].notify;
         (*teventpp)[indx].resolv_beg  = timerp[j].resolv_beg;
         (*teventpp)[indx].resolv_end  = timerp[j].resolv_end;
         (*teventpp)[indx].flag        = timerp[j].flag_stop | PRT_EVENT;
         (*teventpp)[indx].print       = 1;
         (*teventpp)[indx].flag2       = 0;
         (*teventpp)[indx].cancel      = timerp[j].cancel;
         (*teventpp)[indx].offset      = timerp[j].offset_stop;
         (*teventpp)[indx].delay       = timerp[j].delay_stop;
         (*teventpp)[indx].security    = timerp[j].security_stop;
         (*teventpp)[indx].ddoptions   = timerp[j].ddoptions;
         (*teventpp)[indx].dawnlt      = timerp[j].dawnlt;
         (*teventpp)[indx].dawngt      = timerp[j].dawngt;
         (*teventpp)[indx].dusklt      = timerp[j].dusklt;
         (*teventpp)[indx].duskgt      = timerp[j].duskgt;
         (*teventpp)[indx].macro       = timerp[j].macro_stop;
         (*teventpp)[indx].ptr         = timerp[j].ptr_start;
         (*teventpp)[indx].intv	       = timerp[j].intv;
         (*teventpp)[indx].nintv       = timerp[j].nintv;
      }
      j++ ;
   }
   (*teventpp)[indx].link = -1;
   return;
}

/*----------------------------------------------------------------------------+
 | Associate each tevent with the trigger which call its macro (if any).      |
 +----------------------------------------------------------------------------*/
void associate_tevent_triggers ( TEVENT *teventp, MACRO *macrop )
{
   int j;

   if ( !teventp )
      return;

   j = 0;
   while ( teventp[j].line_no > 0 ) {
      teventp[j].trig = macrop[teventp[j].macro].trig;
      j++;
   }

   return;
}

/*----------------------------------------------------------------------------+
 | Replace events involving delayed macros with new events calling new macros |
 | with zero delay.  (Necessary to avoid pending delayed macros, which are    |
 | erased when a new schedule is uploaded.)  Security events (handled         |
 | separately) and Dawn/Dusk related events are not modified.                 |
 +----------------------------------------------------------------------------*/
void replace_delayed_events ( TEVENT **teventpp, MACRO **macropp, 
                                                      unsigned char **elementp )
{
   int           j, cindx, macro, delay;
   unsigned int  flag, needed;

   if ( *teventpp == NULL )
      return;

   if ( verbose )
      (void)printf("Entering replace_delayed_events() at generation %d\n",
              current_tevent_generation);

   /* Check whether we need to do anything at all */
   needed = NO;
   j = 0;
   while ( (*teventpp)[j].line_no > 0 ) {
      if ( (*teventpp)[j].generation != current_tevent_generation ||
           (flag = (*teventpp)[j].flag) == NO_EVENT ||
           (*teventpp)[j].delay == 0 ||
           (flag & SEC_EVENT) != 0 ||
           (flag & CLOCK_EVENT) == 0         ) {
         j++;
         continue;
      }
      needed = YES;
      break;
   }

   if ( needed == NO )
      return;

   j = 0;
   while ( (*teventpp)[j].line_no > 0 ) {
      if ( (*teventpp)[j].generation != current_tevent_generation || 
           (flag = (*teventpp)[j].flag) == NO_EVENT ) {
         j++;
         continue;
      }

      /* Create a new tevent */
      cindx = spawn_child_tevent( teventpp, j );

      if ( (delay = (*teventpp)[j].delay) == 0 ||
           (flag & SEC_EVENT) != 0 ||
           (flag & CLOCK_EVENT) == 0  ) {
         j++;
         continue;
      }

      if ( verbose )
         (void)printf("Replacing macro '%s' with non-delayed macro.\n",
            (*macropp)[(*teventpp)[j].macro].label);
  
      /* Create a duplicate macro with a new name and zero delay */
      macro = macro_dupe_special(macropp, (*teventpp)[j].macro, elementp);

      (*teventpp)[cindx].offset += (*teventpp)[j].delay;
      (*teventpp)[cindx].delay = 0;
      (*teventpp)[cindx].macro = macro;
      (*teventpp)[cindx].flag |= (PRT_EVENT);
      (*teventpp)[cindx].print += 1;

      j++;
   }

   (void)update_current_tevent_generation();

   return;
}


/*----------------------------------------------------------------------------+
 | Try to create a unique macro name for a combined macro which will fit      |
 | in the allowed space and yet be meaningful to the user.                    |
 +----------------------------------------------------------------------------*/
char *unique_macro_name ( int *maclist, int listlen, MACRO *macrop, char *prefix )
{
   /* Quick fix for now */
   static char macname[MACRO_LEN + 1];
   static int  macnum = 0;

   (void)sprintf(macname, "%s_%02d", prefix, macnum++);
   return macname;
}

/*----------------------------------------------------------------------------+
 | Return 1 if two tevents are the same except for the macros, and the macros |
 | have the same delay time.                                                  |
 +----------------------------------------------------------------------------*/
int is_tevent_similar ( int one, int two, TEVENT *teventp )
{
   unsigned int mask = (TIME_EVENTS | SEC_EVENT);

   return teventp[one].generation   !=  teventp[two].generation     ?  0 :
          teventp[one].dow_bmap     !=  teventp[two].dow_bmap       ?  0 :
          teventp[one].sched_beg    !=  teventp[two].sched_beg      ?  0 :
          teventp[one].sched_end    !=  teventp[two].sched_end      ?  0 :
          teventp[one].notify       !=  teventp[two].notify         ?  0 :
         (teventp[one].flag & mask) != (teventp[two].flag & mask)   ?  0 : 
          teventp[one].offset       !=  teventp[two].offset         ?  0 :
          teventp[one].notify       !=  teventp[two].notify         ?  0 :
          teventp[one].ddoptions    !=  teventp[two].ddoptions      ?  0 :
          teventp[one].dawnlt       !=  teventp[two].dawnlt         ?  0 : 
          teventp[one].dawngt       !=  teventp[two].dawngt         ?  0 : 
          teventp[one].dusklt       !=  teventp[two].dusklt         ?  0 : 
          teventp[one].duskgt       !=  teventp[two].duskgt         ?  0 : 
          teventp[one].delay        !=  teventp[two].delay          ?  0 : 1;
}

/*----------------------------------------------------------------------------+
 | Combine similar tevents if possible, by creating secondary macros.         |
 +----------------------------------------------------------------------------*/
void combine_similar_tevents ( TEVENT **teventpp, MACRO **macropp, unsigned char **elementp )
{
   int  j, k, m;
   int  sm, nsim, bsm, linki, linkm;
   int  elem, mac, nelem, loc, indx, cindx, ibuff;
   int  simlist[256];
   int  maclist[256];
   unsigned char elembuff[4096];
   char macname[MACRO_LEN];

   if ( *teventpp == NULL )
      return ;

   if ( verbose )
      (void)printf("Entering combine_similar_events() at generation %d\n",
           current_tevent_generation);

   /* Reset the "done" flags */
   j = 0;
   while ( (*teventpp)[j].line_no > 0 ) {
      (*teventpp)[j].done = 0;
       j++;
   }

   j = 0;
   while ( (*teventpp)[j].line_no > 0 ) {  
      if ( (*teventpp)[j].generation != current_tevent_generation  ||
           (*teventpp)[j].flag == NO_EVENT ||
           (*teventpp)[j].done  )   {
         j++;
         continue;
      }

      /* Compile a list of similar tevents, i.e., with same date range, */
      /* dow bmap, type, offset, security, macro delay.                 */
      k = j + 1;
      nsim = 0;
      simlist[nsim++] = j;

      while ( (*teventpp)[k].line_no > 0 )   {
         if ( (*teventpp)[k].generation != current_tevent_generation ||
              (*teventpp)[k].flag == NO_EVENT ||
              (*teventpp)[k].done ) {
            k++;
            continue;
         }
         if ( is_tevent_similar(j, k, *teventpp) ) 
            simlist[nsim++] = k;
         k++;
      }

      /* Make a list of the corresponding macro indexes */
      for ( k = 0; k < nsim; k++ )
         maclist[k] = (*teventpp)[simlist[k]].macro;

      /* Reorder the links to group together similar tevents */
      /* for reporting purposes.                             */

      for ( k = nsim - 1; k > 0; k-- ) {
         (*teventpp)[simlist[k]].plink = simlist[k - 1];
      }
      
      indx = j;
      for ( k = 1; k < nsim; k++ ) {
         sm = simlist[k];
         if ( (*teventpp)[indx].link == sm ) {
            indx = sm;
            continue;
         }

         m = 0;
         bsm = -1;
         while ( (*teventpp)[m].line_no > 0 ) { 
            if ( (*teventpp)[m].link == sm ) {
               bsm = m;
               break;
            }
            m++;
         }

         if ( bsm < 0 ) {
            (void)fprintf(stderr, 
                 "Internal error: combine_similar_tevents(): No back link to tevent %d\n", sm);
            exit(1);
         }                

         linki = (*teventpp)[indx].link;
         linkm = (*teventpp)[sm].link;

         (*teventpp)[sm].link = linki;
         (*teventpp)[indx].link = sm;
         (*teventpp)[bsm].link = linkm;
         indx = sm;
      }

      /* Create a new TEVENT linked to the last in the group */
      cindx = spawn_child_tevent( teventpp, simlist[nsim - 1]);

      /* Replace its line_no and pos with that of the first */
      /* in the group.                                      */
      (*teventpp)[cindx].line_no = (*teventpp)[simlist[0]].line_no;
      (*teventpp)[cindx].pos     = (*teventpp)[simlist[0]].pos;

      /* Store the number of combined events */
      (*teventpp)[cindx].combined = nsim;
 
      /* If more than 1 tevent in the list, flag them for printing */
      if ( nsim > 1 ) {
         for ( k = 0; k < nsim; k++ ) 
            (*teventpp)[simlist[k]].flag |= PRT_EVENT;
         (*teventpp)[cindx].flag |= (PRT_EVENT | COMB_EVENT);
         (*teventpp)[cindx].print = nsim + 1;
      }

      /* If more than 1 tevent in the list, we must create a */
      /* new macro for the combined tevent.                  */

      if ( nsim > 1 ) {
         /* Concatenate the macro elements of the similar events */
         ibuff = 0;
         nelem = 0;
         for ( k = 0; k < nsim; k++ ) {
            mac = maclist[k];
            elem = (*macropp)[mac].element;
            nelem += (*macropp)[mac].nelem;
            for ( m = 0; m < (*macropp)[mac].total; m++ ) {
               elembuff[ibuff++] = (*elementp)[elem++];
            }
         }

         /* Create a combined macro */
         (void)strncpy2(macname, unique_macro_name(maclist, nsim, 
                        *macropp, COMB_MAC_PREFIX), sizeof(macname) - 1);

         mac = macro_index(macropp, macname, DERIVED_MACRO);
         (*teventpp)[cindx].macro = mac;

         loc = macro_element_index( elementp, ibuff );

         (*macropp)[mac].element = loc;
         (*macropp)[mac].total = ibuff;
         (*macropp)[mac].nelem = nelem;
         (*macropp)[mac].line_no = 9999;
         /* (*macropp)[mac].modflag |= COMBINED; */
         (*macropp)[mac].delay = (*teventpp)[cindx].delay;

         for ( k = 0; k < ibuff; k++ ) {
            (*elementp)[loc++] = elembuff[k];
         }
      }

      /* Mark the similar tevents as "done" */
      for ( k = 0; k < nsim; k++ )  {
         sm = simlist[k];
         (*teventpp)[sm].done = 1;
      }
   }

   update_current_tevent_generation();

   return;
}
                  

/*----------------------------------------------------------------------------+
 | Return 1 if two tevents have the same dow_bmask and begin/end days but     |
 | not the same offsets.  (If a timer has two events with the same time,      |
 | only the first will be executed.)                                          |
 +----------------------------------------------------------------------------*/
int is_compatible ( TEVENT *teventp1, TEVENT *teventp2 )
{
   return teventp1->dow_bmap   != teventp2->dow_bmap   ?  0 :
          teventp1->resolv_beg != teventp2->resolv_beg ?  0 :
          teventp1->resolv_end != teventp2->resolv_end ?  0 :
          teventp1->offset     == teventp2->offset     ?  0 :
          teventp1->notify     != teventp2->notify     ?  0 : 1 ;
   
}

/*----------------------------------------------------------------------------+
 | Copy data from two tevents into one timer.  If the second index is -1, the |
 | "stop" part of the timer will be a null event.                             |
 +----------------------------------------------------------------------------*/
void copy_tevents_to_timer ( int tev1, int tev2, int timr, 
                                              TEVENT *teventp, TIMER *timerp)
{
   extern int current_timer_generation;

   if ( !teventp )
      return;
   
   timerp[timr].line_no       = 9999;
   timerp[timr].generation    = current_timer_generation;
   timerp[timr].dow_bmap      = teventp[tev1].dow_bmap;
   timerp[timr].sched_beg     = teventp[tev1].sched_beg;
   timerp[timr].sched_end     = teventp[tev1].sched_end;
   timerp[timr].notify        = teventp[tev1].notify;
   timerp[timr].resolv_beg    = teventp[tev1].resolv_beg;
   timerp[timr].resolv_end    = teventp[tev1].resolv_end;

   timerp[timr].line1          = teventp[tev1].line_no;
   timerp[timr].pos1           = teventp[tev1].pos;
   timerp[timr].tevent_start   = tev1;
   timerp[timr].flag_start     = teventp[tev1].flag;
   timerp[timr].flag_combined  = teventp[tev1].flag | teventp[tev1].flag2;
   timerp[timr].offset_start   = teventp[tev1].offset;
   timerp[timr].delay_start    = teventp[tev1].delay;
   timerp[timr].security_start = teventp[tev1].security;
   timerp[timr].macro_start    = teventp[tev1].macro;

   if ( tev2 >= 0 ) {
      timerp[timr].line2          = teventp[tev2].line_no;
      timerp[timr].pos2           = teventp[tev2].pos;
      timerp[timr].tevent_stop    = tev2;
      timerp[timr].flag_stop      = teventp[tev2].flag;
      timerp[timr].flag_combined |= teventp[tev2].flag | teventp[tev2].flag2;
      timerp[timr].offset_stop    = teventp[tev2].offset;
      timerp[timr].delay_stop     = teventp[tev2].delay;
      timerp[timr].security_stop  = teventp[tev2].security;
      timerp[timr].macro_stop     = teventp[tev2].macro;
   }
   else {
      timerp[timr].line2          = 0;
      timerp[timr].pos2           = 0;
      timerp[timr].tevent_stop    = -1;
      timerp[timr].flag_stop      = NO_EVENT;
      timerp[timr].offset_stop    = NULL_TIME;
      timerp[timr].delay_stop     = 0;
      timerp[timr].security_stop  = 0;
      timerp[timr].macro_stop     = NULL_MACRO_INDEX;
   }

   return;
}

 
/*----------------------------------------------------------------------------+
 | Recombine tevents into new timers.                                         |
 +----------------------------------------------------------------------------*/
int reconstruct_timers ( TEVENT *teventp, TIMER **timerpp )
{
   extern int current_tevent_generation;
   extern int timer_generation_delta;

   int          j, k, endch, tnum, indx = 0, matched;
   unsigned int flag;

   if ( !teventp )
      return 0;

   if ( verbose )
      (void)printf("Entering reconstruct_timers() at tevent generation %d\n",
           current_tevent_generation);

   timer_generation_delta = 1;
   update_current_timer_generation();

   /* Find the end of the existing linked timer chain */
   endch = -1;
   tnum = 0;
   while ( (*timerpp)[tnum].line_no > 0 ) {
      if ( (*timerpp)[tnum].link == -1 )
         endch = tnum;
      tnum++;
   }

   /* Count the number of timed events and reset the "done" flags */
   tnum = 0;
   while ( teventp[tnum].line_no > 0 ) {
      teventp[tnum].done = 0;
      tnum++;
   }

   /* First try to combine like events, i.e., Dawn with Dawn, */
   /* Dusk with Dusk, or Clock with Clock.                    */

   for ( j = 0; j < tnum; j++ ) {
      if ( teventp[j].generation != current_tevent_generation ||
           teventp[j].flag == NO_EVENT ||
           teventp[j].flag & CHAIN_EVENT ||
           teventp[j].done )
         continue;

      flag = teventp[j].flag & TIME_EVENTS;
      matched = 0;
      for ( k = j + 1; k < tnum; k++ ) {
         if ( teventp[k].generation != current_tevent_generation ||
              teventp[k].flag == NO_EVENT ||
              (teventp[k].flag & TIME_EVENTS) != flag  ||
              teventp[k].done )
            continue;
         if ( is_compatible(&teventp[j], &teventp[k]) ) {
            matched = 1;
            break;
         }
      }

      if ( matched ) {
         /* Two tevents found which can be combined into one timer */
         indx = timer_index(timerpp);
         copy_tevents_to_timer( j, k, indx, teventp, *timerpp );

         /* Remove these tevents from further consideration */
         teventp[j].done = 1;
         teventp[k].done = 1;
         /* Link to existing chain */
         (*timerpp)[endch].link = indx;
         (*timerpp)[indx].link = -1;
         endch = indx;
      }
   }

   /* Next try to combine any leftover Dawn/Dusk events with */
   /* each other if allowed.                                 */
   if ( configp->res_overlap == RES_OVLAP_COMBINED ) {
      for ( j = 0; j < tnum; j++ ) {
         if ( teventp[j].generation != current_tevent_generation ||
              teventp[j].flag == NO_EVENT || 
              teventp[j].flag & (CLOCK_EVENT | CHAIN_EVENT) ||
              teventp[j].done )
            continue;

         matched = 0;
         for ( k = j + 1; k < tnum; k++ ) {
            if ( teventp[k].generation != current_tevent_generation ||
                 teventp[k].flag == NO_EVENT || 
                 teventp[k].flag & (CLOCK_EVENT | CHAIN_EVENT) ||
                 teventp[k].done )
               continue;
            if ( is_compatible(&teventp[j], &teventp[k]) ) {
               matched = 1;
               break;
            }
         }

         if ( matched ) {
            /* Two tevents found which can be combined into one timer */
            indx = timer_index(timerpp);
            copy_tevents_to_timer( j, k, indx, teventp, *timerpp );

            /* Remove these tevents from further consideration */
            teventp[j].done = 1;
            teventp[k].done = 1;
            /* Link to existing chain */
            (*timerpp)[endch].link = indx;
            (*timerpp)[indx].link = -1;
            endch = indx;
         }
         else {
            /* Singleton tevent; set the timer stop event to null */
            indx = timer_index(timerpp);
            copy_tevents_to_timer( j, -1, indx, teventp, *timerpp );

            /* Remove this tevent from further consideration */
            teventp[j].done = 1;
            /* Link to existing chain */
            (*timerpp)[endch].link = indx;
            (*timerpp)[indx].link = -1;
            endch = indx;
         }
      }
   }

   /* Finally, try to combine all remaining events, excluding */
   /* Dawn and Dusk combinations.                             */
   for ( j = 0; j < tnum; j++ ) {
      if ( teventp[j].generation != current_tevent_generation ||
           teventp[j].flag == NO_EVENT ||
           teventp[j].flag & CHAIN_EVENT || 
           teventp[j].done )
         continue;

      matched = 0;
      flag = teventp[j].flag & (DAWN_EVENT | DUSK_EVENT);
      for ( k = j + 1; k < tnum; k++ ) {
         if ( teventp[k].generation != current_tevent_generation ||
              teventp[k].flag == NO_EVENT ||
              teventp[k].flag & CHAIN_EVENT ||
              (teventp[k].flag & (DAWN_EVENT | DUSK_EVENT) && flag)  || 
              teventp[k].done )
            continue;
         if ( is_compatible(&teventp[j], &teventp[k]) && (configp->fix_stopstart_error == NO) ) {
            matched = 1;
            break;
         }
      }

      if ( matched ) {
         /* Two tevents found which can be combined into one timer */
         indx = timer_index(timerpp);
         copy_tevents_to_timer( j, k, indx, teventp, *timerpp );

         /* Remove these tevents from further consideration */
         teventp[j].done = 1;
         teventp[k].done = 1;
         /* Link to existing chain */
         (*timerpp)[endch].link = indx;
         (*timerpp)[indx].link = -1;
         endch = indx;
      }
      else {
         /* Singleton tevent; set the timer stop event to null */
         indx = timer_index(timerpp);
         copy_tevents_to_timer( j, -1, indx, teventp, *timerpp );

         /* Remove this tevent from further consideration */
         teventp[j].done = 1;
         /* Link to existing chain */
         (*timerpp)[endch].link = indx;
         (*timerpp)[indx].link = -1;
         endch = indx;
      }
   }
   return indx;
}

/*---------------------------------------------------------------------+
 | Store a sequence number 1-7 with each trigger corresponding to its  |
 | order among those triggers which execute the same macro.            |
 | Return non-zero if the sequence number is greater than 7, which is  |
 | the largest number the CM11A hardware can report (reserving 7 for   |
 | cases of 7 or higher) to unambiguously identify the particular      | 
 | trigger.                                                            |
 +---------------------------------------------------------------------*/
int set_trigger_tags ( TRIGGER *triggerp )
{
   int j, count, warn;
   int index[1000];

   if ( triggerp == NULL )
      return 0;

   for ( j = 0; j < 1000; j++ )
      index[j] = 0;

   j = 0;
   warn = 0;
   while ( triggerp[j].line_no > 0 ) {
      count = ++index[triggerp[j].macro];
      if ( count > 6 )
         warn = 1;
      triggerp[j].tag = min(7, count);
      j++;
   }

   return warn;
}


/*----------------------------------------------------------------------------+
 | Mark the macros actually in use in the current program interval            |
 +----------------------------------------------------------------------------*/
void identify_macros_in_use ( MACRO *macrop, TEVENT *teventp )
{
   extern int current_tevent_generation;

   int j;

   /* First, those called by triggers.  Others not */
   j = 0;
   while ( macrop[j].label[0] != '\0' ) {
      /* Set all offsets to zero */
      macrop[j].offset = 0;

      if ( macrop[j].refer & TRIGGER_PARSER )
         macrop[j].use = USED;
      else
         macrop[j].use = NOTUSED;
      j++;
   }

   /* Then those called by tevents which are active in this interval */
   if ( teventp == NULL )
      return;

   j = 0;
   while ( teventp[j].line_no > 0 ) {
      if ( teventp[j].generation == current_tevent_generation &&
           teventp[j].flag != NO_EVENT ) {
         macrop[teventp[j].macro].use = USED;
      }
      j++;
   }

   return;
}

/*----------------------------------------------------------+
 | Create separate tevents for any interval where dawn/dusk |
 | tevent intervals partially overlap, so that on any given |
 | date each tevent will have the same value for dawn and/or|
 | dusk.                                                    |
 +----------------------------------------------------------*/
void resolve_tevent_overlap_comb ( TEVENT **teventpp )
{
   extern int    current_tevent_generation;
   int           breaker[366];
   unsigned int  tflag, flag[366];
   int           intv[732];
   int           beg, end;
   int           i, j, val, indx, cindx, size, nintv;

   if ( !(*teventpp) )
      return;

   if ( verbose )
      (void)printf("Entering resolve_tevent_overlap_comb() at generation %d\n", 
              current_tevent_generation);

   /* If there are no dawn/dusk tevents, there's nothing */
   /* to do here.                                       */

   i = 0;
   val = 0;
   while ( (*teventpp)[i].line_no > 0 ) {
      if ( (*teventpp)[i].generation != current_tevent_generation ) {
         i++;
         continue;
      }
      if ( (*teventpp)[i].flag & (DAWN_EVENT | DUSK_EVENT) )
         val++;
      i++;
   }

   if ( val == 0 )
      return;


   /* Initialize arrays for a breaker and for flags */
   for ( j = 0; j < 366; j++ ) {
      breaker[j] = 0;
      flag[j] = 0;
   }

   /* Fill in the breaker array for dawn/dusk tevents.   */
   /* Increment the generation of clock tevents          */
   /* and we're done with them.                          */
   i = 0;
   while ( (*teventpp)[i].line_no > 0 ) {
      if ( (*teventpp)[i].generation != current_tevent_generation ) {
         i++;
         continue;
      }

      if ( (*teventpp)[i].flag & (DAWN_EVENT | DUSK_EVENT) ) {
         for ( j = (*teventpp)[i].resolv_beg; j <= (*teventpp)[i].resolv_end; j++ )
            breaker[j] += 1;
      }
      else {
         (*teventpp)[i].flag2 = 0;
         increment_tevent_generation( *teventpp, i );
      }
      i++;
   }


   size = 0;
   i = 0;
   while ( (*teventpp)[i].line_no > 0 ) {
      if ( (*teventpp)[i].generation != current_tevent_generation ) {
         i++;
         continue;
      }

      tflag = (*teventpp)[i].flag & (DAWN_EVENT | DUSK_EVENT) ;
      beg = (*teventpp)[i].resolv_beg;
      end = (*teventpp)[i].resolv_end;
      val = breaker[beg];
      (*teventpp)[i].intv = size;
      intv[size++] = beg;
      for ( j = beg; j <= end; j++ ) {
         /* Flags for all sun timers are OR'd together. */
         flag[j] |= tflag;
         if ( val != breaker[j] ) {
            if ( j > beg ) {
               intv[size++] = j - 1;
               intv[size++] = j;
            }
            val = breaker[j];
         }
      }
      intv[size++] = end;
      (*teventpp)[i].nintv = (size - (*teventpp)[i].intv) / 2;
      i++;
   }

   i = 0;
   while ( (*teventpp)[i].line_no > 0 ) {
      if ( (*teventpp)[i].generation != current_tevent_generation ) {
         i++;
         continue;
      }

      nintv = (*teventpp)[i].nintv;
      indx = (*teventpp)[i].intv + 2 * (nintv - 1);
      for ( j = 0; j < nintv; j++ ) {
         beg = intv[indx];
         end = intv[indx + 1];
         indx -= 2;
         tflag = flag[beg];

         cindx = spawn_child_tevent( teventpp, i );

         /* The child tevent flag2 indicates whether to resolve */
         /* sun times based on dawn only, dusk only, or both.   */

         (*teventpp)[cindx].flag2 |= tflag;
         (*teventpp)[cindx].resolv_beg = beg;
         (*teventpp)[cindx].resolv_end = end;
      }
      i++;
   }

   (void) update_current_tevent_generation();

   return;
}

/*----------------------------------------------------------+
 | Create separate tevents for any interval where dawn/dusk |
 | tevent intervals partially overlap, so that on any given |
 | date each tevent will have the same value for dawn and/or|
 | dusk.                                                    |
 | This alternate function does dawn and dusk separately.   |
 +----------------------------------------------------------*/
void resolve_tevent_overlap_sep ( TEVENT **teventpp )
{
   extern int    current_tevent_generation;
   int           breaker[366];
   unsigned int  dflag;
   int           intv[732];
   int           beg, end;
   int           i, j, k, val, indx, cindx, size, nintv;

   if ( !(*teventpp) )
      return;

   if ( verbose )
      (void)printf("Entering resolve_tevent_overlap_sep() at generation %d\n", 
              current_tevent_generation);

   /* If there are no dawn/dusk tevents, there's nothing */
   /* to do here.                                       */

   i = 0;
   val = 0;
   while ( (*teventpp)[i].line_no > 0 ) {
      if ( (*teventpp)[i].generation != current_tevent_generation ) {
         i++;
         continue;
      }
      if ( (*teventpp)[i].flag & (DAWN_EVENT | DUSK_EVENT) )
         val++;
      i++;
   }
   if ( val == 0 )
      return;

   /* Increment the generation of non-dawn/dusk tevents  */
   /* and then we're done with them.                     */
   i = 0;
   while ( (*teventpp)[i].line_no > 0 ) {
      if ( (*teventpp)[i].generation != current_tevent_generation ||
           (*teventpp)[i].flag & (DAWN_EVENT | DUSK_EVENT) ) {
         i++;
         continue;
      }
      (*teventpp)[i].flag2 = 0;
      increment_tevent_generation( *teventpp, i );

      i++;
   }

   /* Set dflag for the first pass through the loop */
   dflag = DAWN_EVENT;
   for ( k = 0; k < 2; k++ ) {
      /* Initialize arrays for a breaker and for flags */
      for ( i = 0; i < 366; i++ ) {
         breaker[i] = 0;
      }

      /* Fill in the breaker array */
      i = 0;
      while ( (*teventpp)[i].line_no > 0 ) {
         if ( (*teventpp)[i].generation != current_tevent_generation ||
              ((*teventpp)[i].flag & dflag) == 0 ) {
            i++;
            continue;
         }
         for ( j = (*teventpp)[i].resolv_beg; j <= (*teventpp)[i].resolv_end; j++ )
            breaker[j] += 1;
         i++;
      }

      size = 0;
      i = 0;
      while ( (*teventpp)[i].line_no > 0 ) {
         if ( (*teventpp)[i].generation != current_tevent_generation ||
              ((*teventpp)[i].flag & dflag) == 0 ) {
            i++;
            continue;
         }

         beg = (*teventpp)[i].resolv_beg;
         end = (*teventpp)[i].resolv_end;
         val = breaker[beg];
         (*teventpp)[i].intv = size;
         intv[size++] = beg;
         for ( j = beg; j <= end; j++ ) {
            if ( val != breaker[j] ) {
               if ( j > beg ) {
                  intv[size++] = j - 1;
                  intv[size++] = j;
               }
               val = breaker[j];
            }
         }
         intv[size++] = end;
         (*teventpp)[i].nintv = (size - (*teventpp)[i].intv) / 2;
         i++;
      }

      i = 0;
      while ( (*teventpp)[i].line_no > 0 ) {
         if ( (*teventpp)[i].generation != current_tevent_generation ||
              ((*teventpp)[i].flag & dflag) == 0 ) {
            i++;
            continue;
         }
  
         nintv = (*teventpp)[i].nintv;
         indx = (*teventpp)[i].intv + 2 * (nintv - 1);
         for ( j = 0; j < nintv; j++ ) {
            beg = intv[indx];
            end = intv[indx + 1];
            indx -= 2;

            cindx = spawn_child_tevent( teventpp, i );

            /* The child tevent flag2 indicates whether to resolve */
            /* sun times based on dawn only, dusk only, or both.   */

            (*teventpp)[cindx].flag2 |= dflag;
            (*teventpp)[cindx].resolv_beg = beg;
            (*teventpp)[cindx].resolv_end = end;
         }
         i++;
      }
      /* Set dflag for second pass through the loop */
      dflag = DUSK_EVENT;
   }

   (void) update_current_tevent_generation();

   return;
}



/*---------------------------------------------------------+
 | Manage a binary search for the highest integer yielding |
 | a "good" result, i.e., last_result => 0, such that      |
 | integer+1 yields a "bad" result, i.e., last_result < 0. |
 | The function returns 0 until such time as the value in  |
 | next_try is the desired integer, when it then returns 1.|
 +---------------------------------------------------------*/
int iter_mgr ( int last_result, long *next_try,
                                 long max_step, int *restart )
{
   static long last_bad, last_good ;
   static int  stage;
   long        step;

   if ( *restart ) {
      *restart = 0;
      stage = 0 ;
   }

   switch ( stage ) {
      case 0 :
         /* Start by trying 0 */
         stage = 1;
         *next_try = 0L;
         return 0;
      case 1 :
         /* Then try 1 */
         stage = 2 ;
         if ( last_result < 0 ) {
            last_bad = 0L;
            *next_try = 1L;
            return 0;
         }
         else {
            return 1;
         }
      case 2 :
         /* Now keep doubling until we get a "good" result */
         if ( last_result < 0 ) {
            last_bad = *next_try;
            step = min( *next_try + 1L, max_step );
            *next_try += step;
            return 0;
         }
         else {
            last_good = *next_try;
            if ( last_result == 0 || (last_good - last_bad) == 1L )
               return 1;
            else {
               stage = 3;
               *next_try = (last_bad + last_good) / 2L;
               return 0;
            }
         }
      case 3 :
         /* Now keep splitting the range until the values  */
         /* yielding good and bad results differ by only 1 */
         if ( last_result < 0 ) {
            last_bad = *next_try;
            *next_try = max((last_bad + last_good)/2L, last_bad + 1L);
            return 0;
         }
         else {
            if ( last_result == 0L || (last_good - last_bad) == 1L )
               return 1;
            last_good = *next_try;
            *next_try = max((last_bad + last_good)/2L, last_bad + 1L);
            return 0;
         }
   }
   return 0;   /* Keep the compiler happy */
}

/*---------------------------------------------------------+
 | Return the fixed time approximating dawn or dusk times  |
 | over an interval from the array of daily values, the    |
 | beginning and ending year days, and the option chosen   |
 | by the user.  Recognized options (defines) are: FIRST,  |
 | EARLIEST, LATEST, AVERAGE, and MEDIAN.                  |
 +---------------------------------------------------------*/
int set_suntime ( int *daily_sun, int begin_day, int end_day, 
                       unsigned char option, int *error )
{
   int      rmin, rmax, dsj, value;
   int      j;
   long int sum;

   if ( begin_day > end_day ) {
      (void)fprintf(stderr, 
         "set_suntime(): Begin day (%d) > End day (%d)\n",
               begin_day, end_day);
      return -1;
   }

   rmin = rmax = daily_sun[begin_day];
   sum = 0L;
   for ( j = begin_day; j <= end_day; j++ ) {
      dsj = daily_sun[j];
      sum += (long)dsj;
      rmin = min(rmin, dsj);
      rmax = max(rmax, dsj);
   }
   *error = rmax - rmin;

   switch ( option ) {
      case FIRST :
         /* Use the value from the first day of the interval */
         value = daily_sun[begin_day];
         break;
      case EARLIEST :
         /* Use earliest value over the interval */
         value = rmin;
         break;
      case LATEST :
         /* Use latest value over the interval */
         value = rmax;
         break;
      case AVERAGE :
         /* Use average value over the interval */
         value = (int)(sum / (long)(end_day - begin_day + 1));
         break;
      case MEDIAN :
         /* Use median value over the interval */
         value = (rmin + rmax) / 2;
         break;
      default :
         (void)fprintf(stderr,
           "set_suntime(): Option (%d) not recognized.", option);
         value = -1;
         break;
   }

   return value;
}

/*---------------------------------------------------------+
 | For each timer with dawn/dusk based events, create one  |
 | or more new timers with subintervals of the date range, |
 | with the subintervals chosen to minimize the error      |
 | in the dawn/dusk times over that subinterval subject to |
 | the available CM11a memory free space.                  |
 | Then resolve the dawn/dusk based times into clock times.|
 | The resulting error in dawn/dusk times from actual      |
 | (as determined by our sunrise/sunset calculator) is     |
 | passed back to the caller.                              |
 +---------------------------------------------------------*/
int resolve_sun_times( TIMER **timerpp, CALEND *calendp,
             int freespace, int *tot_timers, int *max_error )
{
   extern int    line_no, current_timer_generation;

   double        latitude, longitude;
   time_t        tzone;

   long          julianday;
   long          errval;

   unsigned int  size, sizelimit /*, totintv = 0 */;

   int           dawn[366], dusk[366], scode[366], dummy[366], dst[366];
   int           *ptr1 = NULL, *ptr2 = NULL;
   int           *intvp = NULL;
   int           i, j, ii;
   int           r, rmin, rmax, s, smin, smax, iter;
   int           value;
   int           indx, cindx;
   int           result = -1, restart;
   int           dbegin, dend;
   int           year, month, day, yday;
   int           dawnerr, duskerr, error;
   int           sun_timers, res_timers;
   int           status = 0, totintv = 0;

   enum        { STtime, DLtime };


   /* Report error as zero in case of early return */
   *max_error = 0;

   /* If no timers at all, simply return */
   if ( !(*timerpp) ) {
      *tot_timers = 0;
      *max_error = 0;
      return 0;
   }

   if ( verbose )
      (void)printf("Entering resolve_sun_times() at timer generation %d\n",
                current_timer_generation); 

   /* Get geographic location and timezone from   */
   /* that stored in the global CONFIG structure. */

   if ( configp->loc_flag != (LATITUDE | LONGITUDE) ) {
      (void)fprintf(stderr, 
         "LATITUDE and/or LONGITUDE not specified in %s\n",
	    pathspec(CONFIG_FILE));
      exit(1);
   }

   latitude  = configp->latitude;
   longitude = configp->longitude;
   tzone     = configp->tzone;

   /* Get date information from the CALEND structure */

   year        = calendp->year;
   month       = calendp->month;
   day         = calendp->mday;
   yday        = calendp->yday;

   /* Count the timers which are already fully resolved and  */
   /* count the timers needing dawn/dusk time resolution.    */
   /* Change the generation of the fully-resolved timers to  */
   /* the next generation.                                   */

   j = 0;
   res_timers = sun_timers = 0;
   while ( (*timerpp)[j].line_no > 0 ) {
      if ( (*timerpp)[j].generation != current_timer_generation ||
           (*timerpp)[j].flag_combined == NO_EVENT ) {
         j++;
         continue;
      }

      if ( ((*timerpp)[j].flag_combined & TIME_EVENTS) == CLOCK_EVENT ) {
         res_timers++;
      }
      else {
         sun_timers++;
      }
      j++;
   }

   /* If there are no timers needing dawn/dusk resolution,   */
   /* we're done here and can return.                        */

   if ( sun_timers == 0 ) {
      *tot_timers = res_timers;
      *max_error = 0;
      (void) update_current_timer_generation();
      return 0;
   }


   /* Calculate sunrise and sunset for 366 days from today's */
   /* date and store in arrays.                              */

   /* Get today's Julian Day - the big number, not the day   */
   /* of the year.                                           */

   julianday = daycount2JD(calendp->today);

   for ( j = 0; j < 366; j++ ) {
      scode[j] = suntimes( latitude, longitude, tzone, julianday,
         configp->sunmode, configp->sunmode_offset, &dawn[j], &dusk[j], NULL, NULL );
      julianday++ ;
   }

   /* Also create a dummy array */
   for ( j = 0; j < 366; j++ )
      dummy[j] = 0;

   /* For Arctic/Antarctic regions - Substitute a dawn and/or  */
   /* dusk time for days when the sun is continually above or  */
   /* below the horizon, or no sunrise or sunset.              */

   for ( j = 0; j < 366; j++ ) {
      if ( scode[j] & UP_ALL_DAY ) {
         dawn[j] = 1;     /* 00:01 */
	 dusk[j] = 1438;  /* 23:58 */
      }
      else if ( scode[j] & DOWN_ALL_DAY ) {
	 dawn[j] = 1438;
	 dusk[j] = 1;
      }
      else if ( scode[j] & NO_SUNRISE ) {
	 dawn[j] = 1;
      }
      else if ( scode[j] & NO_SUNSET ) {
	 dusk[j] = 1438;
      }
   }

   /* Put upper and/or lower bounds on dawn if specified in    */
   /* the config file.                                         */

   if ( configp->min_dawn != OFF || configp->max_dawn != OFF ) {
      for ( j = 0; j < 366; j++ ) 
         dst[j] = time_adjust(j + yday, dawn[j], LGL2STD);
      if ( configp->min_dawn != OFF ) {
         for ( j = 0; j < 366; j++ ) 
            dawn[j] = max(dawn[j], configp->min_dawn - dst[j]);
      }
      if ( configp->max_dawn != OFF ) {
         for ( j = 0; j < 366; j++ )
            dawn[j] = min(dawn[j], configp->max_dawn - dst[j]);
      }  
   }

   /* Put upper and/or lower bounds on dusk if specified in    */
   /* the config file.                                         */

   if ( configp->min_dusk != OFF || configp->max_dusk != OFF ) {
      for ( j = 0; j < 366; j++ ) 
         dst[j] = time_adjust(j + yday, dusk[j], LGL2STD);
      if ( configp->min_dusk != OFF ) {
         for ( j = 0; j < 366; j++ ) 
            dusk[j] = max(dusk[j], configp->min_dusk - dst[j]);
      }
      if ( configp->max_dusk != OFF ) {
         for ( j = 0; j < 366; j++ )
            dusk[j] = min(dusk[j], configp->max_dusk - dst[j]);
      }
   }  

   /* In the following section, the date interval specified in */
   /* a timer is broken up into subintervals, each subinterval */
   /* having the same sunrise and/or sunset time error over    */
   /* the subinterval.  The error is minimized subject to the  */
   /* constraint of the available freespace in the CM11a       */
   /* memory - each subinterval will require a separate timer  */
   /* occupying 9 bytes of memory.                             */

   /* Set pointers to arrays depending whether clock or        */
   /* dawn/dusk timers and store them in the timer structure.  */

   i = 0;
   while (  (*timerpp)[i].line_no > 0  ) {
      if ( (*timerpp)[i].generation != current_timer_generation ||
           (*timerpp)[i].flag_combined == NO_EVENT ) {
         i++;
         continue;
      }

      switch ( (*timerpp)[i].flag_combined & (DAWN_EVENT | DUSK_EVENT) ) {
         case DAWN_EVENT :
            (*timerpp)[i].ptr_start = dawn;
            (*timerpp)[i].ptr_stop  = dawn;
            break;

         case DUSK_EVENT :
            (*timerpp)[i].ptr_start = dusk;
            (*timerpp)[i].ptr_stop  = dusk;
            break;

         case (DAWN_EVENT | DUSK_EVENT) :
            (*timerpp)[i].ptr_start = dawn;
            (*timerpp)[i].ptr_stop  = dusk;
            break;

         default :
            (*timerpp)[i].ptr_start = dummy;
            (*timerpp)[i].ptr_stop  = dummy;
            break;
      }

      /* If both the start and stop pointers are the same */
      /* we need use only one of them.                    */

      (*timerpp)[i].num_ptr =
        (*timerpp)[i].ptr_start == (*timerpp)[i].ptr_stop ? 1 : 2 ;

      i++;
   }

   /* Now start the iteration to determine the minimum    */
   /* error in dawn and or dusk times consistant with     */
   /* the available freespace in the CM11a memory.        */

   /* Each iteration starts with a specified error in     */
   /* the dawn or dusk value and determines the total     */
   /* number of intervals required.  Then iterates the    */
   /* error until the total number of intervals will just */
   /* fit in the available CM11a memory freespace.        */

   /* Upper limit to speed up the iteration and prevent   */
   /* possible integer overflow in weird cases.           */
   sizelimit = 2 * PROMSIZE / 9 + 100 ;

   iter = 0;
   restart = 1;
   while ( iter < 1000 && !iter_mgr( result, &errval,
                                         1000L, &restart) ) {
      iter++;
      totintv = 0;
      size = 0;

      i = 0;
      while ( (*timerpp)[i].line_no > 0 ) {

         if ( size > sizelimit )
            break;

         if ( (*timerpp)[i].generation != current_timer_generation ||
              (*timerpp)[i].flag_combined == NO_EVENT ) {
            i++;
            continue;
         }

         (*timerpp)[i].intv = size;
         ptr1 = (*timerpp)[i].ptr_start;
         ptr2 = (*timerpp)[i].ptr_stop;

         indx = 0;
         dbegin = (*timerpp)[i].resolv_beg;
         dend   = (*timerpp)[i].resolv_end;
         ii = intv_index(&intvp, &size);
         intvp[ii] = dbegin;

         switch ( (*timerpp)[i].num_ptr ) {
            case 1 :
               rmin = rmax = ptr1[dbegin];
               for ( j = dbegin; j <= dend; j++ ) {
                  r = ptr1[j];
                  rmin = min(rmin, r); rmax = max(rmax, r);
                  if ( (rmax - rmin) > errval ) {
                     if ( j > dbegin ) {
                        ii = intv_index(&intvp, &size);
                        intvp[ii] = j-1;
                        ii = intv_index(&intvp, &size);
                        intvp[ii] = j;
                     }
                     rmin = rmax = r;
                  }
               }
               break;

            case 2 :
               rmin = rmax = ptr1[dbegin];
               smin = smax = ptr2[dbegin];
               for ( j = dbegin; j <= dend; j++ ) {
                  r = ptr1[j]; s = ptr2[j];
                  rmin = min(rmin, r); smin = min(smin, s);
                  rmax = max(rmax, r); smax = max(smax, s);
                  if ( (rmax - rmin) > errval ||
                       (smax - smin) > errval     ) {
                     if ( j > dbegin ) {
                        ii = intv_index(&intvp, &size);
                        intvp[ii] = j-1;
                        ii = intv_index(&intvp, &size);
                        intvp[ii] = j;
                     }
                     rmin = rmax = r;
                     smax = smin = s;
                  }
               }
               break;
         }
         ii = intv_index(&intvp, &size);
         intvp[ii] = dend;

         (*timerpp)[i].nintv = (size - (*timerpp)[i].intv)/2 ;
         i++;
      }
      totintv = size/2;

      result = freespace > 9 * totintv ?  1 :
               freespace < 9 * totintv ? -1 : 0  ;

      /* If the error exceeds 2 days ( 2880 minutes ) at this */
      /* point there's no point in trying further.            */

      if ( errval > (2*24*60) ) {
         (void) fprintf(stderr, "Insufficient CM11a memory.\n");
         return 1;
      }
   }

   /* Check that the iteration limit has not been exceeded */
   if ( iter >= 1000 ) {
      (void) fprintf(stderr, "Iteration limit exceeded.\n");
      return 1;
   }

   /* These are the max error in dawn/dusk times from "actual" */
   /* and the total number of timers to be programmed.         */
   *max_error = (int)errval;
   *tot_timers = (int)totintv;

   /* In the following, a new timer is created for each  */
   /* date subinterval and linked in a chain to the      */
   /* original (parent) timers.  The offsets from dawn   */
   /* and dusk are replaced by clock times.              */

   i = 0;
   while ( (*timerpp)[i].line_no > 0 ) {

      if ( (*timerpp)[i].generation != current_timer_generation ||
           (*timerpp)[i].flag_combined == NO_EVENT  ) {
         i++;
         continue;
      }

      line_no = (*timerpp)[i].line_no ;
      dawnerr = duskerr = 0;

      indx = (*timerpp)[i].intv;
      for ( j = 0; j < (*timerpp)[i].nintv; j++ ) {

         dbegin = intvp[indx];
         dend   = intvp[indx + 1];
         indx += 2;

         /* Create a child timer */
         cindx = spawn_child_timer( timerpp, i );

         (*timerpp)[cindx].resolv_beg = dbegin;
         (*timerpp)[cindx].resolv_end = dend;

         /* Add the fixed time for dawn/dusk to the offsets therefrom */
         /* stored in the timer structures.                           */

         switch ( (*timerpp)[cindx].flag_start & (DAWN_EVENT | DUSK_EVENT) ) {
            case DAWN_EVENT :
               value = set_suntime(dawn, dbegin, dend, configp->dawn_option, &error);
               (*timerpp)[cindx].offset_start += value;
               (*timerpp)[cindx].error_start = error;
               break; 
            case DUSK_EVENT :
               value = set_suntime(dusk, dbegin, dend, configp->dusk_option, &error);
               (*timerpp)[cindx].offset_start += value;
               (*timerpp)[cindx].error_start = error;
               break;
            default :
               value = 0;
               (*timerpp)[cindx].error_start = 0;
               break;
         }

         if ( (*timerpp)[cindx].flag_start != NO_EVENT  &&  
             ( (*timerpp)[cindx].offset_start < 0  || 
                         (*timerpp)[cindx].offset_start > 1439 ) ) {
              (void)fprintf(stderr,
                  "Line %d: Expanded timer[%d] start time %d falls outside range 0-1439\n",
                      (*timerpp)[cindx].line1, cindx, (*timerpp)[cindx].offset_start );
              status = 1;
         }

         switch ( (*timerpp)[cindx].flag_stop & (DAWN_EVENT | DUSK_EVENT)) {
            case DAWN_EVENT :
               value = set_suntime(dawn, dbegin, dend, configp->dawn_option, &error);
               (*timerpp)[cindx].offset_stop += value;
               (*timerpp)[cindx].error_stop = error;
               break; 
            case DUSK_EVENT :
               value = set_suntime(dusk, dbegin, dend, configp->dusk_option, &error);
               (*timerpp)[cindx].offset_stop += value;
               (*timerpp)[cindx].error_stop = error;
               break;
            default :
               value = 0;
               (*timerpp)[cindx].error_stop = 0;
               break;
         }

         if ( (*timerpp)[cindx].flag_stop != NO_EVENT &&
                 ( (*timerpp)[cindx].offset_stop < 0  ||
                   (*timerpp)[cindx].offset_stop > 1439 ) ) {
              (void)fprintf(stderr,
                  "Line %02d: Expanded timer[%d] stop time %d falls outside range 0-1439\n",
                      (*timerpp)[cindx].line2, cindx, (*timerpp)[cindx].offset_stop );              
              status = 1;
         } 
      }             /* end j loop */
      i++ ;
   }             /* end i loop */

   /* We're finished with the array of intervals */
   free( intvp );
   intvp = NULL;

   (void) update_current_timer_generation();

   return status;
}


/*---------------------------------------------------------------------+
 | Return the amount of CM11a memory available (bytes) when that used  |
 | by triggers, macros, timers, internal pointers and list terminators |
 | is subtracted from the total size.  Entering with one of the        |
 | structure pointers set to NULL will exclude it from the count.      |
 +---------------------------------------------------------------------*/
int get_freespace( int generation, TIMER *timerp, TRIGGER *triggerp,
                                                         MACRO *macrop )
{
   int   j;
   int   timclk, timsun, timcount, trig, mac, macsize, used;

   j = 0; timclk = 0; timsun = 0;
   while ( timerp && timerp[j].line_no >= 0 ) {
      if ( timerp[j].generation != generation  ||
           timerp[j].flag_combined == NO_EVENT   ) {
         j++;
         continue;
      }
      if ( (timerp[j].flag_combined & TIME_EVENTS) == CLOCK_EVENT )
         timclk++ ;
      else if ( timerp[j].flag_combined & (DAWN_EVENT | DUSK_EVENT) )
         timsun++ ;
      else
         (void) fprintf(stderr, "Timer error: Index = %d  Flag = %02xh\n",
              j, (timerp[j]).flag_combined );
      j++;
   }
   timcount = timclk + timsun ;

   j = 0; trig = 0;
   while ( triggerp && triggerp[j++].line_no >= 0 )
      trig++;

   j = 0; mac = 0; macsize = 0;
   while ( macrop && macrop[j].line_no >= 0 ) {
      if ( !macrop[j].isnull && macrop[j].use == USED ) {
         mac++;
         macsize += macrop[j].total;
      }
      j++;
   }

   /* Note: This formula MUST be the same as that used */
   /* in function create_memory_image_[high|low]().               */
   used = 2                           /* Offset of Trigger table */
      + 9 * timcount + 1              /* Timers + terminator */
      + 3 * trig + 2                  /* Triggers + terminator */
      + 2 * mac + macsize             /* User defined macros */
      + 2;                            /* Terminator */
   if ( configp->macterm == YES )
      used += mac;

   return (PROMSIZE - used) ;
}

/*---------------------------------------------------------------------+
 | Create the memory image to be downloaded to the CM11a               |
 | This function loads the macros at the top of memory.                |
 | Note: The formula used to compute free space at the end of function |
 | get_freespace() must agree with what is done in this function.      |
 +---------------------------------------------------------------------*/
void create_memory_image_high ( unsigned char *prommap, TIMER *timerp,
             TRIGGER *triggerp, MACRO *macrop, unsigned char *elementp )
{
   extern   int  current_timer_generation;

   int           j, k, size, offset;
   int           begin, end, start, stop;
   int           macro1, macro2;
   unsigned int  security1, security2;

   /* Initialize memory image */
   for ( j = 0; j < PROMSIZE; j++ )
      prommap[j] = 0xff;

   /* Store zeros in the last two bytes for the terminator */
   prommap[PROMSIZE - 1] = 0;
   prommap[PROMSIZE - 2] = 0;

   offset = PROMSIZE - 2;

   /* Establish the offset of each macro in the eeprom and store  */
   /* in the memory image.      Macros are loaded at the top of   */
   /* memory but in the order they were defined, i.e., the last   */
   /* macro defined by a macro command in the user's schedule     */
   /* will be highest in memory, just below the terminator.       */

   /* Get the total space to be occupied by the macros */
   size = 0;
   j = 0;
   while ( macrop[j].line_no > 0 ) {
      if ( !macrop[j].isnull && macrop[j].use == USED ) {
         if ( configp->macterm == YES )
            size += macrop[j].total + 3;
         else
            size += macrop[j].total + 2;
      }
      j++;
   }

   offset -= size;

   macrop[NULL_MACRO_INDEX].offset = 0;

   j = 0;
   while ( macrop[j].line_no > 0 ) {
      if ( !macrop[j].isnull && macrop[j].use == USED ) {
         /* Save the offset */
         macrop[j].offset = offset;

         prommap[offset++] = macrop[j].delay;

         /* Make sure the macro doesn't have too many elements */
         if ( macrop[j].nelem > 255 ) {
            fprintf(stderr, "Combined macro %s has too many (%d) elements.\n",
               macrop[j].label, macrop[j].nelem);
            exit(1);
         }

         prommap[offset++] = (unsigned char)macrop[j].nelem;

         for ( k = 0; k < macrop[j].total; k++ )  {
            prommap[offset++] = elementp[macrop[j].element + k];
         }
         if ( configp->macterm == YES )
            prommap[offset++] = 0;
      }
      j++;
   }

   /* Store current timers */
   j = 0; offset = 2;
   while ( timerp && timerp[j].line_no > 0 ) {
      if ( timerp[j].generation != current_timer_generation  ||
           timerp[j].flag_combined == NO_EVENT  ) {
         j++;
         continue;
      }

      prommap[offset]   = timerp[j].dow_bmap ;
      timerp[j].memloc = offset;
      begin = timerp[j].resolv_beg;
      end   = timerp[j].resolv_end;
      prommap[offset+1] = (unsigned char)(begin & 0xff) ;
      prommap[offset+2] = (unsigned char)(end   & 0xff) ;

      /* (The extra steps here handle the case of null macros) */
      start = min(0x0f, timerp[j].offset_start / 120);
      stop  = min(0x0f, timerp[j].offset_stop / 120);
      prommap[offset+3] = (unsigned char)((start << 4) | stop);

      start = 0x7f & (timerp[j].offset_start - 120 * start);
      stop  = 0x7f & (timerp[j].offset_stop  - 120 * stop);

      prommap[offset+4] = (unsigned char)(((begin & 0x100) >> 1) | start);
      prommap[offset+5] = (unsigned char)(((end & 0x100) >> 1) | stop);

      security1 = timerp[j].flag_start & SEC_EVENT ? SECURITY_ON : 0;
      security2 = timerp[j].flag_stop  & SEC_EVENT ? SECURITY_ON : 0;
      macro1 = macrop[timerp[j].macro_start].offset;
      macro2 = macrop[timerp[j].macro_stop ].offset;
      prommap[offset+6] = (unsigned char)((macro1 & 0x300) >> 4)   |
                          (unsigned char)((security1 & 0x03u) << 6)  |
                          (unsigned char)((macro2 & 0x300) >> 8)   |
                          (unsigned char)((security2 & 0x03u) << 2);

      prommap[offset+7] = (unsigned char)(macro1 & 0xff);
      prommap[offset+8] = (unsigned char)(macro2 & 0xff);

      offset += 9;

      j++;
   }
   /* Terminate the timers */
   prommap[offset++] = 0xff ;

   /* Load offset of trigger table into location 0 */
   prommap[0] = (unsigned char)((offset & 0xff00) >> 8);
   prommap[1] = (unsigned char)(offset & 0xff);

   /* Now store the triggers @ 3 bytes each + 2 terminators */
   j = 0;
   while ( triggerp && triggerp[j].line_no > 0 ) {
      triggerp[j].memloc = offset;
      prommap[offset++]   = triggerp[j].housecode << 4 |
                          triggerp[j].unitcode;
      macro1 = macrop[triggerp[j].macro].offset;
      prommap[offset++] = (unsigned char)((triggerp[j].command) << 7) |
                          (triggerp[j].tag << 4) |
                          (unsigned char)((macro1 & 0xf00) >> 8);
      prommap[offset++] = (unsigned char)(macro1 & 0xff);
      j++ ;
   }
   /* Terminate with 2 bytes of 0xff */
   prommap[offset++] = 0xff;
   prommap[offset++] = 0xff;

   return ;
}

/*---------------------------------------------------------------------+
 | Create the memory image to be downloaded to the CM11a               |
 | This one loads the macros immediately folowing the triggers.        |
 | Note: The formula used to compute free space at the end of function |
 | get_freespace() must agree with what is done in this function.      |
 +---------------------------------------------------------------------*/
void create_memory_image_low ( unsigned char *prommap, TIMER *timerp,
             TRIGGER *triggerp, MACRO *macrop, unsigned char *elementp )
{
   extern   int  current_timer_generation;

   int           j, k, offset;
   int           begin, end, start, stop;
   int           macro1, macro2;
   unsigned int  security1, security2;

   /* Initialize memory image */
   for ( j = 0; j < PROMSIZE; j++ )
      prommap[j] = 0x00;


   /* Add up the space required for timers and triggers */
   offset = 2;  /* Initial jump instruction */
   j = 0;
   while ( timerp && timerp[j].line_no > 0 ) {
      if ( timerp[j].generation != current_timer_generation  ||
           timerp[j].flag_combined == NO_EVENT  ) {
         j++;
         continue;
      }
      offset += 9;
      j++;
   }

   offset += 1;  /* Timer terminator */

   /* Now count the triggers @ 3 bytes each + 2 terminators */
   j = 0;
   while ( triggerp && triggerp[j].line_no > 0 ) {
      offset += 3;
      j++;
   }

   offset += 2;  /* Trigger terminator */

   /* Load the macros, storing their offsets */

   macrop[NULL_MACRO_INDEX].offset = 0;

   j = 0;
   while ( macrop[j].line_no > 0 ) {
      if ( !macrop[j].isnull && macrop[j].use == USED ) {
         /* Save the offset */
         macrop[j].offset = offset;

         prommap[offset++] = macrop[j].delay;

         /* Make sure the macro doesn't have too many elements */
         if ( macrop[j].nelem > 255 ) {
            fprintf(stderr, "Combined macro %s has too many (%d) elements.\n",
               macrop[j].label, macrop[j].nelem);
            exit(1);
         }

         prommap[offset++] = (unsigned char)macrop[j].nelem;

         for ( k = 0; k < macrop[j].total; k++ )  {
            prommap[offset++] = elementp[macrop[j].element + k];
         }
         if ( configp->macterm == YES )
            prommap[offset++] = 0;
      }
      j++;
   }

   /* Store two zeros for the terminator */
   prommap[offset++] = 0;
   prommap[offset++] = 0;

   /* Go back and load the timers and triggers. */

   j = 0; offset = 2;
   while ( timerp && timerp[j].line_no > 0 ) {
      if ( timerp[j].generation != current_timer_generation  ||
           timerp[j].flag_combined == NO_EVENT  ) {
         j++;
         continue;
      }

      prommap[offset]   = timerp[j].dow_bmap ;
      timerp[j].memloc = offset;
      begin = timerp[j].resolv_beg;
      end   = timerp[j].resolv_end;
      prommap[offset+1] = (unsigned char)(begin & 0xff) ;
      prommap[offset+2] = (unsigned char)(end   & 0xff) ;

      /* (The extra steps here handle the case of null macros) */
      start = min(0x0f, timerp[j].offset_start / 120);
      stop  = min(0x0f, timerp[j].offset_stop / 120);
      prommap[offset+3] = (unsigned char)((start << 4) | stop);

      start = 0x7f & (timerp[j].offset_start - 120 * start);
      stop  = 0x7f & (timerp[j].offset_stop  - 120 * stop);

      prommap[offset+4] = (unsigned char)(((begin & 0x100) >> 1) | start);
      prommap[offset+5] = (unsigned char)(((end & 0x100) >> 1) | stop);

      security1 = timerp[j].flag_start & SEC_EVENT ? SECURITY_ON : 0;
      security2 = timerp[j].flag_stop  & SEC_EVENT ? SECURITY_ON : 0;
      macro1 = macrop[timerp[j].macro_start].offset;
      macro2 = macrop[timerp[j].macro_stop ].offset;
      prommap[offset+6] = (unsigned char)((macro1 & 0x300) >> 4)   |
                          (unsigned char)((security1 & 0x03u) << 6)  |
                          (unsigned char)((macro2 & 0x300) >> 8)   |
                          (unsigned char)((security2 & 0x03u) << 2);

      prommap[offset+7] = (unsigned char)(macro1 & 0xff);
      prommap[offset+8] = (unsigned char)(macro2 & 0xff);

      offset += 9;

      j++;
   }
   /* Terminate the timers */
   prommap[offset++] = 0xff ;

   /* Load offset of trigger table into location 0 */
   prommap[0] = (unsigned char)((offset & 0xff00) >> 8);
   prommap[1] = (unsigned char)(offset & 0xff);

   /* Now store the triggers @ 3 bytes each + 2 terminators */
   j = 0;
   while ( triggerp && triggerp[j].line_no > 0 ) {
      triggerp[j].memloc = offset;
      prommap[offset++]   = triggerp[j].housecode << 4 |
                          triggerp[j].unitcode;
      macro1 = macrop[triggerp[j].macro].offset;
      prommap[offset++] = (unsigned char)((triggerp[j].command) << 7) |
                          (triggerp[j].tag << 4) |
                          (unsigned char)((macro1 & 0xf00) >> 8);
      prommap[offset++] = (unsigned char)(macro1 & 0xff);
      j++ ;
   }
   /* Terminate with 2 bytes of 0xff */
   prommap[offset++] = 0xff;
   prommap[offset++] = 0xff;

   return ;
}


/*---------------------------------------------------------------------+
 | Write the EEPROM memory image to disk as a pure binary file.        |
 +---------------------------------------------------------------------*/
int write_image_bin ( char *pathname, unsigned char *prommap )
{
   FILE   *fd_bin;

   if ( !(fd_bin = fopen(pathname, "wb")) ) {
      (void) fprintf(stderr,
         "Unable to open CM11a memory image binary file '%s' for writing.\n",
            pathname);
      return 1;
   }

   if ( fwrite( prommap, 1, PROMSIZE, fd_bin) != PROMSIZE ) {
      (void) fprintf(stderr, "Unable to write memory image file '%s'\n",
                       pathname );
      (void) fclose( fd_bin );
      return 1;
   }
   (void) fclose( fd_bin );

   return 0;
}

/*---------------------------------------------------------------------+
 | Write the EEPROM memory image to disk as a hexadecimal dump.        |
 +---------------------------------------------------------------------*/
int write_image_hex ( char *pathname, unsigned char *prommap )
{
   FILE    *fd_hex;
   char    outbuf[80];
   char    buf[16];

   int     j, k, loc;

   if ( !(fd_hex = fopen(pathname, "w")) ) {
      (void) fprintf(stderr,
         "Unable to open CM11a memory image hex dump file '%s' for writing.\n",
            pathname);
      return 1;
   }

   loc = 0;
   for ( j = 0; j < PROMSIZE; j += 16 ) {
      (void) sprintf(outbuf, "%03X   ", loc);
      for ( k = 0; k < 8; k++ ) {
         (void) sprintf(buf, "%02X ", prommap[j+k]);
         (void) strncat(outbuf, buf, sizeof(outbuf) - 1 - strlen(outbuf));
      }
      (void) strncat(outbuf, " ", sizeof(outbuf) - 1 - strlen(outbuf));
      for ( k = 8; k < 16; k++ ) {
         (void) sprintf(buf, "%02X ", prommap[j+k]);
         (void) strncat(outbuf, buf, sizeof(outbuf) - 1 - strlen(outbuf));
      }
      (void) fprintf(fd_hex, "%s\n", outbuf);
      loc += 16;
   }
   (void) fclose(fd_hex);
   return 0;
}

/*---------------------------------------------------------------------*
 | Store information to be written to or read from the X10record file  |
 | in one place.                                                       |
 +---------------------------------------------------------------------*/
void store_record_info ( CALEND *calendp )
{
   
   x10record.isready = 1;
   x10record.dayset = calendp->today;
   x10record.yday  = calendp->yday;
   x10record.day_zero = calendp->day_zero;
   x10record.tzone    = configp->tzone;
   x10record.flags = ( timer_size > 0 ) ? HAS_TIMERS : 0;
   x10record.flags |= configp->mode;
   x10record.dstminutes = configp->dstminutes;
   x10record.program_days = configp->program_days;

   return;
}

/*---------------------------------------------------------------------*
 | Function to delete the X10record file; for use when the CM11a       |
 | EEPROM is erased.                                                   |
 +---------------------------------------------------------------------*/
void remove_record_file ( void )
{
   int   code;

   if ( verbose )
      printf("Deleting X10 Record File %s\n", pathspec(RECORD_FILE));

   code = remove( pathspec(RECORD_FILE) ) ;
   if ( code != 0 && errno != 2 ) {
      (void)fprintf(stderr, 
         "WARNING: Unable to delete X10 Record File %s - errno = %d\n",
          pathspec(RECORD_FILE), errno);
   }
   return;
}

/*---------------------------------------------------------------------+
 | Write a file with configuration and calendar information which      |
 | heyu will need to save to properly set the CM11a clock-calendar at  |
 | any later time.                                                     |
 +---------------------------------------------------------------------*/
int write_record_file ( char *pathname, CALEND *calendp )
{
   FILE *fd;

   store_record_info( calendp );

   if ( !(fd = fopen( pathname, "w" )) ) {
      (void) fprintf(stderr, "Unable to open record file '%s' for writing.\n",
         pathname);
      exit(1);
   }
   
   (void)fprintf(fd, "# Generated by heyu - do not delete or modify.\n");

   (void)fprintf(fd, "# Config:   %s\n", pathspec(heyu_config));
   (void)fprintf(fd, "# Schedule: %s\n", schedfile);
   (void)fprintf(fd, "# Uploaded: %s\n", legal_time_string());

   (void)fprintf(fd, "%ld %d %d %ld %d %d %d\n",
       x10record.dayset,
       x10record.yday,
       x10record.day_zero,
       x10record.tzone,
       x10record.flags,
       x10record.dstminutes,
       x10record.program_days);

   (void) fclose(fd);

   return 0;
}


/*---------------------------------------------------------------------+
 | Read info from x10record file and store in record_info structure.   |
 | Return one of the following values:                                 |
 |  VALID_RECORD_FILE                                                  |
 |  NO_RECORD_FILE                                                     |
 |  BAD_RECORD_FILE  (i.e., corrupted)                                 |
 +---------------------------------------------------------------------*/
int read_record_file ( void )
{
   extern int verbose, i_am_relay;

   FILE *fd;
   char buffer[256];
   int  n, lines;

   x10record.isready = 0;

   if ( verbose && i_am_relay != 1 ) {
      (void)printf("Searching for %s\n", pathspec(RECORD_FILE));
   }
   
   if ( !(fd = fopen( pathspec(RECORD_FILE), "r" )) ) {
      if ( verbose && i_am_relay != 1 ) {
         (void)printf("File %s is absent\n", pathspec(RECORD_FILE));
      }
      return NO_RECORD_FILE;
   }

   if ( verbose && i_am_relay != 1 ) {
      (void)printf("%s is present\n", pathspec(RECORD_FILE));
   }
   
   n = 0; lines = 0;
   while ( fgets(buffer, 256, fd) != NULL ) {
     lines++;
     (void)strtrim(buffer);
     if ( *buffer == '#' || *buffer == '\0' )
        continue;
     n = sscanf(buffer, "%ld %d %d %ld %u %d %d",
       &x10record.dayset,
       &x10record.yday,
       &x10record.day_zero,
       &x10record.tzone,
       &x10record.flags,
       &x10record.dstminutes,
       &x10record.program_days);

     break;
   }
   (void) fclose( fd );

   /* A zero-length record file is equivalent to no record file. */
   if ( lines == 0 ) {
      if ( verbose && i_am_relay != 1 ) {
         (void)printf("File %s is empty\n", pathspec(RECORD_FILE));
      }
      return NO_RECORD_FILE;
   }
   
   if ( n != 7 ) {
      if ( verbose && i_am_relay != 1 ) {
         (void)printf("File %s is corrupted\n", pathspec(RECORD_FILE));
      }
      return BAD_RECORD_FILE;
   }

   x10record.isready = 1;

   return VALID_RECORD_FILE;
}


/*---------------------------------------------------------------------+
 | Return the number of days >= 0 until expiration of an uploaded      |
 | schedule from the x10 Record File and the system date.              |
 |   Return SCHEDULE_EXPIRED if the schedule has expired.              |
 |   Return NO_EXPIRATION if there are no timers defined.              |
 |   Return NO_RECORD_FILE if no (or empty) record file.               |
 |   Return BAD_RECORD_FILE if the record file is corrupted.           | 
 +---------------------------------------------------------------------*/
int get_upload_expire ( void ) 
{
   int     retcode, elapsed, expire;
   long    daynow;
   time_t  now;
   
   retcode = read_record_file();
   
   if ( retcode != VALID_RECORD_FILE )
      return retcode;

   if ( !(x10record.flags & HAS_TIMERS) )
      return NO_EXPIRATION;

   time(&now);
   daynow = ((long)now - x10record.tzone)/86400L;

   elapsed = (int)(daynow - x10record.dayset) + x10record.yday - x10record.day_zero;
   expire = x10record.program_days - elapsed;

   if ( expire < 0 )
      return SCHEDULE_EXPIRED;

   return expire;
}

/*---------------------------------------------------------------------+
 | Translate the readings from the CM11a clock to Legal Time, using    |
 | what is recorded in the X10 Record File (if one exists), otherwise  | 
 | assume the CM11a clock is set to Standard Time.  The arguments are: |
 |    *Idaysp   Day of Week bitmap (Sun = 1, Sat = 64)                 |
 |    *Ijdayp   Day of Year counter (0-365)                            |
 |    *Ihrp     Hours (0-23)                                           |
 |    *Iminp    Minutes (0-59)                                         |
 |    *Isecp    Seconds (0-59)                                         |
 |    *expire   Days until expiration of uploaded schedule (0-365) or  |
 |              invalidation code (-1 to -4)                           |
 +---------------------------------------------------------------------*/
struct tm *cm11a_to_legal ( int *Idaysp, int *Ijdayp, int *Ihrp,
	           int *Iminp, int *Isecp, int *expire )
{

   struct tm     *tms, *tmp;
   static struct tm tmstat;  
   time_t        dtimep;
   long          daynow;
   int           delta;
   unsigned char bmap;

   /* Get the user's timezone */
   get_std_timezone();

   /* Fix TZ names if necessary */
   fix_tznames();
   
   tms = &tmstat;
   
   time(&dtimep);
   tmp = stdtime(&dtimep);
   memcpy((void *)tms, (void *)tmp, sizeof(struct tm));

   /* Load the required data into the x10record structure if the program */
   /* has not already done so.                                           */
   *expire = get_upload_expire();

   if ( *expire != NO_RECORD_FILE && *expire != BAD_RECORD_FILE ) {
      delta = *Ijdayp - x10record.yday + x10record.day_zero;
      daynow = x10record.dayset + (long)delta;
   }
   else {
      daynow = *Ijdayp - tms->tm_yday + ((long)dtimep - std_tzone)/86400L ;
   }
 
   dtimep = (time_t)3600 * (time_t)(*Ihrp) + (time_t)60 * (time_t)(*Iminp) +
     (time_t)(*Isecp) + (time_t)86400 * (time_t)daynow + (time_t)std_tzone;
   tmp = localtime(&dtimep);
   memcpy((void *)tms, (void *)tmp, sizeof(struct tm));

   /* Adjust the displayed day of the week if the day has changed */
   /* into the previous or following day                          */
   if ( tms->tm_hour < *Ihrp ) {
      bmap = (unsigned char)(*Idaysp);
      *Idaysp = (int)lrotbmap(bmap);
   }
   
   tms->tm_wday = bmap2wday((unsigned char)(*Idaysp));
   *Ihrp = tms->tm_hour;
   *Iminp = tms->tm_min;
   *Isecp = tms->tm_sec;
   *Ijdayp = tms->tm_yday;

   return tms;
}
	
/*---------------------------------------------------------------------+
 | Return a pointer to a tm structure containing the appropriate       |
 | settings for the CM11a clock based on the argument time_t timep and |
 | what is recorded in the x10 Record File (if one exists).            |
 +---------------------------------------------------------------------*/
struct tm *legal_to_cm11a ( time_t *dtimep )
{
   extern int      i_am_relay;

   static struct tm tmstat;

   struct tm     *tms, *tmp;
   time_t        seconds;
   long          daynow;
   int           note;

   /* Maintain our own static structure because the structure built in */
   /* to the standard time functions is overwritten each call.         */
   tms = &tmstat;

   /* Load the record file data into the x10record structure */
   (void) read_record_file();

   tmp = stdtime(dtimep);
   memcpy( (void *)tms, (void *)tmp, sizeof(struct tm) );

   /* Just return the pointer if no (or corrupt) x10record file */
   /* or if the schedule does not expire because there are no   */
   /* timers defined.                                           */
   if ( x10record.isready == 0 || !(x10record.flags | HAS_TIMERS) ) {
      return tms;
   }
   
   /* Force the date to be greater than or the same as the date the CM11a */
   /* was programmed by incrementing the year if necessary. (We can't set */
   /* the CM11a clock to a negative day count.)                           */
 
   note = 0;
   while ( *dtimep - (time_t)std_tzone < (time_t)(x10record.dayset * 86400L) ) {
      tms->tm_year += 1;
      *dtimep = mktime(tms);
      note = 1;
   }
   if ( note && i_am_relay != 1 ) {
      (void)fprintf(stderr, 
         "Date adjusted forward to year %d\n", tms->tm_year + 1900);
   }

   seconds = *dtimep - (time_t)std_tzone;

   /* Convert to Standard Time */
   if ( tms->tm_isdst > 0 ) {
      tmp = gmtime(&seconds);
      memcpy( (void *)tms, (void *)tmp, sizeof(struct tm) );
   }

   daynow = (long)seconds/86400L;
   
   /* Set the value of the yday according to the Heyu programming mode */
   tms->tm_yday = (int)(daynow - x10record.dayset);
   return tms;
}    

/*---------------------------------------------------------------------+
 | Display status message                                              |
 +---------------------------------------------------------------------*/
void display_status_message ( int expire )
{
   switch ( expire ) {
     case NO_RECORD_FILE :
      (void) fprintf(stdout, "No schedule has been uploaded by Heyu.\n");
      break;
     case NO_EXPIRATION :
      (void) fprintf(stdout, "Uploaded schedule does not expire.\n");
      break;
     case BAD_RECORD_FILE :
      (void) fprintf(stdout, "X10 Record File '%s' has been corrupted.\n", 
                              pathspec(RECORD_FILE));
      break;
     case SCHEDULE_EXPIRED :
      (void) fprintf(stdout, "Uploaded schedule has expired.\n");
      break;
     default :
      (void) fprintf(stdout, "Uploaded schedule will expire in %d days.\n",
               expire);
      break;
   }
   return;
}

/*---------------------------------------------------------------------+
 | Display CM11a status and required CM11a clock settings.             |
 | display_mode = 1 displays human-readable message.                   |
 | display_mode = 0 displays only the code, for use with scripts.      | 
 +---------------------------------------------------------------------*/
void display_cm11a_status ( int display_mode )
{
   int expire;

   expire = get_upload_expire();

   if ( display_mode == 1 )
      display_status_message( expire );
   else 
      (void) fprintf(stdout, "%d\n", expire);

   return;
}

/*---------------------------------------------------------------------+
 | qsort() compare function for write_macroxref()                      |
 +---------------------------------------------------------------------*/
int compmac( struct macindx *one, struct macindx *two )
{
   return (one->offset - two->offset) ;
}

/*---------------------------------------------------------------------+
 | Write a macro xref table, i.e., label vs offset in EEPROM, to disk. |
 | Optionally append macro helper tokens.                              |
 +---------------------------------------------------------------------*/
int write_macroxref ( char *pathname, MACRO *macrop, unsigned char *elementp, int ichksum )
{
   FILE          *fd ;
   int           i, j, k, m, index, mask, count;
   unsigned char cmdcode, hcode;
   unsigned int  bmap;
   char          hc;
   int           (*fptr)() = &compmac;

   static int strucsize = sizeof(struct macindx);
   static char *label[] = {"", "", "On","Off", "Dim", "Bright"};

   struct macindx *macp;

   if ( !(fd = fopen(pathname, "w")) ) {
      (void) fprintf(stderr,
         "Unable to open macro xref file '%s' for write.\n", pathname);
      return 1;
   }

   /* Write the image checksum as a label at address 0 */
   fprintf(fd, "   0  %d\n", ichksum);


   /* Count macros in use */

   count = 0;
   j = 0;
   while ( macrop[j].line_no > 0 ) {
      if ( !macrop[j].isnull && macrop[j].use == USED )
         count++;
      j++;
   }

   if ( (macp = calloc( count, strucsize  )) == NULL ) {
      fprintf(stderr, "write_macroxref() - Unable to allocate memory.\n");
      exit(1);
   }

   count = 0;
   j = 0;
   while ( macrop[j].line_no > 0 ) {
      if ( !macrop[j].isnull && macrop[j].use == USED ) {
         macp[count].index = j;
         macp[count].offset = macrop[j].offset ;
         count++;
      }
      j++ ;
   }

   qsort ( macp, count, sizeof(struct macindx), fptr );

   for ( j = 0; j < count; j++ ) {
      index = macp[j].index;
      (void) fprintf(fd, "%4d  %s",
         macp[j].offset, macrop[index].label);

      if ( configp->xref_append == YES ) {
         i = macrop[index].element ;
         for ( k = 0; k < macrop[index].nelem; k++ ) {
            cmdcode = elementp[i] & 0x0fu;
            hcode = (elementp[i] & 0xf0u) >> 4;
            hc = tolower((int)code2hc(hcode));
            bmap = elementp[i+1] << 8 | elementp[i+2];

            if ( cmdcode > 1 && cmdcode < 6 ) {  
               mask = 1;
               for ( m = 0; m < 16; m++ ) {
                  if ( bmap & mask )   
                     fprintf(fd, " %c%d%s", hc, bitpos2unit(m), label[cmdcode]);
                  mask = mask << 1;
               }
               i += (cmdcode < 4) ? 3 : 4;
            }
            else {
               i += (cmdcode == 7) ? 6 : 3;
            }
         }
      }
      fprintf(fd, "\n");
   }

   (void) fclose(fd);

   free( macp );

   return 0;
}

/*---------------------------------------------------------------------+
 | Compute a 12 bit checksum of the memory image                       |
 +---------------------------------------------------------------------*/
int image_chksum ( unsigned char *prommap )
{
   long int      sum = 0;
   unsigned char *sp;

   sp = prommap;
   while ( sp < (prommap + PROMSIZE) ) 
      sum += (long)(*sp++);

   return (int)(sum & 0x0fff);
}

/*---------------------------------------------------------------------+
 | Return the macro label corresponding to the argument address in the |
 | macro xref file.  Return "Unknown" if no file or no address match.  |
 | If the file includes the checksum of the image file (entry at       |
 | address zero), pass it back through the argument list.              | 
 +---------------------------------------------------------------------*/
int lookup_macro ( int address, char *maclabel, int *ichksum )
{
   FILE        *fd;
   char        *sp;
   char        buffer[127];
   char        minibuf[MACRO_LEN + 1];

   *ichksum = -1;

   if ( !(fd = fopen(pathspec(MACROXREF_FILE), "r")) ) {
      strncpy2(maclabel, "_unknown_", MACRO_LEN);
      return 0;
   }

   while ( fgets(buffer, sizeof(buffer)/sizeof(char), fd) != NULL ) {
      if ( (int)strtol(buffer, &sp, 10) == 0 ) {
         get_token(minibuf, &sp, " \t\n", MACRO_LEN);
         *ichksum = (int)strtol(minibuf, &sp, 10);
      }
      else if ( (int)strtol(buffer, &sp, 10) == address ) {
         get_token(minibuf, &sp, " \t\n", MACRO_LEN);
         strncpy2(maclabel, minibuf, MACRO_LEN);
         fclose(fd);
         return 1;
      }
   }
   fclose(fd);
   strncpy2(maclabel, "_unknown_", MACRO_LEN);
   return 0;
}
   
/*---------------------------------------------------------------------+
 | Return the macro EEPROM address corresponding to the argument macro |
 | name argument in the x10macroxref file.  Return -1 if not found.    |
 | If the file includes the checksum of the image file (entry at       |
 | address zero), pass it back through the argument list.              | 
 +---------------------------------------------------------------------*/
int macro_rev_lookup ( char *macname, int *ichksum )
{
   FILE        *fd;
   char        *sp;
   char        buffer[127];
   int         val, macaddr;
   int         tokc;
   char        **tokv;

   if ( !(fd = fopen(pathspec(MACROXREF_FILE), "r")) )
      return -1;

   *ichksum = -1;
   macaddr = -1;

   while ( fgets(buffer, sizeof(buffer)/sizeof(char), fd) != NULL ) {
      tokenize(buffer, " \t\n", &tokc, &tokv);
      if ( tokc < 2 ) {
         free(tokv);
         continue;
      }
      if ( (val = (int)strtol(tokv[0], &sp, 10)) == 0 && *sp == '\0' )  
         *ichksum = (int)strtol(tokv[1], &sp, 10);
      else if ( strcmp(tokv[1], macname) == 0 ) {  
         macaddr = val;
         free(tokv);
         break;
      }
      free(tokv);
   }
   fclose(fd);

   return macaddr;
}
   

/*---------------------------------------------------------------------+
 | This function supervises the whole job of reading the config and    |
 | schedule files and creating the CM11a memory image file.  It also   |
 | creates a bunch of other files for reference and/or debugging.      |
 |                                                                     |
 | If the argument is PROC_UPLOAD, the memory image is uploaded to the |
 | CM11a interface, the X10 Record File is written, and the CM11a      |
 | clock is set to the appropriate time.                               |
 +---------------------------------------------------------------------*/
int process_data ( int proc_code )
{
   FILE          *fd;

   TIMER         *timerp = NULL;
   TEVENT        *teventp = NULL;
   TRIGGER       *triggerp = NULL;
   MACRO         *macrop = NULL;
   CALEND        today, *calendp;

   unsigned char prommap[PROMSIZE];

   unsigned char *elementp = NULL;
   char          *sp;
   int           retcode, freespace, tot_timers, max_error;
   int           ss_error;
   int           ichksum;
   int           now_time = -1;
   extern int    c_setclock(int, char **);
   extern void   upload_eeprom_image(unsigned char *);


   /* Open and parse the x10 configuration file and store */
   /* the information in the global CONFIG structure.     */

   get_configuration(CONFIG_INIT);
   line_no = 0;

   /* See if the user has specified the full path for a schedule   */
   /* file by a command line option or in an environment variable, */
   /* otherwise use what's been specified in the configuration     */
   /* file (or its default).                                       */

   if ( !((sp = optptr->schedp) || (sp = getenv("X10SCHED"))) ) 
      sp = pathspec(configp->schedfile);

   (void)strncpy2(schedfile, sp, sizeof(schedfile) - 1);

   /* Open the x10 schedule file */      
   if ( !(fd = fopen(schedfile, "r")) ) {
      (void)fprintf(stderr, "Unable to open schedule file '%s'\n",
                                           schedfile);
      exit(1);
   }

   (void) printf("Schedule: %s\n", schedfile);

   /* Parse the schedule file and store the data in structure arrays */
   retcode = parse_sched ( fd, &timerp, &triggerp, &macrop, &elementp );
   (void) fclose( fd );

   if ( retcode ) {
      (void) fprintf(stderr,
         "Quitting due to errors in schedule file '%s'\n", schedfile); 
      exit(1);
   }

   if ( !timerp && !triggerp ) {
      (void) fprintf(stderr,
         "No Timers or Triggers defined in schedule file %s\n", schedfile);
      exit(1);
   }

   /* Get today's date information and store it in a */
   /* CALEND structure.                              */

   calendp = &today;
   calendar_today( calendp );

   /* Split the timers into individual timed events for start and stop */
   split_timers( timerp, &teventp );

   /* Save the earliest macro execution time if a timer in the    */
   /* schedule file specified the time as "now[+NN]".             */
   now_time = get_first_now_time(teventp);   

   /* If a macro in a tevent is also called by a trigger, associate */
   /* the tevent with the trigger.                                  */
   associate_tevent_triggers( teventp, macrop );

   /* Replace delayed events with undelayed events having increased */
   /* offsets (and new macros) to compensate if requested           */
   if ( configp->repl_delay == YES )
      replace_delayed_events( &teventp, &macrop, &elementp );

   /* Combine similar tevents if requested */
   if ( configp->combine_events == YES )
      combine_similar_tevents( &teventp, &macrop, &elementp );

   /* Compress macros by merging unit codes for the same */
   /* HouseCode and Command.                             */
   if ( configp->compress_macros == YES )
      compress_macros( macrop, elementp );

   /* Adjust the times for events programmed with Security mode */
   security_adjust_legal( &teventp, &macrop, &elementp );

   /* Convert the programmed month/day ranges into day counts   */
   /* over the programmed interval and adjust purely Clock      */
   /* events for Daylight time if applicable.                   */

   resolve_dates( &teventp, calendp, configp->program_days );

   /* Resolve Dawn/Dusk timer options, if any */
   resolve_dawndusk_options( &teventp, calendp );

   clear_disabled_events( &teventp );

   /* Resolve overlapping date ranges for Dawn/Dusk relative    */
   /* events so that on any given day all events will have the  */
   /* same value for Dawn and/or Dusk.                          */

   if ( configp->res_overlap == RES_OVLAP_COMBINED ) 
      resolve_tevent_overlap_comb( &teventp ); /* Older method */
   else
      resolve_tevent_overlap_sep( &teventp );

   /* Mark macros actually in use over the programmed interval. */
   identify_macros_in_use(macrop, teventp);

   /* Internal check */
   verify_tevent_links( teventp );

   /* Recombine the individual tevents into timers */
   (void) reconstruct_timers( teventp, &timerp );

   /* Check total freespace before going any further. */
   freespace = get_freespace(current_timer_generation, timerp, triggerp, macrop );

   /* Subtract timer space we are holding in reserve */
   freespace -= 9 * configp->reserved_timers;

   if ( freespace < 0 ) {
      (void) fprintf(stderr,
         "Schedule too large: CM11a memory size exceeded by %d bytes ( = %.1f%% )\n",
             -freespace, 100. * (double)(-freespace)/(double)PROMSIZE);
      exit(1);
   }

   /* Get freespace excluding timers  */
   freespace = get_freespace(-1, NULL, triggerp, macrop)
                                           - 9 * configp->reserved_timers;

   /* Resolve any dawn/dusk based timers into a sequence of clock */
   /* based timers, choosing the date intervals so as to minimize */
   /* the error in dawn and/or dusk times over each interval      */
   /* subject to the constraint of the available freespace.       */

   retcode = resolve_sun_times( &timerp, calendp, freespace,
                                       &tot_timers, &max_error);
   if ( retcode ) {
      (void)fprintf(stderr,
               "Quitting due to errors.\n");
       return 1;
   }

   /* Set a sequence number for each trigger, warning if */
   /* the number exceeds 6.                              */
   if ( set_trigger_tags( triggerp ) ) {
      (void)printf("Warning: More than 6 triggers reference the same macro.\n");
   }

   /* Get the remaining freespace. */
   freespace = get_freespace( current_timer_generation, timerp, triggerp,
                                           macrop );
   
   if ( calendp->asif_flag & (ASIF_DATE | ASIF_TIME) )
      (void)printf("Simulation as if %s\n", asif_time_string());

   (void) printf("Expanded timers = %3d\n", tot_timers);
   (void) printf(
	"Max dawn/dusk error over the %d day period = %d minutes.\n",
                 configp->program_days, max_error);
   (void) printf(
       "Interface memory free = %d bytes ( = %.1f%% )\n",
                 freespace, 100.*(double)freespace/(double)PROMSIZE);

   /* Check for start time = stop time error */
   ss_error = check_timer_start_stop(timerp);

   /* Internal check */
    verify_timer_links( timerp );

   /* Create the memory image to be downloaded to the CM11a */
   #ifdef LOADLOW
   create_memory_image_low ( prommap, timerp, triggerp, macrop,
                                                    elementp );
   #else
   create_memory_image_high ( prommap, timerp, triggerp, macrop,
                                                    elementp );
   #endif  /* End of #ifdef */

   /* Compute a (16 bit) checksum for the memory image */
   ichksum = image_chksum( prommap );
   
   /* Create a report for the user */
   (void) final_report( altpathspec(REPORT_FILE), calendp, timerp,
                        teventp, triggerp, macrop, elementp, proc_code );
   if ( ss_error ) {
      (void) fprintf(stderr, "*** Internal error: stop time = start time ***\n");
      (void) fprintf(stderr, 
         "%d timers - see OUTPUT TIMERS section in file %s for details.\n", 
                            ss_error, altpathspec(REPORT_FILE));
      (void) fprintf(stderr,
         "Add line \"config FIX_STOPSTART_ERROR YES\" to schedule file and rerun.\n");
      (void) fprintf(stderr, "Quitting due to errors.\n");
      return 1;
   }
   else      
      (void) fprintf(stdout, "See file %s for details.\n", 
                                       altpathspec(REPORT_FILE));

   /* Upload the EEPROM image to the CM11a and create a file  */
   /* listing the dates and configuration which is necessary  */
   /* if we later want to update the CM11a clock or check the */
   /* status of the uploaded data.                            */

   if ( proc_code == PROC_UPLOAD ) {
      upload_eeprom_image( prommap );

      (void) write_record_file(pathspec(RECORD_FILE), calendp);

      (void) write_macroxref( pathspec(MACROXREF_FILE),
                                           macrop, elementp, ichksum);

      (void) write_image_bin( pathspec(IMAGE_FILE), prommap);

      (void) printf("Setting interface clock to current Standard Time.\n");
      c_setclock( 1, NULL );

   }
   else if ( configp->checkfiles == YES ) {   
      (void) printf("Writing .check and .hex files.\n");  

      /* Write the same record file with a ".check" extension */
      (void) write_record_file( pathspec(RECORD_FILE_CHECK), calendp );

      /* Write the same macroxref file with a ".check" extension */
      (void) write_macroxref( pathspec(MACROXREF_FILE_CHECK),
                                         macrop, elementp, ichksum);

      /* Write the same image file with a ".check" extension */
      (void) write_image_bin( pathspec(IMAGE_FILE_CHECK), prommap);

      /* Write the image file in hex format */ 
      (void) write_image_hex( pathspec(IMAGE_HEX_FILE), prommap );
   }
 
   /* Display "now" time, if any */
   if ( now_time >= 0 )
      (void) printf("The first 'now+NN' event macro will execute at %02d:%02d:00\n",
         now_time / 60, now_time % 60 );         

   free( timerp );
   free( teventp );
   free( triggerp );
   free( macrop );
   free( elementp );

   return 0;
}

/*---------------------------------------------------------------------+
 | This function simulates execution of heyu by cron on a daily basis  |
 | for the next 366 days, to insure there are no snags in the schedule |
 | file.  No files other than a cronreport file are written.           |
 +---------------------------------------------------------------------*/
int crontest ( void )
{
   FILE          *fd_sched, *fd_cron;

   TIMER         *timerp = NULL;
   TEVENT        *teventp = NULL;
   TRIGGER       *triggerp = NULL;
   MACRO         *macrop = NULL;
   unsigned char *elementp = NULL;

   CALEND        today, *calendp;

   int           retcode, freespace, minfree, maxfree;
   int           min_timers, max_timers, tot_timers;
   int           max_error, least_error, worst_error;
   int           j;
   char          *sp;
   
   /* Open and parse the x10 configuration file and store */
   /* the information in the global CONFIG structure.     */

   get_configuration(CONFIG_INIT);

   /* See if the user has specified the full path for a schedule   */
   /* file by a command line option or in an environment variable, */
   /* otherwise use what's been specified in the configuration     */
   /* file (or its default).                                       */

   if ( !((sp = optptr->schedp) || (sp = getenv("X10SCHED"))) ) 
      sp = pathspec(configp->schedfile);

   (void)strncpy2(schedfile, sp, sizeof(schedfile) - 1);

   /* Open the x10 schedule file */      
   if ( !(fd_sched = fopen(schedfile, "r")) ) {
      (void)fprintf(stderr, "Unable to open schedule file '%s'\n",
                                           schedfile);
      exit(1);
   }

   /* Parse the schedule file and store the data in structure arrays */
   retcode = parse_sched( fd_sched, &timerp, &triggerp, &macrop, &elementp );
   (void) fclose( fd_sched );

   if ( retcode ) {
      (void) fprintf(stderr,
         "Quitting due to errors in schedule file '%s'\n", schedfile);
      exit(1);
   }

   if ( configp->mode == COMPATIBLE ) {
      (void)fprintf(stderr, 
          "croncheck is not applicable when configured for COMPATIBLE mode.\n");
      exit(1);
   }




   /* Nothing to do if no timers */
   if ( !timerp ) {
      (void) printf("No timers found in schedule file - nothing to do.\n");
      exit(0);
   }

   /* Get today's date information and store it in a */
   /* a CALEND structure.                            */

   calendp = &today;
   calendar_today( calendp );

   /* Split the timers into individual timed events for start and stop */
   split_timers( timerp, &teventp );

   /* If a macro in a tevent is also called by a trigger, associate */
   /* the tevent with the trigger.                                  */
   associate_tevent_triggers ( teventp, macrop );

   /* Replace delayed events with undelayed events having increased */
   /* offsets (and new macros) to compensate if requested           */
   if ( configp->repl_delay == YES )
      replace_delayed_events ( &teventp, &macrop, &elementp );

   /* Combine similar tevents if requested */
   if ( configp->combine_events == YES )
      combine_similar_tevents ( &teventp, &macrop, &elementp );

   /* Compress macros by merging unit codes for the same */
   /* HouseCode and Command and eliminating duplicates.  */
   if ( configp->compress_macros == YES )
      compress_macros ( macrop, elementp );

   /* Adjust the times for events programmed with Security mode */
   security_adjust_legal ( &teventp, &macrop, &elementp );

   /* Save the timer and tevents up to this point */
   save_state ( timerp, teventp );

   /* Open a file to store the detailed results */
   if ( !(fd_cron = fopen(altpathspec(CRON_REPORT_FILE), "w")) ) {
      (void)fprintf(stderr, "Unable to open cron report file '%s'\n",
                    altpathspec(CRON_REPORT_FILE));
      exit(1);
   }

   (void)fprintf(fd_cron, "Results of Daily Operation for the next 366 Days\n");
   (void)fprintf(fd_cron, "================================================\n\n");

   least_error = 10000;
   worst_error = 0;

   minfree = 10000;
   maxfree = 0;

   min_timers  = 10000;
   max_timers  = 0;

   (void) fprintf(stdout,  "Begin date Time    Timers   Freespace   Dawn/Dusk error\n");
   (void) fprintf(stdout,  "---------- -----   ------  -----------  ---------------\n");
   (void) fprintf(fd_cron, "Begin date Time    Timers   Freespace   Dawn/Dusk error\n");
   (void) fprintf(fd_cron, "---------- -----   ------  -----------  ---------------\n");

   for ( j = 0; j < 366; j++ ) {
      (void) fprintf(stdout,  "%4d/%02d/%02d %02d:%02d    ",
             calendp->year, calendp->month, calendp->mday,
	     calendp->minutes / 60, calendp->minutes % 60);
      fflush(stdout);

      (void) fprintf(fd_cron, "%4d/%02d/%02d %02d:%02d    ",
             calendp->year, calendp->month, calendp->mday,
	     calendp->minutes / 60, calendp->minutes % 60);

      /* Convert the programmed month/day ranges into day counts   */
      /* over the programmed interval and adjust purely Clock      */
      /* events for Daylight time if applicable.                   */

      resolve_dates ( &teventp, calendp, configp->program_days );

      /* Resolve Dawn/Dusk timer options, if any */
      resolve_dawndusk_options( &teventp, calendp );

      clear_disabled_events( &teventp );

      /* Resolve overlapping date ranges for Dawn/Dusk relative    */
      /* events so that on any given day all events will have the  */
      /* same value for Dawn and/or Dusk.                          */

      if ( configp->res_overlap == RES_OVLAP_COMBINED ) 
         resolve_tevent_overlap_comb( &teventp ); /* Older method */
      else
         resolve_tevent_overlap_sep( &teventp );

      /* Mark macros actually in use over the programmed interval. */
      identify_macros_in_use(macrop, teventp);

      /* Recombine the individual tevents into timers */
      (void) reconstruct_timers( teventp, &timerp );

      /* Check total freespace before going any further. */
      freespace = get_freespace(current_timer_generation, timerp, triggerp, macrop );

      /* Subtract timer space we are holding in reserve */
      freespace -= 9 * configp->reserved_timers;

      if ( freespace < 0 ) {
         (void) fprintf(stderr,
            "Schedule too large: CM11a memory size exceeded by %d bytes ( = %.1f%% )\n",
                -freespace, 100. * (double)(-freespace)/(double)PROMSIZE);
         exit(1);
      }

      /* Get freespace excluding timers   */

      freespace = get_freespace(-1, NULL, triggerp, macrop )
                                               - 9 * configp->reserved_timers;

      /* Resolve any dawn/dusk based timers into a sequence of clock */
      /* based timers, choosing the date intervals so as to minimize */
      /* the error in dawn and/or dusk times over each interval      */
      /* subject to the constraint of the available freespace.       */

      retcode = resolve_sun_times( &timerp, calendp, freespace,
                                       &tot_timers, &max_error);
      if ( retcode ) {
         (void)fprintf(stderr,
                  "\nQuitting due to errors.\n");
          return 1;
      }

      min_timers = min(min_timers, tot_timers);
      max_timers = max(max_timers, tot_timers);

      least_error = min(least_error,  max_error);
      worst_error = max(worst_error, max_error);

      /* Get the remaining freespace. */
      freespace = get_freespace( current_timer_generation, timerp, triggerp,
                                        macrop );

      minfree = min(minfree, freespace);
      maxfree = max(maxfree, freespace);

      /* Display the results */
      (void) fprintf(fd_cron, "%3d   %4d (%4.1f%%)  %4d\n",
         tot_timers, freespace, 100.*(double)freespace/1024., max_error);

      (void) fprintf(stdout,  "%3d   %4d (%4.1f%%)  %4d\r",
         tot_timers, freespace, 100.*(double)freespace/1024., max_error);
      fflush(stdout);

      /* Check for start time = stop time error */
      if ( check_timer_start_stop(timerp) > 0 ) {
         (void) fprintf(stderr, "Internal error: stop time = start time\n");
         (void) fprintf(stderr, "CM11A Bug! - will not execute stop event macro\n");
         exit(1);
      }

      /* Advance our calendar by one day */
      advance_calendar( calendp, 1 );

      /* Restore timers and tevents to the state saved before the */
      /* first pass through the loop.                             */
      restore_state ( timerp, teventp );
   }

   (void) printf("Overall results:%40s\n", " ");
   (void) fprintf(stdout,  "Minimum             %3d   %4d (%4.1f%%)  %4d\n",
        min_timers, minfree, 100.*(double)minfree/1024., least_error);
   (void) fprintf(stdout,  "Maximum             %3d   %4d (%4.1f%%)  %4d\n",
        max_timers, maxfree, 100.*(double)maxfree/1024., worst_error);
   (void) fprintf(stdout, "See file %s for details.\n", altpathspec(CRON_REPORT_FILE));

   (void) fprintf(fd_cron, "\nOverall results:\n");
   (void) fprintf(fd_cron, "Minimum             %3d   %4d (%4.1f%%)  %4d\n",
        min_timers, minfree, 100.*(double)minfree/1024., least_error);
   (void) fprintf(fd_cron, "Maximum             %3d   %4d (%4.1f%%)  %4d\n",
        max_timers, maxfree, 100.*(double)maxfree/1024., worst_error);

   (void) fclose( fd_cron );

   free( timerp );
   free( teventp );
   free( triggerp );
   free( macrop );
   free( elementp );

   return 0;
}


/*---------------------------------------------------------------------+
 | Write a table of daily sunrise/set or twilight to disk using        |
 | location and timezone parameters from the configuration file.       |
 +---------------------------------------------------------------------*/
int write_sun_table ( int format, int year, int sunmode, int offset, int timemode )
{
   FILE *fd_sun;
   char filename[256];
   static char *fname[] = {"SunRiseSet", "CivilTwilight",
      "NautTwilight", "AstronTwilight", "SunAngleOffset", };

   get_configuration(CONFIG_INIT);

   if ( configp->loc_flag != (unsigned char)(LATITUDE | LONGITUDE) ) {
      fprintf(stderr,
       "LATITUDE and/or LONGITUDE not specified in %s\n",
                                         pathspec(CONFIG_FILE));
      return 1;
   }

   if ( format == FMT_PORTRAIT )
      sprintf(filename, "%s_%d.txt", altpathspec(fname[sunmode]), year);
   else
      sprintf(filename, "%s_%d_wide.txt", altpathspec(fname[sunmode]), year);

   if ( (fd_sun = fopen(filename, "w")) ) {
      printf("Writing file %s\n", filename);
      if ( format == FMT_PORTRAIT ) {
         display_sun_table(fd_sun, year, configp->tzone, sunmode, offset, timemode, 
             configp->lat_d, configp->lat_m, configp->lon_d, configp->lon_m);
      }
      else {
         display_sun_table_wide(fd_sun, year, configp->tzone, sunmode, offset, timemode, 
             configp->lat_d, configp->lat_m, configp->lon_d, configp->lon_m);
      }
   }
   else {
      fprintf(stderr, "Unable to open file '%s' for write\n",
                                                        filename);
      return 1;
   }

   return 0;
}


/*---------------------------------------------------------------------+
 | Convert a yday measured from Jan 1 of year0 to year, month, day.    |
 +---------------------------------------------------------------------*/
char *yday2datestr ( long jan1day, int yday )

{

   static char buffer[20];
   time_t     now;
   long       delta_days;

   if ( !configp->read_flag ) {
      get_std_timezone();
      configp->tzone = std_tzone;
   }

   delta_days = (long)yday + jan1day;
   now = (time_t)(86400L * delta_days + configp->tzone );

   gendate(buffer, now, YES, NO);

   return buffer;
}

/*---------------------------------------------------------------------+
 | Convert a yday measured from Jan 1 of year0 to year, month, day.    |
 +---------------------------------------------------------------------*/
void yday2date ( long jan1day, int yday, int *year, int *month,
                                                   int *mday, int *wday )

{

   time_t     now;
   struct tm  *tms;
   long       delta_days;

   if ( !configp->read_flag ) {
      get_std_timezone();
      configp->tzone = std_tzone;
   }

   delta_days = (long)yday + jan1day;
   now = (time_t)(86400L * delta_days + configp->tzone );

   tms = localtime( &now );
   *year  = tms->tm_year + 1900;
   *month = tms->tm_mon + 1;
   *mday  = tms->tm_mday;
   *wday  = tms->tm_wday;

   return;
}

/*---------------------------------------------------------------------+
 | Symbols for event flags.                                            |
 +---------------------------------------------------------------------*/
char flag_def( unsigned int flag )
{
   return (flag & CLOCK_EVENT)   ? 'C' :
          (flag & DAWN_EVENT)    ? 'R' :
          (flag & DUSK_EVENT)    ? 'S' : 'X' ;
}

/*---------------------------------------------------------------------+
 | Qsort compare function for display events()                         |
 +---------------------------------------------------------------------*/
int comp_events ( struct ev_s *one, struct ev_s *two )
{
   return
          (one->line > two->line) ?  1 :
          (one->line < two->line) ? -1 :
          (one->pos  > two->pos)  ?  1 :
          (one->pos  < two->pos)  ? -1 :
          (one->flag > two->flag) ?  1 :
          (one->flag < two->flag) ? -1 :
          (one->beg  > two->beg)  ?  1 :
          (one->beg  < two->beg)  ? -1 : 0 ;
}

/*---------------------------------------------------------------------+
 | Find the start of the tevent chain, i.e., the tevent not linked to  |
 | by any other.                                                       |
 +---------------------------------------------------------------------*/
int find_startchain ( TEVENT *teventp )
{
   int j, k, size, *evrlink;
   static int sizint = sizeof(int);

   size = 0;
   while ( teventp[size].line_no > 0 ) 
      size++;

   if ( (evrlink = calloc( size, sizint )) == NULL ) {
      fprintf(stderr, "find_startchain() - Unable to allocate memory.\n");
      exit(1);
   }

   for ( j = 0; j < size; j++ )
      evrlink[j] = 0;
 
   for ( j = 0; j < size; j++ ) {
      k = teventp[j].link;
      if ( k > (size - 1) ) {
         (void)fprintf(stderr, 
           "find_startchain() - link %d out of bound %d at index %d\n", k, size - 1, j);
         return -1;
      }
      if ( k >= 0 )
         evrlink[k] = 1;
   }
   for ( j = 0; j < size; j++ ) {
      if ( evrlink[j] == 0 )
         break;
   }

   free(evrlink);

   return j;
} 

  
/*---------------------------------------------------------------------+
 | Display the amount of EEPROM memory used by timers, triggers, and   |
 | macros.                                                             |
 +---------------------------------------------------------------------*/
void display_eeprom_usage ( FILE *fd, TIMER *timerp, TRIGGER *triggerp,
                                                         MACRO *macrop )
{
   int j;
   int sun_event, clk_event, ntimers, ntriggers, nmacros, macspace;
   int ovhead, free;

   sun_event = clk_event = ntimers = 0;
   j = 0;
   while ( timerp && timerp[j].line_no > 0 ) {
      if ( timerp[j].generation != current_timer_generation ||
           timerp[j].flag_combined == NO_EVENT ) {
         j++;
         continue;
      }
      ntimers++;
      if ( timerp[j].flag_start & (DAWN_EVENT | DUSK_EVENT) )
         sun_event++;
      else if ( timerp[j].flag_start & CLOCK_EVENT )
         clk_event++;
      if ( timerp[j].flag_stop & (DAWN_EVENT | DUSK_EVENT) )
         sun_event++;
      else if ( timerp[j].flag_stop & CLOCK_EVENT )
         clk_event++;

      j++;
   }

   ntriggers = 0;
   j = 0;
   while ( triggerp && triggerp[j].line_no > 0 ) {
      ntriggers++;
      j++;
   }
      
   nmacros = macspace = 0;
   j = 0;
   while ( macrop[j].line_no > 0 ) {
      if ( !macrop[j].isnull && macrop[j].use == USED ) {
         nmacros++;
         macspace += macrop[j].total + 2;
      }
      j++;
   }
   macspace += (configp->macterm == YES) ? nmacros : 0 ; 
   
   ovhead = 2 +  /* Initial jump */
            1 +  /* Timer terminator */
            2 +  /* Trigger terminator */
            2 ;  /* Final macro terminator */  

   free = (PROMSIZE)
        - 9 * ntimers
        - 3 * ntriggers
        - macspace
        - ovhead;

   fprintf(fd, "\nEEPROM Utilization\n");
   fprintf(fd, "Type      Nbr  Size\n");
   fprintf(fd, "----      ---  ----\n");
   fprintf(fd, "Timers    %3d  %4d (%4.1f%%)\n", 
         ntimers, 9 * ntimers, 900. * (float)(ntimers) / 1024. );
   fprintf(fd, "Triggers  %3d  %4d (%4.1f%%)\n",
         ntriggers, 3 * ntriggers, 300. * (float)ntriggers / 1024. );
   fprintf(fd, "Macros    %3d  %4d (%4.1f%%)\n",
         nmacros, macspace, 100. * (float)macspace / 1024. );
   fprintf(fd, "Overhead       %4d (%4.1f%%)\n",
         ovhead, 100. * (float)ovhead / 1024.);
   fprintf(fd, "Freespace      %4d (%4.1f%%)\n",
         free, 100. * (float)free / 1024.);
   fprintf(fd, "               ----\n");
   fprintf(fd, "Total          %4d\n\n", PROMSIZE);

   return ;

}

/*---------------------------------------------------------------------+
 | Display timer options                                               |
 +---------------------------------------------------------------------*/
char *display_timer_options ( TIMER *timerp, int index )
{
   static char   buffer[64];
   char          minibuf[32];
   unsigned char ddopts; 

   *buffer = '\0';
   if ( !(ddopts = timerp[index].ddoptions) )
      return buffer;
   if ( ddopts & DAWNGT ) {
      sprintf(minibuf, "  DawnGT %02d:%02d",
         timerp[index].dawngt / 60, timerp[index].dawngt % 60);
      strncat(buffer, minibuf, sizeof(buffer) - 1 - strlen(buffer));
   }
   if ( ddopts & DAWNLT ) {
      sprintf(minibuf, "  DawnLT %02d:%02d",
         timerp[index].dawnlt / 60, timerp[index].dawnlt % 60);
      strncat(buffer, minibuf, sizeof(buffer) - 1 - strlen(buffer));
   }
   if ( ddopts & DUSKGT ) {
      sprintf(minibuf, "  DuskGT %02d:%02d",
         timerp[index].duskgt / 60, timerp[index].duskgt % 60);
      strncat(buffer, minibuf, sizeof(buffer) - 1 - strlen(buffer));
   }
   if ( ddopts & DUSKLT ) {
      sprintf(minibuf, "  DuskLT %02d:%02d",
         timerp[index].dusklt / 60, timerp[index].dusklt % 60);
      strncat(buffer, minibuf, sizeof(buffer) - 1 - strlen(buffer));
   }

   return buffer;
}


/*---------------------------------------------------------------------+
 | Display timed events included in timers.                            |
 | Null events are ignored.                                            |
 +---------------------------------------------------------------------*/
int display_events ( FILE *fd, TIMER *timerp, TEVENT *teventp, 
                                 MACRO *macrop, CALEND *calendp )
{
   extern int  current_timer_generation;

   struct ev_s  event[1024];
   int          (*fptr)() = &comp_events;

   int          prtlist[1024]; 
   int          j, k, m, startchain, j1, j2, nevents, count, retcode;
   int          line, pos, ncomb, macro, error, sflag;
   int          dayzero, /*year, month, day, wday,*/ sched_end;
   int          beg, end, endprev/*, year2, month2, day2*/;
   int          createday, endday, lostday;
   unsigned int flag, ddflag;
   long         jan1day;
   char         shift, secur = ' ';
   int          legal, secnom, tadj, offset = 0;
   char         *sec_flag = " sss";
   char         note[6];
   char         minibuf[32];
   unsigned char ddoptions;
   char         datestr1[16], datestr2[16];

   if ( timerp == NULL )
      return 0;

   jan1day = calendp->jan1day;
   dayzero = calendp->day_zero;
   createday = calendp->create_day;
   endday  = calendp->yday + configp->program_days - 1;

   startchain = find_startchain( teventp );
    
   ddflag = NO_EVENT;  
   nevents = 0;
   j = 0;
   while ( timerp[j].line_no > 0 ) {
      if ( timerp[j].generation != current_timer_generation ) {
          j++;
          continue;
      }
      if ( timerp[j].macro_start != NULL_MACRO_INDEX ) {
         event[nevents].tevent = timerp[j].tevent_start;
         event[nevents].timer = j;
         event[nevents].line = timerp[j].line1;
         event[nevents].pos = timerp[j].pos1;
         event[nevents].tpos = 1;
         event[nevents].beg = timerp[j].resolv_beg;
         event[nevents].flag = timerp[j].flag_start & SEC_EVENT;
         ddflag |= timerp[j].flag_start & TIME_EVENTS;
         nevents++;
      }
      if ( timerp[j].macro_stop != NULL_MACRO_INDEX ) {
         event[nevents].tevent = timerp[j].tevent_stop;
         event[nevents].timer = j;
         event[nevents].line = timerp[j].line2;
         event[nevents].pos = timerp[j].pos2;
         event[nevents].tpos = 2;
         event[nevents].beg = timerp[j].resolv_beg;
         event[nevents].flag = timerp[j].flag_stop & SEC_EVENT;
         ddflag |= timerp[j].flag_stop & TIME_EVENTS;
         nevents++;
      }
      j++;
   }

   (void)fprintf( fd, "TIMED EVENTS as expanded and included in uploaded Timers\n"); 
   if ( nevents == 0 ) {
      (void)fprintf( fd, "-- None --\n\n" );
      retcode = 0;
   }
   else 
      retcode = 1;

   /* Sort the events */
   qsort((void *)event, nevents, sizeof(struct ev_s), fptr);


   if ( nevents > 0 ) {
      if ( ddflag & (DAWN_EVENT | DUSK_EVENT) ) {
         (void)fprintf( fd,
             "Line  Week-  Interval                        Sched/   Macro/ Dawn/Dusk\n");
         (void)fprintf( fd,
             "-Pos   days  Beg End  Date begin Date end    Expand   A.E.T.   Error\n");
         (void)fprintf( fd,
             "---- ------- --- ---  ---------- ----------  -----    -----  --------\n");
      }
      else {
         (void)fprintf( fd,
             "Line  Week-  Interval                        Sched/   Macro/\n");
         (void)fprintf( fd,
             "-Pos   days  Beg End  Date begin Date end    Expand   A.E.T.\n");
         (void)fprintf( fd,
             "---- ------- --- ---  ---------- ----------  -----    ------\n");
      }
   }


   /* Isolate a group of events to print */
   line = event[0].line;
   pos = event[0].pos;
   sflag = event[0].flag;
   j1 = j2 = 0;

   while ( j1 < nevents ) {
      while ( j2 < nevents && 
              event[j2].line == line  && 
              event[j2].pos  == pos   && 
              event[j2].flag == sflag     ) { 
         j2++;
      }

      /* Find the original tevent(s)/timer(s) which gave birth */
      /* to this event and display them.                       */

      ncomb = 0;
      j = event[j1].tevent;
      count = teventp[j].print;
      while ( count > 0 && j >= 0 ) {
         if ( teventp[j].generation == 0 ) {
            teventp[j].flag |= ACTIVE_EVENT;
         }
         if ( teventp[j].flag & PRT_EVENT ) {
            prtlist[ncomb++] = j;
            count--;
         }
         j = teventp[j].plink;
      }

      ddoptions = 0;
      for ( k = ncomb - 1; k >= 0; k-- ) {
         m = prtlist[k];
         j = teventp[m].timer;

         macro = teventp[m].macro;
         flag = teventp[m].flag;
         offset = teventp[m].offset;
         pos = teventp[m].pos;
         secur  = sec_flag[flag & SEC_EVENT ? SECURITY_ON & 0x03 : 0];
         line = timerp[j].line_no;
	 ddoptions |= timerp[j].ddoptions;

         if ( /* teventp[m].*/ flag & COMB_EVENT ) 
            (void)fprintf( fd, " **  ");
         else
            (void)fprintf(fd, "%02d-%d ", line, pos);

         if ( timerp[j].notify >= 0 ) {
            (void)fprintf( fd, "%s          expire-%-3d      ",        
               bmap2dow(timerp[j].dow_bmap), timerp[j].notify);
         }
         else {
            sched_end = timerp[j].sched_end;
            if ( sched_end > 1231 ) {
               /* Remove the kluge for reversed date range */
               sched_end -= 1200;
            }

            fprintf(fd, "%s          ", bmap2dow(timerp[j].dow_bmap));
            switch ( configp->date_format ) {
               case MDY_ORDER :
                   fprintf(fd, "%02d%c%02d      %02d%c%02d       ",
                      timerp[j].sched_beg / 100, configp->date_separator, timerp[j].sched_beg % 100,
                      sched_end / 100, configp->date_separator, sched_end % 100);
                   break;
               case DMY_ORDER :
                   fprintf(fd, "%02d%c%02d      %02d%c%02d       ",
                      timerp[j].sched_beg % 100, configp->date_separator, timerp[j].sched_beg / 100,
                      sched_end % 100, configp->date_separator, sched_end / 100);
                   break;
               case YMD_ORDER :
                   fprintf(fd, "     %02d%c%02d      %02d%c%02d  ",
                      timerp[j].sched_beg / 100, configp->date_separator, timerp[j].sched_beg % 100,
                      sched_end / 100, configp->date_separator, sched_end % 100);
                   break;
               default :
                   break;
            }
               
#if 0
            (void)fprintf( fd, "%s          %02d/%02d      %02d/%02d",        
                  bmap2dow(timerp[j].dow_bmap),
                  timerp[j].sched_beg / 100, timerp[j].sched_beg % 100,
                  sched_end / 100, sched_end % 100);
#endif
         }

         if ( flag & CLOCK_EVENT ) {
            (void)fprintf( fd, "%02d:%02d%c   %s%s\n",
               offset / 60, offset % 60, secur, macrop[macro].label,
               display_timer_options(timerp, j));
         }
         else if ( flag & DAWN_EVENT ) {
            (void)sprintf(minibuf, "dawn%+d%c", offset, secur);
            (void)fprintf( fd, "%-15s %s%s\n", minibuf, macrop[macro].label,
               display_timer_options(timerp, j));
         }
         else if ( flag & DUSK_EVENT ) {
            (void)sprintf(minibuf, "dusk%+d%c", offset, secur);
            (void)fprintf( fd, "%-15s %s%s\n", minibuf, macrop[macro].label,
               display_timer_options(timerp, j));
         }
         else {
            (void)fprintf( fd, "\n");
         }
         m = teventp[m].link;
      }
         
      /* Now display events as expanded by heyu. */

      endprev = 0;
      for ( k = j1; k < j2; k++ ) {
         j = event[k].timer;
         if ( event[k].tpos == 1 ) {
            offset = timerp[j].offset_start;
            flag   = timerp[j].flag_start;
            secur  = sec_flag[flag & SEC_EVENT ? SECURITY_ON & 0x03 : 0];
            secnom = flag & SEC_EVENT ? SECURITY_OFFSET_ADJUST : 0; 
            macro  = timerp[j].macro_start;
            error  = timerp[j].error_start;
         }
         else {
            offset = timerp[j].offset_stop;
            flag   = timerp[j].flag_stop;
            secur  = sec_flag[flag & SEC_EVENT ? SECURITY_ON & 0x03 : 0];
            secnom = flag & SEC_EVENT ? SECURITY_OFFSET_ADJUST : 0; 
            macro  = timerp[j].macro_stop;
            error  = timerp[j].error_stop;
         }
         if ( flag & SEC_EVENT )
            (void)strncpy2(note, "s", sizeof(note) - 1);
         else
            note[0] = '\0';

         if ( flag & SUBST_EVENT )
            (void)strncpy2(note, " r", sizeof(note) - 1);
        
         /* Display blank line if there is a gap in dates */
	 /* as the result of timer options.               */
         if ( k > j1 && ddoptions && timerp[j].resolv_beg != (endprev + 1))
            (void)fprintf( fd, "\n");
         endprev = timerp[j].resolv_end;

         (void)fprintf( fd, "     %s %03d-%03d",
            bmap2dow(timerp[j].dow_bmap),
            timerp[j].resolv_beg, timerp[j].resolv_end);

 
         /* Determine the actual event execution legal time */
         legal = offset + macrop[macro].delay + secnom;

         tadj  = time_adjust(timerp[j].resolv_beg + dayzero, legal, STD2LGL);
         if ( time_adjust(timerp[j].resolv_end + dayzero, legal, STD2LGL) != tadj )
            (void)strncat(note, "*", sizeof(note) - 1 - strlen(note));
         legal += tadj;
         if ( legal < 0 ) {
            legal += 1440;
            shift = '<';
         }
         else if ( legal > 1439 ) {
            legal -= 1440;
            shift = '>';
         }
         else {
            shift = ' ';
         }

         fprintf(fd, "  %s", yday2datestr(jan1day, timerp[j].resolv_beg + dayzero));
         fprintf(fd, " %s",  yday2datestr(jan1day, timerp[j].resolv_end + dayzero));

#if 0
         yday2date(   (void)sprintf(buffer, "%s %s %02d %4d %02d:%02d:%02d %s",
     wday_name[tms->tm_wday], month_name[tms->tm_mon], tms->tm_mday, tms->tm_year + 1900,
        tms->tm_hour, tms->tm_min, tms->tm_sec, heyu_tzname[tms->tm_isdst]);
jan1day, timerp[j].resolv_beg + dayzero,
                                          &year, &month, &day, &wday);
         (void)fprintf( fd, "  %02d/%02d/%04d-", month, day, year);
         yday2date(jan1day, timerp[j].resolv_end + dayzero,
                                           &year, &month, &day, &wday);
         (void)fprintf( fd, "%02d/%02d/%04d", month, day, year);
#endif




         if ( flag & (DAWN_EVENT | DUSK_EVENT) )
            (void)fprintf( fd, 
              "  %02d:%02d%c  %c%02d:%02d%-4s[%2d]\n", 
                offset / 60, offset % 60, secur, shift, legal / 60, legal % 60, note, error);
         else
            (void)fprintf( fd, 
              "  %02d:%02d%c  %c%02d:%02d%-4s\n", 
                offset / 60, offset % 60, secur, shift, legal / 60, legal % 60, note);
      }

      (void)fprintf( fd, "\n");
      j1 = j2 ;
      line = event[j1].line;
      pos  = event[j1].pos;
      sflag = event[j1].flag;
   }

   /* Display LOST events */
   (void)fprintf( fd, "TIMED EVENTS day-shifted out of this period.\n");

   j = startchain; nevents = 0;
   while ( j >= 0 ) {
      if ( teventp[j].flag & LOST_EVENT )
         prtlist[nevents++] = j;
      j = teventp[j].link;
   }
 
   if ( nevents == 0 ) {
      (void)fprintf( fd, "-- None --\n\n");
   }
   else {
      (void)fprintf( fd,
          "Line  Wdays  Beg End  Date begin Date end    Time     Macro\n");
      (void)fprintf( fd,
          "---- ------- --- ---  ---------- ----------  -----    -----\n");

      for ( k = 0; k < nevents; k++ ) {
         j = prtlist[k];
         lostday = teventp[j].lostday; 
//         yday2date(jan1day, lostday + dayzero , &year, &month, &day, &wday);
         strcpy(datestr1, yday2datestr(jan1day, lostday + dayzero));

         if ( teventp[j].flag & COMB_EVENT ) 
            (void)fprintf( fd, " **  ");
         else
            (void)fprintf( fd, "%02d-%d ", teventp[j].line_no, teventp[j].pos);

#if 0
         (void)fprintf( fd, "%s %03d-%03d  %02d/%02d/%4d-%02d/%02d/%4d  %02d:%02d%c   %s\n",
            bmap2dow(teventp[j].dow_bmap), lostday, lostday, 
            month, day, year, month, day, year,
            teventp[j].offset / 60, teventp[j].offset % 60,   
            sec_flag[teventp[j].flag & SEC_EVENT ? SECURITY_ON & 0x03 : 0],
            macrop[teventp[j].macro].label);
#endif
         (void)fprintf( fd, "%s %03d-%03d  %s %s  %02d:%02d%c   %s\n",
            bmap2dow(teventp[j].dow_bmap), lostday, lostday, 
            datestr1, datestr1,
            teventp[j].offset / 60, teventp[j].offset % 60,   
            sec_flag[teventp[j].flag & SEC_EVENT ? SECURITY_ON & 0x03 : 0],
            macrop[teventp[j].macro].label);


         
      }
      (void)fprintf( fd, "\n\n");
   }


   /* Display CANCELLED events */
   (void)fprintf( fd, "TIMED EVENTS skipped per Timer options.\n");

   j = startchain; nevents = 0;
   while ( j >= 0 ) {
      if ( teventp[j].flag & CANCEL_EVENT )
         prtlist[nevents++] = j;
      j = teventp[j].link;
   }
 
   if ( nevents == 0 ) {
      (void)fprintf( fd, "-- None --\n\n");
   }
   else {
      (void)fprintf( fd,
          "Line  Wdays  Beg End  Date begin Date end    StdTime  Macro\n");
      (void)fprintf( fd,
          "---- ------- --- ---  ---------- ----------  -------  -----\n");

      for ( k = 0; k < nevents; k++ ) {
         j = prtlist[k];
         beg    = teventp[j].resolv_beg;
         end    = teventp[j].resolv_end;
         offset = teventp[j].offset;
         macro  = teventp[j].macro;
         flag   = teventp[j].flag;
         secur = sec_flag[flag & SEC_EVENT ? SECURITY_ON & 0x03 : 0]; 
  
//         yday2date(jan1day, beg + dayzero, &year, &month, &day, &wday);
//         yday2date(jan1day, end + dayzero, &year2, &month2, &day2, &wday);

         strcpy(datestr1, yday2datestr(jan1day, beg + dayzero));
         strcpy(datestr2, yday2datestr(jan1day, end + dayzero)); 

         if ( teventp[j].flag & COMB_EVENT ) 
            (void)fprintf( fd, " **  ");
         else
            (void)fprintf( fd, "%02d-%d ", teventp[j].line_no, teventp[j].pos);

         (void)fprintf( fd, "%s %03d-%03d  %s %s",
            bmap2dow(teventp[j].dow_bmap), beg, end, 
//            month, day, year, month2, day2, year2);
            datestr1, datestr2);

	 
         if ( flag & CLOCK_EVENT ) {
            (void)sprintf(minibuf, "  %02d:%02d%c",
               offset / 60, offset % 60, secur);
            (void)fprintf( fd, "%-10s %s\n", minibuf, macrop[macro].label);
         }
         else if ( flag & DAWN_EVENT ) {
            (void)sprintf(minibuf, "  dawn%+d%c", offset, secur);
            (void)fprintf( fd, "%-10s %s\n", minibuf, macrop[macro].label);
         }
         else if ( flag & DUSK_EVENT ) {
            (void)sprintf(minibuf, "  dusk%+d%c", offset, secur);
            (void)fprintf( fd, "%-10s %s\n", minibuf, macrop[macro].label);
         }
         else {
            (void)fprintf( fd, "\n");
	 }

      }
      (void)fprintf( fd, "\n\n");
   }


   /* Display the tevents not included in this period */
   (void)fprintf( fd, "TIMED EVENTS from schedule not active during this period.\n");

   j = startchain; nevents = 0;
   while ( j >= 0 ) {
      if ( teventp[j].generation == 0 && (teventp[j].flag & ACTIVE_EVENT) == 0 )
         prtlist[nevents++] = j;
      j = teventp[j].link;
   }
 
   if ( nevents == 0 ) {
      (void)fprintf( fd, "-- None --\n\n");
      return retcode;
   }
   else {
      (void)fprintf( fd,
          "Line  Wdays  Beg End  Date begin Date end    Time     Macro\n");
      (void)fprintf( fd,
          "---- ------- --- ---  ---------- ----------  -----    -----\n");
      
      for ( k = 0; k < nevents; k++ ) {
         m = prtlist[k];
         teventp[m].flag &= ~PRT_EVENT;
         j = teventp[m].timer;

         macro = teventp[m].macro;
         if ( teventp[m].pos == 1 ) {
            offset = timerp[j].offset_start;
            flag   = timerp[j].flag_start;
            secur  = sec_flag[flag & SEC_EVENT ? SECURITY_ON & 0x03 : 0];
            line   = timerp[j].line_no;
            pos    = 1;
         }
         else {
            offset = timerp[j].offset_stop;
            flag   = timerp[j].flag_stop;
            secur  = sec_flag[flag & SEC_EVENT ? SECURITY_ON & 0x03 : 0];
            line   = timerp[j].line_no;
            pos    = 2;
         }

         if ( teventp[m].flag & COMB_EVENT ) 
            (void)fprintf( fd, " **  ");
         else
            (void)fprintf(fd, "%02d-%d ", line, pos);

         if ( timerp[j].notify >= 0 ) {
            (void)fprintf( fd, "%s          expire-%-3d      ",        
               bmap2dow(timerp[j].dow_bmap), timerp[j].notify);
         }
         else {
            sched_end = timerp[j].sched_end;
            if ( sched_end > 1231 ) {
               /* Remove the kluge for reversed date range */
               sched_end -= 1200;
            }

#if 0
            (void)fprintf( fd, "%s          %02d/%02d      %02d/%02d",        
                  bmap2dow(timerp[j].dow_bmap),
                  timerp[j].sched_beg / 100, timerp[j].sched_beg % 100,
                  sched_end / 100, sched_end % 100);
#endif
            fprintf(fd, "%s          ", bmap2dow(timerp[j].dow_bmap));
            switch ( configp->date_format ) {
               case MDY_ORDER :
                   fprintf(fd, "%02d%c%02d      %02d%c%02d       ",
                      timerp[j].sched_beg / 100, configp->date_separator, timerp[j].sched_beg % 100,
                      sched_end / 100, configp->date_separator, sched_end % 100);
                   break;
               case DMY_ORDER :
                   fprintf(fd, "%02d%c%02d      %02d%c%02d       ",
                      timerp[j].sched_beg % 100, configp->date_separator, timerp[j].sched_beg / 100,
                      sched_end % 100, configp->date_separator, sched_end / 100);
                   break;
               case YMD_ORDER :
                   fprintf(fd, "     %02d%c%02d      %02d%c%02d  ",
                      timerp[j].sched_beg / 100, configp->date_separator, timerp[j].sched_beg % 100,
                      sched_end / 100, configp->date_separator, sched_end % 100);
                   break;
               default :
                   break;
            }
               
            
         }
         if ( flag & CLOCK_EVENT ) {
            (void)fprintf( fd, "%02d:%02d%c   %s\n",
               offset / 60, offset % 60, secur, macrop[macro].label);
         }
         else if ( flag & DAWN_EVENT ) {
            (void)fprintf( fd,
            "dawn%-+3d%c %s\n", offset, secur, macrop[macro].label);
         }
         else if ( flag & DUSK_EVENT ) {
            (void)fprintf( fd,
            "dusk%-+3d%c %s\n", offset, secur, macrop[macro].label);
         }
         else {
            (void)fprintf( fd, "\n");
         }
      }     
   }  

   return retcode;
}

#if 0
/*---------------------------------------------------------------------+
 | Display timed events included in timers.                            |
 | Null events are ignored.                                            |
 +---------------------------------------------------------------------*/
int display_events_old ( FILE *fd, TIMER *timerp, TEVENT *teventp, 
                                 MACRO *macrop, CALEND *calendp )
{
   extern int  current_timer_generation;

   struct ev_s  event[1024];
   int          (*fptr)() = &comp_events;

   int          prtlist[1024]; 
   int          j, k, m, startchain, j1, j2, nevents, count, retcode;
   int          line, pos, ncomb, macro, error, sflag;
   int          dayzero, year, month, day, wday, sched_end;
   int          beg, end, endprev, year2, month2, day2;
   int          createday, endday, lostday;
   unsigned int flag, ddflag;
   long         jan1day;
   char         shift, secur = ' ';
   int          legal, secnom, tadj, offset = 0;
   char         *sec_flag = " sss";
   char         note[6];
   char         minibuf[32];
   unsigned char ddoptions;

   if ( timerp == NULL )
      return 0;

   jan1day = calendp->jan1day;
   dayzero = calendp->day_zero;
   createday = calendp->create_day;
   endday  = calendp->yday + configp->program_days - 1;

   startchain = find_startchain( teventp );
    
   ddflag = NO_EVENT;  
   nevents = 0;
   j = 0;
   while ( timerp[j].line_no > 0 ) {
      if ( timerp[j].generation != current_timer_generation ) {
          j++;
          continue;
      }
      if ( timerp[j].macro_start != NULL_MACRO_INDEX ) {
         event[nevents].tevent = timerp[j].tevent_start;
         event[nevents].timer = j;
         event[nevents].line = timerp[j].line1;
         event[nevents].pos = timerp[j].pos1;
         event[nevents].tpos = 1;
         event[nevents].beg = timerp[j].resolv_beg;
         event[nevents].flag = timerp[j].flag_start & SEC_EVENT;
         ddflag |= timerp[j].flag_start & TIME_EVENTS;
         nevents++;
      }
      if ( timerp[j].macro_stop != NULL_MACRO_INDEX ) {
         event[nevents].tevent = timerp[j].tevent_stop;
         event[nevents].timer = j;
         event[nevents].line = timerp[j].line2;
         event[nevents].pos = timerp[j].pos2;
         event[nevents].tpos = 2;
         event[nevents].beg = timerp[j].resolv_beg;
         event[nevents].flag = timerp[j].flag_stop & SEC_EVENT;
         ddflag |= timerp[j].flag_stop & TIME_EVENTS;
         nevents++;
      }
      j++;
   }

   (void)fprintf( fd, "TIMED EVENTS as expanded and included in uploaded Timers\n"); 
   if ( nevents == 0 ) {
      (void)fprintf( fd, "-- None --\n\n" );
      retcode = 0;
   }
   else 
      retcode = 1;

   /* Sort the events */
   qsort((void *)event, nevents, sizeof(struct ev_s), fptr);


   if ( nevents > 0 ) {
      if ( ddflag & (DAWN_EVENT | DUSK_EVENT) ) {
         (void)fprintf( fd,
             "Line  Week-  Interval                        Sched/   Macro/ Dawn/Dusk\n");
         (void)fprintf( fd,
             "-Pos   days  Beg End  Date begin Date end    Expand   A.E.T.   Error\n");
         (void)fprintf( fd,
             "---- ------- --- ---  ---------- ----------  -----    -----  --------\n");
      }
      else {
         (void)fprintf( fd,
             "Line  Week-  Interval                        Sched/   Macro/\n");
         (void)fprintf( fd,
             "-Pos   days  Beg End  Date begin Date end    Expand   A.E.T.\n");
         (void)fprintf( fd,
             "---- ------- --- ---  ---------- ----------  -----    ------\n");
      }
   }


   /* Isolate a group of events to print */
   line = event[0].line;
   pos = event[0].pos;
   sflag = event[0].flag;
   j1 = j2 = 0;

   while ( j1 < nevents ) {
      while ( j2 < nevents && 
              event[j2].line == line  && 
              event[j2].pos  == pos   && 
              event[j2].flag == sflag     ) { 
         j2++;
      }

      /* Find the original tevent(s)/timer(s) which gave birth */
      /* to this event and display them.                       */

      ncomb = 0;
      j = event[j1].tevent;
      count = teventp[j].print;
      while ( count > 0 && j >= 0 ) {
         if ( teventp[j].generation == 0 ) {
            teventp[j].flag |= ACTIVE_EVENT;
         }
         if ( teventp[j].flag & PRT_EVENT ) {
            prtlist[ncomb++] = j;
            count--;
         }
         j = teventp[j].plink;
      }

      ddoptions = 0;
      for ( k = ncomb - 1; k >= 0; k-- ) {
         m = prtlist[k];
         j = teventp[m].timer;

         macro = teventp[m].macro;
         flag = teventp[m].flag;
         offset = teventp[m].offset;
         pos = teventp[m].pos;
         secur  = sec_flag[flag & SEC_EVENT ? SECURITY_ON & 0x03 : 0];
         line = timerp[j].line_no;
	 ddoptions |= timerp[j].ddoptions;

         if ( /* teventp[m].*/ flag & COMB_EVENT ) 
            (void)fprintf( fd, " **  ");
         else
            (void)fprintf(fd, "%02d-%d ", line, pos);

         if ( timerp[j].notify >= 0 ) {
            (void)fprintf( fd, "%s          expire-%-3d      ",        
               bmap2dow(timerp[j].dow_bmap), timerp[j].notify);
         }
         else {
            sched_end = timerp[j].sched_end;
            if ( sched_end > 1231 ) {
               /* Remove the kluge for reversed date range */
               sched_end -= 1200;
            }
            (void)fprintf( fd, "%s          %02d/%02d      %02d/%02d",        
                  bmap2dow(timerp[j].dow_bmap),
                  timerp[j].sched_beg / 100, timerp[j].sched_beg % 100,
                  sched_end / 100, sched_end % 100);
         }

         if ( flag & CLOCK_EVENT ) {
            (void)fprintf( fd, "       %02d:%02d%c   %s%s\n",
               offset / 60, offset % 60, secur, macrop[macro].label,
               display_timer_options(timerp, j));
         }
         else if ( flag & DAWN_EVENT ) {
            (void)sprintf(minibuf, "       dawn%+d%c", offset, secur);
            (void)fprintf( fd, "%-15s %s%s\n", minibuf, macrop[macro].label,
               display_timer_options(timerp, j));
         }
         else if ( flag & DUSK_EVENT ) {
            (void)sprintf(minibuf, "       dusk%+d%c", offset, secur);
            (void)fprintf( fd, "%-15s %s%s\n", minibuf, macrop[macro].label,
               display_timer_options(timerp, j));
         }
         else {
            (void)fprintf( fd, "\n");
         }
         m = teventp[m].link;
      }
         
      /* Now display events as expanded by heyu. */

      endprev = 0;
      for ( k = j1; k < j2; k++ ) {
         j = event[k].timer;
         if ( event[k].tpos == 1 ) {
            offset = timerp[j].offset_start;
            flag   = timerp[j].flag_start;
            secur  = sec_flag[flag & SEC_EVENT ? SECURITY_ON & 0x03 : 0];
            secnom = flag & SEC_EVENT ? SECURITY_OFFSET_ADJUST : 0; 
            macro  = timerp[j].macro_start;
            error  = timerp[j].error_start;
         }
         else {
            offset = timerp[j].offset_stop;
            flag   = timerp[j].flag_stop;
            secur  = sec_flag[flag & SEC_EVENT ? SECURITY_ON & 0x03 : 0];
            secnom = flag & SEC_EVENT ? SECURITY_OFFSET_ADJUST : 0; 
            macro  = timerp[j].macro_stop;
            error  = timerp[j].error_stop;
         }
         if ( flag & SEC_EVENT )
            (void)strncpy2(note, "s", sizeof(note) - 1);
         else
            note[0] = '\0';

         if ( flag & SUBST_EVENT )
            (void)strncpy2(note, " r", sizeof(note) - 1);
        
         /* Display blank line if there is a gap in dates */
	 /* as the result of timer options.               */
         if ( k > j1 && ddoptions && timerp[j].resolv_beg != (endprev + 1))
            (void)fprintf( fd, "\n");
         endprev = timerp[j].resolv_end;

         (void)fprintf( fd, "     %s %03d-%03d",
            bmap2dow(timerp[j].dow_bmap),
            timerp[j].resolv_beg, timerp[j].resolv_end);

 
         /* Determine the actual event execution legal time */
         legal = offset + macrop[macro].delay + secnom;

         tadj  = time_adjust(timerp[j].resolv_beg + dayzero, legal, STD2LGL);
         if ( time_adjust(timerp[j].resolv_end + dayzero, legal, STD2LGL) != tadj )
            (void)strncat(note, "*", sizeof(note) - 1 - strlen(note));
         legal += tadj;
         if ( legal < 0 ) {
            legal += 1440;
            shift = '<';
         }
         else if ( legal > 1439 ) {
            legal -= 1440;
            shift = '>';
         }
         else {
            shift = ' ';
         }

         yday2date(jan1day, timerp[j].resolv_beg + dayzero,
                                          &year, &month, &day, &wday);
         (void)fprintf( fd, "  %02d/%02d/%04d-", month, day, year);
         yday2date(jan1day, timerp[j].resolv_end + dayzero,
                                           &year, &month, &day, &wday);
         (void)fprintf( fd, "%02d/%02d/%04d", month, day, year);
         if ( flag & (DAWN_EVENT | DUSK_EVENT) )
            (void)fprintf( fd, 
              "  %02d:%02d%c  %c%02d:%02d%-4s[%2d]\n", 
                offset / 60, offset % 60, secur, shift, legal / 60, legal % 60, note, error);
         else
            (void)fprintf( fd, 
              "  %02d:%02d%c  %c%02d:%02d%-4s\n", 
                offset / 60, offset % 60, secur, shift, legal / 60, legal % 60, note);
      }

      (void)fprintf( fd, "\n");
      j1 = j2 ;
      line = event[j1].line;
      pos  = event[j1].pos;
      sflag = event[j1].flag;
   }

   /* Display LOST events */
   (void)fprintf( fd, "TIMED EVENTS day-shifted out of this period.\n");

   j = startchain; nevents = 0;
   while ( j >= 0 ) {
      if ( teventp[j].flag & LOST_EVENT )
         prtlist[nevents++] = j;
      j = teventp[j].link;
   }
 
   if ( nevents == 0 ) {
      (void)fprintf( fd, "-- None --\n\n");
   }
   else {
      (void)fprintf( fd,
          "Line  Wdays  Beg End  Date begin Date end    Time     Macro\n");
      (void)fprintf( fd,
          "---- ------- --- ---  ---------- ----------  -----    -----\n");

      for ( k = 0; k < nevents; k++ ) {
         j = prtlist[k];
         lostday = teventp[j].lostday; 
         yday2date(jan1day, lostday + dayzero , &year, &month, &day, &wday);

         if ( teventp[j].flag & COMB_EVENT ) 
            (void)fprintf( fd, " **  ");
         else
            (void)fprintf( fd, "%02d-%d ", teventp[j].line_no, teventp[j].pos);

         (void)fprintf( fd, "%s %03d-%03d  %02d/%02d/%4d-%02d/%02d/%4d  %02d:%02d%c   %s\n",
            bmap2dow(teventp[j].dow_bmap), lostday, lostday, 
            month, day, year, month, day, year,
            teventp[j].offset / 60, teventp[j].offset % 60,   
            sec_flag[teventp[j].flag & SEC_EVENT ? SECURITY_ON & 0x03 : 0],
            macrop[teventp[j].macro].label);         
      }
      (void)fprintf( fd, "\n\n");
   }


   /* Display CANCELLED events */
   (void)fprintf( fd, "TIMED EVENTS skipped per Timer options.\n");

   j = startchain; nevents = 0;
   while ( j >= 0 ) {
      if ( teventp[j].flag & CANCEL_EVENT )
         prtlist[nevents++] = j;
      j = teventp[j].link;
   }
 
   if ( nevents == 0 ) {
      (void)fprintf( fd, "-- None --\n\n");
   }
   else {
      (void)fprintf( fd,
          "Line  Wdays  Beg End  Date begin Date end    StdTime  Macro\n");
      (void)fprintf( fd,
          "---- ------- --- ---  ---------- ----------  -------  -----\n");

      for ( k = 0; k < nevents; k++ ) {
         j = prtlist[k];
         beg    = teventp[j].resolv_beg;
         end    = teventp[j].resolv_end;
         offset = teventp[j].offset;
         macro  = teventp[j].macro;
         flag   = teventp[j].flag;
         secur = sec_flag[flag & SEC_EVENT ? SECURITY_ON & 0x03 : 0]; 
  
         yday2date(jan1day, beg + dayzero, &year, &month, &day, &wday);
         yday2date(jan1day, end + dayzero, &year2, &month2, &day2, &wday);

         if ( teventp[j].flag & COMB_EVENT ) 
            (void)fprintf( fd, " **  ");
         else
            (void)fprintf( fd, "%02d-%d ", teventp[j].line_no, teventp[j].pos);

         (void)fprintf( fd, "%s %03d-%03d  %02d/%02d/%4d-%02d/%02d/%4d",
            bmap2dow(teventp[j].dow_bmap), beg, end, 
            month, day, year, month2, day2, year2);

	 
         if ( flag & CLOCK_EVENT ) {
            (void)sprintf(minibuf, "  %02d:%02d%c",
               offset / 60, offset % 60, secur);
            (void)fprintf( fd, "%-10s %s\n", minibuf, macrop[macro].label);
         }
         else if ( flag & DAWN_EVENT ) {
            (void)sprintf(minibuf, "  dawn%+d%c", offset, secur);
            (void)fprintf( fd, "%-10s %s\n", minibuf, macrop[macro].label);
         }
         else if ( flag & DUSK_EVENT ) {
            (void)sprintf(minibuf, "  dusk%+d%c", offset, secur);
            (void)fprintf( fd, "%-10s %s\n", minibuf, macrop[macro].label);
         }
         else {
            (void)fprintf( fd, "\n");
	 }

      }
      (void)fprintf( fd, "\n\n");
   }


   /* Display the tevents not included in this period */
   (void)fprintf( fd, "TIMED EVENTS from schedule not active during this period.\n");

   j = startchain; nevents = 0;
   while ( j >= 0 ) {
      if ( teventp[j].generation == 0 && (teventp[j].flag & ACTIVE_EVENT) == 0 )
         prtlist[nevents++] = j;
      j = teventp[j].link;
   }
 
   if ( nevents == 0 ) {
      (void)fprintf( fd, "-- None --\n\n");
      return retcode;
   }
   else {
      (void)fprintf( fd,
          "Line  Wdays  Beg End  Date begin Date end    Time     Macro\n");
      (void)fprintf( fd,
          "---- ------- --- ---  ---------- ----------  -----    -----\n");
      
      for ( k = 0; k < nevents; k++ ) {
         m = prtlist[k];
         teventp[m].flag &= ~PRT_EVENT;
         j = teventp[m].timer;

         macro = teventp[m].macro;
         if ( teventp[m].pos == 1 ) {
            offset = timerp[j].offset_start;
            flag   = timerp[j].flag_start;
            secur  = sec_flag[flag & SEC_EVENT ? SECURITY_ON & 0x03 : 0];
            line   = timerp[j].line_no;
            pos    = 1;
         }
         else {
            offset = timerp[j].offset_stop;
            flag   = timerp[j].flag_stop;
            secur  = sec_flag[flag & SEC_EVENT ? SECURITY_ON & 0x03 : 0];
            line   = timerp[j].line_no;
            pos    = 2;
         }

         if ( teventp[m].flag & COMB_EVENT ) 
            (void)fprintf( fd, " **  ");
         else
            (void)fprintf(fd, "%02d-%d ", line, pos);

         if ( timerp[j].notify >= 0 ) {
            (void)fprintf( fd, "%s          expire-%-3d      ",        
               bmap2dow(timerp[j].dow_bmap), timerp[j].notify);
         }
         else {
            sched_end = timerp[j].sched_end;
            if ( sched_end > 1231 ) {
               /* Remove the kluge for reversed date range */
               sched_end -= 1200;
            }
            (void)fprintf( fd, "%s          %02d/%02d      %02d/%02d",        
                  bmap2dow(timerp[j].dow_bmap),
                  timerp[j].sched_beg / 100, timerp[j].sched_beg % 100,
                  sched_end / 100, sched_end % 100);
         }
         if ( flag & CLOCK_EVENT ) {
            (void)fprintf( fd, "       %02d:%02d%c   %s\n",
               offset / 60, offset % 60, secur, macrop[macro].label);
         }
         else if ( flag & DAWN_EVENT ) {
            (void)fprintf( fd,
            "       dawn%-+3d%c %s\n", offset, secur, macrop[macro].label);
         }
         else if ( flag & DUSK_EVENT ) {
            (void)fprintf( fd,
            "       dusk%-+3d%c %s\n", offset, secur, macrop[macro].label);
         }
         else {
            (void)fprintf( fd, "\n");
         }
      }     
   }  

   return retcode;
}
#endif

/*---------------------------------------------------------------------+
 | Display timers for final_report()                                   |
 | Also check for stop time = start time error and return 1 if so,     |
 | otherwise return 0 if OK.                                           |
 +---------------------------------------------------------------------*/
int display_timers( FILE *fd, TIMER *timerp, TEVENT *teventp, MACRO *macrop )
{
   extern int current_timer_generation;
   int        j, maxlabel, status;
   char       *sec_flag = " sss";

   if ( !timerp ) {
      (void)fprintf( fd, "\n");
      return 0;
   }

   /* Get maximum length of "Start" macro labels used in the timers */
   maxlabel = 0;
   j = 0;
   while ( timerp[j].line_no > 0 ) {
      if ( timerp[j].generation == current_timer_generation ) {
         maxlabel = max(maxlabel, (int)strlen(macrop[timerp[j].macro_start].label));
      }
      j++;
   }

   (void) fprintf( fd,
       "OUTPUT TIMERS - Times in minutes after midnight Standard Time\n-------------\n");
   (void) fprintf( fd,
       "(Codes: C = Clock, R = Dawn, S = Dusk, X = null, s = Security)\n");
   if ( configp->display_offset == YES ) 
      (void) fprintf( fd, "Loc  ");
   (void) fprintf( fd,
      "Line Line   WkDay   Beg End  F Time   F Time   %-*s  Macro\n",
           maxlabel, "Macro");
   if ( configp->display_offset == YES )
      (void) fprintf( fd, "---  ");
   (void) fprintf( fd,
      "---- ----  -------  --- ---  - ----   - ----   %-*s  -----\n",
           maxlabel, "-----");


   j = 0;
   status = 0;
   while ( timerp[j].line_no > 0 ) {
      if ( timerp[j].generation != current_timer_generation ) {
         j++;
         continue;
      }

      /* Display the offset of the timer in the memory image */
      if ( configp->display_offset == YES )
         (void) fprintf( fd, "%03x  ", timerp[j].memloc);

      /* Override line number and position for a combined event because */
      /* it's not valid anyway; otherwise display it.                   */

      if ( teventp[timerp[j].tevent_start].flag & COMB_EVENT )
         (void) fprintf( fd, " **  " );
      else
        (void) fprintf( fd, "%02d-%1d ",timerp[j].line1, timerp[j].pos1);

      if ( timerp[j].flag_stop == NO_EVENT )
         (void) fprintf( fd, "----  " );
      else if ( teventp[timerp[j].tevent_stop].flag & COMB_EVENT )
         (void) fprintf( fd, " **   " );
      else
        (void) fprintf( fd, "%02d-%1d  ",timerp[j].line2, timerp[j].pos2);

      /* Display the rest of the data. */

      (void) fprintf( fd, "%7s  %03d-%03d  %c %04d%c  %c %04d%c  %-*s  %s\n",
         bmap2dow(timerp[j].dow_bmap),
         timerp[j].resolv_beg, timerp[j].resolv_end,
         flag_def(timerp[j].flag_start & TIME_EVENTS),
         timerp[j].offset_start,
         sec_flag[timerp[j].flag_start & SEC_EVENT ? SECURITY_ON & 0x03 : 0],
         flag_def(timerp[j].flag_stop & TIME_EVENTS),
         timerp[j].offset_stop,
         sec_flag[timerp[j].flag_stop & SEC_EVENT ? SECURITY_ON & 0x03 : 0],
         maxlabel,
         macrop[timerp[j].macro_start].label,
         macrop[timerp[j].macro_stop].label);

      if ( timerp[j].offset_stop == timerp[j].offset_start ) {
         fprintf(fd, "***** Internal error: stop time = start time *****\n");
         fprintf(fd, "      CM11A bug! - will not launch macro '%s'\n",
            macrop[timerp[j].macro_stop].label);
         status = 1;
      }

      j++;
   }
   (void)fprintf( fd, "\n");
   return status;
}


/*---------------------------------------------------------------------+
 | Generate a final report of Timers, Triggers, and Macros in use.     |
 +---------------------------------------------------------------------*/
int final_report ( char *pathname, CALEND *calendp, TIMER *timerp,
   TEVENT *teventp, TRIGGER *triggerp, MACRO *macrop,
   unsigned char *elementp, int proc_code)
{
   FILE          *fd;
   int           j;
   int           original, current;
   int           status;

   static char   *off_on[] = {"off", "on "};

   original = 0;
   current  = update_current_timer_generation();

   if ( !(fd = fopen(pathname, "w")) ) {
      (void) fprintf(stderr, "Unable to open report file '%s' for writing.\n",
                 pathname);
      return -1;
   }

   (void)fprintf( fd, "Report of expanded Timers, Triggers, and Macros\n");
   (void)fprintf( fd, "===============================================\n");

   /* Display Config & Schedule files and the Date and Time created */
   (void)fprintf( fd, "Version: %s\n", VERSION);
   (void)fprintf( fd, "Config File:   %s\n", pathspec(heyu_config));
   (void)fprintf( fd, "Schedule File: %s\n", schedfile);
   (void)fprintf( fd, "Report Date: %s\n\n", legal_time_string());

   (void)fprintf( fd, "Upload schedule to the interface: %s\n\n",
      (proc_code == PROC_UPLOAD) ? "YES" : "NO" );

   display_config_overrides( fd );
   
   if ( calendp->asif_flag & (ASIF_DATE | ASIF_TIME) )
      (void)fprintf( fd, "Simulation as if %s\n", asif_time_string());

   if ( configp->mode == HEYU_MODE )
     (void)fprintf( fd, "Configured for HEYU mode; Programmed for %d Days.\n\n",
          configp->program_days);
   else
     (void)fprintf( fd, "Configured for COMPATIBLE mode.\n\n");

   (void)fprintf( fd, "Dawn/Dusk defined as ");
   (void)fprintf( fd, 
       ((configp->sunmode == RiseSet) ? "Sunrise/Sunset\n\n" :
	(configp->sunmode == CivilTwi) ? "Civil Twilight\n\n" :
	(configp->sunmode == NautTwi) ? "Nautical Twilight\n\n" :
	(configp->sunmode == AstroTwi) ? "Astronomical Twilight\n\n" :
	(configp->sunmode == AngleOffset) ? "Sun centre at %d angle minutes below horizon\n\n" : "???"),
	 configp->sunmode_offset);

   (void) fprintf( fd, "Actual Event Times (A.E.T.) include macro delay, if any.\n");
   (void) fprintf( fd, "Schedule Times and A.E.T. are Civil Time.\n");

   (void) fprintf( fd, "Expanded times are Standard Time.\n\n");

   (void) fprintf( fd, "Symbols used:\n");
   (void) fprintf( fd, "  **  Combined event.\n");
   (void) fprintf( fd, "  *   Time change during interval.\n");
   (void) fprintf( fd, "  >/< Next/Previous day.\n");
   (void) fprintf( fd, "  r   Heyu-generated random initial security day.\n\n"); 

   /* Display individual timed events included in timers */

   display_events( fd, timerp, teventp, macrop, calendp );

   (void)fprintf( fd, "\n");

   /* Display the uploaded timers */

   status = display_timers( fd, timerp, teventp, macrop );

   (void)fprintf( fd, "\n");

   /* Display triggers */

   (void) fprintf( fd, "TRIGGERS\n--------\n");
   j = 0;
   while ( triggerp && triggerp[j].line_no > 0 ) {
      if ( configp->display_offset == YES )
         (void) fprintf( fd, "%03x ", triggerp[j].memloc);

      (void) fprintf( fd, "trigger  %c%-2d %3s  %s\n",
            code2hc(triggerp[j].housecode),
            code2unit(triggerp[j].unitcode),
            off_on[triggerp[j].command],
            macrop[triggerp[j].macro].label);
      j++;
   }

   if ( triggerp == NULL )
      (void) fprintf( fd, "-- None --\n");

   (void) fprintf( fd, "\n");

   /* Display uploaded macros */

   (void) fprintf( fd, 
     "\nMACROS uploaded  ( * denotes modified delay and/or compression)\n---------------\n");
   (void) display_macros( fd, USED, macrop, elementp );


   /* Display unused macros */

   (void)fprintf(fd,"\nMACROS unused/combined and omitted from upload\n");
   (void)fprintf(fd,"----------------------------------------------\n");

   if ( !display_macros( fd, NOTUSED, macrop, elementp ) ) 
      (void)fprintf( fd, "-- None --\n");

   display_eeprom_usage( fd, timerp, triggerp, macrop );

   fclose( fd );

   return status;
}


/*---------------------------------------------------------------------+
 | Store in global variable heyu_path the directory containing the     |
 | heyu configuration file and check if this directory is writable.    |
 | Store the actual configuration filename in heyu_configp->             |
 | Set flag is_writable if heyu_path is writable.                      |
 +---------------------------------------------------------------------*/
void find_heyu_path( void )
{
   extern int  is_writable;
   extern int  verbose;
   extern int  i_am_relay;

   FILE   *fd;	
   char   *sp, *sp1;
   int    pid;
   char   subdir[PATH_LEN + 1];
   char   tmpfile[PATH_LEN + 1];
   static char envconf[PATH_LEN + 1];
   struct stat statbuf;

   int    ignoret;

   /* Find the configuration file. */

   for (;;) {
      /* Look for command line option or X10CONFIG environment variable */
      if ( (sp = optptr->configp) || (!optptr->subp && (sp = getenv("X10CONFIG"))) ) {
         if ( (sp1 = strrchr(sp, '/')) == NULL ) {
	    (void)strncpy2(heyu_path, "./", sizeof(heyu_path) - 1);
	    (void)strncpy2(heyu_config, sp, sizeof(heyu_config) - 1);
         }	    
         else {
	    sp1++;
	    (void)strncpy2(heyu_path, sp, sp1 - sp);
	    (void)strncpy2(heyu_config, sp1, sizeof(heyu_config) - 1);
         }
	 if ( !(*heyu_config) ) {
	    (void)fprintf(stderr, "No filename specified for '%s'\n", sp);
	    exit(1);
	 }

         /* Verify there is a readable config file */
         if ( verbose && i_am_relay != 1 )
            (void)printf("Searching for '%s'\n", pathspec(heyu_config));
	 if ( stat(pathspec(heyu_config), &statbuf) != 0 || 
	                       !(statbuf.st_mode & S_IFREG) ) {
	    (void)fprintf(stderr, 
		"Bad configuration file '%s' specified.\n", 
		        pathspec(heyu_config));
	    exit(1);
	 }
		 
         if ( (fd = fopen(pathspec(heyu_config), "r")) ) {
            (void)fclose(fd);
	    if ( verbose && i_am_relay != 1 )
	       (void)printf("Found configuration file '%s'\n",
			       pathspec(heyu_config) );
            break;
         }
         else {
            (void)fprintf(stderr, 
               "Unable to find (or open) Heyu configuration file '%s'\n",
	                       pathspec(heyu_config) );
            exit(1);
         } 
      }
	   
      /* Next check for subdirectory command line option */
      /* (-0 ... -9) or HEYUSUB  environment variable.   */
      if ( (sp = optptr->subp) ) {
         sprintf(subdir, "%s%s/", HEYUSUB_PREFIX, sp);
      }
      else if ( (sp = getenv("HEYUSUB")) ) {
         /* Remove leading / if any */
         if ( *sp == '/' )
            (void)strncpy2(subdir, sp + 1, sizeof(subdir) - 1);
         else
            (void)strncpy2(subdir, sp, sizeof(subdir) - 1);
        
         if ( *(sp = subdir + (strlen(subdir) - 1)) != '/' )
            (void)strncat(subdir, "/", sizeof(subdir) - 1 - strlen(subdir));
      }
      else { 
         *subdir = '\0';
      }

      /* Look in the user's $HOME/HOMEBASEDIR/ directory */
      if ( (sp = getenv("HOME")) ) {
         (void)strncpy2(heyu_path, sp, sizeof(heyu_path) - 1);
         if ( *(sp = heyu_path + strlen(heyu_path) - 1) != '/')
            (void)strncat(heyu_path, "/", sizeof(heyu_path) - 1 - strlen(heyu_path));
         (void)strncat(heyu_path, HOMEBASEDIR, sizeof(heyu_path) - 1 - strlen(heyu_path)); 
         (void)strncat(heyu_path, subdir, sizeof(heyu_path) - 1 - strlen(heyu_path));

         /* Verify there is a readable config file */
         if ( verbose && i_am_relay != 1 )
            (void)printf("Searching for '%s'\n", pathspec(CONFIG_FILE));

         if ( (fd = fopen(pathspec(CONFIG_FILE), "r")) ) {
            (void)fclose(fd);
            (void)strncpy2(heyu_config, CONFIG_FILE, sizeof(heyu_config) - 1);
            break;
         }
      }

      /* Next look in SYSBASEDIR directory (or subdirectory thereof) */        
      (void)sprintf(heyu_path, "%s/%s", SYSBASEDIR, subdir);
      
      /* Verify there is a readable config file */
      if ( verbose && i_am_relay != 1 )
         (void)printf("Searching for '%s'\n", pathspec(CONFIG_FILE_ETC));
      if ( (fd = fopen(pathspec(CONFIG_FILE_ETC), "r")) ) {
         (void)fclose(fd);
         (void)strncpy2(heyu_config, CONFIG_FILE_ETC, sizeof(heyu_config) - 1);
         break;
      }
      else if ( !i_am_relay ) {
         (void)fprintf(stderr, "Unable to find Heyu configuration file.\n"),
         exit(1);
      } 
   }

   if ( verbose && i_am_relay != 1 )
      (void)printf("Found configuration file '%s'\n", pathspec(heyu_config));

   snprintf(envconf, sizeof(envconf) - 1, "X10CONFIG=%s", pathspec(heyu_config));
   putenv(envconf);
   

   /* Verify the heyu_path directory is readable and writable by  */
   /* creating and writing to a temporary file, then reading back */
   /* what we wrote.                                              */

   sprintf(tmpfile, "heyu.%d.tmp", (int)getpid());
   if ( (fd = fopen(pathspec(tmpfile), "w")) ) {
      fprintf(fd, "%d\n", (int)getpid());
      fclose(fd);
   }
   else {
      is_writable = 0;
      (void)fprintf(stderr, "Unable to write in Heyu directory '%s'\n", heyu_path);
      return;
   }
   if ( (fd = fopen(pathspec(tmpfile), "r")) ) {
      ignoret = fscanf(fd, "%d\n", &pid);
      fclose(fd);
      if ( pid == getpid() ) {
         remove(pathspec(tmpfile));
         is_writable = 1;
         if ( verbose && i_am_relay != 1 ) 
            (void)printf("Heyu directory %s is writable.\n", heyu_path);

         return;
      }
   }
   else {
      fprintf(stderr, "Read error while testing Heyu directory writeability\n");
      remove(pathspec(tmpfile));
      is_writable = 0;
   }

   return;
}

/*----------------------------------------------------------------------------+
 | Compress the elements in a macro by merging the unit bmaps for commands    |
 | which are otherwise similar for the same housecodes and removing           |
 | duplicate commands, e.g.:                                                  | 
 |  A1 ON; B1 OFF; A2 ON --> A1,2 ON; B1 OFF                                  |
 |  A1 ON; A1 OFF; A1 ON --> A1 ON; A1 OFF                                    |
 |                                                                            |
 | Element ordering is otherwise preserved to the extent possible.            |
 |                                                                            |
 | Commands like DIM are merged only if they have the same "data", e.g. the   |
 | same dim values.                                                           | 
 +----------------------------------------------------------------------------*/
int compress_elements ( unsigned char *elemlist, int *nelem, int *total ) 
{
   int      j, k, m, matched;
   int      indxj, lenj, unitj, cmdlen, outelem, ntot;
   int      *index, *length, *done;

   unsigned char hcfunj, cmdcode;
   unsigned char *outbuff;
   static int sizint = sizeof(int);
   static int sizuchr = sizeof(unsigned char);

   index   = calloc( *total, sizint );
   length  = calloc( *total, sizint );
   done    = calloc( *total, sizint );
   outbuff = calloc( *total, sizuchr );

   if ( index == NULL || length == NULL || done == NULL || outbuff == NULL ) {
      fprintf(stderr, "compress_elements() - Unable to allocate memory.\n");
      exit(1);
   }

   j = 0; k = 0;
   while ( j < *total ) {
      index[k] = j;
      done[k] = 0;
      cmdcode = elemlist[j] & 0x0fu;

      /* Find the length of the macro element in the master table */
      if ( (cmdlen = macro_element_length(cmdcode)) <= 0 ) {
         (void)fprintf(stderr, 
            "compress_elements(): Internal table error.\n");
         return 0;
      }

      j += length[k] = cmdlen;
      k++;
   }
   if ( k != *nelem )  {
      (void)fprintf(stderr, 
         "compress_elements(): Mismatch in number of elements.\n");
      return 0;
   }

   ntot = 0; outelem = 0;
   for ( j = 0; j < *nelem; j++ ) {
      if ( done[j] )
         continue;

      done[j] = 1;
      indxj = index[j];
      hcfunj = elemlist[indxj];
      lenj = length[j];
      unitj = ntot + 1;
      for ( k = indxj; k < indxj + lenj; k++ ) 
         outbuff[ntot++] = elemlist[k];
      outelem++;

      for ( k = j + 1; k < *nelem; k++ ) {
         if ( done[k] )
            continue;
         if ( hcfunj == elemlist[index[k]] && lenj == length[k] ) {
            /* Compare the element data bytes */
            matched = 1;
            for ( m = 3; m < lenj; m++ ) {
               if ( elemlist[indxj + m] != elemlist[index[k] + m] ) {
                  matched = 0;
                  break;
               }
            }
            if ( matched ) {
               /* Combine the unit bmap */
               outbuff[unitj] |= elemlist[index[k] + 1];
               outbuff[unitj + 1] |= elemlist[index[k] + 2];
               done[k] = 1;
            }
         }
      }
   }

   for ( j = 0; j < ntot; j++ ) 
      elemlist[j] = outbuff[j];

   *nelem = outelem;
   *total = ntot;

   free(index);
   free(length);
   free(done);
   free(outbuff);

   return 1;
}

/*----------------------------------------------------------------------------+
 | Compress the elements in all macros.                                       |
 +----------------------------------------------------------------------------*/
void compress_macros ( MACRO *macrop, unsigned char *elementp ) 
{
   int      j, oldtotal;
   unsigned char *ptr;

   j = 1;
   while ( macrop[j].line_no > 0 ) {
      oldtotal = macrop[j].total;
      ptr = &elementp[macrop[j].element];
      if ( !compress_elements(ptr, &macrop[j].nelem, &macrop[j].total) )
         exit(1);
      if ( macrop[j].total < oldtotal )
         macrop[j].modflag |= COMPRESSED;
      j++;
   }
 
   return;
}

/*---------------------------------------------------------------------+
 | Read a 1024 byte file containing a CM11a memory image and upload    |
 | it to the CM11a interface.                                          |
 +---------------------------------------------------------------------*/
int upload_image_from_file ( char *pathspec )
{
   FILE          *fd;
   int           size;
   unsigned char prommap[2 * PROMSIZE];
   extern void   upload_eeprom_image(unsigned char *);

   if ( !(fd = fopen(pathspec, "r")) ) {
      (void)fprintf(stderr, "Unable to read file '%s'\n", pathspec);
      exit(1);
   }

   /* Try reading more than 1024 bytes to verify the size is correct */
   size = fread( (void *)prommap, 1, 2 * PROMSIZE, fd);

   if ( size != PROMSIZE ) {
      (void)fprintf(stderr, "File size is not %d bytes.\n", PROMSIZE);
      exit(1);
   }

   /* Upload the image to the interface */

   upload_eeprom_image( prommap );

   return 0;
}


/*---------------------------------------------------------------------+
 | Handle Heyu 'upload' command and arguments.                         |
 +---------------------------------------------------------------------*/
int c_upload ( int argc, char *argv[] )
{
   extern  int  is_writable;
   
   int retcode;
 
   if ( invalidate_for_cm10a() != 0 )
      return 1;
   
   if ( argc == 2 ) {
      if ( !is_writable ) {
         (void)fprintf(stderr, 
            "Heyu directory %s must be writable - quitting.\n", heyu_path);
         return 1;
      }         
      retcode = process_data(PROC_UPLOAD);

      return retcode;
   }

   if ( argc == 3 ) {
      if ( strcmp("status", argv[2]) == 0 ) { 
         display_cm11a_status(1);
	 retcode = 0;
      }
      else if ( strcmp("cronstatus", argv[2]) == 0 ) { 
         display_cm11a_status(0);
	 retcode = 0;
      }
      else if ( !is_writable ) {
         (void)fprintf(stderr,
             "Heyu directory %s must be writable - quitting.\n", heyu_path);
         retcode = 1;
      }         
      else if ( strcmp("check", strlower(argv[2])) == 0 ) {
         retcode = process_data(PROC_CHECK);
      }
      else if ( strcmp("croncheck", argv[2]) == 0 ) {
         retcode = crontest();
      }
      else {
         (void)fprintf(stderr, "Usage: %s %s [check|croncheck|status|cronstatus]\n",
            argv[0], argv[1]);
         retcode = 1;
      }
   }
   else if ( argc == 4 && strcmp("imagefile", strlower(argv[2])) == 0 ) {
      /* Undocumented - for experimental use only */
      retcode = upload_image_from_file(argv[3]);
   }
   else {
      (void)fprintf(stderr, "Usage: %s %s [check|croncheck|status|cronstatus]\n",
         argv[0], argv[1]);
      retcode = 1;
   }

   return retcode;
}

/*---------------------------------------------------------------------+
 | Usage messages for 'heyu utility'                                   |
 +---------------------------------------------------------------------*/
void display_utility_usage( int argc, char *argv[] )
{
   int  j = 0;

   char *optmsg[] = {
      "syscheck     Display your system calendar/clock configuration.\n",
      "dawndusk     Display Dawn/Dusk today, per Dawn/Dusk definition",
      "             in configuration file.\n",
      "suntable [-r|c|n|a|s|w] [yyyy]",
      "             Write a file of daily dawn/dusk times (by default",
      "             per Dawn/Dusk definition in configuration file)",
      "             for the current year or year yyyy.",
      "             Override options:",
      "                -r  Display Sunrise and Sunset",
      "                -c  Display Civil Twilights",
      "                -n  Display Nautical Twilights",
      "                -a  Display Astronomical Twilights",
      "             Other options:",
      "                -s  Display Standard Time instead of Civil Time",
      "                -w  Wide format.  (Printing on US letter or A4 size paper",
      "                    requires Landscape mode and 8-pt fixed font.)\n",
      "calibrate    Calibration for fast timing loops.\n",
   };
 
   (void)fprintf(stdout, "%s %s  options:\n", argv[0], argv[1]);

   for ( j = 0; j < (sizeof(optmsg)/sizeof(char *)); j++ )
     (void)fprintf(stdout, "   %s\n", optmsg[j]);

   return;
}
  
/*---------------------------------------------------------------------+
 | Set the various status bits described in Sec 8 of protocol.txt      |
 +---------------------------------------------------------------------*/
int c_set_status ( int argc, char *argv[] )
{
   extern int c_set_status_bits( unsigned char );

   if ( strcmp("newbattery", strlower(argv[1])) == 0 ) {
      return c_set_status_bits( RESET_BATTERY_TIMER );
   }	 
   else if ( strcmp("purge", strlower(argv[1])) == 0 ) {
      return c_set_status_bits( PURGE_DELAYED_MACROS );
   }
   else if ( strcmp("clear", strlower(argv[1])) == 0 ) {
      return c_set_status_bits( MONITORED_STATUS_CLEAR );
   }
   else if ( strcmp("reserved", strlower(argv[1])) == 0 ) {
      /* Undocumented */
      /* This one to test whether the "reserved" bit actually */
      /* doesn't do anything.                                 */
      return c_set_status_bits( RESERVED_STATUS_BIT );
   }
   return 0;
}	 

/*---------------------------------------------------------------------+
 | Handle Heyu 'utility' command and arguments.                        |
 | (There's no spool file open so commands are restricted.)            |
 +---------------------------------------------------------------------*/
int c_utility ( int argc, char *argv[] )
{
   int           j, sunmode, offset, timemode, format, year;
   time_t        utc0_dawn, utc0_dusk;
   time_t        now;
   struct tm     *tmp;
   char          *sp;

   static char   *sunmodelabel[] = {"Sunrise/Sunset", "Civil Twilight",
        "Nautical Twilight", "Astronomical Twilight", "Sun angle offset = %d'", };

   unsigned long loopcount;
   unsigned long loop_calibrate ( void );
   extern void display_env_masks ( void );

   if ( argc >= 3 ) {
      if ( strcmp("syscheck", strlower(argv[2])) == 0 ) {
         get_configuration(CONFIG_INIT);
         display_sys_calendar();
         return 0;
      }
      else if ( strcmp("suntable", argv[2]) == 0 ) {
         get_configuration(CONFIG_INIT);
	 if ( configp->loc_flag != (LATITUDE | LONGITUDE) ) {
	    fprintf(stderr,
	       "LATITUDE and/or LONGITUDE not specified in %s\n",
                  pathspec(CONFIG_FILE));
            return 1;
         }
         now = time(NULL);
         year = localtime(&now)->tm_year + 1900;
         sunmode = configp->sunmode;
         offset = configp->sunmode_offset;
         timemode = TIMEMODE_CIVIL;
         format = FMT_PORTRAIT;

         for ( j = 3; j < argc; j++ ) {
            if ( *argv[j] == '-' ) {
	       if ( strlen(argv[j]) >= 2 ) {
                  sp = argv[j] + 1;
                  if ( *sp == 'w' && strlen(argv[j]) == 2 ) {
                     format = FMT_LANDSCAPE;
		     continue;
                  }
                  else if ( *sp == 's' && strlen(argv[j]) == 2 ) {
                     timemode = TIMEMODE_STANDARD;
		     continue;
                  }
                  else if ( *sp == 'o' ) {
                     sunmode = AngleOffset;
	             if ( strlen(argv[j]) > 2 || ( argc > j + 1 && strlen(argv[j + 1]) > 0 ) ) {
		        if ( strlen(argv[j]) > 2 )
		           offset = strtol(argv[j] + 2, &sp, 10);
		        else
		           offset = strtol(argv[++j], &sp, 10);
			if (*sp == '\0')
		        	continue;
		     }
	          }
	          else if ( strlen(argv[j]) == 2 ) {
	             if ( *sp == 'r' ) {
		        sunmode = RiseSet;
                        continue;
                     }
                     else if ( *sp == 'c' ) {
		        sunmode = CivilTwi;
                        continue;
                     }
                     else if ( *sp == 'n' ) {
		        sunmode = NautTwi;
                        continue;
                     }
                     else if ( *sp == 'a' ) {
		        sunmode = AstroTwi;
                        continue;
                     }
                  }
               }
               fprintf(stderr, "Invalid parameter '%s'\n", argv[j]);
               return 1;
            }
            else {
               year = (int)strtol(argv[j], &sp, 10);
               if ( !strchr(" /t/n", *sp) || year < 1970 || year > 2099 ) {
                  fprintf(stderr, "Invalid year '%s'\n", argv[argc - 1]);
                  return 1;
               }
            }
         }

         return write_sun_table(format, year, sunmode, offset, timemode);
      }
      else if (strcmp("dawndusk", argv[2]) == 0 ) {
         get_configuration(CONFIG_INIT);
         fix_tznames();
	 if ( configp->loc_flag != (LATITUDE | LONGITUDE) ) {
	    fprintf(stderr,
	       "LATITUDE and/or LONGITUDE not specified in %s\n",
                  pathspec(CONFIG_FILE));
            return 1;
         }
         now = time(NULL);
         local_dawndusk(now, &utc0_dawn, &utc0_dusk);
         tmp = localtime(&utc0_dawn);
         printf("Dawn = %02d:%02d %s", tmp->tm_hour, tmp->tm_min, heyu_tzname[tmp->tm_isdst]);
         tmp = localtime(&utc0_dusk);
         printf("  Dusk = %02d:%02d %s  ", tmp->tm_hour,  tmp->tm_min, heyu_tzname[tmp->tm_isdst]);
         printf(sunmodelabel[configp->sunmode], configp->sunmode_offset);
         printf("\n");
         return 0;
      }
         
      else if (strcmp("calibrate", argv[2]) == 0 ) {
         printf("Calibrating for fast timing loops.\n");
	 loopcount = loop_calibrate();  /* countdown for 1 second */
	 if ( loopcount == 0 ) {
	    fprintf(stderr, "Internal error: Calibration failed - possible overflow.\n");
	    return 1;
	 }
	 printf("Paste this line into your Heyu configuration file:\n");
	 printf("TIMER_LOOPCOUNT  %lu\n", loopcount);
         return 0;
      }
      else {
         display_utility_usage( argc, argv );
         return 1;
      }
   }
   else {
      display_utility_usage( argc, argv );
      return 1;
   }

   return 0;
} 

/*---------------------------------------------------------------------+
 | Display aliases from user's config file.                            |
 +---------------------------------------------------------------------*/
void show_aliases ( ALIAS *aliasptr, unsigned int hcodemap )
{
   int  j, count, maxlabel, maxunits;
   char hc;

   printf(" [Aliases]\n");

   if ( aliasptr == (ALIAS *)NULL ) {
      printf("  -none-\n\n");
      return;
   }

   j = 0;
   count = 0;
   maxlabel = maxunits = 0;
   while ( aliasptr[j].label[0] != '\0' ) {
      maxlabel = max(maxlabel, (int)strlen(aliasptr[j].label));
      maxunits = max(maxunits, (int)strlen(bmap2units(aliasptr[j].unitbmap)));
      count++;
      j++;
   }

   if ( count == 0 ) {
      printf("  -none-\n");
      return;
   }

   for ( hc = 'A'; hc <= 'P'; hc++ ) {
      if ( (hcodemap & (1 << hc2code(hc))) == 0 ) {
         continue;
      }
      for ( j = 0; j < count; j++ ) {
         if ( toupper((int)aliasptr[j].housecode) == hc ) {
            printf("  alias  %-*s  %c%-*s  %s  %s\n", maxlabel, aliasptr[j].label, 
              aliasptr[j].housecode, maxunits, bmap2units(aliasptr[j].unitbmap),
              lookup_module_name(aliasptr[j].modtype), display_module_options(j));
         }
      }
   }

   printf("\n");

   return;
}

/*---------------------------------------------------------------------+
 | Display scenes from user's config file.                             |
 +---------------------------------------------------------------------*/
void show_scenes ( SCENE *scenep )
{
   int  j, count, maxlen;

   printf(" [Scenes]\n");
   j = 0;
   count = 0;
   maxlen = 0;
   while ( scenep && scenep[j].label[0] != '\0' ) {
      if ( scenep[j].type == F_SCENE ) {
         maxlen = max(maxlen, (int)strlen(scenep[j].label));
         count++;
      }
      j++;
   }

   if ( count == 0 ) {
      printf("  -none-\n\n");
      return;
   }

   j = 0;
   while ( scenep && scenep[j].label[0] != '\0' ) {
      if ( scenep[j].type == F_SCENE )
         printf("  scene  %-*s  %s\n",
               maxlen, scenep[j].label, scenep[j].body);
      j++;
   }
   printf("\n");
   return;
}

/*---------------------------------------------------------------------+
 | Display usersyns from user's config file.                           |
 +---------------------------------------------------------------------*/
void show_usersyns ( SCENE *scenep )
{
   int  j, count, maxlen;

   printf(" [Usersyns]\n");
   j = 0;
   count = 0;
   maxlen = 0;
   while ( scenep && scenep[j].label[0] != '\0' ) {
      if ( scenep[j].type == F_USYN ) {
          maxlen = max(maxlen, (int)strlen(scenep[j].label));
          count++;
      }
      j++;
   }

   if ( count == 0 ) {
      printf("  -none-\n\n");
      return;
   }

   j = 0;
   while ( scenep && scenep[j].label[0] != '\0' ) {
      if ( scenep[j].type == F_USYN ) {
         printf("  usersyn  %-*s  %s\n", 
               maxlen, scenep[j].label, scenep[j].body);
      }
      j++;
   }
   printf("\n");
   return;
}


/*---------------------------------------------------------------------+
 | 'show' command #1 - for information which can be determined from    |
 | the user's config file.  Return 1 if command not recognized here.   |
 +---------------------------------------------------------------------*/
int c_show1 ( int argc, char *argv[] )
{
   void x10state_show ( unsigned char );
   void remove_x10state_file ( void );
   void send_x10state_command ( unsigned char, unsigned char );
   int read_x10state_file (void );
   void show_housemap ( void );
   void show_module_mask ( unsigned char );
   void show_launcher ( unsigned char );
   void show_powerfail_launcher ( void );
   void show_sensorfail_launcher ( void );
   void show_rfflood_launcher ( void );
   void show_timeout_launcher ( void );
   void show_exec_launcher ( void );
   void show_hourly_launcher ( void );
   void show_all_launchers ( void );
   void show_configuration ( void );
   void show_configuration_compressed ( void );

   char hc;
   unsigned int hcodemap;
   int j, k;

#if 0
   static struct showlist_st {
      char type;
      int  minlabel;
      int  minparm;
      int  maxparm;
      char *option;
   } showlist = {
      {1, 2, "al[iases] [H]   Aliases defined in config file\n"},
      {2, 2, "ar[med]         Armed status of system (*)\n"},
      {1, 2, "sc[enes]        Scenes defined in config file\n"},
      {2, 2, "se[nsors]       Security sensor states (*)\n"},
      {1, 1, "u[sersyns]      Usersyns defined in config file\n"},
      {1, 1, "m[odules] H     Module attributes, housecode H\n"},
      {1, 1, "l[aunchers] [H] Launchers, all or only housecode H or -p -s -r -t\n"},
      {2, 1, "h[ousemap] [H]  Overall system state, or details housecode H (*)\n"},
      {1, 2, "da[wndusk]      Display Dawn and Dusk times for today (*)\n"},
      {2, 2, "di[mlevels]     Dim levels of modules as percent (*)\n"},
      {2, 2, "ra[wlevels]     Native levels of modules (*)\n"},
      {2, 1, "f[lags]         Flag states (*)\n"},
      {2, 2, "ti[mers]        Display active timers 1-16 (*)\n"},
      {2, 2, "ts[tamp] Hu     Date and time of last signal on Hu (*)\n"},
      {2, 1, "g[roups] H      Extended group assignments and levels (*)\n"},
      {1, 1, "c[onfig]        Display stripped Heyu configuration file\n"},
      {2, 4, "rfxs[ensors]    Tabular display of all RFXSensors (*)\n"},
      {2, 4, "rfxm[eters]     Tabular display of all RFXMeters (*)\n"},
      {2, 1, "o[thers]        Cumulative received address map (*) - clear with\n"},
      {0, 0, "                  'heyu initothers' or 'heyu initstate'\n"},
      {0, 0, "(*) Require the heyu state engine to be running\n"},
      {},
 
      while ( showlist[j].minlabel > 0 ) {
         for ( j = 0; j < strlen(argv[2]; j++ ) {
            if ( strncmp(arg
            
#endif



 

   if ( argc < 3 ) {
      printf("heyu show options:\n");
      printf("  al[iases] [H]   Aliases defined in config file\n");
      printf("  ar[med]         Armed status of system (*)\n");
      printf("  sc[enes]        Scenes defined in config file\n");
      printf("  se[nsors]       Sensor status (*)\n");
      printf("  u[sersyns]      Usersyns defined in config file\n");
      printf("  m[odules] H     Module attributes, housecode H\n");
      printf("  l[aunchers] [H] Launchers, all or only housecode H or -e|p|r|s|t\n");
      printf("  h[ousemap] [H]  Overall system state, or details housecode H (*)\n");
      printf("  da[wndusk]      Display Dawn and Dusk times for today (*)\n");
#ifdef HASDMX
      printf("  dim[levels]     Dim levels of modules as percent (*)\n");
#else
      printf("  di[mlevels]     Dim levels of modules as percent (*)\n");
#endif
      printf("  ra[wlevels]     Native levels of modules (*)\n");
      printf("  f[lags]         Flag states (*)\n");
      printf("  ti[mers]        Display active timers (*)\n");
      printf("  ts[tamp] Hu     Date and time of last signal on Hu (*)\n");
      printf("  g[roups] H      Extended group assignments and levels (*)\n");
      printf("  c[onfig]        Display stripped Heyu configuration file\n");
      printf("  x[10security]   Tabular display of all X10 Security sensors (*)\n");

#ifdef HASRFXS
#ifdef HASRFXM
      printf("  rfxs[ensors]    Tabular display of all RFXSensors (*)\n");
#else
      printf("  rf[xsensors]    Tabular display of all RFXSensors (*)\n");
#endif
#endif

#ifdef HASRFXM
#ifdef HASRFXS
      printf("  rfxm[eters]     Tabular display of all RFXMeters (*)\n");
#else
      printf("  rf[xmeters]     Tabular display of all RFXMeters (*)\n");
#endif      
#endif
#ifdef HASDMX
      printf("  dig[imax]       Tabular display of all DigiMax (*)\n");
#endif
#ifdef HASORE
      printf("  or[egon]        Tabular display of all Oregon sensors (*)\n");
      printf("  ot[hers]        Cumulative received address map (*) - clear with\n");
#else
      printf("  o[thers]        Cumulative received address map (*) - clear with\n");
#endif
      printf("                    'heyu initothers' or 'heyu initstate'\n");
      printf("  (*) Require the heyu state engine to be running\n");     
      return 0;
   }
   
   get_configuration(CONFIG_INIT);

   if ( strlen(argv[2]) == 1 && strchr("asdtro", *argv[2]) ) {
      fprintf(stderr, "'%s' is ambiguous - supply more characters\n", argv[2]);
      exit(1);
   }
   else if ( strncmp("aliases", argv[2], 2) == 0 ) {
      hcodemap = 0;
      for ( j = 3; j < argc; j++ ) {
         for ( k = 0; k < strlen(argv[j]); k++ ) {
            hc = toupper((int)((unsigned char)(*(argv[j] + k))));
            if ( hc < 'A' || hc > 'P' ) {
               fprintf(stderr, "Invalid housecode '%c'\n", *(argv[j] + k));
               exit(1);
            }
            hcodemap |= 1 << hc2code(hc);
         }
      }
      hcodemap = (hcodemap == 0) ? 0xffff : hcodemap;
      show_aliases(configp->aliasp, hcodemap);
   }
   else if ( strncmp("scenes", argv[2], 2) == 0 )
      show_scenes(configp->scenep);
   else if ( strncmp("usersyns", argv[2], 1) == 0 )
      show_usersyns(configp->scenep);
   else if ( strncmp("modules", argv[2], 1) == 0 ) {
      if ( argc < 4 ) {
         fprintf(stderr, "Housecode needed\n");
         exit(1);
      }
      hc = toupper((int)(*argv[3]));
      if ( hc < 'A' || hc > 'P' ) {
         fprintf(stderr, "Invalid housecode '%s'\n", argv[3]);
         exit(1);
      }
      show_module_mask(hc2code(hc));
   }
   else if ( strncmp("launchers", argv[2], 1) == 0 ) {
      if ( argc < 4 ) {
         show_all_launchers();
         printf("\n");
      }
      else if ( strncmp(argv[3], "-powerfail", 2) == 0 ) {
         show_powerfail_launcher();
         printf("\n");
      }
      else if ( strncmp(argv[3], "-sensorfail", 2) == 0 ) {
         show_sensorfail_launcher();
         printf("\n");
      }
      else if ( strncmp(argv[3], "-rfflood", 2) == 0 ) {
         show_rfflood_launcher();
         printf("\n");
      }
      else if ( strncmp(argv[3], "-timeout", 2) == 0 ) {
         show_timeout_launcher();
         printf("\n");
      }
      else if ( strncmp(argv[3], "-exec", 2) == 0 ) {
         show_exec_launcher();
         printf("\n");
      }
      else if ( strncmp(argv[3], "-hourly", 2) == 0 ) {
         show_hourly_launcher();
         printf("\n");
      }
      else {
         hc = toupper((int)(*argv[3]));
         if ( hc < 'A' || hc > 'P' ) {
            fprintf(stderr, "Invalid housecode '%s'\n", argv[3]);
            exit(1);
         }
         show_launcher(hc2code(hc));
         printf("\n");
      }
   }
   else if ( strncmp("config", argv[2], 1) == 0 ) {
      show_configuration();
      printf("\n");
   }
   else {
      return 1;
   }

   return 0;
}

/*---------------------------------------------------------------------+
 | 'show' command #2 - for information requiring the state engine be   |
 | running.                                                            |
 +---------------------------------------------------------------------*/
int c_show2 ( int argc, char *argv[] )
{
   void x10state_show ( unsigned char );
   int  fetch_x10state ( void );
   void show_housemap ( void );
   void show_module_mask ( unsigned char );
   void show_launcher ( unsigned char );
   void show_all_dimlevels_raw ( void );
   void show_sticky_addr ( void );
   void show_flags_old (void );
   void show_geoflags (void );
   void show_long_flags ( void );
   int  check_for_engine ( void );
   int  read_x10state_file ( void );
   int  show_signal_timestamp ( char * );
   int  show_security_sensors ( void );
   void show_armed_status ( void );
   int  show_global_timers ( void );
   int  show_state_dawndusk (void );
   void show_extended_groups ( unsigned char );
   int  show_rfxsensors ( void );
   int  show_rfxmeters ( void );
   int  show_digimax ( void );
   int  show_oregon ( void );
   int  show_x10_security ( void );

   char hc;
   
   if ( check_for_engine() != 0 ) {
      fprintf(stderr, "State engine is not running.\n");
      return 1;
   }
		       
   if ( argc < 3 ) {
      fprintf(stderr, "Too few arguments\n");
      return 1;
   }
   if ( read_x10state_file() != 0 ) {
      fprintf(stderr, "Unable to read state file.\n");
      return 1;
   }
      
   if ( strncmp("housemap", argv[2], 1) == 0 ) {
      if ( argc == 3 ) {  
         show_housemap();
      }
      else {
         if ( fetch_x10state() != 0 )
            return 1;
         hc = toupper((int)(*argv[3]));
         if ( hc < 'A' || hc > 'P' ) {
            fprintf(stderr, "Invalid housecode '%s'\n", argv[3]);
            exit(1);
         }
         x10state_show(hc2code(hc));
      }
   }
   else if ( strncmp("dimlevels", argv[2], 3) == 0 ) {
      show_all_dimlevels();
   }
   else if ( strncmp("rawlevels", argv[2], 2) == 0 ) {
      show_all_dimlevels_raw();
   }
   else if ( strncmp("flags", argv[2], 1) == 0 ) {
      if ( fetch_x10state() != 0 )
         return 1;
      if ( configp->show_flags_mode == NEW )
         show_long_flags();
      else
         show_flags_old();
   }
   else if ( strncmp("geoflags", argv[2], 2) == 0 ) {
      if ( fetch_x10state() != 0 )
         return 1;
      show_geoflags();
   }
   else if ( strncmp("armed", argv[2], 2) == 0 ) {
      if ( fetch_x10state() != 0 )
         return 1;
      show_armed_status();
   }
   else if ( strncmp("sensors", argv[2], 2) == 0 ) {
      show_security_sensors();
   }
   else if ( strncmp("others", argv[2], 2) == 0 ) {
      if ( fetch_x10state() != 0 )
         return 1;
      show_sticky_addr();
   }
   else if ( strncmp("tstamp", argv[2], 2) == 0 ) {
      if ( argc < 4 ) {
         fprintf(stderr, "Too few parameters.\n");
         return 1;
      }
      else if ( argc > 4 ) {
         fprintf(stderr, "Too many parameters.\n");
         return 1;
      }
      return show_signal_timestamp(argv[3]);
   }
   else if ( strncmp("timers", argv[2], 2) == 0 ) {
      if ( fetch_x10state() != 0 )
         return 1;
      return show_global_timers();
   }
   else if ( strncmp("dawndusk", argv[2], 2) == 0 ) {
      if ( fetch_x10state() != 0 )
         return 1;
      return show_state_dawndusk();
   }
   else if ( strncmp("groups", argv[2], 1) == 0 ) {
      if ( argc < 4 ) {
         fprintf(stderr, "Housecode needed\n");
         return 1;
      }
      hc = toupper((int)(*argv[3]));
      if ( hc < 'A' || hc > 'P' ) {
         fprintf(stderr, "Invalid housecode '%s'\n", argv[3]);
         return 1;
      }
      show_extended_groups(hc2code(hc));
   }
   else if ( strncmp("x10security", argv[2], 1) == 0 ) {
      return show_x10_security();
   }

#ifdef HASRFXM
   else if ( strncmp("rfxmeters", argv[2], 4) == 0 ) {
      return show_rfxmeters();
   }
#ifdef HASRFXS
   else if ( (strlen(argv[2]) == 2 && strcmp("rf",  argv[2]) == 0) || 
             (strlen(argv[2]) == 3 && strcmp("rfx", argv[2]) == 0)    ) {
     fprintf(stderr, "%s is ambiguous - supply more characters.\n", argv[2]);
     return 1;
   }
#else
   else if ( strncmp(argv[2], "rf", 2) == 0 ) {
     return show_rfxmeters();
   }
#endif
#endif


#ifdef HASRFXS
   else if ( strncmp("rfxsensors", argv[2], 4) == 0 ) {
      return show_rfxsensors();
   }
#ifdef HASRFXM
   else if ( (strlen(argv[2]) == 2 && strcmp("rf",  argv[2]) == 0) || 
             (strlen(argv[2]) == 3 && strcmp("rfx", argv[2]) == 0)    ) {
      fprintf(stderr, "%s is ambiguous - supply more characters.\n", argv[2]);
      return 1;
   }
#else
   else if ( strncmp(argv[2], "rf", 2) == 0 ) {
      return show_rfxsensors();
   }
#endif
#endif

   else if ( strncmp(argv[2], "digimax", 3) == 0 ) {
      return show_digimax();
   }
#ifdef HASORE
   else if ( strncmp(argv[2], "oregon", 2) == 0 ) {
      return show_oregon();
   }
#endif
            
   else {
      fprintf(stderr, "Show argument '%s' not recognized.\n", argv[2]);
      exit(1);
   }

   return 0;
}



/*---------------------------------------------------------------------+
 |  Translate a macro from the image of an EEPROM in memory into       |
 |  direct commands and send to the CM11A as direct commands.          |
 |  Argument 'image' is a pointer to the 1024 byte image buffer in     |
 |  memory.  Argument 'macaddr' is the offset of the macro in that     |
 |  buffer.                                                            |
 |  The macro delay is ignored, and operation does not chain to a      |
 |  subsequent delayed macro as it would in actual CM11A operation.    |
 |  Return 0 if successful, otherwise 1 on first failure.              |
 +---------------------------------------------------------------------*/
int send_macro_immediate ( unsigned char *image, unsigned int macaddr )
{
   unsigned char hcode, func = 0xff, nelem;
   unsigned char buf[16];
   unsigned char *sp;
   unsigned int  bitmap, staflag;
   int           k, retcode = 0;
   int           syncmode;
   int           line_sync_mode(void);

   syncmode = line_sync_mode();

   /* Ignore delay */
   sp = image + macaddr + 1;
   nelem = *sp++;

   /* Sent each element */
   for ( k = 0; k < (int)nelem; k++ ) {
      if ( sp < (image + 5) || sp > (image + 0x3ff) ) {
         fprintf(stderr, "Macro element is outside EEPROM image\n");
         return 1;
      }
      hcode = (sp[0] & 0xf0u) >> 4;
      func =  sp[0] & 0x0fu;
      bitmap = (sp[1] << 8) | sp[2];
      staflag = 0;

      /* Send the address */
      if ( send_address(hcode, bitmap, syncmode) != 0 ) {
         fprintf(stderr, "Unable to send address\n");
         return 1;
      }

      /* Send the function */
      if ( func == 4 || func == 5 ) {
         /* Dim or Bright function */
         if ( sp[3] & 0x80u ) {
            /* Brighten before dimming */
            buf[0] = 0x06 | (22 << 3);
            buf[1] = (sp[0] & 0xf0u) | 5;
            if ( (retcode = send_command(buf, 2, 0, syncmode)) != 0 )
               break;
         }
         buf[0] = 0x06 | ((sp[3] & 0x7fu) << 3);
         buf[1] = sp[0];
         
         if ( (retcode = send_command(buf, 2, 0, syncmode)) != 0 )
            break;
         sp += 4;
      }
      else if ( func == 7 ) {
         /* Extended function */
         buf[0] = 0x07u;
         buf[1] = (sp[0] & 0xf0u) | 0x07u;
         buf[2] = sp[3];
         buf[3] = sp[4];
         buf[4] = sp[5];

         staflag = (buf[4] == 0x37) ? 1 : 0;

         /* Kluge "fix" for checksum 5A problem.    */
         /* CM11A seems to disregard the dim field  */
         /* in the header byte.                     */
         if ( checksum(buf, 5) == 0x5A && configp->fix_5a == YES )
            buf[0] = 0x0F;

         if ( (retcode = send_command(buf, 5, staflag, syncmode)) != 0 )
            break;
         sp += 6;
      }
      else if ( func == 15 ) {
         /* Status Request function */
         staflag = 1;
         buf[0] = 0x06;
         buf[1] = sp[0];
         if ( (retcode = send_command(buf, 2, staflag, syncmode)) != 0 )
            break;
         sp += 3;
      }
      else {
         /* Basic function */
         buf[0] = 0x06;
         buf[1] = sp[0];
         if ( (retcode = send_command(buf, 2, 0, syncmode)) != 0 )
            break;
         sp += 3;
      }
   }

   if ( retcode != 0 )
      fprintf(stderr, "Unable to send command function %d\n", func);
   
   return retcode;
}

/*---------------------------------------------------------------------+
 |  Translate a macro from the image of an EEPROM in memory into       |
 |  direct commands and send to the CM11A as direct commands.          |
 |  Argument 'image' is a pointer to the 1024 byte image buffer in     |
 |  memory.  Argument 'macaddr' is the offset of the macro in that     |
 |  buffer.                                                            |
 |  The macro delay itself is ignored, but operation chains to a       |
 |  subsequent delayed macro as it would in actual CM11A operation.    |
 |  Return 0 if successful, otherwise 1 on first failure.              |
 +---------------------------------------------------------------------*/
int send_macro_chain_immediate ( unsigned char *image, unsigned int macaddr )
{
   unsigned char hcode, func = 0xff, nelem;
   unsigned char buf[16];
   unsigned char *sp;
   unsigned char ischained;
   unsigned int  bitmap, staflag;
   int           k, retcode = 0;
   int           syncmode;
   int           line_sync_mode(void);

   ischained = 1;
   sp = image + macaddr + 1;

   syncmode = line_sync_mode();

   while ( ischained != 0 ) {
      nelem = *sp++;

      /* Sent each element */
      for ( k = 0; k < (int)nelem; k++ ) {
         if ( sp < (image + 5) || sp > (image + 0x3ff) ) {
            fprintf(stderr, "Macro element is outside EEPROM image\n");
            return 1;
         }
         hcode = (sp[0] & 0xf0u) >> 4;
         func =  sp[0] & 0x0fu;
         bitmap = (sp[1] << 8) | sp[2];
         staflag = 0;

         /* Send the address */
         if ( send_address(hcode, bitmap, syncmode) != 0 ) {
            fprintf(stderr, "Unable to send address\n");
            return 1;
         }

         /* Send the function */
         if ( func == 4 || func == 5 ) {
            /* Dim or Bright function */
            if ( sp[3] & 0x80u ) {
               /* Brighten before dimming */
               buf[0] = 0x06 | (22 << 3);
               buf[1] = (sp[0] & 0xf0u) | 5;
               if ( (retcode = send_command(buf, 2, 0, syncmode)) != 0 )
                  break;
            }
            buf[0] = 0x06 | ((sp[3] & 0x7fu) << 3);
            buf[1] = sp[0];
         
            if ( (retcode = send_command(buf, 2, 0, syncmode)) != 0 )
               break;
            sp += 4;
         }
         else if ( func == 7 ) {
            /* Extended function */
            buf[0] = 0x07;
            buf[1] = (sp[0] & 0xf0u) | 0x07u;
            buf[2] = sp[3];
            buf[3] = sp[4];
            buf[4] = sp[5];

            staflag = (buf[4] == 0x37) ? 1 : 0;

            /* Kluge "fix" for checksum 5A problem.    */
            /* CM11A seems to disregard the dim field  */
            /* in the header byte.                     */
            if ( checksum(buf, 5) == 0x5A && configp->fix_5a == YES )
               buf[0] = 0x0F;

            if ( (retcode = send_command(buf, 5, staflag, syncmode)) != 0 )
               break;
            sp += 6;
         }
         else if ( func == 15 ) {
            /* Status Request function */
            buf[0] = 0x06;
            buf[1] = sp[0];
            staflag = 1;
            if ( (retcode = send_command(buf, 2, staflag, syncmode)) != 0 )
               break;
            sp += 3;
         }
         else {
            /* Basic function */
            buf[0] = 0x06;
            buf[1] = sp[0];
            if ( (retcode = send_command(buf, 2, 0, syncmode)) != 0 )
               break;
            sp += 3;
         }
      }
      ischained = *sp++;
   }

   if ( retcode != 0 )
      fprintf(stderr, "Unable to send command function %d\n", func);
   
   return retcode;
}


/*---------------------------------------------------------------------+
 |  Return the total length in bytes of the macro pointed to by macptr |
 +---------------------------------------------------------------------*/
unsigned char macro_length ( unsigned char *macptr )
{
   unsigned char k, nelem, func;
   unsigned char *sp;

   sp = macptr + 1;
   nelem = *sp++;

   for ( k = 0; k < nelem; k++ ) {
      func = *sp & 0x0fu;
      if ( func == 4 || func == 5 )
         sp += 4;
      else if ( func == 7 )
         sp += 6;
      else
         sp += 3;
   }
   return (sp - macptr);
} 

/* Structure used by c_catchup() */
struct catchup_st {
   unsigned int  start;
   unsigned int  macaddr;
   unsigned char delay;
};

/*---------------------------------------------------------------------+
 | qsort compare function for c_catchup()                              |
 | Delayed events are ordered before undelayed events if the actual    |
 | execution times are the same.                                       |
 +---------------------------------------------------------------------*/
int cmp_catchup ( struct catchup_st *one, struct catchup_st *two )
{
   return  (one->start < two->start) ? -1 :
           (one->start > two->start) ?  1 :
           (one->delay > two->delay) ? -1 :
           (one->delay < two->delay) ?  1 : 0 ;
} 

/*---------------------------------------------------------------------+
 |  Execute the commands in macros in an uploaded schedule for timer   |
 |  events scheduled between 0:00 hours and the current time today.    |
 +---------------------------------------------------------------------*/
int c_catchup ( int argc, char *argv[] )
{
   time_t        now;
   struct tm     *tmp;
   int           j, ichksum, size;
   unsigned int           day, minutes;
   int           (*fptr)() = &cmp_catchup;
   unsigned char dow, dow_bmap;
   unsigned char *image, *sp, *dp;
   unsigned int  beg, end, start, stop, delay;
   unsigned int  macstart, macstop, macaddr;
   unsigned char *image_ptr(void);
   struct catchup_st table[210];
   char     maclabel[MACRO_LEN + 1];

   if ( invalidate_for_cm10a() != 0 )
      return 1;

   switch ( get_upload_expire() ) {
     case NO_RECORD_FILE :
        fprintf(stderr, "No schedule has been uploaded by Heyu.\n");
        return 1;
     case NO_EXPIRATION :
        fprintf(stderr, "Uploaded schedule contains no Timers.\n");
        return 1;
     case BAD_RECORD_FILE :
        fprintf(stderr, "X10 Record File has been corrupted.\n"); 
        return 1;
     case SCHEDULE_EXPIRED :
        fprintf(stderr, "Uploaded schedule has expired.\n");
        return 1;
     default :
        break;
   }

   lookup_macro(0, maclabel, &ichksum);
   if ( !loadcheck_image(ichksum) ) {
      fprintf(stderr, "Unable to load x10image file, ichksum = %d\n", ichksum);
      return 1;
   }
   image = image_ptr();

   time(&now);
   tmp = legal_to_cm11a(&now);
   minutes = 60 * tmp->tm_hour + tmp->tm_min + ((tmp->tm_sec > 0) ? 1 : 0);

   day = tmp->tm_yday;
   dow = 1 << (tmp->tm_wday);

   sp = image + 2;
   size = 0;
   while ( *sp != 0xff ) {
      dow_bmap = sp[0];
      beg = sp[1] | ((sp[4] & 0x80u) << 1);
      end = sp[2] | ((sp[5] & 0x80u) << 1);
      if ( !(dow & dow_bmap) || day < beg || day > end ) {
         sp += 9;
         continue;
      }
      start = 120 * ((sp[3] & 0xf0u) >> 4) + (sp[4] & 0x7fu) +
                   SECURITY_OFFSET_ADJUST * ((sp[6] & 0x80u) >> 7);
      stop  = 120 * (sp[3] & 0x0fu) + (sp[5] & 0x7fu) +
                   SECURITY_OFFSET_ADJUST * ((sp[6] & 0x08u) >> 3);
      macstart = sp[7] | ((sp[6] & 0x30u) << 4);
      macstop = sp[8] | ((sp[6] & 0x03u) << 8);

      /* Add macro delay to start time */
      delay = *(image + macstart);
      start += delay;
      if ( start < minutes ) {
         table[size].start = start;
         table[size].macaddr = macstart;
         table[size].delay = (delay > 0) ? 1 : 0;
         size++;
         /* Include any chained macros */
         dp = image + macstart;
         while ( *(dp += macro_length(dp)) > 0 ) {
            start += *dp;
            if ( start >= minutes )
               break; 
            table[size].start = start;
            table[size].macaddr = dp - image;
            table[size].delay = 1;
            size++;
         }
      }
   
      delay = *(image + macstop);
      stop  += delay;
      if ( stop < minutes ) {
         table[size].start = stop;
         table[size].macaddr = macstop;
         table[size].delay = (delay > 0) ? 1 : 0;
         size++;
         /* Include any chained macros */
         dp = image + macstop;
         while ( *(dp += macro_length(dp)) > 0 ) {
            stop += *dp;
            if ( stop >= minutes )
               break; 
            table[size].start = stop;
            table[size].macaddr = dp - image;
            table[size].delay = 1;
            size++;
         }
      }
      sp += 9;
   }

   if ( size == 0 ) {
      fprintf(stderr,
        "No macros scheduled for execution before now.\n");
      return 0;
   }

   /* Sort the table in order of ascending execution times */
   qsort((void *)table, size, (sizeof(struct catchup_st)), fptr);

   /* Now send the macro commands */
   for ( j = 0; j < size; j++ ) {
      macaddr = table[j].macaddr;
      lookup_macro(macaddr, maclabel, &ichksum);
      printf("Emulating macro %s at address %d\n",
                   maclabel, macaddr);
      fflush(stdout);
      if ( send_macro_immediate(image, macaddr) != 0 )
         return 1;
   }

   return 0;
}


/*---------------------------------------------------------------------+
 |  Execute the commands in an uploaded macro.                         |
 +---------------------------------------------------------------------*/
int c_macro ( int argc, char *argv[] )
{
   int           macaddr, ichksum;
   unsigned char *image;
   unsigned char *image_ptr(void);

   if ( invalidate_for_cm10a() != 0 )
      return 1;

   switch ( get_upload_expire() ) {
     case NO_RECORD_FILE :
        fprintf(stderr, "No schedule has been uploaded by Heyu.\n");
        return 1;
     case BAD_RECORD_FILE :
        fprintf(stderr, "X10 Record File has been corrupted.\n"); 
        return 1;
     default :
        break;
   }

   if ( argc < 3 ) {
      fprintf(stderr, "Macro name needed.\n");
      return 1;
   }

   if ( (macaddr = macro_rev_lookup(argv[2], &ichksum)) < 0 ) {
      fprintf(stderr, "Macro '%s' not found in %s file.\n",
                                    argv[2], pathspec(MACROXREF_FILE));
      return 1;
   }

   if ( !loadcheck_image(ichksum) ) {
      fprintf(stderr, "Unable to load x10image file, ichksum = %d\n", ichksum);
      return 1;
   }
   image = image_ptr();

   printf("Emulating macro %s at address %d\n", argv[2], macaddr);
   fflush(stdout);

   if ( send_macro_immediate(image, macaddr) != 0 )
      return 1;

   return 0;
}

/*---------------------------------------------------------------------+
 |  Read heyu options from the command line and store pointers in      |
 |  structure opt_st.  Return the number of argv[] tokens used for the |
 |  options, or -1 if invalid usage or invalid option.                 |
 |  Options supported:                                                 |
 |    -v                 Verbose mode.                                 |
 |    -c <pathname>      Configuration file.                           |
 |    -s <pathname>      Schedule file.                                |
 |    -0 ... -9          Subdirectory of $HOME/.heyu/ or /etc/heyu/    |
 |                        where config file is stored, e.g.,           |
 |                         -2 => $HOME/.heyu/2/x10config               |
 |    -tr                Sync with rising AC slope.                    |
 |    -tf                Sync with falling AC slope.                   |
 +---------------------------------------------------------------------*/
int heyu_getopt( int argc, char **argv, struct opt_st *optptr )
{
   int j, ntokens;

   optptr->configp = NULL;
   optptr->schedp = NULL;
   optptr->subp = NULL;
   optptr->dispsub[0] = '\0';
   optptr->verbose = 0;
   optptr->linesync = NO_SYNC;

   if ( argc < 2 )
      return -1;

   ntokens = 0;
   for ( j = 1; j < argc; j++ ) {
      if ( *argv[j] != '-' )
         break;

      if ( strncmp(argv[j], "-v", 2) == 0 ) {
         if ( strlen(argv[j]) == 2 ) {
            optptr->verbose = 1;
            ntokens++;
         }
         else {
            fprintf(stderr, "Invalid option '%s'\n", argv[j]);
            return -1;
         }
      }
      else if ( strncmp(argv[j], "-c", 2) == 0 ) {
         if ( (int)strlen(argv[j]) > 2 ) {
            optptr->configp = argv[j] + 1;
            ntokens++;
         }
         else if ( (j + 1) < argc && *argv[j + 1] != '-' ) {
            optptr->configp = argv[j + 1];
            ntokens += 2;
            j++;
         }
         else {
            fprintf(stderr, "Invalid option usage '%s'\n", argv[j]);
            return -1;
         }
      }
      else if ( strncmp(argv[j], "-s", 2) == 0 ) {
         if ( (int)strlen(argv[j]) > 2 ) {
            optptr->schedp = argv[j] + 2;
            ntokens++;
         }
         else if ( (j + 1) < argc && *(argv[j + 1]) != '-' ) {
            optptr->schedp = argv[j + 1];
            ntokens += 2;
            j++;
         }
         else {
            fprintf(stderr, "Invalid option usage '%s'\n", argv[j]);
            return -1;
         }
      }
      else if ( strncmp(argv[j], "-tr", 3) == 0 ) {
         if ( strlen(argv[j]) == 3 ) {
            optptr->linesync = RISE_SYNC;
            ntokens++;
         }
         else {
            fprintf(stderr, "Invalid option '%s'\n", argv[j]);
            return -1;
         }
      }
      else if ( strncmp(argv[j], "-tf", 3) == 0 ) {
         if ( strlen(argv[j]) == 3 ) {
            optptr->linesync = FALL_SYNC;
            ntokens++;
         }
         else {
            fprintf(stderr, "Invalid option '%s'\n", argv[j]);
            return -1;
         }
      }
      else if ( strlen(argv[j]) == 2 && isdigit((int)(*(argv[j] + 1))) ) {
         optptr->subp = argv[j] + 1;
         strncpy2(optptr->dispsub, argv[j] + 1, NAME_LEN - 1);
         ntokens++;
      }

      else {
         fprintf(stderr, "Invalid parameter '%s'\n", argv[j]);
         return -1;
      }
   }

   if ( optptr->subp && optptr->configp ) {
      fprintf(stderr,
        "Options -%s and -c may not both be specified.\n", optptr->subp);
      return -1;
   }
          
   return ntokens;
}

/*---------------------------------------------------------------------+
 | Return the line synchronization mode specified by CL option.        |
 +---------------------------------------------------------------------*/
int line_sync_mode ( void )
{
   return optptr->linesync;
}
 
/*---------------------------------------------------------------------+
 | Return the current system time as seconds after 0:00:00 hours legal |
 | (wall clock) time.                                                  |
 +---------------------------------------------------------------------*/
long int systime_now ( void )
{
   time_t     now;
   struct tm  *tms;

   time(&now);
   tms = localtime(&now);

   return ( 3600 * tms->tm_hour + 60 * tms->tm_min + tms->tm_sec);
}


/*---------------------------------------------------------------------+
 | Locate the macro address (if any) corresponding to the argument     |
 | trigger hcode|ucode and function (1 = on, 0 = off).  Return 1 if    |
 | found and pass back the macro address through the argument list.    |
 | Otherwise return 0.                                                 |
 +---------------------------------------------------------------------*/
int locate_triggered_macro ( unsigned char *imagep, char hc, int unit,
      char *trig_func, int *macaddr)
{
   unsigned char *cp;
   unsigned char trigaddr, funcmask;

   trigaddr = hc2code(hc) << 4 | unit2code(unit);
   funcmask = (!strcmp(strlower(trig_func), "off")) ? 0x00 : 0x80;
   
   cp = imagep + (imagep[0] << 8) + imagep[1];

   while (*cp != 0xffu && *(cp+1) != 0xffu && cp < (imagep + PROMSIZE) ) {
      if ( *cp == trigaddr && (*(cp+1) & 0x80u) == funcmask ) {
         *macaddr = (*(cp+1) & 0x0fu) << 8 | *(cp+2);
         return 1;
      }
      cp += 3;
   }
   return 0;
}

/*---------------------------------------------------------------------+
 |  Execute commands in a uploaded macro image as if triggered.        |
 |    argv[2] = Hu address, argv[3] = "on" or "off"                    |
 +---------------------------------------------------------------------*/
int c_trigger ( int argc, char *argv[] )
{
   int           macaddr;
   int           ichksum, xchksum;
   unsigned char *imagep;
   unsigned char *image_ptr(void);
   char          maclabel[MACRO_LEN + 1];
   unsigned int  aflags, bitmap;
   char          hc, unit;

   if ( invalidate_for_cm10a() != 0 )
      return 1;

   switch ( get_upload_expire() ) {
     case NO_RECORD_FILE :
        fprintf(stderr, "No schedule has been uploaded by Heyu.\n");
        return 1;
     default :
        break;
   }

   if ( argc != 4 || (strcmp(argv[3], "on") != 0 && strcmp(argv[3], "off") != 0) ) {
      fprintf(stderr, "Usage: %s trigger Hu on|off\n", argv[0]);
      return 1;
   }

   aflags = parse_addr(argv[2], &hc, &bitmap);
   if ( !(aflags & A_VALID) || (aflags & (A_PLUS | A_MINUS | A_MULT)) || bitmap == 0 ) {
      fprintf(stderr, "Invalid Housecode|Unit address %s\n", argv[2]);
      return 1;
   }
   unit = code2unit(single_bmap_unit(bitmap ));

   if ( load_image(&ichksum) == 0 ) {
      fprintf(stderr, "Unable to load x10image file.\n");
      return 1;
   }
   imagep = image_ptr();

   if ( locate_triggered_macro(imagep, hc, unit, argv[3], &macaddr) == 0 ) {
      fprintf(stderr, "No trigger for '%c%d %s' found in file %s.\n",
                   hc, unit, argv[3], pathspec(IMAGE_FILE));
      return 1;
   }

   if ( lookup_macro(macaddr, maclabel, &xchksum) == 0 ) {
      fprintf(stderr, "Unknown macro at triggered address 0x%03x\n", macaddr);
      return 1;
   }
   if ( xchksum != ichksum ) {
      fprintf(stderr, "Mismatch between x10image and x10macroxref files.\n");
      return 1;
   }

   printf("Emulating triggered macro %s\n", maclabel);
   fflush(stdout);

   if ( send_macro_immediate(imagep, macaddr) != 0 )
      return 1;

   return 0;
}

/*---------------------------------------------------------------------+
 |  Execute the commands in macros in an uploaded schedule for timer   |
 |  events scheduled between 0:00 hours and the current time today.    |
 +---------------------------------------------------------------------*/
int c_timer_times ( int argc, char *argv[] )
{
   time_t        now;
   struct tm     *tmp;
   int           j, ichksum, size;
   unsigned int           day, minutes;
   int           (*fptr)() = &cmp_catchup;
   unsigned char dow, dow_bmap;
   unsigned char *image, *sp, *dp;
   unsigned int  beg, end, start, stop, delay;
   unsigned int  macstart, macstop, macaddr;
   unsigned char *image_ptr(void);
   struct catchup_st table[210];
   char     maclabel[MACRO_LEN + 1];

   if ( invalidate_for_cm10a() != 0 )
      return 1;

   switch ( get_upload_expire() ) {
     case NO_RECORD_FILE :
        fprintf(stderr, "No schedule has been uploaded by Heyu.\n");
        return 1;
     case NO_EXPIRATION :
        fprintf(stderr, "Uploaded schedule contains no Timers.\n");
        return 1;
     case BAD_RECORD_FILE :
        fprintf(stderr, "X10 Record File has been corrupted.\n"); 
        return 1;
     case SCHEDULE_EXPIRED :
        fprintf(stderr, "Uploaded schedule has expired.\n");
        return 1;
     default :
        break;
   }

   lookup_macro(0, maclabel, &ichksum);
   if ( !loadcheck_image(ichksum) ) {
      fprintf(stderr, "Unable to load x10image file, ichksum = %d\n", ichksum);
      return 1;
   }
   image = image_ptr();

   time(&now);
   tmp = legal_to_cm11a(&now);
   minutes = 60 * tmp->tm_hour + tmp->tm_min + ((tmp->tm_sec > 0) ? 1 : 0);

   day = tmp->tm_yday;
   dow = 1 << (tmp->tm_wday);

minutes = 1440;

   sp = image + 2;
   size = 0;
   while ( *sp != 0xff ) {
      dow_bmap = sp[0];
      beg = sp[1] | ((sp[4] & 0x80u) << 1);
      end = sp[2] | ((sp[5] & 0x80u) << 1);
      if ( !(dow & dow_bmap) || day < beg || day > end ) {
         sp += 9;
         continue;
      }
      start = 120 * ((sp[3] & 0xf0u) >> 4) + (sp[4] & 0x7fu) +
                   SECURITY_OFFSET_ADJUST * ((sp[6] & 0x80u) >> 7);
      stop  = 120 * (sp[3] & 0x0fu) + (sp[5] & 0x7fu) +
                   SECURITY_OFFSET_ADJUST * ((sp[6] & 0x08u) >> 3);
      macstart = sp[7] | ((sp[6] & 0x30u) << 4);
      macstop = sp[8] | ((sp[6] & 0x03u) << 8);

      /* Add macro delay to start time */
      delay = *(image + macstart);
      start += delay;
      if ( start < minutes ) {
         table[size].start = start;
         table[size].macaddr = macstart;
         table[size].delay = (delay > 0) ? 1 : 0;
         size++;
         /* Include any chained macros */
         dp = image + macstart;
         while ( *(dp += macro_length(dp)) > 0 ) {
            start += *dp;
            if ( start >= minutes )
               break; 
            table[size].start = start;
            table[size].macaddr = dp - image;
            table[size].delay = 1;
            size++;
         }
      }
   
      delay = *(image + macstop);
      stop  += delay;
      if ( stop < minutes ) {
         table[size].start = stop;
         table[size].macaddr = macstop;
         table[size].delay = (delay > 0) ? 1 : 0;
         size++;
         /* Include any chained macros */
         dp = image + macstop;
         while ( *(dp += macro_length(dp)) > 0 ) {
            stop += *dp;
            if ( stop >= minutes )
               break; 
            table[size].start = stop;
            table[size].macaddr = dp - image;
            table[size].delay = 1;
            size++;
         }
      }
      sp += 9;
   }

   if ( size == 0 ) {
      fprintf(stderr,
        "No macros scheduled for execution before now.\n");
      return 0;
   }

   /* Sort the table in order of ascending execution times */
   qsort((void *)table, size, (sizeof(struct catchup_st)), fptr);

   /* Now display the execution times and macro */
   for ( j = 0; j < size; j++ ) {
      macaddr = table[j].macaddr;
      lookup_macro(macaddr, maclabel, &ichksum);
      start = table[j].start;
      printf("%02d:%02d  %s\n", start / 60, start % 60, maclabel);
   }

   return 0;
}

/*-----------------------------------------------------------------+
 | Determine elapsed minutes from 0:00 hrs Standard Time on Jan 1  |
 | of the current year until the next NDSTINTV changes between     |
 | Standard/Daylight Time.  Store results in global struct         |
 | dststruct array lgls[] and in struct config.                    |
 +-----------------------------------------------------------------*/
int get_dst_info ( int startyear )
{
   time_t      now, seconds, startsec, jan1sec, jul1sec, epoch ;
   struct tm   jantms, jultms, *tmjan, *tmjul, *tms;
   int         indx, sindx = 0, j, nintv, val, startval, dstminutes, year;
   int         iter, result = -1, restart;
   int         offset[2];
   long        delta;

   /* Get current date and time */
   time(&now) ;
   tms = localtime(&now);

   year = tms->tm_year + 1900;
   if ( startyear >= 1970 )
      year = startyear;

   tms->tm_year = year - 1900;

   /* Get calendar seconds at 0:00 hours Legal Time on Jan 1st of this year */
   tms->tm_mon = 0;
   tms->tm_mday = 1;
   tms->tm_hour = 0;
   tms->tm_min = tms->tm_sec = 0;
   tms->tm_isdst = -1;

   epoch = mktime(tms);
   epoch = ((epoch - std_tzone) / (time_t)86400) * (time_t)86400 + std_tzone;

   tmjan = &jantms;

   for ( j = 0; j < NDSTINTV/2; j++ ) {

      /* Get calendar seconds at 0:00 hours Legal Time on Jan 1st of the year */
      tmjan->tm_year = year + j - 1900;
      tmjan->tm_mon = 0;
      tmjan->tm_mday = 1;
      tmjan->tm_hour = 0;
      tmjan->tm_min = tmjan->tm_sec = 0;
      tmjan->tm_isdst = -1;

      jan1sec = mktime(tmjan);

      tmjul = &jultms;

      /* Calendar seconds at same legal time on July 1st */
      tmjul->tm_year = year + j - 1900;
      tmjul->tm_mon = 6;
      tmjul->tm_mday = 1;
      tmjul->tm_hour = 0;
      tmjul->tm_min = tmjul->tm_sec = 0;
      tmjul->tm_isdst = -1;

      jul1sec = mktime(tmjul);

      /* Reduce difference by full days of 86400 seconds */
      dstminutes = (int)((jul1sec - jan1sec) % (time_t)86400 / (time_t)60);
      dstminutes = min( dstminutes, 1440 - dstminutes );

      offset[0] = 0;
      offset[1] = dstminutes;

      /* Reduce to seconds at 0:00 hours Standard Time */
      jan1sec = ((jan1sec - std_tzone) / (time_t)86400) * (time_t)86400 + std_tzone;

      if ( (val = tmjan->tm_isdst) > 0 ) {
         /* Daylight time in Southern hemisphere */
         indx = 1;
         startval = val;
      }
      else if ( (val = tmjul->tm_isdst) > 0 ) {
         /* Daylight time in Northern hemisphere */
         indx = 0;
         startval = 0;
      }
      else {
         /* Daylight time not in effect this year */
         lgls[2 * j].elapsed = lgls[2 * j + 1].elapsed = -1;
         lgls[2 * j].offset  = lgls[2 * j + 1].offset = 0;
         stds[2 * j].elapsed = stds[2 * j + 1].elapsed = -1;
         stds[2 * j].offset  = stds[2 * j + 1].offset = 0;
         stdr[2 * j].elapsed = stdr[2 * j + 1].elapsed = -1;
         stdr[2 * j].offset  = stdr[2 * j + 1].offset = 0; 
         continue;
      }

      startsec = jan1sec;
      for ( nintv = 0; nintv < 2; nintv++ ) {
         iter = 0;
         result = -1;
         restart = 1;
         while ( iter < 1000 && !iter_mgr(result, &delta, 30*86400L, &restart) ) {
            iter++;
            seconds = startsec + (time_t)delta;
            tms = localtime(&seconds);
            result = (tms->tm_isdst == startval) ? -1 : 1 ;
         }
         if ( iter > 999 ) {
            (void) fprintf(stderr, "convergence error in get_dst_info()\n");
            exit(1);
         }

         /* Store as elapsed minutes from 0:00 hours Jan 1 Standard Time at   */
         /* the start year, adjusted for changeover from daylight to standard */
         /* time and for changeover from standard to daylight time.           */

         sindx = 2 * j + nintv;

         lgls[sindx].elapsed = (long)(seconds - epoch)/60L + offset[indx];
         lgls[sindx].offset = offset[indx];
         if ( UNDEF_TIME == DST_TIME )
            stds[sindx].elapsed = lgls[sindx].elapsed;
         else 
            stds[sindx].elapsed = (long)(seconds - epoch)/60L + dstminutes;

         stds[sindx].offset = offset[indx];
         stdr[sindx].elapsed = (long)(seconds - epoch)/60L;
         stdr[sindx].offset = offset[indx]; 

         indx = (indx + 1) % 2;
         startval = (startval == val) ? 0 : val;
         startsec = seconds + (time_t)86400 ;

      }
   }

   configp->dstminutes = max(lgls[0].offset, lgls[1].offset);
   if ( lgls[0].elapsed > 0 ) {
      configp->isdst = 1;
   }

#if 0
   for (iter = 0; iter < NDSTINTV; iter++ )
      printf("lgls[%d].elapsed = %ld\n", iter, lgls[iter].elapsed);
#endif

   return sindx;
}

