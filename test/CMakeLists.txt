find_program(BASH_PROGRAM bash REQUIRED)

option(GERBV_ENABLE_CODE_COVERAGE "Enable code coverage instrumentation" OFF)
if(GERBV_ENABLE_CODE_COVERAGE)
    include(CodeCoverage)
    append_coverage_compiler_flags()
endif()

add_test(
    NAME gerbv_regression_tests
    COMMAND ${BASH_PROGRAM} ${CMAKE_CURRENT_SOURCE_DIR}/run_tests.sh
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

# CMake 3.28+ required for generator expressions in test properties (already required by project)
set_tests_properties(gerbv_regression_tests PROPERTIES
    ENVIRONMENT "GERBV=$<TARGET_FILE:gerbv-app>;GERBV_SCHEMEINIT=${CMAKE_SOURCE_DIR}/thirdparty/tinyscheme"
)

if(WIN32)
    set_tests_properties(gerbv_regression_tests PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:gerbv-shared>"
    )
else()
    set_tests_properties(gerbv_regression_tests PROPERTIES
        ENVIRONMENT_MODIFICATION "LD_LIBRARY_PATH=path_list_prepend:$<TARGET_FILE_DIR:gerbv-shared>"
    )
endif()

message(STATUS "BUILD_TESTING: ${BUILD_TESTING}")
message(STATUS "GERBV_ENABLE_CODE_COVERAGE: ${GERBV_ENABLE_CODE_COVERAGE}")
if(BUILD_TESTING AND GERBV_ENABLE_CODE_COVERAGE)
    message(STATUS "Setting up code coverage for gerbv-regression-tests")
    setup_target_for_coverage_lcov(
        NAME lcov
        EXECUTABLE "${CMAKE_CTEST_COMMAND}"
        EXECUTABLE_ARGS --output-on-failure -C ${CMAKE_BUILD_TYPE} -V
        DEPENDENCIES $<TARGET_FILE:gerbv-app>
        LCOV_ARGS --ignore-errors mismatch,inconsistent --rc branch_coverage=1 --rc function_coverage=1 --no-external
        EXCLUDE "build/config/*" "thirdparty/*"
    )
endif()