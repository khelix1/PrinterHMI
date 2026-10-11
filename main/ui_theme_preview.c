#include "ui_button_press.h"
#include "ui_theme_preview.h"
#include "ui_dashboard_layout_profile.h"
#include "ui_studio_icons.h"
#include "ui_text.h"

#include <stddef.h>
#include "esp_heap_caps.h"
#include "misc/cache/instance/lv_image_cache.h"
#include "ui_font_fallback.h"

typedef struct {
    ui_theme_id_t theme;
    bool studio;
    uint32_t background;
    uint32_t surface;
    uint32_t card;
    uint32_t control;
    uint32_t border;
    uint32_t accent;
    uint32_t text;
    uint32_t muted;
    uint32_t success;
    uint32_t danger;
    int32_t radius;
    lv_opa_t surface_opa;
    const char *name;
    const char *code;
    const char *description;
} ui_theme_preview_palette_t;

static ui_theme_preview_palette_t preview_palette(ui_theme_id_t theme)
{
    switch (theme) {
        case UI_THEME_STUDIO_DARK:
            return (ui_theme_preview_palette_t){
                .studio=true, .background=0x15161B, .surface=0x22232B, .card=0x22232B,
                .control=0x322746, .border=0x363640, .accent=0x8B6CFF,
                .text=0xF2F0EC, .muted=0xA8A7B7, .success=0xD8F36A,
                .danger=0xFF4D65, .radius=18, .surface_opa=LV_OPA_COVER,
                .name="STUDIO DARK", .code="STUDIO", .description="Bottom dock / studio console",
            };
        case UI_THEME_CLASSIC:
            return (ui_theme_preview_palette_t){
                .background = 0x18130F,
                .surface = 0x2B211B,
                .card = 0x32251E,
                .control = 0x49362B,
                .border = 0xB87545,
                .accent = 0xE07A3F,
                .text = 0xF4E8DA,
                .muted = 0xC5A88E,
                .success = 0x6DD39E,
                .danger = 0xF06A66,
                .radius = 18,
                .surface_opa = LV_OPA_COVER,
                .name = "FOUNDRY",
                .code = "THEME A",
                .description = "Warm workshop",
            };

        case UI_THEME_GLASS:
            return (ui_theme_preview_palette_t){
                .background = 0x03040A,
                .surface = 0x0A1020,
                .card = 0x0C1428,
                .control = 0x17142E,
                .border = 0x3A67A0,
                .accent = 0x4C9FBA,
                .text = 0xEAF3FF,
                .muted = 0x91A6D8,
                .success = 0x35FFC6,
                .danger = 0xFF4FA3,
                .radius = 22,
                .surface_opa = (lv_opa_t)230,
                .name = "DARK GLASS",
                .code = "THEME C",
                .description = "Smoked layers",
            };

        case UI_THEME_OPERATOR_SHELL:
            return (ui_theme_preview_palette_t){
                .background = 0x09121E,
                .surface = 0x101B2A,
                .card = 0x101B2A,
                .control = 0x1A2633,
                .border = 0x3D6F99,
                .accent = 0x33C7FF,
                .text = 0xE8F1FF,
                .muted = 0x8FA7C2,
                .success = 0x70E000,
                .danger = 0xFF4D4D,
                .radius = 10,
                .surface_opa = LV_OPA_COVER,
                .name = "OPERATOR SHELL",
                .code = "LAYOUT THEME",
                .description = "Eight-item rail",
            };

        case UI_THEME_OPERATOR:
        default:
            return (ui_theme_preview_palette_t){
                .background = 0x0B1118,
                .surface = 0x101B2A,
                .card = 0x101B2A,
                .control = 0x1A2633,
                .border = 0x3D6F99,
                .accent = 0x19C7E8,
                .text = 0xE8F1FF,
                .muted = 0x8FA7C2,
                .success = 0x70E000,
                .danger = 0xFF4D4D,
                .radius = 10,
                .surface_opa = LV_OPA_COVER,
                .name = "OPERATOR",
                .code = "THEME B",
                .description = "Print-cell flat",
            };
    }
}

static lv_obj_t *preview_rect(lv_obj_t *parent,
                              int32_t x,
                              int32_t y,
                              int32_t width,
                              int32_t height,
                              uint32_t background,
                              uint32_t border,
                              int32_t radius,
                              lv_opa_t opacity)
{
    lv_obj_t *object = lv_obj_create(parent);

    if (!object) return NULL;

    /* Preview geometry is explicit; discard inherited effects/transitions. */
    lv_obj_remove_style_all(object);
    lv_obj_set_size(object, width, height);
    lv_obj_set_pos(object, x, y);
    lv_obj_clear_flag(object, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(object, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_pad_all(object, 0, 0);
    lv_obj_set_style_bg_color(object, lv_color_hex(background), 0);
    lv_obj_set_style_bg_opa(object, opacity, 0);
    lv_obj_set_style_border_color(object, lv_color_hex(border), 0);
    lv_obj_set_style_border_width(object, border ? 1 : 0, 0);
    lv_obj_set_style_radius(object, radius, 0);

    return object;
}

static lv_obj_t *preview_label(lv_obj_t *parent,
                               const char *text,
                               const lv_font_t *font,
                               uint32_t color,
                               int32_t x,
                               int32_t y)
{
    lv_obj_t *label = lv_label_create(parent);

    if (!label) return NULL;

    lv_label_set_text(label, text ? text : ui_text(""));
    lv_obj_set_pos(label, x, y);
    lv_obj_set_style_text_font(label, ui_font_with_fallback(font), 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_obj_clear_flag(label, LV_OBJ_FLAG_CLICKABLE);

    return label;
}

/* Each miniature is rendered once; scrolling only copies RGB565 pixels. */
static void preview_pixels_deleted(lv_event_t *event)
{
    lv_obj_t *canvas = lv_event_get_target(event);
    lv_image_cache_drop(lv_image_get_src(canvas));
    heap_caps_free(lv_event_get_user_data(event));
}

/* One scene definition feeds both the cached pixels and allocation fallback.
 * Miniature fonts belong to the preview, never the currently active theme. */
typedef struct {
    lv_layer_t *layer;
    lv_obj_t *parent;
    bool glass;
} preview_scene_t;

static void sample_rect(preview_scene_t *scene, int x, int y, int w, int h,
                        uint32_t bg, uint32_t border, int radius, lv_opa_t opa)
{
    if (!scene->layer) {
        lv_obj_t *object=preview_rect(scene->parent,x,y,w,h,bg,border,radius,opa);
        if (object && scene->glass && opa<LV_OPA_COVER) {
            lv_obj_set_style_bg_color(object,lv_color_hex(0x1D3152),0);
            lv_obj_set_style_bg_grad_color(object,lv_color_hex(bg),0);
            lv_obj_set_style_bg_grad_dir(object,LV_GRAD_DIR_VER,0);
        }
        return;
    }
    lv_draw_rect_dsc_t d;
    lv_draw_rect_dsc_init(&d);
    d.bg_color=lv_color_hex(bg); d.bg_opa=opa; d.radius=radius;
    d.border_color=lv_color_hex(border); d.border_width=border?1:0;
    if (scene->glass && opa<LV_OPA_COVER) {
        lv_color_t colors[]={lv_color_hex(0x1D3152),lv_color_hex(bg)};
        lv_grad_init_stops(&d.bg_grad,colors,NULL,NULL,2);
        lv_grad_vertical_init(&d.bg_grad);
    }
    lv_area_t a={x,y,x+w-1,y+h-1};
    lv_draw_rect(scene->layer,&d,&a);
}

static void sample_text(preview_scene_t *scene, const char *text, const lv_font_t *font,
                        uint32_t color, int x, int y, int w, bool center)
{
    font=ui_font_with_fallback(font);
    if (!scene->layer) {
        lv_obj_t *label=preview_label(scene->parent,text,font,color,x,y);
        lv_obj_remove_style_all(label);
        lv_obj_set_pos(label,x,y);
        lv_obj_set_style_text_font(label,font,0);
        lv_obj_set_style_text_color(label,lv_color_hex(color),0);
        lv_obj_set_style_text_align(label,center?LV_TEXT_ALIGN_CENTER:LV_TEXT_ALIGN_LEFT,0);
        lv_obj_set_size(label,w,font->line_height);
        lv_label_set_long_mode(label,LV_LABEL_LONG_CLIP);
        return;
    }
    lv_draw_label_dsc_t d;
    lv_draw_label_dsc_init(&d);
    d.text=text; d.font=font; d.color=lv_color_hex(color);
    d.align=center?LV_TEXT_ALIGN_CENTER:LV_TEXT_ALIGN_LEFT;
    lv_area_t a={x,y,x+w-1,y+font->line_height-1};
    lv_draw_label(scene->layer,&d,&a);
}

/* Reuse the production Studio dock paths in both renderers. Cached previews
 * draw their strokes once, so theme chooser scrolling still copies pixels. */
static void sample_studio_icon(preview_scene_t *scene,unsigned index,int x,int y,uint32_t color)
{
    lv_obj_t *icon=ui_studio_icon_create(scene->parent?scene->parent:lv_screen_active(),index);
    lv_obj_set_pos(icon,x,y);
    ui_studio_icon_color(icon,lv_color_hex(color));
    if (!scene->layer) return;
    for (unsigned i=0;i<lv_obj_get_child_count(icon);i++) {
        lv_obj_t *line=lv_obj_get_child(icon,i);
        const lv_point_precise_t *points=lv_line_get_points(line);
        unsigned count=lv_line_get_point_count(line);
        for (unsigned j=1;j<count;j++) {
            lv_draw_line_dsc_t d;
            lv_draw_line_dsc_init(&d);
            d.color=lv_color_hex(color);d.width=2;d.round_start=j==1;d.round_end=1;
            d.p1=(lv_point_precise_t){x+points[j-1].x,y+points[j-1].y};
            d.p2=(lv_point_precise_t){x+points[j].x,y+points[j].y};
            lv_draw_line(scene->layer,&d);
        }
    }
    lv_obj_delete(icon);
}

static void preview_draw_scene(preview_scene_t *scene,
                               const ui_theme_preview_palette_t *p, int width)
{
    const lv_font_t *caption=&lv_font_montserrat_14;
    const lv_font_t *body=&lv_font_montserrat_20;
    int inner=width-28;
    if (p->studio) {
        /* Studio's open header, borderless model well, divided instruments and
         * eight-item bottom dock replace the generic card/action sample. */
        caption=&ui_studio_font_18;
        body=&ui_studio_font_20;
        int split=14+inner*52/100;
        int right=split+12, right_width=width-14-right;
        sample_text(scene,"Dashboard",body,p->text,14,0,inner-72,false);
        sample_rect(scene,width-72,8,6,6,p->success,0,3,LV_OPA_COVER);
        sample_text(scene,"Ready",caption,p->muted,width-62,0,60,false);
        sample_rect(scene,14,29,inner,1,p->border,0,0,LV_OPA_COVER);
        /* A stepped printed part on a build plate, scaled to the model well. */
        int model_w=(split-28)*60/100;
        int model_x=14+(split-14-model_w)/2;
        sample_rect(scene,model_x,90,model_w,5,0xB59CFF,0,2,LV_OPA_COVER);
        sample_rect(scene,model_x+5,70,model_w-10,20,p->accent,0,3,LV_OPA_COVER);
        sample_rect(scene,model_x+model_w/4,49,model_w/2,22,0xB59CFF,0,3,LV_OPA_COVER);
        sample_rect(scene,split,40,1,66,p->border,0,0,LV_OPA_COVER);
        sample_text(scene,"Nozzle",caption,p->muted,right,35,right_width,false);
        sample_text(scene,"215 / 220",body,p->text,right,55,right_width,false);
        sample_text(scene,"Bed 60 C",caption,p->muted,right,82,right_width,false);
        sample_rect(scene,14,110,inner,3,0x3A3B45,0,1,LV_OPA_COVER);
        sample_rect(scene,14,110,inner*72/100,3,p->accent,0,1,LV_OPA_COVER);
        sample_rect(scene,14,121,inner,35,p->surface,0,12,LV_OPA_COVER);
        for (int i=0;i<8;i++) {
            int start=14+inner*i/8, end=14+inner*(i+1)/8;
            if (!i) sample_rect(scene,start+2,124,end-start-4,29,p->control,0,9,LV_OPA_COVER);
            sample_studio_icon(scene,(unsigned)i,start+(end-start-26)/2,126,i?p->muted:0xB59CFF);
        }
        return;
    }
    const ui_dashboard_layout_profile_t *profile=ui_dashboard_layout_profile_for_theme(p->theme);
    caption=&lv_font_montserrat_12;
    body=&lv_font_montserrat_14;
    /* The production dashboard profile determines the preview/instrument split.
     * All four legacy shells retain the same eight-item left navigation rail. */
    int rail=36, start=14+rail+8, content=width-14-start;
    int gap=6, print_width=(content-gap)*profile->active_print.width/
        (profile->active_print.width+profile->machine_status.width);
    int instruments=start+print_width+gap, instrument_width=width-14-instruments;
    int radius=p->radius/3;
    sample_rect(scene,14,0,inner,20,p->surface,p->border,radius,p->surface_opa);
    sample_text(scene,"PrinterHMI",body,p->text,22,2,inner-80,false);
    sample_text(scene,"Ready",caption,p->success,width-66,3,50,false);
    sample_rect(scene,14,26,rail,130,p->surface,p->border,radius,p->surface_opa);
    const char *icons[]={LV_SYMBOL_HOME,LV_SYMBOL_LIST,LV_SYMBOL_FILE,LV_SYMBOL_IMAGE,
        LV_SYMBOL_SETTINGS,LV_SYMBOL_EDIT,LV_SYMBOL_LOOP,LV_SYMBOL_SETTINGS};
    for (int i=0;i<8;i++) {
        if (!i) sample_rect(scene,17,29,rail-6,13,p->control,p->accent,3,LV_OPA_COVER);
        sample_text(scene,icons[i],caption,i?p->muted:p->accent,14,29+i*15,rail,true);
    }
    sample_rect(scene,start,26,content,16,p->surface,p->success,radius,p->surface_opa);
    sample_text(scene,"PRINTING  72%",caption,p->text,start+6,27,content-12,false);
    sample_rect(scene,start,48,print_width,80,p->card,p->border,radius,p->surface_opa);
    sample_text(scene,"Print",caption,p->text,start+6,51,print_width-12,false);
    int model_width=print_width*55/100, model_x=start+(print_width-model_width)/2;
    sample_rect(scene,model_x,107,model_width,4,p->muted,0,1,LV_OPA_COVER);
    sample_rect(scene,model_x+3,90,model_width-6,17,p->accent,0,2,LV_OPA_COVER);
    sample_rect(scene,model_x+model_width/4,75,model_width/2,16,p->accent,0,2,LV_OPA_COVER);
    sample_rect(scene,start+6,119,print_width-12,3,p->background,0,1,LV_OPA_COVER);
    sample_rect(scene,start+6,119,(print_width-12)*72/100,3,p->accent,0,1,LV_OPA_COVER);
    bool split=profile->machine_content.composition==UI_DASHBOARD_MACHINE_SPLIT_CARDS;
    sample_rect(scene,instruments,48,instrument_width,split?39:80,p->card,p->border,radius,p->surface_opa);
    sample_text(scene,"Nozzle",caption,p->muted,instruments+5,51,instrument_width-10,false);
    sample_text(scene,"215/220",body,p->text,instruments+5,65,instrument_width-10,false);
    if (split) sample_rect(scene,instruments,92,instrument_width,36,p->card,p->border,radius,p->surface_opa);
    sample_text(scene,"Bed 60 C",caption,p->muted,instruments+5,split?96:90,instrument_width-10,false);
    if (!split) sample_text(scene,"Fan  80%",caption,p->muted,instruments+5,107,instrument_width-10,false);
    const char *actions[]={"Pause","Stop","Tools"};
    for (int i=0;i<3;i++) {
        int x=start+content*i/3, end=start+content*(i+1)/3;
        sample_rect(scene,x,135,end-x-4,21,p->control,i==1?p->danger:p->border,radius,p->surface_opa);
        sample_text(scene,actions[i],caption,i==1?p->danger:p->text,x,139,end-x-4,true);
    }
    return;
}

static bool preview_cached_sample(lv_obj_t *parent,
                                  const ui_theme_preview_palette_t *p, int width)
{
    uint32_t stride=lv_draw_buf_width_to_stride(width,LV_COLOR_FORMAT_RGB565);
    void *pixels=heap_caps_calloc(1,(size_t)stride*156,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(!pixels) return false;
    lv_obj_t *canvas=lv_canvas_create(parent);
    if(!canvas){heap_caps_free(pixels);return false;}
    lv_obj_remove_style_all(canvas);
    lv_obj_clear_flag(canvas,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
    lv_canvas_set_buffer(canvas,pixels,width,156,LV_COLOR_FORMAT_RGB565);
    lv_obj_add_event_cb(canvas,preview_pixels_deleted,LV_EVENT_DELETE,pixels);
    lv_canvas_fill_bg(canvas,lv_color_hex(p->background),LV_OPA_COVER);
    lv_layer_t layer;
    lv_canvas_init_layer(canvas,&layer);
    preview_scene_t scene={.layer=&layer,.glass=p->theme==UI_THEME_GLASS};
    preview_draw_scene(&scene,p,width);
    lv_canvas_finish_layer(canvas,&layer);
    return true;
}

static void preview_add_sample_ui(lv_obj_t *parent,
                                  const ui_theme_preview_palette_t *palette,
                                  int32_t preview_width)
{
    if (preview_cached_sample(parent,palette,preview_width)) return;
    preview_scene_t scene={.parent=parent,.glass=palette->theme==UI_THEME_GLASS};
    preview_draw_scene(&scene,palette,preview_width);
}

lv_obj_t *ui_theme_preview_create(
    lv_obj_t *parent,
    ui_theme_id_t theme,
    bool selected,
    int32_t x,
    int32_t y,
    int32_t width,
    int32_t height,
    lv_event_cb_t event_cb,
    void *user_data)
{
    if (!parent) return NULL;

    ui_theme_preview_palette_t palette = preview_palette(theme);
    palette.theme = theme;
    lv_obj_t *button = lv_button_create(parent);

    if (!button) return NULL;

    lv_obj_remove_style_all(button);
    lv_obj_set_size(button, width, height);
    lv_obj_set_pos(button, x, y);
    lv_obj_clear_flag(button, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(button, 0, 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(palette.background), 0);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(
        button,
        lv_color_hex(selected ? palette.accent : palette.border),
        0);
    lv_obj_set_style_border_width(button, selected ? 4 : 2, 0);
    lv_obj_set_style_radius(button, palette.radius, 0);
    lv_obj_set_style_shadow_width(button, 0, 0);
    lv_obj_set_style_outline_width(button, 0, 0);
    ui_button_stable_press(button);
    lv_obj_set_style_outline_color(button,lv_color_hex(palette.accent),LV_STATE_FOCUS_KEY);

    lv_obj_set_height(button, LV_SIZE_CONTENT);
    lv_obj_set_style_min_height(button, height, 0);
    lv_obj_set_flex_flow(button, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(button, 10, 0);
    lv_obj_set_style_pad_row(button, 8, 0);
    lv_obj_t *heading = lv_obj_create(button);
    lv_obj_remove_style_all(heading);
    lv_obj_clear_flag(heading, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(heading, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(heading, LV_FLEX_FLOW_ROW);
    lv_obj_t *name = preview_label(heading, palette.name, UI_FONT_BODY_LARGE, palette.text, 0, 0);
    lv_obj_set_width(name, 0); lv_obj_set_flex_grow(name, 1);
    if (selected) preview_label(heading, LV_SYMBOL_OK, UI_FONT_BODY_LARGE, palette.success, 0, 0);
    lv_obj_t *description = lv_label_create(button);
    lv_label_set_text_fmt(description, "%s | %s", palette.code, palette.description);
    lv_obj_set_width(description, LV_PCT(100));
    lv_obj_set_style_text_font(description, ui_font_with_fallback(UI_FONT_CAPTION), 0);
    lv_obj_set_style_text_color(description, lv_color_hex(palette.muted), 0);
    lv_obj_t *sample = lv_obj_create(button);
    lv_obj_remove_style_all(sample);
    lv_obj_clear_flag(sample, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(sample, LV_PCT(100), 156);
    preview_add_sample_ui(sample, &palette, width - 20 - (selected ? 8 : 4));

    if (event_cb) {
        lv_obj_add_event_cb(
            button,
            event_cb,
            LV_EVENT_CLICKED,
            user_data);
    }

    return button;
}
