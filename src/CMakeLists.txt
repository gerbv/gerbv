
######
# Building the library, installed as libgerbv.
# The naming convention is to avoid name clashing in targets

add_library(gerbv-lib INTERFACE)
target_sources(gerbv-lib INTERFACE
                amacro.c amacro.h
                csv.c csv.h csv_defines.h
                draw-gdk.c draw-gdk.h
                draw.c draw.h
                drill.c drill.h
                drill_stats.c drill_stats.h
                export-drill.c
                export-geda-pcb.c
                export-image.c
                export-svg.c export-svg.h
                export-isel-drill.c
                export-rs274x.c
                gerb_file.c gerb_file.h
                gerb_image.c gerb_image.h
                gerb_stats.c gerb_stats.h
                gerber.c gerber.h
                gerbv.c gerbv.h
                gerbv_icon.h
                pick-and-place.c pick-and-place.h
                selection.c selection.h
                tooltable.c)
target_link_libraries(gerbv-lib INTERFACE compilation-flags config gettext_support)

# DXF support, always using the bundled one
target_sources(gerbv-lib INTERFACE export-dxf.cpp)

# We must include dxflib_bundle in respective actual library,
# probably because it is an OBJECT that actually get compiled/linked
# in there and not when in an INTERFACE library
#target_link_libraries(gerbv INTERFACE dxflib_bundled)

##
## Lets build the shared version, aka libgerbv.so
add_library(gerbv-shared SHARED)
target_link_libraries(gerbv-shared PRIVATE gerbv-lib dxflib_bundled)

set_target_properties(gerbv-shared PROPERTIES
                   VERSION 1.11.0
                   SOVERSION 1
                   PUBLIC_HEADER gerbv.h
                   OUTPUT_NAME gerbv)

install(TARGETS gerbv-shared
        PUBLIC_HEADER
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${CMAKE_PROJECT_NAME})

##
## Lets build the static version of libgerbv, aka libgerbv.a
add_library(gerbv-static STATIC)
target_link_libraries(gerbv-static PRIVATE gerbv-lib dxflib_bundled)

set_target_properties(gerbv-static PROPERTIES
                   OUTPUT_NAME gerbv)

install(TARGETS gerbv-static)

######
# Building the application, installed as gerbv.
# The naming convention is to avoid name clashing in targets

add_executable(gerbv-app)
set_target_properties(gerbv-app PROPERTIES
                     OUTPUT_NAME gerbv)
target_sources(gerbv-app PRIVATE
                attribute.c attribute.h
                callbacks.c callbacks.h
                icons.h
                interface.c interface.h
                main.c main.h
                project.c project.h
                render.c render.h
                table.c table.h
                lrealpath.c lrealpath.h)
# target_compile_definitions(gerbv-app PUBLIC GDK_DISABLE_DEPRECATED GTK_DISABLE_DEPRECATED) # Removes all deprecated functions
target_compile_definitions(gerbv-app PRIVATE $<$<BOOL:${DEBUG_PRINTOUT}>:DEBUG=1>)
target_link_libraries(gerbv-app PRIVATE gerbv-shared compilation-flags config tinyscheme gettext_support)
add_dependencies(gerbv-app generated)
if(WIN32)
    # Embed the application icon via Windows resource file
    target_sources(gerbv-app PRIVATE gerbv.rc)
    # Build as a GUI application (no console window).  The existing
    # attach_console_for_win() in main.c re-attaches to the parent
    # console when launched from a terminal.
    if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
        target_link_options(gerbv-app PRIVATE -mwindows)
    endif()
endif()
if(APPLE)
    set_target_properties(gerbv-app PROPERTIES
                         INSTALL_RPATH "${CMAKE_INSTALL_FULL_LIBDIR}")
endif()

install(TARGETS gerbv-app)

set(prefix ${CMAKE_INSTALL_PREFIX})
set(exec_prefix "\$\{prefix\}")
set(libdir "\$\{prefix\}/${CMAKE_INSTALL_LIBDIR}")
set(includedir "\$\{prefix\}/${CMAKE_INSTALL_INCLUDEDIR}")
set(PACKAGE ${PROJECT_NAME})
configure_file(libgerbv.pc.in libgerbv.pc @ONLY)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libgerbv.pc
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
