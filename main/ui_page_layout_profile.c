#include "ui_page_layout_profile.h"

#include "custom_theme.h"
#include "ui_theme.h"

static const ui_page_layout_profile_t s_operator = {
    .drybox = {
        .subtitle = "Filament Conditioning",
        .environment = {20, 120, 385, 230},
        .drying_system = {425, 120, 395, 230},
        .material_program = {20, 365, 800, 145},
    },
    .printer = {
        .subtitle = "Machine Status and Print Control",
        .active = {20, 118, 800, 220},
        .status = {20, 350, 800, 94},
        .actions = {20, 456, 800, 54},
    },
    .files = {
        .subtitle = "Select a G-code file to preview and print.",
        .breadcrumb = {30, 62, 360, 22},
        .up = {390, 10, 80, 42},
        .search = {476, 10, 100, 42},
        .sort = {582, 10, 120, 42},
        .refresh = {708, 10, 110, 42},
        .list = {20, 86, 814, 422},
    },
    .network = {
        .subtitle = "Connectivity and remote access",
        .wifi = {16, 130, 402, 190},
        .moonraker = {436, 130, 402, 190},
        .networks = {16, 332, 474, 180},
        .actions = {508, 332, 330, 180},
    },
    .settings = {
        .subtitle = "System configuration",
        .banner = {20, 20, 800, 86},
        .content = {20, 118, 800, 398},
    },
    .telemetry = {
        .subtitle = "Live machine instrumentation",
        .metric_x = {20, 224, 428, 632},
        .charts = {20, 208, 800, 300},
    },
};

/*
 * Operator Shell keeps actions together at the bottom, giving Printer an
 * explicit live-status side column and a larger active-job workspace.
 */
static const ui_page_layout_profile_t s_operator_shell = {
    .drybox = {
        .subtitle = "Cell Material Conditioning",
        .environment = {20, 120, 385, 230},
        .drying_system = {425, 120, 395, 230},
        .material_program = {20, 365, 800, 145},
    },
    .printer = {
        .subtitle = "Live Machine Control",
        .active = {20, 118, 500, 278},
        .status = {532, 118, 288, 278},
        .actions = {20, 444, 800, 64},
    },
    .files = {
        .subtitle = "Cell Job Library",
        .breadcrumb = {30, 62, 360, 22},
        .up = {390, 10, 80, 42},
        .search = {476, 10, 100, 42},
        .sort = {582, 10, 120, 42},
        .refresh = {708, 10, 110, 42},
        .list = {20, 86, 814, 422},
    },
    .network = {
        .subtitle = "Cell Connectivity",
        .wifi = {16, 130, 402, 190},
        .moonraker = {436, 130, 402, 190},
        .networks = {16, 332, 474, 180},
        .actions = {508, 332, 330, 180},
    },
    .settings = {
        .subtitle = "Cell Interface and System Control",
        .banner = {20, 20, 800, 86},
        .content = {20, 118, 800, 398},
    },
    .telemetry = {
        .subtitle = "Live Cell Instrumentation",
        .metric_x = {20, 224, 428, 632},
        .charts = {20, 208, 800, 300},
    },
};

static const ui_page_layout_profile_t s_foundry = {
    .drybox = {
        .subtitle = "Material Workshop",
        .environment = {435, 120, 385, 230},
        .drying_system = {20, 120, 395, 230},
        .material_program = {20, 365, 800, 145},
    },
    .printer = {
        .subtitle = "Workshop Machine Control",
        .active = {20, 118, 800, 220},
        .status = {20, 416, 800, 94},
        .actions = {20, 350, 800, 54},
    },
    .files = {
        .subtitle = "Workshop Job Library",
        .breadcrumb = {30, 62, 360, 22},
        .up = {738, 10, 80, 42},
        .search = {632, 10, 100, 42},
        .sort = {506, 10, 120, 42},
        .refresh = {390, 10, 110, 42},
        .list = {20, 86, 814, 422},
    },
    .network = {
        .subtitle = "Cell Connections",
        .wifi = {436, 130, 402, 190},
        .moonraker = {16, 130, 402, 190},
        .networks = {364, 332, 474, 180},
        .actions = {16, 332, 330, 180},
    },
    .settings = {
        .subtitle = "Workshop Configuration",
        .banner = {20, 20, 800, 86},
        .content = {20, 118, 800, 398},
    },
    .telemetry = {
        .subtitle = "Process Measurements",
        .metric_x = {632, 428, 224, 20},
        .charts = {20, 198, 800, 310},
    },
};

static const ui_page_layout_profile_t s_glass = {
    .drybox = {
        .subtitle = "Environmental Process Control",
        .environment = {20, 120, 385, 230},
        .drying_system = {425, 120, 395, 230},
        .material_program = {20, 365, 800, 145},
    },
    .printer = {
        .subtitle = "Live Machine Console",
        .active = {20, 118, 800, 220},
        .status = {20, 416, 800, 94},
        .actions = {20, 350, 800, 54},
    },
    .files = {
        .subtitle = "Visual Print Queue",
        .breadcrumb = {30, 62, 360, 22},
        .up = {738, 10, 80, 42},
        .search = {632, 10, 100, 42},
        .sort = {506, 10, 120, 42},
        .refresh = {390, 10, 110, 42},
        .list = {20, 86, 814, 422},
    },
    .network = {
        .subtitle = "Linked Systems",
        .wifi = {16, 130, 402, 190},
        .moonraker = {436, 130, 402, 190},
        .networks = {364, 332, 474, 180},
        .actions = {16, 332, 330, 180},
    },
    .settings = {
        .subtitle = "Interface and System Control",
        .banner = {20, 20, 800, 86},
        .content = {20, 118, 800, 398},
    },
    .telemetry = {
        .subtitle = "Live Instrument Matrix",
        .metric_x = {20, 428, 224, 632},
        .charts = {20, 198, 800, 310},
    },
};

const ui_page_layout_profile_t *
ui_page_layout_profile_for_theme(ui_theme_id_t theme)
{
    switch (theme) {
        case UI_THEME_CLASSIC:
            return &s_foundry;
        case UI_THEME_GLASS:
            return &s_glass;
        case UI_THEME_OPERATOR_SHELL:
            return &s_operator_shell;
        case UI_THEME_OPERATOR:
        default:
            return &s_operator;
    }
}

const ui_page_layout_profile_t *ui_page_layout_profile_current(void)
{
    const ui_page_layout_profile_t *custom =
        custom_theme_page_profile();
    return custom
        ? custom
        : ui_page_layout_profile_for_theme(
            ui_theme_get_active());
}
