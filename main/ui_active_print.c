#include "ui_active_print.h"
#include "ui_text.h"
#include "ui_preview_lightbox.h"
#include "ui_theme.h"
#include "ui_text_fit.h"
#include "ui_thumbnail.h"
#include "thumbnail_render.h"
#include "ui_widgets.h"
#include "esp_heap_caps.h"
#include <string.h>
#include <stdio.h>

typedef struct {
    lv_obj_t *root;
    lv_obj_t *canvas;
    unsigned calls;
    uint32_t epoch;
    lv_obj_t *preview_box;
    lv_obj_t *preview_label;
    lv_obj_t *footer;
    lv_obj_t *remaining;
} active_print_ctx_t;


static uint32_t s_active_print_generation;
uint32_t ui_active_print_thumb_generation(void){return s_active_print_generation;}
static lv_obj_t *s_active_print_panel = NULL;
static lv_obj_t *s_active_print_thumb_canvas = NULL;
static uint16_t *s_active_print_thumb_canvas_buf = NULL;
static char s_active_print_thumb_canvas_file[160] = "";

static active_print_ctx_t *active_begin(lv_obj_t *panel)
{
    if(!panel)panel=s_active_print_panel;
    if(!panel)return NULL;
    active_print_ctx_t *ctx=lv_obj_get_user_data(panel);
    if(!ctx || ctx->root!=panel || ctx->calls)return NULL;
    ++ctx->calls;return ctx;
}
static void active_end(active_print_ctx_t *ctx)
{
    if(--ctx->calls==0 && !ctx->root)lv_free(ctx);
}
static bool active_current(active_print_ctx_t *ctx,uint32_t epoch)
{
    return ctx->root && ctx->epoch==epoch;
}
static void active_field_deleted(lv_event_t *event)
{
    active_print_ctx_t *ctx=lv_event_get_user_data(event);lv_obj_t *target=lv_event_get_target_obj(event);
    lv_obj_t **fields[]={&ctx->preview_box,&ctx->preview_label,&ctx->footer,&ctx->remaining,&ctx->canvas};
    for(unsigned i=0;i<5;i++)if(*fields[i]==target){*fields[i]=NULL;++ctx->epoch;++s_active_print_generation;}
    if(target==s_active_print_thumb_canvas){s_active_print_thumb_canvas=NULL;s_active_print_thumb_canvas_file[0]=0;}
}
static void active_print_preview_clicked_cb(lv_event_t *event)
{
    active_print_ctx_t *ctx=lv_event_get_user_data(event);
    if(!ctx || !ctx->root || ctx->root!=s_active_print_panel || ctx->calls || !ctx->canvas ||
        lv_event_get_code(event)!=LV_EVENT_CLICKED || lv_event_get_target_obj(event)!=ctx->preview_box ||
        lv_event_get_current_target_obj(event)!=ctx->preview_box || lv_obj_has_state(ctx->preview_box,LV_STATE_DISABLED) ||
        lv_obj_has_flag(ctx->canvas,LV_OBJ_FLAG_HIDDEN))return;
    ++ctx->calls;char file[sizeof(s_active_print_thumb_canvas_file)];
    memcpy(file,s_active_print_thumb_canvas_file,sizeof(file));
    ui_preview_lightbox_show_file_object(ctx->canvas,file);active_end(ctx);
}
static void active_print_delete_cb(lv_event_t *event)
{
    active_print_ctx_t *ctx=lv_event_get_user_data(event);
    if(!ctx || lv_event_get_target_obj(event)!=ctx->root || lv_event_get_current_target_obj(event)!=ctx->root)return;
    lv_obj_t *panel=ctx->root;
    lv_obj_t *fields[]={ctx->preview_box,ctx->preview_label,ctx->footer,ctx->remaining,ctx->canvas};
    for(unsigned i=0;i<5;i++)if(fields[i])lv_obj_remove_event_cb_with_user_data(fields[i],active_field_deleted,ctx);
    if(ctx->preview_box)lv_obj_remove_event_cb_with_user_data(ctx->preview_box,active_print_preview_clicked_cb,ctx);
    if(panel==s_active_print_panel){s_active_print_panel=NULL;s_active_print_thumb_canvas=NULL;s_active_print_thumb_canvas_file[0]=0;}
    ctx->root=ctx->preview_box=ctx->preview_label=ctx->footer=ctx->remaining=ctx->canvas=NULL;++ctx->epoch;++s_active_print_generation;
    lv_obj_set_user_data(panel,NULL);
    if(!ctx->calls)lv_free(ctx);
}

lv_obj_t *ui_active_print_thumb_box(lv_obj_t *card)
{
    if (!card) return NULL;

    active_print_ctx_t *ctx = (active_print_ctx_t *)lv_obj_get_user_data(card);
    if (!ctx) return NULL;

    return ctx->preview_box;
}


lv_obj_t **ui_active_print_thumb_canvas_ref(void)
{
    return &s_active_print_thumb_canvas;
}

uint16_t **ui_active_print_thumb_canvas_buf_ref(void)
{
    return &s_active_print_thumb_canvas_buf;
}

char *ui_active_print_thumb_canvas_file(void)
{
    return s_active_print_thumb_canvas_file;
}

size_t ui_active_print_thumb_canvas_file_size(void)
{
    return sizeof(s_active_print_thumb_canvas_file);
}

void ui_active_print_thumb_set_placeholder(lv_obj_t *card,const char *text)
{
    active_print_ctx_t *ctx=active_begin(card);if(!ctx)return;
    uint32_t epoch=ctx->epoch;
    if(ctx->preview_label){
        lv_label_set_text(ctx->preview_label,text?text:"Model preview\n\nNo preview loaded");
        if(!active_current(ctx,epoch))goto finished;
        lv_obj_clear_flag(ctx->preview_label,LV_OBJ_FLAG_HIDDEN);
        if(!active_current(ctx,epoch))goto finished;
        lv_obj_center(ctx->preview_label);
        if(active_current(ctx,epoch))lv_obj_move_foreground(ctx->preview_label);
    }
finished:active_end(ctx);
}
void ui_active_print_thumb_clear_placeholder(lv_obj_t *card)
{
    active_print_ctx_t *ctx=active_begin(card);if(!ctx)return;
    if(ctx->preview_label)lv_obj_add_flag(ctx->preview_label,LV_OBJ_FLAG_HIDDEN);
    active_end(ctx);
}

bool ui_active_print_thumb_canvas_matches(const char *file)
{
    return s_active_print_thumb_canvas &&
           file && file[0] &&
           strcmp(s_active_print_thumb_canvas_file, file) == 0;
}

bool ui_active_print_thumb_ensure_canvas_buffer(size_t pixels)
{
    if(!pixels || pixels>THUMBNAIL_PREVIEW_WIDTH*THUMBNAIL_PREVIEW_HEIGHT)return false;
    if (s_active_print_thumb_canvas_buf) return true;
    pixels=THUMBNAIL_PREVIEW_WIDTH*THUMBNAIL_PREVIEW_HEIGHT;

    s_active_print_thumb_canvas_buf = heap_caps_malloc(pixels * sizeof(uint16_t),
                                                       MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s_active_print_thumb_canvas_buf) {
        s_active_print_thumb_canvas_buf = heap_caps_malloc(pixels * sizeof(uint16_t),
                                                           MALLOC_CAP_8BIT);
    }

    return s_active_print_thumb_canvas_buf != NULL;
}

void ui_active_print_thumb_delete_canvas(void)
{
    active_print_ctx_t *ctx=active_begin(NULL);if(!ctx)return;
    lv_obj_t *canvas=ctx->canvas;ctx->canvas=NULL;
    s_active_print_thumb_canvas=NULL;s_active_print_thumb_canvas_file[0]=0;
    if(canvas){++ctx->epoch;++s_active_print_generation;lv_obj_remove_event_cb_with_user_data(canvas,active_field_deleted,ctx);lv_obj_delete(canvas);}
    active_end(ctx);
}
void ui_active_print_thumb_forget_canvas_file(void){s_active_print_thumb_canvas_file[0]=0;}
static void active_apply(lv_obj_t *card,int w,int h,const char *file,bool foreground)
{
    if(card!=s_active_print_panel || !s_active_print_thumb_canvas_buf || w<=0 || h<=0 ||
        w>THUMBNAIL_PREVIEW_WIDTH || h>THUMBNAIL_PREVIEW_HEIGHT)return;
    active_print_ctx_t *ctx=active_begin(card);if(!ctx)return;
    uint32_t epoch=ctx->epoch;char copied[sizeof(s_active_print_thumb_canvas_file)]="";
    if(file && strlen(file)<sizeof(copied))memcpy(copied,file,strlen(file)+1);
    if(!ctx->preview_box)goto finished;
    bool created=!ctx->canvas;
    if(created){ctx->canvas=lv_canvas_create(ctx->preview_box);s_active_print_thumb_canvas=ctx->canvas;lv_obj_add_event_cb(ctx->canvas,active_field_deleted,LV_EVENT_DELETE,ctx);}
    lv_canvas_set_buffer(ctx->canvas,s_active_print_thumb_canvas_buf,w,h,LV_COLOR_FORMAT_RGB565);
    if(!active_current(ctx,epoch))goto finished;
    ui_thumbnail_fit_object(ctx->canvas,ctx->preview_box,w,h,0);
    if(!active_current(ctx,epoch))goto finished;
    if(created || foreground)lv_obj_move_foreground(ctx->canvas);
    if(!active_current(ctx,epoch))goto finished;
    if(ctx->preview_label)lv_obj_add_flag(ctx->preview_label,LV_OBJ_FLAG_HIDDEN);
    if(active_current(ctx,epoch) && ctx->root==s_active_print_panel)memcpy(s_active_print_thumb_canvas_file,copied,sizeof(copied));
finished:active_end(ctx);
}
void ui_active_print_thumb_show_canvas_from_buffer(lv_obj_t *card,int w,int h,const char *file){active_apply(card,w,h,file,true);}
void ui_active_print_thumb_apply_canvas_from_buffer(lv_obj_t *card,int w,int h,const char *file){active_apply(card,w,h,file,false);}


lv_obj_t *ui_active_print_create(
    lv_obj_t *parent,
    int x,
    int y,
    int w,
    int h)
{
    const ui_dashboard_rect_t rect = {x, y, w, h};
    const ui_dashboard_active_print_layout_t layout = {
        .heading_x = 18,
        .heading_y = 12,
        .filename_x = 140,
        .filename_y = 14,
        .preview_x = 20,
        .preview_y = 42,
        .preview_width = 0,
        .preview_height = 0,
        .footer_x = 18,
        .footer_bottom = 31,
    };

    return ui_active_print_create_profile(
        parent,
        &rect,
        &layout);
}


lv_obj_t *ui_active_print_create_profile(
    lv_obj_t *parent,
    const ui_dashboard_rect_t *rect,
    const ui_dashboard_active_print_layout_t *layout)
{
    if (!parent || !rect || !layout) {
        return NULL;
    }

    const int x = rect->x;
    const int y = rect->y;
    const int w = rect->width;
    const int h = rect->height;
    int preview_width =
        layout->preview_width > 0
            ? layout->preview_width
            : w - (layout->preview_x * 2);
    int preview_height =
        layout->preview_height > 0
            ? layout->preview_height
            : h - layout->footer_bottom -
                layout->preview_y - 12;

    if (ui_theme_get_active() == UI_THEME_OPERATOR && layout->preview_height <= 0) {
        /* Reserve two readable timing lines instead of clipping a long strip. */
        preview_height = h - layout->preview_y -
            (ui_font_with_fallback(UI_FONT_CAPTION)->line_height * 2 + 12);
    }

    if (preview_width < 120) preview_width = 120;
    if (preview_height < 80) preview_height = 80;

    /*
     * The Active Print center card now uses the exact same shared
     * Operator shell as the Drybox center cards.
     *
     * Thumbnail geometry and runtime behavior remain unchanged.
     */
    lv_obj_t *panel =
        ui_create_operator_card(
            parent,
            x,
            y,
            w,
            h);

    if (!panel) {
        return NULL;
    }

    active_print_ctx_t *ctx =
        lv_malloc(sizeof(active_print_ctx_t));

    if (!ctx) {
        return panel;
    }

    memset(ctx, 0, sizeof(*ctx));

    /*
     * Shared Drybox-style card heading.
     */
    if (!ui_theme_is_studio()) ui_create_operator_card_heading(
        panel,
        "Active print",
        layout->heading_x,
        layout->heading_y);

    /*
     * The expanded Dashboard card gives the preview another 30 px
     * vertically while preserving its established width and canvas path.
     */
    ctx->preview_box = lv_obj_create(panel);

    lv_obj_clear_flag(
        ctx->preview_box,
        LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_size(
        ctx->preview_box,
        preview_width,
        preview_height);

    lv_obj_set_pos(
        ctx->preview_box,
        layout->preview_x,
        layout->preview_y);

    ui_apply_preview_style(
        ctx->preview_box);

    lv_obj_add_flag(
        ctx->preview_box,
        LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(
        ctx->preview_box,
        active_print_preview_clicked_cb,
        LV_EVENT_CLICKED,
        ctx);

    ctx->preview_label =
        lv_label_create(ctx->preview_box);

    lv_label_set_text(
        ctx->preview_label,
        "Model preview\n\nNo preview loaded");

    ui_apply_text_body_large(
        ctx->preview_label);
    ui_apply_label_dim(
        ctx->preview_label);

    lv_obj_set_style_text_align(
        ctx->preview_label,
        LV_TEXT_ALIGN_CENTER,
        0);

    lv_obj_center(ctx->preview_label);

    /*
     * Status strip overlays the lower edge of the panel without
     * changing any existing setter or formatting behavior.
     */
    ctx->footer = lv_label_create(panel);

    lv_label_set_text(
        ctx->footer,
        ui_text(ui_theme_get_active() == UI_THEME_OPERATOR
            ? "Layer --/--\n--:-- elapsed  |  REM --:--"
            : "LAYER --/--    ELAPSED --:--    REM --:--"));

    lv_obj_set_width(
        ctx->footer,
        w - (layout->footer_x * 2));

    lv_label_set_long_mode(
        ctx->footer,
        LV_LABEL_LONG_CLIP);

    ui_apply_text_caption(
        ctx->footer);

    lv_obj_set_style_text_color(
        ctx->footer,
        UI_ACCENT_CYAN,
        0);

    lv_obj_set_style_text_align(
        ctx->footer,
        LV_TEXT_ALIGN_CENTER,
        0);

    /*
     * Keep layer and timing metadata visually integrated with the
     * Active Print card instead of placing it on a dark overlay.
     */
    lv_obj_set_style_bg_opa(
        ctx->footer,
        LV_OPA_TRANSP,
        0);

    lv_obj_set_style_pad_top(
        ctx->footer,
        ui_theme_density_metric(2, 4, 6),
        0);

    lv_obj_set_style_pad_bottom(
        ctx->footer,
        ui_theme_density_metric(2, 4, 6),
        0);

    lv_obj_set_pos(
        ctx->footer,
        layout->footer_x,
        h - layout->footer_bottom);

    lv_obj_move_foreground(ctx->footer);

    if (ui_theme_is_studio()) {
        lv_obj_set_style_pad_all(panel,0,0);
        lv_obj_set_style_bg_opa(panel,LV_OPA_TRANSP,0);
        lv_obj_set_style_border_width(panel,0,0);
        lv_obj_set_width(ctx->footer,280);
        lv_obj_set_pos(ctx->footer,340,166);
        lv_obj_set_style_pad_all(ctx->footer,0,0);
        lv_obj_set_style_text_align(ctx->footer,LV_TEXT_ALIGN_LEFT,0);
        ui_apply_custom_label_style(ctx->footer,UI_FONT_BODY,UI_TEXT_DIM);
        ctx->remaining=lv_label_create(panel);
        lv_label_set_text(ctx->remaining,"Remaining --:--");
        ui_apply_custom_label_style(ctx->remaining,UI_FONT_BODY,UI_TEXT);
        lv_label_set_long_mode(ctx->remaining,LV_LABEL_LONG_MODE_DOTS);
        lv_obj_set_size(ctx->remaining,280,LV_SIZE_CONTENT);
        lv_obj_set_pos(ctx->remaining,340,192);
    } else {
        /* Anchor timing to the card content area at every text density. */
        lv_obj_set_style_pad_all(ctx->footer,0,0);
        lv_obj_align(ctx->footer,LV_ALIGN_BOTTOM_MID,0,-4);
    }

    lv_obj_set_user_data(panel, ctx);

    lv_obj_add_event_cb(
        panel,
        active_print_delete_cb,
        LV_EVENT_DELETE,
        ctx);

    ++s_active_print_generation;
    ctx->root=panel;
    lv_obj_t *fields[]={ctx->preview_box,ctx->preview_label,ctx->footer,ctx->remaining};
    for(unsigned i=0;i<4;i++)if(fields[i])lv_obj_add_event_cb(fields[i],active_field_deleted,LV_EVENT_DELETE,ctx);
    s_active_print_thumb_canvas=NULL;s_active_print_thumb_canvas_file[0]=0;
    s_active_print_panel = panel;

    return panel;
}


void ui_active_print_set(lv_obj_t *panel,const char *layer,const char *elapsed,const char *remaining)
{
    active_print_ctx_t *ctx=active_begin(panel);if(!ctx)return;
    uint32_t epoch=ctx->epoch;
    if(ctx->remaining){
        if(ctx->footer)lv_label_set_text_fmt(ctx->footer,"Layer %s",layer?layer:"--/--");
        if(!active_current(ctx,epoch))goto finished;
        lv_label_set_text_fmt(ctx->remaining,"%s elapsed  •  %s",elapsed?elapsed:"--:--",remaining?remaining:"REM --:--");
        if(active_current(ctx,epoch))ui_text_fit_single_line(ctx->remaining,UI_FONT_BODY);
    }else if(ctx->footer){
        char buf[128];
        if (ui_theme_get_active() == UI_THEME_OPERATOR) {
            snprintf(buf,sizeof(buf),"Layer %s\n%s elapsed  |  %s",layer?layer:"--/--",elapsed?elapsed:"--:--",remaining?remaining:"REM --:--");
        } else {
            snprintf(buf,sizeof(buf),"LAYER %s    ELAPSED %s    %s",layer?layer:"--/--",elapsed?elapsed:"--:--",remaining?remaining:"REM --:--");
        }
        lv_label_set_text(ctx->footer,buf);
    }
finished:active_end(ctx);
}
