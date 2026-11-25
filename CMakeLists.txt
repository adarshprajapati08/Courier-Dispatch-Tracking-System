cmake_minimum_required(VERSION 3.16)
project(CourierDispatchTrackingSystem VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(COURIER_ENABLE_WARNINGS "Enable compiler warnings" ON)

find_package(SQLite3 REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

# CivetWeb and cJSON package names vary across Linux distributions and Windows.
# Prefer normal header/library discovery, then use pkg-config as a fallback.
find_path(CIVETWEB_INCLUDE_DIR NAMES civetweb.h)
find_library(CIVETWEB_LIBRARY NAMES civetweb libcivetweb)
find_path(CJSON_INCLUDE_DIR NAMES cjson/cJSON.h cJSON.h)
find_library(CJSON_LIBRARY NAMES cjson libcjson)

if((NOT CIVETWEB_INCLUDE_DIR OR NOT CIVETWEB_LIBRARY) AND NOT WIN32)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(CIVETWEB REQUIRED civetweb)
endif()
if((NOT CJSON_INCLUDE_DIR OR NOT CJSON_LIBRARY) AND NOT WIN32)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(CJSON REQUIRED libcjson)
endif()

if(NOT CIVETWEB_LIBRARY)
    message(FATAL_ERROR "CivetWeb development library was not found. Install CivetWeb or provide CIVETWEB_LIBRARY and CIVETWEB_INCLUDE_DIR.")
endif()
if(NOT CJSON_LIBRARY)
    message(FATAL_ERROR "cJSON development library was not found. Install cJSON or provide CJSON_LIBRARY and CJSON_INCLUDE_DIR.")
endif()

add_executable(courier_server
    backend/src/main.c
    backend/src/server.c
    backend/src/database.c
    backend/src/auth.c
    backend/src/shipment.c
    backend/src/tracking.c
    backend/src/user.c
    backend/src/admin.c
    backend/src/staff.c
    backend/src/validation.c
    backend/src/utils.c
)

target_include_directories(courier_server PRIVATE
    backend/include
    ${CIVETWEB_INCLUDE_DIR}
    ${CJSON_INCLUDE_DIR}
)
target_link_libraries(courier_server PRIVATE
    ${CIVETWEB_LIBRARY}
    ${CJSON_LIBRARY}
    SQLite::SQLite3
    OpenSSL::Crypto
    Threads::Threads
)

if(NOT WIN32)
    target_compile_definitions(courier_server PRIVATE _POSIX_C_SOURCE=200809L)
endif()

if(COURIER_ENABLE_WARNINGS)
    if(MSVC)
        target_compile_options(courier_server PRIVATE /W4)
    else()
        target_compile_options(courier_server PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Wformat=2)
    endif()
endif()

set_target_properties(courier_server PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)
