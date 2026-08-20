#include "ui_page_title.h"
#include "ui_text.h"
#include "ui_theme.h"

void ui_page_title_create(lv_obj_t *parent,
                          const char *title_text,
                          const char *subtitle_text)
{
    if (!parent) return;

    lv_obj_t *title = lv_label_create(parent);
    lv_label_set_text(title, title_text ? title_text : ui_text(""));
    ui_apply_text_title(title);
    ui_apply_label_bright(title);
    lv_obj_set_pos(title, 30, 2);

    lv_obj_t *subtitle = lv_label_create(parent);
    lv_label_set_text(subtitle,
                      subtitle_text ? subtitle_text : ui_text(""));
    ui_apply_text_body(subtitle);
    ui_apply_label_dim(subtitle);
    lv_obj_set_pos(subtitle, 30, 28);
}
