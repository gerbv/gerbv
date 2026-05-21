find_program(BASH_PROGRAM bash REQUIRED)

add_test(
    NAME gerbv_regression_tests
    COMMAND ${BASH_PROGRAM} ${CMAKE_CURRENT_SOURCE_DIR}/run_tests.sh
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

# CMake 3.28+ required for generator expressions in test properties (already required by project)
set_tests_properties(gerbv_regression_tests PROPERTIES
    ENVIRONMENT "GERBV=$<TARGET_FILE:gerbv-app>;GERBV_SCHEMEINIT=${CMAKE_SOURCE_DIR}/thirdparty/tinyscheme"
)

# Prepend shared library directory so tests find libgerbv at runtime
if(WIN32)
    set(LIBGERBV_ENV_MOD "PATH=path_list_prepend:$<TARGET_FILE_DIR:gerbv-shared>")
else()
    set(LIBGERBV_ENV_MOD "LD_LIBRARY_PATH=path_list_prepend:$<TARGET_FILE_DIR:gerbv-shared>")
endif()

set_tests_properties(gerbv_regression_tests PROPERTIES
    ENVIRONMENT_MODIFICATION "${LIBGERBV_ENV_MOD}"
)

# libgerbv consumer link test — verifies downstream projects
# can link against libgerbv and resolve all symbols (#366)
add_executable(test_libgerbv test_libgerbv.c)
target_include_directories(test_libgerbv PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_libgerbv PRIVATE gerbv-shared PkgConfig::GLIB PkgConfig::GTK)
add_test(NAME libgerbv_link_test COMMAND test_libgerbv)
set_tests_properties(libgerbv_link_test PROPERTIES
    ENVIRONMENT_MODIFICATION "${LIBGERBV_ENV_MOD}"
)

# libgerbv example programs from doc/example-code/ — compile and run
# them as integration tests to catch API drift in documented examples.
# Example 6 is compile-only (GTK GUI, can't run headless).
set(EXAMPLE_DIR ${CMAKE_SOURCE_DIR}/doc/example-code)

foreach(n RANGE 1 6)
    add_executable(example${n} ${EXAMPLE_DIR}/example${n}.c)
    target_include_directories(example${n} PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(example${n} PRIVATE gerbv-shared PkgConfig::GLIB PkgConfig::GTK)
endforeach()

# Examples 1-5 run headless — add as ctest targets
foreach(n RANGE 1 5)
    add_test(
        NAME libgerbv_example${n}
        COMMAND example${n}
        WORKING_DIRECTORY ${EXAMPLE_DIR}
    )
    set_tests_properties(libgerbv_example${n} PROPERTIES
        ENVIRONMENT_MODIFICATION "${LIBGERBV_ENV_MOD}"
    )
endforeach()

# Example 6: compile-only (GTK GUI requires display)
