#pragma once

#include "lvgl.h"

#include "ui_button.h"
typedef enum {
    UI_POPUP_STANDARD = 0,
    UI_POPUP_DANGER
} ui_popup_kind_t;

/*
 * Semantic actions used inside shared popups.
 *
 * Popup owners describe what an action means. The popup/theme layer maps
 * that meaning onto the shared button system.
 */
typedef enum {
    UI_POPUP_ACTION_PRIMARY = 0,
    UI_POPUP_ACTION_SECONDARY,
    UI_POPUP_ACTION_CONFIRM,
    UI_POPUP_ACTION_CANCEL,
    UI_POPUP_ACTION_CLOSE,
    UI_POPUP_ACTION_DANGER,
    UI_POPUP_ACTION_CHOICE
} ui_popup_action_t;

typedef enum {
    UI_POPUP_FOOTER_LEFT = 0,
    UI_POPUP_FOOTER_CENTER,
    UI_POPUP_FOOTER_RIGHT
} ui_popup_footer_slot_t;

lv_obj_t *ui_popup_create(lv_obj_t *parent,
                          int32_t width,
                          int32_t height,
                          ui_popup_kind_t kind);

lv_obj_t *ui_popup_add_title(lv_obj_t *popup,
                             const char *text,
                             bool danger,
                             int32_t y_offset);

lv_obj_t *ui_popup_add_message(lv_obj_t *popup,
                               const char *text,
                               bool muted,
                               int32_t y_offset);

/*
 * Shared Operator-popup structural primitives.
 *
 * Geometry remains selected by the caller, while visual treatment is
 * centralized here and in the active theme.
 */
lv_obj_t *ui_popup_add_divider(lv_obj_t *popup,
                               int32_t x,
                               int32_t y,
                               int32_t width);

lv_obj_t *ui_popup_add_header_divider(lv_obj_t *popup,
                                      int32_t y);

lv_obj_t *ui_popup_add_footer_divider(lv_obj_t *popup,
                                      int32_t y);

/*
 * Standard Operator-popup footer geometry.
 *
 * Footer actions use a 48 px touch target, 24 px horizontal inset and
 * 12 px bottom inset. The divider is placed consistently above them.
 */
lv_obj_t *ui_popup_add_standard_footer_divider(lv_obj_t *popup);

lv_obj_t *ui_popup_add_footer_action(lv_obj_t *popup,
                                     ui_popup_action_t action,
                                     const char *text,
                                     int32_t width,
                                     ui_popup_footer_slot_t slot,
                                     lv_event_cb_t event_cb,
                                     void *user_data,
                                     lv_obj_t **label_out);

lv_obj_t *ui_popup_add_body(lv_obj_t *popup,
                            const char *text,
                            int32_t x,
                            int32_t y,
                            int32_t width);

lv_obj_t *ui_popup_add_status_label(lv_obj_t *popup,
                                    const char *text,
                                    int32_t x,
                                    int32_t y,
                                    int32_t width);

lv_obj_t *ui_popup_add_caption(lv_obj_t *popup,
                               const char *text,
                               int32_t x,
                               int32_t y,
                               int32_t width);

lv_obj_t *ui_popup_add_progress_status(lv_obj_t *popup,
                                       const char *text,
                                       int32_t x,
                                       int32_t y,
                                       int32_t width);

lv_obj_t *ui_popup_add_progress_value(lv_obj_t *popup,
                                      const char *text,
                                      int32_t x,
                                      int32_t y,
                                      int32_t width);

lv_obj_t *ui_popup_add_progress_bar(lv_obj_t *popup,
                                    int32_t x,
                                    int32_t y,
                                    int32_t width,
                                    int32_t height,
                                    int32_t minimum,
                                    int32_t maximum,
                                    int32_t value);

lv_obj_t *ui_popup_add_progress_detail(lv_obj_t *popup,
                                       const char *text,
                                       int32_t x,
                                       int32_t y,
                                       int32_t width);

lv_obj_t *ui_popup_add_close_button(lv_obj_t *popup,
                                    int32_t width,
                                    int32_t height,
                                    lv_align_t align,
                                    int32_t x_offset,
                                    int32_t y_offset,
                                    ui_button_kind_t kind,
                                    lv_event_cb_t event_cb,
                                    void *user_data);

lv_obj_t *ui_popup_add_list(lv_obj_t *popup,
                            int32_t x,
                            int32_t y,
                            int32_t width,
                            int32_t height);

lv_obj_t *ui_popup_add_selectable_row(lv_obj_t *parent,
                                      const char *text,
                                      int32_t x,
                                      int32_t y,
                                      int32_t width,
                                      int32_t height,
                                      lv_event_cb_t event_cb,
                                      void *user_data);

void ui_popup_set_selectable_row_selected(lv_obj_t *row,
                                          bool selected);
void ui_popup_set_selectable_row_text_font(lv_obj_t *row,
                                           const lv_font_t *font);

lv_obj_t *ui_popup_add_textarea(lv_obj_t *popup,
                                int32_t width,
                                int32_t height,
                                lv_align_t align,
                                int32_t x_offset,
                                int32_t y_offset,
                                bool one_line,
                                bool password_mode,
                                uint32_t max_length,
                                const char *placeholder,
                                const char *initial_text,
                                const char *accepted_chars);

lv_obj_t *ui_popup_add_keyboard(lv_obj_t *popup,
                                lv_obj_t *textarea,
                                int32_t width,
                                int32_t height,
                                lv_align_t align,
                                int32_t x_offset,
                                int32_t y_offset,
                                lv_keyboard_mode_t mode);

lv_obj_t *ui_popup_add_action_at(lv_obj_t *popup,
                                 ui_popup_action_t action,
                                 const char *text,
                                 int32_t x,
                                 int32_t y,
                                 int32_t width,
                                 int32_t height,
                                 lv_event_cb_t event_cb,
                                 void *user_data,
                                 lv_obj_t **label_out);

lv_obj_t *ui_popup_add_action_aligned(lv_obj_t *popup,
                                      ui_popup_action_t action,
                                      const char *text,
                                      int32_t width,
                                      int32_t height,
                                      lv_align_t align,
                                      int32_t x_offset,
                                      int32_t y_offset,
                                      lv_event_cb_t event_cb,
                                      void *user_data,
                                      lv_obj_t **label_out);

/*
 * Low-level geometry helpers retained inside the popup subsystem.
 *
 * New popup owners should use ui_popup_add_action_at() or
 * ui_popup_add_action_aligned() so action intent remains explicit.
 */
lv_obj_t *ui_popup_add_button_at(lv_obj_t *popup,
                                 const char *text,
                                 int32_t x,
                                 int32_t y,
                                 int32_t width,
                                 int32_t height,
                                 ui_button_kind_t kind,
                                 lv_event_cb_t event_cb,
                                 void *user_data,
                                 lv_obj_t **label_out);

lv_obj_t *ui_popup_add_button_aligned(lv_obj_t *popup,
                                      const char *text,
                                      int32_t width,
                                      int32_t height,
                                      lv_align_t align,
                                      int32_t x_offset,
                                      int32_t y_offset,
                                      ui_button_kind_t kind,
                                      lv_event_cb_t event_cb,
                                      void *user_data,
                                      lv_obj_t **label_out);
