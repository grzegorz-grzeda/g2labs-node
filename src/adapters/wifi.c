/* SPDX-License-Identifier: MIT */
/*
 * Wi-Fi adapter: turns the core's Wi-Fi requests into Zephyr Wi-Fi management
 * requests, and Zephyr Wi-Fi and IPv4 events into core events.
 */
#include "adapters.h"

#include <errno.h>
#include <string.h>

#include <zephyr/logging/log.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/dhcpv4.h>
#include <zephyr/net/hostname.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/wifi_mgmt.h>

#include <node/core.h>

#include "../app.h"

LOG_MODULE_REGISTER(wifi_adapter, LOG_LEVEL_INF);

#define WIFI_EVENTS                                                                                \
	(NET_EVENT_WIFI_SCAN_RESULT | NET_EVENT_WIFI_SCAN_DONE | NET_EVENT_WIFI_CONNECT_RESULT |  \
	 NET_EVENT_WIFI_DISCONNECT_RESULT)

/* Security of networks heard in the latest scan, so joins use the right one. */
#define SECURITY_CACHE_SIZE 32

struct heard_network {
	char ssid[NODE_SSID_MAX_LEN + 1];
	enum wifi_security_type security;
};

static struct heard_network heard[SECURITY_CACHE_SIZE];
static size_t heard_count;
static K_MUTEX_DEFINE(heard_lock);

static struct net_mgmt_event_callback wifi_cb;
static struct net_mgmt_event_callback ipv4_cb;

static void remember_security(const char *ssid, enum wifi_security_type security)
{
	k_mutex_lock(&heard_lock, K_FOREVER);
	for (size_t i = 0; i < heard_count; i++) {
		if (strcmp(heard[i].ssid, ssid) == 0) {
			heard[i].security = security;
			goto out;
		}
	}
	if (heard_count < SECURITY_CACHE_SIZE) {
		strcpy(heard[heard_count].ssid, ssid);
		heard[heard_count].security = security;
		heard_count++;
	}
out:
	k_mutex_unlock(&heard_lock);
}

static enum wifi_security_type security_for(const char *ssid, const char *password)
{
	enum wifi_security_type security =
		password[0] == '\0' ? WIFI_SECURITY_TYPE_NONE : WIFI_SECURITY_TYPE_PSK;

	k_mutex_lock(&heard_lock, K_FOREVER);
	for (size_t i = 0; i < heard_count; i++) {
		if (strcmp(heard[i].ssid, ssid) == 0) {
			security = heard[i].security;
			break;
		}
	}
	k_mutex_unlock(&heard_lock);

	switch (security) {
	case WIFI_SECURITY_TYPE_WPA_PSK:
		/* WPA1-only access points: the driver joins them as PSK. */
		return WIFI_SECURITY_TYPE_PSK;
	case WIFI_SECURITY_TYPE_SAE:
		return WIFI_SECURITY_TYPE_SAE_AUTO;
	default:
		return security;
	}
}

static void post_event(enum node_event_type type)
{
	app_post_event(&(struct node_event){.type = type});
}

static void on_network_heard(const struct wifi_scan_result *result)
{
	struct node_event ev = {.type = NODE_EVENT_WIFI_NETWORK_HEARD};
	size_t len = MIN(result->ssid_length, NODE_SSID_MAX_LEN);

	if (len == 0) {
		return; /* hidden network */
	}
	memcpy(ev.network_heard.ssid, result->ssid, len);
	ev.network_heard.rssi = result->rssi;
	remember_security(ev.network_heard.ssid, result->security);
	app_post_event(&ev);
}

static void wifi_event_handler(struct net_mgmt_event_callback *cb, uint64_t mgmt_event,
			       struct net_if *iface)
{
	switch (mgmt_event) {
	case NET_EVENT_WIFI_SCAN_RESULT:
		on_network_heard(cb->info);
		break;
	case NET_EVENT_WIFI_SCAN_DONE:
		post_event(NODE_EVENT_WIFI_SCAN_DONE);
		break;
	case NET_EVENT_WIFI_CONNECT_RESULT: {
		const struct wifi_status *status = cb->info;

		/*
		 * Associated (CONFIG_WIFI_STA_AUTO_DHCPV4 is off): ask for an
		 * address; CONNECTED follows once DHCP gives one.
		 */
		if (status->status == 0) {
			post_event(NODE_EVENT_WIFI_ASSOCIATED);
			net_dhcpv4_start(iface);
		} else {
			LOG_WRN("join failed: %d", status->status);
			post_event(NODE_EVENT_WIFI_DISCONNECTED);
		}
		break;
	}
	case NET_EVENT_WIFI_DISCONNECT_RESULT:
		net_dhcpv4_stop(iface);
		post_event(NODE_EVENT_WIFI_DISCONNECTED);
		break;
	default:
		break;
	}
}

static void ipv4_event_handler(struct net_mgmt_event_callback *cb, uint64_t mgmt_event,
			       struct net_if *iface)
{
	ARG_UNUSED(cb);

	if (mgmt_event == NET_EVENT_IPV4_ADDR_ADD && iface == net_if_get_first_wifi()) {
		post_event(NODE_EVENT_WIFI_CONNECTED);
	}
}

void wifi_adapter_init(void)
{
	net_mgmt_init_event_callback(&wifi_cb, wifi_event_handler, WIFI_EVENTS);
	net_mgmt_add_event_callback(&wifi_cb);
	net_mgmt_init_event_callback(&ipv4_cb, ipv4_event_handler, NET_EVENT_IPV4_ADDR_ADD);
	net_mgmt_add_event_callback(&ipv4_cb);
}

int wifi_adapter_scan(void *ctx)
{
	ARG_UNUSED(ctx);

	struct net_if *iface = net_if_get_first_wifi();
	struct wifi_scan_params params = {0};

	if (iface == NULL) {
		return -ENODEV;
	}

	k_mutex_lock(&heard_lock, K_FOREVER);
	heard_count = 0;
	k_mutex_unlock(&heard_lock);

	LOG_INF("scanning for Known Networks");
	int err = net_mgmt(NET_REQUEST_WIFI_SCAN, iface, &params, sizeof(params));

	if (err) {
		LOG_ERR("scan request failed: %d", err);
	}
	return err;
}

int wifi_adapter_connect(void *ctx, const char *ssid, const char *password)
{
	ARG_UNUSED(ctx);

	struct net_if *iface = net_if_get_first_wifi();

	if (iface == NULL) {
		return -ENODEV;
	}

	struct wifi_connect_req_params params = {
		.ssid = (const uint8_t *)ssid,
		.ssid_length = strlen(ssid),
		.psk = (const uint8_t *)password,
		.psk_length = strlen(password),
		.security = security_for(ssid, password),
		.band = WIFI_FREQ_BAND_UNKNOWN,
		.channel = WIFI_CHANNEL_ANY,
		.mfp = WIFI_MFP_OPTIONAL,
		.timeout = SYS_FOREVER_MS,
	};

	LOG_INF("joining Known Network \"%s\"", ssid);
	int err = net_mgmt(NET_REQUEST_WIFI_CONNECT, iface, &params, sizeof(params));

	if (err) {
		LOG_ERR("join request failed: %d", err);
	}
	return err;
}

int wifi_adapter_disconnect(void *ctx)
{
	ARG_UNUSED(ctx);

	struct net_if *iface = net_if_get_first_wifi();

	if (iface == NULL) {
		return -ENODEV;
	}

	LOG_INF("leaving the current network");
	int err = net_mgmt(NET_REQUEST_WIFI_DISCONNECT, iface, NULL, 0);

	/* -EALREADY: not on a network, which is what was asked for. */
	if (err && err != -EALREADY) {
		LOG_WRN("disconnect request failed: %d", err);
		return err;
	}
	return 0;
}

int wifi_adapter_rssi(void *ctx, int *rssi)
{
	ARG_UNUSED(ctx);

	struct net_if *iface = net_if_get_first_wifi();
	struct wifi_iface_status status = {0};

	if (iface == NULL) {
		return -ENODEV;
	}

	int err = net_mgmt(NET_REQUEST_WIFI_IFACE_STATUS, iface, &status, sizeof(status));

	if (err) {
		return err;
	}
	*rssi = status.rssi;
	return 0;
}

int wifi_adapter_ipv4(void *ctx, uint8_t addr[NODE_IPV4_LEN])
{
	ARG_UNUSED(ctx);

	struct net_if *iface = net_if_get_first_wifi();
	struct net_in_addr *ipv4 =
		iface != NULL ? net_if_ipv4_get_global_addr(iface, NET_ADDR_PREFERRED) : NULL;

	if (ipv4 == NULL) {
		return -ENOENT;
	}
	memcpy(addr, ipv4->s4_addr, sizeof(ipv4->s4_addr));
	return 0;
}

int wifi_adapter_mac(uint8_t mac[NODE_MAC_LEN])
{
	struct net_if *iface = net_if_get_first_wifi();
	struct net_linkaddr *addr = iface != NULL ? net_if_get_link_addr(iface) : NULL;

	if (addr == NULL || addr->len != NODE_MAC_LEN) {
		return -ENODEV;
	}
	memcpy(mac, addr->addr, NODE_MAC_LEN);
	return 0;
}

int wifi_adapter_mdns_name(char *name, size_t len)
{
	int n = snprintf(name, len, "%s.local", net_hostname_get());

	return n < 0 || (size_t)n >= len ? -ENOMEM : 0;
}

int wifi_adapter_link_info(int *rssi, char *ip, size_t ip_len)
{
	struct net_if *iface = net_if_get_first_wifi();

	if (iface == NULL) {
		return -ENODEV;
	}

	int err = wifi_adapter_rssi(NULL, rssi);

	if (err) {
		return err;
	}

	uint8_t addr[NODE_IPV4_LEN];

	if (wifi_adapter_ipv4(NULL, addr) != 0 ||
	    net_addr_ntop(NET_AF_INET, addr, ip, ip_len) == NULL) {
		snprintf(ip, ip_len, "none");
	}
	return 0;
}
