
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


