#include "ui_bed_mesh_profiles.h"
#include "printer_connection_policy.h"
#include "moonraker_config_controller.h"
#include "ui_value_update.h"
#include "ui_text.h"
#include "ui_form_dialog.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ui_popup.h"
#include "ui_theme.h"

typedef struct {
    lv_obj_t *owner;
    uint32_t owner_generation;
    lv_obj_t *actions[4], *connection_label, *confirm_connection_label;
    lv_obj_t *profile_popup, *profile_list, *profile_editor;
    lv_obj_t *profile_name_input, *profile_confirm, *editor_feedback, *editor_continue;
    lv_obj_t *rows[BED_MESH_MAX_PROFILES];
    char pending_profile_name[BED_MESH_PROFILE_NAME_MAX];
    bed_mesh_profile_name_t reported_names[BED_MESH_MAX_PROFILES];
    bed_mesh_profile_name_t displayed_names[BED_MESH_MAX_PROFILES];
    size_t reported_count, displayed_count;
    int16_t selected_profile;
    bool mesh_valid, connection_blocked, confirmation_blocked, confirmation_removing;
    char active_name[BED_MESH_PROFILE_NAME_MAX];
    ui_bed_mesh_profiles_command_cb_t command;
} ui_bed_mesh_profiles_state_t;

static ui_bed_mesh_profiles_state_t s;
/* Kept outside resettable UI state so transport callbacks cannot reopen/dispatch. */
static uint32_t s_epoch;
static bool s_dispatching;

static void close_profile_editor_cb(lv_event_t *event);
static void close_profile_confirm_cb(lv_event_t *event);
static void close_profile_popup_cb(lv_event_t *event);

static bool valid_profile_name_ui(const char *name)
{
    if (!name || !name[0]) return false;
    size_t length = strnlen(name, BED_MESH_PROFILE_NAME_MAX);
    if (!length || length >= BED_MESH_PROFILE_NAME_MAX) return false;
    for (size_t i = 0; i < length; ++i) {
        char c = name[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.')) return false;
    }
    return true;
}

static const char *selected_profile_name(void)
{
    if (s.selected_profile < 0 || (size_t)s.selected_profile >= s.displayed_count) return NULL;
    return s.displayed_names[s.selected_profile];
}

static bool reported_profile(const char *name)
{
    if (!name) return false;
    for (size_t i = 0; i < s.reported_count; ++i)
        if (!strcmp(name, s.reported_names[i])) return true;
    return false;
}

static const char *profile_reason(void)
{
    moonraker_state_t live;
    moonraker_state_snapshot(&live);
    return s.owner_generation != moonraker_config_generation()
        ? "Printer changed - reopen Bed Mesh." : printer_idle_reason(&live);
}

void ui_bed_mesh_profiles_refresh_availability(void)
{
    const char *reason = profile_reason();
    bool selected = reported_profile(selected_profile_name());
    for (unsigned i = 0; i < 4; ++i) if (s.actions[i]) {
        bool blocked = reason[0] || s_dispatching || (i == 0 && !s.mesh_valid) ||
            ((i == 1 || i == 2) && !selected);
        if (blocked) lv_obj_add_state(s.actions[i], LV_STATE_DISABLED);
        else lv_obj_remove_state(s.actions[i], LV_STATE_DISABLED);
    }
    /* Do not erase rejected-command feedback on every page refresh. */
    if (reason[0]) {
        ui_value_set_text(s.connection_label, reason);
        ui_value_set_text(s.confirm_connection_label, reason);
        s.connection_blocked = s.connection_label != NULL;
        s.confirmation_blocked = s.confirm_connection_label != NULL;
    } else {
        if (s.connection_blocked) ui_value_set_text(s.connection_label, "");
        if (s.confirmation_blocked) ui_value_set_text(s.confirm_connection_label, "");
        s.connection_blocked = s.confirmation_blocked = false;
    }
}

static bool profile_command_ready(lv_obj_t *feedback)
{
    const char *reason = profile_reason();
    if (!reason[0]) return true;
    ui_value_set_text(feedback, reason);
    ui_bed_mesh_profiles_refresh_availability();
    return false;
}

static void profile_ref_deleted(lv_event_t *event)
{
    lv_obj_t *object = lv_event_get_target_obj(event);
    bool retired = false;
    for (unsigned i = 0; i < 4; ++i) if (s.actions[i] == object) {
        s.actions[i] = NULL;
        retired = true;
    }
    for (unsigned i = 0; i < BED_MESH_MAX_PROFILES; ++i) if (s.rows[i] == object) {
        s.rows[i] = NULL;
        if (s.selected_profile == (int16_t)i) s.selected_profile = -1;
        retired = true;
    }
    lv_obj_t **fields[] = {&s.profile_list, &s.connection_label, &s.confirm_connection_label,
        &s.profile_name_input, &s.editor_feedback, &s.editor_continue};
    for (unsigned i = 0; i < sizeof(fields) / sizeof(fields[0]); ++i)
        if (*fields[i] == object) { *fields[i] = NULL; retired = true; }
    if (retired) ++s_epoch;
}

static void watch_profile_ref(lv_obj_t *object)
{
    if (object) lv_obj_add_event_cb(object, profile_ref_deleted, LV_EVENT_DELETE, NULL);
}

static bool profile_action_event(lv_event_t *event, lv_obj_t *button)
{
    if (!button || lv_obj_has_state(button, LV_STATE_DISABLED)) return false;
    return !event || (lv_event_get_code(event) == LV_EVENT_CLICKED &&
        lv_event_get_target_obj(event) == button && lv_event_get_current_target_obj(event) == button);
}

static void retire_editor(void)
{
    ++s_epoch;
    s.profile_editor = NULL;
    s.profile_name_input = NULL;
    s.editor_feedback = NULL;
    s.editor_continue = NULL;
}

static void retire_confirmation(void)
{
    ++s_epoch;
    s.profile_confirm = NULL;
    s.confirm_connection_label = NULL;
    s.confirmation_blocked = false;
    s.confirmation_removing = false;
    s.actions[3] = NULL;
    s.pending_profile_name[0] = '\0';
}

static void close_profile_editor_cb(lv_event_t *event)
{
    (void)event;
    lv_obj_t *popup = s.profile_editor;
    retire_editor();
    if (popup) lv_obj_delete(popup);
}

static void close_profile_confirm_cb(lv_event_t *event)
{
    (void)event;
    lv_obj_t *popup = s.profile_confirm;
    retire_confirmation();
    if (popup) lv_obj_delete(popup);
}

static void close_profile_popup_cb(lv_event_t *event)
{
    (void)event;
    ++s_epoch;
    lv_obj_t *popup = s.profile_popup;
    lv_obj_t *editor = s.profile_editor;
    lv_obj_t *confirm = s.profile_confirm;
    s.profile_popup = NULL;
    s.profile_list = NULL;
    s.connection_label = NULL;
    s.connection_blocked = false;
    s.selected_profile = -1;
    memset(s.actions, 0, sizeof(s.actions));
    memset(s.rows, 0, sizeof(s.rows));
    /* Retire the complete old session before any DELETE callback can reopen it. */
    retire_editor();
    retire_confirmation();
    if (editor) lv_obj_delete(editor);
    if (confirm) lv_obj_delete(confirm);
    if (popup) lv_obj_delete(popup);
}

static void profile_popup_deleted(lv_event_t *event)
{
    lv_obj_t *popup = lv_event_get_target_obj(event);
    if (popup == s.profile_popup) {
        s.profile_popup = NULL;
        close_profile_popup_cb(NULL);
    } else if (popup == s.profile_confirm) retire_confirmation();
    else if (popup == s.profile_editor) retire_editor();
}

static void owner_deleted(lv_event_t *event)
{
    if (lv_event_get_target_obj(event) != s.owner) return;
    s.owner = NULL;
    ui_bed_mesh_profiles_close();
}

static void style_profile_selection(lv_obj_t *row,bool selected)
{
    ui_popup_set_selectable_row_selected(row,selected);
    /* Keep the name/badge alignment stable when theme selection skins refresh. */
    lv_obj_set_style_pad_hor(row,16,0);
    lv_obj_set_style_pad_ver(row,10,0);
}

static void profile_row_cb(lv_event_t *event)
{
    if (!event || !s.profile_popup || !s.profile_list || s_dispatching ||
        lv_event_get_code(event) != LV_EVENT_CLICKED) return;
    uintptr_t encoded = (uintptr_t)lv_event_get_user_data(event);
    if (!encoded || encoded - 1 >= s.displayed_count) return;
    lv_obj_t *selected = lv_event_get_target_obj(event);
    if (lv_event_get_current_target_obj(event) != selected ||
        lv_obj_get_parent(selected) != s.profile_list ||
        s.rows[encoded - 1] != selected ||
        lv_obj_has_state(selected, LV_STATE_DISABLED)) return;
    s.selected_profile = (int16_t)(encoded - 1);
    for (unsigned i = 0; s.profile_list && i < lv_obj_get_child_count(s.profile_list); ++i) {
        lv_obj_t *row = lv_obj_get_child(s.profile_list, i);
        style_profile_selection(row, row == selected);
    }
    ui_value_set_text(s.connection_label, reported_profile(selected_profile_name())
        ? "" : "This profile is no longer reported. Close and reopen to refresh the list.");
    ui_bed_mesh_profiles_refresh_availability();
}

static bool dispatch_profile_command(lv_obj_t *popup, lv_obj_t *feedback, const char *command)
{
    if (!popup || s_dispatching || !s.command || !profile_command_ready(feedback)) return false;
    uint32_t epoch = s_epoch, owner = s.owner_generation;
    ui_bed_mesh_profiles_command_cb_t send = s.command;
    s_dispatching = true;
    bool accepted = send(command);
    s_dispatching = false;
    if (s_epoch != epoch || s.owner_generation != owner || owner != moonraker_config_generation()) return false;
    if (!accepted) {
        ui_value_set_text(feedback, "Request was not accepted. Your selection is retained. Retry when connected.");
        ui_bed_mesh_profiles_refresh_availability();
        return false;
    }
    return true;
}

static void load_selected_profile_cb(lv_event_t *event)
{
    if (!s.profile_popup || s_dispatching || !profile_action_event(event, s.actions[1])) return;
    const char *name = selected_profile_name();
    if (!valid_profile_name_ui(name) || !reported_profile(name)) {
        ui_value_set_text(s.connection_label, "Select a reported profile. Close and reopen to refresh the list.");
        return;
    }
    char command[128];
    snprintf(command, sizeof(command), "BED_MESH_PROFILE LOAD=%s", name);
    if (dispatch_profile_command(s.profile_popup, s.connection_label, command)) close_profile_popup_cb(NULL);
}

static void confirmed_profile_command(lv_event_t *event, bool removing)
{
    if (!s.profile_confirm || s_dispatching || s.confirmation_removing != removing ||
        !profile_action_event(event, s.actions[3]) || !valid_profile_name_ui(s.pending_profile_name)) return;
    const char *operation = removing ? "REMOVE" : "SAVE";
    if ((!strcmp(operation, "REMOVE") && !reported_profile(s.pending_profile_name)) ||
        (!strcmp(operation, "SAVE") && !s.mesh_valid)) {
        ui_value_set_text(s.confirm_connection_label, "The reviewed profile or mesh is no longer available. Go back and review again.");
        return;
    }
    char command[128];
    snprintf(command, sizeof(command), "BED_MESH_PROFILE %s=%s\nSAVE_CONFIG", operation, s.pending_profile_name);
    /* Keep the exact reviewed script; Klipper stops before SAVE_CONFIG on error. */
    if (dispatch_profile_command(s.profile_confirm, s.confirm_connection_label, command)) close_profile_popup_cb(NULL);
}

static void confirm_save_profile_cb(lv_event_t *event) { confirmed_profile_command(event, false); }
static void confirm_remove_profile_cb(lv_event_t *event) { confirmed_profile_command(event, true); }

static void show_profile_confirmation(bool removing)
{
    if (!s.profile_popup || s_dispatching || !s.pending_profile_name[0]) return;
    if (s.profile_confirm) { lv_obj_move_foreground(s.profile_confirm); return; }
    s.profile_confirm = ui_form_dialog(ui_text(removing ? "Remove bed mesh profile?" : "Save bed mesh profile?"),
        650, 410, removing ? UI_POPUP_DANGER : UI_POPUP_STANDARD);
    if (!s.profile_confirm) { s.pending_profile_name[0] = '\0'; return; }
    ui_form_divide_title(s.profile_confirm);
    lv_obj_add_event_cb(s.profile_confirm, profile_popup_deleted, LV_EVENT_DELETE, NULL);
    s.confirmation_removing = removing;
    lv_obj_t *body=ui_form_body(s.profile_confirm);
    ui_form_text(body,"Profile name",UI_FONT_CAPTION,UI_TEXT_DIM);
    ui_form_text(body,s.pending_profile_name,UI_FONT_BODY_LARGE,UI_TEXT_BRIGHT);
    ui_form_text(body,removing ? "Remove this saved mesh profile." :
        "Save the current mesh under this name. An existing profile with the same name will be replaced.",UI_FONT_BODY,UI_TEXT);
    ui_form_text(body,"SAVE_CONFIG restarts Klipper. Wait for the printer to reconnect as Ready.",UI_FONT_CAPTION,UI_TEXT_DIM);
    s.confirm_connection_label = ui_form_text(body, "", UI_FONT_CAPTION, UI_TEXT_DIM);
    watch_profile_ref(s.confirm_connection_label);
    ui_form_action(s.profile_confirm, UI_BUTTON_SECONDARY, LV_SYMBOL_LEFT " Back", close_profile_confirm_cb, NULL);
    s.actions[3] = ui_form_action(s.profile_confirm, removing ? UI_BUTTON_DANGER : UI_BUTTON_PRIMARY,
        removing ? LV_SYMBOL_TRASH " Remove & restart" : LV_SYMBOL_SAVE " Save & restart",
        removing ? confirm_remove_profile_cb : confirm_save_profile_cb, NULL);
    watch_profile_ref(s.actions[3]);
    ui_bed_mesh_profiles_refresh_availability();
    ui_form_finish(s.profile_confirm);
}

static void save_name_continue_cb(lv_event_t *event)
{
    if (!s.profile_editor || !s.profile_name_input || s_dispatching ||
        !profile_action_event(event, s.editor_continue)) return;
    if (!profile_command_ready(s.editor_feedback)) return;
    const char *name = lv_textarea_get_text(s.profile_name_input);
    if (!valid_profile_name_ui(name)) {
        ui_value_set_text(s.editor_feedback, "Enter a name using letters, numbers, underscore, dot or hyphen.");
        return;
    }
    memcpy(s.pending_profile_name, name, strlen(name) + 1);
    uint32_t epoch = s_epoch;
    close_profile_editor_cb(NULL);
    if (s_epoch != epoch + 1U || !s.profile_popup || s.profile_editor || s.profile_confirm) return;
    show_profile_confirmation(false);
}

static void save_as_profile_cb(lv_event_t *event)
{
    if (!s.profile_popup || s_dispatching || !profile_action_event(event, s.actions[0])) return;
    if (s.profile_confirm) { lv_obj_move_foreground(s.profile_confirm); return; }
    if (s.profile_editor) { lv_obj_move_foreground(s.profile_editor); return; }
    if (!profile_command_ready(s.connection_label)) return;
    if (!s.mesh_valid) { ui_value_set_text(s.connection_label, "No current mesh is available to save."); return; }
    s.profile_editor = ui_form_dialog(ui_text("Save bed mesh profile"), 760, 560, UI_POPUP_STANDARD);
    if (!s.profile_editor) return;
    ui_form_divide_title(s.profile_editor);
    lv_obj_add_event_cb(s.profile_editor, profile_popup_deleted, LV_EVENT_DELETE, NULL);
    ui_form_text(ui_form_body(s.profile_editor), "Profile name", UI_FONT_CAPTION, UI_TEXT_DIM);
    s.profile_name_input = ui_form_input(ui_form_body(s.profile_editor), "example: textured_plate", "", false, BED_MESH_PROFILE_NAME_MAX - 1);
    lv_textarea_set_accepted_chars(s.profile_name_input, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.-");
    s.editor_feedback = ui_form_text(ui_form_body(s.profile_editor), "", UI_FONT_CAPTION, UI_TEXT_DIM);
    watch_profile_ref(s.profile_name_input);
    watch_profile_ref(s.editor_feedback);
    ui_form_keyboard(s.profile_editor, s.profile_name_input, LV_KEYBOARD_MODE_TEXT_LOWER);
    ui_form_action(s.profile_editor, UI_BUTTON_SECONDARY, LV_SYMBOL_CLOSE " Cancel", close_profile_editor_cb, NULL);
    s.editor_continue = ui_form_action(s.profile_editor, UI_BUTTON_PRIMARY, "Continue", save_name_continue_cb, NULL);
    watch_profile_ref(s.editor_continue);
    ui_form_finish(s.profile_editor);
}

static void remove_selected_profile_cb(lv_event_t *event)
{
    if (!s.profile_popup || s_dispatching || !profile_action_event(event, s.actions[2])) return;
    if (s.profile_confirm) { lv_obj_move_foreground(s.profile_confirm); return; }
    if (s.profile_editor) { lv_obj_move_foreground(s.profile_editor); return; }
    if (!profile_command_ready(s.connection_label)) return;
    const char *name = selected_profile_name();
    if (!valid_profile_name_ui(name) || !reported_profile(name)) return;
    memcpy(s.pending_profile_name, name, strlen(name) + 1);
    show_profile_confirmation(true);
}

void ui_bed_mesh_profiles_show_cb(lv_event_t *event)
{
    (void)event;
    if (s_dispatching) return;
    if (s.profile_popup) {
        lv_obj_move_foreground(s.profile_popup);
        if (s.profile_editor) lv_obj_move_foreground(s.profile_editor);
        if (s.profile_confirm) lv_obj_move_foreground(s.profile_confirm);
        return;
    }
    s.profile_popup = ui_form_dialog(ui_text("Bed mesh profiles"), 760, 520, UI_POPUP_STANDARD);
    if (!s.profile_popup) return;
    ui_form_divide_title(s.profile_popup);
    lv_obj_set_scrollbar_mode(ui_form_body(s.profile_popup), LV_SCROLLBAR_MODE_OFF);
    ++s_epoch;
    lv_obj_add_event_cb(s.profile_popup, profile_popup_deleted, LV_EVENT_DELETE, NULL);
    s.selected_profile = -1;
    s.displayed_count = s.reported_count;
    memcpy(s.displayed_names, s.reported_names, sizeof(s.displayed_names));
    ui_form_text(ui_form_body(s.profile_popup),"Select a saved profile to load or remove. Save as stores the current mesh.",UI_FONT_CAPTION,UI_TEXT_DIM);
    s.connection_label = ui_form_text(ui_form_body(s.profile_popup), "", UI_FONT_CAPTION, UI_TEXT_DIM);
    watch_profile_ref(s.connection_label);
    s.profile_list = lv_obj_create(ui_form_body(s.profile_popup));
    watch_profile_ref(s.profile_list);
    lv_obj_remove_style_all(s.profile_list);
    lv_obj_set_size(s.profile_list, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(s.profile_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(s.profile_list, 8, 0);
    lv_obj_clear_flag(s.profile_list, LV_OBJ_FLAG_SCROLLABLE);
    if (!s.displayed_count) ui_form_text(s.profile_list, "No saved bed mesh profiles reported.", UI_FONT_BODY, UI_TEXT_DIM);
    for (size_t i = 0; i < s.displayed_count; ++i) {
        const char *name = s.displayed_names[i];
        bool active = !strcmp(name, s.active_name);
        if (active) s.selected_profile = (int16_t)i;
        lv_obj_t *row = ui_popup_add_selectable_row(s.profile_list, name, 0, 0, 1, 52,
            profile_row_cb, (void *)(uintptr_t)(i + 1));
        if (row) {
            s.rows[i] = row;
            watch_profile_ref(row);
            lv_obj_set_width(row, LV_PCT(100));
            lv_obj_set_style_min_height(row, UI_FONT_BODY_LARGE->line_height + 20, 0);
            lv_obj_set_height(row,LV_SIZE_CONTENT);
            lv_obj_set_flex_flow(row,LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(row,LV_FLEX_ALIGN_START,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);
            lv_obj_set_style_pad_column(row,12,0);
            lv_obj_t *label = lv_obj_get_child(row, 0);
            lv_label_set_long_mode(label,LV_LABEL_LONG_WRAP);
            lv_obj_set_size(label,0,LV_SIZE_CONTENT);
            lv_obj_set_flex_grow(label,1);
            if(active){
                lv_obj_t *badge=lv_label_create(row);
                lv_label_set_text(badge,"Active");
                ui_apply_custom_label_style(badge,UI_FONT_CAPTION,UI_TEXT_DIM);
            }
            style_profile_selection(row, active);
        }
    }
    s.actions[0] = ui_form_action(s.profile_popup, UI_BUTTON_SECONDARY, LV_SYMBOL_SAVE " Save as", save_as_profile_cb, NULL);
    s.actions[1] = ui_form_action(s.profile_popup, UI_BUTTON_PRIMARY, LV_SYMBOL_PLAY " Load", load_selected_profile_cb, NULL);
    s.actions[2] = ui_form_action(s.profile_popup, UI_BUTTON_DANGER, LV_SYMBOL_TRASH " Remove", remove_selected_profile_cb, NULL);
    ui_form_action(s.profile_popup, UI_BUTTON_SECONDARY, LV_SYMBOL_CLOSE " Close", close_profile_popup_cb, NULL);
    for (unsigned i = 0; i < 3; ++i) watch_profile_ref(s.actions[i]);
    ui_bed_mesh_profiles_refresh_availability();
    ui_form_finish(s.profile_popup);
}

void ui_bed_mesh_profiles_init(lv_obj_t *owner, ui_bed_mesh_profiles_command_cb_t command_cb)
{
    ui_bed_mesh_profiles_close();
    s.owner = owner;
    s.owner_generation = moonraker_config_generation();
    s.command = command_cb;
    s.selected_profile = -1;
    if (owner) lv_obj_add_event_cb(owner, owner_deleted, LV_EVENT_DELETE, NULL);
}

void ui_bed_mesh_profiles_update(const bed_mesh_snapshot_t *mesh)
{
    s.mesh_valid = mesh && mesh->valid;
    s.reported_count = mesh && mesh->profile_names ? mesh->profile_count : 0;
    if (s.reported_count > BED_MESH_MAX_PROFILES) s.reported_count = BED_MESH_MAX_PROFILES;
    memset(s.reported_names, 0, sizeof(s.reported_names));
    for (size_t i = 0; i < s.reported_count; ++i) {
        memcpy(s.reported_names[i], mesh->profile_names[i], BED_MESH_PROFILE_NAME_MAX);
        s.reported_names[i][BED_MESH_PROFILE_NAME_MAX - 1] = '\0';
    }
    if (mesh) {
        memcpy(s.active_name, mesh->profile_name, sizeof(s.active_name));
        s.active_name[sizeof(s.active_name) - 1] = '\0';
    } else s.active_name[0] = '\0';
    ui_bed_mesh_profiles_refresh_availability();
}

void ui_bed_mesh_profiles_close(void)
{
    if (s.owner) lv_obj_remove_event_cb(s.owner, owner_deleted);
    lv_obj_t *popup = s.profile_popup, *editor = s.profile_editor, *confirm = s.profile_confirm;
    ++s_epoch;
    memset(&s, 0, sizeof(s));
    s.selected_profile = -1;
    if (editor) lv_obj_delete(editor);
    if (confirm) lv_obj_delete(confirm);
    if (popup) lv_obj_delete(popup);
}
