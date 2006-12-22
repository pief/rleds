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
** Header file defining everything related to network interface handlers
*/

#ifndef _RLEDS_NETIFHANDLERS_H
#define _RLEDS_NETIFHANDLERS_H

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "base.h"

/* Current version of the network interface handler API */
#define NETIFHANDLER_API_VER 1

/* Common filename prefix for network interface handlers */
#define NETIFHANDLER_PREFIX "netifh_"

/*
** Defines for communication between network interface handlers and the main program as to
** which pins of a LED should be enabled.
*/
typedef enum _ledstate
{
	LEDSTATE_OFF,					/* LED is turned off */
	LEDSTATE_PRIM,					/* Primary LED pin is turned on */
	LEDSTATE_SEC,					/* Secondary LED pin is turned on */
	LEDSTATE_BOTH					/* Both LED pins are turned on */
} LEDSTATE;

/*
** Network interface handlers usually need to keep state information about the network
** interfaces they watch. For this purpose they must declare a NETIF structure, whose actual
** declaration is done in the network interface handler's header file, and is accessed in the
** main program only as a handle. The typedef, however, must be done here since it's
** referenced in the callback definitions below.
*/
typedef struct _netif NETIF;

/*
** Defining structure for network interface handlers. These do whatever is necessary
** to determine the state of an interface.
**
** For an interface handler for "foo" devices, the source files should be named
** netifh_foo.[ch], the resulting shared library object will be named netifh_foo.so and thus
** the NETIFHANDLER structure defined must be named "netifh_foo".
*/
typedef struct _netifhandler
{
	/* == Read-only information about the network interface handler == */

	int		api_ver;			/* API version implemented by this handler.
							   (Always use NETIFHANDLER_API_VER as #defined above). */

	char 		*desc,				/* Description of the handler */
	     		*ver;				/* Version of the handler */

	char		*tricol_desc;			/* Description text for this handler's tri-color LED
							   support (see included handlers for examples) */

	/* == Callback functions == */

	/*
	** Init function.
	**
	** "if_name" is the name of the interface for which the network interface handler is to be
	** initialized.
	**
	** Returns a NETIF handle on success or NULL on failure.
	*/
	NETIF		*(*init)(char *if_name);

	/*
	** Shutdown function. 
	**
	** "netif" is a NETIF handle as obtained by a call to this network interface handler's init()
	** function.
	**
	** Returns OK on success and ERR on failure, in which case the program's error code should
	** be set appropriately. 
	*/
	RC		(*shutdown)(NETIF *netif);

	/*
	** LED color function.
	**
	** This is the main function of every interface handler: to do whatever is necessary with
	** the interface to determine a color state to return.
	**
	** "netif" is a NETIF handle as obtained by a call to this network interface handler's init()
	** function. "ledstate" is a pointer to a LEDSTATE variable that is to hold the desired state
	** of the LED.
	**
	** Returns OK on success and ERR if errors occured.
	*/
	RC		(*col)(NETIF *netif, LEDSTATE *ledstate);

	/*
	** Returns network interface handler-internal error messages.
	**
	** "netif" is a NETIF handle as obtained by a call to this network interface handler's init()
	** function. If "netif" is NULL, global error messages will be returned (e.g. failure to
	** allocate memory for an NETIF structure).
	**
	** Returns the last error message associated with the specified NETIF handle.
	*/
	char		*(*errmsg)(NETIF *netif);
} NETIFHANDLER;

#endif /* _RLEDS_NETIFHANDLERS_H */

