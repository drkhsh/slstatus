#include <sys/socket.h>
#include <sys/types.h>
#include <ifaddrs.h>
#include <limits.h>
#include <netdb.h>
#include <pwd.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

static void
username(void)
{
	uid_t uid = geteuid();
	struct passwd *pw = getpwuid(uid);

	if (pw == NULL) {
		warn("Failed to get username");
		strlcpy(resp, UNKNOWN_STR, sizeof(resp));
	}

	snprintf(resp, sizeof(resp), "%s", pw->pw_name);
}

static void
uid(void)
{
	snprintf(resp, sizeof(resp), "%d", geteuid());
}

static void
run_command(const char *cmd)
{
	FILE *fp;
	char buf[64] = UNKNOWN_STR;

	fp = popen(cmd, "r");
	if (fp == NULL) {
		warn("Failed to get command output for %s", cmd);
		strlcpy(resp, UNKNOWN_STR, sizeof(resp));
	}
	fgets(buf, sizeof(buf), fp);
	buf[sizeof(buf)-1] = '\0';
	pclose(fp);

	snprintf(resp, sizeof(resp), "%s", buf);
}

static void
load_avg(void)
{
	double avgs[3];

	if (getloadavg(avgs, 3) < 0) {
		warnx("Failed to get the load avg");
		strlcpy(resp, UNKNOWN_STR, sizeof(resp));
	}

	snprintf(resp, sizeof(resp), "%.2f %.2f %.2f", avgs[0], avgs[1], avgs[2]);
}

static void
ip(const char *iface)
{
	struct ifaddrs *ifaddr, *ifa;
	int s;
	char host[NI_MAXHOST];

	if (getifaddrs(&ifaddr) == -1) {
		warn("Failed to get IP address for interface %s", iface);
		strlcpy(resp, UNKNOWN_STR, sizeof(resp));
	}

	for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr == NULL) {
			continue;
		}
		s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in), host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
		if ((strcmp(ifa->ifa_name, iface) == 0) && (ifa->ifa_addr->sa_family == AF_INET)) {
			if (s != 0) {
				warnx("Failed to get IP address for interface %s", iface);
				strlcpy(resp, UNKNOWN_STR, sizeof(resp));
			}
			snprintf(resp, sizeof(resp), "%s", host);
			return;
		}
	}

	freeifaddrs(ifaddr);

	strlcpy(resp, UNKNOWN_STR, sizeof(resp));
}

static void
hostname(void)
{
	char *lhost = "";
	gethostname(lhost, HOST_NAME_MAX);
	strlcpy(resp, lhost, sizeof(resp));
}

static void
gid(void)
{
	snprintf(resp, sizeof(resp), "%d", getgid());
}

static void
datetime(const char *fmt)
{
	time_t t;
	char str[80];

	t = time(NULL);
	if (strftime(str, sizeof(str), fmt, localtime(&t)) == 0) {
		strlcpy(resp, UNKNOWN_STR, sizeof(resp));
	}

	strlcpy(resp, str, sizeof(resp));
}
