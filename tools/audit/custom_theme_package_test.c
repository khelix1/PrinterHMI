/* Real SD parser + runtime token dispatch; filesystem fixtures supplied by Python. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "custom_theme.c"
static void check_file(const char *dir,const char *name,bool expected,custom_theme_entry_t *out){
 char path[512];snprintf(path,sizeof(path),"%s/%s.json",dir,name);
 assert(parse_theme_file(path,out)==expected);
}
int main(int argc,char **argv){
 assert(argc==2);custom_theme_entry_t entry;
 check_file(argv[1],"blank",true,&entry);
 assert(entry.dashboard.active_print.x==2000);
 assert(entry.dashboard.machine_status.x==2000);
 assert(entry.pages.settings.content.x==16);
 for(int i=0;i<4;i++)check_file(argv[1],(const char *const[]){"studio","operator","foundry","glass"}[i],true,&entry);
 for(int i=0;i<8;i++)check_file(argv[1],(const char *const[]){"missing_color","invalid_color","studio_legacy","studio_layout","studio_opacity","missing_metric","overflow_layout","oversized"}[i],false,&entry);
 check_file(argv[1],"studio",true,&entry);s_themes=&entry;s_theme_count=1;s_active_index=0;
 ui_theme_set_active(UI_THEME_STUDIO_DARK);
 assert(!custom_theme_dashboard_profile()&&!custom_theme_page_profile());
 /* Exercise every v2 color, including mask bits above 31 and the highest bit. */
 for(size_t i=0;i<sizeof(s_color_routes)/sizeof(s_color_routes[0]);i++){
  const color_route_t *r=&s_color_routes[i];uint32_t rgb=0;
  assert(custom_theme_color_override(r->operator_rgb,r->foundry_rgb,r->glass_rgb,&rgb));
  assert(rgb==0x123456);
  assert(lv_color_to_u32(ui_theme_studio_color(r->operator_rgb,r->foundry_rgb,r->glass_rgb,0xFFFFFF))==lv_color_to_u32(lv_color_hex(0x123456)));
 }
 assert(ui_theme_studio_metric(12,20,22,18)==3);
 assert(ui_theme_studio_metric(10,18,20,24)==4);
 assert(ui_theme_studio_metric(10,20,24,18)==5);
 check_file(argv[1],"operator",true,&entry);
 assert(custom_theme_dashboard_profile()&&custom_theme_page_profile());
 uint32_t rgb;assert(!custom_theme_color_override(0x112233,0x223344,0x334455,&rgb));
 assert(!(entry.color_mask & (UINT64_C(1)<<COLOR_DANGER_DARK)));
 s_active_index=-1;
 assert(lv_color_to_u32(ui_theme_studio_color(1,2,3,0xABCDEF))==lv_color_to_u32(lv_color_hex(0xABCDEF)));
 assert(ui_theme_studio_metric(12,20,22,18)==18);
 puts("PASS: real theme parser accepts browser exports; invalid contracts rejected; all 58 Studio color routes, radii, v1 geometry and built-in fallbacks verified");
}
