/*
** rleds - Router LED control program
** Copyright (c) 2006 by Pieter Hollants <pieter@hollants.com>
**
** This program is licensed under the GNU General Public License, version 2,
** as published by the Free Software Foundation and available in the file
** COPYING and the Internet location http://www.gnu.org/licenses/gpl.html.
**
** This program is distributed in the hope that it will be useful, but
** WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
** or FITNESS FOR A PARTICULAR PURPOSE.
**
** Header file defining everything related to LED drivers
*/

#ifndef _RLEDS_LEDDRIVERS_H
#define _RLEDS_LEDDRIVERS_H

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include <stdlib.h>

#include "base.h"

/* Current version of the LED driver API */
#define LEDDRIVER_API_VER 1

/* Common filename prefix for LED drivers */
#define LEDDRIVER_PREFIX "leddrvr_"

/*
** A PIN structure defines a particular pin, existing at some port, which
** is controlled by a LED driver. More precisely, it associates a user-space
** exposed name with two integer values that are used within the LED driver
** exclusively, indicating which value is to be written into which register.
** The main program only uses the "allocated" flag.
*/
typedef struct _pin
{
	char		*name;				/* Name of the pin */
	BOOL		allocated;			/* Pin already allocated? */

	uint		reg,				/* Register (use your own #defines) */
			val;				/* Value to write into register (= 2^pin) */
} PIN;

/* Macro to initialize a PIN array */
#define INIT_PIN(n,r,v) { n, FALSE, r, v }

/* Macro to terminate a PIN array */
#define END_PINS { NULL, FALSE, 0, 0 }

/*
** Defining structure for LED drivers. These actually access the hardware by
** means of some device node and modify their registers as to enable and disable
** LEDs.
**
** For a driver for a "foo" port, the source files should be named leddrvrvr_foo.[ch],
** the resulting shared library object will be named leddrvr_foo.so and thus the
** LEDDRIVER structure defined must be named "leddrvr_foo".
**
** Note about the PIN definition: LED drivers should use an internal management
** structure to list the pins they make available and to track information as to which
** pins are actually allocated to LED structures. Name this structure PIN and return resp.
** accept it according to the callback definitions below.
*/
typedef struct _leddriver
{
	int		api_ver;			/* API version implemented by this LED driver.
							   (Always use LEDDRIVER_API_VER as defined above). */

	char		*desc,				/* Description for the LED driver */
			*ver;				/* Version of the LED driver */

	char		*def_dev;			/* Default device */
	PIN		*pins;				/* Array of pins controlled by this driver */

	/* Callback functions implemented by LED drivers */
	RC		(*init)(char *dev);		/* Init function. Called once to open and initialize
							   "dev". */

	void		(*enable)(PIN *pin);		/* Specifies a pin to be enabled by the commit() call. */
	RC		(*commit)(void);		/* Enables the pins specified by calls to set() before. */
	RC		(*reset)(void);			/* Reset (i.e. turn off all pins) */

	char		*(*errmsg)(void);		/* Returns driver-internal error messages */
} LEDDRIVER;

#endif /* _RLEDS_LEDDRIVERS_H */
