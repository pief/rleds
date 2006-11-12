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
** Header file defining everything related to interface handlers
*/

#ifndef _RLEDS_IFHANDLERS_H
#define _RLEDS_IFHANDLERS_H

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "base.h"

/* Current version of the interface handler API */
#define IFHANDLER_API_VER 1

/* Common filename prefix for interface handlers */
#define IFHANDLER_PREFIX "ifh_"

/*
** Since we can't include leds.h here (leds.h includes ifhandlers.h itself), we
** employ a forward declaration of the "LED" structure.
*/
typedef struct _led LED;

/*
** Defining structure for interface handlers. These do whatever is necessary
** to determine the state of an interface.
**
** For an interface handler for "foo" devices, the source files should be named
** ifh_foo.[ch], the resulting shared library object will be named ifh_foo.so and thus
** the IFHANDLER structure defined must be named "ifh_foo".
*/
typedef struct _ifhandler
{
	int		api_ver;			/* API version implemented by this interface handler.
							   (Always use IFHANDLER_API_VER as #defined above). */

	char 		*desc,				/* Description of the interface handler */
	     		*ver;				/* Version of the interface handler */

	char		*tricol_desc;			/* Description text for this handler's tri-color LED
							   support (see included handlers for examples) */

	/* Callback functions implemented by interface handlers */
	RC		(*init)(LED *led);		/* Init function.

							   May use led->ifh_pdata and led->ifh_cdata. */

	RC		(*powerctl)(LED *led);		/* Power control function.

							   Called every iteration of the main loop, this function
							   decides whether the LED should be turned on or off (by writing
							   to led->on). It is also responsible for any blinking. */
	RC		(*colorctl)(LED *led);		/* Color control function.

							   Called every iteration of the main loop except for a
							   blocking period of a few cycles after color changes, this
							   function determines the color of the LED (by writing to
							   led->color).

							   This callback is optional and will only be called for
							   a particular LED if a secondary pin was configured. */

	char		*(*errmsg)(void);		/* Returns interface handler-internal error messages */
} IFHANDLER;

#endif /* _RLEDS_IFHANDLERS_H */
