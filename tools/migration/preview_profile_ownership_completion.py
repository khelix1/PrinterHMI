#!/usr/bin/env python3
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
MAIN = ROOT / "main"


def read(name: str) -> str:
    path = MAIN / name
    if not path.exists():
        raise RuntimeError(f"missing required file: {path}")
    return path.read_text()


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"expected one {label}, found {count}")
    return text.replace(old, new, 1)


worker = read("printer_profile_preview_worker_v32.c")
main = read("main.c")
printer = read("ui_printer_v32.c")


marker = "PREVIEW_PROFILE_OWNERSHIP_COMPLETE"
if marker in main:
    print("PASS: preview profile ownership completion already installed")
    raise SystemExit(0)


# An idle printer retains its own last explicitly selected preview. A live
# printing/paused filename will replace it when it changes.
worker = replace_once(
    worker,
    '''    if (!state_has_current_print(state) || !file[0]) {
        printer_preview_cache_v32_invalidate(index);
        return;
    }
''',
    '''    if (!state_has_current_print(state) || !file[0]) {
        return;
    }
''',
    "inactive idle-preview invalidation",
)


# Capture the profile identity alongside every asynchronous Dashboard render.
main = replace_once(
    main,
    'static char dash_thumb_render_file[160] = "";\n',
    'static char dash_thumb_render_file[160] = "";\n'
    'static int dash_thumb_render_profile_index = -1;\n'
    'static uint32_t dash_thumb_render_generation = 0;\n',
    "Dashboard render identity globals",
)

main = replace_once(
    main,
    '''    safe_copy(dash_thumb_render_file, sizeof(dash_thumb_render_file), thumbnail_session_v32_selected_file());

    dash_thumb_render_running = true;
''',
    '''    safe_copy(dash_thumb_render_file, sizeof(dash_thumb_render_file), thumbnail_session_v32_selected_file());
    dash_thumb_render_profile_index =
        moonraker_config_active_profile_index();
    dash_thumb_render_generation =
        moonraker_config_generation();

    dash_thumb_render_running = true;
''',
    "Dashboard render identity capture",
)


# File-browser previews publish even when the user cancels instead of printing.
sync_old = '''    /* CACHE_PUBLISH_SYNC */
    if (printer_controller_is_live_state(printer_state) &&
        strcmp(thumbnail_session_v32_selected_file(), printer_file) == 0) {
        printer_preview_cache_v32_publish_active(
            printer_file,
            dash_thumb_canvas_buf,
            DASH_THUMB_CANVAS_W,
            DASH_THUMB_CANVAS_H);
    }
'''

sync_new = '''    /* CACHE_PUBLISH_SYNC: PREVIEW_PROFILE_OWNERSHIP_COMPLETE */
    const char *cache_file =
        thumbnail_session_v32_selected_file();

    if (cache_file && cache_file[0]) {
        printer_preview_cache_v32_publish_active(
            cache_file,
            dash_thumb_canvas_buf,
            DASH_THUMB_CANVAS_W,
            DASH_THUMB_CANVAS_H);
    }
'''

main = replace_once(
    main,
    sync_old,
    sync_new,
    "synchronous preview cache publish",
)


async_old = '''        if (ok) {
            if (printer_controller_is_live_state(printer_state) &&
                strcmp(dash_thumb_render_file, printer_file) == 0) {
                printer_preview_cache_v32_publish_active(
                    dash_thumb_render_file,
                    dash_thumb_canvas_buf,
                    DASH_THUMB_CANVAS_W,
                    DASH_THUMB_CANVAS_H);
            }

            ESP_LOGI(TAG,
'''

async_new = '''        if (ok) {
            bool same_profile =
                dash_thumb_render_generation ==
                    moonraker_config_generation() &&
                dash_thumb_render_profile_index ==
                    moonraker_config_active_profile_index();

            if (same_profile && dash_thumb_render_file[0]) {
                printer_preview_cache_v32_publish_active(
                    dash_thumb_render_file,
                    dash_thumb_canvas_buf,
                    DASH_THUMB_CANVAS_W,
                    DASH_THUMB_CANVAS_H);
            } else if (!same_profile) {
                ESP_LOGW(TAG,
                         "Discarded stale Dashboard preview render");
            }

            ESP_LOGI(TAG,
'''

main = replace_once(
    main,
    async_old,
    async_new,
    "asynchronous preview cache publish",
)


# Never apply a completed old-profile render to the current Dashboard.
poll_old = '''    if (dash_thumb_render_ready && !dash_thumb_render_failed) {
        dashboard_apply_rendered_thumbnail();
    }
'''

poll_new = '''    if (dash_thumb_render_ready &&
        !dash_thumb_render_failed &&
        dash_thumb_render_generation == moonraker_config_generation() &&
        dash_thumb_render_profile_index ==
            moonraker_config_active_profile_index()) {
        dashboard_apply_rendered_thumbnail();
    }
'''

main = replace_once(
    main,
    poll_old,
    poll_new,
    "Dashboard render UI application guard",
)


# Restore or clear the Dashboard directly from the newly selected profile slot.
declaration_anchor = '''static void test_moonraker_now(void);
static void safe_copy(char *dst, size_t dst_len, const char *src);
'''

main = replace_once(
    main,
    declaration_anchor,
    declaration_anchor +
    'static void dashboard_restore_active_profile_preview(void);\n',
    "Dashboard restore declaration",
)

restore_function = r'''

static void dashboard_restore_active_profile_preview(void)
{
    const char *file = NULL;
    uint32_t revision = 0;

    const lv_image_dsc_t *image =
        printer_preview_cache_v32_image(
            moonraker_config_active_profile_index(),
            &file,
            &revision);

    (void)revision;

    if (!image ||
        !file || !file[0] ||
        image->header.cf != LV_COLOR_FORMAT_RGB565 ||
        image->header.w != DASH_THUMB_CANVAS_W ||
        image->header.h != DASH_THUMB_CANVAS_H ||
        image->data_size <
            DASH_THUMB_CANVAS_W *
            DASH_THUMB_CANVAS_H * sizeof(uint16_t)) {
        ui_dashboard_v32_thumb_delete_canvas();
        ui_dashboard_v32_thumb_set_placeholder(
            "PRINT\nTHUMBNAIL\n\nNo preview loaded");
        return;
    }

    if (!ui_dashboard_v32_thumb_ensure_canvas_buffer(
            DASH_THUMB_CANVAS_W * DASH_THUMB_CANVAS_H)) {
        return;
    }

    memcpy(
        dash_thumb_canvas_buf,
        image->data,
        DASH_THUMB_CANVAS_W *
            DASH_THUMB_CANVAS_H * sizeof(uint16_t));

    ui_dashboard_v32_thumb_show_canvas_from_buffer(
        DASH_THUMB_CANVAS_W,
        DASH_THUMB_CANVAS_H,
        file);
}
'''

restore_anchor = '''static void dash_thumb_render_task(void *arg)
'''

main = replace_once(
    main,
    restore_anchor,
    restore_function + '\n\n' + restore_anchor,
    "Dashboard restore function anchor",
)


# Clear all global presentation state on a profile switch, then bind the
# Dashboard to the newly active profile's cache.
main = replace_once(
    main,
    '''static void printer_profiles_active_changed_bridge(void)
{
    reset_active_printer_runtime_state();
''',
    '''static void printer_profiles_active_changed_bridge(void)
{
    reset_preview_state_for_host_change();
    reset_active_printer_runtime_state();
    dashboard_restore_active_profile_preview();
''',
    "active-profile preview reset",
)

main = replace_once(
    main,
    '''    ui_printer_chooser_v32_hide();
    ui_dashboard_v32_create();
    ui_shell_set_active_nav(UI_SHELL_PAGE_DASHBOARD);
''',
    '''    ui_printer_chooser_v32_hide();
    ui_dashboard_v32_create();
    dashboard_restore_active_profile_preview();
    ui_shell_set_active_nav(UI_SHELL_PAGE_DASHBOARD);
''',
    "chooser Dashboard cache binding",
)


# Printer page identity is (profile, revision), not revision alone. Two profile
# slots can legitimately both be at revision 1.
printer = replace_once(
    printer,
    'static uint32_t s_preview_cache_revision = 0;\n',
    'static uint32_t s_preview_cache_revision = 0;\n'
    'static int s_preview_cache_profile_index = -1;\n',
    "Printer preview cache identity",
)

old_cache_block = r'''    if (!preview_file || !preview_file[0]) {
        preview_show_placeholder();
        return;
    }

    const char *cached_file = NULL;
    uint32_t cached_revision = 0;

    const lv_image_dsc_t *cached_image =
        printer_preview_cache_v32_image(
            moonraker_config_active_profile_index(),
            &cached_file,
            &cached_revision);

    if (cached_image &&
        cached_file &&
        strcmp(cached_file, preview_file) == 0) {
'''

new_cache_block = r'''    const char *cached_file = NULL;
    uint32_t cached_revision = 0;
    int active_profile =
        moonraker_config_active_profile_index();

    const lv_image_dsc_t *cached_image =
        printer_preview_cache_v32_image(
            active_profile,
            &cached_file,
            &cached_revision);

    if (cached_image &&
        cached_file &&
        cached_file[0] &&
        (!preview_file ||
         !preview_file[0] ||
         strcmp(cached_file, preview_file) == 0)) {
'''

printer = replace_once(
    printer,
    old_cache_block,
    new_cache_block,
    "Printer cache-first selection",
)

printer = replace_once(
    printer,
    '''        if (s_preview_image &&
            s_preview_cache_revision != cached_revision) {
''',
    '''        if (s_preview_image &&
            (s_preview_cache_profile_index != active_profile ||
             s_preview_cache_revision != cached_revision)) {
''',
    "Printer profile/revision refresh condition",
)

printer = replace_once(
    printer,
    '''            s_preview_cache_revision = cached_revision;
        }
''',
    '''            s_preview_cache_revision = cached_revision;
            s_preview_cache_profile_index = active_profile;
        }
''',
    "Printer cache identity update",
)

printer = replace_once(
    printer,
    '''        return;
    }

    if (!thumbnail_manager_v32_has_png()) {
''',
    '''        return;
    }

    if (!preview_file || !preview_file[0]) {
        preview_show_placeholder();
        return;
    }

    if (!thumbnail_manager_v32_has_png()) {
''',
    "Printer cache-first fallback",
)

printer = replace_once(
    printer,
    '''    s_preview_canvas_file[0] = '\0';
    s_preview_cache_revision = 0;
''',
    '''    s_preview_canvas_file[0] = '\0';
    s_preview_cache_revision = 0;
    s_preview_cache_profile_index = -1;
''',
    "Printer preview reset identity",
)


(MAIN / "printer_profile_preview_worker_v32.c").write_text(worker)
(MAIN / "main.c").write_text(main)
(MAIN / "ui_printer_v32.c").write_text(printer)

print("PASS: profile-owned preview behavior completed")
print("  - selected-file preview survives Cancel and idle polling")
print("  - chooser, Dashboard, and Printer bind to the same profile slot")
print("  - profile index plus endpoint owns every cache entry")
print("  - stale asynchronous renders are discarded after switching")
print("  - no preview can remain visible from another profile")
print("Next: idf.py build")

