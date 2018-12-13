#include <stddef.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <event2/event.h>
#include <event2/listener.h>

#include "capwap_message.h"
#include "CWProtocol.h"
#include "network.h"
#include "ac_mainloop.h"
#include "ac_manager.h"
#include "CWLog.h"

static void capwap_main_fsm(struct capwap_wtp *wtp, struct cw_ctrlmsg *ctrlmsg)
{
	int err;

	switch (cwmsg_ctrlmsg_get_type(ctrlmsg)) {
	case CW_MSG_TYPE_VALUE_DISCOVERY_REQUEST:
		wtp->state = QUIT;
		break;
	case CW_MSG_TYPE_VALUE_JOIN_REQUEST:
		if (wtp->state == IDLE) {
			err = capwap_idle_to_join(wtp, ctrlmsg);
			if (err) {
				CWLog("Error during enter join state: %d", err);
				wtp->state = QUIT;
				break;
			}
			wtp->attr = find_dev_attr_by_mac(wtp->if_path + 9);
			if (!wtp->attr) {
				wtp->attr = dev_attr_alloc();
				err = ac_init_dev_attr(wtp);
			} else {
				err = ac_update_dev_attr(wtp);
			}
			if (err) {
				CWLog("Error init dev attr, quit");
				wtp->state = QUIT;
				break;
			}
			save_device_config(wtp->attr, 0);
			wtp->state = JOIN;
		}
		break;
	case CW_MSG_TYPE_VALUE_CONFIGURE_REQUEST:
		if (wtp->state == JOIN || wtp->state == CONFIGURE) {
			err = capwap_join_to_configure(wtp, ctrlmsg);
			if (!err)
				wtp->state = CONFIGURE;
		}
		break;
	case CW_MSG_TYPE_VALUE_CHANGE_STATE_EVENT_REQUEST:
		if (wtp->state == CONFIGURE || wtp->state == DATA_CHECK) {
			err = capwap_configure_to_data_check(wtp, ctrlmsg);
			if (err)
				break;
			err = capwap_init_wtp_interface(wtp);
			if (err) {
				CWLog("Init interface %s error", wtp->if_path);
				wtp->state = QUIT;
				break;
			}
			wtp->state = DATA_CHECK;
			err = ac_add_wlan(wtp);
			if (!err) {
				wtp->attr->status = DEV_RUNNING;
				save_device_config(wtp->attr, 0);
			}
		}
		break;
	case CW_MSG_TYPE_VALUE_WLAN_CONFIGURATION_RESPONSE:
		if (wtp->state == DATA_CHECK || wtp->state == RUN) {
			err = cwmsg_parse_wlan_config_response(ctrlmsg, wtp);
			if (err) {
				CWCritLog("wtp %s wlan set error", wtp->ip_addr);
				wtp->attr->status = DEV_SET_ERROR;
				save_device_config(wtp->attr, 0);
				break;
			} else {
				err = ac_add_wlan(wtp);
				if (!err) {
					wtp->attr->status = DEV_RUNNING;
					save_device_config(wtp->attr, 0);
				}
			}
		}
	default:
		// We need DATA_CHECK state here to handle echo request.
		if (wtp->state == DATA_CHECK || wtp->state == RUN) {
			err = capwap_run(wtp, ctrlmsg);
			if (err)
				wtp->state = QUIT;
		}
		break;
	}
	if (wtp->state == QUIT)
		event_base_loopbreak(wtp->ev_base);
}

static void capwap_control_port(evutil_socket_t sock, short what, void *arg)
{
	uint8_t buff[1024];
	struct capwap_wtp *wtp = arg;
	struct cw_ctrlmsg *ctrlmsg = NULL;
	int msg_len;
	int err;

	if (what & EV_READ) {
		msg_len = capwap_recv_ctrl_message(sock, buff, sizeof(buff));
		if (msg_len <= 0) {
			CWWarningLog("receive error");
			return;
		}
		ctrlmsg = cwmsg_ctrlmsg_malloc();
		err = cwmsg_ctrlmsg_parse(ctrlmsg, buff, msg_len);
		if (err) {
			cwmsg_ctrlmsg_free(ctrlmsg);
			CWWarningLog("Capwap message parse error(%d) from %s:%d", err, wtp->ip_addr, sock_get_port(&wtp->ctrl_addr));
			return;
		}
		wtp->seq_num = cwmsg_ctrlmsg_get_seqnum(ctrlmsg);
		// Add again to reschedule timeout
		event_add(wtp->ctrl_ev, &wtp->ctrl_tv);

		CWLog("receive %d from %s:%d", cwmsg_ctrlmsg_get_type(ctrlmsg), wtp->ip_addr, sock_get_port(&wtp->ctrl_addr));
		capwap_main_fsm(wtp, ctrlmsg);
		cwmsg_ctrlmsg_free(ctrlmsg);
	} else if (what & EV_TIMEOUT) {
		CWLog("wtp %s timeout, consider offline...", wtp->ip_addr);
		wtp->state = QUIT;
		event_base_loopbreak(wtp->ev_base);
	}
}

void capwap_free_wtp(struct capwap_wtp *wtp)
{
	event_free(wtp->ctrl_ev);
	event_free(wtp->data_ev);
	evconnlistener_free(wtp->if_ev);

	FREE(wtp->location);
	FREE(wtp->name);
}

void *capwap_manage_wtp(void *arg)
{
	struct capwap_wtp *wtp = arg;
	int sock;

	CWLog("New thread to manage wtp %s", wtp->ip_addr);
	sock = capwap_init_socket(CW_CONTROL_PORT, &wtp->ctrl_addr, wtp->wtp_addr_len);
	if (sock < 0) {
		CWCritLog("establish to %s error: %s", wtp->ip_addr, strerror(-sock));
		return NULL;
	}

	wtp->ctrl_sock = sock;
	wtp->state = IDLE;
	wtp->ev_base = event_base_new();
	wtp->ctrl_ev = event_new(wtp->ev_base, sock, EV_READ | EV_PERSIST, capwap_control_port, wtp);

	wtp->ctrl_tv.tv_sec = 1000;
	event_add(wtp->ctrl_ev, &wtp->ctrl_tv);

	event_base_dispatch(wtp->ev_base);

	CWCritLog("wtp %s closed", wtp->ip_addr);
	capwap_free_wtp(wtp);
	free(arg);

	return NULL;
}

uint8_t get_echo_interval(struct capwap_wtp *wtp)
{
	return (uint8_t)wtp->attr->ap_alive_time / CW_ECHO_MAX_RETRANSMIT_DEFAULT;
}

uint32_t get_idle_timeout(struct capwap_wtp *wtp)
{
	return (uint32_t)wtp->attr->client_idle_time;
}

struct wifi_info *find_wifi_info(struct capwap_wtp *wtp, uint8_t radio_id, uint8_t wlan_id)
{
	int i;

	if (!wtp)
		return NULL;
	for (i = 0; i < WIFI_NUM; i++) {
		if (wtp->wifi[i].radio_id == radio_id && wtp->wifi[i].wlan_id == wlan_id)
			return &wtp->wifi[i];
	}
	return NULL;
}
