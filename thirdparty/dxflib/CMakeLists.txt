
# Bundled dxflib (GPL-2+, RibbonSoft/QCAD)
add_library(dxflib_bundled OBJECT)
target_sources(dxflib_bundled PRIVATE dl_dxf.cpp dl_writer_ascii.cpp)
set_target_properties(dxflib_bundled PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(dxflib_bundled PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(dxflib_bundled PRIVATE gettext_support)
