from pathlib import Path

header_path = Path("main/ui_printer_live_status.h")
source_path = Path("main/ui_printer_live_status.c")
main_path = Path("main/main.c")

header = header_path.read_text()
source = source_path.read_text()
main = main_path.read_text()

old_header_signature = """    lv_obj_t **flow_label,
    lv_obj_t **layer_label,
    ui_printer_live_status_send_gcode_cb_t send_gcode_cb);
"""

new_header_signature = """    lv_obj_t **flow_label,
    lv_obj_t **layer_label,
    double initial_speed_factor,
    double initial_flow_factor,
    ui_printer_live_status_send_gcode_cb_t send_gcode_cb);
"""

old_source_signature = """    lv_obj_t **flow_label,
    lv_obj_t **layer_label,
    ui_printer_live_status_send_gcode_cb_t send_gcode_cb)
"""

new_source_signature = """    lv_obj_t **flow_label,
    lv_obj_t **layer_label,
    double initial_speed_factor,
    double initial_flow_factor,
    ui_printer_live_status_send_gcode_cb_t send_gcode_cb)
"""

old_reset = """    s_send_gcode_cb = send_gcode_cb;
    s_pending_speed_factor = -1;
    s_pending_flow_factor = -1;
"""

new_reset = """    /*
     * Pending values deliberately survive page destruction/recreation.
     * Moonraker remains authoritative and clears them when its reported
     * factor matches the requested value.
     */
    s_send_gcode_cb = send_gcode_cb;
"""

old_create_end = """    s_flow_factor_slider =
        create_tuning_slider(
            card,
            575,
            174,
            UI_OK,
            PRINTER_TUNING_FLOW);
}
"""

new_create_end = """    s_flow_factor_slider =
        create_tuning_slider(
            card,
            575,
            174,
            UI_OK,
            PRINTER_TUNING_FLOW);

    /*
     * Do not flash/reset to 100% when returning to the Printer page.
     * Initialize immediately from the latest Moonraker state or from a
     * command that is still awaiting confirmation.
     */
    refresh_factor_slider(
        s_speed_factor_slider,
        s_speed_factor_value,
        initial_speed_factor,
        &s_pending_speed_factor);

    refresh_factor_slider(
        s_flow_factor_slider,
        s_flow_factor_value,
        initial_flow_factor,
        &s_pending_flow_factor);
}
"""

old_main_call = """        &printer_speed_label,
        &printer_flow_label,
        moonraker_send_gcode);
"""

new_main_call = """        &printer_speed_label,
        &printer_flow_label,
        printer_speed_factor,
        printer_flow_factor,
        moonraker_send_gcode);
"""

checks = [
    (header, old_header_signature, "header create signature"),
    (source, old_source_signature, "source create signature"),
    (source, old_reset, "pending-value reset"),
    (source, old_create_end, "slider creation ending"),
    (main, old_main_call, "Printer-page creation call"),
]

for text, anchor, description in checks:
    count = text.count(anchor)
    if count != 1:
        raise RuntimeError(
            f"expected one {description}, found {count}")

header = header.replace(
    old_header_signature,
    new_header_signature,
    1)

source = source.replace(
    old_source_signature,
    new_source_signature,
    1)

source = source.replace(
    old_reset,
    new_reset,
    1)

source = source.replace(
    old_create_end,
    new_create_end,
    1)

main = main.replace(
    old_main_call,
    new_main_call,
    1)

header_path.write_text(header)
source_path.write_text(source)
main_path.write_text(main)

print("Printer sliders now restore across page navigation.")
print("Klipper/Moonraker remains the authoritative owner.")
