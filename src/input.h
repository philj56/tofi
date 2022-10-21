#ifndef INPUT_H
#define INPUT_H

#include <xkbcommon/xkbcommon.h>
#include "tofi.h"

void input_handle_keypress(struct tofi *tofi, xkb_keycode_t keycode);

#endif /* INPUT_H */
