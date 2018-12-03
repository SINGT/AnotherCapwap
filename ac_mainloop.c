#include <stddef.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <event2/event.h>

#include "capwap_message.h"
#include "CWProtocol.h"
#include "network.h"
#include "ac_mainloop.h"
#include "CWLog.h"

#define AC_NAME "my_ac"

void capwap_main_fsm(evutil_socket_t sock, short what, void *arg)
{
	uint8_t buff[1024];
	char addr_str[INET_ADDRMAXLEN];
	struct capwap_wtp *wtp = arg;
	struct cw_ctrlmsg *ctrlmsg = cwmsg_ctrlmsg_malloc();
	int msg_len;
	int err;

	if (what & EV_READ) {
		msg_len = capwap_recv_ctrl_message(sock, buff, sizeof(buff));
		if (msg_len <= 0) {
			CWWarningLog("receive error");
			return;
		}
		if ((err = cwmsg_ctrlmsg_parse(ctrlmsg, buff, msg_len))) {
			cwmsg_ctrlmsg_free(ctrlmsg);
			CWWarningLog("Capwap message parse error(%d) from %s:%d", err, wtp->ip_addr, sock_get_port(&wtp->ctrl_addr));
			return;
		}

		CWLog("receive %d from %s:%d", cwmsg_ctrlmsg_get_type(ctrlmsg), wtp->ip_addr, sock_get_port(&wtp->ctrl_addr));
		switch (wtp->state)
		{
			case IDLE:
				if ((err = capwap_idle_to_join(wtp, ctrlmsg))) {
					CWLog("Error during enter join state: %d", err);
				} else {
					wtp->state = JOIN;
				}
				break;
			case DISCOVERY:
				break;

			case JOIN:

				break;
			case DATA_CHECK:
				//
				break;
			case QUIT:
				CWCritLog("wtp %s quit!", wtp->ip_addr);
				event_base_loopbreak(wtp->ev_base);
				break;

			default:
				CWCritLog("Unknown state %d, quit", wtp->state);
				wtp->state = QUIT;
				break;
		}

		// Add again to reschedule timeout
		// event_add(wtp->ctrl_ev, &wtp->ctrl_tv);
		cwmsg_ctrlmsg_free(ctrlmsg);
	} else if (what & EV_TIMEOUT) {
		CWLog("wtp %s timeout, consider offline...", wtp->ip_addr);
		event_base_loopbreak(wtp->ev_base);
	}
}

void capwap_free_wtp(struct capwap_wtp *wtp)
{
	event_free(wtp->ctrl_ev);
	event_free(wtp->data_ev);

	FREE(wtp->location);
	FREE(wtp->name);
}

void *capwap_manage_wtp(void *arg)
{
	char addr_str[INET_ADDRMAXLEN];
	struct capwap_wtp *wtp = arg;
	int sock;

	CWLog("%s", __func__);
	if ((sock = capwap_init_socket(CW_CONTROL_PORT, &wtp->ctrl_addr, wtp->ctrl_addr_len)) < 0) {
		CWCritLog("establish to %s error: %s", wtp->ip_addr, strerror(-sock));
		return NULL;
	}

	wtp->ctrl_sock = sock;
	wtp->state = IDLE;
	wtp->ev_base = event_base_new();
	wtp->ctrl_ev = event_new(wtp->ev_base, sock, EV_READ | EV_PERSIST, capwap_main_fsm, wtp);

	wtp->ctrl_tv.tv_sec = 10;
	event_add(wtp->ctrl_ev, NULL);

	event_base_dispatch(wtp->ev_base);

	CWCritLog("wtp %s closed", wtp->ip_addr);
	capwap_free_wtp(wtp);
	free(arg);

	return NULL;
}
