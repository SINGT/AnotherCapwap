#ifndef __CAPWAP_CWStevens_HEADER__
#define __CAPWAP_CWStevens_HEADER__

#include <sys/socket.h>
#include <netinet/in.h>
#include "capwap_message.h"
#include "ac_mainloop.h"

#define		CW_CONTROL_PORT						5246
#define		CW_DATA_PORT						5247

char *sock_ntop_r(const struct sockaddr_storage *sa, char *str);
int sock_get_port(struct sockaddr_storage *sa);
void *sock_get_addr(struct sockaddr_storage *sa);
int sock_cpy_addr_port(struct sockaddr_storage *sa1, const struct sockaddr_storage *sa2);
void sock_set_port_cw(struct sockaddr_storage *sa, int port);
int sock_cmp_port(const struct sockaddr_storage *sa1, const struct sockaddr_storage *sa2, socklen_t salen);
int sock_cmp_addr(const struct sockaddr_storage *sa1, const struct sockaddr_storage *sa2, socklen_t salen);

int capwap_init_socket(int port, struct sockaddr_storage *client, int client_len);
ssize_t capwap_recv_message(int sock, void *buff, int len, struct sockaddr *addr, socklen_t *addr_len);
ssize_t capwap_recv_ctrl_message(int sock, void *buff, int len);
int capwap_send_message(int sock, struct iovec *io, size_t iovlen, struct sockaddr_storage *addr, int addr_len);
int capwap_send_ctrl_message(struct capwap_wtp *wtp, struct cw_ctrlmsg *msg);
int capwap_send_ctrl_message_unconnected(int sock, struct cw_ctrlmsg *msg, struct sockaddr_storage *addr, int addr_len);

int capwap_init_interface_sock(char *path);

#endif	/* __CAPWAP_CWStevens_HEADER__ */