#include <string.h>
#include <errno.h>
#include <event2/event.h>

#include "capwap_message.h"
#include "network.h"
#include "CWProtocol.h"
#include "ac_mainloop.h"
#include "CWLog.h"

static int capwap_parse_data_check_request(struct capwap_wtp *wtp, struct cw_ctrlmsg *data_check_req)
{
	uint16_t elem_type;
	uint16_t elem_len;
	void *elem_value;
	uint32_t result;

	if (cwmsg_ctrlmsg_get_type(data_check_req) != CW_MSG_TYPE_VALUE_CHANGE_STATE_EVENT_REQUEST)
		return -EINVAL;

	cwmsg_ctrlmsg_for_each_elem(data_check_req, elem_type, elem_len, elem_value) {
		switch (elem_type) {
		case CW_MSG_ELEMENT_RESULT_CODE_CW_TYPE:
			result = cwmsg_parse_u32(elem_value);
			if (result != CW_PROTOCOL_SUCCESS)
				CWLog("data check result fail");
			break;
		case CW_MSG_ELEMENT_RADIO_OPERAT_STATE_CW_TYPE:
		default:
			break;
		}
	}

	return 0;
}

static int capwap_send_data_check_response(struct capwap_wtp *wtp)
{
	struct cw_ctrlmsg *data_check;
	int err;

	data_check = cwmsg_ctrlmsg_new(CW_MSG_TYPE_VALUE_CHANGE_STATE_EVENT_RESPONSE, wtp->seq_num);
	if (!data_check)
		return -EINVAL;

	err = capwap_send_ctrl_message(wtp, data_check);
	cwmsg_ctrlmsg_destroy(data_check);
	return err;
}

int capwap_configure_to_data_check(struct capwap_wtp *wtp, struct cw_ctrlmsg *data_check_req)
{
	int err;

	err = capwap_parse_data_check_request(wtp, data_check_req);
	if (err) {
		CWWarningLog("Parse data_check request failed with %d", err);
		return err;
	}

	wtp->data_sock = capwap_init_socket(CW_DATA_PORT, &wtp->ctrl_addr, wtp->ctrl_addr_len);
	if (wtp->data_sock < 0)
		return wtp->data_sock;
	wtp->data_ev = event_new(wtp->ev_base, wtp->data_sock, EV_READ | EV_PERSIST, capwap_data_channel, wtp);
	err = event_add(wtp->data_ev, NULL);
	if (err) {
		CWLog("Start data channel failed with %d", err);
		return err;
	}

	return capwap_send_data_check_response(wtp);
}
