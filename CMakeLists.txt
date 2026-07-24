# ESP-IDF project file for PrinterHMI v4.0.0
cmake_minimum_required(VERSION 3.5)

# Must be set BEFORE project.cmake is included so IDF sees the local BSP extras.
set(EXTRA_COMPONENT_DIRS
    ${CMAKE_CURRENT_LIST_DIR}/common_components
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

add_compile_options(-Wformat=2)
set(PROJECT_VER "4.0.0")
# PRINTERHMI_RELEASE_VERSION_4_0_0
project(PrinterHMI)
