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
** Parallel port LED driver
*/

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/ioctl.h>
#include <linux/ppdev.h>

#include "../common/base.h"
#include "../common/leddrivers.h"

#include "leddrvr_parallel.h"

/* Pins supported by this driver */
char *_pinnames[] = {
	"Strobe",	"AutoFeed",	"Init",		"SelIn",
	"D0",		"D1",		"D2",		"D3",
	"D4",		"D5",		"D6",		"D7",
	NULL
};
REG _pinregs[] = {
	CONTROL_REG,	CONTROL_REG,	CONTROL_REG,	CONTROL_REG,
	DATA_REG,	DATA_REG,	DATA_REG,	DATA_REG,
	DATA_REG,	DATA_REG,	DATA_REG,	DATA_REG
};
uint _pinvals[] = {
	1,		2,		4,		8,
	1,		2,		4,		8,
	16,		32,		64,		128
};
#define NUM_PINS (sizeof(_pinnames)/sizeof(char *))-1

/* LEDDRIVER structure required by the main program */
LEDDRIVER leddrvr_parallel =
{
	LEDDRIVER_API_VER,				/* API version implemented by this LED driver */

	"drives LEDs attached to a parallel port",	/* Description for the LED driver */
	LEDDRVR_PARALLEL_VERSION,			/* Version of the LED driver */

	DEFAULT_DEVICE,					/* Default device */
	_pinnames,					/* Array of pins controlled by this driver */

	leddrvr_parallel_init,				/* Init function */
	leddrvr_parallel_shutdown,			/* Shutdown function */
	leddrvr_parallel_alloc,				/* Allocates a pin */
	leddrvr_parallel_enable,			/* Set pin to be enabled */
	leddrvr_parallel_commit,			/* Commit changes made by enable() to actual hardware */
	leddrvr_parallel_reset,				/* Resets all pins */
	leddrvr_parallel_errmsg				/* Returns driver-internal error messages */
};

/* Buffer for error messages */
char _errmsg[MAX_ERRMSG_LEN];

/* Initialization function */
PORT *leddrvr_parallel_init(char *dev_name)
{
	PORT *port;

	/* If no device name was specified, use the default */
	if (!dev_name)
		dev_name = DEFAULT_DEVICE;

	/* Initialize error message buffer */
	*_errmsg = '\0';

	/* Allocate PORT structure for this device */
	port = malloc(sizeof(PORT));
	if (port)
		port->allocated = calloc(NUM_PINS, sizeof(BOOL));
	if (!port || !port->allocated)
	{
		snprintf(_errmsg, sizeof(_errmsg),
		         "Not enough memory for PORT structure!\n");
		return NULL;
	}

	/* Open parallel port */
	port->fd = open(dev_name, O_RDWR);
	if (port->fd == -1)
	{
		if (errno == ENOENT)
		{
			snprintf(_errmsg, sizeof(_errmsg),
			         "Parallel port device \"%s\" does not exist -- check your system configuration!\n",
			         dev_name);
		}
		else
		{
			snprintf(_errmsg, sizeof(_errmsg),
			         "Could not open parallel port device \"%s\":\n%s!\n",
			         dev_name, strerror(errno));
		}

		return NULL;
	}

	/* Claim port */
	if (ioctl(port->fd, PPCLAIM))
	{
		snprintf(_errmsg, sizeof(_errmsg),
		         "Could not claim parallel port device \"%s\":\n%s!\n",
		         dev_name, strerror(errno));
		return NULL;
	}

	/* Remember device name for error messages */
	port->dev_name = strdup(dev_name);

	fprintf(stderr, "Will initialize %s (%d) (allocated=%p)\n", port->dev_name, port->fd, port->allocated);

	/* Finally, initialize it */
	if (leddrvr_parallel_reset(port) != OK)
	{
		/* Preserve error message */
		strncpy(_errmsg, port->errmsg, sizeof(_errmsg));

		(void)leddrvr_parallel_shutdown(port);
		return NULL;
	}

	return port;
}

/* Shutdown function */
RC leddrvr_parallel_shutdown(PORT *port)
{
	assert(port);

	/* Release port */
	if (ioctl(port->fd, PPRELEASE))
	{
		snprintf(_errmsg, sizeof(_errmsg),
		         "Could not release parallel port device \"%s\":\n%s!\n",
		         port->dev_name, strerror(errno));
		free(port);
		return ERR;
	}

	if (port->dev_name)
		free(port->dev_name);
	if (port->allocated)
		free(port->allocated);
	free(port);

	return OK;
}

/* Allocate the specified pin */
RC leddrvr_parallel_alloc(PORT *port, char *pin)
{
	char **p;
	int i;

	assert(port && port->allocated && pin);

	/* Lookup specified pin */
	for (i=0, p=_pinnames; i<NUM_PINS; i++, p++)
	{
		if (strcasecmp(*p, pin) == 0)
		{
			/* Found, now check if available */
			if (!port->allocated[i])
			{
				port->allocated[i] = TRUE;
				return OK;
			}

			/* Nope, already in use */
			snprintf(port->errmsg, sizeof(port->errmsg),
			         "Pin \"%s\" of device \"%s\" already in use -- specified twice?\n",
			         pin, port->dev_name);
			return ERR;
		}
	}

	/* Pin not found */
	snprintf(port->errmsg, sizeof(port->errmsg),
	         "The \"parallel\" LED driver does not know about a pin named \"%s\"!\n",
	         pin);
	return ERR;
}

/* Set pin to be enabled */
RC leddrvr_parallel_enable(PORT *port, char *pin)
{
	char **p;
	int i;

	assert(port && port->allocated && pin);

	/* Lookup specified pin */
	for (i=0, p=_pinnames; i<NUM_PINS; i++, p++)
	{
		if (strcasecmp(*p, pin) == 0)
		{
			/* Found, now check if not allocated */
			if (!port->allocated[i])
			{
				snprintf(port->errmsg, sizeof(port->errmsg),
				         "Pin \"%s\" of device \"%s\" was not allocated!\n",
				         pin, port->dev_name);
				return ERR;
			}

			/* It was, so add the its regval to reg */
			if (_pinregs[i] == CONTROL_REG)
				port->cval -= _pinvals[i];
			else
				port->dval += _pinvals[i];

			return OK;
		}
	}

	/* Pin not found */
	snprintf(port->errmsg, sizeof(port->errmsg),
	         "The \"parallel\" LED driver does not know about a pin named \"%s\"!\n",
	         pin);
	return ERR;
}

/* Commit changes made by calls to enable() to actual hardware */
RC leddrvr_parallel_commit(PORT *port)
{
	assert(port && port->fd);

	fprintf(stderr, "[%s] Writing to control: %d, to data: %d\n", port->dev_name, port->cval, port->dval);

	/* Write out values */
	if (ioctl(port->fd, PPWCONTROL, &port->cval) == -1 ||
	    ioctl(port->fd, PPWDATA, &port->dval)    == -1)
	{
		snprintf(port->errmsg, sizeof(port->errmsg),
		         "ioctl() on parallel port device \"%s\" failed:\n%s!\n",
			 port->dev_name, strerror(errno));
		return ERR;
	}

	/* Reset values */
	port->cval = CONTROL_INIT;
	port->dval = DATA_INIT;

	return OK;
}

/* Reset (i.e. turn off all pins) */
RC leddrvr_parallel_reset(PORT *port)
{
	assert(port && port->fd);

	/* Reset values... */
	port->cval = CONTROL_INIT;
	port->dval = DATA_INIT;

	fprintf(stderr, "[%s/%d] Reset cval to %d and dval to %d\n", port->dev_name, port->fd, port->cval, port->dval);

	/* ..and write out */
	if (ioctl(port->fd, PPWCONTROL, port->cval) == -1 ||
	    ioctl(port->fd, PPWDATA, port->dval)    == -1)
	{
		snprintf(port->errmsg, sizeof(port->errmsg),
		         "ioctl() on parallel port device \"%s\" failed:\n%s!\n",
			 port->dev_name, strerror(errno));
		return ERR;
	}

	return OK;
}

/* Returns LED driver-internal error messages */
char *leddrvr_parallel_errmsg(PORT *port)
{
	if (port)
		return port->errmsg;
	else
		return _errmsg;
}
