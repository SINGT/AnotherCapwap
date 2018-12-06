#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#include "tlv.h"
#include "capwap_message.h"
#include "network.h"
#include "CWProtocol.h"
#include "ac_mainloop.h"
#include "CWLog.h"

static int capwap_new_client(struct client_msg *client)
{
	pthread_t thread;
	pthread_attr_t thread_attr;
	struct capwap_wtp *arg;
	int err;

	if (!client)
		return -EINVAL;

	if (!(arg = malloc(sizeof(*arg)))) {
		CWCritLog("No memory for new wtp!!!");
		return -ENOMEM;
	}

	memset(arg, 0, sizeof(*arg));
	memcpy(&arg->ctrl_addr, &client->addr, client->addr_len);
	arg->ctrl_addr_len = client->addr_len;
	if (!inet_ntop(client->addr.ss_family, &client->addr, arg->ip_addr, sizeof(arg->ip_addr))) {
		CWLog("convert ip string failed with %d", errno);
		free(arg);
		return -EINVAL;
	}

	// Start a new thread
	if ((err = pthread_attr_init(&thread_attr)) ||
	    (err = pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_DETACHED))) {
		CWCritLog("pthread_attr fail, no more new wtp");
		free(arg);
		return err;
	}
	if ((err = pthread_create(&thread, &thread_attr, capwap_manage_wtp, arg))) {
		CWWarningLog("create thread fail");
		free(arg);
		return err;
	}
	return 0;
}

static int capwap_send_discovery_response(int sock, struct cw_ctrlhdr *ctrl_hdr, struct client_msg *client)
{
	struct cw_ctrlmsg *discovery = cwmsg_ctrlmsg_new(CW_MSG_TYPE_VALUE_DISCOVERY_RESPONSE, ctrl_hdr->seq_num);

	CWLog("%s", __func__);
	cwmsg_assemble_string(discovery, CW_MSG_ELEMENT_AC_NAME_CW_TYPE, AC_NAME, TLV_NOFREE);
	cwmsg_assemble_ipv4_addr(discovery, CW_MSG_ELEMENT_CW_CONTROL_IPV4_ADDRESS_CW_TYPE, "br-lan");

	return capwap_send_ctrl_message_unconnected(sock, discovery, &client->addr, client->addr_len);
}

int capwap_discovery_state(int sock, struct cw_ctrlhdr *ctrl_hdr, struct client_msg *client)
{
	int err;

	// TODO: check discovery request to determine whether to send response or not

	// Start new thread to receive data before send response, so that we won't miss join request in new thread.
	if ((err = capwap_new_client(client))) {
		CWLog("start thread for new wtp failed, not send response");
		return err;
	}
	return capwap_send_discovery_response(sock, ctrl_hdr, client);
}