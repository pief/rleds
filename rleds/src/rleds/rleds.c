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
** Main program
*/

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <limits.h>
#include <signal.h>
#include <fcntl.h>
#include <dirent.h>
#include <libgen.h>
#include <errno.h>
#include <getopt.h>
#include <dlfcn.h>

#include "../common/base.h"
#include "../common/leddrivers.h"
#include "../common/ifhandlers.h"
#include "../common/leds.h"

#include "rleds.h"

const char *_prgbanner =
        "%s - Router LED control program\n"
        "Copyright (c) 2006 by Pieter Hollants <pieter@hollants.com>\n\n";

/* Set by our signal handler */
char _shutdown = 0;

/* Global error message variables */
char _errmsg[MAX_ERRMSG_LEN];

/* Command line arguments */
const char *_short_opts = "liV";
static struct option _long_opts[] =
{
	{ "led-drivers",	no_argument,		NULL,	'l' },
	{ "interface-handlers",	no_argument,		NULL,	'i' },
	{ "help",		no_argument,		NULL,	'h' },
	{ "usage",		no_argument,		NULL,	'h' },
	{ "version",		no_argument,		NULL,	'V' },
	{ NULL,			0,			NULL,	0 }
};

const char *_help =
        "This program is licensed under the GNU General Public License, version 2.\n"
        "See the file COPYING or visit http://www.gnu.org/licenses/gpl.html for details.\n\n"

        "Usage: %s <options>\n"
	"       %s <LEDSPEC1> [<LEDSPEC2> ...]\n\n"

        "Options:\n"
	"  -l, --led-drivers         list available LED drivers and their pin names\n"
	"  -i, --interface-handlers  list available interface handlers\n"
        "  -V, --version             print version and exit\n\n"

	"<LEDSPEC> is a string of the format\n"
	" <ifname>['['<interface handler>']']:<led driver>['['<device>']']:<prim>[,<sec>]\n"
	"where <prim> and optionally <sec> define the pins of the LED driver at which the\n"
	"(tri-color) LED for <ifname> is connected.\n\n"

	"Examples:\n"
	" eth0:parallel:2 ppp0[ppp]:parallel[/dev/parport1]:3,4 eth3:serial[/dev/tty5]:1\n";

/* LEDs */
LED *_leds;
uint _num_leds;

/*
** obj = load_shobj(path, ifstruct_name)
**
** Loads a shared object implementing some functionality and returns a pointer to its
** interface structure.
**
** "path" is the complete path to the shared object to be opened.
**
** "path" is also used to construct a "canonical name" for a structure which is supposed
** to exist inside the shared object and which sort of defines the interface to this object
** (ie. the interface structure). This canonical name is created by removing the directory part
** and the suffix. For example, "/usr/lib/rleds/ifh_generic.so" becomes "ifh_generic".
**
** Returns a pointer to the object's interface structure, NULL if the required structure could
** not be found and -1 on error, in which case an error message can be found in _errmsg.
*/
void *load_shobj(char *path)
{
	void *dlobj, *ifstruct;
	char *ifstruct_name, *p;
	char *errmsg;

	/* Attempt to open the specified file as a dynamic library */
	dlobj = dlopen(path, RTLD_LAZY);
	if (!dlobj)
	{
		snprintf(_errmsg, sizeof(_errmsg),
		         "Could not dlopen() \"%s\":\n%s!\n",
		         path, dlerror());
		return (void *)-1;
	}

	/* Create the structure name based on the shared object name */
	ifstruct_name = basename(strdup(path));
	p = strstr(ifstruct_name, ".so");
	if (p)
		*p = '\0';

	/* Attempt to locate defining structure */
	dlerror();
	ifstruct = dlsym(dlobj, ifstruct_name);
	errmsg = dlerror();
	if (errmsg)
	{
		snprintf(_errmsg, sizeof(_errmsg),
		         "dlsym() on \"%s\" for \"%s\" failed: %s\n",
		         path, ifstruct_name, errmsg);
		return (void *)-1;
	}

	return ifstruct;
}

/*
** leddrvr = load_leddriver(dir, leddriver_name)
**
** Attempts to load a LED driver in "dir" by its canonical name, e.g. "parallel"
** instead of "/foo/bar/drvr_parallel.so".
**
** Returns a pointer to the led driver's LEDDRIVER structure or NULL on error, in
** which case an error message can be found in _errmsg.
*/
LEDDRIVER *load_leddriver(char *dir, char *leddriver_name)
{
	char *path;
	size_t len;
	LEDDRIVER *leddrvr;

	/* Compose full path */
	len = strlen(dir) + 1 + strlen(LEDDRIVER_PREFIX) + strlen(leddriver_name) + 4;
	path = malloc(len);
	if (!path)
	{
		snprintf(_errmsg, sizeof(_errmsg),
		         "Not enough memory for complete path to LED driver \"%s\"!\n",
		         leddriver_name);
		return NULL;
	}
	snprintf(path, len, "%s/%s%s.so", dir, LEDDRIVER_PREFIX, leddriver_name);

	/* Attemt to load as shared object */
	leddrvr = (LEDDRIVER *)load_shobj(path);
	if (leddrvr == (void *)-1)
		return NULL;
	if (!leddrvr)
	{
		snprintf(_errmsg, sizeof(_errmsg),
		         "\"%s\" misses the defining LEDDRIVER structure!\n",
		         leddriver_name);
		return NULL;
	}

	return leddrvr;
}

/*
** ifh = load_ifhandler(dir, ifhandler_name)
**
** Attempts to load an interface handler in "dir" by its canonical name, e.g. "generic"
** instead of "/foo/bar/ifh_generic.so".
**
** Returns a pointer to the interface handler's IFHANDLER structure or NULL on error, in
** which case an error message can be found in _errmsg.
*/
IFHANDLER *load_ifhandler(char *dir, char *ifhandler_name)
{
	char *path;
	size_t len;
	IFHANDLER *ifh;

	/* Compose full path */
	len = strlen(dir) + 1 + strlen(IFHANDLER_PREFIX) + strlen(ifhandler_name) + 4;
	path = malloc(len);
	if (!path)
	{
		snprintf(_errmsg, sizeof(_errmsg),
		         "Not enough memory for complete path to interface handler \"%s\"!\n",
		         ifhandler_name);
		return NULL;
	}
	snprintf(path, len, "%s/%s%s.so", dir, IFHANDLER_PREFIX, ifhandler_name);

	/* Attemt to load as shared object */
	ifh = (IFHANDLER *)load_shobj(path);
	if (ifh == (void *)-1)
		return NULL;
	if (!ifh)
	{
		snprintf(_errmsg, sizeof(_errmsg),
		         "\"%s\" misses the defining IFHANDLER structure!\n",
		         ifhandler_name);
		return NULL;
	}

	return ifh;
}

/*
** rc = list_shobjs(dir, name, struct_name, filter_func, print_func)
**
** Lists all available shared objects in the specified directory of a specific type.
** "name" is a representative name for this type of objects in the plural form,
** "struct_name" is the name of the interface structure type that the shared object
** must export, "filter_func" is a callback function passed to scandir() and "print_func" is a
** function that is called to print the details of the shared object.
**
** This function is not meant to be called directly. Instead use macros such as
** list_leddrivers() and list_ifhandlers() below.
*/
RC list_shobjs(char *dir,
               char *name,
               char *struct_name,
               int (*filter_func)(const struct dirent *),
               void (*print_func)(char *, void *))
{
	int i, count;
	struct dirent **dirents;

	/* Scan the directory */
	count = scandir(dir, &dirents, filter_func, alphasort);
	if (count < 0)
	{
		fprintf(stderr,
		        "Could not scandir() \"%s\":\n%s!\n",
		        dir, strerror(errno));
		return ERR;
	}
	if (count == 0)
	{
		fprintf(stdout,
		        "No %s installed in \"%s\" -- incomplete installation?\n",
		        name, dir);
		return ERR;
	}

	fprintf(stdout, _prgbanner, PACKAGE_STRING);

	fprintf(stdout, "Available %s:\n", name);

	/* Process files found */
	for (i = 0; i < count; i++)
	{
		void *ifstruct;
		char *path;
		int len;

		/* Compose full path */
		len = strlen(dir) + strlen(dirents[i]->d_name) + 2;
		path = malloc(len);
		if (!path)
		{
			fprintf(stderr,
			        "Out of memory composing path for \"%s\"!\n",
			        dirents[i]->d_name);
			return ERR;
		}
		snprintf(path, len, "%s/%s", dir, dirents[i]->d_name);

		/* Attemt to load as shared object */
		ifstruct = load_shobj(path);
		if (ifstruct == (void *)-1)
		{
			fputs(_errmsg, stderr);
			return ERR;
		}
		if (!ifstruct)
		{
			fprintf(stderr,
			        "\"%s\": missing %s structure\n",
			        dirents[i]->d_name, struct_name);
			continue;
		}

		print_func(dirents[i]->d_name, ifstruct);
	}

	return OK;
}

/*
** Filter function for scandir() employed in list_shobjs() above. Referenced
** by the list_leddrivers() macro below.
**
** Returns 1 for filenames that begin with the prefix "leddrvr_" and 0 for other
** filenames.
*/
int filter_leddrivers(const struct dirent *dirent)
{
	if (!strncmp(dirent->d_name, LEDDRIVER_PREFIX, strlen(LEDDRIVER_PREFIX)))
		return 1;
	else
		return 0;
}

/*
** Print function for list_shobjs() above. Referenced by the list_leddrivers()
** macro below.
*/
void print_leddriver(char *name,
                     void *ifstruct)
{
	LEDDRIVER *leddrvr = (LEDDRIVER *)ifstruct;
	char *leddrvr_name, *p;
	PIN *pin;
	char buf[PRINT_INDENT];

	/* Compare API versions */
	if (leddrvr->api_ver != LEDDRIVER_API_VER)
	{
		fprintf(stderr,
		        "\"%s\": wrong API version (%d != ours: %d)\n",
		        name, leddrvr->api_ver, LEDDRIVER_API_VER);
		return;
	}

	/* Create short name so the user knows what to specify in LEDSPECs */
	leddrvr_name = strdup(name) + strlen(LEDDRIVER_PREFIX);
	p = strstr(leddrvr_name, ".so");
	if (p)
		*p = '\0';

	/* Print out information */
	snprintf(buf, PRINT_INDENT,
	         "- %s (v%s) ",
	         leddrvr_name, leddrvr->ver);

	fprintf(stdout,
	        "%-*s%s\n",
	        PRINT_INDENT, buf, leddrvr->desc);

	fprintf(stdout,
	        "%*cDefault device: \"%s\"\n",
	        PRINT_INDENT, ' ', leddrvr->def_dev);

	fprintf(stdout,
	        "%*cPins:",
	        PRINT_INDENT, ' ');
	for (pin = leddrvr->pins; pin->name; pin++)
	{
		fprintf(stdout, " %s", pin->name);
	}
	fprintf(stdout, "\n");
}

/*
** rc = list_leddrivers(dir)
**
** Lists all available LED drivers in PACKAGE_LIBDIR (shared library objects named
** leddrvr_<name>.so and containing an LEDDRIVER structure named leddriver_<name>),
** among with their help texts.
**
** Since this actually is a #define, 
*/
#define list_leddrivers(dir) \
 list_shobjs(dir, "LED drivers", "LEDDRIVER", filter_leddrivers, print_leddriver)

/*
** Filter function for scandir() employed in list_interfacehandlers() below.
**
** Returns 1 for filenames that begin with the prefix "ifh_" and 0 for other
** filenames.
*/
int filter_ifhandlers(const struct dirent *dirent)
{
	if (!strncmp(dirent->d_name, IFHANDLER_PREFIX, strlen(IFHANDLER_PREFIX)))
		return 1;
	else
		return 0;
}

/*
** Print function for list_shobjs() above. Referenced by the list_ifhandlers()
** macro below.
*/
void print_ifhandler(char *name,
                     void *ifstruct)
{
	IFHANDLER *ifh = (IFHANDLER *)ifstruct;
	char *ifh_name, *p;
	char buf[PRINT_INDENT];

	/* Compare API versions */
	if (ifh->api_ver != IFHANDLER_API_VER)
	{
		fprintf(stderr,
		        "\"%s\": wrong API version (%d != ours: %d)\n",
		        name, ifh->api_ver, IFHANDLER_API_VER);
		return;
	}

	/* Create short name so the user knows what to specify in LEDSPECs */
	ifh_name = strdup(name) + strlen(IFHANDLER_PREFIX);
	p = strstr(ifh_name, ".so");
	if (p)
		*p = '\0';

	/* Print out information */
	snprintf(buf, PRINT_INDENT,
	         "- %s (v%s) ",
	         ifh_name, ifh->ver);

	fprintf(stdout,
	        "%-*s%s\n",
	        PRINT_INDENT, buf, ifh->desc);

	fprintf(stdout,
	        "%*cTri-color LEDs: %s\n",
	        PRINT_INDENT, ' ', ifh->tricol_desc);
}

/*
** rc = list_ifhandlers(dir)
**
** Lists all available interface handlers in PACKAGE_LIBDIR (shared library objects
** named ifh_<name>.so and containing an IFHANDLER structure named ifh_<name>) together
** with their help texts.
*/
#define list_ifhandlers(dir) \
 list_shobjs(dir, "interface handlers", "IFHANDLER", filter_ifhandlers, print_ifhandler)

/*
** rc = split_ledspec(spec, &if_name, &ifh_name, &leddrvr_name, &device, &prim_pin, &sec_pin);
**
** Splits up an LED specification in the format
**  <interface name>['('<interface handler>')']:<led driver>['('<device>')']:<prim>[,<sec>]
** returning the components in the supplied pointers.
**
** Returns OK on success and ERR on failure.
*/
RC split_ledspec(char *spec,
                 char **if_name,
                 char **ifh_name,
                 char **leddrvr_name,
                 char **device,
                 char **prim_pin,
                 char **sec_pin)
{
	char *p;

	/* Sanity check */
	if (!spec || !if_name || !ifh_name || !leddrvr_name || !device || !prim_pin || !sec_pin)
		return ERR;

	/* Chop spec using the double colon */
	p = strdup(spec);
	*if_name = strsep(&p, ":");
	*leddrvr_name = strsep(&p, ":");
	*prim_pin = strsep(&p, ":");
	if (p || !*leddrvr_name || !*prim_pin)
	{
		return ERR;
	}

	/* Then process the smaller pieces */
	p = *if_name;
	*if_name = strsep(&p, "[");
	if (p)
	{
		*ifh_name = strsep(&p, "]");
		if (!p)
			return ERR;
	}
	else
		*ifh_name = NULL;


	p = *leddrvr_name;
	*leddrvr_name = strsep(&p, "[");
	if (p)
	{
		*device = strsep(&p, "]");
		if (!p)
			return ERR;
	}
	else
		*device = NULL;

	*sec_pin = *prim_pin;
	*prim_pin = strsep(sec_pin, ",");
	if (*sec_pin){
		if (strpbrk(*sec_pin, ":[],"))
			return ERR;
	}
	else
		*sec_pin = NULL;

	return OK;
}

/*
** pin = alloc_pin(leddrvr, pin_name)
**
** Searches the specified LED driver's "pins" array for the pin "pin_name".
**
** Returns a pointer to the PIN structure on success or NULL on failure, in
** which case an error message can be found in _errmsg.
*/
PIN *alloc_pin(LEDDRIVER *leddrvr, char *pin_name)
{
	PIN *pin;

	for (pin = leddrvr->pins; pin->name; pin++)
	{
		if (!strcasecmp(pin_name, pin->name))
		{
			if (pin->allocated)
			{
				snprintf(_errmsg, sizeof(_errmsg),
				         "pin \"%s\" specified twice",
				         pin_name);
				return NULL;
			}

			pin->allocated = TRUE;
			return pin;				
		}
	}

	snprintf(_errmsg, sizeof(_errmsg),
	         "no such pin \"%s\"",
	         pin_name);
	return NULL;
}

/*
** clear_leds()
**
** Instructs all LED drivers to clear their pins.
**
** Returns OK on success and ERR on failure.
*/
static RC clear_leds(void)
{
	int i;

	for (i = 0; i < _num_leds; i++)
	{
		if (_leds[i].leddrvr->reset() != OK)
		{
			strncpy(_errmsg, _leds[i].leddrvr->errmsg(), sizeof(_errmsg)-1);
			return ERR;
		}
	}

	return OK;
}

/*
** shutdown_handler(sig)
**
** Signal handler for all signals. Turns off all LEDs and instructs the main
** routine to shut down.
*/
static void shutdown_handler(int sig)
{
	/* At this place we can safely ignore the return code */
	(void)clear_leds();

	_shutdown = 1;
	signal(sig, shutdown_handler);
}

/*
** init(argc, argv);
**
** Initialization routine.
**
** exit() is used here instead of return codes since we not only decide on whether
** to exit or not but also on the return code.
*/
static void init(int argc, char **argv)
{
	int c, opt_idx = 0, i;

	/* Process command line options */
	while (1)
	{
		c = getopt_long(argc, argv, _short_opts, _long_opts, &opt_idx);
		if (c < 0)
			break;

		switch (c)
		{
			/* -l, --led-drivers */
			case 'l':
			{
				if (list_leddrivers(PACKAGE_LIBDIR) == OK)
					exit(0);
				else
					exit(1);
			}
			/* -i, --interface-handlers */
			case 'i':
			{
				if (list_ifhandlers(PACKAGE_LIBDIR) == OK)
					exit(1);
				else
					exit(0);
			}
			/* -V, --version */
			case 'V':
			{
				printf(_prgbanner, PACKAGE_STRING);
				printf("Compiled on %s %s\n", __DATE__, __TIME__);
				exit(0);
			}
			/* --help, --usage */
			case 'h':
			{
				printf(_prgbanner, PACKAGE_NAME, PACKAGE_VERSION);
				printf(_help, argv[0], argv[0]);
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

	/* Check that at least one LED structure was created */
	_num_leds = argc-optind;
	if (_num_leds == 0)
	{
		fprintf(stderr, "No LED specifications given on command line!\n");
		fprintf(stderr, "Try \"%s --help\" or \"%s --usage\" for more information.\n",
		        argv[0], argv[0]);
		exit(1);
	}

	/* The remaining arguments are assumed to be LED specifications, so we allocate
	   memory for LED structures */
	_leds = calloc(_num_leds, sizeof(LED));
	if (!_leds)
	{
		fprintf(stderr, "Could not allocate memory for LED structures!\n");
		exit(1);
	}

	/* Process LED specifications */
	for (i=0; optind < argc ; optind++, i++)
	{
		char *if_name, *ifh_name, *leddrvr_name, *device, *prim_pin, *sec_pin;
		BOOL pins_ok;

		/* Split up LED specification */
		if (split_ledspec(argv[optind],
		                  &if_name, &ifh_name,
		                  &leddrvr_name, &device,
		                  &prim_pin, &sec_pin) != OK)
		{
			fprintf(stderr,
			        "Invalid LED specification \"%s\"!\n",
			        argv[optind]);
			fprintf(stderr,
			        "Try \"%s --help\" or \"%s --usage\" for more information.\n",
			        argv[0], argv[0]);
			exit(1);
		}

		/* We've got a default interface handler */
		if (!ifh_name)
			ifh_name = DEFAULT_IFH;

		/* Set up LED structure */
		_leds[i].if_name  = if_name;
		_leds[i].color    = LEDCOLOR_PRIM;

		/* Load LED driver... */
		_leds[i].leddrvr = load_leddriver(PACKAGE_LIBDIR, leddrvr_name);
		if (!_leds[i].leddrvr)
		{
			fputs(_errmsg, stderr);
			exit(1);
		}

		/* ...and interface handler */
		_leds[i].ifh = load_ifhandler(PACKAGE_LIBDIR, ifh_name);
		if (!_leds[i].ifh)
		{
			fputs(_errmsg, stderr);
			exit(1);
		}

		/* Initialize it */
		if (_leds[i].ifh->init(&_leds[i]) != OK)
		{
			fputs(_leds[i].ifh->errmsg(), stderr);
			exit(1);
		}

		/* Check specified pins */
		pins_ok = FALSE;
		_leds[i].prim_pin = alloc_pin(_leds[i].leddrvr, prim_pin);
		if (_leds[i].prim_pin)
		{
			if (sec_pin)
			{
				if (_leds[i].ifh->colorctl)
				{
					_leds[i].sec_pin = alloc_pin(_leds[i].leddrvr, prim_pin);
					if (_leds[i].sec_pin)
						pins_ok = TRUE;
				}
				else
					snprintf(_errmsg, sizeof(_errmsg),
					         "interface handler does not support a secondary pin");
			}
			else
				pins_ok = TRUE;
		}
		if (!pins_ok)
		{
			fprintf(stderr,
			        "Error in LED specification \"%s\": %s!\n",
			        argv[optind], _errmsg);
			exit(1);
		}

		/* Finally initialize the LED driver */
		if (_leds[i].leddrvr->init(device) != OK)
		{
			fputs(_leds[i].leddrvr->errmsg(), stderr);
			exit(1);
		}
	}

	/* Now that all structures are set up, clear all LEDs */
	if (clear_leds() != OK)
	{
		fprintf(stderr,
		        "Error clearing LEDs: %s!\n",
		        _errmsg);
		exit(1);
	}

	/* Install signal handler */
	signal(SIGHUP, shutdown_handler);
	signal(SIGINT, shutdown_handler);
	signal(SIGABRT, shutdown_handler);
	signal(SIGTERM, shutdown_handler);
	signal(SIGSEGV, shutdown_handler);
	signal(SIGBUS, shutdown_handler);
	signal(SIGUSR1, shutdown_handler);
	signal(SIGUSR2, shutdown_handler);
}

/*
** Main code.
*/
int main(int argc, char **argv)
{
	/* Initialize */
	init(argc, argv);

	/* Loop until someone presses CTRL-C */
	while (!_shutdown)
	{
		int i;

		/* Process all LEDs watched */
		for (i = 0; i < _num_leds; i++)
		{
			LED *led = &_leds[i];

			/* Call this LED's interface handler's power control function
			   and also its color control function, if defined and if a
			   secondary pin was specified */
			if (led->ifh->powerctl(led) != OK)
				_shutdown = TRUE;
			if (led->sec_pin)
				led->ifh->colorctl(led);

			fprintf(stderr, "LED for iface %s: up %d color %d (prim: %d, sec: %d, both: %d) led->prim_pin: %p sec_pin: %p\n",
			        led->if_name, led->up, led->color, LEDCOLOR_PRIM, LEDCOLOR_SEC, LEDCOLOR_BOTH,
			        led->prim_pin, led->sec_pin);

			/* Enable LED pins as necessary */
			if (led->on)
			{
				if (led->color & LEDCOLOR_PRIM)
				{
					fprintf(stderr, "enabling prim\n");
					led->leddrvr->enable(led->prim_pin);
				}
				if (led->color & LEDCOLOR_SEC)
				{
					fprintf(stderr, "enabling sec\n");
					led->leddrvr->enable(led->sec_pin);
				}
			}
		}

		/* Walk over another time, this time committing the changes
		   to the LED drivers */
		for (i = 0; i < _num_leds; i++)
		{
			if (_leds[i].leddrvr->commit() != OK)
				_shutdown = TRUE;
		}

		/* Sleep for a while */
		usleep(SLEEP_TIME);
	}

	/* At this place we can safely ignore the return code */
	(void)clear_leds();

	return 0;
}
