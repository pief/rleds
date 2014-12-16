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
** Header file for generic network interface handler
*/

#ifndef NETIFH_GENERIC_H
#define NETIFH_GENERIC_H

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../common/base.h"
#include "../common/netifhandlers.h"

/* Since netifh_generic is part of the main rleds package, we use the same version
   number */
#define NETIFH_GENERIC_VERSION PACKAGE_VERSION

/* Maximum length of buffer for error messages */
#define MAX_ERRMSG_LEN 100

/* sysfs prefix to interface data (with trailing slash) */
#define SYSFS_PREFIX "/sys/class/net/"

/* sysfx suffixes to get number of received and number of transmitted packets (with heading slashes) */
#define SYSFS_RX_SUFFIX "/statistics/rx_packets"
#define SYSFS_TX_SUFFIX "/statistics/tx_packets"

/* Length of buffer for reads from sysfs files */
#define SYSFS_BUFLEN 20

/* Our private NETIF structure */
struct _netif
{
	char		*if_path;			/* Sysfs path for interface */
	BOOL		up;				/* Remember whether interface is/was up */

	char		*rx_path,			/* Sysfs path for rx_packets value */
			*tx_path;			/* Sysfs path for tx_packets value */
	long int 	rx_packets,			/* Last remembered rx_packets value */
			tx_packets;			/* Last remembered tx_packets value */

	char		errmsg[MAX_ERRMSG_LEN];		/* Error message */
};

/* Prototypes for the functions implemented in this interface handler */
NETIF *netifh_generic_init(char *if_name);
RC netifh_generic_shutdown(NETIF *netif);
RC netifh_generic_col(NETIF *netif, LEDSTATE *ledstate);
char *netifh_generic_errmsg(NETIF *netif);

#endif
