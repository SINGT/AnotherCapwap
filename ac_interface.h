#ifndef _AC_INTERFACE_H
#define _AC_INTERFACE_H

#define JSON_BUFF_LEN 1000

#include <stdint.h>

#define JSON_CMD 1
#define LIST_CMD 2
#define UPDATE_CMD 3

enum capwap_if_msg_t {
	U32_TYPE,
	STRING_TYPE,
	JSON_TYPE,
};

struct capwap_interface_message {
	uint32_t cmd;
	enum capwap_if_msg_t type;
};

#define AC_MAIN_INTERFACE "/tmp/WTP.00_00_00_00_00_00"

int capwap_init_main_interface();
void capwap_destroy_main_interface(void);
int capwap_init_wtp_interface(struct capwap_wtp *wtp);


#endif // _AC_INTERFACE_H