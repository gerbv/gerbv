
add_library(config INTERFACE)
target_compile_definitions(config INTERFACE HAVE_CONFIG_H)
target_include_directories(config INTERFACE ${CMAKE_CURRENT_BINARY_DIR})

include(${CMAKE_SOURCE_DIR}/BuildOptions.cmake)

cmake_path(RELATIVE_PATH CMAKE_INSTALL_FULL_DATADIR BASE_DIRECTORY ${CMAKE_INSTALL_FULL_BINDIR} OUTPUT_VARIABLE BINDIR_TO_DATADIR)
cmake_path(RELATIVE_PATH CMAKE_INSTALL_PREFIX BASE_DIRECTORY ${CMAKE_INSTALL_FULL_BINDIR} OUTPUT_VARIABLE BINDIR_TO_EXECPREFIX)
cmake_path(RELATIVE_PATH DATADIR BASE_DIRECTORY ${CMAKE_INSTALL_FULL_BINDIR} OUTPUT_VARIABLE BINDIR_TO_PKGDATADIR)

include(CheckIncludeFile)
check_include_file(unistd.h HAVE_UNISTD_H)
check_include_file(getopt.h HAVE_GETOPT_H)
check_include_file(string.h HAVE_STRING_H)
check_include_file(strings.h HAVE_STRINGS_H)
check_include_file(sys/mman.h HAVE_SYS_MMAN_H)
check_include_file(sys/types.h HAVE_SYS_TYPES_H)
check_include_file(sys/stat.h HAVE_SYS_STAT_H)
check_include_file(stdlib.h HAVE_STDLIB_H)
check_include_file(regex.h HAVE_REGEX_H)
check_include_file(libgen.h HAVE_LIBGEN_H)
check_include_file(time.h HAVE_TIME_H)
check_include_file(dlfcn.h HAVE_DLFCN_H)
check_include_file(wchar.h HAVE_WCHAR_H) # Potentially update source csv.c

include(CheckFunctionExists)
check_function_exists(getopt_long HAVE_GETOPT_LONG)
check_function_exists(strlwr HAVE_STRLWR)
check_function_exists(realpath HAVE_REALPATH)
check_function_exists(canonicalize_file_name HAVE_CANONICALIZE_FILE_NAME)
check_function_exists(dcgettext HAVE_DCGETTEXT)
check_function_exists(iconv HAVE_ICONV)

include(CheckSymbolExists)
check_symbol_exists(canonicalize_file_name stdlib.h HAVE_CANONICALIZE_FILE_NAME_DECL)

include(CheckLibraryExists)
check_library_exists(m pow "" HAVE_LIBM)
check_library_exists(dl dlopen "" HAVE_LIBDL)

set(PACKAGE_VERSION ${PROJECT_VERSION})
set(PACKAGE ${PROJECT_NAME})
set(PACKAGE_NAME ${PROJECT_NAME})
set(PACKAGE_BUGREPORT "")
set(PACKAGE_STRING "${PROJECT_NAME} ${PROJECT_VERSION}")
set(PACKAGE_TARNAME ${PROJECT_NAME})
set(PACKAGE_URL "https://github.com/gerbv")
set(VERSION ${GIT_VERSION})

if(${Gettext_FOUND})
    set(HAVE_GETTEXT 1)
endif()
if(Intl_FOUND)
    set(ENABLE_NLS 1)
endif()

if(WIN32)
    set(GERBV_DIR_SEPARATOR_C '\\\\')
    set(GERBV_DIR_SEPARATOR_S "\\\\")
else()
    set(GERBV_DIR_SEPARATOR_C '/')
    set(GERBV_DIR_SEPARATOR_S "/")
endif()

############################################################
#
# scheme configure stuff
#
option(STANDALONE "Scheme interpreter not used standalone" OFF)
option(USE_DL "Dynamic linking in Scheme interpreter" ON)
option(SUN_DL "DL Sun method" ON)
option(USE_MATH "Math in Scheme interpreter" ON)
option(USE_ASCII_NAMES "ASCII names in Scheme interpreter" ON)
option(USE_COLON_HOOKS "Colon Hooks in Scheme interpreter" ON)
option(USE_STRING_HOOKS "String Hooks in Scheme interpreter" ON)
option(USE_INTERFACE "Use extension interface of Scheme interpreter" ON)

configure_file(config.h.in config.h @ONLY)

set(GENERATOR_SCRIPT_DIR ${CMAKE_SOURCE_DIR}/cmake/generate)

# Generate at configure time so xgettext can find them
execute_process(
    COMMAND ${CMAKE_COMMAND}
            -DINPUT_BUGS=${CMAKE_SOURCE_DIR}/BUGS
            -DOUTPUT_BUGS=${CMAKE_CURRENT_BINARY_DIR}/bugs.c
            -DINPUT_AUTHORS=${CMAKE_SOURCE_DIR}/AUTHORS
            -DOUTPUT_AUTHORS=${CMAKE_CURRENT_BINARY_DIR}/authors.c
            -P ${GENERATOR_SCRIPT_DIR}/generate_headers.cmake
    COMMAND_ERROR_IS_FATAL ANY
)

add_custom_command(
    OUTPUT bugs.c authors.c
    COMMAND ${CMAKE_COMMAND}
            -DINPUT_BUGS=${CMAKE_SOURCE_DIR}/BUGS
            -DOUTPUT_BUGS=${CMAKE_CURRENT_BINARY_DIR}/bugs.c
            -DINPUT_AUTHORS=${CMAKE_SOURCE_DIR}/AUTHORS
            -DOUTPUT_AUTHORS=${CMAKE_CURRENT_BINARY_DIR}/authors.c
             -P ${GENERATOR_SCRIPT_DIR}/generate_headers.cmake
    DEPENDS ${CMAKE_SOURCE_DIR}/BUGS ${CMAKE_SOURCE_DIR}/AUTHORS
    VERBATIM)

add_custom_target(generated DEPENDS bugs.c authors.c)
set_target_properties(generated PROPERTIES
                        ADDITIONAL_CLEAN_FILES "bugs.c;authors.c")
