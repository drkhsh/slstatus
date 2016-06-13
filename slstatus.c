/* See LICENSE file for copyright and license details. */

/* global libraries */
#include <alsa/asoundlib.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <limits.h>
#include <locale.h>
#include <netdb.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#include <X11/Xlib.h>

/* local headers */
#include "slstatus.h"
#include "config.h"

/* set statusbar */
void
setstatus(const char *str)
{
    /* set WM_NAME via X11 */
    XStoreName(dpy, DefaultRootWindow(dpy), str);
    XSync(dpy, False);
}

/* smprintf function */
char *
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

/* battery percentage */
char *
battery_perc(const char *battery)
{
    int now, full, perc;
    char batterynowfile[64] = "";
    char batteryfullfile[64] = "";
    FILE *fp;

    /* generate battery nowfile path */
    strcat(batterynowfile, batterypath);
    strcat(batterynowfile, battery);
    strcat(batterynowfile, "/");
    strcat(batterynowfile, batterynow);

    /* generate battery fullfile path */
    strcat(batteryfullfile, batterypath);
    strcat(batteryfullfile, battery);
    strcat(batteryfullfile, "/");
    strcat(batteryfullfile, batteryfull);

    /* open battery now file */
    if (!(fp = fopen(batterynowfile, "r"))) {
        fprintf(stderr, "Error opening battery file.%s",batterynowfile);
        return smprintf("n/a");
    }

    /* read value */
    fscanf(fp, "%i", &now);

    /* close battery now file */
    fclose(fp);

    /* open battery full file */
    if (!(fp = fopen(batteryfullfile, "r"))) {
        fprintf(stderr, "Error opening battery file.");
        return smprintf("n/a");
    }

    /* read value */
    fscanf(fp, "%i", &full);

    /* close battery full file */
    fclose(fp);

    /* calculate percent */
    perc = now / (full / 100);

    /* return perc as string */
    return smprintf("%d%%", perc);
}

/* cpu percentage */
char *
cpu_perc(const char *null)
{
    int perc;
    long double a[4], b[4];
    FILE *fp;

    /* open stat file */
    if (!(fp = fopen("/proc/stat","r"))) {
        fprintf(stderr, "Error opening stat file.");
        return smprintf("n/a");
    }

    /* read values */
    fscanf(fp, "%*s %Lf %Lf %Lf %Lf", &a[0], &a[1], &a[2], &a[3]);

    /* close stat file */
    fclose(fp);

    /* wait a second (for avg values) */
    sleep(1);

    /* open stat file */
    if (!(fp = fopen("/proc/stat","r"))) {
        fprintf(stderr, "Error opening stat file.");
        return smprintf("n/a");
    }

    /* read values */
    fscanf(fp, "%*s %Lf %Lf %Lf %Lf", &b[0], &b[1], &b[2], &b[3]);

    /* close stat file */
    fclose(fp);

    /* calculate avg in this second */
    perc = 100 * ((b[0]+b[1]+b[2]) - (a[0]+a[1]+a[2])) / ((b[0]+b[1]+b[2]+b[3]) - (a[0]+a[1]+a[2]+a[3]));

    /* return perc as string */
    return smprintf("%d%%", perc);
}

/* date and time */
char *
datetime(const char *timeformat)
{
    time_t tm;
    size_t bufsize = 64;
    char *buf = malloc(bufsize);

    /* get time in format */
    time(&tm);
    setlocale(LC_TIME, "");
    if(!strftime(buf, bufsize, timeformat, localtime(&tm))) {
        setlocale(LC_TIME, "C");
        fprintf(stderr, "Strftime failed.\n");
        return smprintf("n/a");
    }

    setlocale(LC_TIME, "C");
    /* return time */
    char *ret = smprintf("%s", buf);
    free(buf);
    return ret;
}

/* disk free */
char *
disk_free(const char *mountpoint)
{
    struct statvfs fs;

    /* try to open mountpoint */
    if (statvfs(mountpoint, &fs) < 0) {
        fprintf(stderr, "Could not get filesystem info.\n");
        return smprintf("n/a");
    }

    /* return free */
    return smprintf("%f", (float)fs.f_bsize * (float)fs.f_bfree / 1024 / 1024 / 1024);
}

/* disk usage percentage */
char *
disk_perc(const char *mountpoint)
{
    int perc = 0;
    struct statvfs fs;

    /* try to open mountpoint */
    if (statvfs(mountpoint, &fs) < 0) {
        fprintf(stderr, "Could not get filesystem info.\n");
        return smprintf("n/a");
    }

    /* calculate percent */
    perc = 100 * (1.0f - ((float)fs.f_bfree / (float)fs.f_blocks));

    /* return perc */
    return smprintf("%d%%", perc);
}

/* disk total */
char *
disk_total(const char *mountpoint)
{
    struct statvfs fs;

    /* try to open mountpoint */
    if (statvfs(mountpoint, &fs) < 0) {
        fprintf(stderr, "Could not get filesystem info.\n");
        return smprintf("n/a");
    }

    /* return total */
    return smprintf("%f", (float)fs.f_bsize * (float)fs.f_blocks / 1024 / 1024 / 1024);
}

/* disk used */
char *
disk_used(const char *mountpoint)
{
    struct statvfs fs;

    /* try to open mountpoint */
    if (statvfs(mountpoint, &fs) < 0) {
        fprintf(stderr, "Could not get filesystem info.\n");
        return smprintf("n/a");
    }

    /* return used */
    return smprintf("%f", (float)fs.f_bsize * ((float)fs.f_blocks - (float)fs.f_bfree) / 1024 / 1024 / 1024);
}

/* entropy available */
char *
entropy(const char *null)
{
    int entropy = 0;
    FILE *fp;

    /* open entropy file */
    if (!(fp = fopen("/proc/sys/kernel/random/entropy_avail", "r"))) {
        fprintf(stderr, "Could not open entropy file.\n");
        return smprintf("n/a");
    }

    /* extract entropy */
    fscanf(fp, "%d", &entropy);

    /* close entropy file */
    fclose(fp);

    /* return entropy */
    return smprintf("%d", entropy);
}

/* gid */
char *
gid(const char *null)
{
    gid_t gid;

    if ((gid = getgid()) < 0) {
        fprintf(stderr, "Could no get gid.");
        return smprintf("n/a");
    } else {
        return smprintf("%d", gid);
    }

    return smprintf("n/a");
}

/* hostname */
char *
hostname(const char *null)
{
    char hostname[HOST_NAME_MAX];
    FILE *fp;

    /* open hostname file */
    if (!(fp = fopen("/proc/sys/kernel/hostname", "r"))) {
        fprintf(stderr, "Could not open hostname file.\n");
        return smprintf("n/a");
    }

    /* extract hostname */
    fscanf(fp, "%s\n", hostname);

    /* close hostname file */
    fclose(fp);

    /* return entropy */
    return smprintf("%s", hostname);
}

/* ip address */
char *
ip(const char *interface)
{
    struct ifaddrs *ifaddr, *ifa;
    int s;
    char host[NI_MAXHOST];

    /* check if getting ip address works */
    if (getifaddrs(&ifaddr) == -1)
    {
        fprintf(stderr, "Error getting IP address.");
        return smprintf("n/a");
    }

    /* get the ip address */
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
    {
        if (ifa->ifa_addr == NULL)
            continue;

        s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in), host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);

        if ((strcmp(ifa->ifa_name, interface) == 0) && (ifa->ifa_addr->sa_family == AF_INET))
        {
            if (s != 0)
            {
                fprintf(stderr, "Error getting IP address.");
                return smprintf("n/a");
            }
            return smprintf("%s", host);
        }
    }

    /* free the address */
    freeifaddrs(ifaddr);

    /* return n/a if nothing works */
    return smprintf("n/a");
}

/* ram free */
char *
ram_free(const char *null)
{
    long free;
    FILE *fp;

    /* open meminfo file */
    if (!(fp = fopen("/proc/meminfo", "r"))) {
        fprintf(stderr, "Error opening meminfo file.");
        return smprintf("n/a");
    }

    /* read the values */
    fscanf(fp, "MemTotal: %*d kB\n");
    fscanf(fp, "MemFree: %ld kB\n", &free);

    /* close meminfo file */
    fclose(fp);

    /* return free ram as string */
    return smprintf("%f", (float)free / 1024 / 1024);
}

/* ram percentage */
char *
ram_perc(const char *null)
{
    int perc;
    long total, free, buffers, cached;
    FILE *fp;

    /* open meminfo file */
    if (!(fp = fopen("/proc/meminfo", "r"))) {
        fprintf(stderr, "Error opening meminfo file.");
        return smprintf("n/a");
    }

    /* read the values */
    fscanf(fp, "MemTotal: %ld kB\n", &total);
    fscanf(fp, "MemFree: %ld kB\n", &free);
    fscanf(fp, "MemAvailable: %ld kB\nBuffers: %ld kB\n", &buffers, &buffers);
    fscanf(fp, "Cached: %ld kB\n", &cached);

    /* close meminfo file */
    fclose(fp);

    /* calculate percentage */
    perc = 100 * ((total - free) - (buffers + cached)) / total;

    /* return perc as string */
    return smprintf("%d%%", perc);
}

/* ram total */
char *
ram_total(const char *null)
{
    long total;
    FILE *fp;

    /* open meminfo file */
    if (!(fp = fopen("/proc/meminfo", "r"))) {
        fprintf(stderr, "Error opening meminfo file.");
        return smprintf("n/a");
    }

    /* read the values */
    fscanf(fp, "MemTotal: %ld kB\n", &total);

    /* close meminfo file */
    fclose(fp);

    /* return total ram as string */
    return smprintf("%f", (float)total / 1024 / 1024);
}

/* ram used */
char *
ram_used(const char *null)
{
    long free, total, buffers, cached, used;
    FILE *fp;

    /* open meminfo file */
    if (!(fp = fopen("/proc/meminfo", "r"))) {
        fprintf(stderr, "Error opening meminfo file.");
        return smprintf("n/a");
    }

    /* read the values */
    fscanf(fp, "MemTotal: %ld kB\n", &total);
    fscanf(fp, "MemFree: %ld kB\n", &free);
    fscanf(fp, "MemAvailable: %ld kB\nBuffers: %ld kB\n", &buffers, &buffers);
    fscanf(fp, "Cached: %ld kB\n", &cached);

    /* close meminfo file */
    fclose(fp);

    /* calculate used */
    used = total - free - buffers - cached;

    /* return used ram as string */
    return smprintf("%f", (float)used / 1024 / 1024);
}

/* temperature */
char *
temp(const char *file)
{
    int temperature;
    FILE *fp;

    /* open temperature file */
    if (!(fp = fopen(file, "r"))) {
        fprintf(stderr, "Could not open temperature file.\n");
        return smprintf("n/a");
    }

    /* extract temperature */
    fscanf(fp, "%d", &temperature);

    /* close temperature file */
    fclose(fp);

    /* return temperature in degrees */
    return smprintf("%d°C", temperature / 1000);
}

/* username */
char *
username(const char *null)
{
    register struct passwd *pw;
    register uid_t uid;

    /* get the values */
    uid = geteuid ();
    pw = getpwuid (uid);

    /* if it worked, return */
    if (pw) {
        return smprintf("%s", pw->pw_name);
    }
    else {
        fprintf(stderr, "Could not get username.\n");
        return smprintf("n/a");
    }

    return smprintf("n/a");
}

/* uid */
char *
uid(const char *null)
{
    register uid_t uid;

    /* get the values */
    uid = geteuid ();

    /* if it worked, return */
    if (uid) {
        return smprintf("%d", uid);
    }
    else {
        fprintf(stderr, "Could not get uid.\n");
        return smprintf("n/a");
    }

    return smprintf("n/a");
}


/* alsa volume percentage */
char *
vol_perc(const char *soundcard)
{
    int mute = 0;
    long vol = 0, max = 0, min = 0;

    /* get volume from alsa */
    snd_mixer_t *handle;
    snd_mixer_elem_t *pcm_mixer, *mas_mixer;
    snd_mixer_selem_id_t *vol_info, *mute_info;
    snd_mixer_open(&handle, 0);
    snd_mixer_attach(handle, soundcard);
    snd_mixer_selem_register(handle, NULL, NULL);
    snd_mixer_load(handle);
    snd_mixer_selem_id_malloc(&vol_info);
    snd_mixer_selem_id_malloc(&mute_info);
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

    /* return the string (mute) */
    if (!mute)
        return smprintf("mute");
    else
        return smprintf("%d%%", (vol * 100) / max);
}

/* wifi percentage */
char *
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

    /* generate the path name */
    memset(path, 0, sizeof path);
    strcat(path, "/sys/class/net/");
    strcat(path, wificard);
    strcat(path, "/operstate");

    /* open wifi file */
    if(!(fp = fopen(path, "r"))) {
        fprintf(stderr, "Error opening wifi operstate file.");
        return smprintf("n/a");
    }

    /* read the status */
    fgets(status, 5, fp);

    /* close wifi file */
    fclose(fp);

    /* check if interface down */
    if(strcmp(status, "up\n") != 0){
        return smprintf("n/a");
    }

    /* open wifi file */
    if (!(fp = fopen("/proc/net/wireless", "r"))) {
        fprintf(stderr, "Error opening wireless file.");
        return smprintf("n/a");
    }

    /* extract the signal strength */
    strcpy(needle, wificard);
    strcat(needle, ":");
    fgets(buf, bufsize, fp);
    fgets(buf, bufsize, fp);
    fgets(buf, bufsize, fp);
    if ((datastart = strstr(buf, needle)) != NULL) {
        datastart = strstr(buf, ":");
        sscanf(datastart + 1, " %*d   %d  %*d  %*d        %*d      %*d      %*d      %*d      %*d        %*d", &strength);
    }

    /* close wifi file */
    fclose(fp);

    /* return strength in percent */
    return smprintf("%d%%", strength);
}

/* main function */
int
main()
{
    char status_string[1024];
    struct arg argument;

    /* try to open display */
    if (!(dpy = XOpenDisplay(0x0))) {
        fprintf(stderr, "Cannot open display!\n");
        exit(1);
    }

    /* return status every interval */
    for (;;) {
        /* clear the string */
        memset(status_string, 0, sizeof(status_string));

        /* generate status_string */
        for (size_t i = 0; i < sizeof(args) / sizeof(args[0]); ++i) {
            argument = args[i];
            char *res = argument.func(argument.args);
            char *element = smprintf(argument.format, res);
            strcat(status_string, element);
            free(res);
            free(element);
        }

        /* return the statusbar */
        setstatus(status_string);

        /* wait, "update_interval - 1" because of get_cpu_usage() which uses 1 second */
        sleep(update_interval -1);
    }

    /* close display */
    XCloseDisplay(dpy);

    /* exit successfully */
    return 0;
}
