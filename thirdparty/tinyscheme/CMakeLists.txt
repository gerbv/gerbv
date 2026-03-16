
add_library(tinyscheme STATIC)

## Bundled TinyScheme 1.35 (BSD 3-Clause, Dimitrios Souflis)
target_sources(tinyscheme PRIVATE scheme.c dynload.c)

set_target_properties(tinyscheme PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(tinyscheme PUBLIC ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(tinyscheme PRIVATE config gettext_support)

if(Intl_FOUND)
    target_link_libraries(tinyscheme PRIVATE Intl::Intl)
endif()

install(FILES ${CMAKE_CURRENT_LIST_DIR}/init.scm
        DESTINATION ${CMAKE_INSTALL_DATADIR}/${CMAKE_PROJECT_NAME}/${SCMSUBDIR})
