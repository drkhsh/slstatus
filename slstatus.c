/* See LICENSE file for copyright and license details. */

#include <err.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <X11/Xlib.h>

#undef strlcat
#undef strlcpy

#include "extern/arg.h"
#include "extern/strlcat.h"
#include "extern/strlcpy.h"
#include "extern/concat.h"

struct arg {
	void (*func)();
	const char *fmt;
	const char *args;
};

static void battery_perc(const char *);
static void battery_state(const char *);
static void cpu_perc(void);
static void datetime(const char *);
static void disk_free(const char *);
static void disk_perc(const char *);
static void disk_total(const char *);
static void disk_used(const char *);
static void entropy(void);
static void gid(void);
static void hostname(void);
static void ip(const char *);
static void load_avg(void);
static void ram_free(void);
static void ram_perc(void);
static void ram_used(void);
static void ram_total(void);
static void run_command(const char *);
static void temp(const char *);
static void uid(void);
static void uptime(void);
static void username(void);
static void vol_perc(const char *);
static void wifi_perc(const char *);
static void wifi_essid(const char *);

static void set_status(const char *);
static void sighandler(const int);
static void usage(void);

char *argv0;
char concat[];
static char resp[4096];
static unsigned short int delay, done, dflag, oflag;
static Display *dpy;

#include "config.h"

#include "slstatus.posix.c"
#ifdef __linux__
	#include "slstatus.linux.c"
#elif defined __FreeBSD__ | __OpenBSD__
	#include "slstatus.bsd.c"
#endif

static void
set_status(const char *str)
{
	XStoreName(dpy, DefaultRootWindow(dpy), str);
	XSync(dpy, False);
}

static void
sighandler(const int signo)
{
	if (signo == SIGTERM || signo == SIGINT) {
		done = 1;
	}
}

static void
usage(void)
{
	fprintf(stderr, "usage: %s [-dhov]\n", argv0);
	exit(1);
}

int
main(int argc, char *argv[])
{
	unsigned short int i;
	char str[4096];
	char element[100];
	struct sigaction act;

	ARGBEGIN {
		case 'd':
			dflag = 1;
			break;
		case 'o':
			oflag = 1;
			break;
		default:
			usage();
	} ARGEND

	if (dflag && oflag)
		usage();
	if (dflag)
		(void)daemon(1, 1);
	if (!oflag)
		dpy = XOpenDisplay(NULL);

	memset(&act, 0, sizeof(act));
	act.sa_handler = sighandler;
	sigaction(SIGINT,  &act, 0);
	sigaction(SIGTERM, &act, 0);

	while (!done) {
		str[0] = '\0';

		for (i = 0; i < sizeof(args) / sizeof(args[0]); ++i) {
			if (args[i].args == NULL)
				args[i].func();
			else
				args[i].func(args[i].args);

			snprintf(element, sizeof(element), args[i].fmt, resp);
			strlcat(str, element, sizeof(str));
		}

		if (!oflag)
			set_status(str);
		else
			printf("%s\n", str);

		/*
		 * subtract delay time spend in function
		 * calls from the actual global delay time
		 */
		sleep(UPDATE_INTERVAL - delay);
		delay = 0;
	}
	if (!oflag) {
		set_status((char *)0);
		XCloseDisplay(dpy);
	}
	return (0);
}
