/* See LICENSE file for copyright and license details. */

#include <dirent.h>
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

#define LEN(x) (sizeof (x) / sizeof *(x))

struct arg {
	const char *(*func)();
	const char *fmt;
	const char *args;
};

static const char *bprintf(const char *fmt, ...);
static const char *battery_perc(const char *bat);
static const char *battery_power(const char *bat);
static const char *battery_state(const char *bat);
static const char *cpu_freq(void);
static const char *cpu_perc(void);
static const char *datetime(const char *fmt);
static const char *disk_free(const char *mnt);
static const char *disk_perc(const char *mnt);
static const char *disk_total(const char *mnt);
static const char *disk_used(const char *mnt);
static const char *entropy(void);
static const char *gid(void);
static const char *hostname(void);
static const char *ip(const char *iface);
static const char *kernel_release(void);
static const char *keyboard_indicators(void);
static const char *load_avg(void);
static const char *num_files(const char *dir);
static const char *ram_free(void);
static const char *ram_perc(void);
static const char *ram_used(void);
static const char *ram_total(void);
static const char *run_command(const char *cmd);
static const char *swap_free(void);
static const char *swap_perc(void);
static const char *swap_used(void);
static const char *swap_total(void);
static const char *temp(const char *file);
static const char *uid(void);
static const char *uptime(void);
static const char *username(void);
static const char *vol_perc(const char *card);
static const char *wifi_perc(const char *iface);
static const char *wifi_essid(const char *iface);
static void sighandler(const int signo);
static void usage(void);

char *argv0;
static unsigned short int delay = 0;
static unsigned short int done;
static Display *dpy;

#include "config.h"

static char buf[MAXLEN];

static const char *
bprintf(const char *fmt, ...)
{
	va_list ap;
	size_t len;

	va_start(ap, fmt);
	len = vsnprintf(buf, sizeof(buf) - 1, fmt, ap);
	va_end(ap);

	if (len >= sizeof(buf))
		buf[sizeof(buf)-1] = '\0';

	return buf;
}

static const char *
battery_perc(const char *bat)
{
	int n, perc;
	char path[PATH_MAX];
	FILE *fp;

	snprintf(path, sizeof(path), "%s%s%s", "/sys/class/power_supply/", bat, "/capacity");
	fp = fopen(path, "r");
	if (fp == NULL) {
		warn("Failed to open file %s", path);
		return UNKNOWN_STR;
	}
	n = fscanf(fp, "%i", &perc);
	fclose(fp);
	if (n != 1)
		return UNKNOWN_STR;

	return bprintf("%d", perc);
}

static const char *
battery_power(const char *bat)
{
	char path[PATH_MAX];
	FILE *fp;
	int n, watts;

	snprintf(path, sizeof(path), "%s%s%s", "/sys/class/power_supply/", bat, "/power_now");
	fp = fopen(path, "r");
	if (fp == NULL) {
		warn("Failed to open file %s", path);
		return UNKNOWN_STR;
	}
	n = fscanf(fp, "%i", &watts);
	fclose(fp);
	if (n != 1)
		return UNKNOWN_STR;

	return bprintf("%d", (watts + 500000) / 1000000);
}

static const char *
battery_state(const char *bat)
{
	FILE *fp;
	struct {
		char *state;
		char *symbol;
	} map[] = {
		{ "Charging",    "+" },
		{ "Discharging", "-" },
		{ "Full",        "=" },
		{ "Unknown",     "/" },
	};
	size_t i;
	int n;
	char path[PATH_MAX], state[12];

	snprintf(path, sizeof(path), "%s%s%s", "/sys/class/power_supply/", bat, "/status");
	fp = fopen(path, "r");
	if (fp == NULL) {
		warn("Failed to open file %s", path);
		return UNKNOWN_STR;
	}
	n = fscanf(fp, "%12s", state);
	fclose(fp);
	if (n != 1)
		return UNKNOWN_STR;

	for (i = 0; i < LEN(map); i++) {
		if (!strcmp(map[i].state, state)) {
			break;
		}
	}

	return (i == LEN(map)) ? "?" : map[i].symbol;
}

static const char *
cpu_freq(void)
{
	int n, freq;
	FILE *fp;

	fp = fopen("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq", "r");
	if (fp == NULL) {
		warn("Failed to open file /sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq");
		return UNKNOWN_STR;
	}
	n = fscanf(fp, "%i", &freq);
	fclose(fp);
	if (n != 1)
		return UNKNOWN_STR;

	return bprintf("%d", (freq + 500) / 1000);
}

static const char *
cpu_perc(void)
{
	int n, perc;
	long double a[4], b[4];
	FILE *fp;

	fp = fopen("/proc/stat", "r");
	if (fp == NULL) {
		warn("Failed to open file /proc/stat");
		return UNKNOWN_STR;
	}
	n = fscanf(fp, "%*s %Lf %Lf %Lf %Lf", &a[0], &a[1], &a[2], &a[3]);
	fclose(fp);
	if (n != 4)
		return UNKNOWN_STR;

	delay++;
	sleep(delay);

	fp = fopen("/proc/stat", "r");
	if (fp == NULL) {
		warn("Failed to open file /proc/stat");
		return UNKNOWN_STR;
	}
	n = fscanf(fp, "%*s %Lf %Lf %Lf %Lf", &b[0], &b[1], &b[2], &b[3]);
	fclose(fp);
	if (n != 4)
		return UNKNOWN_STR;

	perc = 100 * ((b[0]+b[1]+b[2]) - (a[0]+a[1]+a[2])) / ((b[0]+b[1]+b[2]+b[3]) - (a[0]+a[1]+a[2]+a[3]));
	return bprintf("%d", perc);
}

static const char *
datetime(const char *fmt)
{
	time_t t;

	t = time(NULL);
	if (strftime(buf, sizeof(buf), fmt, localtime(&t)) == 0)
		return UNKNOWN_STR;

	return buf;
}

static const char *
disk_free(const char *mnt)
{
	struct statvfs fs;

	if (statvfs(mnt, &fs) < 0) {
		warn("Failed to get filesystem info");
		return UNKNOWN_STR;
	}

	return bprintf("%f", (float)fs.f_bsize * (float)fs.f_bfree / 1024 / 1024 / 1024);
}

static const char *
disk_perc(const char *mnt)
{
	int perc;
	struct statvfs fs;

	if (statvfs(mnt, &fs) < 0) {
		warn("Failed to get filesystem info");
		return UNKNOWN_STR;
	}

	perc = 100 * (1.0f - ((float)fs.f_bfree / (float)fs.f_blocks));

	return bprintf("%d", perc);
}

static const char *
disk_total(const char *mnt)
{
	struct statvfs fs;

	if (statvfs(mnt, &fs) < 0) {
		warn("Failed to get filesystem info");
		return UNKNOWN_STR;
	}

	return bprintf("%f", (float)fs.f_bsize * (float)fs.f_blocks / 1024 / 1024 / 1024);
}

static const char *
disk_used(const char *mnt)
{
	struct statvfs fs;

	if (statvfs(mnt, &fs) < 0) {
		warn("Failed to get filesystem info");
		return UNKNOWN_STR;
	}

	return bprintf("%f", (float)fs.f_bsize * ((float)fs.f_blocks - (float)fs.f_bfree) / 1024 / 1024 / 1024);
}

static const char *
entropy(void)
{
	int n, num;
	FILE *fp;

	fp= fopen("/proc/sys/kernel/random/entropy_avail", "r");
	if (fp == NULL) {
		warn("Failed to open file /proc/sys/kernel/random/entropy_avail");
		return UNKNOWN_STR;
	}
	n = fscanf(fp, "%d", &num);
	fclose(fp);
	if (n != 1)
		return UNKNOWN_STR;

	return bprintf("%d", num);
}

static const char *
gid(void)
{
	return bprintf("%d", getgid());
}

static const char *
hostname(void)
{
	if (gethostname(buf, sizeof(buf)) == -1) {
		warn("hostname");
		return UNKNOWN_STR;
	}

	return buf;
}

static const char *
ip(const char *iface)
{
	struct ifaddrs *ifaddr, *ifa;
	int s;
	char host[NI_MAXHOST];

	if (getifaddrs(&ifaddr) == -1) {
		warn("Failed to get IP address for interface %s", iface);
		return UNKNOWN_STR;
	}

	for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr == NULL) {
			continue;
		}
		s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in), host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
		if ((strcmp(ifa->ifa_name, iface) == 0) && (ifa->ifa_addr->sa_family == AF_INET)) {
			if (s != 0) {
				warnx("Failed to get IP address for interface %s", iface);
				return UNKNOWN_STR;
			}
			return bprintf("%s", host);
		}
	}

	freeifaddrs(ifaddr);

	return UNKNOWN_STR;
}

static const char *
kernel_release(void)
{
	struct utsname udata;

	if (uname(&udata) < 0) {
		return UNKNOWN_STR;
	}

	return bprintf("%s", udata.release);
}

static const char *
keyboard_indicators(void)
{
	Display *dpy = XOpenDisplay(NULL);
	XKeyboardState state;

	if (dpy == NULL) {
		warnx("XOpenDisplay failed");
		return UNKNOWN_STR;
	}
	XGetKeyboardControl(dpy, &state);
	XCloseDisplay(dpy);

	switch (state.led_mask) {
		case 1:
			return "c";
		case 2:
			return "n";
		case 3:
			return "cn";
		default:
			return "";
	}
}

static const char *
load_avg(void)
{
	double avgs[3];

	if (getloadavg(avgs, 3) < 0) {
		warnx("Failed to get the load avg");
		return UNKNOWN_STR;
	}

	return bprintf("%.2f %.2f %.2f", avgs[0], avgs[1], avgs[2]);
}

static const char *
num_files(const char *dir)
{
	struct dirent *dp;
	DIR *fd;
	int num = 0;

	if ((fd = opendir(dir)) == NULL) {
		warn("Failed to get number of files in directory %s", dir);
		return UNKNOWN_STR;
	}

	while ((dp = readdir(fd)) != NULL) {
		if (!strcmp(dp->d_name, ".") || !strcmp(dp->d_name, ".."))
			continue; /* skip self and parent */
		num++;
	}

	closedir(fd);

	return bprintf("%d", num);
}

static const char *
ram_free(void)
{
	long free;
	FILE *fp;
	int n;

	fp = fopen("/proc/meminfo", "r");
	if (fp == NULL) {
		warn("Failed to open file /proc/meminfo");
		return UNKNOWN_STR;
	}
	n = fscanf(fp, "MemFree: %ld kB\n", &free);
	fclose(fp);
	if (n != 1)
		return UNKNOWN_STR;

	return bprintf("%f", (float)free / 1024 / 1024);
}

static const char *
ram_perc(void)
{
	long total, free, buffers, cached;
	FILE *fp;

	fp = fopen("/proc/meminfo", "r");
	if (fp == NULL) {
		warn("Failed to open file /proc/meminfo");
		return UNKNOWN_STR;
	}
	if (fscanf(fp, "MemTotal: %ld kB\n", &total) != 1 ||
	    fscanf(fp, "MemFree: %ld kB\n", &free) != 1 ||
	    fscanf(fp, "MemAvailable: %ld kB\nBuffers: %ld kB\n",
	           &buffers, &buffers) != 2 ||
	    fscanf(fp, "Cached: %ld kB\n", &cached) != 1)
		goto scanerr;
	fclose(fp);

	return bprintf("%d", 100 * ((total - free) - (buffers + cached)) / total);

scanerr:
	fclose(fp);
	return UNKNOWN_STR;
}

static const char *
ram_total(void)
{
	long total;
	FILE *fp;
	int n;

	fp = fopen("/proc/meminfo", "r");
	if (fp == NULL) {
		warn("Failed to open file /proc/meminfo");
		return UNKNOWN_STR;
	}
	n = fscanf(fp, "MemTotal: %ld kB\n", &total);
	fclose(fp);
	if (n != 1)
		return UNKNOWN_STR;

	return bprintf("%f", (float)total / 1024 / 1024);
}

static const char *
ram_used(void)
{
	long free, total, buffers, cached;
	FILE *fp;

	fp = fopen("/proc/meminfo", "r");
	if (fp == NULL) {
		warn("Failed to open file /proc/meminfo");
		return UNKNOWN_STR;
	}
	if (fscanf(fp, "MemTotal: %ld kB\n", &total) != 1 ||
	    fscanf(fp, "MemFree: %ld kB\n", &free) != 1 ||
	    fscanf(fp, "MemAvailable: %ld kB\nBuffers: %ld kB\n",
	           &buffers, &buffers) != 2 ||
	    fscanf(fp, "Cached: %ld kB\n", &cached) != 1)
		goto scanerr;
	fclose(fp);

	return bprintf("%f", (float)(total - free - buffers - cached) / 1024 / 1024);

scanerr:
	fclose(fp);
	return UNKNOWN_STR;
}

static const char *
run_command(const char *cmd)
{
	char *p;
	FILE *fp;

	fp = popen(cmd, "r");
	if (fp == NULL) {
		warn("Failed to get command output for %s", cmd);
		return UNKNOWN_STR;
	}
	p = fgets(buf, sizeof(buf) - 1, fp);
	pclose(fp);
	if (!p)
		return UNKNOWN_STR;
	if ((p = strrchr(buf, '\n')) != NULL)
		p[0] = '\0';

	return buf[0] ? buf : UNKNOWN_STR;
}

static const char *
swap_free(void)
{
	long total, free;
	FILE *fp;
	size_t bytes_read;
	char *match;

	fp = fopen("/proc/meminfo", "r");
	if (fp == NULL) {
		warn("Failed to open file /proc/meminfo");
		return UNKNOWN_STR;
	}

	if ((bytes_read = fread(buf, sizeof(char), sizeof(buf) - 1, fp)) == 0) {
		warn("swap_free: read error");
		fclose(fp);
		return UNKNOWN_STR;
	}
	fclose(fp);

	if ((match = strstr(buf, "SwapTotal")) == NULL)
		return UNKNOWN_STR;
	sscanf(match, "SwapTotal: %ld kB\n", &total);

	if ((match = strstr(buf, "SwapFree")) == NULL)
		return UNKNOWN_STR;
	sscanf(match, "SwapFree: %ld kB\n", &free);

	return bprintf("%f", (float)free / 1024 / 1024);
}

static const char *
swap_perc(void)
{
	long total, free, cached;
	FILE *fp;
	size_t bytes_read;
	char *match;

	fp = fopen("/proc/meminfo", "r");
	if (fp == NULL) {
		warn("Failed to open file /proc/meminfo");
		return UNKNOWN_STR;
	}

	if ((bytes_read = fread(buf, sizeof(char), sizeof(buf) - 1, fp)) == 0) {
		warn("swap_perc: read error");
		fclose(fp);
		return UNKNOWN_STR;
	}
	fclose(fp);

	if ((match = strstr(buf, "SwapTotal")) == NULL)
		return UNKNOWN_STR;
	sscanf(match, "SwapTotal: %ld kB\n", &total);

	if ((match = strstr(buf, "SwapCached")) == NULL)
		return UNKNOWN_STR;
	sscanf(match, "SwapCached: %ld kB\n", &cached);

	if ((match = strstr(buf, "SwapFree")) == NULL)
		return UNKNOWN_STR;
	sscanf(match, "SwapFree: %ld kB\n", &free);

	return bprintf("%d", 100 * (total - free - cached) / total);
}

static const char *
swap_total(void)
{
	long total;
	FILE *fp;
	size_t bytes_read;
	char *match;

	fp = fopen("/proc/meminfo", "r");
	if (fp == NULL) {
		warn("Failed to open file /proc/meminfo");
		return UNKNOWN_STR;
	}
	if ((bytes_read = fread(buf, sizeof(char), sizeof(buf) - 1, fp)) == 0) {
		warn("swap_total: read error");
		fclose(fp);
		return UNKNOWN_STR;
	}
	fclose(fp);

	if ((match = strstr(buf, "SwapTotal")) == NULL)
		return UNKNOWN_STR;
	sscanf(match, "SwapTotal: %ld kB\n", &total);

	return bprintf("%f", (float)total / 1024 / 1024);
}

static const char *
swap_used(void)
{
	long total, free, cached;
	FILE *fp;
	size_t bytes_read;
	char *match;

	fp = fopen("/proc/meminfo", "r");
	if (fp == NULL) {
		warn("Failed to open file /proc/meminfo");
		return UNKNOWN_STR;
	}
	if ((bytes_read = fread(buf, sizeof(char), sizeof(buf) - 1, fp)) == 0) {
		warn("swap_used: read error");
		fclose(fp);
		return UNKNOWN_STR;
	}
	fclose(fp);

	if ((match = strstr(buf, "SwapTotal")) == NULL)
		return UNKNOWN_STR;
	sscanf(match, "SwapTotal: %ld kB\n", &total);

	if ((match = strstr(buf, "SwapCached")) == NULL)
		return UNKNOWN_STR;
	sscanf(match, "SwapCached: %ld kB\n", &cached);

	if ((match = strstr(buf, "SwapFree")) == NULL)
		return UNKNOWN_STR;
	sscanf(match, "SwapFree: %ld kB\n", &free);

	return bprintf("%f", (float)(total - free - cached) / 1024 / 1024);
}

static const char *
temp(const char *file)
{
	int n, temp;
	FILE *fp;

	fp = fopen(file, "r");
	if (fp == NULL) {
		warn("Failed to open file %s", file);
		return UNKNOWN_STR;
	}
	n = fscanf(fp, "%d", &temp);
	fclose(fp);
	if (n != 1)
		return UNKNOWN_STR;

	return bprintf("%d", temp / 1000);
}

static const char *
uptime(void)
{
	struct sysinfo info;
	int h = 0;
	int m = 0;

	sysinfo(&info);
	h = info.uptime / 3600;
	m = (info.uptime - h * 3600 ) / 60;

	return bprintf("%dh %dm", h, m);
}

static const char *
username(void)
{
	struct passwd *pw = getpwuid(geteuid());

	if (pw == NULL) {
		warn("Failed to get username");
		return UNKNOWN_STR;
	}

	return bprintf("%s", pw->pw_name);
}

static const char *
uid(void)
{
	return bprintf("%d", geteuid());
}


static const char *
vol_perc(const char *card)
{
	unsigned int i;
	int v, afd, devmask;
	char *vnames[] = SOUND_DEVICE_NAMES;

	afd = open(card, O_RDONLY | O_NONBLOCK);
	if (afd == -1) {
		warn("Cannot open %s", card);
		return UNKNOWN_STR;
	}

	if (ioctl(afd, SOUND_MIXER_READ_DEVMASK, &devmask) == -1) {
		warn("Cannot get volume for %s", card);
		close(afd);
		return UNKNOWN_STR;
	}
	for (i = 0; i < LEN(vnames); i++) {
		if (devmask & (1 << i) && !strcmp("vol", vnames[i])) {
			if (ioctl(afd, MIXER_READ(i), &v) == -1) {
				warn("vol_perc: ioctl");
				close(afd);
				return UNKNOWN_STR;
			}
		}
	}

	close(afd);

	return bprintf("%d", v & 0xff);
}

static const char *
wifi_perc(const char *iface)
{
	int i, perc;
	char *p, *datastart;
	char path[PATH_MAX];
	char status[5];
	FILE *fp;

	snprintf(path, sizeof(path), "%s%s%s", "/sys/class/net/", iface, "/operstate");
	fp = fopen(path, "r");
	if (fp == NULL) {
		warn("Failed to open file %s", path);
		return UNKNOWN_STR;
	}
	p = fgets(status, 5, fp);
	fclose(fp);
	if(!p || strcmp(status, "up\n") != 0) {
		return UNKNOWN_STR;
	}

	fp = fopen("/proc/net/wireless", "r");
	if (fp == NULL) {
		warn("Failed to open file /proc/net/wireless");
		return UNKNOWN_STR;
	}

	for (i = 0; i < 3; i++) {
		if (!(p = fgets(buf, sizeof(buf) - 1, fp)))
			break;
	}
	fclose(fp);
	if (i < 2 || !p)
		return UNKNOWN_STR;

	if ((datastart = strstr(buf, iface)) == NULL)
		return UNKNOWN_STR;

	datastart = (datastart+(strlen(iface)+1));
	sscanf(datastart + 1, " %*d   %d  %*d  %*d		  %*d	   %*d		%*d		 %*d	  %*d		 %*d", &perc);

	return bprintf("%d", perc);
}

static const char *
wifi_essid(const char *iface)
{
	static char id[IW_ESSID_MAX_SIZE+1];
	int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	struct iwreq wreq;

	memset(&wreq, 0, sizeof(struct iwreq));
	wreq.u.essid.length = IW_ESSID_MAX_SIZE+1;
	snprintf(wreq.ifr_name, sizeof(wreq.ifr_name), "%s", iface);

	if (sockfd == -1) {
		warn("Failed to get ESSID for interface %s", iface);
		return UNKNOWN_STR;
	}
	wreq.u.essid.pointer = id;
	if (ioctl(sockfd,SIOCGIWESSID, &wreq) == -1) {
		warn("Failed to get ESSID for interface %s", iface);
		return UNKNOWN_STR;
	}

	close(sockfd);

	if (strcmp(id, "") == 0)
		return UNKNOWN_STR;
	else
		return id;
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
	fprintf(stderr, "usage: %s [-s]\n", argv0);
	exit(1);
}

int
main(int argc, char *argv[])
{
	struct arg argument;
	struct sigaction act;
	size_t i, len;
	int sflag = 0;
	char status_string[MAXLEN];
	char *element;

	ARGBEGIN {
		case 's':
			sflag = 1;
			break;
		default:
			usage();
	} ARGEND

	if (argc) {
		usage();
	}

	memset(&act, 0, sizeof(act));
	act.sa_handler = sighandler;
	sigaction(SIGINT,  &act, 0);
	sigaction(SIGTERM, &act, 0);

	if (!sflag) {
		dpy = XOpenDisplay(NULL);
		if (!dpy) {
			fprintf(stderr, "slstatus: cannot open display");
			exit(1);
		}
	}

	setlocale(LC_ALL, "");

	while (!done) {
		status_string[0] = '\0';

		for (element = status_string, i = len = 0; i < LEN(args);
		     ++i, element += len) {
			argument = args[i];
			len = snprintf(element, sizeof(status_string)-1 - len,
			               argument.fmt,
			               argument.func(argument.args));
			if (len >= sizeof(status_string)) {
				status_string[sizeof(status_string)-1] = '\0';
				break;
			}
		}

		if (sflag) {
			printf("%s\n", status_string);
		} else {
			XStoreName(dpy, DefaultRootWindow(dpy), status_string);
			XSync(dpy, False);
		}

		if ((UPDATE_INTERVAL - delay) <= 0) {
			delay = 0;
			continue;
		} else {
			sleep(UPDATE_INTERVAL - delay);
			delay = 0;
		}
	}

	if (!sflag) {
		XStoreName(dpy, DefaultRootWindow(dpy), NULL);
		XCloseDisplay(dpy);
	}

	return 0;
}
