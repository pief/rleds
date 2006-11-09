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
** Header file defining everything related to LEDs
*/

#ifndef _RLEDS_LEDS_H
#define _RLEDS_LEDS_H

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include <stdlib.h>

#include "base.h"
#include "leddrivers.h"
#include "ifhandlers.h"

/* The different LED color states supported */
typedef enum _ledcolor
{
	LEDCOLOR_UNDEFINED,			/* Undefined state */
	LEDCOLOR_PRIM,				/* Primary LED pin enabled */
	LEDCOLOR_SEC,				/* Secondary LED pin enabled (only for tri-color LEDs) */
	LEDCOLOR_BOTH				/* Both LED pins enabled (only for tri-color LEDs) */
} LEDCOLOR;

/* Defining structure for LEDs. Associates interface handlers and LED drivers and their pins. */
struct _led
{
	char		*if_name;		/* Interface belonging to this LED */

	IFHANDLER	*ifh;			/* The associated interface handler */
	BOOL		up;			/* Interface up? (= sysfs paths present?) */
	void		*ifh_pdata,		/* Pointers to extra data used by interface handlers */
			*ifh_cdata;

	LEDDRIVER	*leddrvr;		/* The associated LED driver */
	PIN		*prim_pin,		/* Primary LED pin */
			*sec_pin;		/* Secondary LED pin (for tri-color LED, may be NULL) */
	BOOL		on;			/* Power enabled? */
	LEDCOLOR	color;			/* Current color */
};

#endif /* _RLEDS_LEDS_H */
