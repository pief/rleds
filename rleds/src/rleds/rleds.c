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

#include <assert.h>
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
#include "../common/netifhandlers.h"

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
struct option _long_opts[] =
{
	{ "led-drivers",	no_argument,		NULL,	'l' },
	{ "netif-handlers",	no_argument,		NULL,	'i' },
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
	"  -i, --netif-handlers      list available network interface handlers\n"
        "  -V, --version             print version and exit\n\n"

	"<LEDSPEC> is a string of the format\n"
	" <netifname>['['<netifhandler>']']:<led driver>['['<device>']']:<prim>[,<sec>]\n"
	"where <prim> and optionally <sec> define the pins of the LED driver at which the\n"
	"(tri-color) LED for <netifname> is connected.\n\n"

	"Examples:\n"
	" eth0:parallel:2 ppp0[ppp]:parallel[/dev/parport1]:3,4 eth3:serial[/dev/tty5]:1\n";

/* Dynamically created LED management array */
LED *_leds;
uint _num_leds;

/* Additionally created array of pointers to LED structures that contains only one LED
   structure for each PORT handle that was obtained. This is used so we don't call a
   LED driver's shutdown function for a PORT handle twice.

   An alternative approach would be to keep all PORT handles in a linked list. */
LED **_ports;
uint _num_ports;

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

	assert(path);

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
		         "dlsym() error in %s\n",
		         errmsg);
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

	assert(dir && leddriver_name);

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
** netifh = load_netifhandler(dir, ifhandler_name)
**
** Attempts to load an network interface handler in "dir" by its canonical name, e.g.
** "generic" instead of "/foo/bar/netifh_generic.so".
**
** Returns a pointer to the network interface handler's NETIFHANDLER structure or NULL
** on error, in which case an error message can be found in _errmsg.
*/
NETIFHANDLER *load_netifhandler(char *dir, char *netifhandler_name)
{
	char *path;
	size_t len;
	NETIFHANDLER *netifh;

	assert(dir && netifhandler_name);

	/* Compose full path */
	len = strlen(dir) + 1 + strlen(NETIFHANDLER_PREFIX) + strlen(netifhandler_name) + 4;
	path = malloc(len);
	if (!path)
	{
		snprintf(_errmsg, sizeof(_errmsg),
		         "Not enough memory for complete path to network interface handler \"%s\"!\n",
		         netifhandler_name);
		return NULL;
	}
	snprintf(path, len, "%s/%s%s.so", dir, NETIFHANDLER_PREFIX, netifhandler_name);

	/* Attemt to load as shared object */
	netifh = (NETIFHANDLER *)load_shobj(path);
	if (netifh == (void *)-1)
		return NULL;
	if (!netifh)
	{
		snprintf(_errmsg, sizeof(_errmsg),
		         "\"%s\" misses the defining NETIFHANDLER structure!\n",
		         netifhandler_name);
		return NULL;
	}

	return netifh;
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
** Returns OK on success and ERR on failure.
*/
RC list_shobjs(char *dir,
               char *name,
               char *struct_name,
               int (*filter_func)(const struct dirent *),
               void (*print_func)(char *, void *))
{
	int i, count;
	struct dirent **dirents;

	assert(dir && name && struct_name && filter_func && print_func);

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
void print_leddriver(char *name, void *ifstruct)
{
	LEDDRIVER *leddrvr = (LEDDRIVER *)ifstruct;
	char *leddrvr_name, **pin, *p;
	char buf[PRINT_INDENT];

	assert(name && ifstruct);

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
	for (pin = leddrvr->pins; *pin; pin++)
	{
		fprintf(stdout, " %s", *pin);
	}
	fprintf(stdout, "\n");
}

/*
** Filter function for scandir() employed in list_netifhandlers() below.
**
** Returns 1 for filenames that begin with the prefix "netifh_" and 0 for other
** filenames.
*/
int filter_netifhandlers(const struct dirent *dirent)
{
	if (!strncmp(dirent->d_name, NETIFHANDLER_PREFIX, strlen(NETIFHANDLER_PREFIX)))
		return 1;
	else
		return 0;
}

/*
** Print function for list_shobjs() above. Referenced by the list_netifhandlers()
** macro below.
*/
void print_netifhandler(char *name, void *ifstruct)
{
	NETIFHANDLER *netifh = (NETIFHANDLER *)ifstruct;
	char *netifh_name, *p;
	char buf[PRINT_INDENT];

	assert(name && ifstruct);

	/* Compare API versions */
	if (netifh->api_ver != NETIFHANDLER_API_VER)
	{
		fprintf(stderr,
		        "\"%s\": wrong API version (%d != ours: %d)\n",
		        name, netifh->api_ver, NETIFHANDLER_API_VER);
		return;
	}

	/* Create short name so the user knows what to specify in LEDSPECs */
	netifh_name = strdup(name) + strlen(NETIFHANDLER_PREFIX);
	p = strstr(netifh_name, ".so");
	if (p)
		*p = '\0';

	/* Print out information */
	snprintf(buf, PRINT_INDENT,
	         "- %s (v%s) ",
	         netifh_name, netifh->ver);

	fprintf(stdout,
	        "%-*s%s\n",
	        PRINT_INDENT, buf, netifh->desc);

	fprintf(stdout,
	        "%*cTri-color LEDs: %s\n",
	        PRINT_INDENT, ' ', netifh->tricol_desc);
}

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

	assert(spec && if_name && ifh_name && leddrvr_name && device && prim_pin && sec_pin);

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
** init(argc, argv);
**
** Initialization routine.
**
** exit() is used here instead of return codes since we not only decide on whether
** to exit or not but also on the return code.
*/
void init(int argc, char **argv)
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
				if (list_shobjs(PACKAGE_LIBDIR, "LED drivers", "LEDDRIVER",
				                filter_leddrivers, print_leddriver) == OK)
					exit(0);
				else
					exit(1);
			}
			/* -i, --interface-handlers */
			case 'i':
			{
				if (list_shobjs(PACKAGE_LIBDIR, "network interface handlers", "NETIFHANDLER",
				                filter_netifhandlers, print_netifhandler) == OK)
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

	/* The remaining arguments are assumed to be LED specifications. Check that at least
	   one such definition was given. */
	_num_leds = argc-optind;
	if (_num_leds == 0)
	{
		fprintf(stderr, "No LED specifications given on command line!\n");
		fprintf(stderr, "Try \"%s --help\" or \"%s --usage\" for more information.\n",
		        argv[0], argv[0]);
		exit(1);
	}

	/* Allocate memory for LED structures and _ports array. _num_leds is used here
	   as an initial upper bound for the size of the _ports array. We'll realloc() later. */
	_leds = calloc(_num_leds, sizeof(LED));
	_ports = calloc(_num_leds, sizeof(LED *));
	if (!_leds || !_ports)
	{
		fprintf(stderr, "Could not allocate memory for LED structures!\n");
		exit(1);
	}
	_num_ports = 0;

	/* Install shutdown routine */
	atexit(shutdown);	

	/* Process LED specifications */
	for (i=0; optind<argc ; optind++, i++)
	{
		char *netifh_name, *leddrvr_name;
		LED *led = &_leds[i];
		int j;
		BOOL pins_ok;

		/* Split up LED specification */
		if (split_ledspec(argv[optind],
		                  &led->netif_name, &netifh_name,
		                  &leddrvr_name, &led->device_name,
		                  &led->prim_pin, &led->sec_pin) != OK)
		{
			fprintf(stderr,
			        "Invalid LED specification \"%s\"!\n",
			        argv[optind]);
			fprintf(stderr,
			        "Try \"%s --help\" or \"%s --usage\" for more information.\n",
			        argv[0], argv[0]);
			exit(1);
		}

		/* Use default name for network interface handler, if necessary */
		if (!netifh_name)
			netifh_name = DEFAULT_NETIFH;

		/* Load specified network interface handler */
		led->netifh = load_netifhandler(PACKAGE_LIBDIR, netifh_name);
		if (!led->netifh)
		{
			fputs(_errmsg, stderr);
			exit(1);
		}

		/* ...and initialize it */
		led->netif = led->netifh->init(led->netif_name);
		if (!led->netif)
		{
			fputs(led->netifh->errmsg(NULL), stderr);
			exit(1);
		}

		/* Load specified LED driver */
		led->leddrvr = load_leddriver(PACKAGE_LIBDIR, leddrvr_name);
		if (!led->leddrvr)
		{
			fputs(_errmsg, stderr);
			exit(1);
		}

		/* Check whether a PORT structure has already been initialized for
		   this device */
		for (j = 0; j < i; j++)
		{
			LED *prev_led = &_leds[j];

			if (strcasecmp(led->device_name, prev_led->device_name) == 0)
				led->port = prev_led->port;
		}

		/* If not, initialize LED driver for the specified device */
		if (!led->port)
		{
			led->port = led->leddrvr->init(led->device_name);
			if (led->port)
			{
				_ports[_num_ports] = led;
				_num_ports++;
			}
		}
		if (!led->port)
		{
			fputs(led->leddrvr->errmsg(NULL), stderr);
			exit(1);
		}

		/* Try to allocate specified pins */
		pins_ok = FALSE;
		if (led->leddrvr->alloc(led->port, led->prim_pin) == OK)
		{
			if (led->sec_pin)
			{
				if (led->leddrvr->alloc(led->port, led->sec_pin) == OK)
					pins_ok = TRUE;
			}
			else
				pins_ok = TRUE;
		}
		if (!pins_ok)
		{
			fprintf(stderr,
			        "Error in LED specification \"%s\": %s!\n",
			        argv[optind], led->leddrvr->errmsg(led->port));
			exit(1);
		}

		/* Finally, complete LED structure initialization */
		led->ledstate = LEDSTATE_OFF;
	}

	/* Install signal handler */
	signal(SIGHUP, sig_handler);
	signal(SIGINT, sig_handler);
	signal(SIGABRT, sig_handler);
	signal(SIGTERM, sig_handler);
	signal(SIGUSR1, sig_handler);
	signal(SIGUSR2, sig_handler);
}

/*
** Shutdown function.
*/
void shutdown(void)
{
	int i;

	/* Shutdown interface handlers and LED drivers */
	for (i = 0; i < _num_ports; i++)
	{
		LED *led = _ports[i];

		(void)_leds[i].leddrvr->reset(_leds[i].port);

		led->netifh->shutdown(led->netif);
		led->leddrvr->shutdown(led->port);
	}
}

/*
** sig_handler(sig)
**
** Signal handler for all signals. Turns off all LEDs and instructs the main
** routine to shut down.
*/
void sig_handler(int sig)
{
	_shutdown = 1;
	signal(sig, sig_handler);
}


/*
** Main routine.
*/
int main(int argc, char **argv)
{
	int i;

	/* Initialize */
	init(argc, argv);

	/* Loop until someone presses CTRL-C */
	while (!_shutdown)
	{
		/* Process all LEDs watched */
		for (i = 0; i < _num_leds; i++)
		{
			RC rc = OK;
			LED *led = &_leds[i];

			fprintf(stderr, "calling led->netifh->col() with led->netif: %p led->ledstate: %p\n",
			        led->netif, &led->ledstate);

			/* Call this LED's interface handler's LED color function */
			if (led->netifh->col(led->netif, &led->ledstate) != OK)
			{
				fprintf(stderr,
				        "Error examining interface \"%s\": %s!\n",
				        _leds[i].netif_name, _leds[i].netifh->errmsg(_leds[i].netif));
				_shutdown = TRUE;
				break;
			}

			fprintf(stderr, ".");

			/* Enable LED pins as necessary */
			if (led->ledstate == LEDSTATE_PRIM || led->ledstate == LEDSTATE_BOTH)
				rc = led->leddrvr->enable(led->port, led->prim_pin);
			if (led->sec_pin &&
			    (led->ledstate == LEDSTATE_SEC || led->ledstate == LEDSTATE_BOTH))
				rc |= led->leddrvr->enable(led->port, led->sec_pin);
			if (rc != OK)
			{
				fprintf(stderr,
				        "Error enabling pins on \"%s\": %s!\n",
				        led->device_name, led->leddrvr->errmsg(_leds[i].port));
				_shutdown = TRUE;
				break;
			}
		}

		/* Walk over another time, this time committing the changes
		   to the LED drivers */
		for (i = 0; i < _num_leds; i++)
		{
			LED *led = &_leds[i];

			if (led->leddrvr->commit(led->port) != OK)
				_shutdown = TRUE;
		}

		/* Sleep for a while */
		usleep(SLEEP_TIME);
	}

	return 0;
}
