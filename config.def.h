/* See LICENSE file for copyright and license details. */

/* alsa sound */
static const char soundcard[] = "default";
static const char channel[]   = "Master";

/* cpu temperature */
static const char tempfile[] = "/sys/devices/platform/coretemp.0/hwmon/hwmon2/temp1_input";

/* wifi */
static const char wificard[] = "wlp3s0";

/* battery */
static const char batterynowfile[]  = "/sys/class/power_supply/BAT0/energy_now";
static const char batteryfullfile[] = "/sys/class/power_supply/BAT0/energy_full_design";

/* time */
static const char timeformat[] = "%y-%m-%d %H:%M:%S";

/* statusbar */
#define FORMATSTRING "wifi %4s | bat %4s | cpu %4s %3s | ram %3s | vol %4s | %3s"
#define ARGUMENTS wifi_signal, battery, cpu_usage, cpu_temperature, ram_usage, volume, datetime
