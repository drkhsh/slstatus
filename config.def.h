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

/* bar update interval in seconds (smallest value = 1) */
static unsigned int update_interval = 1;

/* mountpoint for diskusage */
static const char mountpath[] = "/home";

/* statusbar
Possible arguments:
- battery (battery percentage)
- cpu_temperature (cpu temperature in degrees)
- cpu usage (cpu usage in percent)
- datetime (date and time)
- diskusage (disk usage in percent)
- ram_usage (ram usage in percent)
- volume (alsa volume and mute status in percent)
- wifi_signal (wifi signal in percent) */
#define FORMATSTRING "wifi %4s | bat %4s | cpu %4s %3s | ram %3s | vol %4s | disk %4s | %3s"
#define ARGUMENTS wifi_signal, battery, cpu_usage, cpu_temperature, ram_usage, volume, diskusage, datetime
