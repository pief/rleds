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

/*
** Since drvr_parallel is part of the main rleds package, we use the same version
** number
*/
#define LEDDRVR_PARALLEL_VERSION PACKAGE_VERSION

/* Valid values for the "register" field of our PIN structures */
#define CONTROL_REG	1
#define DATA_REG	2

/* Default device */
#define DEFAULT_DEVICE "/dev/parport0"

/* Maximum length of buffer for error messages */
#define MAX_ERRMSG_LEN 100

/*
** Initialization values for control and data registers (all writeable
** control register pins are active low)
*/
const int CONTROL_INIT = PARPORT_CONTROL_STROBE |
                         PARPORT_CONTROL_AUTOFD |
                         PARPORT_CONTROL_INIT   |
                         PARPORT_CONTROL_SELECT;
const int DATA_INIT    = 0;

/* Prototypes for the functions implemented in this LED driver */
RC leddrvr_parallel_init(char *devname);
void leddrvr_parallel_enable(PIN *pin);
RC leddrvr_parallel_commit(void);
RC leddrvr_parallel_reset(void);
char *leddrvr_parallel_errmsg(void);

#endif
