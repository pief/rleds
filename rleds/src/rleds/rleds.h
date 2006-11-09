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
** Header file for the main program
*/

/*
** Maximum length of buffer for error messages
*/
#define MAX_ERRMSG_LEN (PATH_MAX + 100)

/*
** Number of characters for indent in print_*() functions
*/
#define PRINT_INDENT 20

/*
** Name of the default interface handler
*/
#define DEFAULT_IFH "generic"

/*
** Number of microseconds to sleep between each call to the power control
** functions (ie. minimum time a LED will light resp. stay off)
*/
#define SLEEP_TIME 25000
