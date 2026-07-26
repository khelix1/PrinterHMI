#include "custom_theme.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

#include "cJSON.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

#define CUSTOM_THEME_DIRECTORY "/sdcard/PrinterHMI/themes"
#define CUSTOM_THEME_MAX_BYTES (48 * 1024)
#define CUSTOM_THEME_PATH_MAX 192

static const char *TAG = "custom_theme";

typedef enum {
    COLOR_BACKGROUND = 0,
    COLOR_BACKGROUND_DEEP,
    COLOR_POPUP,
    COLOR_TOPBAR,
    COLOR_NAVIGATION,
    COLOR_PANEL,
    COLOR_PANEL_ALT,
    COLOR_CARD,
    COLOR_CARD_DARK,
    COLOR_CONTROL,
    COLOR_CONTROL_ALT,
    COLOR_BORDER,
    COLOR_BORDER_SOFT,
    COLOR_BORDER_CONTROL,
    COLOR_TEXT,
    COLOR_TEXT_BRIGHT,
    COLOR_TEXT_DIM,
    COLOR_TEXT_MUTED,
    COLOR_ACCENT,
    COLOR_ACCENT_BRIGHT,
    COLOR_SUCCESS,
    COLOR_WARNING,
    COLOR_DANGER,
    COLOR_TELEMETRY_GRID,
    COLOR_TELEMETRY_BED,
    COLOR_TELEMETRY_CHAMBER,
    COLOR_TELEMETRY_HUMIDITY,
    COLOR_COUNT
} custom_color_id_t;

typedef enum {
    METRIC_RADIUS_CARD = 0,
    METRIC_RADIUS_BUTTON,
    METRIC_RADIUS_PANEL,
    METRIC_COUNT
} custom_metric_id_t;

typedef struct {
    const char *name;
    uint32_t operator_rgb;
    uint32_t foundry_rgb;
    uint32_t glass_rgb;
    custom_color_id_t id;
} color_route_t;

static const color_route_t s_color_routes[] = {
    {"background", 0x0B1118, 0x18130F, 0x03040A, COLOR_BACKGROUND},
    {"background_deep", 0x06101B, 0x0F0B09, 0x010207, COLOR_BACKGROUND_DEEP},
    {"popup", 0x0B1324, 0x261A14, 0x080B16, COLOR_POPUP},
    {"topbar", 0x111A24, 0x2A1C14, 0x080C18, COLOR_TOPBAR},
    {"navigation", 0x101821, 0x211711, 0x060A14, COLOR_NAVIGATION},
    {"panel", 0x101B2A, 0x2B211B, 0x0A1020, COLOR_PANEL},
    {"panel_alt", 0x101A25, 0x35271E, 0x11152A, COLOR_PANEL_ALT},
    {"card", 0x101B2A, 0x32251E, 0x0C1428, COLOR_CARD},
    {"card_dark", 0x0B1118, 0x15100D, 0x040711, COLOR_CARD_DARK},
    {"control", 0x1A2633, 0x49362B, 0x111D36, COLOR_CONTROL},
    {"control_alt", 0x162235, 0x3B2C24, 0x17142E, COLOR_CONTROL_ALT},
    {"border", 0x25476A, 0x6B4B36, 0x29466A, COLOR_BORDER},
    {"border_soft", 0x31445C, 0x594236, 0x1B2A43, COLOR_BORDER_SOFT},
    {"border_control", 0x35506F, 0x8A6245, 0x3A67A0, COLOR_BORDER_CONTROL},
    {"text", 0xE8F1FF, 0xF4E8DA, 0xEAF3FF, COLOR_TEXT},
    {"text_bright", 0xFFFFFF, 0xFFF9F2, 0xFFFFFF, COLOR_TEXT_BRIGHT},
    {"text_dim", 0x8FA7C2, 0xC5A88E, 0x91A6D8, COLOR_TEXT_DIM},
    {"text_muted", 0xAFC7E8, 0xD7C3AF, 0xB7C9F5, COLOR_TEXT_MUTED},
    {"success", 0x70E000, 0x6DD39E, 0x35FFC6, COLOR_SUCCESS},
    {"warning", 0xFFC857, 0xF2B84B, 0xFFE56B, COLOR_WARNING},
    {"danger", 0xFF4D4D, 0xF06A66, 0xFF4FA3, COLOR_DANGER},
    {"telemetry_grid", 0x173047, 0x5C4031, 0x1B3152, COLOR_TELEMETRY_GRID},
    {"telemetry_bed", 0xFFCF66, 0xF6C453, 0xFFD166, COLOR_TELEMETRY_BED},
    {"telemetry_chamber", 0x35E0D0, 0x61D0BE, 0x3BFFDA, COLOR_TELEMETRY_CHAMBER},
    {"telemetry_humidity", 0xA679FF, 0xC994E8, 0xE56BFF, COLOR_TELEMETRY_HUMIDITY},
};

typedef struct {
    int32_t operator_value;
    int32_t foundry_value;
    int32_t glass_value;
    custom_metric_id_t id;
} metric_route_t;

static const metric_route_t s_metric_routes[] = {
    {12, 20, 22, METRIC_RADIUS_CARD},
    {10, 18, 20, METRIC_RADIUS_BUTTON},
    {10, 20, 24, METRIC_RADIUS_PANEL},
};

typedef struct {
    custom_theme_summary_t summary;
    char path[CUSTOM_THEME_PATH_MAX];
    uint64_t color_mask;
    uint32_t colors[COLOR_COUNT];
    uint8_t metric_mask;
    int32_t metrics[METRIC_COUNT];
    bool has_surface_opacity;
    uint8_t surface_opacity;
    ui_dashboard_layout_profile_t dashboard;
    ui_page_layout_profile_t pages;
} custom_theme_entry_t;

static custom_theme_entry_t *s_themes = NULL;
static size_t s_theme_count = 0;
static int s_active_index = -1;

static void safe_copy(char *destination, size_t size, const char *source)
{
    if (!destination || size == 0) return;
    snprintf(destination, size, "%s", source ? source : "");
}

static bool ends_with_theme(const char *name)
{
    if (!name) return false;
    size_t length = strlen(name);
    const char *suffix = ".phmitheme";
    size_t suffix_length = strlen(suffix);
    if (length <= suffix_length) return false;
    return strcasecmp(name + length - suffix_length, suffix) == 0;
}

static bool valid_id(const char *id)
{
    if (!id) return false;
    size_t length = strlen(id);
    if (length < 2 || length > CUSTOM_THEME_ID_MAX) return false;
    if (!isalnum((unsigned char)id[0])) return false;

    for (size_t index = 0; index < length; ++index) {
        unsigned char value = (unsigned char)id[index];
        if (!(value >= 'a' && value <= 'z') &&
            !(value >= '0' && value <= '9') &&
            value != '-') {
            return false;
        }
    }
    return true;
}

static bool json_string(
    const cJSON *object,
    const char *name,
    char *output,
    size_t output_size,
    size_t maximum,
    bool required)
{
    const cJSON *item =
        cJSON_GetObjectItemCaseSensitive(object, name);

    if (!cJSON_IsString(item) || !item->valuestring) {
        if (!required) {
            if (output && output_size) output[0] = 0;
            return true;
        }
        return false;
    }

    size_t length = strlen(item->valuestring);
    if (length == 0 || length > maximum || length >= output_size) {
        return false;
    }

    safe_copy(output, output_size, item->valuestring);
    return true;
}

static bool json_integer(
    const cJSON *object,
    const char *name,
    int minimum,
    int maximum,
    int *output)
{
    const cJSON *item =
        cJSON_GetObjectItemCaseSensitive(object, name);
    if (!cJSON_IsNumber(item) ||
        item->valuedouble != (double)item->valueint ||
        item->valueint < minimum ||
        item->valueint > maximum) {
        return false;
    }
    if (output) *output = item->valueint;
    return true;
}

static bool parse_hex_color(const char *text, uint32_t *output)
{
    if (!text || strlen(text) != 7 || text[0] != '#') return false;
    char *end = NULL;
    unsigned long value = strtoul(text + 1, &end, 16);
    if (!end || *end != 0 || value > 0xFFFFFFUL) return false;
    if (output) *output = (uint32_t)value;
    return true;
}

static bool parse_rect(
    const cJSON *page,
    const char *name,
    ui_dashboard_rect_t *rect)
{
    const cJSON *object =
        cJSON_GetObjectItemCaseSensitive(page, name);
    if (!cJSON_IsObject(object) || !rect) return false;

    int x = 0, y = 0, width = 0, height = 0;
    const cJSON *visible =
        cJSON_GetObjectItemCaseSensitive(
            object,
            "visible");
    if (!json_integer(object, "x", 0, 814, &x) ||
        !json_integer(object, "y", 0, 508, &y) ||
        !json_integer(object, "width", 40, 854, &width) ||
        !json_integer(object, "height", 20, 528, &height) ||
        !cJSON_IsBool(visible) ||
        x + width > 854 ||
        y + height > 528) {
        return false;
    }

    /*
     * Existing page builders retain ownership of their objects and callback
     * wiring. Move a disabled surface outside the 854x528 application root
     * instead of creating theme-specific page implementations.
     */
    rect->x = cJSON_IsTrue(visible) ? x : 2000;
    rect->y = cJSON_IsTrue(visible) ? y : 2000;
    rect->width = width;
    rect->height = height;
    return true;
}

static ui_theme_id_t parse_base_theme(const cJSON *root, bool *valid)
{
    const cJSON *item =
        cJSON_GetObjectItemCaseSensitive(root, "base_theme");
    if (!cJSON_IsString(item) || !item->valuestring) {
        *valid = false;
        return UI_THEME_OPERATOR;
    }

    *valid = true;
    if (strcmp(item->valuestring, "operator") == 0) return UI_THEME_OPERATOR;
    if (strcmp(item->valuestring, "foundry") == 0) return UI_THEME_CLASSIC;
    if (strcmp(item->valuestring, "glass") == 0) return UI_THEME_GLASS;
    *valid = false;
    return UI_THEME_OPERATOR;
}

static bool parse_palette(
    const cJSON *root,
    custom_theme_entry_t *entry)
{
    const cJSON *palette =
        cJSON_GetObjectItemCaseSensitive(root, "palette");
    if (!cJSON_IsObject(palette)) return false;

    for (size_t index = 0;
         index < sizeof(s_color_routes) / sizeof(s_color_routes[0]);
         ++index) {
        const color_route_t *route = &s_color_routes[index];
        const cJSON *item =
            cJSON_GetObjectItemCaseSensitive(palette, route->name);
        if (!cJSON_IsString(item) ||
            !parse_hex_color(item->valuestring,
                             &entry->colors[route->id])) {
            return false;
        }
        entry->color_mask |= (UINT64_C(1) << route->id);
    }

    const cJSON *accent =
        cJSON_GetObjectItemCaseSensitive(palette, "accent");
    const cJSON *bright =
        cJSON_GetObjectItemCaseSensitive(palette, "accent_bright");
    if (!cJSON_IsString(accent) ||
        !cJSON_IsString(bright) ||
        !parse_hex_color(accent->valuestring,
                         &entry->colors[COLOR_ACCENT]) ||
        !parse_hex_color(bright->valuestring,
                         &entry->colors[COLOR_ACCENT_BRIGHT])) {
        return false;
    }
    entry->color_mask |= UINT64_C(1) << COLOR_ACCENT;
    entry->color_mask |= UINT64_C(1) << COLOR_ACCENT_BRIGHT;
    entry->summary.preview_background =
        entry->colors[COLOR_BACKGROUND];
    entry->summary.preview_card =
        entry->colors[COLOR_CARD];
    entry->summary.preview_accent =
        entry->colors[COLOR_ACCENT];
    entry->summary.preview_text =
        entry->colors[COLOR_TEXT];
    return true;
}

static bool parse_metrics(
    const cJSON *root,
    custom_theme_entry_t *entry)
{
    const cJSON *metrics =
        cJSON_GetObjectItemCaseSensitive(root, "metrics");
    if (!cJSON_IsObject(metrics)) return false;

    int radius_card = 0;
    int radius_button = 0;
    int radius_panel = 0;

    if (!json_integer(metrics, "radius_card", 0, 36,
                      &radius_card) ||
        !json_integer(metrics, "radius_button", 0, 36,
                      &radius_button) ||
        !json_integer(metrics, "radius_panel", 0, 36,
                      &radius_panel)) {
        return false;
    }

    entry->metrics[METRIC_RADIUS_CARD] = radius_card;
    entry->metrics[METRIC_RADIUS_BUTTON] = radius_button;
    entry->metrics[METRIC_RADIUS_PANEL] = radius_panel;
    entry->metric_mask =
        (1U << METRIC_RADIUS_CARD) |
        (1U << METRIC_RADIUS_BUTTON) |
        (1U << METRIC_RADIUS_PANEL);

    int opacity = 0;
    if (!json_integer(metrics, "surface_opacity", 96, 255, &opacity)) {
        return false;
    }
    entry->surface_opacity = (uint8_t)opacity;
    entry->has_surface_opacity = true;
    return true;
}

static bool parse_layouts(
    const cJSON *root,
    custom_theme_entry_t *entry)
{
    const cJSON *layouts =
        cJSON_GetObjectItemCaseSensitive(root, "layouts");
    if (!cJSON_IsObject(layouts)) return false;

    entry->dashboard =
        *ui_dashboard_layout_profile_for_theme(
            entry->summary.base_theme);
    entry->pages =
        *ui_page_layout_profile_for_theme(
            entry->summary.base_theme);

    const cJSON *page = NULL;

    page = cJSON_GetObjectItemCaseSensitive(layouts, "dashboard");
    if (!cJSON_IsObject(page) ||
        !parse_rect(page, "banner", &entry->dashboard.banner) ||
        !parse_rect(page, "active_print", &entry->dashboard.active_print) ||
        !parse_rect(page, "machine_status", &entry->dashboard.machine_status) ||
        !parse_rect(page, "command_bar", &entry->dashboard.command_bar)) {
        return false;
    }

    page = cJSON_GetObjectItemCaseSensitive(layouts, "drybox");
    if (!cJSON_IsObject(page) ||
        !parse_rect(page, "environment", &entry->pages.drybox.environment) ||
        !parse_rect(page, "drying_system", &entry->pages.drybox.drying_system) ||
        !parse_rect(page, "material_program", &entry->pages.drybox.material_program)) {
        return false;
    }

    page = cJSON_GetObjectItemCaseSensitive(layouts, "printer");
    if (!cJSON_IsObject(page) ||
        !parse_rect(page, "active", &entry->pages.printer.active) ||
        !parse_rect(page, "status", &entry->pages.printer.status) ||
        !parse_rect(page, "actions", &entry->pages.printer.actions)) {
        return false;
    }

    page = cJSON_GetObjectItemCaseSensitive(layouts, "files");
    if (!cJSON_IsObject(page) ||
        !parse_rect(page, "breadcrumb", &entry->pages.files.breadcrumb) ||
        !parse_rect(page, "up", &entry->pages.files.up) ||
        !parse_rect(page, "search", &entry->pages.files.search) ||
        !parse_rect(page, "sort", &entry->pages.files.sort) ||
        !parse_rect(page, "refresh", &entry->pages.files.refresh) ||
        !parse_rect(page, "list", &entry->pages.files.list)) {
        return false;
    }

    page = cJSON_GetObjectItemCaseSensitive(layouts, "network");
    if (!cJSON_IsObject(page) ||
        !parse_rect(page, "wifi", &entry->pages.network.wifi) ||
        !parse_rect(page, "moonraker", &entry->pages.network.moonraker) ||
        !parse_rect(page, "networks", &entry->pages.network.networks) ||
        !parse_rect(page, "actions", &entry->pages.network.actions)) {
        return false;
    }

    page = cJSON_GetObjectItemCaseSensitive(layouts, "settings");
    if (!cJSON_IsObject(page) ||
        !parse_rect(page, "banner", &entry->pages.settings.banner) ||
        !parse_rect(page, "content", &entry->pages.settings.content)) {
        return false;
    }

    page = cJSON_GetObjectItemCaseSensitive(layouts, "telemetry");
    ui_dashboard_rect_t metric;
    const char *metric_names[] = {
        "metric_1", "metric_2", "metric_3", "metric_4"
    };
    if (!cJSON_IsObject(page)) return false;
    for (int index = 0; index < 4; ++index) {
        if (!parse_rect(page, metric_names[index], &metric)) return false;
        entry->pages.telemetry.metric_x[index] = metric.x;
    }
    if (!parse_rect(page, "charts", &entry->pages.telemetry.charts)) {
        return false;
    }
    return true;
}

static bool parse_theme_file(
    const char *path,
    custom_theme_entry_t *entry)
{
    struct stat status;
    if (!path || !entry ||
        stat(path, &status) != 0 ||
        status.st_size <= 0 ||
        status.st_size > CUSTOM_THEME_MAX_BYTES) {
        return false;
    }

    FILE *file = fopen(path, "rb");
    if (!file) return false;

    char *buffer = heap_caps_malloc(
        (size_t)status.st_size + 1,
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buffer) {
        buffer = malloc((size_t)status.st_size + 1);
    }
    if (!buffer) {
        fclose(file);
        return false;
    }

    size_t read_count =
        fread(buffer, 1, (size_t)status.st_size, file);
    fclose(file);
    buffer[read_count] = 0;

    cJSON *root =
        cJSON_ParseWithLength(buffer, read_count);
    heap_caps_free(buffer);
    if (!root) return false;

    memset(entry, 0, sizeof(*entry));
    safe_copy(entry->path, sizeof(entry->path), path);

    const cJSON *schema =
        cJSON_GetObjectItemCaseSensitive(root, "schema");
    int schema_version = 0;
    bool base_valid = false;

    bool valid =
        cJSON_IsString(schema) &&
        strcmp(schema->valuestring, "printerhmi.theme") == 0 &&
        json_integer(root, "schema_version", 1, 1, &schema_version) &&
        json_string(root, "id",
                    entry->summary.id,
                    sizeof(entry->summary.id),
                    CUSTOM_THEME_ID_MAX, true) &&
        valid_id(entry->summary.id) &&
        json_string(root, "name",
                    entry->summary.name,
                    sizeof(entry->summary.name),
                    CUSTOM_THEME_NAME_MAX, true) &&
        json_string(root, "author",
                    entry->summary.author,
                    sizeof(entry->summary.author),
                    CUSTOM_THEME_AUTHOR_MAX, false) &&
        json_string(root, "description",
                    entry->summary.description,
                    sizeof(entry->summary.description),
                    CUSTOM_THEME_DESCRIPTION_MAX, false);

    entry->summary.base_theme =
        parse_base_theme(root, &base_valid);

    valid = valid &&
        base_valid &&
        parse_palette(root, entry) &&
        parse_metrics(root, entry) &&
        parse_layouts(root, entry);

    cJSON_Delete(root);
    return valid;
}

static bool duplicate_id(const char *id)
{
    for (size_t index = 0; index < s_theme_count; ++index) {
        if (strcmp(s_themes[index].summary.id, id) == 0) {
            return true;
        }
    }
    return false;
}

static custom_theme_entry_t *active_theme(void)
{
    if (!s_themes ||
        s_active_index < 0 ||
        (size_t)s_active_index >= s_theme_count) {
        return NULL;
    }
    return &s_themes[s_active_index];
}

size_t custom_theme_scan_sd(void)
{
    char active_id[CUSTOM_THEME_ID_MAX + 1] = "";
    if (custom_theme_is_active()) {
        safe_copy(active_id, sizeof(active_id),
                  custom_theme_active_id());
    }

    if (s_themes) {
        heap_caps_free(s_themes);
        s_themes = NULL;
    }
    s_theme_count = 0;
    s_active_index = -1;

    mkdir("/sdcard/PrinterHMI", 0775);
    mkdir(CUSTOM_THEME_DIRECTORY, 0775);

    DIR *directory = opendir(CUSTOM_THEME_DIRECTORY);
    if (!directory) {
        ESP_LOGW(TAG, "Theme directory unavailable: errno=%d", errno);
        return 0;
    }

    s_themes = heap_caps_calloc(
        CUSTOM_THEME_MAX_COUNT,
        sizeof(*s_themes),
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s_themes) {
        s_themes = calloc(
            CUSTOM_THEME_MAX_COUNT,
            sizeof(*s_themes));
    }
    if (!s_themes) {
        closedir(directory);
        ESP_LOGE(TAG, "Theme registry allocation failed");
        return 0;
    }

    struct dirent *item = NULL;
    while (s_theme_count < CUSTOM_THEME_MAX_COUNT &&
           (item = readdir(directory)) != NULL) {
        if (!ends_with_theme(item->d_name)) continue;

        char path[CUSTOM_THEME_PATH_MAX];
        int written = snprintf(
            path, sizeof(path),
            "%s/%s",
            CUSTOM_THEME_DIRECTORY,
            item->d_name);
        if (written <= 0 || written >= (int)sizeof(path)) continue;

        custom_theme_entry_t candidate;
        if (!parse_theme_file(path, &candidate)) {
            ESP_LOGW(TAG, "Rejected invalid theme: %s", item->d_name);
            continue;
        }
        if (duplicate_id(candidate.summary.id)) {
            ESP_LOGW(TAG, "Rejected duplicate theme id: %s",
                     candidate.summary.id);
            continue;
        }

        s_themes[s_theme_count++] = candidate;
        ESP_LOGI(TAG, "Theme discovered: %s (%s)",
                 candidate.summary.name,
                 candidate.summary.id);
    }
    closedir(directory);

    if (active_id[0]) {
        (void)custom_theme_activate_id(active_id);
    }

    ESP_LOGI(TAG, "Custom themes available: %u",
             (unsigned)s_theme_count);
    return s_theme_count;
}

size_t custom_theme_count(void)
{
    return s_theme_count;
}

const custom_theme_summary_t *custom_theme_summary(size_t index)
{
    if (!s_themes || index >= s_theme_count) return NULL;
    return &s_themes[index].summary;
}

bool custom_theme_activate(size_t index)
{
    if (!s_themes || index >= s_theme_count) return false;
    s_active_index = (int)index;
    ui_theme_set_active(s_themes[index].summary.base_theme);
    ESP_LOGI(TAG, "Custom theme active: %s",
             s_themes[index].summary.name);
    return true;
}

bool custom_theme_activate_id(const char *id)
{
    if (!id) return false;
    for (size_t index = 0; index < s_theme_count; ++index) {
        if (strcmp(s_themes[index].summary.id, id) == 0) {
            return custom_theme_activate(index);
        }
    }
    return false;
}

void custom_theme_deactivate(void)
{
    s_active_index = -1;
}

bool custom_theme_is_active(void)
{
    return active_theme() != NULL;
}

const char *custom_theme_active_id(void)
{
    custom_theme_entry_t *entry = active_theme();
    return entry ? entry->summary.id : "";
}

const char *custom_theme_active_name(void)
{
    custom_theme_entry_t *entry = active_theme();
    return entry ? entry->summary.name : "";
}

ui_theme_id_t custom_theme_base(void)
{
    custom_theme_entry_t *entry = active_theme();
    return entry ? entry->summary.base_theme : UI_THEME_OPERATOR;
}

bool custom_theme_remove(size_t index)
{
    if (!s_themes || index >= s_theme_count) return false;
    bool was_active = (int)index == s_active_index;
    char path[CUSTOM_THEME_PATH_MAX];
    safe_copy(path, sizeof(path), s_themes[index].path);

    if (unlink(path) != 0) {
        ESP_LOGE(TAG, "Could not remove %s: errno=%d", path, errno);
        return false;
    }

    if (was_active) custom_theme_deactivate();
    custom_theme_scan_sd();
    return true;
}

bool custom_theme_color_override(
    uint32_t operator_rgb,
    uint32_t foundry_rgb,
    uint32_t glass_rgb,
    uint32_t *rgb_out)
{
    custom_theme_entry_t *entry = active_theme();
    if (!entry || !rgb_out) return false;

    for (size_t index = 0;
         index < sizeof(s_color_routes) / sizeof(s_color_routes[0]);
         ++index) {
        const color_route_t *route = &s_color_routes[index];
        if (route->operator_rgb == operator_rgb &&
            route->foundry_rgb == foundry_rgb &&
            route->glass_rgb == glass_rgb &&
            (entry->color_mask & (UINT64_C(1) << route->id))) {
            *rgb_out = entry->colors[route->id];
            return true;
        }
    }
    return false;
}

bool custom_theme_accent_override(
    uint8_t tone,
    uint32_t *rgb_out)
{
    custom_theme_entry_t *entry = active_theme();
    if (!entry || !rgb_out) return false;
    custom_color_id_t id =
        tone >= 2 ? COLOR_ACCENT_BRIGHT : COLOR_ACCENT;
    if (!(entry->color_mask & (UINT64_C(1) << id))) return false;
    *rgb_out = entry->colors[id];
    return true;
}

bool custom_theme_metric_override(
    int32_t operator_value,
    int32_t foundry_value,
    int32_t glass_value,
    int32_t *value_out)
{
    custom_theme_entry_t *entry = active_theme();
    if (!entry || !value_out) return false;

    for (size_t index = 0;
         index < sizeof(s_metric_routes) / sizeof(s_metric_routes[0]);
         ++index) {
        const metric_route_t *route = &s_metric_routes[index];
        if (route->operator_value == operator_value &&
            route->foundry_value == foundry_value &&
            route->glass_value == glass_value &&
            (entry->metric_mask & (1U << route->id))) {
            *value_out = entry->metrics[route->id];
            return true;
        }
    }
    return false;
}

bool custom_theme_surface_opacity(uint8_t *opacity_out)
{
    custom_theme_entry_t *entry = active_theme();
    if (!entry || !entry->has_surface_opacity || !opacity_out) {
        return false;
    }
    *opacity_out = entry->surface_opacity;
    return true;
}

const ui_dashboard_layout_profile_t *
custom_theme_dashboard_profile(void)
{
    custom_theme_entry_t *entry = active_theme();
    return entry ? &entry->dashboard : NULL;
}

const ui_page_layout_profile_t *
custom_theme_page_profile(void)
{
    custom_theme_entry_t *entry = active_theme();
    return entry ? &entry->pages : NULL;
}
