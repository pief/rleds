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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <limits.h>
#include <ctype.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <sys/ioctl.h>
#include <getopt.h>
#include <linux/ppdev.h>

#include "rleds.h"

const char *_prgbanner =
        "%s %s - Router LED control program\n"
        "Copyright (c) 2006 by Pieter Hollants <pieter@hollants.com>\n\n";

char _shutdown = 0;

char *_leddev_name = DEFAULT_LEDDEV;
int _leddev;

LED *_first_led = NULL, *_last_led = NULL;

const char *_short_opts = "d:e:g:p:v:Vw:";
static struct option _long_opts[] =
{
	{ "device",	required_argument,	NULL,	'd' },
	{ "ethernet",	required_argument,	NULL,	'e' },
	{ "generic",	required_argument,	NULL,	'g' },
	{ "help",	no_argument,		NULL,	OPT_HELP },
	{ "ppp",	required_argument,	NULL,	'p' },
	{ "usage",	no_argument,		NULL,	OPT_HELP },
	{ "version",	no_argument,		NULL,	'V' },
	{ "vpn",	required_argument,	NULL,	'v' },
	{ "wlan",	required_argument,	NULL,	'w' },
	{ 0, 0, 0, 0 }
};

const char *_help =
        "This program is licensed under the GNU General Public License, version 2.\n"
        "See the file COPYING or visit http://www.gnu.org/licenses/gpl.html for details.\n\n"

        "Usage: %s <options>\n\n"

        "At least one of the following options is required:\n"
        "  -e, --ethernet=IFSPEC     watch Ethernet interface (dual: link speed)\n"
	"  -g, --generic=IFSPEC      watch generic interface (dual: unused)\n"
	"  -p, --ppp=IFSPEC          watch PPP interface (dual: connection status)\n"
	"  -v, --vpn=IFSPEC          watch VPN interface (dual: tunnel status)\n"
	"  -w, --wlan=IFSPEC         watch WLAN device (dual: AP associations)\n\n"

	"IFSPEC specifies an interface and a LED's wiring as follows:\n"
	"  <interface>:<pin>         single-color LED connected at <pin> (min 1, max 8)\n"
	"  <interface>:<prim>,<sec>  dual-color LED connected at pins <prim>, <sec>\n"
	"Dual-color LEDs visualize besides traffic also information as shown above.\n\n"

        "Additional options:\n"
        "  -d, --device              set parport device that drives the LEDs\n"
	"                            (default: %s)\n"
        "  -V, --version             print version and exit\n";

/* Splits up an IFSPEC. */
static int split_ifspec(char *spec, char **if_name, int *prim_pin, int *sec_pin)
{
	char *p = spec;

	/* Interface name */
	*if_name = strsep(&p, ":");
	if (!if_name || !p)
		return 1;

	/* Primary pin argument */
	if (!isdigit(*p))
		return 1;
	*prim_pin = atoi(p);
	while (isdigit(*p))
		p++;
	if (*p == ',')
	{
		*p++='\0';

		/* Secondary pin argument */
		if (!isdigit(*p))
			return 1;
		*sec_pin = atoi(p);
		while (isdigit(*p))
			p++;
	}
	if (*p != '\0')
		return 1;

	return 0;
}

/* Sets up a new LED structure. */
static LED *new_led(char *if_name, IFTYPE if_type, int prim_pin, int sec_pin)
{
	LED *led;
	char filenamebuf[PATH_MAX];

	/* Allocate new LED structure... */
	led = malloc(sizeof(LED));
	if (!led)
	{
		fprintf(stderr, "Could not allocate memory for LED structure!\n");
		return NULL;
	}

	/* ...and initialize it */
	led->if_name = strdup(if_name);
	led->if_type = if_type;

	if ((prim_pin < 1) || (prim_pin > 8))
	{
		fprintf(stderr, "Invalid primary pin specified for interface \"%s\"!\n"
		                "Please specify a value between 1 and 8.\n",
		        if_name);
		return NULL;
	}
	led->prim_pin = (char)prim_pin;

	if (if_type == IFTYPE_GENERIC)
	{
		if (sec_pin)
		{
			fprintf(stderr, "Secondary pin specified for generic interface \"%s\"!\n"
			                "(Generic interfaces do not support dual-color LEDs.)\n",
				if_name);
			return NULL;
		}
	}
	else if (sec_pin)
	{
		if ((sec_pin < 1) || (sec_pin > 8))
		{
			fprintf(stderr, "Invalid secondary pin specified for interface \"%s\"!\n"
			                "Please specify a value between 1 and 8 (but not %d).\n",
			        if_name, prim_pin);
			return NULL;
		}
		if (sec_pin == prim_pin)
		{
			fprintf(stderr, "Interface \"%s\"'s secondary pin can't be same as primary pin (%d)!\n",
			        if_name, prim_pin);
			return NULL;
		}
	}
	led->sec_pin = (char)sec_pin;

	led->color = LEDCOLOR_PRIM;
	led->on = 0;
	led->up = 0;

	snprintf(filenamebuf, sizeof(filenamebuf), "%s%s", SYSFS_PREFIX, if_name);
	led->if_path = strdup(filenamebuf);

	snprintf(filenamebuf, sizeof(filenamebuf), "%s%s%s", SYSFS_PREFIX, if_name, SYSFS_RX_SUFFIX);
	led->rx_path = strdup(filenamebuf);

	snprintf(filenamebuf, sizeof(filenamebuf), "%s%s%s", SYSFS_PREFIX, if_name, SYSFS_TX_SUFFIX);
	led->tx_path = strdup(filenamebuf);

	led->rx_packets = led->tx_packets = 0;

	/* Append new LED structure to list of LEDs */
	if (!_last_led)
		_first_led = _last_led = led;
	else
		_last_led = _last_led->next = led;

	return led;
}

/* Turns off all LEDs */
static void clearLeds(void)
{
	int val = 0;
	ioctl(_leddev, PPWDATA, &val);
}

/* Signal handler */
static void shutdown_handler(int sig)
{
	clearLeds();
	_shutdown = 1;
	signal(sig, shutdown_handler);
}

/* Initialization routine. In case of errors we call exit() directly. */
static void init(int argc, char **argv)
{
	int c, opt_idx = 0;

	while (1)
	{
		char *if_name;
		IFTYPE if_type;
		int prim_pin, sec_pin;

		c = getopt_long(argc, argv, _short_opts, _long_opts, &opt_idx);
		if (c < 0)
			break;

		if (c == 'e')
			if_type = IFTYPE_ETHERNET;
		else if (c == 'p')
			if_type = IFTYPE_PPP;
		else if (c == 'v')
			if_type = IFTYPE_VPN;
		else if (c == 'w')
			if_type = IFTYPE_WLAN;
		else
			if_type = IFTYPE_GENERIC;

		switch (c)
		{
			/* -d, --device */
			case 'd':
			{
				_leddev_name = strdup(optarg);
				break;
			}
			/* -e, --ethernet */
			/* -g, --generic */
			/* -p, --ppp */
			/* -v, --vpn */
			/* -w, --wlan */
			case 'e':
			case 'g':
			case 'p':
			case 'v':
			case 'w':
			{
				if (split_ifspec(optarg, &if_name, &prim_pin, &sec_pin))
				{
					fprintf(stderr, "Invalid IFSPEC \"%s\" specified with option \"-%c\"!\n",
					        optarg, c);
					exit(1);
				}
				if (!new_led(if_name, if_type, prim_pin, sec_pin))
					exit(1);
				break;
			}
			/* -V, --version */
			case 'V':
			{
				printf(_prgbanner, PACKAGE_NAME, PACKAGE_VERSION);
				printf("Compiled on %s %s\n", __DATE__, __TIME__);
				exit(0);
			}
			/* --help, --usage */
			case OPT_HELP:
			{
				printf(_prgbanner, PACKAGE_NAME, PACKAGE_VERSION);
				printf(_help, argv[0], DEFAULT_LEDDEV);
				exit(0);
			}
			/* Unknown option */
			case '?':
			{
				/* getopt_long already printed an error message */
				fprintf(stderr, "Try \"%s --help\" or \"%s --usage\" for more information.\n",
				        argv[0], argv[0]);
				exit(1);
			}
		}

	}

	/* Check that at least one LED/interface definition was given */
	if (!_last_led)
	{
		fprintf(stderr, "At least one of the \"-e\", \"-g\", \"-p\", \"-v\" or \"-w\" options must be specified!\n");
		fprintf(stderr,"Try \"%s --help\" or \"%s --usage\" for more information.\n",
		        argv[0], argv[0]);
		exit(1);
	}

	/* Open LED device */
	_leddev = open(_leddev_name, O_RDWR);
	if (!_leddev)
	{
		fprintf(stderr, "Could not open parallel port device \"%s\":\n%s!\n",
		        _leddev_name, strerror(errno));
		exit(1);
	}

	/* Claim parallel port device */
	if (ioctl(_leddev, PPCLAIM))
	{
		fprintf(stderr, "Could not claim parallel port device \"%s\":\n%s!\n",
		        _leddev_name, strerror(errno));
		exit(1);		
	}

	/* Clear LEDs and install signal handler */
	clearLeds();
	signal(SIGHUP, shutdown_handler);
	signal(SIGINT, shutdown_handler);
	signal(SIGABRT, shutdown_handler);
	signal(SIGTERM, shutdown_handler);
	signal(SIGSEGV, shutdown_handler);
	signal(SIGBUS, shutdown_handler);
	signal(SIGUSR1, shutdown_handler);
	signal(SIGUSR2, shutdown_handler);
}

/* Main code */
int main(int argc, char **argv)
{
	/* Initialize */
	init(argc, argv);

	/* Loop until someone presses CTRL-C */
	while (!_shutdown)
	{
		LED *led;
		int val = 0;

		/* Process all LEDs/interfaces watched */
		for (led = _first_led; led; led = led->next)
		{
			DIR *dir;

			/* Check whether interface is up (= sysfs path exists) */
			dir = opendir(led->if_path);
			if (dir)
			{
				FILE *file;
				char buf[SYSFS_BUFLEN];
				long int rx_packets, tx_packets;

				/* Fetch current rx_packets value */
				file = fopen(led->rx_path, "r");
				if (file)
				{
					fgets(buf, sizeof(buf), file);
					rx_packets = atol(buf);
					fclose(file);
				}
				else
				{
					fprintf(stderr, "Could not open \"%s\":\n%s\n",
					        led->rx_path, strerror(errno));
					continue;
				}

				/* Fetch current tx_packets value */
				file = fopen(led->tx_path, "r");
				if (file)
				{
					fgets(buf, sizeof(buf), file);
					tx_packets = atol(buf);
					fclose(file);
				}
				else
				{
					fprintf(stderr, "Could not open \"%s\":\n%s\n",
					        led->tx_path, strerror(errno));
					continue;
				}

				/* If the interface just went up (or we're at startup), turn on the LED */
				if (!led->up)
				{
					led->up = 1;
					led->on = 1;

					/* Read initial values */
					led->rx_packets = rx_packets;
					led->tx_packets = tx_packets;
				}
				/* Otherwise compare rx_packets and tx_packets values */
				else
				{
					/* Was there a change in any of the values? */
					if (rx_packets > led->rx_packets ||
					    tx_packets > led->tx_packets)
					{
						/* If yes, toggle the LED */
						led->on = 1 - led->on;

						/* And remember the new values */
						led->rx_packets = rx_packets;
						led->tx_packets = tx_packets;
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
				fprintf(stderr, "Could not open \"%s\":\n%s\n",
				        led->if_path, strerror(errno));
			}

			/* Compose value to be written to parallel port */
			if (led->on)
				val += (1 << (led->prim_pin - 1));
		}

		/* Set LEDs */
		ioctl(_leddev, PPWDATA, &val);

		/* Sleep for a while */
		usleep(25000);
	}

	return 0;
}
