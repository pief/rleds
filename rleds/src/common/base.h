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
** Header file with basic definitions
*/

#ifndef _RLEDS_BASE_H
#define _RLEDS_BASE_H

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

/*
** We stick to the usual programming convention (at least on unix) that a return
** code of zero indicates success and non-zero codes indicate failure. However, we
** wrap this convention around the definition of a "RC" type. It's just "nicer"
** and allows for stricter return type checking by the compiler.
*/
#undef RC
#undef ERR
#undef OK
typedef enum _rc { ERR=-1, OK=0 } RC;

/*
** Define a "true" boolean type for use outside of return codes.
*/
typedef enum _bool { FALSE, TRUE } BOOL;

#endif /* _RLEDS_BASE_H */

