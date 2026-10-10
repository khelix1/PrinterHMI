#pragma once

#include "ui_theme.h"

void ui_toast_show(ui_status_kind_t kind,
                       const char *title,
                       const char *detail);
void ui_toast_close(void);

/* One appliance-wide preference; warnings/errors cannot be disabled. */
void ui_toast_init(void);
bool ui_toast_confirmations_enabled(void);
bool ui_toast_set_confirmations(bool enabled);
const char *ui_toast_preference_label(void);
void ui_toast_preferences_show(void (*changed)(void));
void ui_toast_preferences_close(void);

#include "ui_shell.h"
/* Persistent notice with an explicit destination for this captured printer. */
void ui_toast_show_link(ui_status_kind_t kind, const char *title, const char *detail, ui_shell_page_t page);
