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

if(WIN32)
    set_tests_properties(gerbv_regression_tests PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:gerbv-shared>"
    )
else()
    set_tests_properties(gerbv_regression_tests PROPERTIES
        ENVIRONMENT_MODIFICATION "LD_LIBRARY_PATH=path_list_prepend:$<TARGET_FILE_DIR:gerbv-shared>"
    )
endif()

# Issue #488: verify negative coordinates in actual Excellon exports.
add_test(
    NAME gerbv_issue_488_negative_drill_coordinates
    COMMAND ${BASH_PROGRAM} ${CMAKE_CURRENT_SOURCE_DIR}/test_negative_drill_coordinates.sh
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(gerbv_issue_488_negative_drill_coordinates PROPERTIES
    ENVIRONMENT "GERBV=$<TARGET_FILE:gerbv-app>;GERBV_SCHEMEINIT=${CMAKE_SOURCE_DIR}/thirdparty/tinyscheme"
)
if(WIN32)
    set_tests_properties(gerbv_issue_488_negative_drill_coordinates PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:gerbv-shared>"
    )
else()
    set_tests_properties(gerbv_issue_488_negative_drill_coordinates PROPERTIES
        ENVIRONMENT_MODIFICATION "LD_LIBRARY_PATH=path_list_prepend:$<TARGET_FILE_DIR:gerbv-shared>"
    )
endif()
