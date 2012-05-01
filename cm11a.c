/*
 * Copyright 1996, 1997, 1998, 1999 by Daniel B. Suthers,
 * Pleasanton Ca. 94588 USA
 * E-MAIL dbs@tanj.com
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

/* This module has some functions unique to the CM11A interface.
 *
 */
 
#include <stdio.h>
#include "x10.h"

extern unsigned char cm11map[];
extern unsigned char map2cm11[];


/* This function will transform a 16 bit bitmap of devices in X10 format,
 * IE bit 6 is device 1 to a human readable map where bit 0 is device 1.
 * The map is bigendian, IE bit 0 is at the far right.
 * Unit 1 example:starting with 040 (1000000b)  will return 1 (0001b)
 * Unit 4 example:starting with 0400 (10000000000b)  will return 8 (1000b)
 */
unsigned int x2unitmap(xmap)
int xmap;
{
    unsigned int ret_map;
    int cntr;

    cntr = ret_map = 0;
    while( cntr < 16)
    {
        if( (xmap & ( 0x01 << cntr)) != 0 )
	    ret_map  |= (0x1 << (map2cm11[cntr] - 1));
		/* Note:  The ret_map is 0 relative and the map2cm11 array 
		 *        is 1 relative, so we must adjust by one.
		 */
	cntr++;
    }
    
return(ret_map);
}


/* b2s converts a 16 bit int to a 16 byte binary ascii string, msb on the left
 * It's used for visualizing the bits as they are displayed.
 * (Good for debugging)
 */
char *b2s(bin)
unsigned int bin;
{
    static char bite[17];
    int i, ii;

    i = 16;
    ii = 0;
    while( i > 0  )
    {
        bite[--i] = ((bin & (0x01 << ii++)) != 0 ) +'0' ;
    }
    bite[16] = '\0';

    return(bite);
}
