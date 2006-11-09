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
** Generic interface handler
*/

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <dirent.h>

#include "../common/base.h"
#include "../common/ifhandlers.h"
#include "../common/leds.h"

#include "ifh_generic.h"

/* IFHANDLER structure required by the main program */
IFHANDLER ifh_generic =
{
	IFHANDLER_API_VER,				/* API version implemented by this interface handler */

	"generic interface handler",			/* Description of the interface handler */
	IFH_GENERIC_VERSION,				/* Version of the interface handler */

	"not supported",				/* Description text for this handler's tri-color LED support */

	ifh_generic_init,				/* Initialization function */

	ifh_generic_power,				/* Power control function */
	NULL,						/* Color control function */

	ifh_generic_errmsg				/* Returns interface handler-internal error messages */	
};

/* Buffer for error messages */
char _errmsg[MAX_ERRMSG_LEN];

/* Initialization function */
RC ifh_generic_init(LED *led)
{
	struct ifh_generic_pdata *pdata;
	char filenamebuf[PATH_MAX];

	/* Initialize error message buffer */
	*_errmsg = '\0';

	/* Allocate ifh_generic_pdata structure for this LED */
	pdata = malloc(sizeof(struct ifh_generic_pdata));
	if (!pdata)
	{
		snprintf(_errmsg, sizeof(_errmsg),
		         "ifh_generic_init(): Not enough memory for ifh_generic_pdata structure!\n");
		return ERR;
	}

	snprintf(filenamebuf, sizeof(filenamebuf), "%s%s", SYSFS_PREFIX, led->if_name);
	pdata->if_path = strdup(filenamebuf);

	snprintf(filenamebuf, sizeof(filenamebuf), "%s%s%s", SYSFS_PREFIX, led->if_name, SYSFS_RX_SUFFIX);
	pdata->rx_path = strdup(filenamebuf);

	snprintf(filenamebuf, sizeof(filenamebuf), "%s%s%s", SYSFS_PREFIX, led->if_name, SYSFS_TX_SUFFIX);
	pdata->tx_path = strdup(filenamebuf);

	/* Hook ifh_generic_pdata structure into LED structure */
	led->ifh_pdata = (void *)pdata;

	return OK;
}

/* Power control function */
RC ifh_generic_power(LED *led)
{
	struct ifh_generic_pdata *pdata;
	DIR *dir;

	/* Get pointer to ifh_generic_pdata structure */
	pdata = (struct ifh_generic_pdata *)led->ifh_pdata;

	/* Check whether interface is up (= sysfs path exists) */
	dir = opendir(pdata->if_path);
	if (dir)
	{
		FILE *file;
		char buf[SYSFS_BUFLEN];
		long int rx_packets, tx_packets;

		/* Fetch current rx_packets value */
		file = fopen(pdata->rx_path, "r");
		if (file)
		{
			fgets(buf, sizeof(buf), file);
			rx_packets = atol(buf);
			fclose(file);
		}
		else
		{
			snprintf(_errmsg, sizeof(_errmsg),
			         "Could not open \"%s\":\n%s\n",
				 pdata->rx_path, strerror(errno));
			return ERR;
		}

		/* Fetch current tx_packets value */
		file = fopen(pdata->tx_path, "r");
		if (file)
		{
			fgets(buf, sizeof(buf), file);
			tx_packets = atol(buf);
			fclose(file);
		}
		else
		{
			snprintf(_errmsg, sizeof(_errmsg),
			         "Could not open \"%s\":\n%s\n",
				 pdata->tx_path, strerror(errno));
			return ERR;
		}

		/* If the interface just went up (and during startup), turn on the LED */
		if (!led->up)
		{
			led->up = 1;
			led->on = 1;

			/* Store initial values */
			pdata->rx_packets = rx_packets;
			pdata->tx_packets = tx_packets;
		}
		/* Otherwise compare rx_packets and tx_packets values */
		else
		{
			/* Was there a change in any of the values? */
			if (rx_packets > pdata->rx_packets ||
			     tx_packets > pdata->tx_packets)
			{
				/* If yes, toggle the LED */
				led->on = 1 - led->on;

				/* And remember the new values */
				pdata->rx_packets = rx_packets;
				pdata->tx_packets = tx_packets;
			}
			/* Otherwise turn the LED back on */
			else
				led->on = 1;
		}

		closedir(dir);
	}
	else if (errno == ENOENT)
	{
		led->up = 0;
		led->on = 0;
	}
	else
	{
		snprintf(_errmsg, sizeof(_errmsg),
		         "Could not open \"%s\":\n%s\n",
			 pdata->if_path, strerror(errno));
		return ERR;
	}

	return OK;
}

/* Returns interface handler-internal error messages */
char *ifh_generic_errmsg(void)
{
        return _errmsg;
}

