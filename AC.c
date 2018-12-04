#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <errno.h>
#include <pthread.h>

#include <event2/event.h>

#include "tlv.h"
#include "capwap_message.h"
#include "network.h"
#include "CWProtocol.h"
#include "ac_mainloop.h"
#include "uci_config.h"
#include "CWLog.h"

int gLoggingLevel = DEFAULT_LOGGING_LEVEL;
int gEnabledLog = 1;

static void capwap_main_loop(evutil_socket_t sock, short what, void *arg)
{
	unsigned char buff[1024];
	struct client_msg client;
	char addr[INET_ADDRMAXLEN];
	struct cw_protohdr header;
	struct cw_ctrlhdr ctrl_hdr;
	int ret_len;

	if (!(what & EV_READ)) {
		CWLog("%s not read event", __func__);
		return;
	}

	client.addr_len = sizeof(client.addr);
	while ((ret_len = recvfrom(sock, buff, sizeof(buff), 0, (struct sockaddr *)&client.addr, &client.addr_len)) < 0) {
		if (errno == EINTR)
			continue;
		else
			goto fail;
	}

	cwmsg_protohdr_parse(buff, &header);
	cwmsg_ctrlhdr_parse(buff + CAPWAP_HEADER_LEN(header), &ctrl_hdr);

	if (ctrl_hdr.type == CW_MSG_TYPE_VALUE_DISCOVERY_REQUEST) {
		CWLog("receive a discovery request from %s", sock_ntop_r(&client.addr, addr));
		client.data = buff;
		client.data_len = ret_len;
		capwap_discovery_state(sock, &ctrl_hdr, &client);
		return;
	} else {
		CWWarningLog("Receive unexpected request %d from %s", sock_ntop_r(&client.addr, addr));
	}

	return;

fail:
	CWWarningLog("Error receiving message(%d):%s", errno, strerror(errno));
	return;
}

int main(int argc, char const *argv[])
{
	int sock;
	struct event_base *base;
	struct event *new_ev;
	int err = 0;

	if (err = uci_interface_init()) {
		CWLog("Uci interface init error: %s", strerror(-err));
		return err;
	}

	if ((err = capwap_init_main_interface())) {
		CWLog("Capwap init main interface fail: %s", strerror(-err));
		goto err_interface;
	}

	if ((sock = capwap_init_socket(CW_CONTROL_PORT, NULL, 0)) < 0) {
		CWCritLog("Can't create basic sock");
		goto err_socket;
	}

	base = event_base_new();
	new_ev = event_new(base, sock, EV_READ | EV_PERSIST, capwap_main_loop, NULL);

	event_add(new_ev, NULL);
	CWLog("start main loop");
	event_base_dispatch(base);
	CWLog("end main loop");

	close(sock);
err_socket:
	capwap_destory_main_interface();
err_interface:
	uci_interface_free();

	return err;
}

