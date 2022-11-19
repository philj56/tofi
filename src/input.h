#ifndef INPUT_H
#define INPUT_H

#include <xkbcommon/xkbcommon.h>
#include "tofi.h"

void input_handle_keypress(struct tofi *tofi, xkb_keycode_t keycode);
void input_refresh_results(struct tofi *tofi);

#endif /* INPUT_H */
