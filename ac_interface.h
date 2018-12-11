#ifndef _AC_INTERFACE_H
#define _AC_INTERFACE_H

#define JSON_BUFF_LEN 1000

#include <stdint.h>

#define JSON_CMD	1
#define LIST_CMD	2
#define UPDATE_CMD	3
#define MSG_END_CMD	4

#define MSG_TYPE_RESULT	1
#define MSG_TYPE_STRING	2
#define MSG_TYPE_EOF	3

struct capwap_interface_message {
	uint32_t cmd;
	uint16_t type;
	uint16_t length;
};

#define AC_MAIN_MAC "00_00_00_00_00_00"
#define AC_MAIN_INTERFACE "/tmp/WTP.00_00_00_00_00_00"

#endif // _AC_INTERFACE_H