#include <json-c/json_object.h>
#include <json-c/json_tokener.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include "ipc.h"
#include "log.h"

static int ipc_open(void);
static int ipc_send(int socket, struct json_object *request);
static struct json_object *ipc_receive(int socket);

struct json_object *ipc_submit(struct json_object *request)
{
	int sock = ipc_open();
	if (sock == -1) {
		return NULL;
	}

	if (ipc_send(sock, request) == -1) {
		close(sock);
		return NULL;
	}

	struct json_object *resp = ipc_receive(sock);
	close(sock);
	return resp;
}

/*
 * Open a connection to the UNIX socket specified by
 * the environment variable GREETD_SOCK.
 *
 * Returns the socket file descriptor on success, or -1 on failure.
 */
int ipc_open(void)
{
	char *greetd_sock = getenv("GREETD_SOCK");

	if (greetd_sock == NULL) {
		log_error("GREETD_SOCK not set.\n");
		return -1;
	}

	int sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sock == -1) {
		log_error("Unable to create socket: %s\n", strerror(errno));
		return -1;
	}

	struct sockaddr_un remote = { .sun_family = AF_UNIX };
	strncpy(remote.sun_path, greetd_sock, sizeof(remote.sun_path));

	if (connect(sock, (struct sockaddr *)&remote, sizeof(remote)) == -1) {
		log_error("Unable to connect to greetd: %s\n", strerror(errno));
		close(sock);
		return -1;
	}
	return sock;
}

/*
 * Send an IPC request to the specified socket.
 *
 * Returns 0 on success, or -1 on failure.
 */
int ipc_send(int sock, struct json_object *request)
{
	const char *str = json_object_to_json_string(request);
	uint32_t len = strlen(str);

	if (send(sock, &len, sizeof(len), 0) == -1) {
		log_error("Error sending request size: %s\n", strerror(errno));
		return -1;
	}

	if (send(sock, str, len, 0) == -1) {
		log_error("Error sending request: %s\n", strerror(errno));
		return -1;
	}
	return 0;
}

/*
 * Receive an IPC response on the specified socket.
 *
 * Returns the response on success, or NULL on failure.
 */
struct json_object *ipc_receive(int sock)
{
	uint32_t len = 0;

	if (recv(sock, &len, sizeof(len), 0) != sizeof(len)) {
		log_error("Error receiving response size: %s\n", strerror(errno));
		return NULL;
	}

	char *buf = malloc(len + 1);
	if (recv(sock, buf, len, 0) != len) {
		log_error("Error receiving response: %s\n", strerror(errno));
		free(buf);
		return NULL;
	}

	buf[len] = '\0';

	enum json_tokener_error error;
	struct json_object *resp = json_tokener_parse_verbose(buf, &error);
	free(buf);

	if (resp == NULL) {
		log_error("Error parsing response: %s\n", json_tokener_error_desc(error));
	}
	return resp;

}
