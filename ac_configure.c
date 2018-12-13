#include <string.h>
#include <errno.h>

#include "tlv.h"
#include "capwap_message.h"
#include "network.h"
#include "CWProtocol.h"
#include "ac_mainloop.h"
#include "CWLog.h"

static int capwap_parse_configure_request(struct capwap_wtp *wtp, struct cw_ctrlmsg *cfg_req)
{
	uint16_t elem_type;
	uint16_t elem_len;
	void *elem_value;

	if (cwmsg_ctrlmsg_get_type(cfg_req) != CW_MSG_TYPE_VALUE_CONFIGURE_REQUEST)
		return -EINVAL;

	cwmsg_ctrlmsg_for_each_elem(cfg_req, elem_type, elem_len, elem_value) {
		switch (elem_type) {
		case CW_MSG_ELEMENT_IEEE80211_MULTI_DOMAIN_CAPABILITY_CW_TYPE:
			if (cwmsg_parse_wifi_info(wtp, elem_value, elem_len))
				return -EINVAL;
			break;
		case CW_MSG_ELEMENT_AC_NAME_CW_TYPE:
		case CW_MSG_ELEMENT_AC_NAME_INDEX_CW_TYPE:
		case CW_MSG_ELEMENT_RADIO_ADMIN_STATE_CW_TYPE:
		case CW_MSG_ELEMENT_STATISTICS_TIMER_CW_TYPE:
		case CW_MSG_ELEMENT_WTP_REBOOT_STATISTICS_CW_TYPE:
		case CW_MSG_ELEMENT_IEEE80211_WTP_RADIO_INFORMATION_CW_TYPE:
		case CW_MSG_ELEMENT_IEEE80211_MAC_OPERATION_CW_TYPE:
		case CW_MSG_ELEMENT_IEEE80211_SUPPORTED_RATES_CW_TYPE:
		default:
			break;
		}
	}

	return 0;
}

static int capwap_send_configure_response(struct capwap_wtp *wtp)
{
	struct cw_ctrlmsg *cfg_resp;
	int err;

	cfg_resp = cwmsg_ctrlmsg_new(CW_MSG_TYPE_VALUE_CONFIGURE_RESPONSE, wtp->seq_num);
	if (!cfg_resp || cwmsg_assemble_timers(cfg_resp, wtp) ||
	    cwmsg_assemble_idle_timeout(cfg_resp, wtp) ||
	    cwmsg_assemble_wtp_fallback(cfg_resp, wtp))
		return -ENOMEM;

	err = capwap_send_ctrl_message(wtp, cfg_resp);
	cwmsg_ctrlmsg_free(cfg_resp);

	return err;
}

int capwap_join_to_configure(struct capwap_wtp *wtp, struct cw_ctrlmsg *cfg_req)
{
	int err;

	err = capwap_parse_configure_request(wtp, cfg_req);
	if (err) {
		CWWarningLog("Parse configure request failed with %d", err);
		return err;
	}

	return capwap_send_configure_response(wtp);
}
