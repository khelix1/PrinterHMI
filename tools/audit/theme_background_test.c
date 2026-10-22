/* Real LVGL rendering and the real v3 parser; ESP heap/log boundaries only. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int fail_background_alloc;
#include "custom_theme.c"
#include "custom_theme_background.c"
static void flush(lv_display_t *display, const lv_area_t *area, uint8_t *pixels)
{ (void)area; (void)pixels; lv_display_flush_ready(display); }
int main(int argc,char **argv)
{
    assert(argc==4);
    custom_theme_entry_t entry;
    assert(parse_theme_file(argv[1],&entry));
    custom_theme_entry_t studio;assert(parse_theme_file(argv[3],&studio));
    assert(studio.summary.base_theme==UI_THEME_STUDIO_DARK);
    assert(theme_background_asset_name_valid(strrchr(entry.background_path,'/')+1));
    assert(!theme_background_asset_name_valid("../background.phmibg"));
    assert(!theme_background_asset_name_valid(NULL));
    s_themes=&entry;s_theme_count=1;s_active_index=0;
    lv_init();lv_display_t *display=lv_display_create(1024,600);assert(display);
    lv_display_set_color_format(display,LV_COLOR_FORMAT_RGB565);
    uint8_t *buffer=calloc(1,THEME_BACKGROUND_BYTES);assert(buffer);
    lv_display_set_buffers(display,buffer,NULL,THEME_BACKGROUND_BYTES,LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(display,flush);
    lv_obj_t *screen=lv_screen_active();lv_obj_set_style_bg_color(screen,lv_color_hex(0xFFFFFF),0);
    ui_theme_set_root_decorator(custom_theme_background_apply);
    ui_theme_decorate_root(screen);assert(s_cache&&s_cache->references==1);
    const lv_image_dsc_t *image=lv_obj_get_style_bg_image_src(screen,0);
    assert(image&&image->header.w==1024&&image->header.h==600);
    lv_refr_now(display);
    assert(((uint16_t *)buffer)[80*1024+190]==0xF800);
    assert(((uint16_t *)buffer)[80*1024+600]==0x001F);
    lv_obj_t *page=lv_obj_create(screen);lv_obj_set_pos(page,170,72);lv_obj_set_size(page,854,528);
    lv_obj_set_style_border_width(page,0,0);lv_obj_set_style_radius(page,0,0);
    lv_obj_set_style_bg_color(page,lv_color_hex(0xFF00FF),0);
    ui_theme_decorate_root(page);assert(s_cache->references==2);
    image=lv_obj_get_style_bg_image_src(page,0);assert(image&&image->header.stride==2048);
    assert(image->data == s_cache->pixels + (72*1024+170)*2);
    lv_refr_now(display);
    assert(((uint16_t *)buffer)[80*1024+190]==0xF800);
    assert(((uint16_t *)buffer)[80*1024+600]==0x001F);
    assert(((uint16_t *)buffer)[599*1024+1023]==0x001F);
    lv_obj_t *dialog=lv_obj_create(screen);lv_obj_set_size(dialog,500,300);
    ui_theme_decorate_root(dialog);assert(!lv_obj_get_style_bg_image_src(dialog,0));lv_obj_delete(dialog);
    /* Rescanning/deactivating metadata must not free descriptors pinned by pages. */
    s_active_index=-1;ui_theme_decorate_root(screen);assert(s_cache&&s_cache->references==1);
    lv_refr_now(display);assert(((uint16_t *)buffer)[80*1024+190]==0xF800);
    lv_obj_delete(page);assert(!s_cache);
    s_active_index=0;fail_background_alloc=1;ui_theme_decorate_root(screen);assert(!s_cache);
    assert(!lv_obj_get_style_bg_image_src(screen,0));fail_background_alloc=0;
    ui_accessibility_t accessible={.high_contrast=true};ui_theme_set_accessibility(accessible);
    ui_theme_decorate_root(screen);assert(!s_cache);
    accessible.high_contrast=false;ui_theme_set_accessibility(accessible);
    /* Corrupt pixels pass metadata/header checks but fail the full CRC read. */
    FILE *file=fopen(entry.background_path,"r+b");assert(file);assert(!fseek(file,-1,SEEK_END));fputc(1,file);fclose(file);
    ui_theme_decorate_root(screen);assert(!s_cache&&!lv_obj_get_style_bg_image_src(screen,0));
    assert(!parse_theme_file(argv[2],&entry));
    lv_display_delete(display);free(buffer);lv_deinit();
    puts("PASS: artwork v3 parser, actual RGB565 screen/page rendering, shared pixel references, registry detachment, deletion, CRC/PSRAM failures and high-contrast fallback");
}
