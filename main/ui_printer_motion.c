#include "ui_printer_motion.h"
#include "ui_printer_toolhead.h"

void ui_printer_motion_init(void)
{
    ui_printer_toolhead_init();
}

bool ui_printer_motion_format_jog_command(const char *axis,
                                          double jog_step,
                                          char *cmd,
                                          size_t cmd_size)
{
    return ui_printer_toolhead_format_jog_command(
        axis,
        jog_step,
        cmd,
        cmd_size);
}

bool ui_printer_motion_format_extrude_command(const char *dir,
                                              char *cmd,
                                              size_t cmd_size)
{
    return ui_printer_toolhead_format_extrude_command(
        dir,
        cmd,
        cmd_size);
}

bool ui_printer_motion_format_z_offset_command(const char *adjustment,
                                               char *cmd,
                                               size_t cmd_size)
{
    return ui_printer_toolhead_format_z_offset_command(
        adjustment,
        cmd,
        cmd_size);
}

void ui_printer_motion_show(lv_obj_t **step1_btn,
                            lv_obj_t **step10_btn,
                            lv_obj_t **step50_btn,
                            double *jog_step,
                            ui_printer_motion_send_gcode_cb_t send_gcode_cb)
{
    ui_printer_toolhead_show(
        step1_btn,
        step10_btn,
        step50_btn,
        jog_step,
        (ui_printer_toolhead_send_gcode_cb_t)send_gcode_cb);
}
