/*
 * Copyright (C) 2016 Ali H. Fardan (Raiz)
 * see LICENSE for details
 */

#define VERSION "4.0"
#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 201112L

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/sysinfo.h>
#include <sys/types.h>

#include <X11/Xlib.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <limits.h>
#include <linux/wireless.h>
#include <locale.h>
#include <netdb.h>
#include <pwd.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#undef strlcat
#undef strlcpy

#include "extern/arg.h"
#include "extern/strlcat.h"
#include "extern/strlcpy.h"

struct arg {
	char *(*func)();
	const char *fmt;
	const char *args;
};

static char *_e(const char*, ...);
static char *datetime(const char *);
static char *gid(void);
static char *hostname(void);
static char *ip(const char *);
static char *load_avg(void);
static char *run_command(const char *);
static char *smprintf(const char *, ...);
static char *temp(const char *);
static char *uid(void);
static char *uptime(void);
static char *username(void);
static void set_status(const char *);
static void sighandler(const int);

char *argv0;
static Display *dpy;
static unsigned short int dflag, oflag;
static unsigned short int done;

#include "config.h"

/* TODO: use this on the rest of the program */
static char *
_e(const char *fmt, ...)
{
	va_list ap;

	if (fmt) {
		va_start(ap, fmt);
		vfprintf(stderr, fmt, ap);
		if (fmt[strlen(fmt)-1] == ':')
			fprintf(stderr, " %s", strerror(errno));
		fputc('\n', stderr);
		va_end(ap);
	}

	return (smprintf("%s", UNKNOWN_STR));
}

static char *
smprintf(const char *fmt, ...)
{
	va_list ap;
	char *ret;
	int len;

	va_start(ap, fmt);
	len = vsnprintf(NULL, 0, fmt, ap);
	va_end(ap);

	ret = malloc(++len);
	if (ret == NULL)
		err(1, "malloc");

	va_start(ap, fmt);
	vsnprintf(ret, len, fmt, ap);
	va_end(ap);

	return ret;
}

static char *
datetime(const char *fmt)
{
	time_t t;
	char str[80];

	t = time(NULL);
	if (strftime(str, sizeof(str), fmt, localtime(&t)) == 0)
		return (_e(NULL));

	return smprintf("%s", str);
}

static char *
disk_free(const char *mnt)
{
	struct statvfs fs;

	if (statvfs(mnt, &fs) < 0) {
		warn("Failed to get filesystem info");
		return smprintf(UNKNOWN_STR);
	}

	return smprintf("%f", (float)fs.f_bsize * (float)fs.f_bfree / 1024 / 1024 / 1024);
}

static char *
disk_perc(const char *mnt)
{
	int perc;
	struct statvfs fs;

	if (statvfs(mnt, &fs) < 0) {
		warn("Failed to get filesystem info");
		return smprintf(UNKNOWN_STR);
	}

	perc = 100 * (1.0f - ((float)fs.f_bfree / (float)fs.f_blocks));

	return smprintf("%d%%", perc);
}

static char *
disk_total(const char *mnt)
{
	struct statvfs fs;

	if (statvfs(mnt, &fs) < 0) {
		warn("Failed to get filesystem info");
		return smprintf(UNKNOWN_STR);
	}

	return smprintf("%f", (float)fs.f_bsize * (float)fs.f_blocks / 1024 / 1024 / 1024);
}

static char *
disk_used(const char *mnt)
{
	struct statvfs fs;

	if (statvfs(mnt, &fs) < 0) {
		warn("Failed to get filesystem info");
		return smprintf(UNKNOWN_STR);
	}

	return smprintf("%f", (float)fs.f_bsize * ((float)fs.f_blocks - (float)fs.f_bfree) / 1024 / 1024 / 1024);
}

static char *
gid(void)
{
	return smprintf("%d", getgid());
}

static char *
hostname(void)
{
	char buf[HOST_NAME_MAX];

	if (gethostname(buf, sizeof(buf)) == -1) {
		return (_e("hostname:"));
	}

	return smprintf("%s", buf);
}

static char *
ip(const char *iface)
{
	struct ifaddrs *ifaddr, *ifa;
	int s;
	char host[NI_MAXHOST];

	if (getifaddrs(&ifaddr) == -1) {
		warn("Failed to get IP address for interface %s", iface);
		return smprintf(UNKNOWN_STR);
	}

	for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr == NULL) {
			continue;
		}
		s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in), host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
		if ((strcmp(ifa->ifa_name, iface) == 0) && (ifa->ifa_addr->sa_family == AF_INET)) {
			if (s != 0) {
				warnx("Failed to get IP address for interface %s", iface);
				return smprintf(UNKNOWN_STR);
			}
			return smprintf("%s", host);
		}
	}

	freeifaddrs(ifaddr);

	return smprintf(UNKNOWN_STR);
}

static char *
load_avg(void)
{
	double avgs[3];

	if (getloadavg(avgs, 3) < 0) {
		warnx("Failed to get the load avg");
		return smprintf(UNKNOWN_STR);
	}

	return smprintf("%.2f %.2f %.2f", avgs[0], avgs[1], avgs[2]);
}

static char *
run_command(const char *cmd)
{
	FILE *fp;
	char buf[1024] = "n/a";

	fp = popen(cmd, "r");
	if (fp == NULL) {
		warn("Failed to get command output for %s", cmd);
		return smprintf(UNKNOWN_STR);
	}
	fgets(buf, sizeof(buf)-1, fp);
	pclose(fp);

	buf[strlen(buf)] = '\0';
	strtok(buf, "\n");

	return smprintf("%s", buf);
}

static char *
temp(const char *file)
{
	int temp;
	FILE *fp;

	fp = fopen(file, "r");
	if (fp == NULL) {
		warn("Failed to open file %s", file);
		return smprintf(UNKNOWN_STR);
	}
	fscanf(fp, "%d", &temp);
	fclose(fp);

	return smprintf("%d°C", temp / 1000);
}

static char *
uptime(void)
{
	struct sysinfo info;
	int h = 0;
	int m = 0;

	sysinfo(&info);
	h = info.uptime / 3600;
	m = (info.uptime - h * 3600 ) / 60;

	return smprintf("%dh %dm", h, m);
}

static char *
username(void)
{
	struct passwd *pw = getpwuid(geteuid());

	if (pw == NULL) {
		warn("Failed to get username");
		return smprintf(UNKNOWN_STR);
	}

	return smprintf("%s", pw->pw_name);
}

static char *
uid(void)
{
	return smprintf("%d", geteuid());
}

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

int
main(int argc, char *argv[])
{
	char *res, *element;
	char str[4096];
	struct arg argument;
	struct sigaction act;
	unsigned short int i;

	ARGBEGIN {
		case 'd':
			dflag = 1;
			break;
		case 'o':
			oflag = 1;
			break;
		case 'v':
			printf("sbar-%s\n", VERSION);
			return 0;
		default:
		usage:
			fprintf(stderr, "usage: %s [-d] [-h] [-o] [-v]\n", argv0);
			exit(1);
	} ARGEND

	if (dflag && oflag)
		goto usage;
	if (dflag && daemon(1, 1) == -1)
		err(1, "daemon");

	memset(&act, 0, sizeof(act));
	act.sa_handler = sighandler;
	sigaction(SIGINT,  &act, 0);
	sigaction(SIGTERM, &act, 0);

	if (!oflag)
		dpy = XOpenDisplay(NULL);

	setlocale(LC_ALL, "");

	while (!done) {
		str[0] = '\0';

		for (i = 0; i < sizeof(args) / sizeof(args[0]); ++i) {
			argument = args[i];
			if (argument.args == NULL)
				res = argument.func();
			else
				res = argument.func(argument.args);
			element = smprintf(argument.fmt, res);
			strlcat(str, element, sizeof(str));
			free(res);
			free(element);
		}

		if (!oflag)
			set_status(str);
		else
			printf("%s\n", str);
		sleep(1);
	}

	if (!oflag) {
		set_status(NULL);
		XCloseDisplay(dpy);
	}

	return 0;
}
