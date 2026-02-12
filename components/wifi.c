/* See LICENSE file for copyright and license details. */
#include <ifaddrs.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include "../slstatus.h"
#include "../util.h"

#define RSSI_TO_PERC(rssi) \
			rssi >= -50 ? 100 : \
			(rssi <= -100 ? 0 : \
			(2 * (rssi + 100)))

#if defined(__linux__)
	#include <stdint.h>
	#include <net/if.h>
	#include <linux/netlink.h>
	#include <linux/genetlink.h>
	#include <linux/nl80211.h>

	static int nlsock = -1;
	static uint32_t seq = 1;
	static char resp[4096];

	static char *
	findattr(int attr, const char *p, const char *e, size_t *len)
	{
		while (p < e) {
			struct nlattr nla;
			memcpy(&nla, p, sizeof(nla));
			if (nla.nla_type == attr) {
				*len = nla.nla_len - NLA_HDRLEN;
				return (char *)(p + NLA_HDRLEN);
			}
			p += NLA_ALIGN(nla.nla_len);
		}
		return NULL;
	}

	static uint16_t
	nl80211fam(void)
	{
		static const char family[] = "nl80211";
		static uint16_t id;
		ssize_t r;
		size_t len;
		char ctrl[NLMSG_HDRLEN+GENL_HDRLEN+NLA_HDRLEN+NLA_ALIGN(sizeof(family))] = {0}, *p = ctrl;

		if (id)
			return id;

		memcpy(p, &(struct nlmsghdr){
			.nlmsg_len = sizeof(ctrl),
			.nlmsg_type = GENL_ID_CTRL,
			.nlmsg_flags = NLM_F_REQUEST,
			.nlmsg_seq = seq++,
			.nlmsg_pid = 0,
		}, sizeof(struct nlmsghdr));
		p += NLMSG_HDRLEN;
		memcpy(p, &(struct genlmsghdr){
			.cmd = CTRL_CMD_GETFAMILY,
			.version = 1,
		}, sizeof(struct genlmsghdr));
		p += GENL_HDRLEN;
		memcpy(p, &(struct nlattr){
			.nla_len = NLA_HDRLEN+sizeof(family),
			.nla_type = CTRL_ATTR_FAMILY_NAME,
		}, sizeof(struct nlattr));
		p += NLA_HDRLEN;
		memcpy(p, family, sizeof(family));

		if (nlsock < 0)
			nlsock = socket(AF_NETLINK, SOCK_RAW, NETLINK_GENERIC);
		if (nlsock < 0) {
			warn("socket 'AF_NETLINK':");
			return 0;
		}
		if (send(nlsock, ctrl, sizeof(ctrl), 0) != sizeof(ctrl)) {
			warn("send 'AF_NETLINK':");
			return 0;
		}
		r = recv(nlsock, resp, sizeof(resp), 0);
		if (r < 0) {
			warn("recv 'AF_NETLINK':");
			return 0;
		}
		if ((size_t)r <= sizeof(ctrl))
			return 0;
		p = findattr(CTRL_ATTR_FAMILY_ID, resp + sizeof(ctrl), resp + r, &len);
		if (p && len == 2)
			memcpy(&id, p, 2);

		return id;
	}

	static int
	ifindex(const char *interface)
	{
		static struct ifreq ifr;
		static int ifsock = -1;

		if (ifsock < 0)
			ifsock = socket(AF_UNIX, SOCK_DGRAM, 0);
		if (ifsock < 0) {
			warn("socket 'AF_UNIX':");
			return -1;
		}
		if (strcmp(ifr.ifr_name, interface) != 0) {
			strcpy(ifr.ifr_name, interface);
		}
		if (ioctl(ifsock, SIOCGIFINDEX, &ifr) != 0) {
			warn("ioctl 'SIOCGIFINDEX':");
			return -1;
		}
		return ifr.ifr_ifindex;
	}

	const char *
	wifi_essid(const char *interface)
	{
		uint16_t fam = nl80211fam();
		ssize_t r;
		size_t len;
		char req[NLMSG_HDRLEN+GENL_HDRLEN+NLA_HDRLEN+NLA_ALIGN(4)] = {0}, *p = req;
		int idx = ifindex(interface);
		if (!fam) {
			fprintf(stderr, "nl80211 family not found\n");
			return NULL;
		}
		if (idx < 0) {
			fprintf(stderr, "interface %s not found\n", interface);
			return NULL;
		}

		memcpy(p, &(struct nlmsghdr){
			.nlmsg_len = sizeof(req),
			.nlmsg_type = fam,
			.nlmsg_flags = NLM_F_REQUEST,
			.nlmsg_seq = seq++,
			.nlmsg_pid = 0,
		}, sizeof(struct nlmsghdr));
		p += NLMSG_HDRLEN;
		memcpy(p, &(struct genlmsghdr){
			.cmd = NL80211_CMD_GET_INTERFACE,
			.version = 1,
		}, sizeof(struct genlmsghdr));
		p += GENL_HDRLEN;
		memcpy(p, &(struct nlattr){
			.nla_len = NLA_HDRLEN+4,
			.nla_type = NL80211_ATTR_IFINDEX,
		}, sizeof(struct nlattr));
		p += NLA_HDRLEN;
		memcpy(p, &(uint32_t){idx}, 4);

		if (send(nlsock, req, sizeof(req), 0) != sizeof(req)) {
			warn("send 'AF_NETLINK':");
			return NULL;
		}
		r = recv(nlsock, resp, sizeof(resp), 0);
		if (r < 0) {
			warn("recv 'AF_NETLINK':");
			return NULL;
		}

		if ((size_t)r <= NLMSG_HDRLEN + GENL_HDRLEN)
			return NULL;
		p = findattr(NL80211_ATTR_SSID, resp + NLMSG_HDRLEN + GENL_HDRLEN, resp + r, &len);
		if (p)
			p[len] = 0;

		return p;
	}

	const char *
	wifi_perc(const char *interface)
	{
		static char strength[4];
		struct nlmsghdr hdr;
		uint16_t fam = nl80211fam();
		ssize_t r;
		size_t len;
		char req[NLMSG_HDRLEN + GENL_HDRLEN + NLA_HDRLEN + NLA_ALIGN(4)] = {0}, *p = req, *e;
		int idx = ifindex(interface);

		if (idx < 0) {
			fprintf(stderr, "interface %s not found\n", interface);
			return NULL;
		}

		memcpy(p, &(struct nlmsghdr){
			.nlmsg_len = sizeof(req),
			.nlmsg_type = fam,
			.nlmsg_flags = NLM_F_REQUEST|NLM_F_DUMP,
			.nlmsg_seq = seq++,
			.nlmsg_pid = 0,
		}, sizeof(struct nlmsghdr));
		p += NLMSG_HDRLEN;
		memcpy(p, &(struct genlmsghdr){
			.cmd = NL80211_CMD_GET_STATION,
			.version = 1,
		}, sizeof(struct genlmsghdr));
		p += GENL_HDRLEN;
		memcpy(p, &(struct nlattr){
			.nla_len = NLA_HDRLEN + 4,
			.nla_type = NL80211_ATTR_IFINDEX,
		}, sizeof(struct nlattr));
		p += NLA_HDRLEN;
		memcpy(p, &idx, 4);

		if (send(nlsock, req, sizeof(req), 0) != sizeof(req)) {
			warn("send 'AF_NETLINK':");
			return NULL;
		}

		*strength = 0;
		while (1) {
			r = recv(nlsock, resp, sizeof(resp), 0);
			if (r < 0) {
				warn("recv 'AF_NETLINK':");
				return NULL;
			}
			if ((size_t)r < sizeof(hdr))
				return NULL;

			for (p = resp; p != resp + r && (size_t)(resp + r-p) >= sizeof(hdr); p = e) {
				memcpy(&hdr, p, sizeof(hdr));
				e = resp + r - p < hdr.nlmsg_len ? resp + r : p + hdr.nlmsg_len;

				if (!*strength && hdr.nlmsg_len > NLMSG_HDRLEN+GENL_HDRLEN) {
					p += NLMSG_HDRLEN+GENL_HDRLEN;
					p = findattr(NL80211_ATTR_STA_INFO, p, e, &len);
					if (p)
						p = findattr(NL80211_STA_INFO_SIGNAL_AVG, p, e, &len);
					if (p && len == 1)
						snprintf(strength, sizeof(strength), "%d", RSSI_TO_PERC(*p));
				}
				if (hdr.nlmsg_type == NLMSG_DONE)
					return *strength ? strength : NULL;
			}
		}
	}
#elif defined(__OpenBSD__)
	#include <net/if.h>
	#include <net/if_media.h>
	#include <net80211/ieee80211.h>
	#include <sys/select.h> /* before <sys/ieee80211_ioctl.h> for NBBY */
	#include <net80211/ieee80211_ioctl.h>
	#include <stdlib.h>
	#include <sys/types.h>

	static int
	load_ieee80211_nodereq(const char *interface, struct ieee80211_nodereq *nr)
	{
		struct ieee80211_bssid bssid;
		int sockfd;
		uint8_t zero_bssid[IEEE80211_ADDR_LEN];

		memset(&bssid, 0, sizeof(bssid));
		memset(nr, 0, sizeof(struct ieee80211_nodereq));
		if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
			warn("socket 'AF_INET':");
			return 0;
		}
		strlcpy(bssid.i_name, interface, sizeof(bssid.i_name));
		if ((ioctl(sockfd, SIOCG80211BSSID, &bssid)) < 0) {
			warn("ioctl 'SIOCG80211BSSID':");
			close(sockfd);
			return 0;
		}
		memset(&zero_bssid, 0, sizeof(zero_bssid));
		if (memcmp(bssid.i_bssid, zero_bssid,
		    IEEE80211_ADDR_LEN) == 0) {
			close(sockfd);
			return 0;
		}
		strlcpy(nr->nr_ifname, interface, sizeof(nr->nr_ifname));
		memcpy(&nr->nr_macaddr, bssid.i_bssid, sizeof(nr->nr_macaddr));
		if ((ioctl(sockfd, SIOCG80211NODE, nr)) < 0 && nr->nr_rssi) {
			warn("ioctl 'SIOCG80211NODE':");
			close(sockfd);
			return 0;
		}

		return close(sockfd), 1;
	}

	const char *
	wifi_perc(const char *interface)
	{
		struct ieee80211_nodereq nr;
		int q;

		if (load_ieee80211_nodereq(interface, &nr)) {
			if (nr.nr_max_rssi)
				q = IEEE80211_NODEREQ_RSSI(&nr);
			else
				q = RSSI_TO_PERC(nr.nr_rssi);

			return bprintf("%d", q);
		}

		return NULL;
	}

	const char *
	wifi_essid(const char *interface)
	{
		struct ieee80211_nodereq nr;

		if (load_ieee80211_nodereq(interface, &nr))
			return bprintf("%s", nr.nr_nwid);

		return NULL;
	}
#elif defined(__FreeBSD__)
	#include <net/if.h>
	#include <net80211/ieee80211_ioctl.h>

	int
	load_ieee80211req(int sock, const char *interface, void *data, int type, size_t *len)
	{
		char warn_buf[256];
		struct ieee80211req ireq;
		memset(&ireq, 0, sizeof(ireq));
		ireq.i_type = type;
		ireq.i_data = (caddr_t) data;
		ireq.i_len = *len;

		strlcpy(ireq.i_name, interface, sizeof(ireq.i_name));
		if (ioctl(sock, SIOCG80211, &ireq) < 0) {
			snprintf(warn_buf,  sizeof(warn_buf),
					"ioctl: 'SIOCG80211': %d", type);
			warn(warn_buf);
			return 0;
		}

		*len = ireq.i_len;
		return 1;
	}

	const char *
	wifi_perc(const char *interface)
	{
		union {
			struct ieee80211req_sta_req sta;
			uint8_t buf[24 * 1024];
		} info;
		uint8_t bssid[IEEE80211_ADDR_LEN];
		int rssi_dbm;
		int sockfd;
		size_t len;
		const char *fmt;

		if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
			warn("socket 'AF_INET':");
			return NULL;
		}

		/* Retreive MAC address of interface */
		len = IEEE80211_ADDR_LEN;
		fmt = NULL;
		if (load_ieee80211req(sockfd, interface, &bssid, IEEE80211_IOC_BSSID, &len))
		{
			/* Retrieve info on station with above BSSID */
			memset(&info, 0, sizeof(info));
			memcpy(info.sta.is_u.macaddr, bssid, sizeof(bssid));

			len = sizeof(info);
			if (load_ieee80211req(sockfd, interface, &info, IEEE80211_IOC_STA_INFO, &len)) {
				rssi_dbm = info.sta.info[0].isi_noise +
 					         info.sta.info[0].isi_rssi / 2;

				fmt = bprintf("%d", RSSI_TO_PERC(rssi_dbm));
			}
		}

		close(sockfd);
		return fmt;
	}

	const char *
	wifi_essid(const char *interface)
	{
		char ssid[IEEE80211_NWID_LEN + 1];
		size_t len;
		int sockfd;
		const char *fmt;

		if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
			warn("socket 'AF_INET':");
			return NULL;
		}

		fmt = NULL;
		len = sizeof(ssid);
		memset(&ssid, 0, len);
		if (load_ieee80211req(sockfd, interface, &ssid, IEEE80211_IOC_SSID, &len)) {
			if (len < sizeof(ssid))
				len += 1;
			else
				len = sizeof(ssid);

			ssid[len - 1] = '\0';
			fmt = bprintf("%s", ssid);
		}

		close(sockfd);
		return fmt;
	}
#endif
