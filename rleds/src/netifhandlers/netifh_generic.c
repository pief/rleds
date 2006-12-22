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

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <dirent.h>

#include "../common/base.h"
#include "../common/netifhandlers.h"

#include "netifh_generic.h"

/* Buffer for global error messages */
char _errmsg[MAX_ERRMSG_LEN];

/* NETIFHANDLER structure required by the main program */
NETIFHANDLER netifh_generic =
{
	NETIFHANDLER_API_VER,				/* API version implemented by this interface handler */

	"generic interface handler",			/* Description of the interface handler */
	NETIFH_GENERIC_VERSION,				/* Version of the interface handler */

	"unsupported",					/* Description text for this handler's tri-color LED support */

	netifh_generic_init,				/* Initialization function */
	netifh_generic_shutdown,			/* Shutdown function */
	netifh_generic_col,				/* LED color function */
	netifh_generic_errmsg				/* Returns interface handler-internal error messages */	
};

/* Initialization function */
NETIF *netifh_generic_init(char *if_name)
{
	NETIF *netif;
	char filenamebuf[PATH_MAX];

	assert(if_name);

	/* Initialize error message buffer */
	*_errmsg = '\0';

	/* Allocate NETIF structure for this interface */
	netif = malloc(sizeof(NETIF));
	if (!netif)
	{
		snprintf(_errmsg, sizeof(_errmsg),
		         "Not enough memory for NETIF structure!\n");
		return NULL;
	}

	snprintf(filenamebuf, sizeof(filenamebuf), "%s%s", SYSFS_PREFIX, if_name);
	netif->if_path = strdup(filenamebuf);

	snprintf(filenamebuf, sizeof(filenamebuf), "%s%s%s", SYSFS_PREFIX, if_name, SYSFS_RX_SUFFIX);
	netif->rx_path = strdup(filenamebuf);

	snprintf(filenamebuf, sizeof(filenamebuf), "%s%s%s", SYSFS_PREFIX, if_name, SYSFS_TX_SUFFIX);
	netif->tx_path = strdup(filenamebuf);

	return netif;
}

/* Shutdown function */
RC netifh_generic_shutdown(NETIF *netif)
{
	assert(netif);

	free(netif);

	return OK;
}

/* LED color function */
RC netifh_generic_col(NETIF *netif, LEDSTATE *ledstate)
{
	DIR *dir;

	assert(netif && ledstate);

	/* Check whether interface is up (= sysfs path exists) */
	dir = opendir(netif->if_path);
	if (dir)
	{
		FILE *file;
		char buf[SYSFS_BUFLEN];
		long int rx_packets, tx_packets;

		/* Fetch current rx_packets value */
		file = fopen(netif->rx_path, "r");
		if (file)
		{
			fgets(buf, sizeof(buf), file);
			rx_packets = atol(buf);
			fclose(file);
		}
		else
		{
			snprintf(netif->errmsg, sizeof(netif->errmsg),
			         "Could not open \"%s\":\n%s\n",
				 netif->rx_path, strerror(errno));
			return ERR;
		}

		/* Fetch current tx_packets value */
		file = fopen(netif->tx_path, "r");
		if (file)
		{
			fgets(buf, sizeof(buf), file);
			tx_packets = atol(buf);
			fclose(file);
		}
		else
		{
			snprintf(netif->errmsg, sizeof(netif->errmsg),
			         "Could not open \"%s\":\n%s\n",
				 netif->tx_path, strerror(errno));
			return ERR;
		}

		/* If the interface just went up (and during startup), turn on the LED */
		if (!netif->up)
		{
			netif->up = 1;
			*ledstate = LEDSTATE_PRIM;

			/* Store initial values */
			netif->rx_packets = rx_packets;
			netif->tx_packets = tx_packets;
		}
		/* Otherwise compare rx_packets and tx_packets values */
		else
		{
			/* Was there a change in any of the values? */
			if (rx_packets > netif->rx_packets ||
			    tx_packets > netif->tx_packets)
			{
				/* If yes, toggle the LED */
				if (*ledstate == LEDSTATE_PRIM)
					*ledstate = LEDSTATE_OFF;
				else
					*ledstate = LEDSTATE_PRIM;

				/* And remember the new values */
				netif->rx_packets = rx_packets;
				netif->tx_packets = tx_packets;
			}
			/* Otherwise turn the LED back on */
			else
				*ledstate = LEDSTATE_PRIM;
		}

		closedir(dir);
	}
	else if (errno == ENOENT)
	{
		netif->up = 0;
		*ledstate = LEDSTATE_OFF;
	}
	else
	{
		snprintf(netif->errmsg, sizeof(netif->errmsg),
		         "Could not open \"%s\":\n%s\n",
			 netif->if_path, strerror(errno));
		return ERR;
	}

	return OK;
}

/* Returns interface handler-internal error messages */
char *netifh_generic_errmsg(NETIF *netif)
{
	if (netif)
		return netif->errmsg;

	return _errmsg;
}

