#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "ac_interface.h"
#include "capwap_message.h"
#include "ac_mainloop.h"
#include "ac_manager.h"
#include "uci_config.h"

#define MAX_MAC_ADDR_LENGTH 18

static char *raw_mac_to_string(uint8_t mac[6])
{
	char *s = malloc(MAX_MAC_ADDR_LENGTH);

	if (!s)
		return NULL;
	memset(s, 0, MAX_MAC_ADDR_LENGTH);
	snprintf(s, MAX_MAC_ADDR_LENGTH, "%02x_%02x_%02x_%02x_%02x_%02x",
			 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	return s;
}

int ac_init_dev_attr(struct capwap_wtp *wtp)
{
	struct device_attr *attr = wtp->attr;

	if (!attr)
		return -EINVAL;

	attr->hardware_version = strdup(wtp->descriptor.hardware_version);
	attr->firmware_version = strdup(wtp->descriptor.software_version);
	attr->model = strdup(wtp->board_data.model);
	attr->mac = raw_mac_to_string(wtp->board_data.mac);
	if (!attr->hardware_version || !attr->firmware_version || !attr->model || !attr->mac)
		return -ENOMEM;

	attr->status = 0;
	attr->ap_alive_time = CW_NEIGHBORDEAD_INTERVAL_DEFAULT;
	// TODO: What's client idle time?
	attr->client_idle_time = 10;
	attr->led = 1;
	attr->updating = 0;

	return 0;
}

int ac_update_dev_attr(struct capwap_wtp *wtp)
{
	struct device_attr *attr;

	if (!wtp || !wtp->attr)
		return -EINVAL;

	attr = wtp->attr;
	FREE_STRING(attr->hardware_version);
	attr->hardware_version = strdup(wtp->descriptor.hardware_version);
	FREE_STRING(attr->firmware_version);
	attr->firmware_version = strdup(wtp->descriptor.software_version);
	FREE_STRING(attr->model);
	attr->model = strdup(wtp->board_data.model);
	if (!attr->hardware_version || !attr->firmware_version || !attr->model)
		return -ENOMEM;

	attr->led = wtp->vendor_spec.led_status;
	attr->updating = wtp->vendor_spec.updata_status;

	return 0;
}

/**
 * Send WLAN_CONFIGURATION_REQUEST to add wlan, normally we should call this function
 * twice to add both 2G and 5G.
 * This function only send request, and use wifi_info->setted to mark the request has
 * been sent out.
 */
int ac_add_wlan(struct capwap_wtp *wtp)
{
	struct wifi_info *wifi_info;
	struct wifi_attr *wifi_attr;
	struct cw_ctrlmsg *add_wlan;
	int band;

again:
	if (wtp->wifi[WIFI_2G].setted && wtp->wifi[WIFI_5G].setted) {
		wtp->state = RUN;
		return 0;
	}

	if (!wtp->wifi[WIFI_2G].setted)
		band = WIFI_2G;
	else if (!wtp->wifi[WIFI_5G].setted)
		band = WIFI_5G;

	wifi_info = &wtp->wifi[band];
	wifi_attr = &wtp->attr->wifi.band[band];
	if (!wifi_attr->enabled) {
		wifi_info->setted = 1;
		goto again;
	}

	wifi_info->wlan_id = WLAN_IFACE_ID;
	wifi_info->capability = BIT(15) | BIT(5);
	if (wifi_attr->encryption)
		wifi_info->capability |= BIT(11);
	// KEY, Group TSC, QoS not used
	// Auth Type: 0 open system
	// Mac Mode: 0 LocalMAC
	// Tunnel Mode: 2 802.11 Tunnel
	wifi_info->tunnel_mode = 2;
	// Suppress SSID:1 no
	wifi_info->suppress_ssid = 1;
	wifi_info->channel = wifi_attr->channel;
	strncpy(wifi_info->ssid, wifi_attr->ssid, sizeof(wifi_info->ssid));
	if (wifi_attr->country_code) {
		memcpy(wifi_info->country_code, wifi_attr->country_code, sizeof(wifi_info->country_code));
	} else {
		wifi_info->country_code[0] = 'C';
		wifi_info->country_code[1] = 'N';
	}
	if (wifi_attr->encryption && wifi_attr->password) {
		wifi_info->wpa = 2;
		strncpy(wifi_info->password, wifi_attr->password, sizeof(wifi_info->password));
	}
	add_wlan = cwmsg_ctrlmsg_new(CW_MSG_TYPE_VALUE_WLAN_CONFIGURATION_REQUEST, wtp->seq_num++);
	cwmsg_assemble_add_wlan(add_wlan, wifi_info, wtp);
	return -EAGAIN;
}
