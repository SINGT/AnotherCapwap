#include <string.h>
#include <errno.h>

#include "tlv/tlv.h"
#include "capwap_message.h"
#include "network.h"
#include "CWProtocol.h"
#include "ac_mainloop.h"
#include "CWLog.h"

static int capwap_parse_join_request(struct capwap_wtp *wtp, struct cw_ctrlmsg *join_req)
{
	struct cw_elem_tlv_with_id *id_tlv_elem;
	uint16_t elem_type;
	uint16_t elem_len;
	void *elem_value;

	id_tlv_elem = cwmsg_tlv_with_id_malloc();
	if (!id_tlv_elem)
		return -ENOMEM;

	cwmsg_ctrlmsg_for_each_elem(join_req, elem_type, elem_len, elem_value) {
		switch (elem_type) {
		case CW_MSG_ELEMENT_LOCATION_DATA_CW_TYPE:
			wtp->location = cwmsg_parse_string(elem_value, elem_len);
			break;
		case CW_MSG_ELEMENT_WTP_BOARD_DATA_CW_TYPE:
			cwmsg_tlv_with_id_reinit(id_tlv_elem);
			if (cwmsg_parse_tlv_with_id(&id_tlv_elem, elem_value, min(CW_SESSION_ID_LENGTH, elem_len))) {
				CWLog("Invalid board data elem, ignore this join request");
				goto error_request;
			}
			cwmsg_parse_board_data(&wtp->board_data, id_tlv_elem);
			break;
		case CW_MSG_ELEMENT_SESSION_ID_CW_TYPE:
			cwmsg_parse_raw(wtp->session_id, CW_SESSION_ID_LENGTH, elem_value, elem_len);
			break;
		case CW_MSG_ELEMENT_WTP_DESCRIPTOR_CW_TYPE:
			if (cwmsg_parse_wtp_descriptor(&wtp->descriptor, elem_value, elem_len)) {
				CWLog("Invalid wtp descriptor");
				goto error_request;
			}
			break;
		case CW_MSG_ELEMENT_WTP_NAME_CW_TYPE:
			wtp->name = cwmsg_parse_string(elem_value, elem_len);
			break;
		case CW_MSG_ELEMENT_VENDOR_SPEC_WTP_VERSION:
			wtp->version = cwmsg_parse_u32(elem_value);
			break;
		case CW_MSG_ELEMENT_VENDOR_SPEC_PAYLOAD_BW_CW_TYPE:
			if (cwmsg_parse_vendor_spec(&wtp->vendor_spec, elem_value, elem_len)) {
				CWLog("Invalid vendor spec payload");
				goto error_request;
			}
			break;
		case CW_MSG_ELEMENT_LOCAL_IPV4_ADDRESS_CW_TYPE:
		case CW_MSG_ELEMENT_WTP_FRAME_TUNNEL_MODE_CW_TYPE:
		case CW_MSG_ELEMENT_WTP_MAC_TYPE_CW_TYPE:
		case CW_MSG_ELEMENT_IEEE80211_WTP_RADIO_INFORMATION_CW_TYPE:
		case CW_MSG_ELEMENT_ECN_SUPPORT_CW_TYPE:
			CWLog("Unused Message Element(%d) in Join request", elem_type);
			break;
		default:
			CWLog("Unrecognized Message Element(%d) in Join request", elem_type);
			break;
		}
	}

	cwmsg_tlv_with_id_free(id_tlv_elem);
	return 0;

error_request:
	FREE(wtp->location);
	FREE(wtp->name);
	cwmsg_tlv_with_id_free(id_tlv_elem);
	return -EINVAL;
}

static int capwap_send_join_response(struct capwap_wtp *wtp)
{
	return 0;
}

int capwap_idle_to_join(struct capwap_wtp *wtp, struct cw_ctrlmsg *join_req)
{
	int err;

	if ((err = capwap_parse_join_request(wtp, join_req))) {
		CWWarningLog("join request parse fail: %s", strerror(-err));
		return err;
	}

	capwap_send_join_response(wtp);

	return 0;
}