#include "dashboard_live_controller.h"

#include <stdio.h>
#include <string.h>

#include "moonraker.h"
#include "printer_controller.h"
#include "ui_dashboard.h"

void dashboard_live_controller_push_banner(bool moonraker_ok)
{
    moonraker_state_t state_snapshot;
    moonraker_state_snapshot(&state_snapshot);

    char state[48];
    char file[160];
    char progress[24];
    char eta[48];
    char eta_clock[32];

    printer_controller_format_status_symbol_text(
        state,
        sizeof(state),
        state_snapshot.printer_state,
        moonraker_ok,
        state_snapshot.live_data_ok);

    if (state_snapshot.printer_file[0] &&
        strcmp(state_snapshot.printer_file, "No file") != 0) {
        snprintf(
            file,
            sizeof(file),
            "%.*s",
            (int)sizeof(file) - 1,
            state_snapshot.printer_file);
    } else {
        snprintf(
            file,
            sizeof(file),
            "No active print");
    }

    if (state_snapshot.progress >= 0.0) {
        snprintf(
            progress,
            sizeof(progress),
            "%.0f%%",
            state_snapshot.progress * 100.0);
    } else {
        snprintf(
            progress,
            sizeof(progress),
            "--%%");
    }

    printer_controller_format_eta_clock(
        eta_clock,
        sizeof(eta_clock),
        state_snapshot.progress,
        state_snapshot.print_duration);

    snprintf(
        eta,
        sizeof(eta),
        "ETA %s",
        eta_clock);

    ui_dashboard_set_banner(
        state,
        file,
        eta,
        progress);
}

void dashboard_live_controller_push_machine(void)
{
    moonraker_state_t state_snapshot;
    moonraker_state_snapshot(&state_snapshot);

    char nozzle[32];
    char hotend_name[32];
    char bed[32];
    char chamber[32];
    char humidity[32];
    char speed[24];
    char flow[24];
    char fan[24];

    if (state_snapshot.nozzle_temp > -100.0) {
        snprintf(
            nozzle,
            sizeof(nozzle),
            "%.1f / %.1f C",
            state_snapshot.nozzle_temp,
            state_snapshot.nozzle_target);
    } else {
        snprintf(
            nozzle,
            sizeof(nozzle),
            "-- / -- C");
    }

    snprintf(
        hotend_name,
        sizeof(hotend_name),
        "NOZZLE");

    if (state_snapshot.hotend_count > 1) {
        for (size_t i = 0;
             i < state_snapshot.hotend_count;
             ++i) {
            if (!state_snapshot.hotends[i].active) {
                continue;
            }

            snprintf(
                hotend_name,
                sizeof(hotend_name),
                "T%u ACTIVE",
                (unsigned)i);
            break;
        }
    }

    if (state_snapshot.capabilities.discovered &&
        !state_snapshot.capabilities.has_heated_bed) {
        snprintf(bed, sizeof(bed), "N/A");
    } else if (state_snapshot.bed_temp > -100.0) {
        snprintf(
            bed,
            sizeof(bed),
            "%.1f / %.1f C",
            state_snapshot.bed_temp,
            state_snapshot.bed_target);
    } else {
        snprintf(
            bed,
            sizeof(bed),
            "-- / -- C");
    }

    /*
     * The Dashboard chamber field represents the drybox environmental air
     * temperature rather than the center probe.
     */
    if (state_snapshot.capabilities.discovered &&
        !state_snapshot.capabilities.has_drybox_environment_sensor) {
        snprintf(chamber, sizeof(chamber), "N/A");
        snprintf(humidity, sizeof(humidity), "N/A");
    } else {
        if (state_snapshot.air_temp > -100.0) {
            snprintf(
                chamber,
                sizeof(chamber),
                "%.1f C",
                state_snapshot.air_temp);
        } else {
            snprintf(
                chamber,
                sizeof(chamber),
                "-- C");
        }

        if (state_snapshot.humidity > -100.0) {
            snprintf(
                humidity,
                sizeof(humidity),
                "%.1f %%RH",
                state_snapshot.humidity);
        } else {
            snprintf(
                humidity,
                sizeof(humidity),
                "-- %%RH");
        }
    }

    /*
     * Machine Status reports actual live process values. M220/M221 tuning
     * factors remain editable on the Printer page.
     */
    if (state_snapshot.live_velocity >= 0.0) {
        snprintf(
            speed,
            sizeof(speed),
            "%.0f mm/s",
            state_snapshot.live_velocity);
    } else {
        snprintf(
            speed,
            sizeof(speed),
            "-- mm/s");
    }

    if (state_snapshot.live_flow >= 0.0) {
        snprintf(
            flow,
            sizeof(flow),
            "%.1f mm3/s",
            state_snapshot.live_flow);
    } else {
        snprintf(
            flow,
            sizeof(flow),
            "-- mm3/s");
    }

    if (state_snapshot.capabilities.discovered &&
        !state_snapshot.capabilities.has_part_fan) {
        snprintf(fan, sizeof(fan), "N/A");
    } else if (state_snapshot.part_fan_speed >= 0.0) {
        snprintf(
            fan,
            sizeof(fan),
            "%.0f%%",
            state_snapshot.part_fan_speed);
    } else {
        snprintf(
            fan,
            sizeof(fan),
            "--%%");
    }

    ui_dashboard_set_machine(
        nozzle,
        bed,
        chamber,
        humidity,
        speed,
        flow,
        fan);

    ui_dashboard_set_active_hotend(
        hotend_name,
        nozzle);

    ui_dashboard_set_machine_connection(
        state_snapshot.moonraker_ok &&
        state_snapshot.live_data_ok);

    moonraker_filament_state_t filament_state;
    moonraker_filament_state_snapshot(
        &filament_state);

    ui_dashboard_set_filament(
        state_snapshot.moonraker_ok &&
            state_snapshot.live_data_ok,
        &filament_state);
}
