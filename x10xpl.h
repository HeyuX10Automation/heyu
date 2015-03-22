/*
 * xPL interface for Heyu
 *
 * Copyright (C) 2010, Janusz Krzysztofik <jkrzyszt@tis.icnet.pl>
 *
 *
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

#ifndef __X10XPL_H
#define __X10XPL_H

#ifdef HAVE_XPL_H
#include <xPL.h>
#endif

/*
 * xpl_init(): initialize the xPL layer and set up xPL service(s)
 * 	       based on the Heyu daemon specific i_am_* flags.
 * return_value: TRUE: success, FALSE: failure.
 */
Bool xpl_init(void);

#endif /* __X10XPL_H */
