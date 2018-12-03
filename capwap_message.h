#ifndef __CAPWAP_PROTOCOL_H__
#define __CAPWAP_PROTOCOL_H__

#include <stdint.h>
#include "tlv/tlv.h"

struct client_msg {
	uint32_t data_len;
	void *data;
	struct sockaddr_storage addr;
	socklen_t addr_len;
};

struct cw_protohdr {
	union {
		uint32_t d32;
		struct {
			uint32_t flags : 3;
			uint32_t K : 1;
			uint32_t M : 1;
			uint32_t W : 1;
			uint32_t L : 1;
			uint32_t F : 1;
			uint32_t T : 1;
			uint32_t wbid : 5;
			uint32_t rid : 5;
			uint32_t hlen : 5;
			uint32_t pream_type : 4;
			uint32_t pream_version : 4;
		} b;
	} head;
	union {
		uint32_t d32;
		struct {
			uint16_t rsvd : 3;
			uint16_t offset : 13;
			uint16_t id;
		} b;
	} frag;
}__attribute__((packed));


struct cw_ctrlhdr {
	uint32_t type;
	uint8_t seq_num;
	uint16_t length;
	uint8_t flags;
}__attribute__((packed));

struct cw_ctrlmsg {
	struct cw_protohdr protohdr;
	struct cw_ctrlhdr ctrlhdr;
	void *raw_hdr; // header buffer for sending message
	int raw_hdr_len;
	struct tlv_box elem_box;
};

#define CAPWAP_HEADER_LEN(header) ((header).head.b.hlen * 4)
#define CAPWAP_CONTROL_HEADER_LEN	sizeof(struct cw_ctrlhdr)

#define MALLOC(n)	calloc(1, n)
#define FREE(p) \
	do {               \
		if (p)         \
			free(p);   \
		p = NULL;      \
	} while (0)

#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))

void cwmsg_protohdr_parse(void *buff, struct cw_protohdr *header);
void cwmsg_ctrlhdr_parse(void *msg, struct cw_ctrlhdr *ctrlhdr);

struct cw_ctrlmsg *cwmsg_ctrlmsg_malloc();
void cwmsg_ctrlmsg_free(struct cw_ctrlmsg *msg);
struct cw_ctrlmsg *cwmsg_ctrlmsg_new(uint32_t type, uint8_t seq);
void cwmsg_ctrlmsg_destory(struct cw_ctrlmsg *msg);

int cwmsg_ctrlmsg_parse(struct cw_ctrlmsg *ctrlmsg, void *msg, int len);
void cwmsg_protohdr_set(struct cw_protohdr *header, int radio_id, int keep_alive);
int cwmsg_ctrlmsg_add_element(struct cw_ctrlmsg *ctrlmsg, uint16_t type, uint16_t len, const void *value);
int cwmsg_ctrlmsg_serialize(struct cw_ctrlmsg *ctrlmsg);
void *cwmsg_ctrlmsg_get_buffer(struct cw_ctrlmsg *ctrlmsg);
int cwmsg_ctrlmsg_get_total_len(struct cw_ctrlmsg *ctrlmsg);
int cwmsg_ctrlmsg_get_msg_len(struct cw_ctrlmsg *ctrlmsg);
uint32_t cwmsg_ctrlmsg_get_type(struct cw_ctrlmsg *msg);
char *cwmsg_parse_string(void *value, uint16_t str_len);
void cwmsg_parse_raw(void *dst, uint16_t dst_len, void *value, uint16_t len);
uint8_t cwmsg_parse_u8(void *value);
uint16_t cwmsg_parse_u16(void *value);
uint32_t cwmsg_parse_u32(void *value);

void *capwap_manage_wtp(void *arg);

static struct tlv *__internal_tlv  __attribute__((unused));
#define cwmsg_ctrlmsg_for_each_elem(msg_ptr, type, len, value)                                      \
	tlv_box_for_each_tlv(&(msg_ptr)->elem_box, __internal_tlv, type, len, value)

struct cw_elem_tlv_with_id {
	uint32_t vendor_id;
	struct tlv_box sub_elem;
};

struct cw_elem_tlv_with_id *cwmsg_tlv_with_id_malloc();
void cwmsg_tlv_with_id_reinit(struct cw_elem_tlv_with_id *t);
void cwmsg_tlv_with_id_free(struct cw_elem_tlv_with_id *t);
int cwmsg_parse_tlv_with_id(struct cw_elem_tlv_with_id *elem, void *value, uint16_t len);

struct cw_wtp_board_data {
	uint32_t vendor_id;
	uint8_t model[32];
	uint8_t serial[32];
	uint8_t hardware_version[32];
	uint8_t mac[6];
};

int cwmsg_parse_board_data(struct cw_wtp_board_data *board_data, struct cw_elem_tlv_with_id *elem);

struct cw_wtp_descriptor {
	uint8_t max_radio;
	uint8_t radio_in_use;
	uint8_t num_encrypt;
	uint8_t encryp_wbid;
	uint16_t encryp_cap;
	uint8_t hardware_version[32];
	uint8_t software_version[32];
	uint8_t boot_version[32];
};

int cwmsg_parse_wtp_descriptor(struct cw_wtp_descriptor *desc, void *value, uint16_t len);

struct cw_wtp_vendor_spec {
	uint32_t led_status;
	uint32_t updata_status;
};

#endif
