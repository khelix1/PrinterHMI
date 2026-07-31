#include "ui_bed_mesh_profiles.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_heap_caps.h"
#include "ui_popup.h"
#include "ui_theme.h"

typedef struct {
    lv_obj_t *owner;
    lv_obj_t *profile_popup;
    lv_obj_t *profile_list;
    lv_obj_t *profile_editor;
    lv_obj_t *profile_name_input;
    lv_obj_t *profile_confirm;
    char *pending_profile_name;
    int16_t selected_profile;
    bed_mesh_snapshot_t mesh;
    ui_bed_mesh_profiles_command_cb_t command;
} ui_bed_mesh_profiles_state_t;

static ui_bed_mesh_profiles_state_t s;

static void free_pending_profile_name(void)
{
    if (s.pending_profile_name) {
        heap_caps_free(s.pending_profile_name);
        s.pending_profile_name = NULL;
    }
}

static bool valid_profile_name_ui(const char *name)
{
    if (!name || !name[0]) {
        return false;
    }

    size_t length =
        strnlen(name, BED_MESH_PROFILE_NAME_MAX);

    if (length == 0 ||
        length >= BED_MESH_PROFILE_NAME_MAX) {
        return false;
    }

    for (size_t i = 0; i < length; ++i) {
        char character = name[i];
        bool valid =
            (character >= 'a' && character <= 'z') ||
            (character >= 'A' && character <= 'Z') ||
            (character >= '0' && character <= '9') ||
            character == '_' ||
            character == '-' ||
            character == '.';

        if (!valid) {
            return false;
        }
    }

    return true;
}

static bool set_pending_profile_name(const char *name)
{
    if (!valid_profile_name_ui(name)) {
        return false;
    }

    size_t length = strlen(name);
    char *copy =
        heap_caps_calloc(
            length + 1,
            sizeof(char),
            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    if (!copy) {
        copy =
            heap_caps_calloc(
                length + 1,
                sizeof(char),
                MALLOC_CAP_8BIT);
    }

    if (!copy) {
        return false;
    }

    memcpy(copy, name, length + 1);
    free_pending_profile_name();
    s.pending_profile_name = copy;
    return true;
}

static void close_profile_editor_cb(lv_event_t *e)
{
    (void)e;

    s.profile_name_input = NULL;

    if (s.profile_editor) {
        lv_obj_t *popup = s.profile_editor;
        s.profile_editor = NULL;
        lv_obj_delete(popup);
    }
}

static void close_profile_confirm_cb(lv_event_t *e)
{
    (void)e;

    if (s.profile_confirm) {
        lv_obj_t *popup = s.profile_confirm;
        s.profile_confirm = NULL;
        lv_obj_delete(popup);
    }

    free_pending_profile_name();
}

static void close_profile_popup_cb(lv_event_t *e)
{
    (void)e;

    close_profile_editor_cb(NULL);
    close_profile_confirm_cb(NULL);

    s.profile_list = NULL;
    s.selected_profile = -1;

    if (s.profile_popup) {
        lv_obj_t *popup = s.profile_popup;
        s.profile_popup = NULL;
        lv_obj_delete(popup);
    }
}

static const char *selected_profile_name(void)
{
    if (s.selected_profile < 0 ||
        (size_t)s.selected_profile >= s.mesh.profile_count ||
        !s.mesh.profile_names) {
        return NULL;
    }

    return s.mesh.profile_names[s.selected_profile];
}

static void profile_row_cb(lv_event_t *e)
{
    uintptr_t encoded =
        (uintptr_t)lv_event_get_user_data(e);

    if (encoded == 0) {
        return;
    }

    size_t index = (size_t)(encoded - 1);

    if (index >= s.mesh.profile_count) {
        return;
    }

    s.selected_profile = (int16_t)index;

    lv_obj_t *selected = lv_event_get_target(e);

    if (s.profile_list) {
        uint32_t count =
            lv_obj_get_child_count(s.profile_list);

        for (uint32_t i = 0; i < count; ++i) {
            lv_obj_t *row =
                lv_obj_get_child(s.profile_list, i);

            ui_popup_set_selectable_row_selected(
                row,
                row == selected);
        }
    }
}

static void load_selected_profile_cb(lv_event_t *e)
{
    (void)e;

    const char *name = selected_profile_name();

    if (!name || !s.command) {
        return;
    }

    char command[128];
    int written = snprintf(
        command,
        sizeof(command),
        "BED_MESH_PROFILE LOAD=%s",
        name);

    if (written <= 0 ||
        (size_t)written >= sizeof(command)) {
        return;
    }

    ui_bed_mesh_profiles_command_cb_t send_command = s.command;
    close_profile_popup_cb(NULL);
    send_command(command);
}

static void confirmed_profile_command(
    const char *operation)
{
    if (!operation ||
        !s.pending_profile_name ||
        !s.command) {
        return;
    }

    char command[128];
    int written = snprintf(
        command,
        sizeof(command),
        "BED_MESH_PROFILE %s=%s",
        operation,
        s.pending_profile_name);

    if (written <= 0 ||
        (size_t)written >= sizeof(command)) {
        return;
    }

    ui_bed_mesh_profiles_command_cb_t send_command = s.command;

    /*
     * Close the viewer-owned modal surfaces before SAVE_CONFIG restarts
     * Klipper, while keeping the dedicated Bed Mesh page in place.
     */
    close_profile_popup_cb(NULL);
    send_command(command);
    send_command("SAVE_CONFIG");
}

static void confirm_save_profile_cb(lv_event_t *e)
{
    (void)e;
    confirmed_profile_command("SAVE");
}

static void confirm_remove_profile_cb(lv_event_t *e)
{
    (void)e;
    confirmed_profile_command("REMOVE");
}

static void show_profile_confirmation(
    bool removing)
{
    if (!s.pending_profile_name) {
        return;
    }

    char message[256];

    if (removing) {
        snprintf(
            message,
            sizeof(message),
            "Remove profile \"%s\"?\n\n"
            "SAVE_CONFIG will restart Klipper.",
            s.pending_profile_name);
    } else {
        snprintf(
            message,
            sizeof(message),
            "Save the current mesh as \"%s\"?\n\n"
            "An existing profile with this name will be replaced. "
            "SAVE_CONFIG will restart Klipper.",
            s.pending_profile_name);
    }

    s.profile_confirm =
        ui_popup_create(
            s.profile_popup
                ? s.profile_popup
                : s.owner,
            560,
            300,
            removing
                ? UI_POPUP_DANGER
                : UI_POPUP_STANDARD);

    if (!s.profile_confirm) {
        free_pending_profile_name();
        return;
    }

    ui_popup_add_title(
        s.profile_confirm,
        removing
            ? "REMOVE BED MESH?"
            : "SAVE BED MESH?",
        removing,
        4);
    ui_popup_add_header_divider(
        s.profile_confirm,
        44);
    ui_popup_add_body(
        s.profile_confirm,
        message,
        28,
        76,
        504);
    ui_popup_add_standard_footer_divider(
        s.profile_confirm);
    ui_popup_add_footer_action(
        s.profile_confirm,
        UI_POPUP_ACTION_CANCEL,
        LV_SYMBOL_LEFT " BACK",
        160,
        UI_POPUP_FOOTER_LEFT,
        close_profile_confirm_cb,
        NULL,
        NULL);
    ui_popup_add_footer_action(
        s.profile_confirm,
        removing
            ? UI_POPUP_ACTION_DANGER
            : UI_POPUP_ACTION_CONFIRM,
        removing
            ? LV_SYMBOL_TRASH " REMOVE & RESTART"
            : LV_SYMBOL_SAVE " SAVE & RESTART",
        240,
        UI_POPUP_FOOTER_RIGHT,
        removing
            ? confirm_remove_profile_cb
            : confirm_save_profile_cb,
        NULL,
        NULL);
}

static void save_name_continue_cb(lv_event_t *e)
{
    (void)e;

    if (!s.profile_name_input) {
        return;
    }

    const char *name =
        lv_textarea_get_text(s.profile_name_input);

    if (!set_pending_profile_name(name)) {
        return;
    }

    close_profile_editor_cb(NULL);
    show_profile_confirmation(false);
}

static void save_as_profile_cb(lv_event_t *e)
{
    (void)e;

    if (s.profile_editor) {
        lv_obj_move_foreground(s.profile_editor);
        return;
    }

    s.profile_editor =
        ui_popup_create(
            s.profile_popup
                ? s.profile_popup
                : s.owner,
            760,
            520,
            UI_POPUP_STANDARD);

    if (!s.profile_editor) {
        return;
    }

    ui_popup_add_title(
        s.profile_editor,
        "SAVE BED MESH PROFILE",
        false,
        4);
    ui_popup_add_header_divider(
        s.profile_editor,
        44);
    ui_popup_add_caption(
        s.profile_editor,
        "PROFILE NAME",
        54,
        68,
        220);

    s.profile_name_input =
        ui_popup_add_textarea(
            s.profile_editor,
            652,
            52,
            LV_ALIGN_TOP_MID,
            0,
            88,
            true,
            false,
            BED_MESH_PROFILE_NAME_MAX - 1,
            "example: textured_plate",
            "",
            "abcdefghijklmnopqrstuvwxyz"
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "0123456789_.-");

    ui_popup_add_keyboard(
        s.profile_editor,
        s.profile_name_input,
        652,
        270,
        LV_ALIGN_TOP_MID,
        0,
        154,
        LV_KEYBOARD_MODE_TEXT_LOWER);
    ui_popup_add_standard_footer_divider(
        s.profile_editor);
    ui_popup_add_footer_action(
        s.profile_editor,
        UI_POPUP_ACTION_CANCEL,
        LV_SYMBOL_CLOSE " CANCEL",
        170,
        UI_POPUP_FOOTER_LEFT,
        close_profile_editor_cb,
        NULL,
        NULL);
    ui_popup_add_footer_action(
        s.profile_editor,
        UI_POPUP_ACTION_CONFIRM,
        "CONTINUE",
        180,
        UI_POPUP_FOOTER_RIGHT,
        save_name_continue_cb,
        NULL,
        NULL);
}

static void remove_selected_profile_cb(lv_event_t *e)
{
    (void)e;

    const char *name = selected_profile_name();

    if (!name ||
        !set_pending_profile_name(name)) {
        return;
    }

    show_profile_confirmation(true);
}

void ui_bed_mesh_profiles_show_cb(lv_event_t *e)
{
    (void)e;

    if (s.profile_popup) {
        lv_obj_move_foreground(s.profile_popup);
        return;
    }

    s.profile_popup =
        ui_popup_create(
            s.owner ? s.owner : lv_screen_active(),
            720,
            470,
            UI_POPUP_STANDARD);

    if (!s.profile_popup) {
        return;
    }

    s.selected_profile = -1;

    ui_popup_add_title(
        s.profile_popup,
        "BED MESH PROFILES",
        false,
        4);
    ui_popup_add_header_divider(
        s.profile_popup,
        44);

    s.profile_list =
        ui_popup_add_list(
            s.profile_popup,
            24,
            64,
            672,
            318);

    if (s.profile_list) {
        if (s.mesh.profile_count == 0) {
            lv_obj_t *empty =
                ui_popup_add_status_label(
                    s.profile_popup,
                    "No saved bed-mesh profiles reported.",
                    48,
                    104,
                    624);

            if (empty) {
                lv_obj_set_style_text_color(
                    empty,
                    UI_TEXT_DIM,
                    0);
            }
        } else {
            for (size_t i = 0;
                 i < s.mesh.profile_count;
                 ++i) {
                const char *name =
                    s.mesh.profile_names[i];
                bool active =
                    strcmp(
                        name,
                        s.mesh.profile_name) == 0;

                if (active) {
                    s.selected_profile = (int16_t)i;
                }

                char row_text[96];
                snprintf(
                    row_text,
                    sizeof(row_text),
                    "%s%s",
                    name,
                    active ? "   ACTIVE" : "");

                lv_obj_t *row =
                    ui_popup_add_selectable_row(
                        s.profile_list,
                        row_text,
                        0,
                        (int32_t)i * 50,
                        648,
                        46,
                        profile_row_cb,
                        (void *)(uintptr_t)(i + 1));

                if (row) {
                    ui_popup_set_selectable_row_selected(
                        row,
                        active);
                }
            }
        }
    }

    ui_popup_add_standard_footer_divider(
        s.profile_popup);
    ui_popup_add_action_aligned(
        s.profile_popup,
        UI_POPUP_ACTION_SECONDARY,
        LV_SYMBOL_SAVE " SAVE AS",
        150,
        48,
        LV_ALIGN_BOTTOM_LEFT,
        24,
        -12,
        save_as_profile_cb,
        NULL,
        NULL);
    ui_popup_add_action_aligned(
        s.profile_popup,
        UI_POPUP_ACTION_PRIMARY,
        LV_SYMBOL_PLAY " LOAD",
        120,
        48,
        LV_ALIGN_BOTTOM_LEFT,
        184,
        -12,
        load_selected_profile_cb,
        NULL,
        NULL);
    ui_popup_add_action_aligned(
        s.profile_popup,
        UI_POPUP_ACTION_DANGER,
        LV_SYMBOL_TRASH " REMOVE",
        150,
        48,
        LV_ALIGN_BOTTOM_LEFT,
        314,
        -12,
        remove_selected_profile_cb,
        NULL,
        NULL);
    ui_popup_add_footer_action(
        s.profile_popup,
        UI_POPUP_ACTION_CLOSE,
        LV_SYMBOL_CLOSE " CLOSE",
        130,
        UI_POPUP_FOOTER_RIGHT,
        close_profile_popup_cb,
        NULL,
        NULL);
}



void ui_bed_mesh_profiles_init(
    lv_obj_t *owner,
    ui_bed_mesh_profiles_command_cb_t command_cb)
{
    ui_bed_mesh_profiles_close();
    s.owner = owner;
    s.command = command_cb;
    s.selected_profile = -1;
}


void ui_bed_mesh_profiles_update(
    const bed_mesh_snapshot_t *mesh)
{
    if (mesh) {
        s.mesh = *mesh;
    } else {
        memset(&s.mesh, 0, sizeof(s.mesh));
    }
}


void ui_bed_mesh_profiles_close(void)
{
    close_profile_popup_cb(NULL);
    memset(&s, 0, sizeof(s));
    s.selected_profile = -1;
}
