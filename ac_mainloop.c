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

static void capwap_main_fsm(evutil_socket_t sock, short what, void *arg)
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

		CWLog("receive %d from %s:%d", cwmsg_ctrlmsg_get_type(ctrlmsg), wtp->ip_addr, sock_get_port(&wtp->ctrl_addr));
	} else if (what & EV_TIMEOUT) {
		CWLog("wtp %s timeout, consider offline...", wtp->ip_addr);
		wtp->state = QUIT;
	}

	// We use a infinite loop to evolute our state machine, break the loop to waiting for new packets.
	while (1) {
		switch (wtp->state) {
		case IDLE:
			err = capwap_idle_to_join(wtp, ctrlmsg);
			if (err) {
				CWLog("Error during enter join state: %d", err);
				goto wait_packet;
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
			goto wait_packet;
		case DISCOVERY:
			wtp->state = QUIT;
			break;
		case JOIN:
			err = capwap_join_to_configure(wtp, ctrlmsg);
			if (!err)
				wtp->state = CONFIGURE;
			goto wait_packet;
		case CONFIGURE:
			err = capwap_configure_to_data_check(wtp, ctrlmsg);
			if (!err)
				wtp->state = DATA_CHECK;
			break;
		case DATA_CHECK:
			err = capwap_init_wtp_interface(wtp);
			if (err) {
				CWLog("Init interface %s error", wtp->if_path);
				wtp->state = QUIT;
				break;
			}
			//TODO: send wifi configure here
			wtp->attr->status = 1;
			save_device_config(wtp->attr, 0);
			goto wait_packet;
		case RUN:
			err = capwap_run(wtp, ctrlmsg);
			goto wait_packet;
		case QUIT:
			CWCritLog("wtp %s quit!", wtp->ip_addr);
			event_base_loopbreak(wtp->ev_base);
			goto wait_packet;

		default:
			CWCritLog("Unknown state %d, quit", wtp->state);
			wtp->state = QUIT;
			break;
		}
	}

wait_packet:
	// Add again to reschedule timeout
	event_add(wtp->ctrl_ev, &wtp->ctrl_tv);
	if (ctrlmsg)
		cwmsg_ctrlmsg_free(ctrlmsg);
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
	wtp->ctrl_ev = event_new(wtp->ev_base, sock, EV_READ | EV_PERSIST, capwap_main_fsm, wtp);

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
