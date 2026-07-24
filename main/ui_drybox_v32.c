#include "ui_drybox_v32.h"

/* Temporary bridge while Drybox page implementation still lives in main.c. */
void legacy_create_drybox_tab(void);
void legacy_cleanup_drybox_tab(void);
void legacy_refresh_drybox_tab(void);

void ui_drybox_v32_show(void)
{
    legacy_create_drybox_tab();
}

void ui_drybox_v32_hide(void)
{
    legacy_cleanup_drybox_tab();
}

void ui_drybox_v32_refresh(void)
{
    legacy_refresh_drybox_tab();
}
