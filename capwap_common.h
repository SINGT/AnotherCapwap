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

struct message {
	size_t len;
	void *data;
};

int get_version(char *buff, int len);
int get_hardware(char *buff, int len);
int get_model(char *buff, int len);

#endif // _CAPWAP_COMMON_H_
