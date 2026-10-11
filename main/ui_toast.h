#pragma once

#include "ui_theme.h"

void ui_toast_show(ui_status_kind_t kind,
                       const char *title,
                       const char *detail);
void ui_toast_close(void);

typedef enum { UI_NOTIFY_QUIET=0, UI_NOTIFY_STANDARD=1, UI_NOTIFY_DETAILED=2 } ui_notification_level_t;
ui_notification_level_t ui_toast_level(void);
bool ui_toast_set_level(ui_notification_level_t level);
/* Compatibility with older callers/backups: OFF=Quiet, ON=Detailed. */
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

#include "moonraker.h"
void ui_toast_observe_printer(const moonraker_state_t *state);

/* UI-thread observer for confirmed inactive-profile health transitions. */
void ui_toast_observe_profiles(const moonraker_state_t *active_state);
void ui_toast_set_printer_chooser_action(void (*action)(void));
