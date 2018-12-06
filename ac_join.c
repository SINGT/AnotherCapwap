#include <string.h>
#include <errno.h>

#include "tlv.h"
#include "capwap_message.h"
#include "network.h"
#include "CWProtocol.h"
#include "ac_mainloop.h"
#include "CWLog.h"

static int capwap_parse_join_request(struct capwap_wtp *wtp, struct cw_ctrlmsg *join_req)
{
	struct tlv_box *join_elem;
	uint16_t elem_type;
	uint16_t elem_len;
	void *elem_value;

	if (cwmsg_ctrlmsg_get_type(join_req) != CW_MSG_TYPE_VALUE_JOIN_REQUEST)
		return -EINVAL;

	join_elem = tlv_box_create();
	if (!join_elem)
		return -ENOMEM;

	cwmsg_ctrlmsg_for_each_elem(join_req, elem_type, elem_len, elem_value) {
		switch (elem_type) {
		case CW_MSG_ELEMENT_LOCATION_DATA_CW_TYPE:
			wtp->location = cwmsg_parse_string(elem_value, elem_len);
			break;
		case CW_MSG_ELEMENT_WTP_BOARD_DATA_CW_TYPE:
			tlv_box_init(join_elem);
			tlv_box_set_how(join_elem, SERIAL_WITH_ID);
			if (tlv_box_parse(join_elem, elem_value, elem_len)) {
				CWLog("Invalid board data elem, ignore this join request");
				goto error_request;
			}
			cwmsg_parse_board_data(&wtp->board_data, join_elem);
			tlv_box_destroy(join_elem);
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

	tlv_box_destroy(join_elem);
	return 0;

error_request:
	FREE(wtp->location);
	FREE(wtp->name);
	tlv_box_destroy(join_elem);
	return -EINVAL;
}

static int capwap_send_join_response(struct capwap_wtp *wtp, uint32_t result)
{
	struct cw_ctrlmsg *join_resp;
	int err;

	join_resp = cwmsg_ctrlmsg_new(CW_MSG_TYPE_VALUE_JOIN_RESPONSE, wtp->seq_num);
	if (!join_resp || cwmsg_assemble_ac_descriptor(join_resp) ||
	    cwmsg_assemble_result_code(join_resp, result) ||
	    cwmsg_assemble_string(join_resp, CW_MSG_ELEMENT_AC_NAME_CW_TYPE, AC_NAME, TLV_NOFREE) ||
	    cwmsg_assemble_ipv4_addr(join_resp, CW_MSG_ELEMENT_CW_CONTROL_IPV4_ADDRESS_CW_TYPE, "br-lan"))
		return -ENOMEM;

	err = capwap_send_ctrl_message(wtp, join_resp);
	cwmsg_ctrlmsg_free(join_resp);

	return err;
}

int capwap_idle_to_join(struct capwap_wtp *wtp, struct cw_ctrlmsg *join_req)
{
	int err;
	uint32_t result;

	if ((err = capwap_parse_join_request(wtp, join_req))) {
		CWWarningLog("join request parse fail: %s", strerror(-err));
	}

	if (err == 0)
		result = CW_PROTOCOL_SUCCESS;
	else
		result = CW_PROTOCOL_FAILURE;

	return capwap_send_join_response(wtp, result);
}