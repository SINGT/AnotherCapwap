#ifndef _AC_MAINLOOP_H_
#define _AC_MAINLOOP_H_

#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <sys/un.h>
#include <event2/event.h>

#include "capwap_message.h"
#include "CWProtocol.h"
#include "uci_config.h"
#include "capwap_common.h"

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
	struct sockaddr_storage data_addr;
	struct sockaddr_un wum_addr;
	char ip_addr[INET_ADDRMAXLEN];
	char if_path[32];
	int wtp_addr_len;
	int ctrl_sock;
	int data_sock;
	int if_sock;
	int seq_num;
	struct timeval ctrl_tv;
	struct event_base *ev_base;
	struct event *ctrl_ev;
	struct event *data_ev;
	struct evconnlistener *if_ev;
	void *if_args;

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

uint8_t get_echo_interval(struct capwap_wtp *wtp);
uint32_t get_idle_timeout(struct capwap_wtp *wtp);

int capwap_discovery_state(int sock, struct cw_ctrlhdr *ctrl_hdr, struct client_msg *addr);
int capwap_idle_to_join(struct capwap_wtp *wtp, struct cw_ctrlmsg *join_req);
int capwap_join_to_configure(struct capwap_wtp *wtp, struct cw_ctrlmsg *con_req);
int capwap_configure_to_data_check(struct capwap_wtp *wtp, struct cw_ctrlmsg *data_check_req);
int capwap_run(struct capwap_wtp *wtp, struct cw_ctrlmsg *ctrlmsg);

void capwap_data_channel(evutil_socket_t sock, short what, void *arg);

int capwap_init_main_interface(void);
void capwap_destroy_main_interface(void);
int capwap_init_wtp_interface(struct capwap_wtp *wtp);

#endif // _AC_MAINLOOP_H_
