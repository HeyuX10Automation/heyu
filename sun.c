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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <stdio.h>
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#include <math.h>
#include <time.h>

#include "sun.h"
#include "process.h"

#define PI    3.14159265
#define D2R   (PI / 180.)

/* Ratio, Mean Solar Day / Mean Siderial Day */
#define MSSR   1.0027379

#define KS15   (15. * D2R * MSSR)

/* Local functions */
static void sun_position ( double, double *, double *);
static double local_sidereal_time (double, long int, double);

/*---------------------------------------------------------+
 | Calculate local times and Azimuths of Sunrise and       |
 | Sunset on a specified date.                             |
 | Input:                                                  |
 |   latitude and longitude in degrees are positive for    |
 |     North and East of Greenwich respectively.           |
 |   JulianDay is the Julian Day number at Greenwich Noon. |
 |   timezone in seconds from Greenwich, positive for      |
 |     localities west of Greenwich.                       |
 | Return:                                                 |
 |   sunrise and sunset times are in minutes after local   |
 |     midnight.                                           |
 |   azimuths at rise and set are in degrees (0 - 360).    |
 |   return code (defined in sun.h):                       |
 |     NORMAL_SUN    Sun rises and sets on this day.       |
 |     DOWN_ALL_DAY  Sun below horizon all day.            |
 |     UP_ALL_DAY    Sun above horizon all day.            |
 |     NO_SUNRISE    Sun does not rise on this day.        |
 |     NO_SUNSET     Sun does not set on this day.         |
 |                                                         |
 +---------------------------------------------------------*/

int suntimes ( double latitude, double longitude, long int timezone,
               long int JulianDay,
	       int sunmode, int offset,
               int *sunrise, int *sunset,
               double *azrise, double *azset)
{
   double    tdays, lst, zendist;
   double    slat, clat,cozend ;
   double    rasc[2], decl[2];
   double    hj ;
   int       flag_rise, flag_set;
   int       j ;
   int       retcode = NORMAL_SUN;
   int       *srssptr = NULL;
   double    *azptr = NULL;
   double    a, b, a0, a2, d0, d1, d2, dela, deld, p,
             el0, el2, h0, h1, h2, v0, v1, v2,
             d, e, t3, h7, n7, d7, az;

/* Zenith angles for Sunrise/set and Twilights */
#define ANG_RISESET  (90. + 50./60.)
#define ANG_CIVILTWI (96.)
#define ANG_NAUTITWI (102.)
#define ANG_ASTROTWI (108.)
#define ANG_ANGOFFS  (90. + offset/60.)

   double zenangle[] = {
      ANG_RISESET, ANG_CIVILTWI, ANG_NAUTITWI, ANG_ASTROTWI, ANG_ANGOFFS, };


   *sunrise = *sunset = 0 ;

   /* Elapsed days from 1 Jan 2000 at 00:00 hours UTC0 */
   tdays = (double)(JulianDay - 2451545L) - 0.5 ;

   /* Determine Local Siderial Time. */
   lst = local_sidereal_time( tdays, timezone, longitude );

   tdays += (double)timezone / (3600. * 24.);

   /* Get sun's position */
   for ( j = 0; j < 2; j++ ) {
      sun_position ( tdays, &rasc[j], &decl[j] );
      tdays += 1.0;
   }

   if ( rasc[1] < rasc[0] )
      rasc[1] += 2.*PI ;

#if 0   
   /* sunrise and sunset are defined when the sun is
      50 minutes below the horizon (at sea level).     */
   zendist = D2R * (90. + 50./60.) ;
#endif
   zendist = D2R * zenangle[sunmode];   

   slat = sin(D2R * latitude);
   clat = cos(D2R * latitude);
   cozend = cos(zendist);

   flag_rise = flag_set = 0;

   a0 = rasc[0];
   d0 = decl[0];
   v0 = 0.0;
   dela = rasc[1] - rasc[0];
   deld = decl[1] - decl[0];

   for ( j = 0; j < 24; j++ ) {
      hj = (double) j ;
      p = (1.0 + hj) / 24. ;

      a2 = rasc[0] + p * dela ;
      d2 = decl[0] + p * deld ;

      /* Test an hour for an event */
      el0 = lst + hj * KS15 ;
      el2 = el0 + KS15 ;
      h0 = el0 - a0;
      h2 = el2 - a2 ;
      h1 = 0.5 * (h2 + h0);
      d1 = 0.5 * (d2 + d0) ;

      if ( j == 0 ) {
         v0 = slat * sin(d0) + clat * cos(d0) * cos(h0) - cozend ;
      }
      v2 = slat * sin(d2) + clat * cos(d2) * cos(h2) - cozend ;
      if ( v0 * v2 < 0. ) {
         v1 = slat * sin(d1) + clat * cos(d1) * cos(h1) - cozend ;
         a = 2. * v2 - 4. * v1 + 2. * v0 ;
         b = 4. * v1 - 3. * v0 - v2 ;
         d = b * b - 4. * a * v0 ;
         if ( d >= 0. ) {
            d = sqrt(d);
            if ( v0 < 0. && v2 > 0. ) {
               /* Event is Sunrise */
               srssptr = sunrise ;
               azptr = azrise ;
               flag_rise = 1 ;
            }
            if ( v0 > 0. && v2 < 0. ) {
               /* Event is Sunset */
               srssptr = sunset ;
               azptr = azset ;
               flag_set = 1;
            }
            e = (-b + d) / (2. * a);
            if ( e > 1. || e < 0. )
               e = (-b -d) / (2. * a) ;
            t3 = hj + e ;
            *srssptr = (int)(60.* t3 + 0.5) ;  /* Round off*/

            h7 = h0 + e * (h2 -h0);
            n7 = - cos(d1) * sin(h7);
            d7 = clat * sin(d1) - slat * cos(d1) * cos(h7);

            az = fmod(((atan2(n7, d7) / D2R) + 360.), 360.) ;
            if ( azptr != NULL )
               *azptr = az ;
         }
      }
      if ( flag_rise & flag_set )
         break;
      a0 = a2;
      d0 = d2;
      v0 = v2;
   }
   if ( !(flag_rise | flag_set) ) {
      if ( v2 < 0. ) {
         /* Sun down all day */
         retcode = DOWN_ALL_DAY ;
      }
      else if ( v2 >= 0. ) {
         /* Sun up all day */
         retcode = UP_ALL_DAY ;
      }
   }
   else if ( !flag_rise ) {
      /* No sunrise this date */
      retcode = NO_SUNRISE ;
   }
   else if ( !flag_set ) {
      /* No sunset this date */
      retcode = NO_SUNSET ;
   }

   return retcode;
}

/*---------------------------------------------------------+
 | Calculate local sidereal time.                          |
 | Input:                                                  |
 |   tday - number of days from 2000 Jan 1 at              |
 |     00:00 hours at Greenwich (UTC0).                    |
 +---------------------------------------------------------*/

double local_sidereal_time ( double tday, long int timezone, double longitude )
{
   double s ;

   s = 24110.5 + 8640184.812999999*tday/36525. ;
   s += 86636.6*(double)timezone/(3600.*24.) + 86400.*longitude/360. ;
   s = s/86400. ;
   s = s - floor(s) ;

   return  (s * 360. * D2R) ;
}


/*---------------------------------------------------------+
 | Calculate sun's Right Ascention and Declination.        |
 +---------------------------------------------------------*/

void sun_position ( double tday, double *rasc, double *decl )
{
   double l, g, s, u, v, w ;
   double tcent ;

   /* Julian centuries from 1900.0 */
   tcent = tday / 36525. + 1.0 ;

   /* Fundamental arguments (Van Flandern & Pulkinnen, 1979) */

   l = .779072 + .00273790931 * tday ;
   g = .993126 + .0027377785 * tday ;
   l = 2. * PI * (l - floor(l)) ;
   g = 2. * PI * (g - floor(g)) ;

   v = .39785 * sin(l) - .01 * sin(l - g) +
       .00333 * sin(l + g) - .00021 * sin(l) * tcent ;
   u = 1.0 - .03349 * cos(g) - .00014 * cos(2. * l) +
       .00008 * cos (l) ;
   w = - .0001 - .04129 * sin(2. * l) +
       .03211 * sin(g) + .00104 * sin(2. * l - g) -
       .00035 * sin( 2. * l + g) - .00008 * sin(g) * tcent ;
   s = w / sqrt(u - v * v);
   *rasc = l + atan(s / sqrt(1.0 - s * s));
   s = v / sqrt(u);
   *decl = atan(s / sqrt(1.0 - s * s));

   return ;
}

/*---------------------------------------------------------+
 | Gregorian calendar day to Julian Day number.            |
 | Returns the Julian Day number at Greenwich Noon         |
 | (UTC0 12:00) for the year, month, and day arguments.    |
 |                                                         |
 | The Julian Day number is the count of whole days from   |
 | Noon on 1 Jan 4713 B.C.E. in the Julian Proleptic       |
 | Calendar.  This calculation is historically valid from  |
 | 15 Oct 1582 onward, however any year, month, and day    |
 | greater than zero are acceptable as arguments and will  |
 | yield the logically correct result.                     |
 +---------------------------------------------------------*/

long int greg2jd( int year, int month, int day )
{
   int count ;

   if ( month > 12 ) {
      year += ( month - 1 ) / 12 ;
      month = ( month - 1 ) % 12 + 1 ;
   }

   if ( month > 2 )
      count = - ( 4 * month + 23 ) / 10 ;
   else  {
      count = 365 ;
      year-- ;
   }

   count = count + year / 4
                 - 3 * ( year / 100 + 1 ) / 4
                 + 31 * ( month - 1 ) + day ;

   return ( (long)count + 365L*(long)year + 1721060L ) ;
}

/*---------------------------------------------------------------------+
 | Adjust the times of Dawn and Dusk for abnormal sun conditions, as   |
 | in polar regions, by creating and artificial Dawn and/or Dusk at    |
 | 00:01 or 23:58 as appropriate.                                      |
 +---------------------------------------------------------------------*/
int abnormal_sun_adjust ( int scode, int *dawnp, int *duskp )
{
   switch ( scode ) {
      case NORMAL_SUN :
         break;
      case UP_ALL_DAY :
         *dawnp = 1;
         *duskp = 23 * 60 + 58;
         break;
      case DOWN_ALL_DAY :
         *dawnp = 23 * 60 + 58;
         *duskp = 1;
         break;
      case NO_SUNRISE :
         *dawnp = 1;
         break;
      case NO_SUNSET :
         *duskp = 23 * 60 + 58;
         break;
      default :
         break;
   }
   return scode;
}

/*---------------------------------------------------------------------+
 | Return the Julian Day corresponding to UTC0 Noon for the UTC0 time  |
 | argument (in seconds from 1/1/1970 at 00:00:00 UTC0).               |
 +---------------------------------------------------------------------*/
long int utc2jd ( long lutc0 )
{
   extern CONFIG *configp;

   return (lutc0 - ((lutc0 - configp->tzone) % 86400L)) / 86400L + 2440588L;
}

/*---------------------------------------------------------------------+
 | Compute the UTC0 times of local Dawn and Dusk for the day which     |
 | includes the argument UTC0 time.  UTC0 times are all expressed as   |
 | seconds elapsed from 1/1/1970 at 00:00:00 UTC0.                     |
 +---------------------------------------------------------------------*/
int local_dawndusk( time_t utc0, time_t *utc0_dawn, time_t *utc0_dusk )
{
   long int jd;
   int      dawn, dusk, scode;
   long     lutc0, midnight;

   extern CONFIG   *configp;

   if ( configp->loc_flag != (LATITUDE | LONGITUDE) ) {
      *utc0_dawn = *utc0_dusk = 0;
      return -1;
   }
   
   lutc0 = (long)utc0;

   midnight = lutc0 - ((lutc0 - configp->tzone) % 86400L);

   jd = utc2jd(lutc0);

   scode = suntimes(configp->latitude, configp->longitude, configp->tzone, jd,
               configp->sunmode, configp->sunmode_offset, &dawn, &dusk, NULL, NULL);

   /* Adjust for abnormal sun conditions */
   abnormal_sun_adjust(scode, &dawn, &dusk);

   *utc0_dawn = (time_t)(midnight + 60L * (long)dawn);
   *utc0_dusk = (time_t)(midnight + 60L * (long)dusk);

   return scode;
}


/*-------------------------------------------------------------+
 | Display a table of Dawn and Dusk for the year in the        |
 | format used by the US Naval Observatory website at the time |
 | of this writing at:                                         |
 |    http://aa.usno.navy.mil/data/docs/RS_OneYear.html        |
 | (Standard time only is displayed.)                          |
 | Argument sunmode indicates the definition of Dawn and Dusk: |
 |   0  -> Rise and Set                                        |
 |   1  -> Civil Twilight                                      |
 |   2  -> Nautical Twilight                                   |
 |   3  -> Astronomical Twilight                               |
 | Printing on letter size or A4 paper requires 8 point fixed  |
 | font and landscape mode.                                    |
 +-------------------------------------------------------------*/
int display_sun_table_wide ( FILE *fd_sun, int year, long timezone,
    int sunmode, int offset, int timemode, int lat_d, int lat_m, int lon_d, int lon_m )
{
   static struct tzones {
      char *name;
      long seconds;
   } us_tzones[] = {
     {"Atlantic",         14400 },
     {"Eastern",          18000 },
     {"Central",          21600 },
     {"Mountain",         25200 },
     {"Pacific",          28800 },
     {"Alaska",           32400 },
     {"Hawaii-Aleutian",  36000 },
     {"Samoa",            39600 },
     {"Wake Island",     -43200 },
     {"Guam",            -39600 },
   };
   static int n_tzones = ( sizeof(us_tzones) / sizeof(struct tzones) );

   static int mdays[] = {0,31,28,31,30,31,30,31,31,30,31,30,31};
  
   int time_adjust ( int, int, unsigned char );
  
   long   julianday, julianday0;

   int    j, month, day, yday ;
   int    rise, set, scode;
   int    retcode;
   char   tmark = ' ';
   char   *timename;

   double latitude, longitude;

   get_dst_info(year);

   timename = ( timemode == TIMEMODE_CIVIL ) ? "Civil" : "Standard";

   latitude  = (lat_d < 0) ? (double)lat_d - (double)lat_m / 60.  :
                             (double)lat_d + (double)lat_m / 60. ;
   longitude = (lon_d < 0) ? (double)lon_d - (double)lon_m / 60.  :
                             (double)lon_d + (double)lon_m / 60. ;

   julianday0 = greg2jd(year, 1, 1);

   if ( greg2jd(year + 1, 1, 1) - julianday0 == 366 )
      mdays[2] = 29;
   else
      mdays[2] = 28;

   (void) fprintf(fd_sun, "\n              o  ,    o  ,  \n");
   (void) fprintf(fd_sun, "Location: ");
   (void) fprintf(fd_sun, "%s%03d %02d,",
        longitude < 0 ? "W" : "E", abs(lon_d), abs(lon_m));
   (void) fprintf(fd_sun, " %s%02d %02d",
        latitude  < 0 ? "S" : "N", abs(lat_d), abs(lat_m));

   switch (sunmode) {
      case RiseSet :	   
         (void) fprintf(fd_sun, "%*sSunrise and Sunset for %d", 29, " ", year);
         break;
      case CivilTwi :	   
         (void) fprintf(fd_sun, "%*sCivil Twilight for %d", 29, " ", year);
         break;
      case NautTwi :	   
         (void) fprintf(fd_sun, "%*sNautical Twilight for %d", 29, " ", year);
         break;
      case AstroTwi :	   
         (void) fprintf(fd_sun, "%*sAstronomical Twilight for %d", 26, " ", year);
         break;
      case AngleOffset :	   
         (void) fprintf(fd_sun, "%*sSun centre at %d angle minutes below horizon for %d", 26, " ", offset, year);
         break;
   };
   (void) fprintf(fd_sun, "%*sHEYU ver 2.0 \n\n", 39, " ");


   for ( j = 0; j < n_tzones; j++ ) {
      if ( us_tzones[j].seconds == timezone )
         break;
   }
   if ( j < n_tzones )
      (void) fprintf(fd_sun, "%*sUS/%s %s Time\n\n",
                                            54, " ", us_tzones[j].name, timename);
   else
      (void) fprintf(fd_sun, "%*sTimezone: %.1fh %s of Greenwich - %s Time\n\n", 51, " ",
            (double)abs(timezone)/3600., (timezone < 0 ? "East" : "West"), timename );

   (void) fprintf(fd_sun, "       Jan        Feb        Mar        Apr ");
   (void) fprintf(fd_sun, "       May        Jun        Jul        Aug ");
   (void) fprintf(fd_sun, "       Sep        Oct        Nov        Dec \n");
   (void) fprintf(fd_sun, "Day Rise  Set");

   for ( month = 2; month <= 12; month++ )
      (void) fprintf(fd_sun, "  Rise  Set");
   (void) fprintf(fd_sun, "\n  ");

   for ( month = 1; month <= 12; month++ )
      (void) fprintf(fd_sun, "   h m  h m");

   scode = NORMAL_SUN;
   for ( day = 1; day < 32; day++ ) {
      (void) fprintf(fd_sun, "\n%02d", day);
      for ( month = 1; month <= 12; month++ ) {
         if ( day > mdays[month] ) {
            (void) fprintf(fd_sun, "           ");
            continue;
         }
         julianday = greg2jd( year, month, day );

         retcode = suntimes(latitude, longitude, timezone, julianday,
                 sunmode, offset, &rise, &set, NULL, NULL );

         yday = (int)(julianday - julianday0);

         if ( timemode == TIMEMODE_CIVIL ) {
            /* Adjust times for Daylight Time */
            if ( retcode == NORMAL_SUN || retcode == NO_SUNSET ) {
               rise += time_adjust(yday, rise, LGL2STD);
               if ( rise < 0 || rise > 1439 )
                  retcode = NO_SUNRISE;
            }
            if ( retcode == NORMAL_SUN || retcode == NO_SUNRISE ) {
               set += time_adjust(yday, set, LGL2STD);
               if ( set < 0 || set > 1439 )
                  retcode = NO_SUNSET;
            }

            /* Mark day of time change */
            tmark = (yday == 0 ||
                  time_adjust(yday, 720, LGL2STD) == time_adjust(yday - 1, 720, LGL2STD)) ? ' ' : '*';
         }

         scode |= retcode;

         switch ( retcode ) {
            case DOWN_ALL_DAY :
               (void) fprintf(fd_sun, " %c---- ----", tmark);
               break ;
            case UP_ALL_DAY :
               (void) fprintf(fd_sun, " %c**** ****", tmark);
               break ;
            case NO_SUNRISE :
               (void) fprintf(fd_sun, " %c     %02d%02d", tmark, set/60, set % 60);
               break ;
            case NO_SUNSET :
               (void) fprintf(fd_sun, " %c%02d%02d     ", tmark, rise/60, rise%60);
               break ;
            default :
               (void) fprintf(fd_sun, " %c%02d%02d %02d%02d",
                            tmark, rise/60, rise%60, set/60,set%60 );
         }
      }
   }

   if ( timemode == TIMEMODE_CIVIL ) {
      fprintf(fd_sun, "\n\n(*) Denotes time change");
   }
   else {
      fprintf(fd_sun,
         "\n\n%*sAdd offset for Daylight Time (usually +60 minutes), if and when in effect.", 34, " ");
   }

   if ( scode & (DOWN_ALL_DAY | UP_ALL_DAY) ) {
      fprintf(fd_sun, "\n\n(****) Sun continuously above horizon");
      fprintf(fd_sun,  "%*s(----) Sun continuously below horizon", 60, " ");
   }

   fprintf(fd_sun, "\n");

   return 0;
}


/*-------------------------------------------------------------+
 | Display a table of Dawn and Dusk for the year.              |
 | Argument sunmode indicates the definition of Dawn and Dusk: |
 |   0  -> Rise and Set                                        |
 |   1  -> Civil Twilight                                      |
 |   2  -> Nautical Twilight                                   |
 |   3  -> Astronomical Twilight                               |
 +-------------------------------------------------------------*/
int display_sun_table ( FILE *fd_sun, int year, long timezone,
    int sunmode, int offset, int timemode, int lat_d, int lat_m, int lon_d, int lon_m )
{
   static struct tzones {
      char *name;
      long seconds;
   } us_tzones[] = {
     {"Atlantic",         14400 },
     {"Eastern",          18000 },
     {"Central",          21600 },
     {"Mountain",         25200 },
     {"Pacific",          28800 },
     {"Alaska",           32400 },
     {"Hawaii-Aleutian",  36000 },
     {"Samoa",            39600 },
     {"Wake Island",     -43200 },
     {"Guam",            -39600 },
   };
   static int n_tzones = ( sizeof(us_tzones) / sizeof(struct tzones) );

   static int mdays[] = {0,31,28,31,30,31,30,31,31,30,31,30,31};

   int time_adjust ( int, int, unsigned char );
  
   long   julianday, julianday0;

   int    j, month, day, yday, half ;
   int    rise, set, scode;
   int    retcode;
   char   tmark = ' ';
   char   minibuf[64];
   char   *timename;

   double latitude, longitude;

   get_dst_info(year);

   timename = ( timemode == TIMEMODE_CIVIL ) ? "Civil" : "Standard";

   latitude  = (lat_d < 0) ? (double)lat_d - (double)lat_m / 60.  :
                             (double)lat_d + (double)lat_m / 60. ;
   longitude = (lon_d < 0) ? (double)lon_d - (double)lon_m / 60.  :
                             (double)lon_d + (double)lon_m / 60. ;

   julianday0 = greg2jd(year, 1, 1);

   if ( greg2jd(year + 1, 1, 1) - julianday0 == 366 )
      mdays[2] = 29;
   else
      mdays[2] = 28;

   switch (sunmode) {
      case RiseSet :	   
         sprintf(minibuf, "Sunrise and Sunset for %d\n", year);
         break;
      case CivilTwi :	   
         sprintf(minibuf, "Civil Twilight for %d\n", year);
         break;
      case NautTwi :	   
         sprintf(minibuf, "Nautical Twilight for %d\n", year);
         break;
      case AstroTwi :	   
         sprintf(minibuf, "Astronomical Twilight for %d\n", year);
         break;
      case AngleOffset :	   
         sprintf(minibuf, "Sun centre at %d angle minutes below horizon for %d\n", offset, year);
         break;
   };
   fprintf(fd_sun, "%*s%s", (80 - (int)strlen(minibuf))/2, " ", minibuf);
   (void) fprintf(fd_sun, "\nLocation: ");
   (void) fprintf(fd_sun, "%s%03d:%02d,",
        longitude < 0 ? "W" : "E", abs(lon_d), abs(lon_m));
   (void) fprintf(fd_sun, " %s%02d:%02d",
        latitude  < 0 ? "S" : "N", abs(lat_d), abs(lat_m));

   for ( j = 0; j < n_tzones; j++ ) {
      if ( us_tzones[j].seconds == timezone )
         break;
   }
   if ( j < n_tzones )
      (void) fprintf(fd_sun, "   US/%s %s Time\n\n", us_tzones[j].name, timename);
   else
      (void) fprintf(fd_sun, "   Timezone: %.1fh %s of Greenwich - %s Time\n\n",
            (double)abs(timezone)/3600., (timezone < 0 ? "East" : "West"), timename );

   for ( half = 0; half < 2; half++ ) {

      if ( half == 0 )
         (void) fprintf(fd_sun, "       Jan          Feb          Mar          Apr          May          Jun\n");
      else
         (void) fprintf(fd_sun, "\n\n       Jul          Aug          Sep          Oct          Nov          Dec\n");

      if ( sunmode == 0 ) {
         fprintf(fd_sun, "Day Rise   Set");
         for ( month = 2; month <= 6; month++ )
            fprintf(fd_sun, "   Rise   Set");
         fprintf(fd_sun, "\n  ");
      }
      else {
         fprintf(fd_sun, "Day Morn   Eve");
         for ( month = 2; month <= 6; month++ )
            fprintf(fd_sun, "   Morn   Eve");
         fprintf(fd_sun, "\n  ");
      }

      for ( month = 1; month <= 6; month++ )
         (void) fprintf(fd_sun, "  hh:mm hh:mm");

      scode = NORMAL_SUN;
      for ( day = 1; day < 32; day++ ) {
         (void) fprintf(fd_sun, "\n%02d", day);
         for ( month = 6 * half + 1; month <= 6 * half + 6; month++ ) {
            if ( day > mdays[month] ) {
               (void) fprintf(fd_sun, "             ");
               continue;
            }
            julianday = greg2jd( year, month, day );

            yday = (int)(julianday - julianday0);

            retcode = suntimes(latitude, longitude, timezone, julianday,
                    sunmode, offset, &rise, &set, NULL, NULL );

            if ( timemode == TIMEMODE_CIVIL ) {
               /* Adjust times for Daylight Time */
               if ( retcode == NORMAL_SUN || retcode == NO_SUNSET ) {
                  rise += time_adjust(yday, rise, LGL2STD);
                  if ( rise < 0 || rise > 1439 )
                     retcode = NO_SUNRISE;
               }
               if ( retcode == NORMAL_SUN || retcode == NO_SUNRISE ) {
                  set += time_adjust(yday, set, LGL2STD);
                  if ( set < 0 || set > 1439 )
                     retcode = NO_SUNSET;
               }

               /* Mark day of time change */
               tmark = (yday == 0 ||
                   time_adjust(yday, 720, LGL2STD) == time_adjust(yday - 1, 720, LGL2STD)) ? ' ' : '*';
            }

            scode |= retcode;

            switch ( retcode ) {
               case DOWN_ALL_DAY :
                  (void) fprintf(fd_sun, " %c----- -----", tmark);
                  break ;
               case UP_ALL_DAY :
                  (void) fprintf(fd_sun, " %c***** *****", tmark);
                  break ;
               case NO_SUNRISE :
                  (void) fprintf(fd_sun, " %c      %02d:%02d", tmark, set/60, set % 60);
                  break ;
               case NO_SUNSET :
                  (void) fprintf(fd_sun, " %c%02d:%02d      ", tmark, rise/60, rise%60);
                  break ;
               default :
                  (void) fprintf(fd_sun, " %c%02d:%02d %02d:%02d",
                                 tmark, rise/60, rise%60, set/60,set%60 );
            }
         }
      }
   }

   if ( timemode == TIMEMODE_CIVIL ) {
      (void) fprintf(fd_sun, "\n\n(*) Denotes time change");
   }
   else {
      (void) fprintf(fd_sun, "\n\nAdd offset for Daylight Time (usually +60 minutes) if and when in effect.");
   }

   if ( scode & (DOWN_ALL_DAY | UP_ALL_DAY) ) {
      (void) fprintf(fd_sun, "\n\n(*****) Sun continuously above horizon");
      (void) fprintf(fd_sun,  "%*s(-----) Sun continuously below horizon", 4, " ");
   }

   (void) fprintf(fd_sun, "\n");


   return 0;
}
