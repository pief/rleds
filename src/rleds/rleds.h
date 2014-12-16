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
** Header file for the main program
*/

#ifndef _RLEDS_H
#define _RLEDS_H

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../common/base.h"
#include "../common/netifhandlers.h"
#include "../common/leddrivers.h"

/* Maximum length of buffer for error messages */
#define MAX_ERRMSG_LEN (PATH_MAX + 100)

/* Number of characters for indent in print_*() functions */
#define PRINT_INDENT 20

/* Name of the default network interface handler */
#define DEFAULT_NETIFH "generic"

/* Number of microseconds to sleep between each call to the power control
 functions (ie. minimum time a LED will light resp. stay off) */
#define SLEEP_TIME 25000

/*
** Management structure to keep tracks of the configured LEDs. Associates
** interface handlers and LED drivers.
*/
typedef struct _led
{
	char		*netif_name;		/* Network interface name */
	NETIFHANDLER	*netifh;		/* Associated handler */
	NETIF		*netif;			/* Associated NETIF handle */
	LEDSTATE	ledstate;		/* LED state */

	char		*device_name;		/* Device name */
	LEDDRIVER	*leddrvr;		/* Associated LED driver */
	PORT		*port;			/* Associated PORT handle */
	char		*prim_pin,		/* Primary LED pin */
			*sec_pin;		/* Secondary LED pin (may be NULL) */
} LED;

/* Function prototypes */
void *load_shobj(char *path);
LEDDRIVER *load_leddriver(char *dir, char *leddriver_name);
NETIFHANDLER *load_netifhandler(char *dir, char *netifhandler_name);
RC list_shobjs(char *dir,
               char *name,
               char *struct_name,
               int (*filter_func)(const struct dirent *),
               void (*print_func)(char *, void *));
int filter_leddrivers(const struct dirent *dirent);
void print_leddriver(char *name, void *ifstruct);
int filter_netifhandlers(const struct dirent *dirent);
void print_netifhandler(char *name, void *ifstruct);
RC split_ledspec(char *spec,
                 char **if_name,
                 char **ifh_name,
                 char **leddrvr_name,
                 char **device,
                 char **prim_pin,
                 char **sec_pin);
void init(int argc, char **argv);
void shutdown(void);
void sig_handler(int sig);

#endif /* _RLEDS_H */
