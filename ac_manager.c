#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "ac_interface.h"
#include "capwap_message.h"
#include "ac_mainloop.h"
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
