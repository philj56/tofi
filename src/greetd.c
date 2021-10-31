#include "greetd.h"
#include "ipc.h"
#include <json-c/json_object.h>
#include <string.h>

struct json_object *greetd_create_session(const char *username)
{
	struct json_object *request = json_object_new_object();

	struct json_object *type = json_object_new_string("create_session");
	json_object_object_add(request, "type", type);

	struct json_object *name = json_object_new_string(username);
	json_object_object_add(request, "username", name);

	struct json_object *resp = ipc_submit(request);
	json_object_put(request);
	return resp;
}

struct json_object *greetd_post_auth_message_response(const char *response)
{
	struct json_object *request = json_object_new_object();

	struct json_object *type = json_object_new_string("post_auth_message_response");
	json_object_object_add(request, "type", type);

	if (response != NULL) {
		struct json_object *resp = json_object_new_string(response);
		json_object_object_add(request, "response", resp);
	}

	struct json_object *resp = ipc_submit(request);
	json_object_put(request);
	return resp;
}

struct json_object *greetd_start_session(const char *command)
{
	struct json_object *request = json_object_new_object();

	struct json_object *type = json_object_new_string("start_session");
	json_object_object_add(request, "type", type);

	struct json_object *arr = json_object_new_array_ext(1);
	struct json_object *cmd = json_object_new_string(command);
	json_object_array_add(arr, cmd);
	json_object_object_add(request, "cmd", arr);

	struct json_object *resp = ipc_submit(request);
	json_object_put(request);
	return resp;
}

struct json_object *greetd_cancel_session(void)
{
	struct json_object *request = json_object_new_object();

	struct json_object *type = json_object_new_string("cancel_session");
	json_object_object_add(request, "type", type);

	struct json_object *resp = ipc_submit(request);
	json_object_put(request);
	return resp;
}

enum greetd_response_type greetd_parse_response_type(struct json_object *response)
{
	const char *str = json_object_get_string(json_object_object_get(response, "type"));
	if (!strcmp(str, "success")) {
		return GREETD_RESPONSE_SUCCESS;
	}
	if (!strcmp(str, "error")) {
		return GREETD_RESPONSE_ERROR;
	}
	if (!strcmp(str, "auth_message")) {
		return GREETD_RESPONSE_AUTH_MESSAGE;
	}
	return GREETD_RESPONSE_INVALID;
}

enum greetd_auth_message_type greetd_parse_auth_message_type(struct json_object *response)
{
	const char *str = json_object_get_string(json_object_object_get(response, "auth_message_type"));
	if (!strcmp(str, "visible")) {
		return GREETD_AUTH_MESSAGE_VISIBLE;
	}
	if (!strcmp(str, "secret")) {
		return GREETD_AUTH_MESSAGE_SECRET;
	}
	if (!strcmp(str, "info")) {
		return GREETD_AUTH_MESSAGE_INFO;
	}
	if (!strcmp(str, "error")) {
		return GREETD_AUTH_MESSAGE_ERROR;
	}
	return GREETD_AUTH_MESSAGE_INVALID;
}

enum greetd_error_type greetd_parse_error_type(struct json_object *response)
{
	const char *str = json_object_get_string(json_object_object_get(response, "error_type"));
	if (!strcmp(str, "auth_error")) {
		return GREETD_ERROR_AUTH_ERROR;
	}
	if (!strcmp(str, "error")) {
		return GREETD_ERROR_ERROR;
	}
	return GREETD_ERROR_INVALID;
}
