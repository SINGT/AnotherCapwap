#if !defined(_CAPWAP_COMMON_H_)
#define _CAPWAP_COMMON_H_

#define BIT(n)	(1UL << (n))

#define MALLOC(n)	calloc(1, n)
#define FREE(p)                                                                                    \
	do {                                                                                       \
		if (p)                                                                             \
			free(p);                                                                   \
		p = NULL;                                                                          \
	} while (0)

#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))

#define AC_NAME "AC"

#define INET_ADDRMAXLEN 64

struct message {
	size_t len;
	void *data;
};

#include <netinet/in.h>
int get_version(char *buff, int len);
int get_hardware(char *buff, int len);
int get_model(char *buff, int len);
in_addr_t get_ipv4_addr(char *if_name);

#endif // _CAPWAP_COMMON_H_
