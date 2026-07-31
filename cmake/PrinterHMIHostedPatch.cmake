# PrinterHMI reproducible ESP-Hosted SDIO compatibility patch.
#
# ESP-Hosted 2.12.8 defaults host TX to block-only transfers. On the target
# ESP32-P4/C6 hardware that path can fail with ESP_ERR_TIMEOUT (0x107) and an
# unrecoverable host SDIO restart. Keep RX in block mode, but use byte mode for
# host TX. This is intentionally applied after project() resolves managed
# components so clean checkouts and regenerated dependencies receive the same
# tested configuration.

set(_printerhmi_hosted_manifest
    "${CMAKE_CURRENT_LIST_DIR}/../main/idf_component.yml"
)
set(_printerhmi_hosted_header
    "${CMAKE_CURRENT_LIST_DIR}/../managed_components/espressif__esp_hosted/host/port/esp/freertos/include/port_esp_hosted_host_config.h"
)

if(NOT EXISTS "${_printerhmi_hosted_manifest}")
    message(FATAL_ERROR "PrinterHMI ESP-Hosted manifest is missing")
endif()

file(READ "${_printerhmi_hosted_manifest}" _printerhmi_hosted_manifest_text)
if(NOT _printerhmi_hosted_manifest_text MATCHES
   "espressif/esp_hosted:[ \t]*\\\"2\\.12\\.8\\\"")
    message(FATAL_ERROR
        "PrinterHMI SDIO workaround is validated only with ESP-Hosted 2.12.8"
    )
endif()

if(NOT EXISTS "${_printerhmi_hosted_header}")
    message(FATAL_ERROR
        "ESP-Hosted configuration header is missing after dependency resolution"
    )
endif()

file(READ "${_printerhmi_hosted_header}" _printerhmi_hosted_text)
string(REGEX MATCHALL
    "#define[ \t]+H_SDIO_TX_BLOCK_ONLY_XFER[ \t]+\\([01]\\)"
    _printerhmi_tx_definitions
    "${_printerhmi_hosted_text}"
)
string(REGEX MATCHALL
    "#define[ \t]+H_SDIO_RX_BLOCK_ONLY_XFER[ \t]+\\([01]\\)"
    _printerhmi_rx_definitions
    "${_printerhmi_hosted_text}"
)
list(LENGTH _printerhmi_tx_definitions _printerhmi_tx_count)
list(LENGTH _printerhmi_rx_definitions _printerhmi_rx_count)

if(NOT _printerhmi_tx_count EQUAL 1 OR NOT _printerhmi_rx_count EQUAL 1)
    message(FATAL_ERROR
        "Unexpected ESP-Hosted SDIO transfer-mode definitions"
    )
endif()

string(REGEX REPLACE
    "(#define[ \t]+H_SDIO_TX_BLOCK_ONLY_XFER[ \t]+)\\([01]\\)"
    "\\1(0)"
    _printerhmi_hosted_patched
    "${_printerhmi_hosted_text}"
)
string(REGEX REPLACE
    "(#define[ \t]+H_SDIO_RX_BLOCK_ONLY_XFER[ \t]+)\\([01]\\)"
    "\\1(1)"
    _printerhmi_hosted_patched
    "${_printerhmi_hosted_patched}"
)

if(NOT _printerhmi_hosted_patched STREQUAL _printerhmi_hosted_text)
    file(WRITE "${_printerhmi_hosted_header}" "${_printerhmi_hosted_patched}")
endif()

message(STATUS
    "PrinterHMI ESP-Hosted 2.12.8 SDIO mode: TX byte, RX block"
)

unset(_printerhmi_hosted_manifest)
unset(_printerhmi_hosted_header)
unset(_printerhmi_hosted_manifest_text)
unset(_printerhmi_hosted_text)
unset(_printerhmi_hosted_patched)
unset(_printerhmi_tx_definitions)
unset(_printerhmi_rx_definitions)
unset(_printerhmi_tx_count)
unset(_printerhmi_rx_count)
