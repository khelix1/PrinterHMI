idf_component_register(
    SRCS
        "dashboard_runtime_controller.c"
        "demo_mode_controller.c"
        # Application entry point
        "main.c"

        # Theme and shared UI components
        "ui_theme.c"
        "ui_font_fallback.c"
        "ui_text.c"
        "ui_theme_a.c"
        "ui_theme_b.c"
        "ui_theme_c.c"
        "theme_manager.c"
        "custom_theme.c"
        "ui_theme_preview.c"
        "ui_appearance_popups.c"
        "ui_widgets.c"
        "ui_button.c"
        "ui_popup.c"
        "onboarding_controller.c"
        "ui_setup_wizard.c"
        "ui_cards.c"
        "ui_page_title.c"
        "ui_page_state.c"
        "ui_toast.c"
        "operator_event_log.c"
        "ui_event_history_popup.c"
        "ui_about_popup.c"
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
        "ui_calibration_pid.c"
        "ui_calibration_results.c"

        # Shell and splash
        "ui_shell.c"
        "ui_global_estop.c"
        "ui_devices.c"
        "ui_calibration.c"
        "ui_calibration_layout.c"
        "ui_calibration_geometry.c"
        "ui_calibration_custom.c"
        "ui_console.c"
        "ui_macros.c"
        "ui_splash.c"
        "ui_logo_assets.c"

        # Camera
        "ui_camera.c"
        "camera_stream_controller.c"
        "camera_test_controller.c"
        "camera_catalog_controller.c"
        "camera_jpeg_decoder.c"

        # Tools
        "ui_tools.c"

        # Dashboard
        "ui_dashboard.c"
        "dashboard_live_controller.c"
        "ui_dashboard_layout_profile.c"
        "ui_page_layout_profile.c"
        "ui_dashboard_page.c"
        "ui_dashboard_status.c"
        "ui_status_banner.c"
        "ui_active_print.c"
        "ui_machine_status.c"
        "ui_command_bar.c"

        # Printer
        "ui_printer.c"
        "ui_printer_layout.c"
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
        "ui_bed_mesh.c"

        # Files
        "ui_files.c"
        "files_page_controller.c"
        "files_row_preview.c"
        "file_detail_loader.c"

        # Thumbnails
        "thumbnail_manager.c"
        "thumbnail_render.c"
        "thumbnail_session.c"
        "thumbnail_preview_coordinator.c"
        "ui_thumbnail.c"
        "ui_preview_lightbox.c"

        # Network and Moonraker
        "ui_network.c"
        "ui_network_tools.c"
        "ui_printer_profiles.c"
        "ui_printer_chooser.c"
        "printer_profile_health.c"
        "printer_preview_cache.c"
        "printer_profile_preview_worker.c"
        "printer_preview_store.c"
        "network_activity_controller.c"
        "network_status_controller.c"
        "network_wifi_scan.c"
        "wifi_credentials_store.c"
        "moonraker.c"
        "moonraker_config_controller.c"
        "moonraker_tls_trust_store.c"
        "moonraker_transport_security_controller.c"
        "moonraker_discovery.c"
        "moonraker_probe.c"
        "moonraker_endpoint_test.c"
        "camera_discovery_controller.c"
        "moonraker_poll.c"
        "moonraker_live_transport.c"
        "moonraker_live_websocket.c"

        # Drybox
        "ui_drybox.c"
        "ui_drybox_page.c"

        # Telemetry
        "telemetry_history.c"
        "ui_telemetry_components.c"
        "ui_telemetry_charts.c"
        "ui_telemetry.c"

        # Settings and OTA
        "ui_settings.c"
        "ui_settings_components.c"
        "settings_system_info.c"
        "settings_backup.c"
        "settings_backup_crypto.c"
        "settings_backup_encrypted_worker.c"
        "ui_settings_backup_popup.c"
        "timezone_config.c"
        "ui_settings_popups.c"
        "ui_ota_popup.c"
        "ui_ota_release_browser.c"
        "ota_manager.c"
        "ota_ui_controller.c"
        "ota_boot_validation.c"
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
        esp_driver_jpeg
        espressif__cjson
        espressif__esp_websocket_client
        espressif__esp_wifi_remote

    PRIV_REQUIRES
        esp_https_ota
        mbedtls
        fatfs
        sdmmc
        esp_driver_sdmmc
        espressif__esp_lcd_jd9165
    PRIV_REQUIRES mbedtls
)
