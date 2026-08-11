#pragma once

typedef void (*ui_language_picker_changed_cb_t)(void);

void ui_language_picker_show(ui_language_picker_changed_cb_t changed_cb);
void ui_language_picker_close(void);
