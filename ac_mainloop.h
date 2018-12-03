#ifndef _AC_MAINLOOP_H_
#define _AC_MAINLOOP_H_

#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <sys/un.h>

#include "capwap_message.h"
#include "CWProtocol.h"
#include "uci_config.h"
#include "network.h"

enum wtp_state {
	IDLE,
	DISCOVERY,
	JOIN,
	CONFIGURE,
	IMAGE_DATA,
	DATA_CHECK,
	RUN,
	RESET,
	QUIT,
	SULK,
};

/**
 * This structure should have all the information we need for the wtp.
 * We don't hold message-format elements, instead we use the data parsed from the message.
 */
struct capwap_wtp {
	struct sockaddr_storage ctrl_addr;
	struct sockaddr_un wum_addr;
	char ip_addr[INET_ADDRMAXLEN];
	char if_path[32];
	int ctrl_addr_len;
	int ctrl_sock;
	int data_sock;
	int if_sock;
	int seq_num;
	struct timeval ctrl_tv;
	struct event_base *ev_base;
	struct event *ctrl_ev;
	struct event *data_ev;
	struct event *if_ev;

	enum wtp_state state;
	char *location;
	char *name;
	struct cw_wtp_board_data board_data;
	struct cw_wtp_descriptor descriptor;
	struct cw_wtp_vendor_spec vendor_spec;
	uint8_t session_id[CW_SESSION_ID_LENGTH];
	uint32_t version;

	struct device_attr *attr;
};

int capwap_discovery_state(int sock, struct cw_ctrlhdr *ctrl_hdr, struct client_msg *addr);
int capwap_idle_to_join(struct capwap_wtp *wtp, struct cw_ctrlmsg *join_req);

#endif // _AC_MAINLOOP_H_
