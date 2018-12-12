#ifndef __CAPWAP_PROTOCOL_H__
#define __CAPWAP_PROTOCOL_H__

#include <stdint.h>
#include "capwap_common.h"
#include "tlv.h"

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
	struct tlv_box *elem_box;
};

#define CAPWAP_HEADER_LEN(header) ((header).head.b.hlen * 4)
#define CAPWAP_CONTROL_HEADER_LEN	sizeof(struct cw_ctrlhdr)

void cwmsg_protohdr_parse(void *buff, struct cw_protohdr *header);
void cwmsg_ctrlhdr_parse(void *msg, struct cw_ctrlhdr *ctrlhdr);

struct cw_ctrlmsg *cwmsg_ctrlmsg_malloc();
void cwmsg_ctrlmsg_free(struct cw_ctrlmsg *msg);
struct cw_ctrlmsg *cwmsg_ctrlmsg_new(uint32_t type, uint8_t seq);
void cwmsg_ctrlmsg_destroy(struct cw_ctrlmsg *msg);

int cwmsg_ctrlmsg_parse(struct cw_ctrlmsg *ctrlmsg, void *msg, int len);
void cwmsg_protohdr_set(struct cw_protohdr *header, int radio_id, int keep_alive);
int cwmsg_ctrlmsg_add_element(struct cw_ctrlmsg *ctrlmsg, uint16_t type, struct message *msg, int flag);
int cwmsg_ctrlmsg_serialize(struct cw_ctrlmsg *ctrlmsg);
void *cwmsg_ctrlmsg_get_buffer(struct cw_ctrlmsg *ctrlmsg);
int cwmsg_ctrlmsg_get_total_len(struct cw_ctrlmsg *ctrlmsg);
int cwmsg_ctrlmsg_get_msg_len(struct cw_ctrlmsg *ctrlmsg);
uint32_t cwmsg_ctrlmsg_get_type(struct cw_ctrlmsg *msg);
uint8_t cwmsg_ctrlmsg_get_seqnum(struct cw_ctrlmsg *ctrlmsg);
char *cwmsg_parse_string(void *value, uint16_t str_len);
void cwmsg_parse_raw(void *dst, uint16_t dst_len, void *value, uint16_t len);
uint8_t cwmsg_parse_u8(void *value);
uint16_t cwmsg_parse_u16(void *value);
uint32_t cwmsg_parse_u32(void *value);

void *capwap_manage_wtp(void *arg);
struct capwap_wtp;

static struct tlv *__internal_tlv  __attribute__((unused));
#define cwmsg_ctrlmsg_for_each_elem(msg_ptr, type, len, value)                                      \
	tlv_box_for_each_tlv((msg_ptr)->elem_box, __internal_tlv, type, len, value)

#define VERSION_LEN 64
struct cw_wtp_board_data {
	uint32_t vendor_id;
	char model[VERSION_LEN];
	uint8_t serial[VERSION_LEN];
	char hardware_version[VERSION_LEN];
	uint8_t mac[6];
};
int cwmsg_parse_board_data(struct cw_wtp_board_data *board_data, void *elem_value, uint16_t elem_len);

struct cw_wtp_descriptor {
	uint8_t max_radio;
	uint8_t radio_in_use;
	uint8_t num_encrypt;
	uint8_t encryp_wbid;
	uint16_t encryp_cap;
	char hardware_version[VERSION_LEN];
	char software_version[VERSION_LEN];
	char boot_version[VERSION_LEN];
};
int cwmsg_parse_wtp_descriptor(struct cw_wtp_descriptor *desc, void *value, uint16_t len);

struct cw_wtp_vendor_spec {
	uint32_t led_status;
	uint32_t updata_status;
};
int cwmsg_parse_vendor_spec(struct cw_wtp_vendor_spec *vendor, void *value, uint16_t len);

int cwmsg_parse_wifi_info(struct capwap_wtp *wtp, void *value, uint16_t len);

struct cw_ac_descriptor {
	uint16_t station_num;
	uint16_t station_limit;
	uint16_t active_aps;
	uint16_t max_aps;
	uint8_t security;
	uint8_t r_mac;
	uint8_t reserved;
	uint8_t dtls;
	char hardware_version[VERSION_LEN];
	char software_version[VERSION_LEN];
};
int cwmsg_assemble_ac_descriptor(struct cw_ctrlmsg *msg);

struct cw_elem_ipv4_addr {
	in_addr_t addr;
	uint16_t wtp_count;
} __attribute__((packed));
int cwmsg_assemble_ipv4_addr(struct cw_ctrlmsg *msg, uint16_t type, char *if_name);

int cwmsg_assemble_result_code(struct cw_ctrlmsg *msg, uint32_t result);
int cwmsg_assemble_string(struct cw_ctrlmsg *msg, uint16_t type, char *str, int flag);
int cwmsg_assemble_timers(struct cw_ctrlmsg *msg, struct capwap_wtp *wtp);
int cwmsg_assemble_idle_timeout(struct cw_ctrlmsg *msg, struct capwap_wtp *wtp);
int cwmsg_assemble_wtp_fallback(struct cw_ctrlmsg *msg, struct capwap_wtp *wtp);

#define SSID_MAX_LEN 32
struct wifi_info {
	uint8_t radio_id;
	uint8_t wlan_id;
	uint16_t capability;
	uint8_t key_index;
	uint8_t key_status;
	uint16_t key_len;
	uint8_t key[4];
	uint8_t group_tsc[6];
	uint8_t qos;
	uint8_t auth_type;
	uint8_t mac_mode;
	uint8_t tunnel_mode;
	uint8_t suppress_ssid;
	uint8_t channel;
	uint8_t country_code[2];
	char ssid[SSID_MAX_LEN + 1];
	uint8_t wpa;
	char password[64];
	uint8_t bssid[6];
	uint8_t setted;
};
int cwmsg_assemble_add_wlan(struct cw_ctrlmsg *msg, struct wifi_info *wifi, struct capwap_wtp *wtp);
int cwmsg_parse_wlan_config_response(struct cw_ctrlmsg *msg, struct capwap_wtp *wtp);

#endif
