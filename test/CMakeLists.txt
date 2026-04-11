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

# X2/X3 attribute storage and round-trip test
add_executable(test_x2_attrs test_x2_attrs.c)
target_include_directories(test_x2_attrs PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_x2_attrs PRIVATE gerbv-shared compilation-flags)
add_test(
    NAME x2_attribute_test
    COMMAND test_x2_attrs
        ${CMAKE_CURRENT_SOURCE_DIR}/inputs/test-gerber-x2-attributes.gbx
        ${CMAKE_CURRENT_BINARY_DIR}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
if(WIN32)
    set_tests_properties(x2_attribute_test PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:gerbv-shared>"
    )
else()
    set_tests_properties(x2_attribute_test PROPERTIES
        ENVIRONMENT_MODIFICATION "LD_LIBRARY_PATH=path_list_prepend:$<TARGET_FILE_DIR:gerbv-shared>"
    )
endif()
