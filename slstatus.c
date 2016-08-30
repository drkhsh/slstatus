/* See LICENSE file for copyright and license details. */

#include <alsa/asoundlib.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <limits.h>
#include <linux/wireless.h>
#include <locale.h>
#include <netdb.h>
#include <pwd.h>
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

typedef char *(*op_fun)();
struct arg {
	op_fun func;
	const char *format;
	const char *args;
};

static void setstatus(const char *);
static char *smprintf(const char *, ...);
static char *battery_perc(const char *);
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

static Display *dpy;

#include "config.h"

static void
setstatus(const char *str)
{
	/* set WM_NAME via X11 */
	XStoreName(dpy, DefaultRootWindow(dpy), str);
	XSync(dpy, False);
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
	int now, full, perc;
	char batterynowfile[64];
	char batteryfullfile[64];
	FILE *fp = fopen(batterynowfile, "r");

	strlcpy(batterynowfile, BATTERY_PATH, sizeof(batterynowfile));
	strlcat(batterynowfile, battery, sizeof(batterynowfile));
	strlcat(batterynowfile, "/", sizeof(batterynowfile));
	strlcat(batterynowfile, BATTERY_NOW, sizeof(batterynowfile));

	strlcpy(batteryfullfile, BATTERY_PATH, sizeof(batteryfullfile));
	strlcat(batteryfullfile, battery, sizeof(batteryfullfile));
	strlcat(batteryfullfile, "/", sizeof(batteryfullfile));
	strlcat(batteryfullfile, BATTERY_FULL, sizeof(batteryfullfile));

	if (fp == NULL ) {
		fprintf(stderr, "Error opening battery file: %s: %s\n",
						batterynowfile,
						strerror(errno));
		return smprintf(UNKNOWN_STR);
	}

	fscanf(fp, "%i", &now);
	fclose(fp);

	fp = fopen(batteryfullfile, "r");
	if (fp == NULL) {
		fprintf(stderr, "Error opening battery file: %s\n",
						strerror(errno));
		return smprintf(UNKNOWN_STR);
	}

	fscanf(fp, "%i", &full);
	fclose(fp);

	perc = now / (full / 100);

	return smprintf("%d%%", perc);
}

static char *
cpu_perc(void)
{
	int perc;
	long double a[4], b[4];
	FILE *fp = fopen("/proc/stat","r");

	if (fp == NULL) {
		fprintf(stderr, "Error opening stat file: %s\n",
						strerror(errno));
		return smprintf(UNKNOWN_STR);
	}

	fscanf(fp, "%*s %Lf %Lf %Lf %Lf", &a[0], &a[1], &a[2], &a[3]);
	fclose(fp);

	/* wait a second (for avg values) */
	sleep(1);

	fp = fopen("/proc/stat","r");
	if (fp == NULL) {
		fprintf(stderr, "Error opening stat file: %s\n",
						strerror(errno));
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
		fprintf(stderr, "Could not get filesystem info: %s\n",
						strerror(errno));
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
		fprintf(stderr, "Could not get filesystem info: %s\n",
						strerror(errno));
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
		fprintf(stderr, "Could not get filesystem info: %s\n",
						strerror(errno));
		return smprintf(UNKNOWN_STR);
	}

	return smprintf("%f", (float)fs.f_bsize * (float)fs.f_blocks / 1024 / 1024 / 1024);
}

static char *
disk_used(const char *mountpoint)
{
	struct statvfs fs;

	if (statvfs(mountpoint, &fs) < 0) {
		fprintf(stderr, "Could not get filesystem info: %s\n",
						strerror(errno));
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
		fprintf(stderr, "Could not open entropy file.\n");
		return smprintf(UNKNOWN_STR);
	}

	fscanf(fp, "%d", &entropy);
	fclose(fp);
	return smprintf("%d", entropy);
}

static char *
gid(void)
{
	gid_t gid = getgid();
	return smprintf("%d", gid);
}

static char *
hostname(void)
{
	char hostname[HOST_NAME_MAX];
	FILE *fp = fopen("/proc/sys/kernel/hostname", "r");

	if (fp == NULL) {
		fprintf(stderr, "Could not open hostname file: %s\n",
						strerror(errno));
		return smprintf(UNKNOWN_STR);
	}

	fgets(hostname, sizeof(hostname), fp);
	/* FIXME: needs improvement */
	memset(&hostname[strlen(hostname)-1], '\0',
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
		fprintf(stderr, "Error getting IP address: %s\n",
						strerror(errno));
		return smprintf(UNKNOWN_STR);
	}

	/* get the ip address */
	for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr == NULL)
			continue;

		s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in), host, NI_MAXHOST,
								NULL, 0, NI_NUMERICHOST);

		if ((strcmp(ifa->ifa_name, interface) == 0) && (ifa->ifa_addr->sa_family == AF_INET)) {
			if (s != 0) {
				fprintf(stderr, "Error getting IP address.\n");
				return smprintf(UNKNOWN_STR);
			}
			return smprintf("%s", host);
		}
	}

	/* free the address */
	freeifaddrs(ifaddr);

	return smprintf(UNKNOWN_STR);
}

static char *
load_avg(void)
{
	double avgs[3];

	if (getloadavg(avgs, 3) < 0) {
		fprintf(stderr, "Error getting load avg.\n");
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
		fprintf(stderr, "Error opening meminfo file: %s\n",
						strerror(errno));
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
		fprintf(stderr, "Error opening meminfo file: %s\n",
						strerror(errno));
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
		fprintf(stderr, "Error opening meminfo file: %s\n",
						strerror(errno));
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
		fprintf(stderr, "Error opening meminfo file: %s\n",
						strerror(errno));
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
	char buffer[64];

	if (fp == NULL) {
		fprintf(stderr, "Could not get command output for: %s: %s\n",
						command, strerror(errno));
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
		fprintf(stderr, "Could not open temperature file: %s\n",
							strerror(errno));
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
	register struct passwd *pw;
	register uid_t uid;

	uid = geteuid();
	pw = getpwuid(uid);

	if (pw)
		return smprintf("%s", pw->pw_name);
	else {
		fprintf(stderr, "Could not get username: %s\n",
					strerror(errno));
		return smprintf(UNKNOWN_STR);
	}

	return smprintf(UNKNOWN_STR);
}

static char *
uid(void)
{
	/* FIXME: WHY USE register modifier? */
	register uid_t uid;

	uid = geteuid();

	if (uid)
		return smprintf("%d", uid);
	else {
		fprintf(stderr, "Could not get uid.\n");
		return smprintf(UNKNOWN_STR);
	}

	return smprintf(UNKNOWN_STR);
}


static char *
vol_perc(const char *soundcard)
{
	int mute = 0;
	long vol = 0, max = 0, min = 0;
	snd_mixer_t *handle;
	snd_mixer_elem_t *pcm_mixer, *mas_mixer;
	snd_mixer_selem_id_t *vol_info, *mute_info;

	snd_mixer_open(&handle, 0);
	snd_mixer_attach(handle, soundcard);
	snd_mixer_selem_register(handle, NULL, NULL);
	snd_mixer_load(handle);

	snd_mixer_selem_id_malloc(&vol_info);
	snd_mixer_selem_id_malloc(&mute_info);
	if (vol_info == NULL || mute_info == NULL) {
		fprintf(stderr, "Could not get alsa volume.\n");
		return smprintf(UNKNOWN_STR);
	}
	snd_mixer_selem_id_set_name(vol_info, ALSA_CHANNEL);
	snd_mixer_selem_id_set_name(mute_info, ALSA_CHANNEL);
	pcm_mixer = snd_mixer_find_selem(handle, vol_info);
	mas_mixer = snd_mixer_find_selem(handle, mute_info);

	snd_mixer_selem_get_playback_volume_range((snd_mixer_elem_t *)pcm_mixer, &min, &max);
	snd_mixer_selem_get_playback_volume((snd_mixer_elem_t *)pcm_mixer, SND_MIXER_SCHN_MONO, &vol);
	snd_mixer_selem_get_playback_switch(mas_mixer, SND_MIXER_SCHN_MONO, &mute);

	if (vol_info)
		snd_mixer_selem_id_free(vol_info);
	if (mute_info)
		snd_mixer_selem_id_free(mute_info);
	if (handle)
		snd_mixer_close(handle);

	if (!mute)
		return smprintf("mute");
	else
		return smprintf("%d%%", (vol * 100) / max);
}

static char *
wifi_perc(const char *wificard)
{
	int strength;
	char buf[255];
	char *datastart;
	char path[64];
	char status[5];
	char needle[strlen(wificard)+2];
	FILE *fp = fopen(path, "r");

	strlcpy(path, "/sys/class/net/", sizeof(path));
	strlcat(path, wificard, sizeof(path));
	strlcat(path, "/operstate", sizeof(path));

	if(fp == NULL) {
		fprintf(stderr, "Error opening wifi operstate file: %s\n",
							strerror(errno));
		return smprintf(UNKNOWN_STR);
	}

	fgets(status, 5, fp);
	fclose(fp);
	if(strcmp(status, "up\n") != 0)
		return smprintf(UNKNOWN_STR);

	fp = fopen("/proc/net/wireless", "r");
	if (fp == NULL) {
		fprintf(stderr, "Error opening wireless file: %s\n",
						strerror(errno));
		return smprintf(UNKNOWN_STR);
	}

	strlcpy(needle, wificard, sizeof(needle));
	strlcat(needle, ":", sizeof(needle));
	fgets(buf, sizeof(buf), fp);
	fgets(buf, sizeof(buf), fp);
	fgets(buf, sizeof(buf), fp);

	datastart = strstr(buf, needle);
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
	if(sockfd == -1) {
		fprintf(stderr, "Cannot open socket for interface: %s: %s\n",
						wificard, strerror(errno));
		return smprintf(UNKNOWN_STR);
	}
	wreq.u.essid.pointer = id;
	if (ioctl(sockfd,SIOCGIWESSID, &wreq) == -1) {
		fprintf(stderr, "Get ESSID ioctl failed for interface %s: %s\n",
						wificard, strerror(errno));
		return smprintf(UNKNOWN_STR);
	}

	if (strcmp((char *)wreq.u.essid.pointer, "") == 0)
		return smprintf(UNKNOWN_STR);
	else
		return smprintf("%s", (char *)wreq.u.essid.pointer);
}

int
main(void)
{
	size_t i;
	char status_string[4096];
	char *res, *element;
	struct arg argument;

	dpy = XOpenDisplay(0x0);
	if (!dpy) {
		fprintf(stderr, "Cannot open display!\n");
		exit(1);
	}

	for (;;) {
		memset(status_string, 0, sizeof(status_string));
		for (i = 0; i < sizeof(args) / sizeof(args[0]); ++i) {
			argument = args[i];
			if (argument.args == NULL)
				res = argument.func();
			else
				res = argument.func(argument.args);
			element = smprintf(argument.format, res);
			if (element == NULL) {
				element = smprintf(UNKNOWN_STR);
				fprintf(stderr, "Failed to format output.\n");
			}
			strlcat(status_string, element, sizeof(status_string));
			free(res);
			free(element);
		}

		setstatus(status_string);
		sleep(UPDATE_INTERVAL -1);
	}

	XCloseDisplay(dpy);
	return 0;
}
