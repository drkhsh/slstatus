#include <stdlib.h>

#include "../util.h"

#if defined(__linux__)
	#include <limits.h>

	int get_value(const char* path, const char* type) {
		char value[PATH_MAX];
		if(esnprintf(value, sizeof(value),
					"/sys/class/backlight/%s/%s", path, type) < 0) {
			return -1;
		}
		int val = 0;
		if(pscanf(value, "%d", &val) != 1) {
			return -1;
		}
		return val;
	}

	const char *brightness_perc(const char *brname)
	{
		int actual_br_val = 0, max_br_val = 0;

		if((actual_br_val = get_value(brname, "actual_brightness")) == -1) {
			return NULL;
		};
		if((max_br_val = get_value(brname, "max_brightness")) == -1) {
			return NULL;
		}

		float br_per = (actual_br_val / (float)max_br_val) * 100;
		return bprintf("%d", (int)br_per);
	}

#endif
