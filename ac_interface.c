#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <arpa/inet.h>
#include <errno.h>
#include <event2/event.h>
#include <json-c/json.h>

#include "ac_mainloop.h"
#include "network.h"
#include "capwap_message.h"
#include "ac_interface.h"
#include "CWLog.h"

struct json_operation {
    char *name;
    int (*act)(void *, json_object *);
};

struct json_object *json_object_object_get_old(struct json_object *obj, const char *name)
{
	struct json_object *sub;
	return json_object_object_get_ex(obj, name, &sub) ? sub : NULL;
}

static const char *get_string_of_json_key(json_object *json, const char *key)
{
	struct json_object *key_obj = json_object_object_get_old(json, key);
	if (!key_obj)
		return NULL;

	return json_object_get_string(key_obj);
}

static struct device_attr *find_device_attr_by_json(json_object *json)
{
	const char *dev_mac = get_string_of_json_key(json, "device");

	return find_dev_attr_by_mac(dev_mac);
}

static int set_device_to_group(void *arg, json_object *msg)
{
	struct capwap_wtp *wtp = arg;
	struct device_attr *dev_attr;
	struct group_attr *group_attr;
	int i, err = 0;

	dev_attr = find_device_attr_by_json(msg);
	if (!dev_attr)
		return -EINVAL;

	group_attr = find_group_attr_by_name(
		get_string_of_json_key(msg, "name_of_group"));
	if (!group_attr)
		return -EINVAL;

	dev_attr_mutex_lock(dev_attr);
	set_group_attr_to_device(dev_attr, group_attr);
	group_attr_free(group_attr);
	for (i = 0; i < WIFI_NUM; i++) {
		if (ac_update_wifi(wtp, i))
			err = -EFAULT;
	}
	if (err) {
		// Reload origin config.
		reload_dev_attr(dev_attr);
		dev_attr_mutex_unlock(dev_attr);
	} else {
		// Restart AP to use new config, or save config if the device is off-line.
		if (dev_attr->status == 1) {
			dev_attr_mutex_unlock(dev_attr);
			err = ac_reset_device(wtp);
		} else {
			save_device_config(dev_attr, 1);
			err = dev_attr_mutex_unlock(dev_attr);
		}
	}

	return err;
}

static void parse_json_dev_config(json_object *config_obj, struct device_attr *attr)
{
	if (!attr || !config_obj)
		return;

	json_object_object_foreach(config_obj, key, json_obj) {
		if (!strcmp(key, "name")) {
			FREE_STRING(attr->name);
			attr->name = strdup(json_object_get_string(json_obj));
		} else if (!strcmp(key, "ap_alive_time")) {
			attr->ap_alive_time = json_object_get_int(json_obj);
		} else if (!strcmp(key, "client_alive_time")) {
			attr->client_alive_time = json_object_get_int(json_obj);
		} else if (!strcmp(key, "client_idle_time")) {
			attr->client_idle_time = json_object_get_int(json_obj);
		} else if (!strcmp(key, "led")) {
			attr->led = json_object_get_int(json_obj);
		} else if (!strcmp(key, "custom_wifi")) {
			if (strcmp(json_object_get_string(json_obj), "group"))
				attr->custom_wifi = "group";
		}
	}
	return;
}

static void parse_json_wifi_config(json_object *wifi_obj, struct wifi_attr *attr)
{
	if (!attr)
		return;

	json_object_object_foreach(wifi_obj, key, json_obj) {
		if (!strcmp(key, "enabled")) {
			attr->enabled = json_object_get_int(json_obj);
		} else if (!strcmp(key, "ssid")) {
			FREE_STRING(attr->ssid);
			attr->ssid = strdup(json_object_get_string(json_obj));
		} else if (!strcmp(key, "hide")) {
			attr->hide = json_object_get_int(json_obj);
		} else if (!strcmp(key, "bandwidth_limit")) {
			attr->bandwidth_limit = json_object_get_int(json_obj);
		} else if (!strcmp(key, "bandwidth_upload")) {
			attr->bandwidth_upload = json_object_get_int(json_obj);
		} else if (!strcmp(key, "bandwidth_download")) {
			attr->bandwidth_download = json_object_get_int(json_obj);
		} else if (!strcmp(key, "encryption")) {
			const char *encry = json_object_get_string(json_obj);
			if (!strcmp(encry, "open"))
				attr->encryption = ENCRYPTION_OPEN;
			else
				attr->encryption = ENCRYPTION_WPA2_PSK;
		} else if (!strcmp(key, "password")) {
			FREE_STRING(attr->password);
			attr->password = strdup(json_object_get_string(json_obj));
		} else if (!strcmp(key, "channel")) {
			attr->channel = json_object_get_int(json_obj);
		} else if(!strcmp(key, "country_code")) {
			FREE_STRING(attr->country_code);
			attr->country_code = strdup(json_object_get_string(json_obj));
		}
	}
	// If no password is specified, we set encryption to "open".
	if (!attr->password)
		attr->encryption = ENCRYPTION_OPEN;
	return;
}

static int set_device_config(void *arg, json_object *msg)
{
	struct capwap_wtp *wtp = arg;
	struct device_attr *dev_attr;
	struct json_object *dev_config, *wifi_config;
	int err;

	dev_attr = find_device_attr_by_json(msg);
	if (!dev_attr)
		return -EINVAL;
	dev_config = json_object_object_get_old(msg, "dev_config");
	if (dev_config) {
		dev_attr_mutex_lock(dev_attr);
		parse_json_dev_config(dev_config, dev_attr);
		dev_attr_mutex_unlock(dev_attr);
		err = ac_set_dev_attr(wtp);
		if (err)
			goto err_out;
	}

	wifi_config = json_object_object_get_old(msg, "wifi_2g_config");
	if (wifi_config) {
		dev_attr_mutex_lock(dev_attr);
		dev_attr->custom_wifi = NULL;
		parse_json_wifi_config(wifi_config, &dev_attr->wifi.band[WIFI_2G]);
		dev_attr_mutex_unlock(dev_attr);
		err = ac_update_wifi(wtp, WIFI_2G);
		if (err)
			goto err_out;
	}
	wifi_config = json_object_object_get_old(msg, "wifi_5g_config");
	if (wifi_config) {
		dev_attr_mutex_lock(dev_attr);
		dev_attr->custom_wifi = NULL;
		parse_json_wifi_config(wifi_config, &dev_attr->wifi.band[WIFI_5G]);
		dev_attr_mutex_unlock(dev_attr);
		err = ac_update_wifi(wtp, WIFI_5G);
		if (err)
			goto err_out;
	}

	// We have save_device_config in _CWCloseThread(), so we don't need to save here
	// if the device is online.
	dev_attr_mutex_lock(dev_attr);
	if (dev_attr->status == 1) {
		dev_attr_mutex_unlock(dev_attr);
		err = ac_reset_device(wtp);
	} else {
		err = save_device_config(dev_attr, 1);
		dev_attr_mutex_unlock(dev_attr);
	}

	return err;

err_out:
	dev_attr_mutex_lock(dev_attr);
	reload_dev_attr(dev_attr);
	dev_attr_mutex_unlock(dev_attr);
	return err;
}

static int set_group_name(void *arg, json_object *msg)
{
	struct capwap_wtp *wtp = arg;
	struct device_attr *dev_attr;
	const char *name;
	int err = 0;

	dev_attr = find_device_attr_by_json(msg);
	if (!dev_attr)
		return -EINVAL;

	name = get_string_of_json_key(msg, "name_of_group");
	if (!name)
		return -EINVAL;

	dev_attr_mutex_lock(dev_attr);
	FREE_STRING(dev_attr->group);
	dev_attr->group = strdup(name);
	err = save_device_config(dev_attr, 1);
	dev_attr_mutex_unlock(dev_attr);

	return err;
}

static int do_wtp_command(void *arg, json_object *msg)
{
	struct capwap_wtp *wtp = arg;
	struct device_attr *attr = find_device_attr_by_json(msg);
	const char *command;
	int ret;

	if (!attr)
		return -EINVAL;

	command = get_string_of_json_key(msg, "wtp_command");
	if (!command)
		return -EINVAL;

	dev_attr_mutex_lock(attr);
	if (!attr->status) {
		dev_attr_mutex_unlock(attr);
		return -ENODEV;
	}
	dev_attr_mutex_unlock(attr);

	return ac_do_wtp_command(wtp, command);
}

static int ap_update(void *arg, json_object *msg)
{
	struct capwap_wtp *wtp = arg;
	struct device_attr *attr = find_device_attr_by_json(msg);
	const char *path, *md5, *version;

	if (!attr)
		return -EINVAL;

	path = get_string_of_json_key(msg, "path");
	md5 = get_string_of_json_key(msg, "md5");
	version = get_string_of_json_key(msg, "version");
	if (!path || !md5 || !version)
		return -EINVAL;

	dev_attr_mutex_lock(attr);
	if (!attr->status) {
		dev_attr_mutex_unlock(attr);
		return -ENODEV;
	}
	dev_attr_mutex_unlock(attr);

	return ac_do_ap_update(wtp, path, md5, version);
}

static int delete_device(void *arg, json_object *msg)
{
	struct device_attr *attr = find_device_attr_by_json(msg);
	int err = 0;

	if (!attr)
		return 0;

	dev_attr_mutex_lock(attr);
	if ((attr->status == 1) || (attr->status == 2)) {
		dev_attr_mutex_unlock(attr);
		return -EBUSY;
	}

	err = delete_device_config(attr);
	dev_attr_mutex_unlock(attr);

	if (err)
		return err;

	dev_attr_free(attr);
	free(attr);

	return err;
}

#define OPERATION(func)             \
	{                               \
		.name = #func, .act = func, \
	}

static struct json_operation device_operations[] = {
	OPERATION(set_device_to_group),
	OPERATION(set_device_config),
	OPERATION(set_group_name),
	OPERATION(do_wtp_command),
	OPERATION(ap_update),
};

static struct json_operation ac_operations[] = {
	OPERATION(delete_device),
};

static struct json_operation *find_operation(struct json_operation *op, int op_num, const char *name)
{
	int i = 0;

	for (i = 0; i < op_num; i++) {
		if (!strcmp(op[i].name, name))
			return &op[i];
	}
	return NULL;
}

int json_handle(const char *msg, struct json_operation *op, int op_num, void *arg)
{
	json_object *msg_obj, *cmd_obj;
	struct json_operation *dev_op;
	const char *command;
	int i = 0, err;

	CWLog("%s: %s\n", __func__, msg);

	msg_obj = json_tokener_parse(msg);
	if (is_error(msg_obj))
		return -EINVAL;

	cmd_obj = json_object_object_get_old(msg_obj, "command");
	if (is_error(cmd_obj))
		return -EINVAL;

	command = json_object_get_string(cmd_obj);
	dev_op = find_operation(op, op_num, command);
	if (!dev_op)
		return -EINVAL;

	err = dev_op->act(arg, msg_obj);
	json_object_put(msg_obj);
	return err;
}

static void capwap_handle_json(evutil_socket_t sock, short what, void *arg)
{
	struct capwap_wtp *wtp = arg;
	void *buff;
	socklen_t addr_len;
	int msg_len;
	int err;

	if (what & EV_READ) {
		buff = MALLOC(JSON_BUFF_LEN);
		addr_len = sizeof(wtp->wum_addr);
		msg_len = capwap_recv_message(sock, buff, JSON_BUFF_LEN, (struct sockaddr *)&wtp->wum_addr, &addr_len);
		if (msg_len <= 0) {
			CWCritLog("AC interface %s receive message error", wtp->if_path);
			FREE(buff);
			return;
		}
		err = json_handle(buff, device_operations, ARRAY_SIZE(device_operations), wtp);
		FREE(buff);
		if (err) {
			CWWarningLog("json_handle() returned with %d", err);
			return;
		}
	}
}

int capwap_init_wtp_interface(struct capwap_wtp *wtp)
{
	uint8_t *mac;
	int sock;

	if (!wtp)
		return -EINVAL;

	mac = wtp->board_data.mac;
	bzero(wtp->if_path, sizeof(wtp->if_path));
	snprintf(wtp->if_path, sizeof(wtp->if_path), "/tmp/WTP.%02x_%02x_%02x_%02x_%02x_%02x",
		mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

	if ((sock = capwap_init_interface_sock(wtp->if_path)) < 0) {
		CWLog("create interface socket fail");
		return sock;
	}
	wtp->if_sock = sock;
	wtp->if_ev = event_new(wtp->ev_base, wtp->if_sock, EV_READ | EV_PERSIST, capwap_handle_json, wtp);

	return event_add(wtp->if_ev, NULL);
}

void capwap_destory_wtp_interface(struct capwap_wtp *wtp)
{
	unlink(wtp->if_path);
}

static int capwap_main_handle_interface(void *buff)
{
	struct capwap_interface_message *msg = buff;

	switch (msg->cmd) {
	case JSON_CMD:
		return json_handle(buff + sizeof(*msg), ac_operations, ARRAY_SIZE(ac_operations),
				   NULL);
	}

	return 0;
}

static void capwap_main_interface(evutil_socket_t sock, short what, void *arg)
{
	struct sockaddr_un wum_addr = {0};
	socklen_t addr_len;
	void *buff;
	int msg_len;
	int err;

	if (what & EV_READ) {
		buff = MALLOC(JSON_BUFF_LEN);
		addr_len = sizeof(wum_addr);
		msg_len = capwap_recv_message(sock, buff, JSON_BUFF_LEN, (struct sockaddr *)&wum_addr, &addr_len);
		if (msg_len <= 0) {
			CWCritLog("AC main interface receive message error");
			FREE(buff);
			return;
		}
		err = capwap_main_handle_interface(buff);
		FREE(buff);
		if (err) {
			CWWarningLog("json_handle() returned with %d", err);
			return;
		}
	}

	return;
}

static void *capwap_main_interface_loop(void *arg)
{
	struct sockaddr_un wum_addr = {0};
	struct event_base *base;
	struct event *ev;
	int sock;

	// pthread_detach(pthread_self());
	if ((sock = capwap_init_interface_sock(AC_MAIN_INTERFACE)) < 0) {
		CWCritLog("Create interface socket fail");
		exit(-1);
	}
	base = event_base_new();
	ev = event_new(base, sock, EV_READ | EV_PERSIST, capwap_main_interface, NULL);
	event_add(ev, NULL);

	event_base_dispatch(base);

	return NULL;
}

int capwap_init_main_interface()
{
	pthread_t thread;
	int err;

	if ((err = pthread_create(&thread, NULL, capwap_main_interface_loop, NULL))) {
		CWWarningLog("Create thread failed with %d", err);
		return err;
	}

	return 0;
}

void capwap_destory_main_interface()
{
	unlink(AC_MAIN_INTERFACE);
}
