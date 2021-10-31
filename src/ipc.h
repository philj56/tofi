#ifndef IPC_H
#define IPC_H

#include <json-c/json_object.h>

/*
 * Submit an IPC request to greetd.
 *
 * Returns the response on success, or NULL on failure.
 */
struct json_object *ipc_submit(struct json_object *request);

#endif /* IPC_H */
