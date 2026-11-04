/* Shared offline Files backend. Every widget comes from the production Files
 * UI. */
#include "ui_demo_files.h"
#include "demo_files.h"
#include "moonraker.h"
#include "moonraker_config_controller.h"
#include "printer_connection_policy.h"
#include "ui_files.h"
#include "ui_preview_lightbox.h"
#include "ui_shell.h"
#include "ui_theme.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
static bool files_open;
static lv_obj_t *files_root;
static uint32_t profile_owner;
static bool (*files_send)(const char *);
static char folder[64], query[64];
static unsigned sort_kind;
static ui_thumbnail_t *detail_thumbnail;
const lv_image_dsc_t *ui_demo_files_image(const char *path) {
  static lv_image_dsc_t images[7];
  const demo_file_t *file = demo_files_find(path);
  if (!file)
    return NULL;
  size_t index = 0;
  while (index < demo_files_count() && demo_files_at(index) != file)
    ++index;
  if (index >= sizeof(images) / sizeof(images[0]))
    return NULL;
  lv_image_dsc_t *image = &images[index];
  if (!image->data) {
    int w = 0, h = 0;
    const uint16_t *pixels = demo_files_pixels(path, &w, &h);
    if (!pixels)
      return NULL;
    image->header.magic = LV_IMAGE_HEADER_MAGIC;
    image->header.cf = LV_COLOR_FORMAT_RGB565;
    image->header.w = w;
    image->header.h = h;
    image->header.stride = w * 2;
    image->data_size = w * h * 2;
    image->data = (const uint8_t *)pixels;
  }
  return image;
}
static bool current(void) {
  return files_open && files_root == ui_files_get_popup() &&
         profile_owner == moonraker_config_generation();
}
static void root_deleted(lv_event_t *event) {
  if (lv_event_get_target_obj(event) == files_root) {
    files_root = NULL;
    files_open = false;
    files_send = NULL;
  }
}
static bool contains(const char *text, const char *needle) {
  for (; *text; text++) {
    const char *a = text, *b = needle;
    while (*a && *b &&
           tolower((unsigned char)*a) == tolower((unsigned char)*b)) {
      a++;
      b++;
    }
    if (!*b)
      return true;
  }
  return !*needle;
}
static void preview(const char *path) {
  if (current() && demo_files_find(path))
    ui_files_set_file_thumbnail(path, ui_demo_files_image(path));
}
static int compare(const void *left, const void *right) {
  unsigned a = *(const unsigned *)left, b = *(const unsigned *)right;
  if (sort_kind == 1)
    return a > b ? -1 : a < b ? 1 : 0;
  if (sort_kind == 2 && demo_files_at(a)->mib != demo_files_at(b)->mib)
    return demo_files_at(a)->mib > demo_files_at(b)->mib ? -1 : 1;
  return strcasecmp(demo_files_at(a)->path, demo_files_at(b)->path);
}
static void refresh(void) {
  if (!current() || !ui_files_can_refresh_rows())
    return;
  ui_files_clear_rows();
  ui_files_set_status("");
  ui_files_set_breadcrumb(folder[0] ? folder : NULL);
  ui_files_set_search_text(query);
  const char *sorts[] = {"Name", "Newest", "Size"};
  ui_files_set_sort_text(sorts[sort_kind]);
  unsigned order[16], count = 0;
  for (unsigned i = 0; i < demo_files_count(); i++) {
    const char *slash = strchr(demo_files_at(i)->path, '/');
    if (!query[0] && ((folder[0] && (!slash || strncmp(demo_files_at(i)->path,
                                                       "Calibration/", 12))) ||
                      (!folder[0] && slash)))
      continue;
    if (count < sizeof(order) / sizeof(order[0]) &&
        contains(demo_files_at(i)->path, query))
      order[count++] = i;
  }
  qsort(order, count, sizeof(*order), compare);
  int y = 0, step = ui_theme_density_metric(76, 92, 104);
  if (!folder[0] && !query[0]) {
    ui_files_add_folder_button("Calibration", "Calibration", y);
    y += step;
  }
  for (unsigned i = 0; i < count; i++) {
    unsigned index = order[i];
    ui_files_add_file_entry(demo_files_at(index)->path,
                            demo_files_at(index)->mib * 1024 * 1024,
                            1700000000 + index * 86400, y);
    y += step;
  }
  if (!count && (folder[0] || query[0]))
    ui_files_set_status(query[0] ? "No files match current search"
                                 : "No files in this folder");
}
static void cancel_detail(void) {
  ui_files_close_detail_popup();
}
static bool start(const char *filename) {
  moonraker_state_t m;
  moonraker_state_snapshot(&m);
  if (!current() || !filename || !demo_files_find(filename) || !files_send ||
      printer_idle_reason(&m)[0])
    return false;
  char command[320];
  snprintf(command, sizeof(command), "SDCARD_PRINT_FILE FILENAME=\"%s\"",
           filename);
  lv_obj_t *root=files_root;uint32_t owner=profile_owner;
  bool sent=files_send(command);
  if(!current() || files_root!=root || profile_owner!=owner)return sent;
  if (!sent) {
    ui_files_print_feedback("Simulated command was not accepted.");
    return false;
  }
  cancel_detail();
  ui_shell_page_action(UI_SHELL_PAGE_DASHBOARD);
  return true;
}
static void select_file(const char *path) {
  const demo_file_t *file = demo_files_find(path);
  if (!current() || !file)
    return;
  char metadata[1600];
  if (!demo_files_metadata_text(path, metadata, sizeof(metadata)))
    return;
  lv_obj_t *box = NULL;
  ui_thumbnail_t *view = NULL;
  ui_files_show_detail_popup(path, metadata, &box, &view, cancel_detail, start);
  if (view) {
    ui_thumbnail_bind_owner(view, &detail_thumbnail);
    ui_thumbnail_show_file_image(view, ui_demo_files_image(path), 256, path);
  }
  ui_files_update_detail_metadata(metadata, true);
}
static void search(const char *text) {
  snprintf(query, sizeof(query), "%s", text ? text : "");
  cancel_detail();
  refresh();
}
static void sort(void) {
  sort_kind = (sort_kind + 1) % 3;
  refresh();
}
static void choose_folder(const char *path) {
  if (strcmp(path, "Calibration"))
    return;
  snprintf(folder, sizeof(folder), "Calibration");
  query[0] = 0;
  cancel_detail();
  refresh();
}
static void up(void) {
  if(query[0])query[0]=0;
  else folder[0]=0;
  cancel_detail();
  refresh();
}
void ui_demo_files_close(void) {
  if (!files_open)
    return;
  files_open = false;
  files_root = NULL;
  files_send = NULL;
  ui_preview_lightbox_close();
  ui_files_hide();
  ui_files_set_callbacks(NULL, NULL, NULL);
  ui_files_set_browser_callbacks(NULL, NULL, NULL, NULL);
}
void ui_demo_files_show(bool (*send)(const char *)) {
  ui_demo_files_close();
  files_open = true;
  profile_owner = moonraker_config_generation();
  files_send = send;
  folder[0] = query[0] = 0;
  sort_kind = 0;
  ui_files_set_callbacks(refresh, select_file, preview);
  ui_files_set_browser_callbacks(search, sort, choose_folder, up);
  ui_files_show();
  files_root = ui_files_get_popup();
  if (files_root)
    lv_obj_add_event_cb(files_root, root_deleted, LV_EVENT_DELETE, NULL);
  refresh();
}
