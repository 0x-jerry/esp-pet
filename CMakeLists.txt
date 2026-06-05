cmake_minimum_required(VERSION 3.22)

set(BTSTACK_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/external/btstack)
set(EXTRA_COMPONENT_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/components)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp-pet)
