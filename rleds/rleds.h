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
*/

/* Default parport device to use */
#define DEFAULT_PPDEV "/dev/parport0"

/* Mangement structure to keep track of LEDs and the associated interface */
typedef struct _led
{
	char		*if_path,		/* Path of sysfile dir for interface */
			*rx_path,		/* Path of sysfs file for rx_packets value */
			*tx_path;		/* Path of sysfs file for tx_packets value */
	long int 	rx_packets,		/* Last remembered rx_packets value */
			tx_packets;		/* Last remembered tx_packets value */
	char		up,			/* Interface up? (= sysfs paths present?) */
			on;			/* LED on? */
} LED;

/* sysfs prefix to interface data (with trailing slash) */
#define SYSFS_PREFIX "/sys/class/net/"

/* sysfx suffixes to get number of received and number of transmitted packets (with heading slashes) */
#define SYSFS_RX_SUFFIX "/statistics/rx_packets"
#define SYSFS_TX_SUFFIX "/statistics/tx_packets"

/* Length of buffer for reads from sysfs files */
#define SYSFS_BUFLEN 20
