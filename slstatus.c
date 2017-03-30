/* See LICENSE file for copyright and license details. */

#include <err.h>
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
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/socket.h>
#include <sys/soundcard.h>
#include <sys/sysinfo.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <time.h>
#include <unistd.h>
#include <X11/Xlib.h>

#include "arg.h"

struct arg {
	char *(*func)();
	const char *fmt;
	const char *args;
};

static char *smprintf(const char *fmt, ...);
static char *battery_perc(const char *bat);
static char *battery_state(const char *bat);
static char *cpu_perc(void);
static char *datetime(const char *fmt);
static char *disk_free(const char *mnt);
static char *disk_perc(const char *mnt);
static char *disk_total(const char *mnt);
static char *disk_used(const char *mnt);
static char *entropy(void);
static char *gid(void);
static char *hostname(void);
static char *ip(const char *iface);
static char *kernel_release(void);
static char *keyboard_indicators(void);
static char *load_avg(void);
static char *ram_free(void);
static char *ram_perc(void);
static char *ram_used(void);
static char *ram_total(void);
static char *run_command(const char *cmd);
static char *swap_free(void);
static char *swap_perc(void);
static char *swap_used(void);
static char *swap_total(void);
static char *temp(const char *file);
static char *uid(void);
static char *uptime(void);
static char *username(void);
static char *vol_perc(const char *card);
static char *wifi_perc(const char *iface);
static char *wifi_essid(const char *iface);
static void sighandler(const int signo);
static void usage(const int eval);

char *argv0;
static unsigned short int delay = 0;
static unsigned short int done;
static unsigned short int dflag, oflag, nflag, iflag;
static Display *dpy;

#include "config.h"

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
	if (ret == NULL) {
		err(1, "malloc");
	}

	va_start(ap, fmt);
	vsnprintf(ret, len, fmt, ap);
	va_end(ap);

	return ret;
}

static char *
battery_perc(const char *bat)
{
	int perc;
	char path[PATH_MAX];
	FILE *fp;

	snprintf(path, sizeof(path), "%s%s%s", "/sys/class/power_supply/", bat, "/capacity");
	fp = fopen(path, "r");
	if (fp == NULL) {
		warn("Failed to open file %s", path);
		return smprintf("%s", UNKNOWN_STR);
	}
	fscanf(fp, "%i", &perc);
	fclose(fp);

	return smprintf("%d%%", perc);
}

static char *
battery_state(const char *bat)
{
	char path[PATH_MAX];
	char state[12];
	FILE *fp;

	snprintf(path, sizeof(path), "%s%s%s", "/sys/class/power_supply/", bat, "/status");
	fp = fopen(path, "r");
	if (fp == NULL) {
		warn("Failed to open file %s", path);
		return smprintf("%s", UNKNOWN_STR);
	}
	fscanf(fp, "%12s", state);
	fclose(fp);

	if (strcmp(state, "Charging") == 0) {
		return smprintf("+");
	} else if (strcmp(state, "Discharging") == 0) {
		return smprintf("-");
	} else if (strcmp(state, "Full") == 0) {
		return smprintf("=");
	} else if (strcmp(state, "Unknown") == 0) {
		return smprintf("/");
	} else {
		return smprintf("?");
	}
}

static char *
cpu_perc(void)
{
	int perc;
	long double a[4], b[4];
	FILE *fp;

	fp = fopen("/proc/stat", "r");
	if (fp == NULL) {
		warn("Failed to open file /proc/stat");
		return smprintf("%s", UNKNOWN_STR);
	}
	fscanf(fp, "%*s %Lf %Lf %Lf %Lf", &a[0], &a[1], &a[2], &a[3]);
	fclose(fp);

	if (!nflag) {
		delay++;
		sleep(delay);
	}

	fp = fopen("/proc/stat", "r");
	if (fp == NULL) {
		warn("Failed to open file /proc/stat");
		return smprintf("%s", UNKNOWN_STR);
	}
	fscanf(fp, "%*s %Lf %Lf %Lf %Lf", &b[0], &b[1], &b[2], &b[3]);
	fclose(fp);

	perc = 100 * ((b[0]+b[1]+b[2]) - (a[0]+a[1]+a[2])) / ((b[0]+b[1]+b[2]+b[3]) - (a[0]+a[1]+a[2]+a[3]));
	return smprintf("%d%%", perc);
}

static char *
datetime(const char *fmt)
{
	time_t t;
	char str[80];

	t = time(NULL);
	if (strftime(str, sizeof(str), fmt, localtime(&t)) == 0) {
		return smprintf("%s", UNKNOWN_STR);
	}

	return smprintf("%s", str);
}

static char *
disk_free(const char *mnt)
{
	struct statvfs fs;

	if (statvfs(mnt, &fs) < 0) {
		warn("Failed to get filesystem info");
		return smprintf("%s", UNKNOWN_STR);
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
		return smprintf("%s", UNKNOWN_STR);
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
		return smprintf("%s", UNKNOWN_STR);
	}

	return smprintf("%f", (float)fs.f_bsize * (float)fs.f_blocks / 1024 / 1024 / 1024);
}

static char *
disk_used(const char *mnt)
{
	struct statvfs fs;

	if (statvfs(mnt, &fs) < 0) {
		warn("Failed to get filesystem info");
		return smprintf("%s", UNKNOWN_STR);
	}

	return smprintf("%f", (float)fs.f_bsize * ((float)fs.f_blocks - (float)fs.f_bfree) / 1024 / 1024 / 1024);
}

static char *
entropy(void)
{
	int num;
	FILE *fp;

	fp= fopen("/proc/sys/kernel/random/entropy_avail", "r");
	if (fp == NULL) {
		warn("Failed to open file /proc/sys/kernel/random/entropy_avail");
		return smprintf("%s", UNKNOWN_STR);
	}
	fscanf(fp, "%d", &num);
	fclose(fp);

	return smprintf("%d", num);
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
		warn("hostname");
		return smprintf("%s", UNKNOWN_STR);
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
		return smprintf("%s", UNKNOWN_STR);
	}

	for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr == NULL) {
			continue;
		}
		s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in), host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
		if ((strcmp(ifa->ifa_name, iface) == 0) && (ifa->ifa_addr->sa_family == AF_INET)) {
			if (s != 0) {
				warnx("Failed to get IP address for interface %s", iface);
				return smprintf("%s", UNKNOWN_STR);
			}
			return smprintf("%s", host);
		}
	}

	freeifaddrs(ifaddr);

	return smprintf("%s", UNKNOWN_STR);
}

static char *
kernel_release(void)
{
	struct utsname udata;

	if (uname(&udata) < 0) {
		return smprintf(UNKNOWN_STR);
	}

	return smprintf("%s", udata.release);
}

static char *
keyboard_indicators(void)
{
	Display *dpy = XOpenDisplay(NULL);
	XKeyboardState state;
	XGetKeyboardControl(dpy, &state);
	XCloseDisplay(dpy);

	switch (state.led_mask) {
		case 1:
			return smprintf("c");
			break;
		case 2:
			return smprintf("n");
			break;
		case 3:
			return smprintf("cn");
			break;
		default:
			return smprintf("");
	}
}

static char *
load_avg(void)
{
	double avgs[3];

	if (getloadavg(avgs, 3) < 0) {
		warnx("Failed to get the load avg");
		return smprintf("%s", UNKNOWN_STR);
	}

	return smprintf("%.2f %.2f %.2f", avgs[0], avgs[1], avgs[2]);
}

static char *
ram_free(void)
{
	long free;
	FILE *fp;

	fp = fopen("/proc/meminfo", "r");
	if (fp == NULL) {
		warn("Failed to open file /proc/meminfo");
		return smprintf("%s", UNKNOWN_STR);
	}
	fscanf(fp, "MemFree: %ld kB\n", &free);
	fclose(fp);

	return smprintf("%f", (float)free / 1024 / 1024);
}

static char *
ram_perc(void)
{
	long total, free, buffers, cached;
	FILE *fp;

	fp = fopen("/proc/meminfo", "r");
	if (fp == NULL) {
		warn("Failed to open file /proc/meminfo");
		return smprintf("%s", UNKNOWN_STR);
	}
	fscanf(fp, "MemTotal: %ld kB\n", &total);
	fscanf(fp, "MemFree: %ld kB\n", &free);
	fscanf(fp, "MemAvailable: %ld kB\nBuffers: %ld kB\n", &buffers, &buffers);
	fscanf(fp, "Cached: %ld kB\n", &cached);
	fclose(fp);

	return smprintf("%d%%", 100 * ((total - free) - (buffers + cached)) / total);
}

static char *
ram_total(void)
{
	long total;
	FILE *fp;

	fp = fopen("/proc/meminfo", "r");
	if (fp == NULL) {
		warn("Failed to open file /proc/meminfo");
		return smprintf("%s", UNKNOWN_STR);
	}
	fscanf(fp, "MemTotal: %ld kB\n", &total);
	fclose(fp);

	return smprintf("%f", (float)total / 1024 / 1024);
}

static char *
ram_used(void)
{
	long free, total, buffers, cached;
	FILE *fp;

	fp = fopen("/proc/meminfo", "r");
	if (fp == NULL) {
		warn("Failed to open file /proc/meminfo");
		return smprintf("%s", UNKNOWN_STR);
	}
	fscanf(fp, "MemTotal: %ld kB\n", &total);
	fscanf(fp, "MemFree: %ld kB\n", &free);
	fscanf(fp, "MemAvailable: %ld kB\nBuffers: %ld kB\n", &buffers, &buffers);
	fscanf(fp, "Cached: %ld kB\n", &cached);
	fclose(fp);

	return smprintf("%f", (float)(total - free - buffers - cached) / 1024 / 1024);
}

static char *
run_command(const char *cmd)
{
	char *nlptr;
	FILE *fp;
	char buf[1024] = UNKNOWN_STR;

	fp = popen(cmd, "r");
	if (fp == NULL) {
		warn("Failed to get command output for %s", cmd);
		return smprintf("%s", UNKNOWN_STR);
	}
	fgets(buf, sizeof(buf), fp);
	pclose(fp);
	buf[sizeof(buf)-1] = '\0';

	if ((nlptr = strrchr(buf, '\n')) != NULL) {
		nlptr[0] = '\0';
	}

	return smprintf("%s", buf);
}

static char *
swap_free(void)
{
	long total, free;
	FILE *fp;
	char buf[2048];
	size_t bytes_read;
	char *match;

	fp = fopen("/proc/meminfo", "r");
	if (fp == NULL) {
		warn("Failed to open file /proc/meminfo");
		return smprintf("%s", UNKNOWN_STR);
	}

	if ((bytes_read = fread(buf, sizeof(char), sizeof(buf), fp)) == 0) {
		warn("swap_free: read error");
		fclose(fp);
		return smprintf("%s", UNKNOWN_STR);
	}

	buf[bytes_read] = '\0';
	fclose(fp);

	if ((match = strstr(buf, "SwapTotal")) == NULL) {
		return smprintf("%s", UNKNOWN_STR);
	}
	sscanf(match, "SwapTotal: %ld kB\n", &total);

	if ((match = strstr(buf, "SwapFree")) == NULL) {
		return smprintf("%s", UNKNOWN_STR);
	}
	sscanf(match, "SwapFree: %ld kB\n", &free);

	return smprintf("%f", (float)free / 1024 / 1024);
}

static char *
swap_perc(void)
{
	long total, free, cached;
	FILE *fp;
	char buf[2048];
	size_t bytes_read;
	char *match;

	fp = fopen("/proc/meminfo", "r");
	if (fp == NULL) {
		warn("Failed to open file /proc/meminfo");
		return smprintf("%s", UNKNOWN_STR);
	}

	if ((bytes_read = fread(buf, sizeof(char), sizeof(buf), fp)) == 0) {
		warn("swap_perc: read error");
		fclose(fp);
		return smprintf("%s", UNKNOWN_STR);
	}

	buf[bytes_read] = '\0';
	fclose(fp);

	if ((match = strstr(buf, "SwapTotal")) == NULL) {
		return smprintf("%s", UNKNOWN_STR);
	}
	sscanf(match, "SwapTotal: %ld kB\n", &total);

	if ((match = strstr(buf, "SwapCached")) == NULL) {
		return smprintf("%s", UNKNOWN_STR);
	}
	sscanf(match, "SwapCached: %ld kB\n", &cached);

	if ((match = strstr(buf, "SwapFree")) == NULL) {
		return smprintf("%s", UNKNOWN_STR);
	}
	sscanf(match, "SwapFree: %ld kB\n", &free);

	return smprintf("%d%%", 100 * (total - free - cached) / total);
}

static char *
swap_total(void)
{
	long total;
	FILE *fp;
	char buf[2048];
	size_t bytes_read;
	char *match;

	fp = fopen("/proc/meminfo", "r");
	if (fp == NULL) {
		warn("Failed to open file /proc/meminfo");
		return smprintf("%s", UNKNOWN_STR);
	}
	if ((bytes_read = fread(buf, sizeof(char), sizeof(buf), fp)) == 0) {
		warn("swap_total: read error");
		fclose(fp);
		return smprintf("%s", UNKNOWN_STR);
	}

	buf[bytes_read] = '\0';
	fclose(fp);

	if ((match = strstr(buf, "SwapTotal")) == NULL) {
		return smprintf("%s", UNKNOWN_STR);
	}
	sscanf(match, "SwapTotal: %ld kB\n", &total);

	return smprintf("%f", (float)total / 1024 / 1024);
}

static char *
swap_used(void)
{
	long total, free, cached;
	FILE *fp;
	char buf[2048];
	size_t bytes_read;
	char *match;

	fp = fopen("/proc/meminfo", "r");
	if (fp == NULL) {
		warn("Failed to open file /proc/meminfo");
		return smprintf("%s", UNKNOWN_STR);
	}
	if ((bytes_read = fread(buf, sizeof(char), sizeof(buf), fp)) == 0) {
		warn("swap_used: read error");
		fclose(fp);
		return smprintf("%s", UNKNOWN_STR);
	}

	buf[bytes_read] = '\0';
	fclose(fp);

	if ((match = strstr(buf, "SwapTotal")) == NULL) {
		return smprintf("%s", UNKNOWN_STR);
	}
	sscanf(match, "SwapTotal: %ld kB\n", &total);

	if ((match = strstr(buf, "SwapCached")) == NULL) {
		return smprintf("%s", UNKNOWN_STR);
	}
	sscanf(match, "SwapCached: %ld kB\n", &cached);

	if ((match = strstr(buf, "SwapFree")) == NULL) {
		return smprintf("%s", UNKNOWN_STR);
	}
	sscanf(match, "SwapFree: %ld kB\n", &free);

	return smprintf("%f", (float)(total - free - cached) / 1024 / 1024);
}

static char *
temp(const char *file)
{
	int temp;
	FILE *fp;

	fp = fopen(file, "r");
	if (fp == NULL) {
		warn("Failed to open file %s", file);
		return smprintf("%s", UNKNOWN_STR);
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
		return smprintf("%s", UNKNOWN_STR);
	}

	return smprintf("%s", pw->pw_name);
}

static char *
uid(void)
{
	return smprintf("%d", geteuid());
}


static char *
vol_perc(const char *card)
{
	unsigned int i;
	int v, afd, devmask;
	char *vnames[] = SOUND_DEVICE_NAMES;

	afd = open(card, O_RDONLY | O_NONBLOCK);
	if (afd == -1) {
		warn("Cannot open %s", card);
		return smprintf(UNKNOWN_STR);
	}

	if (ioctl(afd, SOUND_MIXER_READ_DEVMASK, &devmask) == -1) {
		warn("vol_perc: ioctl");
		goto clean;
	}
	for (i = 0; i < (sizeof(vnames) / sizeof((vnames[0]))); i++) {
		if (devmask & (1 << i)) {
			if (!strcmp("vol", vnames[i])) {
				if (ioctl(afd, MIXER_READ(i), &v) == -1) {
					warn("vol_perc: ioctl");
					goto clean;
				}
			}
		}
	}

	close(afd);
	return smprintf("%d%%", v & 0xff);
clean:
	close(afd);
	return smprintf("%s", UNKNOWN_STR);
}

static char *
wifi_perc(const char *iface)
{
	int perc;
	char buf[255];
	char *datastart;
	char path[PATH_MAX];
	char status[5];
	FILE *fp;

	snprintf(path, sizeof(path), "%s%s%s", "/sys/class/net/", iface, "/operstate");
	fp = fopen(path, "r");
	if (fp == NULL) {
		warn("Failed to open file %s", path);
		return smprintf("%s", UNKNOWN_STR);
	}
	fgets(status, 5, fp);
	fclose(fp);
	if(strcmp(status, "up\n") != 0) {
		return smprintf("%s", UNKNOWN_STR);
	}

	fp = fopen("/proc/net/wireless", "r");
	if (fp == NULL) {
		warn("Failed to open file /proc/net/wireless");
		return smprintf("%s", UNKNOWN_STR);
	}

	fgets(buf, sizeof(buf), fp);
	fgets(buf, sizeof(buf), fp);
	fgets(buf, sizeof(buf), fp);
	fclose(fp);

	if ((datastart = strstr(buf, iface)) == NULL) {
		return smprintf("%s", UNKNOWN_STR);
	}
	datastart = (datastart+(strlen(iface)+1));
	sscanf(datastart + 1, " %*d   %d  %*d  %*d		  %*d	   %*d		%*d		 %*d	  %*d		 %*d", &perc);

	return smprintf("%d%%", perc);
}

static char *
wifi_essid(const char *iface)
{
	char id[IW_ESSID_MAX_SIZE+1];
	int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	struct iwreq wreq;

	memset(&wreq, 0, sizeof(struct iwreq));
	wreq.u.essid.length = IW_ESSID_MAX_SIZE+1;
	snprintf(wreq.ifr_name, sizeof(wreq.ifr_name), "%s", iface);

	if (sockfd == -1) {
		warn("Failed to get ESSID for interface %s", iface);
		return smprintf("%s", UNKNOWN_STR);
	}
	wreq.u.essid.pointer = id;
	if (ioctl(sockfd,SIOCGIWESSID, &wreq) == -1) {
		warn("Failed to get ESSID for interface %s", iface);
		return smprintf("%s", UNKNOWN_STR);
	}

	close(sockfd);

	if (strcmp((char *)wreq.u.essid.pointer, "") == 0)
		return smprintf("%s", UNKNOWN_STR);
	else
		return smprintf("%s", (char *)wreq.u.essid.pointer);
}

static void
sighandler(const int signo)
{
	if (signo == SIGTERM || signo == SIGINT) {
		done = 1;
	}
}

static void
usage(const int eval)
{
	fprintf(stderr, "usage: %s [-in] [-d | -o | -v | -h]\n", argv0);
	exit(eval);
}

int
main(int argc, char *argv[])
{
	unsigned short int i;
	char status_string[2048];
	char *res, *element;
	char *ptr;
	struct arg argument;
	struct sigaction act;

	ARGBEGIN {
		case 'd':
			dflag = 1;
			break;
		case 'i':
			iflag = 1;
			break;
		case 'n':
			nflag = 1;
			break;
		case 'o':
			oflag = 1;
			break;
		case 'v':
			printf("slstatus (C) 2016-2017 slstatus engineers\n");
			return 0;
		case 'h':
			usage(0);
		default:
			usage(1);
	} ARGEND

	if (dflag && oflag) {
		usage(1);
	}
	if (dflag && daemon(1, 1) < 0) {
		err(1, "daemon");
	}

	memset(&act, 0, sizeof(act));
	act.sa_handler = sighandler;
	sigaction(SIGINT,  &act, 0);
	sigaction(SIGTERM, &act, 0);

	if (!oflag) {
		dpy = XOpenDisplay(NULL);
	}

	setlocale(LC_ALL, "");

	while (!done) {
		status_string[0] = '\0';

		if (iflag) {
			if (fgets(status_string, sizeof(status_string), stdin) == NULL)
				break; /* done */
			if ((ptr = strrchr(status_string, '\n')) != NULL)
				*ptr = '\0';
		} else {
			for (i = 0; i < sizeof(args) / sizeof(args[0]); ++i) {
				argument = args[i];
				if (argument.args == NULL) {
					res = argument.func();
				} else {
					res = argument.func(argument.args);
				}
				element = smprintf(argument.fmt, res);
				if (element == NULL) {
					element = smprintf("%s", UNKNOWN_STR);
					warnx("Failed to format output");
				}
				strncat(status_string, element, sizeof(status_string) - strlen(status_string) - 1);
				free(res);
				free(element);
			}
		}

		if (oflag) {
			printf("%s\n", status_string);
		} else {
			XStoreName(dpy, DefaultRootWindow(dpy), status_string);
			XSync(dpy, False);
		}

		if (!nflag) {
			if ((UPDATE_INTERVAL - delay) <= 0) {
				delay = 0;
				continue;
			} else {
				sleep(UPDATE_INTERVAL - delay);
				delay = 0;
			}
		}
	}

	if (!oflag) {
		XStoreName(dpy, DefaultRootWindow(dpy), NULL);
		XCloseDisplay(dpy);
	}

	return 0;
}
