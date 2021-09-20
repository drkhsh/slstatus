#include <stdlib.h>

#include "../util.h"

#if defined(__linux__)
	#include <limits.h>

	const char *brightness_perc(const char *brname)
	{
		char path[PATH_MAX];
		if(esnprintf(path, sizeof(path),
					"/sys/class/backlight/%s/actual_brightness", brname) < 0) {
			return NULL;
		}
		int br_val = 0;
		if(pscanf(path, "%d", &br_val) != 1) {
			return "here";
		}

		float br_per = (br_val / 255.00) * 100;
		return bprintf("%d", (int)br_per);
	}

#endif
