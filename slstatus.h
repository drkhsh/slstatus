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
char *battery_perc(const char *);
char *cpu_perc(const char *);
char *datetime(const char *);
char *disk_free(const char *);
char *disk_perc(const char *);
char *disk_total(const char *);
char *disk_used(const char *);
char *entropy(const char*);
char *gid(const char*);
char *hostname(const char *);
char *ip(const char *);
char *ram_free(const char *);
char *ram_perc(const char *);
char *ram_used(const char *);
char *ram_total(const char *);
char *temp(const char *);
char *uid(const char*);
char *username(const char*);
char *vol_perc(const char *);
char *wifi_perc(const char *);
