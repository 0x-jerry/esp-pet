cmake_minimum_required(VERSION 3.22)

set(BLUEPAD32_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/external/bluepad32)
set(BTSTACK_ROOT ${BLUEPAD32_ROOT}/external/btstack)
set(EXTRA_COMPONENT_DIRS ${BLUEPAD32_ROOT}/src/components)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp-pet)
