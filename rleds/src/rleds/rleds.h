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

/* Default LED device to use */
#define DEFAULT_LEDDEV "/dev/parport0"

/* Internal value used to signalize request for help with command line options */
#define OPT_HELP 1

/* The different kinds of interfaces supported (only relevant for dual-color LEDs) */
typedef enum _iftype
{
	IFTYPE_GENERIC,				/* Generic device (no special LED state support) */
	IFTYPE_ETHERNET,			/* Ethernet device (2nd pin used in signaling link speed) */
	IFTYPE_WLAN,				/* WLAN device (2nd pin used in signaling associations) */
	IFTYPE_PPP,				/* PPP device (2nd pin used in signaling connection estd.) */
	IFTYPE_VPN				/* VPN device (2nd pin used in signaling VPN state) */
} IFTYPE;

/* LED color states */
typedef enum _ledcolor
{
	LEDCOLOR_PRIM,				/* Single color LED or primary LED pin enabled */
	LEDCOLOR_SEC,				/* Secondary LED pin enabled */
	LEDCOLOR_BOTH				/* Both LED pins enabled */
} LEDCOLOR;

/* Mangement structure to keep track of LEDs and the associated interface */
typedef struct _led LED;
struct _led
{
	LED		*next;			/* Next LED in linked list */

	char		*if_name;		/* Interface belonging to this LED */
	IFTYPE		if_type;		/* Type of interface */

	char		prim_pin,		/* Number of primary LED pin */
			sec_pin;		/* Number of secondary LED pin (for dual-color LED, may be 0) */
	LEDCOLOR	color;			/* Current color of LED */
	char		on;			/* LED enabled (used for blinking)? */

	char		up,			/* Interface up? (= sysfs paths present?) */
			*if_path,		/* Path of sysfs dir for interface */
			*rx_path,		/* Path of sysfs file for rx_packets value */
			*tx_path;		/* Path of sysfs file for tx_packets value */
	long int 	rx_packets,		/* Last remembered rx_packets value */
			tx_packets;		/* Last remembered tx_packets value */
};

/* sysfs prefix to interface data (with trailing slash) */
#define SYSFS_PREFIX "/sys/class/net/"

/* sysfx suffixes to get number of received and number of transmitted packets (with heading slashes) */
#define SYSFS_RX_SUFFIX "/statistics/rx_packets"
#define SYSFS_TX_SUFFIX "/statistics/tx_packets"

/* Length of buffer for reads from sysfs files */
#define SYSFS_BUFLEN 20
