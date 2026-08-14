#include "ui_camera.h"
#include "lvgl.h"
#include "ui_theme.h"
#include "ui_page_geometry.h"
#include "ui_page_title.h"
#include "ui_shell.h"

static lv_obj_t *s_root;

void ui_camera_show(void)
{
    if (s_root) { lv_obj_clear_flag(s_root, LV_OBJ_FLAG_HIDDEN); lv_obj_move_foreground(s_root); return; }
    s_root = lv_obj_create(lv_screen_active());
    lv_obj_set_size(s_root, UI_PAGE_ROOT_WIDTH, UI_PAGE_ROOT_HEIGHT);
    lv_obj_set_pos(s_root, UI_PAGE_ROOT_X, UI_PAGE_ROOT_Y);
    lv_obj_clear_flag(s_root, LV_OBJ_FLAG_SCROLLABLE);
    ui_apply_root_style(s_root);
    ui_page_title_create(s_root, LV_SYMBOL_IMAGE " CAMERA", "Selected printer live view");
    lv_obj_t *card = lv_obj_create(s_root);
    lv_obj_set_size(card, 800, 392); lv_obj_set_pos(card, 20, 88);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *label = lv_label_create(card);
    lv_label_set_text(label, "CAMERA\\n\\nCamera streaming will be configured per printer profile.");
    ui_apply_text_body(label); lv_obj_center(label);
}

void ui_camera_hide(void)
{
    if (s_root) lv_obj_add_flag(s_root, LV_OBJ_FLAG_HIDDEN);
    ui_shell_raise_topbar(); ui_shell_raise_nav();
}
