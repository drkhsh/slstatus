#include <alsa/asoundlib.h>
#include <err.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <limits.h>
#include <linux/wireless.h>
#include <netdb.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/socket.h>
#include <sys/sysinfo.h>
#include <sys/types.h>
#include <unistd.h>

static void
battery_perc(const char *bat)
{
	int perc;
	FILE *fp;

	ccat(3, "/sys/class/power_supply/", bat, "/capacity");
	fp = fopen(concat, "r");
	if (fp == NULL) {
		warn("Failed to open file %s", concat);
		strlcpy(resp, UNKNOWN_STR, sizeof(resp));
	}
	fscanf(fp, "%i", &perc);
	fclose(fp);

	snprintf(resp, sizeof(resp), "%d%%", perc);
}

static void
battery_state(const char *bat)
{
	char state[12];
	FILE *fp;

	ccat(3, "/sys/class/power_supply/", bat, "/status");
	fp = fopen(concat, "r");
	if (fp == NULL) {
		warn("Failed to open file %s", concat);
		strlcpy(resp, UNKNOWN_STR, sizeof(resp));
	}
	fscanf(fp, "%12s", state);
	fclose(fp);

	if (strcmp(state, "Charging") == 0) {
		strlcpy(resp, "+", sizeof(resp));
	} else if (strcmp(state, "Discharging") == 0) {
		strlcpy(resp, "-", sizeof(resp));
	} else if (strcmp(state, "Full") == 0) {
		strlcpy(resp, "=", sizeof(resp));
	} else {
		strlcpy(resp, "?", sizeof(resp));
	}
}

static void
cpu_perc(void)
{
	int perc;
	long double a[4], b[4];
	FILE *fp;

	fp = fopen("/proc/stat", "r");
	if (fp == NULL) {
		warn("Failed to open file /proc/stat");
		strlcpy(resp, UNKNOWN_STR, sizeof(resp));
	}
	fscanf(fp, "%*s %Lf %Lf %Lf %Lf", &a[0], &a[1], &a[2], &a[3]);
	fclose(fp);

	delay = (UPDATE_INTERVAL - (UPDATE_INTERVAL - 1));
	sleep(delay);

	fp = fopen("/proc/stat", "r");
	if (fp == NULL) {
		warn("Failed to open file /proc/stat");
		strlcpy(resp, UNKNOWN_STR, sizeof(resp));
	}
	fscanf(fp, "%*s %Lf %Lf %Lf %Lf", &b[0], &b[1], &b[2], &b[3]);
	fclose(fp);

	perc = 100 * ((b[0]+b[1]+b[2]) - (a[0]+a[1]+a[2])) / ((b[0]+b[1]+b[2]+b[3]) - (a[0]+a[1]+a[2]+a[3]));
	snprintf(resp, sizeof(resp), "%d%%", perc);
}

static void
disk_free(const char *mnt)
{
	struct statvfs fs;

	if (statvfs(mnt, &fs) < 0) {
		warn("Failed to get filesystem info");
		strlcpy(resp, UNKNOWN_STR, sizeof(resp));
	}

	snprintf(resp, sizeof(resp), "%f", (float)fs.f_bsize * (float)fs.f_bfree / 1024 / 1024 / 1024);
}

static void
disk_perc(const char *mnt)
{
	int perc;
	struct statvfs fs;

	if (statvfs(mnt, &fs) < 0) {
		warn("Failed to get filesystem info");
		strlcpy(resp, UNKNOWN_STR, sizeof(resp));
	}

	perc = 100 * (1.0f - ((float)fs.f_bfree / (float)fs.f_blocks));

	snprintf(resp, sizeof(resp), "%d%%", perc);
}

static void
disk_total(const char *mnt)
{
	struct statvfs fs;

	if (statvfs(mnt, &fs) < 0) {
		warn("Failed to get filesystem info");
		strlcpy(resp, UNKNOWN_STR, sizeof(resp));
	}

	snprintf(resp, sizeof(resp), "%f", (float)fs.f_bsize * (float)fs.f_blocks / 1024 / 1024 / 1024);
}

static void
disk_used(const char *mnt)
{
	struct statvfs fs;

	if (statvfs(mnt, &fs) < 0) {
		warn("Failed to get filesystem info");
		strlcpy(resp, UNKNOWN_STR, sizeof(resp));
	}

	snprintf(resp, sizeof(resp), "%f", (float)fs.f_bsize * ((float)fs.f_blocks - (float)fs.f_bfree) / 1024 / 1024 / 1024);
}

static void
entropy(void)
{
	int num;
	FILE *fp;

	fp= fopen("/proc/sys/kernel/random/entropy_avail", "r");
	if (fp == NULL) {
		warn("Failed to open file /proc/sys/kernel/random/entropy_avail");
		strlcpy(resp, UNKNOWN_STR, sizeof(resp));
	}
	fscanf(fp, "%d", &num);
	fclose(fp);

	snprintf(resp, sizeof(resp), "%d", num);
}

static void
ram_free(void)
{
	long free;
	FILE *fp;

	fp = fopen("/proc/meminfo", "r");
	if (fp == NULL) {
		warn("Failed to open file /proc/meminfo");
		strlcpy(resp, UNKNOWN_STR, sizeof(resp));
	}
	fscanf(fp, "MemFree: %ld kB\n", &free);
	fclose(fp);

	snprintf(resp, sizeof(resp), "%f", (float)free / 1024 / 1024);
}

static void
ram_perc(void)
{
	long total, free, buffers, cached;
	FILE *fp;

	fp = fopen("/proc/meminfo", "r");
	if (fp == NULL) {
		warn("Failed to open file /proc/meminfo");
		strlcpy(resp, UNKNOWN_STR, sizeof(resp));
	}
	fscanf(fp, "MemTotal: %ld kB\n", &total);
	fscanf(fp, "MemFree: %ld kB\n", &free);
	fscanf(fp, "MemAvailable: %ld kB\nBuffers: %ld kB\n", &buffers, &buffers);
	fscanf(fp, "Cached: %ld kB\n", &cached);
	fclose(fp);

	snprintf(resp, sizeof(resp), "%d%%", (int)(100 * ((total - free) - (buffers + cached)) / total));
}

static void
ram_total(void)
{
	long total;
	FILE *fp;

	fp = fopen("/proc/meminfo", "r");
	if (fp == NULL) {
		warn("Failed to open file /proc/meminfo");
		strlcpy(resp, UNKNOWN_STR, sizeof(resp));
	}
	fscanf(fp, "MemTotal: %ld kB\n", &total);
	fclose(fp);

	snprintf(resp, sizeof(resp), "%f", (float)total / 1024 / 1024);
}

static void
ram_used(void)
{
	long free, total, buffers, cached;
	FILE *fp;

	fp = fopen("/proc/meminfo", "r");
	if (fp == NULL) {
		warn("Failed to open file /proc/meminfo");
		strlcpy(resp, UNKNOWN_STR, sizeof(resp));
	}
	fscanf(fp, "MemTotal: %ld kB\n", &total);
	fscanf(fp, "MemFree: %ld kB\n", &free);
	fscanf(fp, "MemAvailable: %ld kB\nBuffers: %ld kB\n", &buffers, &buffers);
	fscanf(fp, "Cached: %ld kB\n", &cached);
	fclose(fp);

	snprintf(resp, sizeof(resp), "%f", (float)(total - free - buffers - cached) / 1024 / 1024);
}

static void
temp(const char *file)
{
	int temp;
	FILE *fp;

	fp = fopen(file, "r");
	if (fp == NULL) {
		warn("Failed to open file %s", file);
		strlcpy(resp, UNKNOWN_STR, sizeof(resp));
	}
	fscanf(fp, "%d", &temp);
	fclose(fp);

	snprintf(resp, sizeof(resp), "%d°C", temp / 1000);
}

static void
uptime(void)
{
	struct sysinfo info;
	int h = 0;
	int m = 0;

	sysinfo(&info);
	h = info.uptime / 3600;
	m = (info.uptime - h * 3600 ) / 60;

	snprintf(resp, sizeof(resp), "%dh %dm", h, m);
}

static void
vol_perc(const char *card)
{
	long int vol, max, min;
	snd_mixer_t *handle;
	snd_mixer_elem_t *elem;
	snd_mixer_selem_id_t *s_elem;

	snd_mixer_open(&handle, 0);
	snd_mixer_attach(handle, card);
	snd_mixer_selem_register(handle, NULL, NULL);
	snd_mixer_load(handle);
	snd_mixer_selem_id_malloc(&s_elem);
	snd_mixer_selem_id_set_name(s_elem, "Master");
	elem = snd_mixer_find_selem(handle, s_elem);

	if (elem == NULL) {
		snd_mixer_selem_id_free(s_elem);
		snd_mixer_close(handle);
		warn("Failed to get volume percentage for %s", card);
		strlcpy(resp, UNKNOWN_STR, sizeof(resp));
	}

	snd_mixer_handle_events(handle);
	snd_mixer_selem_get_playback_volume_range(elem, &min, &max);
	snd_mixer_selem_get_playback_volume(elem, 0, &vol);

	snd_mixer_selem_id_free(s_elem);
	snd_mixer_close(handle);

	snprintf(resp, sizeof(resp), "%d%%", (int)((uint_fast16_t)(vol * 100) / max));
}

static void
wifi_perc(const char *iface)
{
	int perc;
	char buf[255];
	char *datastart;
	char status[5];
	FILE *fp;

	ccat(3, "/sys/class/net/", iface, "/operstate");
	fp = fopen(concat, "r");
	if (fp == NULL) {
		warn("Failed to open file %s", concat);
		strlcpy(resp, UNKNOWN_STR, sizeof(resp));
	}
	fgets(status, 5, fp);
	fclose(fp);
	if(strcmp(status, "up\n") != 0) {
		strlcpy(resp, UNKNOWN_STR, sizeof(resp));
	}

	fp = fopen("/proc/net/wireless", "r");
	if (fp == NULL) {
		warn("Failed to open file /proc/net/wireless");
		strlcpy(resp, UNKNOWN_STR, sizeof(resp));
	}
	ccat(2, iface, ":");
	fgets(buf, sizeof(buf), fp);
	fgets(buf, sizeof(buf), fp);
	fgets(buf, sizeof(buf), fp);
	fclose(fp);

	datastart = strstr(buf, concat);
	if (datastart != NULL) {
		datastart = strstr(buf, ":");
		sscanf(datastart + 1, " %*d   %d  %*d  %*d"
				      "		  %*d	  "
				      " %*d		%*d"
				      "		 %*d	  %*d"
				      "		 %*d", &perc);
	}

	snprintf(resp, sizeof(resp), "%d%%", perc);
}

static void
wifi_essid(const char *iface)
{
	char id[IW_ESSID_MAX_SIZE+1];
	int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	struct iwreq wreq;

	memset(&wreq, 0, sizeof(struct iwreq));
	wreq.u.essid.length = IW_ESSID_MAX_SIZE+1;
	sprintf(wreq.ifr_name, iface);
	if (sockfd == -1) {
		warn("Failed to get ESSID for interface %s", iface);
		strlcpy(resp, UNKNOWN_STR, sizeof(resp));
	}
	wreq.u.essid.pointer = id;
	if (ioctl(sockfd,SIOCGIWESSID, &wreq) == -1) {
		warn("Failed to get ESSID for interface %s", iface);
		strlcpy(resp, UNKNOWN_STR, sizeof(resp));
	}

	close(sockfd);

	if (strcmp((char *)wreq.u.essid.pointer, "") == 0)
		strlcpy(resp, UNKNOWN_STR, sizeof(resp));
	else
		snprintf(resp, sizeof(resp), "%s", (char *)wreq.u.essid.pointer);
}
