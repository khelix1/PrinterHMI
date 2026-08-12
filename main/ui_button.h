#pragma once

#include "lvgl.h"

/*
 * Shared semantic button types.
 *
 * Page and component modules choose what an action means.
 * The active theme decides what that action looks like.
 */
typedef enum {
    UI_BUTTON_PRIMARY = 0,
    UI_BUTTON_SECONDARY,
    UI_BUTTON_SUCCESS,
    UI_BUTTON_WARNING,
    UI_BUTTON_DANGER,
    UI_BUTTON_CANCEL,
    UI_BUTTON_CLOSE,
    UI_BUTTON_OUTLINED

} ui_button_kind_t;

/* Keep visible geometry exact while accepting a near-edge operator touch.
 * Four pixels preserves separation in tightly packed standard controls. */
#define UI_BUTTON_TOUCH_SLOP 4

static inline void ui_button_expand_touch_target(lv_obj_t *button)
{
    if (button) lv_obj_set_ext_click_area(button, UI_BUTTON_TOUCH_SLOP);
}

/*
 * Shared icon/text arrangement used by Operator controls.
 *
 * The button surface always comes from ui_button_create() and therefore
 * retains exactly the same theme-owned outline, radius and press state.
 */
typedef enum {
    UI_BUTTON_ICON_HORIZONTAL = 0,
    UI_BUTTON_ICON_VERTICAL,
    UI_BUTTON_ICON_HORIZONTAL_REVERSE,
    UI_BUTTON_ICON_VERTICAL_REVERSE
} ui_button_icon_layout_t;


/*
 * Creates a fully themed shared button without adding child content.
 *
 * Use this for dynamic list rows and other controls whose caller owns
 * custom labels, icons or layout.
 */
lv_obj_t *ui_button_create_empty(
    lv_obj_t *parent,
    ui_button_kind_t kind);

/*
 * Creates a fully themed button and centered label.
 *
 * Geometry, position, event callbacks and user data remain owned by
 * the calling page or component.
 */
lv_obj_t *ui_button_create(
    lv_obj_t *parent,
    ui_button_kind_t kind,
    const char *text);

/*
 * Applies a semantic button appearance to an existing LVGL button.
 */
void ui_button_apply_kind(
    lv_obj_t *button,
    ui_button_kind_t kind);

/*
 * Creates and centers a themed button label.
 */
lv_obj_t *ui_button_create_label(
    lv_obj_t *button,
    const char *text);


/*
 * Creates a fully themed shared button with independently colored
 * icon and white text labels.
 *
 * No border styling is applied here. The selected ui_button_kind_t
 * remains the sole owner of the button frame.
 */
lv_obj_t *ui_button_create_icon(
    lv_obj_t *parent,
    ui_button_kind_t kind,
    const char *symbol,
    const char *text,
    lv_color_t icon_color,
    ui_button_icon_layout_t layout);
