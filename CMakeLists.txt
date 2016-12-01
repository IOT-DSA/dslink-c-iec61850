cmake_minimum_required(VERSION 3.0.0)
project(dslink-c-iec61850 C)

include("${CMAKE_CURRENT_LIST_DIR}/libiec61850/CMakeLists.txt")
include("${CMAKE_CURRENT_LIST_DIR}/dslink/CMakeLists.txt")
include_directories("include")

set(SRC "src/main.c" "src/add_server.c" "src/client_handler.c" "src/server_status.c" "src/iec61850_server_defs.c" "src/time_iso8601.c" "src/dataset_handler.c" "src/report_handler.c")

add_executable(dslink-c-iec61850 ${SRC})
add_dependencies(dslink-c-iec61850 sdk_dslink_c-static)
target_link_libraries(dslink-c-iec61850 sdk_dslink_c-static)
target_link_libraries(dslink-c-iec61850 iec61850)