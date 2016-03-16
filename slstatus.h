/* See LICENSE file for copyright and license details. */

/* global variables */
static Display *dpy;

/* statusbar configuration type and struct */
typedef char *(*op_fun) (const char *);
struct arg {
    op_fun func;
    const char *format;
    const char *args;
};

/* functions */
void setstatus(const char *);
char *smprintf(const char *, ...);
char *get_battery(const char *);
char *get_cpu_temperature(const char *);
char *get_cpu_usage(const char *);
char *get_datetime(const char *);
char *get_diskusage(const char *);
char *get_ram_usage(const char *);
char *get_volume(const char *);
char *get_wifi_signal(const char *);
