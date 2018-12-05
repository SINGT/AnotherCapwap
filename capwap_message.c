#include <stddef.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <event2/event.h>
#include <string.h>

#include "capwap_message.h"
#include "capwap_common.h"
#include "CWProtocol.h"
#include "network.h"
#include "tlv.h"
#include "CWLog.h"

uint8_t cwmsg_parse_u8(void *value)
{
	return *(uint8_t *)value;
}

uint16_t cwmsg_parse_u16(void *value)
{
	uint16_t tmp;
	memcpy(&tmp, value, sizeof(uint16_t));
	return htons(tmp);
}

uint32_t cwmsg_parse_u32(void *value)
{
	uint32_t tmp;
	memcpy(&tmp, value, sizeof(uint32_t));
	return htonl(tmp);
}

void cwmsg_put_u8(struct message *msg, uint8_t value)
{
	*(uint8_t *)(msg->data + msg->len) = value;
	msg->len += sizeof(uint8_t);
}

void cwmsg_put_u16(struct message *msg, uint16_t value)
{
	uint16_t tmp = htons(value);

	memcpy(msg->data + msg->len, &tmp, sizeof(uint16_t));
	msg->len += sizeof(uint16_t);
}

void cwmsg_put_u32(struct message *msg, uint32_t value)
{
	uint32_t tmp = htonl(value);

	memcpy(msg->data + msg->len, &tmp, sizeof(uint32_t));
	msg->len += sizeof(uint32_t);
}

void cwmsg_put_raw(struct message *msg, void *value, size_t len)
{
	memcpy(msg->data + msg->len, value, len);
	msg->len += len;
}

void cwmsg_protohdr_parse(void *buff, struct cw_protohdr *header)
{
	header->head.d32 = cwmsg_parse_u32(buff);
	buff += sizeof(uint32_t);
	header->frag.d32 = cwmsg_parse_u32(buff);
}

void cwmsg_ctrlhdr_parse(void *msg, struct cw_ctrlhdr *ctrlhdr)
{
	ctrlhdr->type = cwmsg_parse_u32(msg);
	msg += sizeof(uint32_t);
	ctrlhdr->seq_num = cwmsg_parse_u8(msg);
	msg += sizeof(uint8_t);
	ctrlhdr->length = cwmsg_parse_u16(msg);
	msg += sizeof(uint16_t);
	ctrlhdr->flags = cwmsg_parse_u8(msg);
}

struct cw_ctrlmsg *cwmsg_ctrlmsg_malloc()
{
	struct cw_ctrlmsg *msg = malloc(sizeof(struct cw_ctrlmsg));

	if (!msg)
		return NULL;

	memset(msg, 0, sizeof(*msg));
	tlv_box_init(&msg->elem_box);

	return msg;
}

void cwmsg_ctrlmsg_free(struct cw_ctrlmsg *msg)
{
	tlv_box_destroy(&msg->elem_box);
	if (msg->raw_hdr)
		free(msg->raw_hdr);
	free(msg);
}

struct cw_ctrlmsg *cwmsg_ctrlmsg_new(uint32_t type, uint8_t seq)
{
	struct cw_ctrlmsg *msg = cwmsg_ctrlmsg_malloc();

	if (!msg)
		return NULL;

	msg->protohdr.head.b.hlen = sizeof(struct cw_protohdr) / 4;
	msg->protohdr.head.b.wbid = 1;

	msg->ctrlhdr.type = type;
	msg->ctrlhdr.seq_num = seq;

	return msg;
}

void cwmsg_ctrlmsg_destory(struct cw_ctrlmsg *msg)
{
	return cwmsg_ctrlmsg_free(msg);
}

void cwmsg_protohdr_set(struct cw_protohdr *header, int radio_id, int keep_alive)
{
	header->head.b.rid = radio_id;
	header->head.b.wbid = 1;
	header->head.b.K = keep_alive;
	header->head.b.hlen = sizeof(*header) / 4;
}

int cwmsg_ctrlmsg_add_element(struct cw_ctrlmsg *ctrlmsg, uint16_t type, struct message *msg, int flag)
{
	if (!ctrlmsg)
		return -EINVAL;

	return tlv_box_put_raw(&ctrlmsg->elem_box, type, msg, flag);
}

int cwmsg_ctrlmsg_serialize(struct cw_ctrlmsg *ctrlmsg)
{
	uint32_t *val;

	ctrlmsg->raw_hdr_len =
	    CAPWAP_HEADER_LEN(ctrlmsg->protohdr) + CAPWAP_CONTROL_HEADER_LEN;
	ctrlmsg->raw_hdr = malloc(ctrlmsg->raw_hdr_len);
	if (!ctrlmsg->raw_hdr)
		return -ENOMEM;

	ctrlmsg->ctrlhdr.length = tlv_box_get_size(&ctrlmsg->elem_box);

	val = ctrlmsg->raw_hdr;
	*val++ = ntohl(ctrlmsg->protohdr.head.d32);
	*val++ = ntohl(ctrlmsg->protohdr.frag.d32);
	*val++ = htonl(ctrlmsg->ctrlhdr.type);
	*val = ctrlmsg->ctrlhdr.seq_num | (htons(ctrlmsg->ctrlhdr.length) << 8) |
	       (ctrlmsg->ctrlhdr.flags << 24);

	return tlv_box_serialize(&ctrlmsg->elem_box);
}

void *cwmsg_ctrlmsg_get_buffer(struct cw_ctrlmsg *ctrlmsg)
{
	return tlv_box_get_buffer(&ctrlmsg->elem_box);
}

int cwmsg_ctrlmsg_get_msg_len(struct cw_ctrlmsg *ctrlmsg)
{
	return tlv_box_get_size(&ctrlmsg->elem_box);
}

int cwmsg_ctrlmsg_get_total_len(struct cw_ctrlmsg *ctrlmsg)
{
	return cwmsg_ctrlmsg_get_msg_len(ctrlmsg) + CAPWAP_HEADER_LEN(ctrlmsg->protohdr) +
	       CAPWAP_CONTROL_HEADER_LEN;
}

int cwmsg_ctrlmsg_parse(struct cw_ctrlmsg *ctrlmsg, void *msg, int len)
{
	int parsed_len = 0;

	if (!msg || !ctrlmsg)
		return -EINVAL;

	cwmsg_protohdr_parse(msg, &ctrlmsg->protohdr);
	parsed_len += CAPWAP_HEADER_LEN(ctrlmsg->protohdr);
	cwmsg_ctrlhdr_parse(msg + parsed_len, &ctrlmsg->ctrlhdr);
	parsed_len += CAPWAP_CONTROL_HEADER_LEN;
	tlv_box_parse(&ctrlmsg->elem_box, msg + parsed_len, len - parsed_len);

	if (cwmsg_ctrlmsg_get_total_len(ctrlmsg) != len) {
		return -EINVAL;
	}

	return 0;
}

uint32_t cwmsg_ctrlmsg_get_type(struct cw_ctrlmsg *msg)
{
	return msg->ctrlhdr.type;
}

char *cwmsg_parse_string(void *value, uint16_t str_len)
{
	char *s = malloc(str_len + 1);
	if (!s)
		return NULL;
	memcpy(s, value, str_len);
	s[str_len] = '\0';
	return s;
}

void cwmsg_parse_raw(void *dst, uint16_t dst_len, void *value, uint16_t value_len)
{
	if (dst_len < value_len)
		CWWarningLog("Parse message less than origin length!!!");
	memcpy(dst, value, min(dst_len, value_len));
}

int cwmsg_parse_board_data(struct cw_wtp_board_data *board_data, struct tlv_box *elem)
{
	struct tlv *tlv;
	uint16_t type;
	uint16_t len;
	void *value;

	// TODO: Judge vendor ID?
	tlv_box_for_each_tlv(elem, tlv, type, len, value) {
		switch (type) {
		case CW_WTP_MODEL_NUMBER:
			cwmsg_parse_raw(board_data->model, sizeof(board_data->model), value, len);
			break;
		case CW_BASE_MAC:
			cwmsg_parse_raw(board_data->mac, sizeof(board_data->mac), value, len);
			break;
		}
	}

	return 0;
}
int cwmsg_parse_wtp_descriptor(struct cw_wtp_descriptor *desc, void *value, uint16_t len)
{
	struct tlv_box *desc_elem;
	struct tlv *tlv;
	uint16_t offset = 0;
	uint16_t sub_type;
	uint16_t sub_len;
	void *sub_value;

	desc->max_radio = cwmsg_parse_u8(value + offset++);
	desc->radio_in_use = cwmsg_parse_u8(value + offset++);
	desc->num_encrypt = cwmsg_parse_u8(value + offset++);
	desc->encryp_wbid = cwmsg_parse_u8(value + offset++);
	desc->encryp_cap = cwmsg_parse_u16(value + offset);
	offset += sizeof(uint16_t);
	if (!(desc_elem = tlv_box_create()))
		return -EINVAL;
	tlv_box_set_how(desc_elem, SERIAL_EACH_WITH_ID);
	if (tlv_box_parse(desc_elem, value + offset, len - offset)) {
		tlv_box_destroy(desc_elem);
		return -EINVAL;
	}

	tlv_box_for_each_tlv(desc_elem, tlv, sub_type, sub_len, sub_value) {
		switch (sub_type) {
		case CW_WTP_HARDWARE_VERSION:
			cwmsg_parse_raw(desc->hardware_version, sizeof(desc->hardware_version), sub_value, sub_len);
			break;
		case CW_WTP_SOFTWARE_VERSION:
			cwmsg_parse_raw(desc->software_version, sizeof(desc->software_version), sub_value, sub_len);
			break;
		}
	}
	tlv_box_destroy(desc_elem);
	return 0;
}

int cwmsg_parse_vendor_spec(struct cw_wtp_vendor_spec *vendor, void *value, uint16_t len)
{
	struct tlv_box *vendor_elem;
	struct tlv *tlv;
	uint16_t sub_type;
	uint16_t sub_len;
	void *sub_value;

	if (!(vendor_elem = tlv_box_create()))
		return -ENOMEM;

	tlv_box_set_how(vendor_elem, SERIAL_EACH_WITH_ID);
	if (tlv_box_parse(vendor_elem, value, len)) {
		tlv_box_destroy(vendor_elem);
		return -EINVAL;
	}

	tlv_box_for_each_tlv(vendor_elem, tlv, sub_type, sub_len, sub_value) {
		switch (sub_type) {
		case CW_MSG_ELEMENT_VENDOR_SPEC_PAYLOAD_LED:
			vendor->led_status = cwmsg_parse_u8(sub_value);
		case CW_MSG_ELEMENT_VENDOR_SPEC_PAYLOAD_UPDATE_STATUS:
			vendor->updata_status = cwmsg_parse_u8(sub_value);
		}
	}
	tlv_box_destroy(vendor_elem);
	return 0;
}

/*****************************************************************************/

int cwmsg_assemble_ac_descriptor(struct cw_ctrlmsg *msg)
{
	struct message ac_descriptor;
	int total_len;
	struct tlv_box *elem = tlv_box_create();
	struct cw_ac_descriptor *desc = MALLOC(sizeof(*desc));

	if (!elem || !desc)
		return -ENOMEM;

	desc->station_num = 0;
	desc->station_limit = 256;
	desc->active_aps = 0;
	desc->max_aps = 256;
	desc->security = 0;
	desc->r_mac = 2;
	desc->dtls = 2;
	if (get_hardware(desc->hardware_version, sizeof(desc->hardware_version)))
		strncpy(desc->hardware_version, "AC", sizeof(desc->hardware_version));
	if (get_version(desc->software_version, sizeof(desc->software_version)))
		strncpy(desc->software_version, "Openwrt", sizeof(desc->software_version));

	tlv_box_set_how(elem, SERIAL_EACH_WITH_ID);
	if (tlv_box_put_string(elem, CW_AC_HARDWARE_VERSION, desc->hardware_version,
			       TLV_NOCPY | TLV_NOFREE) ||
	    tlv_box_put_string(elem, CW_AC_SOFTWARE_VERSION, desc->software_version,
			       TLV_NOCPY | TLV_NOFREE) ||
	    tlv_box_serialize(elem)) {
		tlv_box_destroy(elem);
		free(desc);
		return -ENOMEM;
	}

	total_len = 12 + tlv_box_get_size(elem);
	ac_descriptor.len = 0;
	ac_descriptor.data = MALLOC(total_len);
	if (!ac_descriptor.data) {
		tlv_box_destroy(elem);
		free(desc);
		return -ENOMEM;
	}
	cwmsg_put_u16(&ac_descriptor, desc->station_num);
	cwmsg_put_u16(&ac_descriptor, desc->station_limit);
	cwmsg_put_u16(&ac_descriptor, desc->active_aps);
	cwmsg_put_u16(&ac_descriptor, desc->max_aps);
	cwmsg_put_u8(&ac_descriptor, desc->security);
	cwmsg_put_u8(&ac_descriptor, desc->r_mac);
	cwmsg_put_u8(&ac_descriptor, desc->reserved);
	cwmsg_put_u8(&ac_descriptor, desc->dtls);
	cwmsg_put_raw(&ac_descriptor, tlv_box_get_buffer(elem), tlv_box_get_size(elem));
	tlv_box_destroy(elem);
	free(desc);

	return cwmsg_ctrlmsg_add_element(msg, CW_MSG_ELEMENT_AC_DESCRIPTOR_CW_TYPE,
					 &ac_descriptor, TLV_NOCPY);
}
