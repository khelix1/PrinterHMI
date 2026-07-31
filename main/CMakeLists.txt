idf_component_register(
    SRCS
        "dashboard_runtime_controller.c"
        # Application entry point
        "main.c"

        # Theme and shared UI components
        "ui_theme.c"
        "ui_theme_a.c"
        "ui_theme_b.c"
        "ui_theme_c.c"
        "theme_manager.c"
        "custom_theme.c"
        "ui_theme_preview.c"
        "ui_theme_lab.c"
        "ui_appearance_popups.c"
        "ui_widgets.c"
        "ui_button.c"
        "ui_popup.c"
        "ui_cards.c"
        "ui_page_title.c"
        "ui_page_state_v32.c"
        "ui_toast_v32.c"
        "operator_event_log.c"
        "ui_event_history_popup.c"
        "console_controller.c"
        "macro_controller.c"
        "printer_action_resolver.c"
        "device_catalog_controller.c"
        "ui_devices_live_values.c"
        "ui_devices_catalog_view.c"
        "calibration_capability_controller.c"
        "calibration_session_controller.c"
        "ui_calibration_motion.c"
        "ui_calibration_manual_probe.c"
        "ui_calibration_pressure_advance.c"

        # Shell and splash
        "ui_shell.c"
        "ui_devices_v32.c"
        "ui_calibration_v32.c"
        "ui_console_v32.c"
        "ui_macros_v32.c"
        "ui_splash_v32.c"
        "ui_logo_assets.c"

        # Dashboard
        "ui_dashboard_v32.c"
        "dashboard_live_controller.c"
        "ui_dashboard_layout_profile.c"
        "ui_page_layout_profile.c"
        "ui_dashboard_page_v32.c"
        "ui_dashboard_status_v32.c"
        "ui_status_banner_v32.c"
        "ui_active_print_v32.c"
        "ui_machine_status_v32.c"
        "ui_command_bar_v32.c"

        # Printer
        "ui_printer_v32.c"
        "ui_printer_layout_v32.c"
        "ui_printer_motion.c"
        "ui_printer_toolhead.c"
        "ui_printer_live_status.c"
        "ui_printer_info_cards.c"
        "ui_printer_actions.c"
        "ui_printer_banner.c"
        "ui_printer_popups.c"
        "printer_controller.c"
        "printer_layer_resolver.c"
        "printer_ui_controller.c"
        "printer_file_controller.c"
        "printer_files.c"
        "bed_mesh_controller.c"
        "ui_bed_mesh_gestures.c"
        "ui_bed_mesh_renderer.c"
        "ui_bed_mesh_profiles.c"
        "ui_bed_mesh_v32.c"

        # Files
        "ui_files_v32.c"
        "files_page_controller.c"
        "files_row_preview_v32.c"
        "file_detail_loader_v32.c"

        # Thumbnails
        "thumbnail_manager_v32.c"
        "thumbnail_render_v32.c"
        "thumbnail_session_v32.c"
        "thumbnail_preview_coordinator_v32.c"
        "ui_thumbnail_v32.c"
        "ui_preview_lightbox_v32.c"

        # Network and Moonraker
        "ui_network_v32.c"
        "ui_network_tools.c"
        "ui_printer_profiles.c"
        "ui_printer_chooser_v32.c"
        "printer_profile_health.c"
        "printer_preview_cache_v32.c"
        "printer_profile_preview_worker_v32.c"
        "printer_preview_store_v32.c"
        "network_activity_controller.c"
        "network_status_controller.c"
        "network_wifi_scan.c"
        "moonraker.c"
        "moonraker_config_controller.c"
        "moonraker_discovery.c"
        "moonraker_probe.c"
        "moonraker_poll.c"
        "moonraker_live_transport.c"
        "moonraker_live_websocket.c"

        # Drybox
        "ui_drybox_v32.c"
        "ui_drybox_page_v32.c"

        # Telemetry
        "telemetry_history.c"
        "ui_telemetry_components.c"
        "ui_telemetry_charts.c"
        "ui_telemetry_v32.c"

        # Settings and OTA
        "ui_settings.c"
        "ui_settings_components.c"
        "settings_system_info.c"
        "settings_backup.c"
        "ui_settings_backup_popup.c"
        "timezone_config.c"
        "ui_settings_popups.c"
        "ui_ota_popup.c"
        "ui_ota_release_browser.c"
        "ota_manager.c"
        "ota_release_catalog.c"

    INCLUDE_DIRS
        "."

    REQUIRES
        lvgl__lvgl
        bsp_extra
        espressif__esp32_p4_function_ev_board
        nvs_flash
        app_update
        esp_wifi
        esp_event
        esp_netif
        esp_http_client
        json
        espressif__esp_websocket_client
        espressif__esp_wifi_remote

    PRIV_REQUIRES
        esp_https_ota
        mbedtls
        fatfs
        sdmmc
        espressif__esp_lcd_jd9165
)
