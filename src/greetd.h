#ifndef GREETD_H
#define GREETD_H

#include <json-c/json_object.h>

enum greetd_request_type {
	GREETD_REQUEST_CREATE_SESSION,
	GREETD_REQUEST_POST_AUTH_MESSAGE_RESPONSE,
	GREETD_REQUEST_START_SESSION,
	GREETD_REQUEST_CANCEL_SESSION
};

enum greetd_response_type {
	GREETD_RESPONSE_INVALID,
	GREETD_RESPONSE_SUCCESS,
	GREETD_RESPONSE_ERROR,
	GREETD_RESPONSE_AUTH_MESSAGE
};

enum greetd_auth_message_type {
	GREETD_AUTH_MESSAGE_INVALID,
	GREETD_AUTH_MESSAGE_VISIBLE,
	GREETD_AUTH_MESSAGE_SECRET,
	GREETD_AUTH_MESSAGE_INFO,
	GREETD_AUTH_MESSAGE_ERROR
};

enum greetd_error_type {
	GREETD_ERROR_INVALID,
	GREETD_ERROR_AUTH_ERROR,
	GREETD_ERROR_ERROR
};

[[nodiscard]] struct json_object *greetd_create_session(const char *username);
[[nodiscard]] struct json_object *greetd_post_auth_message_response(const char *response);
[[nodiscard]] struct json_object *greetd_start_session(const char *command);
[[nodiscard]] struct json_object *greetd_cancel_session(void);

enum greetd_response_type greetd_parse_response_type(struct json_object *response);
enum greetd_auth_message_type greetd_parse_auth_message_type(struct json_object *response);
enum greetd_error_type greetd_parse_error_type(struct json_object *response);

#endif /* GREETD_H */
