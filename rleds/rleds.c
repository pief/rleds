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
#include <signal.h>
#include <limits.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <sys/ioctl.h>
#include <linux/ppdev.h>

#include "rleds.h"

char _shutdown = 0;
int _ppdev;
LED *leds;

/* Turns off all LEDs */
static void clearLeds(void)
{
	int val = 0;
	ioctl(_ppdev, PPWDATA, &val);
}

/* Signal handler */
static void shutdown_handler(int sig)
{
	clearLeds();
	_shutdown = 1;
	signal(sig, shutdown_handler);
}

/* Main code */
int main(int argc, char **argv)
{
	char count, ppdev_specified = 0;
	char *ppdev_name;
	int i;

	/* Check command line arguments */
	count = argc-1;
	if (count && argv[1][0] == '/')
	{
		count--;
		ppdev_specified = 1;
	}
	if ( (count < 1) || (count > 8) )
	{
		fprintf(stderr, "Usage:\n");
		fprintf(stderr,"  %s [<parport device>] <interface0> [<interface1> ... <interface7>]\n\n", argv[0]);
		fprintf(stderr, "Controls LEDs connected on data lines 0-7 on <parport device> (default: /dev/parport0).\n");
		fprintf(stderr, "<interface0> maps to data line 0, <interface1> to data line 1 etc. A LED lights if the\n");
		fprintf(stderr, "associated interface is up and blinks when there is network activity on that interface.\n");
		exit(0);
	}

	/* Handle <parport device> argument */
	if (ppdev_specified)
		ppdev_name = strdup(argv[1]);
	else
		ppdev_name = DEFAULT_PPDEV;

	/* Open parallel port device */
	_ppdev = open(ppdev_name, O_RDWR);
	if (!_ppdev)
	{
		fprintf(stderr, "%s: could not open parallel port device \"%s\":\n%s!\n",
		        argv[0], ppdev_name, strerror(errno));
		exit(1);
	}

	/* Claim parallel port device */
	if (ioctl(_ppdev, PPCLAIM))
	{
		fprintf(stderr, "%s: could not claim parallel port device \"%s\":\n%s!\n",
		        argv[0], ppdev_name, strerror(errno));
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

	/* Create LED structures */
	leds = calloc(count, sizeof(LED));
	if (!leds)
	{
		fprintf(stderr, "%s: could not allocate memory for LED structures!\n", argv[0]);
		exit(1);
	}

	/* Set up LED structures */
	for (i=0; i<count; i++)
	{
		char filenamebuf[PATH_MAX], *iface;

		iface = argv[(ppdev_specified ? i+2 : i+1)];

		snprintf(filenamebuf, sizeof(filenamebuf), "%s%s", SYSFS_PREFIX, iface);
		leds[i].if_path = strdup(filenamebuf);

		snprintf(filenamebuf, sizeof(filenamebuf), "%s%s%s", SYSFS_PREFIX, iface, SYSFS_RX_SUFFIX);
		leds[i].rx_path = strdup(filenamebuf);

		snprintf(filenamebuf, sizeof(filenamebuf), "%s%s%s", SYSFS_PREFIX, iface, SYSFS_TX_SUFFIX);
		leds[i].tx_path = strdup(filenamebuf);
	}

	/* Loop until someone presses CTRL-C */
	while (!_shutdown)
	{
		int val, add;

		/* Process all interfaces watched */
		for (i=0, val=0, add=1; i<count; i++, add=add*2)
		{
			DIR *dir;

			/* Check whether interface is up (= sysfs path exists) */
			dir = opendir(leds[i].if_path);
			if (dir)
			{
				FILE *file;
				char buf[SYSFS_BUFLEN];
				long int rx_packets, tx_packets;

				/* Fetch current rx_packets value */
				file = fopen(leds[i].rx_path, "r");
				if (file)
				{
					fgets(buf, sizeof(buf), file);
					rx_packets = atol(buf);
					fclose(file);
				}
				else
				{
					fprintf(stderr, "%s: could not open \"%s\":\n%s\n",
					        argv[0], leds[i].rx_path, strerror(errno));
					continue;
				}

				/* Fetch current tx_packets value */
				file = fopen(leds[i].tx_path, "r");
				if (file)
				{
					fgets(buf, sizeof(buf), file);
					tx_packets = atol(buf);
					fclose(file);
				}
				else
				{
					fprintf(stderr, "%s: could not open \"%s\":\n%s\n",
					        argv[0], leds[i].tx_path, strerror(errno));
					continue;
				}

				/* If the interface just went up (or we're at startup), turn on the LED */
				if (!leds[i].up)
				{
					leds[i].up = 1;
					leds[i].on = 1;

					/* Read initial values */
					leds[i].rx_packets = rx_packets;
					leds[i].tx_packets = tx_packets;
				}
				/* Otherwise compare rx_packets and tx_packets values */
				else
				{
					/* Was there a change in any of the values? */
					if (rx_packets > leds[i].rx_packets ||
					    tx_packets > leds[i].tx_packets)
					{
						/* If yes, toggle the LED */
						leds[i].on = 1 - leds[i].on;

						/* And remember the new values */
						leds[i].rx_packets = rx_packets;
						leds[i].tx_packets = tx_packets;
					}
					/* Otherwise turn the LED back on */
					else
						leds[i].on = 1;
				}

				closedir(dir);
			}
			else if (errno == ENOENT)
			{
				leds[i].up = 0;
				leds[i].on = 0;
			}
			else
			{
				fprintf(stderr, "%s: could not open \"%s\":\n%s\n",
				        argv[0], leds[i].if_path, strerror(errno));
			}

			/* Compose value to be written to parallel port */
			if (leds[i].on)
				val += add;
		}

		/* Set LEDs */
		ioctl(_ppdev, PPWDATA, &val);

		/* Sleep for a while */
		usleep(25000);
	}

	return 0;
}
