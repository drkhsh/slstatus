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

typedef char *(*op_fun) (const char *);
struct arg {
	op_fun func;
	const char *format;
	const char *args;
};

static void setstatus(const char *);
static char *smprintf(const char *, ...);
static char *battery_perc(const char *);
static char *cpu_perc(const char *);
static char *datetime(const char *);
static char *disk_free(const char *);
static char *disk_perc(const char *);
static char *disk_total(const char *);
static char *disk_used(const char *);
static char *entropy(const char *);
static char *gid(const char *);
static char *hostname(const char *);
static char *ip(const char *);
static char *load_avg(const char *);
static char *ram_free(const char *);
static char *ram_perc(const char *);
static char *ram_used(const char *);
static char *ram_total(const char *);
static char *run_command(const char *);
static char *temp(const char *);
static char *uid(const char *);
static char *uptime(const char *);
static char *username(const char *);
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
	va_list fmtargs;
	char *ret = NULL;

	va_start(fmtargs, fmt);
	if (vasprintf(&ret, fmt, fmtargs) < 0)
		return NULL;
	va_end(fmtargs);

	return ret;
}

static char *
battery_perc(const char *battery)
{
	int now, full, perc;
	char batterynowfile[64] = "";
	char batteryfullfile[64] = "";
	FILE *fp;

	strlcat(batterynowfile, batterypath, sizeof(batterynowfile));
	strlcat(batterynowfile, battery, sizeof(batterynowfile));
	strlcat(batterynowfile, "/", sizeof(batterynowfile));
	strlcat(batterynowfile, batterynow, sizeof(batterynowfile));

	strlcat(batteryfullfile, batterypath, sizeof(batteryfullfile));
	strlcat(batteryfullfile, battery, sizeof(batteryfullfile));
	strlcat(batteryfullfile, "/", sizeof(batteryfullfile));
	strlcat(batteryfullfile, batteryfull, sizeof(batteryfullfile));

	if (!(fp = fopen(batterynowfile, "r"))) {
		fprintf(stderr, "Error opening battery file: %s.\n", batterynowfile);
		return smprintf(unknowntext);
	}

	fscanf(fp, "%i", &now);
	fclose(fp);

	if (!(fp = fopen(batteryfullfile, "r"))) {
		fprintf(stderr, "Error opening battery file.\n");
		return smprintf(unknowntext);
	}

	fscanf(fp, "%i", &full);
	fclose(fp);

	perc = now / (full / 100);

	return smprintf("%d%%", perc);
}

static char *
cpu_perc(const char *null)
{
	int perc;
	long double a[4], b[4];
	FILE *fp;

	if (!(fp = fopen("/proc/stat","r"))) {
		fprintf(stderr, "Error opening stat file.\n");
		return smprintf(unknowntext);
	}

	fscanf(fp, "%*s %Lf %Lf %Lf %Lf", &a[0], &a[1], &a[2], &a[3]);
	fclose(fp);

	/* wait a second (for avg values) */
	sleep(1);

	if (!(fp = fopen("/proc/stat","r"))) {
		fprintf(stderr, "Error opening stat file.\n");
		return smprintf(unknowntext);
	}

	fscanf(fp, "%*s %Lf %Lf %Lf %Lf", &b[0], &b[1], &b[2], &b[3]);
	fclose(fp);
	perc = 100 * ((b[0]+b[1]+b[2]) - (a[0]+a[1]+a[2])) / ((b[0]+b[1]+b[2]+b[3]) - (a[0]+a[1]+a[2]+a[3]));
	return smprintf("%d%%", perc);
}

static char *
datetime(const char *timeformat)
{
	time_t tm;
	size_t bufsize = 64;
	char *buf = malloc(bufsize);
	if (buf == NULL) {
		fprintf(stderr, "Failed to get date/time.\n");
		return smprintf(unknowntext);
	}

	time(&tm);
	setlocale(LC_TIME, "");
	if (!strftime(buf, bufsize, timeformat, localtime(&tm))) {
		setlocale(LC_TIME, "C");
		free(buf);
		fprintf(stderr, "Strftime failed.\n");
		return smprintf(unknowntext);
	}

	setlocale(LC_TIME, "C");
	char *ret = smprintf("%s", buf);
	free(buf);
	return ret;
}

static char *
disk_free(const char *mountpoint)
{
	struct statvfs fs;

	if (statvfs(mountpoint, &fs) < 0) {
		fprintf(stderr, "Could not get filesystem info.\n");
		return smprintf(unknowntext);
	}
	return smprintf("%f", (float)fs.f_bsize * (float)fs.f_bfree / 1024 / 1024 / 1024);
}

static char *
disk_perc(const char *mountpoint)
{
	int perc = 0;
	struct statvfs fs;

	if (statvfs(mountpoint, &fs) < 0) {
		fprintf(stderr, "Could not get filesystem info.\n");
		return smprintf(unknowntext);
	}

	perc = 100 * (1.0f - ((float)fs.f_bfree / (float)fs.f_blocks));
	return smprintf("%d%%", perc);
}

static char *
disk_total(const char *mountpoint)
{
	struct statvfs fs;

	if (statvfs(mountpoint, &fs) < 0) {
		fprintf(stderr, "Could not get filesystem info.\n");
		return smprintf(unknowntext);
	}

	return smprintf("%f", (float)fs.f_bsize * (float)fs.f_blocks / 1024 / 1024 / 1024);
}

static char *
disk_used(const char *mountpoint)
{
	struct statvfs fs;

	if (statvfs(mountpoint, &fs) < 0) {
		fprintf(stderr, "Could not get filesystem info.\n");
		return smprintf(unknowntext);
	}

	return smprintf("%f", (float)fs.f_bsize * ((float)fs.f_blocks - (float)fs.f_bfree) / 1024 / 1024 / 1024);
}

static char *
entropy(const char *null)
{
	int entropy = 0;
	FILE *fp;

	if (!(fp = fopen("/proc/sys/kernel/random/entropy_avail", "r"))) {
		fprintf(stderr, "Could not open entropy file.\n");
		return smprintf(unknowntext);
	}

	fscanf(fp, "%d", &entropy);
	fclose(fp);
	return smprintf("%d", entropy);
}

static char *
gid(const char *null)
{
	gid_t gid;

	if ((gid = getgid()) < 0) {
		fprintf(stderr, "Could no get gid.\n");
		return smprintf(unknowntext);
	} else
		return smprintf("%d", gid);
	return smprintf(unknowntext);
}

static char *
hostname(const char *null)
{
	char hostname[HOST_NAME_MAX];
	FILE *fp;

	if (!(fp = fopen("/proc/sys/kernel/hostname", "r"))) {
		fprintf(stderr, "Could not open hostname file.\n");
		return smprintf(unknowntext);
	}

	fscanf(fp, "%s\n", hostname);
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
		fprintf(stderr, "Error getting IP address.\n");
		return smprintf(unknowntext);
	}

	/* get the ip address */
	for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr == NULL)
			continue;

		s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in), host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);

		if ((strcmp(ifa->ifa_name, interface) == 0) && (ifa->ifa_addr->sa_family == AF_INET)) {
			if (s != 0) {
				fprintf(stderr, "Error getting IP address.\n");
				return smprintf(unknowntext);
			}
			return smprintf("%s", host);
		}
	}

	/* free the address */
	freeifaddrs(ifaddr);

	return smprintf(unknowntext);
}

static char *
load_avg(const char *null)
{
	double avgs[3];

	if (getloadavg(avgs, 3) < 0) {
		fprintf(stderr, "Error getting load avg.\n");
		return smprintf(unknowntext);
	}

	return smprintf("%.2f %.2f %.2f", avgs[0], avgs[1], avgs[2]);
}

static char *
ram_free(const char *null)
{
	long free;
	FILE *fp;

	if (!(fp = fopen("/proc/meminfo", "r"))) {
		fprintf(stderr, "Error opening meminfo file.\n");
		return smprintf(unknowntext);
	}

	fscanf(fp, "MemFree: %ld kB\n", &free);
	fclose(fp);
	return smprintf("%f", (float)free / 1024 / 1024);
}

static char *
ram_perc(const char *null)
{
	int perc;
	long total, free, buffers, cached;
	FILE *fp;

	if (!(fp = fopen("/proc/meminfo", "r"))) {
		fprintf(stderr, "Error opening meminfo file.\n");
		return smprintf(unknowntext);
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
ram_total(const char *null)
{
	long total;
	FILE *fp;

	if (!(fp = fopen("/proc/meminfo", "r"))) {
		fprintf(stderr, "Error opening meminfo file.\n");
		return smprintf(unknowntext);
	}

	fscanf(fp, "MemTotal: %ld kB\n", &total);
	fclose(fp);
	return smprintf("%f", (float)total / 1024 / 1024);
}

static char *
ram_used(const char *null)
{
	long free, total, buffers, cached, used;
	FILE *fp;

	if (!(fp = fopen("/proc/meminfo", "r"))) {
		fprintf(stderr, "Error opening meminfo file.\n");
		return smprintf(unknowntext);
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
	FILE *fp;
	char buffer[64];

	if (!(fp = popen(command, "r"))) {
		fprintf(stderr, "Could not get command output for: %s.\n", command);
		return smprintf(unknowntext);
	}

	fgets(buffer, sizeof(buffer) - 1, fp);
	pclose(fp);
	for (int i = 0 ; i != sizeof(buffer); i++) {
		if (buffer[i] == '\0') {
			good = 1;
			break;
		}
	}
	if (good)
		buffer[strlen(buffer) - 1] = '\0';
	return smprintf("%s", buffer);
}

static char *
temp(const char *file)
{
	int temperature;
	FILE *fp;

	if (!(fp = fopen(file, "r"))) {
		fprintf(stderr, "Could not open temperature file.\n");
		return smprintf(unknowntext);
	}

	fscanf(fp, "%d", &temperature);
	fclose(fp);
	return smprintf("%d°C", temperature / 1000);
}

static char *
uptime(const char *null)
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
username(const char *null)
{
	register struct passwd *pw;
	register uid_t uid;

	uid = geteuid();
	pw = getpwuid(uid);

	if (pw)
		return smprintf("%s", pw->pw_name);
	else {
		fprintf(stderr, "Could not get username.\n");
		return smprintf(unknowntext);
	}

	return smprintf(unknowntext);
}

static char *
uid(const char *null)
{
	register uid_t uid;

	uid = geteuid();

	if (uid)
		return smprintf("%d", uid);
	else {
		fprintf(stderr, "Could not get uid.\n");
		return smprintf(unknowntext);
	}

	return smprintf(unknowntext);
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
		return smprintf(unknowntext);
	}
	snd_mixer_selem_id_set_name(vol_info, channel);
	snd_mixer_selem_id_set_name(mute_info, channel);
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
	int bufsize = 255;
	int strength;
	char buf[bufsize];
	char *datastart;
	char path[64];
	char status[5];
	char needle[sizeof wificard + 1];
	FILE *fp;

	memset(path, 0, sizeof path);
	strlcat(path, "/sys/class/net/", sizeof(path));
	strlcat(path, wificard, sizeof(path));
	strlcat(path, "/operstate", sizeof(path));

	if(!(fp = fopen(path, "r"))) {
		fprintf(stderr, "Error opening wifi operstate file.\n");
		return smprintf(unknowntext);
	}

	fgets(status, 5, fp);
	fclose(fp);
	if(strcmp(status, "up\n") != 0)
		return smprintf(unknowntext);

	if (!(fp = fopen("/proc/net/wireless", "r"))) {
		fprintf(stderr, "Error opening wireless file.\n");
		return smprintf(unknowntext);
	}

	strlcpy(needle, wificard, sizeof(needle));
	strlcat(needle, ":", sizeof(needle));
	fgets(buf, bufsize, fp);
	fgets(buf, bufsize, fp);
	fgets(buf, bufsize, fp);
	if ((datastart = strstr(buf, needle)) != NULL) {
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
	int sockfd;
	struct iwreq wreq;

	memset(&wreq, 0, sizeof(struct iwreq));
	wreq.u.essid.length = IW_ESSID_MAX_SIZE+1;
	sprintf(wreq.ifr_name, wificard);
	if((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
		fprintf(stderr, "Cannot open socket for interface: %s\n", wificard);
		return smprintf(unknowntext);
	}
	wreq.u.essid.pointer = id;
	if (ioctl(sockfd,SIOCGIWESSID, &wreq) == -1) {
		fprintf(stderr, "Get ESSID ioctl failed for interface %s\n", wificard);
		return smprintf(unknowntext);
	}

	if (strcmp((char *)wreq.u.essid.pointer, "") == 0)
		return smprintf(unknowntext);
	else
		return smprintf("%s", (char *)wreq.u.essid.pointer);
}

int
main(void)
{
	char status_string[1024];
	struct arg argument;

	if (!(dpy = XOpenDisplay(0x0))) {
		fprintf(stderr, "Cannot open display!\n");
		exit(1);
	}

	for (;;) {
		memset(status_string, 0, sizeof(status_string));
		for (size_t i = 0; i < sizeof(args) / sizeof(args[0]); ++i) {
			argument = args[i];
			char *res = argument.func(argument.args);
			char *element = smprintf(argument.format, res);
			if (element == NULL) {
				element = smprintf(unknowntext);
				fprintf(stderr, "Failed to format output.\n");
			}
			strlcat(status_string, element, sizeof(status_string));
			free(res);
			free(element);
		}

		setstatus(status_string);
		sleep(update_interval -1);
	}

	XCloseDisplay(dpy);
	return 0;
}
