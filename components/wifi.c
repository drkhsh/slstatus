/* See LICENSE file for copyright and license details. */
#include <err.h>
#include <ifaddrs.h>
#include <linux/wireless.h>
#include <sys/socket.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "../util.h"

const char *
wifi_perc(const char *iface)
{
	struct ifaddrs *ifap, *ifa;
	int i, cur;
	float perc;
	int total = 70; /* the max of /proc/net/wireless */
	char *p, *datastart;
	char path[PATH_MAX];
	char status[5];
	FILE *fp;

	if (getifaddrs(&ifap) == -1) {
		warn("Failed to get signal for interface %s", iface);
		return NULL;
	}

	for (ifa = ifap; ifa != NULL; ifa = ifa->ifa_next) {
		if ((strncmp(ifa->ifa_name, iface, strlen(iface)) == 0) && (ifa->ifa_addr->sa_family == AF_INET)) {
            iface = ifa->ifa_name;
            break;
		}
	}

	freeifaddrs(ifap);

	snprintf(path, sizeof(path), "%s%s%s", "/sys/class/net/", iface, "/operstate");
	fp = fopen(path, "r");
	if (fp == NULL) {
		warn("Failed to open file %s", path);
		return NULL;
	}
	p = fgets(status, 5, fp);
	fclose(fp);
	if(!p || strcmp(status, "up\n") != 0) {
		return NULL;
	}

	fp = fopen("/proc/net/wireless", "r");
	if (fp == NULL) {
		warn("Failed to open file /proc/net/wireless");
		return NULL;
	}

	for (i = 0; i < 3; i++) {
		if (!(p = fgets(buf, sizeof(buf) - 1, fp)))
			break;
	}
	fclose(fp);
	if (i < 2 || !p)
		return NULL;

	if ((datastart = strstr(buf, iface)) == NULL)
		return NULL;

	datastart = (datastart+(strlen(iface)+1));
	sscanf(datastart + 1, " %*d   %d  %*d  %*d		  %*d	   %*d		%*d		 %*d	  %*d		 %*d", &cur);

	perc = (float)cur / total * 100.0;

	return bprintf("%.0f", perc);
}

const char *
wifi_essid(const char *iface)
{
	struct ifaddrs *ifap, *ifa;
	static char id[IW_ESSID_MAX_SIZE+1];
	int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	struct iwreq wreq;

	if (getifaddrs(&ifap) == -1) {
		warn("Failed to get ESSID for interface %s", iface);
		return NULL;
	}

	for (ifa = ifap; ifa != NULL; ifa = ifa->ifa_next) {
		if ((strncmp(ifa->ifa_name, iface, strlen(iface)) == 0) && (ifa->ifa_addr->sa_family == AF_INET)) {
            iface = ifa->ifa_name;
            break;
		}
	}

	freeifaddrs(ifap);

	memset(&wreq, 0, sizeof(struct iwreq));
	wreq.u.essid.length = IW_ESSID_MAX_SIZE+1;
	snprintf(wreq.ifr_name, sizeof(wreq.ifr_name), "%s", iface);

	if (sockfd == -1) {
		warn("Failed to get ESSID for interface %s", iface);
		return NULL;
	}
	wreq.u.essid.pointer = id;
	if (ioctl(sockfd,SIOCGIWESSID, &wreq) == -1) {
		warn("Failed to get ESSID for interface %s", iface);
		close(sockfd);
		return NULL;
	}

	close(sockfd);

	if (strcmp(id, "") == 0)
		return NULL;
	else
		return id;
}
