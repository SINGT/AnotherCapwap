#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/un.h>
#include <unistd.h>
#include <getopt.h>
#include <json-c/json.h>

#include "ac_interface.h"
#include "CWLog.h"

typedef int (*cmd_fn)(void *);

int printWTPList(void *);
int do_update_cmd(void *);

struct command {
	char *name;
	cmd_fn func;
};

struct capwap_interface_message {
	uint32_t cmd;
	uint16_t type;
	uint16_t length;
};

struct command CMDs[] = {
	{"wtps", printWTPList},
	{"update", do_update_cmd},
	{NULL, NULL}
};

void usage(char *name)
{
	printf("%s -c command \n", name);
	printf("\nAvailable commands:\n");
	printf("   wtps: list of active wtps.\n");
	printf(" update: sends a cup (specified with -f) to the specified list of wtps (use -w or -n).\n");
}

/* Read "n" bytes from a descriptor. */
int readn(int fd, void *vptr, size_t n)
{
	size_t nleft = n;
	ssize_t nread = 0;
	char *ptr = vptr;

	while (nleft > 0) {
		if ((nread = recv(fd, ptr, nleft, 0)) < 0) {
			if (errno == EINTR)
				nread = 0; /* and call read() again */
			else
				return (-1);
		} else if (nread == 0)
			break; /* EOF */

		nleft -= nread;
		ptr += nread;
	}
	return (n - nleft); /* return >= 0 */
}

/* Write "n" bytes to a descriptor. */
int writen(int fd, const void *vptr, size_t n)
{
	size_t nleft = n;
	ssize_t nwritten = 0;
	const char *ptr = vptr;

	while (nleft > 0) {
		if ((nwritten = send(fd, ptr, nleft, 0)) <= 0) {
			if (errno == EINTR)
				nwritten = 0; /* and call write() again */
			else
				return (-1); /* error */
		}

		nleft -= nwritten;
		ptr += nwritten;
	}
	return (n);
}

static const char *get_string_of_json_key(json_object *json, const char *key)
{
	struct json_object *key_obj = json_object_object_get_old(json, key);
	if (!key_obj)
		return NULL;

	return json_object_get_string(key_obj);
}

static int connect_to_dev(char *mac)
{
	struct sockaddr_un server, client;
	int sock;

	memset(&server, 0, sizeof(server));
	memset(&client, 0, sizeof(client));
	server.sun_family = AF_LOCAL;
	snprintf(server.sun_path, sizeof(server.sun_path), "/tmp/WTP.%s", mac);
	client.sun_family = AF_LOCAL;
	snprintf(client.sun_path, sizeof(client.sun_path), "/tmp/WUM.%s", mac);

	sock = socket(AF_LOCAL, SOCK_STREAM, 0);
	if (bind(sock, (struct sockaddr *)&client, sizeof(client)) < 0) {
		perror("bind");
		return -errno;
	}
	if (connect(sock, (struct sockaddr *)&client, sizeof(client)) < 0) {
		perror("connect");
		return -errno;
	}
	return sock;
}

int receive_result(int sock)
{
	struct capwap_interface_message if_msg = {0};
	void *buffer;
	int result = 0;
	int len;

	do {
		if (readn(sock, &if_msg, sizeof(if_msg)) < 0) {
			perror("Read result");
			return errno;
		}

		buffer = malloc(if_msg.length);
		if (if_msg.length && !buffer)
			return ENOMEM;

		if (readn(sock, buffer, if_msg.length) < 0) {
			perror("read message");
			return errno;
		}
		if (if_msg.type == MSG_TYPE_STRING) {
			printf("%s\n", buffer);
		} else if (if_msg.type == MSG_TYPE_RESULT) {
			len = if_msg.length < sizeof(result) ? if_msg.length : sizeof(result);
			memcpy(&result, buffer, len);
		}
	} while (if_msg.cmd != MSG_END_CMD);

	return result;
}

int do_json_cmd(char *json_msg)
{
	struct capwap_interface_message if_msg = {0};
	json_object *msg_obj, *cmd_obj;
	char *command;
	char *dev;
	int server;

	msg_obj = json_tokener_parse(json_msg);
	if (is_error(msg_obj))
		return -EINVAL;

	dev = get_string_of_json_key(msg_obj, "device");
	command = get_string_of_json_key(msg_obj, "command");
	if (dev && !strcmp(command, "delete_device"))
		server = connect_to_dev(dev);
	else
		server = connect_to_dev(AC_MAIN_MAC);
	if (server < 0)
		return ENODEV;

	if_msg.cmd = JSON_CMD;
	if_msg.length = strlen(json_msg);
	writen(server, &if_msg, sizeof(if_msg));
	writen(server, json_msg, if_msg.length);

	return receive_result(server);
}

cmd_fn find_func(char *cmd)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(CMDs); i++) {
		if (!strcmp(cmd, CMDs[i].name))
			return CMDs[i].func;
	}

	return NULL;
}

int main(int argc, char const *argv[])
{
	int c = -1;
	char *json_msg = NULL;
	char *command = NULL;
	cmd_fn func;

	while ((c = getopt(argc, argv, "ha:p:w:c:f:n:s:r:l:t:m:j:")) != -1)
		switch (c) {
		case 'j':
			json_msg = optarg;
			break;
		case 'c':
			command = optarg;
			break;
		case 'h':
			usage(argv[0]);
			return 0;
		default:
			usage(argv[0]);
			abort();
		}

	if (json_msg)
		return do_json_cmd(json_msg);

	if (command) {
		func = find_func(command);
		if (!func) {
			printf("Command not support\n");
			usage(argv[0]);
			return -1;
		}
		return func(argv[1]);
	}
	return 0;
}
