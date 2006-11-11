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
** Header file for generic interface handler
*/

#ifndef IFH_GENERIC_H
#define IFH_GENERIC_H

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../common/base.h"
#include "../common/ifhandlers.h"
#include "../common/leds.h"

/* Since ifh_generic is part of the main rleds package, we use the same version
   number */
#define IFH_GENERIC_VERSION PACKAGE_VERSION

/* Prototypes for the functions implemented in this interface handler */
RC ifh_generic_init(LED *led);
RC ifh_generic_power(LED *led);
char *ifh_generic_errmsg(void);

/* Maximum length of buffer for error messages */
#define MAX_ERRMSG_LEN 100

/* sysfs prefix to interface data (with trailing slash) */
#define SYSFS_PREFIX "/sys/class/net/"

/* sysfx suffixes to get number of received and number of transmitted packets (with heading slashes) */
#define SYSFS_RX_SUFFIX "/statistics/rx_packets"
#define SYSFS_TX_SUFFIX "/statistics/tx_packets"

/* Length of buffer for reads from sysfs files */
#define SYSFS_BUFLEN 20

/* Our own management structure, hooked in at led->ifh_pdata */
struct ifh_generic_pdata
{
	char		*if_path,		/* Path of sysfs dir for interface */
			*rx_path,		/* Path of sysfs file for rx_packets value */
			*tx_path;		/* Path of sysfs file for tx_packets value */
	long int 	rx_packets,		/* Last remembered rx_packets value */
			tx_packets;		/* Last remembered tx_packets value */
};

#endif
