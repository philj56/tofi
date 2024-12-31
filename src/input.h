#ifndef INPUT_H
#define INPUT_H

#include <xkbcommon/xkbcommon.h>
#include "tofi.h"

typedef enum {
    MOUSE_EVENT_LEFT_CLICK,
    MOUSE_EVENT_RIGHT_CLICK,
    MOUSE_EVENT_MIDDLE_CLICK,
    MOUSE_EVENT_WHEEL_UP,
    MOUSE_EVENT_WHEEL_DOWN
} mouse_event_t;

void input_handle_mouse(struct tofi *tofi, mouse_event_t event);
void input_handle_keypress(struct tofi *tofi, xkb_keycode_t keycode);
void input_refresh_results(struct tofi *tofi);

#endif /* INPUT_H */
