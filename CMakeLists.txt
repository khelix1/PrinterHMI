# ESP-IDF project file for PrinterHMI
# PRINTERHMI_VERSION_IDENTITY_V1
cmake_minimum_required(VERSION 3.5)

set_property(
    DIRECTORY APPEND
    PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/version.txt"
)

file(STRINGS
    "${CMAKE_CURRENT_LIST_DIR}/version.txt"
    PRINTERHMI_VERSION
    LIMIT_COUNT 1
)
string(STRIP
    "${PRINTERHMI_VERSION}"
    PRINTERHMI_VERSION
)

set(PRINTERHMI_STABLE_VERSION_PATTERN
    "^[0-9]+\\.[0-9]+\\.[0-9]+$"
)
set(PRINTERHMI_NIGHTLY_VERSION_PATTERN
    "^nightly-[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]-[0-9a-f]+$"
)

if(NOT PRINTERHMI_VERSION MATCHES "${PRINTERHMI_STABLE_VERSION_PATTERN}" AND
   NOT PRINTERHMI_VERSION MATCHES "${PRINTERHMI_NIGHTLY_VERSION_PATTERN}")
    message(FATAL_ERROR
        "version.txt must contain X.Y.Z or a generated nightly identity"
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
