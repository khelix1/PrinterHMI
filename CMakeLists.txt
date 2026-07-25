# ESP-IDF project file for PrinterHMI
# PRINTERHMI_VERSION_IDENTITY_V1
cmake_minimum_required(VERSION 3.5)

file(STRINGS
    "${CMAKE_CURRENT_LIST_DIR}/version.txt"
    PRINTERHMI_VERSION
    LIMIT_COUNT 1
)
string(STRIP
    "${PRINTERHMI_VERSION}"
    PRINTERHMI_VERSION
)

if(NOT PRINTERHMI_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR
        "version.txt must contain one semantic X.Y.Z version"
    )
endif()

set(PROJECT_VER "${PRINTERHMI_VERSION}")

# Must be set BEFORE project.cmake is included so IDF sees the local BSP extras.
set(EXTRA_COMPONENT_DIRS
    ${CMAKE_CURRENT_LIST_DIR}/common_components
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

add_compile_options(-Wformat=2)
project(PrinterHMI)
