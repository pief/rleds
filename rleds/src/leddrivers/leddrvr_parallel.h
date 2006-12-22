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
** Header file for parallel port LED driver
*/

#ifndef _RLEDS_DRVR_PARALLEL_H
#define _RLEDS_DRVR_PARALLEL_H

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include <linux/parport.h>

#include "../common/base.h"

/* Since drvr_parallel is part of the main rleds package, we use the same version
   number */
#define LEDDRVR_PARALLEL_VERSION PACKAGE_VERSION

/* Maximum length of buffer for error messages */
#define MAX_ERRMSG_LEN 100

/* Registers available in this LED driver */
typedef enum _reg  {
	CONTROL_REG,					/* Parallel port control register */
	DATA_REG					/* Parallel port data register */
} REG;

/* Default device */
#define DEFAULT_DEVICE "/dev/parport0"

/* Initialization values for control and data registers (all writeable
   control register pins are active low) */
const int CONTROL_INIT = PARPORT_CONTROL_STROBE |
                         PARPORT_CONTROL_AUTOFD |
                         PARPORT_CONTROL_INIT   |
                         PARPORT_CONTROL_SELECT;
const int DATA_INIT    = 0;

/* Our private PORT structure */
struct _port
{
	char		*dev_name;			/* Device name */
	int		fd;				/* The file descriptor for this port */

	int		cval,				/* Control register value to be written */
			dval;				/* Data register value to be written */

	BOOL		*allocated;			/* Tracks which pins of the port have been
							   allocated */

	char		errmsg[MAX_ERRMSG_LEN];		/* Error message */
};

/* Prototypes for the functions implemented in this LED driver */
PORT *leddrvr_parallel_init(char *dev_name);
RC leddrvr_parallel_shutdown(PORT *port);
RC leddrvr_parallel_alloc(PORT *port, char *pin);
RC leddrvr_parallel_enable(PORT *port, char *pin);
RC leddrvr_parallel_commit(PORT *port);
RC leddrvr_parallel_reset(PORT *port);
char *leddrvr_parallel_errmsg(PORT *port);

#endif
