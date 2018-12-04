#include <stddef.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <event2/event.h>
#include <string.h>

#include "capwap_message.h"
#include "CWProtocol.h"
#include "network.h"
#include "tlv.h"
#include "CWLog.h"

void cwmsg_protohdr_parse(void *buff, struct cw_protohdr *header)
{
	uint32_t *val = buff;

	header->head.d32 = htonl(*val++);
	header->frag.d32 = htonl(*val);
}

void cwmsg_ctrlhdr_parse(void *msg, struct cw_ctrlhdr *ctrlhdr)
{
	ctrlhdr->type = ntohl(*(uint32_t *)msg);
	msg += sizeof(uint32_t);
	ctrlhdr->seq_num = *(uint8_t *)msg;
	msg += sizeof(uint8_t);
	ctrlhdr->length = htons(*(uint16_t *)msg);
	msg += sizeof(uint16_t);
	ctrlhdr->flags = *(uint8_t *)msg;
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

int cwmsg_ctrlmsg_add_element(struct cw_ctrlmsg *ctrlmsg, uint16_t type, uint16_t len, const void *value)
{
	if (!ctrlmsg)
		return -EINVAL;

	return tlv_box_put_raw(&ctrlmsg->elem_box, type, len, value);
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

uint8_t cwmsg_parse_u8(void *value)
{
	return *(uint8_t *)value;
}

uint16_t cwmsg_parse_u16(void *value)
{
	uint16_t tmp = *(uint16_t *)value;
	return htons(tmp);
}

uint32_t cwmsg_parse_u32(void *value)
{
	uint32_t tmp = *(uint32_t *)value;
	return htonl(tmp);
}

struct cw_elem_tlv_with_id *cwmsg_tlv_with_id_malloc()
{
	struct cw_elem_tlv_with_id *t = MALLOC(sizeof(*t));
	if (!t)
		return NULL;
	tlv_box_init(&t->sub_elem);
	return t;
}

void cwmsg_tlv_with_id_reinit(struct cw_elem_tlv_with_id *t)
{
	tlv_box_destroy(&t->sub_elem);
}

void cwmsg_tlv_with_id_free(struct cw_elem_tlv_with_id *t)
{
	tlv_box_destroy(&t->sub_elem);
	FREE(t);
}

int cwmsg_parse_tlv_with_id(struct cw_elem_tlv_with_id *elem, void *value, uint16_t len)
{
	// FIXME: Multi elem with different vendor_id parsed in the same struct?
	elem->vendor_id = cwmsg_parse_u32(value);
	return tlv_box_parse(&elem->sub_elem, value + sizeof(uint32_t), len - sizeof(uint32_t));
}

void cwmsg_destory_tlv_with_id(struct cw_elem_tlv_with_id *elem)
{
	tlv_box_destroy(&elem->sub_elem);
}

int cwmsg_parse_board_data(struct cw_wtp_board_data *board_data, struct cw_elem_tlv_with_id *elem)
{
	struct tlv *tlv;
	uint16_t type;
	uint16_t len;
	void *value;

	// TODO: Judge vendor ID?
	tlv_box_for_each_tlv(&elem->sub_elem, tlv, type, len, value) {
		switch (type) {
		case CW_WTP_MODEL_NUMBER:
			len = min(len, sizeof(board_data->model));
			cwmsg_parse_raw(board_data->model, sizeof(board_data->model), value, len);
			break;
		case CW_BASE_MAC:
			len = min(len, sizeof(board_data->mac));
			cwmsg_parse_raw(board_data->mac, sizeof(board_data->mac), value, len);
			break;
		}
	}

	return 0;
}
int cwmsg_parse_wtp_descriptor(struct cw_wtp_descriptor *desc, void *value, uint16_t len)
{
	struct cw_elem_tlv_with_id *id_tlv;
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
	if (!(id_tlv = cwmsg_tlv_with_id_malloc()))
		return -EINVAL;
	if (cwmsg_parse_tlv_with_id(id_tlv, value + offset, len - offset)) {
		cwmsg_tlv_with_id_free(id_tlv);
		return -EINVAL;
	}

	tlv_box_for_each_tlv(&id_tlv->sub_elem, tlv, sub_type, sub_len, sub_value) {
		switch (sub_type) {
		case CW_WTP_HARDWARE_VERSION:
			sub_len = min(sub_len, sizeof(desc->hardware_version));
			cwmsg_parse_raw(desc->hardware_version, sizeof(desc->hardware_version), sub_value, sub_len);
			break;
		case CW_WTP_SOFTWARE_VERSION:
			sub_len = min(sub_len, sizeof(desc->software_version));
			cwmsg_parse_raw(desc->software_version, sizeof(desc->software_version), sub_value, sub_len);
			break;
		}
	}
	cwmsg_tlv_with_id_free(id_tlv);
	return 0;
}

int cwmsg_parse_vendor_spec(struct cw_wtp_vendor_spec *vendor, void *value, uint16_t len)
{
	struct cw_elem_tlv_with_id *id_tlv;
	struct tlv *tlv;
	uint16_t sub_type;
	uint16_t sub_len;
	void *sub_value;

	if (!(id_tlv = cwmsg_tlv_with_id_malloc()))
		return -ENOMEM;

	if (cwmsg_parse_tlv_with_id(id_tlv, value, len)) {
		cwmsg_tlv_with_id_free(id_tlv);
		return -EINVAL;
	}

	tlv_box_for_each_tlv(&id_tlv->sub_elem, tlv, sub_type, sub_len, sub_value) {
		switch (sub_type) {
		case CW_MSG_ELEMENT_VENDOR_SPEC_PAYLOAD_LED:
			vendor->led_status = cwmsg_parse_u8(sub_value);
		case CW_MSG_ELEMENT_VENDOR_SPEC_PAYLOAD_UPDATE_STATUS:
			vendor->updata_status = cwmsg_parse_u8(sub_value);
		}
	}
	return 0;
}

