cmake_minimum_required(VERSION 3.28)

project(gerbv
        VERSION 4.0
        DESCRIPTION "Gerber Viewer"
        LANGUAGES C CXX)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
set(CMAKE_EXPORT_COMPILE_COMMANDS "ON")

## Glib-2.0 and GTK-2.0
find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK REQUIRED IMPORTED_TARGET gtk+-2.0)
pkg_check_modules(GLIB REQUIRED IMPORTED_TARGET glib-2.0>=2.6)

## Gettext and intl
find_package(Gettext REQUIRED) # https://cmake.org/cmake/help/latest/module/FindGettext.html
find_package(Intl) # Removed REQUIRED since it will fail on Mingw where it seems to be part of Gettext

include(GNUInstallDirs)
set(DATADIR "${CMAKE_INSTALL_FULL_DATADIR}/${CMAKE_PROJECT_NAME}")
set(LOCALEDIR "${CMAKE_INSTALL_FULL_LOCALEDIR}")
set(SCMSUBDIR "scheme")
set(BACKEND_DIR "${DATADIR}/${SCMSUBDIR}")

add_library(compilation-flags INTERFACE)
target_compile_options(compilation-flags INTERFACE -Wall -Wimplicit-fallthrough -O3 -Wno-deprecated-declarations)
target_compile_options(compilation-flags INTERFACE $<$<CONFIG:Debug>:-g3>)
target_link_libraries(compilation-flags INTERFACE PkgConfig::GLIB PkgConfig::GTK m)
if(Intl_FOUND)
    target_link_libraries(compilation-flags INTERFACE Intl::Intl)
endif()
if(APPLE)
    target_compile_definitions(compilation-flags INTERFACE QUARTZ)
endif()
if(WIN32)
    target_compile_definitions(compilation-flags INTERFACE WIN32)
endif()
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_definitions(compilation-flags INTERFACE _GNU_SOURCE)
endif()

## This is installation directory settings. These must be solved later by install target
target_compile_definitions(compilation-flags INTERFACE "GERBV_DATADIR=\"${DATADIR}\"")
target_compile_definitions(compilation-flags INTERFACE "GERBV_LOCALEDIR=\"${LOCALEDIR}\"")
target_compile_definitions(compilation-flags INTERFACE "GERBV_BINDIR=\"${CMAKE_INSTALL_FULL_BINDIR}\"")
target_compile_definitions(compilation-flags INTERFACE "GERBV_SCMSUBDIR=\"${SCMSUBDIR}\"")
target_compile_definitions(compilation-flags INTERFACE "GERBV_BACKEND_DIR=\"${BACKEND_DIR}\"")

if(NOT WIN32 AND CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    # set CMAKE_INSTALL_PREFIX with -D parameter to cmake
    set(CPACK_PACKAGING_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX})
endif()

include(GetGitVersion)
get_git_version(GIT_VERSION)

# Parse git version for packaging
# Format: v4.0.1 or v4.0.1-23-gabcd1234 or v4.0.1-dirty
string(REGEX REPLACE "^v" "" GIT_VERSION_CLEAN "${GIT_VERSION}")
string(REGEX MATCH "^([0-9]+)\\.([0-9]+)\\.?([0-9]*)" VERSION_MATCH "${GIT_VERSION_CLEAN}")
set(VERSION_MAJOR "${CMAKE_MATCH_1}")
set(VERSION_MINOR "${CMAKE_MATCH_2}")
set(VERSION_PATCH "${CMAKE_MATCH_3}")

# If no patch version, default to 0
if("${VERSION_PATCH}" STREQUAL "")
    set(VERSION_PATCH "0")
endif()

# Extract additional info (commit count + hash or -dirty)
string(REGEX REPLACE "^[0-9]+\\.[0-9]+\\.?[0-9]*-?" "" VERSION_SUFFIX "${GIT_VERSION_CLEAN}")

# Override project version with git version
set(PROJECT_VERSION_MAJOR ${VERSION_MAJOR})
set(PROJECT_VERSION_MINOR ${VERSION_MINOR})
set(PROJECT_VERSION_PATCH ${VERSION_PATCH})
set(PROJECT_VERSION "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")

message(STATUS "Version from git: ${GIT_VERSION}")
message(STATUS "Parsed version: ${PROJECT_VERSION} (suffix: ${VERSION_SUFFIX})")

add_subdirectory(locale)
add_subdirectory(config)
add_subdirectory(src)
add_subdirectory(man)
add_subdirectory(desktop)
add_subdirectory(thirdparty)

install(DIRECTORY example TYPE DOC) # Rename to examples
install(DIRECTORY share/ TYPE DATA)

enable_testing()
add_subdirectory(test)

# where to find our CMake modules
include(Packing)
