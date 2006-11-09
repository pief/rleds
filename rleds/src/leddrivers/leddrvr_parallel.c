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

/* Array of pins controlled by this driver */
PIN leddrvr_parallel_pins[] =
{
	INIT_PIN("Strobe",	CONTROL_REG,	  1),
	INIT_PIN("AutoFeed",	CONTROL_REG,	  2),
	INIT_PIN("Init",	CONTROL_REG,	  4),
	INIT_PIN("SelIn",	CONTROL_REG,	  8),
	INIT_PIN("D0",		DATA_REG,	  1),
	INIT_PIN("D1",		DATA_REG,	  2),
	INIT_PIN("D2",		DATA_REG,	  4),
	INIT_PIN("D3",		DATA_REG,	  8),
	INIT_PIN("D4",		DATA_REG,	 16),
	INIT_PIN("D5",		DATA_REG,	 32),
	INIT_PIN("D6",		DATA_REG,	 64),
	INIT_PIN("D7",		DATA_REG,	128),
	END_PINS
};

/* LEDDRIVER structure required by the main program */
LEDDRIVER leddrvr_parallel =
{
	LEDDRIVER_API_VER,				/* API version implemented by this LED driver */

	"drives LEDs attached to a parallel port",	/* Description for the LED driver */
	LEDDRVR_PARALLEL_VERSION,			/* Version of the LED driver */

	DEFAULT_DEVICE,					/* Default device */
	leddrvr_parallel_pins,				/* Array of pins controlled by this driver */

	leddrvr_parallel_init,				/* Init function */
	leddrvr_parallel_enable,			/* Specifies a pin to be enabled */
	leddrvr_parallel_commit,			/* Enable previously specified pins */
	leddrvr_parallel_reset,				/* Resets all pins */
	leddrvr_parallel_errmsg				/* Returns driver-internal error messages */
};

/* The parallel port device */
char *_parport_devname;
int _parport_fd;

/* Buffer for error messages */
char _errmsg[MAX_ERRMSG_LEN];

/* Storage for values to be written to control and data registers */
int _cval, _dval;

/* Init function */
RC leddrvr_parallel_init(char *devname)
{
	/* If no device name was specified, use the default */
	if (!devname)
		devname = DEFAULT_DEVICE;

	/* Open parallel port */
	_parport_fd = open(devname, O_RDWR);
	if (_parport_fd == -1)
	{
		if (errno == ENOENT)
		{
			snprintf(_errmsg, sizeof(_errmsg),
			         "Parallel port device \"%s\" does not exist -- check your system configuration!\n",
			         devname);
		}
		else
		{
			snprintf(_errmsg, sizeof(_errmsg),
			         "Could not open parallel port device \"%s\":\n%s!\n",
			         devname, strerror(errno));
		}

		return ERR;
	}

	/* Remember device name for error messages */
	_parport_devname = strdup(devname);

	/* Claim parallel port device */
	if (ioctl(_parport_fd, PPCLAIM))
	{
		snprintf(_errmsg, sizeof(_errmsg),
		         "Could not claim parallel port device \"%s\":\n%s!\n",
		         devname, strerror(errno));
		return ERR;
	}

	/* And initialize */
	return leddrvr_parallel_reset();
}

/* Set pins to be enabled */
void leddrvr_parallel_enable(PIN *pin)
{
	fprintf(stderr, "enable()\n");

	if (!pin)
		return;

	fprintf(stderr, "enabling pin %s\n", pin->name);

	switch (pin->reg)
	{
		case CONTROL_REG:
			_cval -= pin->val;
			fprintf(stderr, "cval - %d = %d\n", pin->val, _cval);
			break;
		case DATA_REG:
			_dval += pin->val;
			fprintf(stderr, "dval + %d = %d\n", pin->val, _dval);
			break;
		default:
			fprintf(stderr, "unknown pin->reg: %d\n", pin->reg);
	}
}

/* Enable previously specified pins  */
RC leddrvr_parallel_commit(void)
{
	fprintf(stderr, "Writing to control: %d, to data: %d\n", _cval, _dval);

	/* Write out values */
	if (ioctl(_parport_fd, PPWCONTROL, &_cval) == -1 ||
	    ioctl(_parport_fd, PPWDATA, &_dval)    == -1)
	{
		snprintf(_errmsg, sizeof(_errmsg),
		         "ioctl() on parallel port device \"%s\" failed:\n%s!\n",
			 _parport_devname, strerror(errno));
		return ERR;
	}

	/* Reset values */
	_cval = CONTROL_INIT;
	_dval = DATA_INIT;

	fprintf(stderr, "Reset cval to %d and dval to %d\n", _cval, _dval);

	return OK;
}

/* Reset (i.e. turn off all pins) */
RC leddrvr_parallel_reset(void)
{
	/* Reset values... */
	_cval = CONTROL_INIT;
	_dval = DATA_INIT;

	fprintf(stderr, "Reset cval to %d and dval to %d\n", _cval, _dval);

	/* ..and write out */
	if (ioctl(_parport_fd, PPWCONTROL, &_cval) == -1 ||
	    ioctl(_parport_fd, PPWDATA, &_dval)    == -1)
	{
		snprintf(_errmsg, sizeof(_errmsg),
		         "ioctl() on parallel port device \"%s\" failed:\n%s!\n",
			 _parport_devname, strerror(errno));
		return ERR;
	}

	return OK;
}

/* Returns LED driver-internal error messages */
char *leddrvr_parallel_errmsg(void)
{
	return _errmsg;
}
