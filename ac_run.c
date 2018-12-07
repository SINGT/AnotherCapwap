#include <string.h>
#include <errno.h>
#include <event2/event.h>

#include "capwap_message.h"
#include "network.h"
#include "CWProtocol.h"
#include "ac_mainloop.h"
#include "CWLog.h"

void capwap_send_keepalive(struct capwap_wtp *wtp, void *buff, size_t len)
{
	struct iovec io;

	io.iov_base = buff;
	io.iov_len = len;
	capwap_send_message(wtp->data_sock, &io, 1, NULL, 0);
}

void capwap_data_channel(evutil_socket_t sock, short what, void *arg)
{
	struct capwap_wtp *wtp = arg;
	struct cw_protohdr protohdr;
	uint8_t buff[64];
	size_t msg_len;

	msg_len = capwap_recv_ctrl_message(sock, buff, sizeof(buff));
	if (msg_len <= 0) {
		CWWarningLog("receive error");
		return;
	}
	cwmsg_protohdr_parse(buff, &protohdr);
	if (protohdr.head.b.K) {
		// Keep-alive response is always the same with resquest
		CWDebugLog("Receive keep-alive");
		capwap_send_keepalive(wtp, buff, msg_len);
	}
}

void capwap_send_echo_response(struct capwap_wtp *wtp)
{
	struct cw_ctrlmsg *echo = cwmsg_ctrlmsg_new(CW_MSG_TYPE_VALUE_ECHO_RESPONSE, wtp->seq_num);

	if (!echo)
		return;
	capwap_send_ctrl_message(wtp, echo);
}

int capwap_run(struct capwap_wtp *wtp, struct cw_ctrlmsg *ctrlmsg)
{
	switch (cwmsg_ctrlmsg_get_type(ctrlmsg)) {
	case CW_MSG_TYPE_VALUE_ECHO_REQUEST:
		capwap_send_echo_response(wtp);
		break;
	case CW_MSG_TYPE_VALUE_CONFIGURE_UPDATE_RESPONSE:
	case CW_MSG_TYPE_VALUE_CHANGE_STATE_EVENT_REQUEST:
	case CW_MSG_TYPE_VALUE_WTP_EVENT_REQUEST:
	case CW_MSG_TYPE_VALUE_WTP_EXECUTE_COMMAND_RESPONSE:
	case CW_MSG_TYPE_VALUE_RESET_RESPONSE:
	case CW_MSG_TYPE_VALUE_WLAN_CONFIGURATION_RESPONSE:
	case CW_MSG_TYPE_VALUE_STATION_CONFIGURATION_RESPONSE:

	case CW_MSG_TYPE_VALUE_DATA_TRANSFER_REQUEST:
	case CW_MSG_TYPE_VALUE_CLEAR_CONFIGURATION_RESPONSE:
		break;
	case CW_MSG_TYPE_VALUE_DISCOVERY_REQUEST:
	default:
		break;
	}

	return 0;
}
