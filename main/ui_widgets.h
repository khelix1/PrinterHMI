#pragma once

#include "lvgl.h"
#include "ui_theme.h"

/* Decorative separators share UI_BORDER at full opacity in every theme.
 * Callers own geometry; border separators preserve their control's surface. */
lv_obj_t *ui_create_divider(lv_obj_t *parent,int x,int y,int width,int height);
void ui_apply_divider_border(lv_obj_t *obj,lv_border_side_t side);

/* Opaque flat scrolling viewport; callers own padding, scrolling and bars. */
void ui_apply_scroll_surface(lv_obj_t *obj);
/* Flat entry surface with the shared bottom divider; geometry remains caller-owned. */
void ui_apply_flat_entry(lv_obj_t *obj);

lv_obj_t *ui_create_panel(lv_obj_t *parent);
lv_obj_t *ui_create_card(lv_obj_t *parent);
lv_obj_t *ui_create_label(lv_obj_t *parent, const char *text, lv_color_t color);
lv_obj_t *ui_create_card_subtitle(lv_obj_t *parent, const char *text);

/*
 * Shared Operator center-card language.
 *
 * These helpers are derived from the Drybox center cards and own:
 *
 *   - card surface
 *   - restrained thin border
 *   - square Operator radius
 *   - zero internal shell padding
 *   - muted section heading
 *   - thin internal divider
 *
 * Pages continue to own card dimensions, position and page-specific
 * content.
 */
lv_obj_t *ui_create_operator_card(
    lv_obj_t *parent,
    int x,
    int y,
    int width,
    int height);

/*
 * Shared compact Operator information card.
 *
 * The returned object is the value label. Its card shell is available
 * through lv_obj_get_parent(value_label).
 *
 * This component owns:
 *
 *   - Operator card surface, border and radius
 *   - centered muted title
 *   - centered compact value
 *   - internal padding and label geometry
 *
 * Pages continue to own position, dimensions, callbacks and live color.
 */
lv_obj_t *ui_create_operator_info_card(
    lv_obj_t *parent,
    const char *title,
    const char *value,
    int x,
    int y,
    int width,
    int height);

lv_obj_t *ui_create_operator_card_heading(
    lv_obj_t *parent,
    const char *text,
    int x,
    int y);

lv_obj_t *ui_create_operator_card_divider(
    lv_obj_t *parent,
    int x,
    int y,
    int width);

/*
 * Shared Operator status-banner shell.
 *
 * This component owns the outer surface and semantic state styling.
 * Each page owns its labels, indicators and telemetry.
 */
lv_obj_t *ui_create_operator_banner(
    lv_obj_t *parent,
    int x,
    int y,
    int width,
    int height,
    ui_status_kind_t kind);

/*
 * Shared Operator sidebar navigation button.
 *
 * The component owns:
 *
 *   - button frame
 *   - separate icon and text labels
 *   - icon/text geometry
 *   - selected and unselected surfaces
 *   - selected and unselected text colors
 *   - pressed and focused treatment
 *
 * The shell owns only position, page identity and click behavior.
 */
lv_obj_t *ui_create_operator_nav_button(
    lv_obj_t *parent,
    int x,
    int y,
    int width,
    int height,
    const char *icon,
    const char *text);

void ui_operator_nav_button_set_selected(
    lv_obj_t *button,
    bool selected);

/* Shared Operator page banner: callers own width and any extra controls.
 * Children 0/1 are the title/subtitle, with fixed theme baselines. */
lv_obj_t *ui_create_operator_page_header(lv_obj_t *parent,const char *title,
                                        const char *subtitle,int32_t width);
