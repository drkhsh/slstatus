/* See LICENSE file for copyright and license details. */

#include <alsa/asoundlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <X11/Xlib.h>

#include "config.h"

char *smprintf(char *fmt, ...);

void setstatus(char *str);

char *wifi_signal();
char *battery();
char *cpu_usage();
char *cpu_temperature();
char *ram_usage();
char *volume();
char *datetime();

static Display *dpy;

/* smprintf function */
char *
smprintf(char *fmt, ...)
{
	va_list fmtargs;
	char *ret;
	int len;

	va_start(fmtargs, fmt);
	len = vsnprintf(NULL, 0, fmt, fmtargs);
	va_end(fmtargs);

	ret = malloc(++len);
	if (ret == NULL) {
		fprintf(stderr, "Malloc error.");
		exit(1);
	}

	va_start(fmtargs, fmt);
	vsnprintf(ret, len, fmt, fmtargs);
	va_end(fmtargs);

	return ret;
}

/* set statusbar (WM_NAME) */
void
setstatus(char *str)
{
	XStoreName(dpy, DefaultRootWindow(dpy), str);
	XSync(dpy, False);
}

/* alsa volume percentage */
char *
volume()
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
        snd_mixer_selem_id_set_name(vol_info, channel);
        snd_mixer_selem_id_set_name(mute_info, channel);
        pcm_mixer = snd_mixer_find_selem(handle, vol_info);
        mas_mixer = snd_mixer_find_selem(handle, mute_info);
        snd_mixer_selem_get_playback_volume_range((snd_mixer_elem_t *)pcm_mixer,
                        &min, &max);
        snd_mixer_selem_get_playback_volume((snd_mixer_elem_t *)pcm_mixer,
                        SND_MIXER_SCHN_MONO, &vol);
        snd_mixer_selem_get_playback_switch(mas_mixer, SND_MIXER_SCHN_MONO,
                        &mute);

        if (vol_info)
                snd_mixer_selem_id_free(vol_info);
        if (mute_info)
                snd_mixer_selem_id_free(mute_info);
        if (handle)
                snd_mixer_close(handle);
        
        if (!mute)
            return "mute";
        else
            return smprintf("%d%%", (vol * 100) / max);
}

/* cpu temperature */
char *
cpu_temperature()
{
    int temperature;
    FILE *fp;

    /* open temperature file */
    if (!(fp = fopen(tempfile, "r"))) {
        fprintf(stderr, "Could not open temperature file.\n");
        exit(1);
    }

    /* extract temperature, close file */
    fscanf(fp, "%d", &temperature);
    fclose(fp);

    /* return temperature in degrees */
    return smprintf("%d°C", temperature / 1000);
}

/* wifi percentage */
char *
wifi_signal()
{
    int bufsize = 255;
	int strength;
	char buf[bufsize];
	char *datastart;
    char path_start[16] = "/sys/class/net/";
    char path_end[11] = "/operstate";
    char path[32];
	FILE *fp;

    /* generate the path name */
    strcat(path, path_start);
    strcat(path, wificard);
    strcat(path, path_end);

    /* open wifi file, extract status, close file */
    if(!(fp = fopen(path, "r"))) {
        fprintf(stderr, "Error opening wifi operstate file.");
        exit(1);
    }
    char status[5];
    fgets(status, 5, fp);
    fclose(fp);

    /* check if interface down */
    if(strcmp(status, "up\n") != 0){
        return "n/a";
    }

    /* open wifi file */
    if (!(fp = fopen("/proc/net/wireless", "r"))) {
        fprintf(stderr, "Error opening wireless file.");
        exit(1);
    }

    /* extract the signal strength and close the file */
	fgets(buf, bufsize, fp);
	fgets(buf, bufsize, fp);
	fgets(buf, bufsize, fp);
	if ((datastart = strstr(buf, "wlp3s0:")) != NULL) {
		datastart = strstr(buf, ":");
		sscanf(datastart + 1, " %*d   %d  %*d  %*d        %*d      %*d      %*d      %*d      %*d        %*d",
	           &strength);
	}
	fclose(fp);

    /* return strength in percent */
    return smprintf("%d%%", strength);
}

/* battery percentage */
char *
battery()
{
	int batt_now;
    int batt_full;
    int batt_perc;
	FILE *fp;

    /* open battery now file, extract and close */
	if (!(fp = fopen(batterynowfile, "r"))) {
        fprintf(stderr, "Error opening battery file.");
        exit(1);
    }
	fscanf(fp, "%i", &batt_now);
	fclose(fp);
	
    /* extract battery full file, extract and close */
	if (!(fp = fopen(batteryfullfile, "r"))) {
        fprintf(stderr, "Error opening battery file.");
        exit(1);
    }
	fscanf(fp, "%i", &batt_full);
	fclose(fp);

    /* calculate percent */
	batt_perc = batt_now / (batt_full / 100);

    /* return percent */
	return smprintf("%d%%", batt_perc);
}

/* date and time */
char *
datetime()
{
	time_t tm;
	size_t bufsize = 19;
	char *buf = malloc(bufsize);

    /* get time in format */
	time(&tm);
	if(!strftime(buf, bufsize, timeformat, localtime(&tm))) {
		fprintf(stderr, "Strftime failed.\n");
		exit(1);
	}

    /* return time */
	return buf;
}

/* cpu percentage */
char *
cpu_usage()
{
    FILE *fp;
    long double a[4], b[4], cpu_perc;

    /* open stat file, read and close, do same after 1 second */
	if (!(fp = fopen("/proc/stat","r"))) {
        fprintf(stderr, "Error opening stat file.");
        exit(1);
    }
    fscanf(fp,"%*s %Lf %Lf %Lf %Lf",&a[0],&a[1],&a[2],&a[3]);
    fclose(fp);
    sleep(1);
	if (!(fp = fopen("/proc/stat","r"))) {
        fprintf(stderr, "Error opening stat file.");
        exit(1);
    }
    fscanf(fp,"%*s %Lf %Lf %Lf %Lf",&b[0],&b[1],&b[2],&b[3]);
    fclose(fp);

    /* calculate average in 1 second */
    cpu_perc = 100 * ((b[0]+b[1]+b[2]) - (a[0]+a[1]+a[2])) / ((b[0]+b[1]+b[2]+b[3]) - (a[0]+a[1]+a[2]+a[3]));

    /* return avg cpu percentage */
    return smprintf("%d%%", (int)cpu_perc);
}

/* ram percentage */
char *
ram_usage()
{
    FILE *fp;
    long total, free, available;
    int ram_perc;

    /* read meminfo file, extract and close */
	if (!(fp = fopen("/proc/meminfo", "r"))) {
        fprintf(stderr, "Error opening meminfo file.");
        exit(1);
    }
    fscanf(fp, "MemTotal: %ld kB\n", &total);
    fscanf(fp, "MemFree: %ld kB\n", &free);
    fscanf(fp, "MemAvailable: %ld kB\n", &available);
    fclose(fp);

    /* calculate percentage */
    ram_perc = 100 * (total - available) / total;

    /* return in percent */
    return smprintf("%d%%",ram_perc);
}

int
main()
{
    char status[1024];

    /* open display */
    if (( dpy = XOpenDisplay(0x0)) == NULL ) {
        fprintf(stderr, "Cannot open display!\n");
        exit(1);
    }

    /* return status every second */	
	for (;;) {
		sprintf(status, FORMATSTRING, ARGUMENTS);
		setstatus(status);
	}

    /* close display */
    XCloseDisplay(dpy);

    /* exit successfully */
	return 0;
}
