/* See LICENSE file for copyright and license details. */

#include <alsa/asoundlib.h>
#include <err.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <limits.h>
#include <linux/wireless.h>
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
#include <sys/sysinfo.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <X11/Xlib.h>

#undef strlcat
#undef strlcpy

#include "strlcat.h"
#include "strlcpy.h"
#include "concat.h"

char concat[];

struct arg {
	char *(*func)();
	const char *format;
	const char *args;
};

static char *smprintf(const char *, ...);
static char *battery_perc(const char *);
static char *battery_state(const char *);
static char *cpu_perc(void);
static char *datetime(const char *);
static char *disk_free(const char *);
static char *disk_perc(const char *);
static char *disk_total(const char *);
static char *disk_used(const char *);
static char *entropy(void);
static char *gid(void);
static char *hostname(void);
static char *ip(const char *);
static char *load_avg(void);
static char *ram_free(void);
static char *ram_perc(void);
static char *ram_used(void);
static char *ram_total(void);
static char *run_command(const char *);
static char *temp(const char *);
static char *uid(void);
static char *uptime(void);
static char *username(void);
static char *vol_perc(const char *);
static char *wifi_perc(const char *);
static char *wifi_essid(const char *);
static void sighandler(const int);

static unsigned short int delay, done;
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
		perror("malloc");
		exit(1);
	}

	va_start(ap, fmt);
	vsnprintf(ret, len, fmt, ap);
	va_end(ap);

	return ret;
}

static char *
battery_perc(const char *battery)
{
	int perc;
	FILE *fp;

	ccat(3, "/sys/class/power_supply/", battery, "/capacity");
	fp = fopen(concat, "r");
	if (fp == NULL) {
		warn("Error opening battery file: %s", concat);
		return smprintf(UNKNOWN_STR);
	}
	fscanf(fp, "%i", &perc);
	fclose(fp);

	return smprintf("%d%%", perc);
}

static char *
battery_state(const char *battery)
{
	char *state[12]; 
	FILE *fp;

	if (!state) {
		warn("Failed to get battery state.");
		return smprintf(UNKNOWN_STR);
	}


	ccat(3, "/sys/class/power_supply/", battery, "/status");
	fp = fopen(concat, "r");
	if (fp == NULL) {
		warn("Error opening battery file: %s", concat);
		return smprintf(UNKNOWN_STR);
	}
	fscanf(fp, "%12s", state);
	fclose(fp);

	if (strcmp(state, "Charging") == 0)
		return smprintf("+");
	else if (strcmp(state, "Discharging") == 0)
		return smprintf("-");
	else if (strcmp(state, "Full") == 0)
		return smprintf("=");
	else
		return smprintf("?");
}

static char *
cpu_perc(void)
{
	int perc;
	long double a[4], b[4];
	FILE *fp = fopen("/proc/stat","r");

	if (fp == NULL) {
		warn("Error opening stat file");
		return smprintf(UNKNOWN_STR);
	}

	fscanf(fp, "%*s %Lf %Lf %Lf %Lf", &a[0], &a[1], &a[2], &a[3]);
	fclose(fp);

	delay = (UPDATE_INTERVAL - (UPDATE_INTERVAL - 1));
	sleep(delay);

	fp = fopen("/proc/stat","r");
	if (fp == NULL) {
		warn("Error opening stat file");
		return smprintf(UNKNOWN_STR);
	}

	fscanf(fp, "%*s %Lf %Lf %Lf %Lf", &b[0], &b[1], &b[2], &b[3]);
	fclose(fp);
	perc = 100 * ((b[0]+b[1]+b[2]) - (a[0]+a[1]+a[2])) / ((b[0]+b[1]+b[2]+b[3]) - (a[0]+a[1]+a[2]+a[3]));

	return smprintf("%d%%", perc);
}

static char *
datetime(const char *timeformat)
{
	time_t t;
	char timestr[80];

	t = time(NULL);
	if (strftime(timestr, sizeof(timestr), timeformat, localtime(&t)) == 0)
		return smprintf(UNKNOWN_STR);

	return smprintf("%s", timestr);
}

static char *
disk_free(const char *mountpoint)
{
	struct statvfs fs;

	if (statvfs(mountpoint, &fs) < 0) {
		warn("Could not get filesystem info");
		return smprintf(UNKNOWN_STR);
	}

	return smprintf("%f", (float)fs.f_bsize * (float)fs.f_bfree / 1024 / 1024 / 1024);
}

static char *
disk_perc(const char *mountpoint)
{
	int perc = 0;
	struct statvfs fs;

	if (statvfs(mountpoint, &fs) < 0) {
		warn("Could not get filesystem info");
		return smprintf(UNKNOWN_STR);
	}

	perc = 100 * (1.0f - ((float)fs.f_bfree / (float)fs.f_blocks));

	return smprintf("%d%%", perc);
}

static char *
disk_total(const char *mountpoint)
{
	struct statvfs fs;

	if (statvfs(mountpoint, &fs) < 0) {
		warn("Could not get filesystem info");
		return smprintf(UNKNOWN_STR);
	}

	return smprintf("%f", (float)fs.f_bsize * (float)fs.f_blocks / 1024 / 1024 / 1024);
}

static char *
disk_used(const char *mountpoint)
{
	struct statvfs fs;

	if (statvfs(mountpoint, &fs) < 0) {
		warn("Could not get filesystem info");
		return smprintf(UNKNOWN_STR);
	}

	return smprintf("%f", (float)fs.f_bsize * ((float)fs.f_blocks - (float)fs.f_bfree) / 1024 / 1024 / 1024);
}

static char *
entropy(void)
{
	int entropy = 0;
	FILE *fp = fopen("/proc/sys/kernel/random/entropy_avail", "r");

	if (fp == NULL) {
		warn("Could not open entropy file");
		return smprintf(UNKNOWN_STR);
	}

	fscanf(fp, "%d", &entropy);
	fclose(fp);

	return smprintf("%d", entropy);
}

static char *
gid(void)
{
	return smprintf("%d", getgid());
}

static char *
hostname(void)
{
	char hostname[HOST_NAME_MAX];
	FILE *fp = fopen("/proc/sys/kernel/hostname", "r");

	if (fp == NULL) {
		warn("Could not open hostname file");
		return smprintf(UNKNOWN_STR);
	}

	fgets(hostname, sizeof(hostname), fp);
	/* FIXME: needs improvement */
	memset(&hostname[strlen(hostname)], '\0',
		sizeof(hostname) - strlen(hostname));
	fclose(fp);

	return smprintf("%s", hostname);
}

static char *
ip(const char *interface)
{
	struct ifaddrs *ifaddr, *ifa;
	int s;
	char host[NI_MAXHOST];

	if (getifaddrs(&ifaddr) == -1) {
		warn("Error getting IP address");
		return smprintf(UNKNOWN_STR);
	}

	for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr == NULL)
			continue;

		s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in), host, NI_MAXHOST,
								NULL, 0, NI_NUMERICHOST);

		if ((strcmp(ifa->ifa_name, interface) == 0) && (ifa->ifa_addr->sa_family == AF_INET)) {
			if (s != 0) {
				warnx("Error getting IP address.");
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
		warnx("Error getting load avg.");
		return smprintf(UNKNOWN_STR);
	}

	return smprintf("%.2f %.2f %.2f", avgs[0], avgs[1], avgs[2]);
}

static char *
ram_free(void)
{
	long free;
	FILE *fp = fopen("/proc/meminfo", "r");

	if (fp == NULL) {
		warn("Error opening meminfo file");
		return smprintf(UNKNOWN_STR);
	}

	fscanf(fp, "MemFree: %ld kB\n", &free);
	fclose(fp);

	return smprintf("%f", (float)free / 1024 / 1024);
}

static char *
ram_perc(void)
{
	int perc;
	long total, free, buffers, cached;
	FILE *fp = fopen("/proc/meminfo", "r");

	if (fp == NULL) {
		warn("Error opening meminfo file");
		return smprintf(UNKNOWN_STR);
	}

	fscanf(fp, "MemTotal: %ld kB\n", &total);
	fscanf(fp, "MemFree: %ld kB\n", &free);
	fscanf(fp, "MemAvailable: %ld kB\nBuffers: %ld kB\n", &buffers, &buffers);
	fscanf(fp, "Cached: %ld kB\n", &cached);

	fclose(fp);
	perc = 100 * ((total - free) - (buffers + cached)) / total;

	return smprintf("%d%%", perc);
}

static char *
ram_total(void)
{
	long total;
	FILE *fp = fopen("/proc/meminfo", "r");

	if (fp == NULL) {
		warn("Error opening meminfo file");
		return smprintf(UNKNOWN_STR);
	}

	fscanf(fp, "MemTotal: %ld kB\n", &total);
	fclose(fp);

	return smprintf("%f", (float)total / 1024 / 1024);
}

static char *
ram_used(void)
{
	long free, total, buffers, cached, used;
	FILE *fp = fopen("/proc/meminfo", "r");

	if (fp == NULL) {
		warn("Error opening meminfo file");
		return smprintf(UNKNOWN_STR);
	}

	fscanf(fp, "MemTotal: %ld kB\n", &total);
	fscanf(fp, "MemFree: %ld kB\n", &free);
	fscanf(fp, "MemAvailable: %ld kB\nBuffers: %ld kB\n", &buffers, &buffers);
	fscanf(fp, "Cached: %ld kB\n", &cached);

	fclose(fp);
	used = total - free - buffers - cached;

	return smprintf("%f", (float)used / 1024 / 1024);
}

static char *
run_command(const char* command)
{
	int good;
	FILE *fp = popen(command, "r");
	char buffer[64] = "";

	if (fp == NULL) {
		warn("Could not get command output for: %s", command);
		return smprintf(UNKNOWN_STR);
	}

	fgets(buffer, sizeof(buffer)-1, fp);
	pclose(fp);
	for (int i = 0 ; i != sizeof(buffer); i++) {
		if (buffer[i] == '\0') {
			good = 1;
			break;
		}
	}
	if (good)
		buffer[strlen(buffer)-1] = '\0';

	return smprintf("%s", buffer);
}

static char *
temp(const char *file)
{
	int temperature;
	FILE *fp = fopen(file, "r");

	if (fp == NULL) {
		warn("Could not open temperature file");
		return smprintf(UNKNOWN_STR);
	}

	fscanf(fp, "%d", &temperature);
	fclose(fp);

	return smprintf("%d°C", temperature / 1000);
}

static char *
uptime(void)
{
	struct sysinfo info;
	int hours = 0;
	int minutes = 0;

	sysinfo(&info);
	hours = info.uptime / 3600;
	minutes = (info.uptime - hours * 3600 ) / 60;

	return smprintf("%dh %dm", hours, minutes);
}

static char *
username(void)
{
	uid_t uid = geteuid();
	struct passwd *pw = getpwuid(uid);

	if (pw == NULL) {
		warn("Could not get username");
		return smprintf(UNKNOWN_STR);
	}

	return smprintf("%s", pw->pw_name);
}

static char *
uid(void)
{
	return smprintf("%d", geteuid());
}


static char *
vol_perc(const char *soundcard)
{
	long int vol, max, min;
	snd_mixer_t *handle;
	snd_mixer_elem_t *elem;
	snd_mixer_selem_id_t *s_elem;

	snd_mixer_open(&handle, 0);
	snd_mixer_attach(handle, soundcard);
	snd_mixer_selem_register(handle, NULL, NULL);
	snd_mixer_load(handle);
	snd_mixer_selem_id_malloc(&s_elem);
	snd_mixer_selem_id_set_name(s_elem, "Master");
	elem = snd_mixer_find_selem(handle, s_elem);

	if (elem == NULL) {
		snd_mixer_selem_id_free(s_elem);
		snd_mixer_close(handle);
		warn("Failed to get volume percentage for: %s", soundcard);
		return smprintf(UNKNOWN_STR);
	}

	snd_mixer_handle_events(handle);
	snd_mixer_selem_get_playback_volume_range(elem, &min, &max);
	snd_mixer_selem_get_playback_volume(elem, 0, &vol);

	snd_mixer_selem_id_free(s_elem);
	snd_mixer_close(handle);

	return smprintf("%d%%", ((uint_fast16_t)(vol * 100) / max));
}

static char *
wifi_perc(const char *wificard)
{
	int strength;
	char buf[255];
	char *datastart;
	char status[5];
	FILE *fp;

	ccat(3, "/sys/class/net/", wificard, "/operstate");

	fp = fopen(concat, "r");

	if (fp == NULL) {
		warn("Error opening wifi operstate file");
		return smprintf(UNKNOWN_STR);
	}

	fgets(status, 5, fp);
	fclose(fp);
	if(strcmp(status, "up\n") != 0)
		return smprintf(UNKNOWN_STR);

	fp = fopen("/proc/net/wireless", "r");
	if (fp == NULL) {
		warn("Error opening wireless file");
		return smprintf(UNKNOWN_STR);
	}

	ccat(2, wificard, ":");
	fgets(buf, sizeof(buf), fp);
	fgets(buf, sizeof(buf), fp);
	fgets(buf, sizeof(buf), fp);

	datastart = strstr(buf, concat);
	if (datastart != NULL) {
		datastart = strstr(buf, ":");
		sscanf(datastart + 1, " %*d   %d  %*d  %*d		  %*d	   %*d		%*d		 %*d	  %*d		 %*d", &strength);
	}

	fclose(fp);

	return smprintf("%d%%", strength);
}

static char *
wifi_essid(const char *wificard)
{
	char id[IW_ESSID_MAX_SIZE+1];
	int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	struct iwreq wreq;

	memset(&wreq, 0, sizeof(struct iwreq));
	wreq.u.essid.length = IW_ESSID_MAX_SIZE+1;
	sprintf(wreq.ifr_name, wificard);
	if (sockfd == -1) {
		warn("Cannot open socket for interface: %s", wificard);
		return smprintf(UNKNOWN_STR);
	}
	wreq.u.essid.pointer = id;
	if (ioctl(sockfd,SIOCGIWESSID, &wreq) == -1) {
		warn("Get ESSID ioctl failed for interface %s", wificard);
		return smprintf(UNKNOWN_STR);
	}

	close(sockfd);

	if (strcmp((char *)wreq.u.essid.pointer, "") == 0)
		return smprintf(UNKNOWN_STR);
	else
		return smprintf("%s", (char *)wreq.u.essid.pointer);
}

static void
sighandler(const int signo)
{
	if (signo == SIGTERM || signo == SIGINT)
		done = 1;
}

int
main(void)
{
	unsigned short int i;
	char status_string[4096];
	char *res, *element, *status_old;
	struct arg argument;
	struct sigaction act;

	memset(&act, 0, sizeof(act));
	act.sa_handler = sighandler;
	sigaction(SIGINT,  &act, 0);
	sigaction(SIGTERM, &act, 0);

	dpy = XOpenDisplay(NULL);

	while (!done) {
		status_string[0] = '\0';
		for (i = 0; i < sizeof(args) / sizeof(args[0]); ++i) {
			argument = args[i];
			if (argument.args == NULL)
				res = argument.func();
			else
				res = argument.func(argument.args);
			element = smprintf(argument.format, res);
			if (element == NULL) {
				element = smprintf(UNKNOWN_STR);
				warnx("Failed to format output.");
			}
			strlcat(status_string, element, sizeof(status_string));
			free(res);
			free(element);
		}
		XStoreName(dpy, DefaultRootWindow(dpy), status_string);
		XSync(dpy, False);
		/*
		 * subtract delay time spend in function
		 * calls from the actual global delay time
		 */
		sleep(UPDATE_INTERVAL - delay);
		delay = 0;
	}

	XStoreName(dpy, DefaultRootWindow(dpy), NULL);
	XSync(dpy, False);

	XCloseDisplay(dpy);

	return 0;
}
