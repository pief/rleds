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
** LED drivers usually need to keep state information about the ports they access.
** For this purpose they must declare a PORT structure, whose actual declaration is done
** in the LED driver's header file, and is accessed in the main program only as a handle
** (PORT = port handle). The typedef, however, must be done here since it's referenced in
** the callback definitions below.
*/
typedef struct _port PORT;

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
	/* == Read-only information about the LED driver == */

	int		api_ver;			/* API version implemented by this LED driver.
							   (Always use LEDDRIVER_API_VER as defined above). */

	char		*desc,				/* Description for the LED driver */
			*ver;				/* Version of the LED driver */

	char		*def_dev;			/* Default device */
	char		**pins;				/* Array of pin names controlled by this driver */

	/* == Callback functions == */

	/*
	** Initialization function.
	**
	** "dev" is the port to open (may be NULL, then "def_dev" will be used).
	**
	** Returns a PORT handle on success or NULL on failure.
	**/
	PORT		*(*init)(char *dev);

	/*
	** Shutdown function.
	**
	** "port" is a PORT handle as obtained by a call to this LED driver's init() function.
	**
	** Returns OK on success and ERR on failure, in which case the program's error code should
	** be set appropriately.
	*/
	RC		(*shutdown)(PORT *port);

	/*
	** Requests the LED driver to allocate the specified pin in the given PORT instance (ie.
	** mark it as in use).
	**
	** "port" is a PORT handle as obtained by a call to this LED driver's init() function.
	** "pin" is one of the pins listed in the LED driver's "pins" array.
	**
	** Returns OK on success and ERR on failure.
	*/
	RC		(*alloc)(PORT *port, char *pin);

	/*
	** Specifies a pin to be enabled by an upcoming commit() call.
	**
	** "port" is a PORT handle as obtained by a call to this LED driver's init() function.
	** "pin" is one of the pins listed in the LED driver's "pins" array which additionally
	** must have been allocated beforehand using the alloc() function above.
	**
	** Returns OK on success and ERR on failure.
	*/
	RC		(*enable)(PORT *port, char *pin);

	/*
	** Commits the changes made by calls to enable() before to the actual hardware.
	**
	** "port" is a PORT handle as obtained by a call to this LED driver's init() function.
	**
	** Returns OK on success and ERR on failure.
	*/
	RC		(*commit)(PORT *port);

	/*
	** Reset (i.e. turn off all pins).
	**
	** "port" is a PORT handle as obtained by a call to this LED driver's init() function.
	**
	** Returns OK on success and ERR on failure.
	*/
	RC		(*reset)(PORT *port);

	/*
	** Returns driver-internal error messages.
	**
	** "port" is a PORT handle as obtained by a call to this LED driver's init() function.
	**
	** Returns the last error message associated with the specified PORT handle.
	*/
	char		*(*errmsg)(PORT *port);
} LEDDRIVER;

#endif /* _RLEDS_LEDDRIVERS_H */
