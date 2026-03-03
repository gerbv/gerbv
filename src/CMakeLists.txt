
######
# Building the library, installed as libgerbv.
# The naming convention is to avoid name clashing in targets

add_library(gerbv)
set_target_properties(gerbv PROPERTIES
                   VERSION 1.10.0
                   SOVERSION 1
                   PUBLIC_HEADER gerbv.h)
target_sources(gerbv PRIVATE
                amacro.c amacro.h
                common.h
                csv.c csv.h csv_defines.h
                draw-gdk.c draw-gdk.h
                draw.c draw.h
                drill.c drill.h
                drill_stats.c drill_stats.h
                export-drill.c
                export-geda-pcb.c
                export-image.c
                export-isel-drill.c
                export-rs274x.c
                gerb_file.c gerb_file.h
                gerb_image.c gerb_image.h
                gerb_stats.c gerb_stats.h
                gerber.c gerber.h
                gerbv.c gerbv.h
                gerbv_icon.h
                gettext.h
                pick-and-place.c pick-and-place.h
                selection.c selection.h
                tooltable.c)
if(DXF_FOUND)
    target_sources(gerbv PRIVATE export-dxf.cpp)
endif()
target_link_libraries(gerbv PRIVATE compilation-flags config)

install(TARGETS gerbv
        PUBLIC_HEADER
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${CMAKE_PROJECT_NAME})

######
# Building the application, installed as gerbv.
# The naming convention is to avoid name clashing in targets

add_executable(gerbv-app)
set_target_properties(gerbv-app PROPERTIES
                     OUTPUT_NAME gerbv
                     PUBLIC_HEADER init.scm)
target_sources(gerbv-app PRIVATE
                attribute.c attribute.h
                callbacks.c callbacks.h
                common.h
                dynload.c dynload.h
                icons.h
                interface.c interface.h
                main.c main.h
                opdefines.h
                project.c project.h
                render.c render.h
                scheme-private.h scheme.c scheme.h
                table.c table.h
                lrealpath.c lrealpath.h)
# target_compile_definitions(gerbv-app PUBLIC GDK_DISABLE_DEPRECATED GTK_DISABLE_DEPRECATED) # Removes all deprecated functions
target_compile_definitions(gerbv-app PRIVATE $<$<BOOL:${DEBUG_PRINTOUT}>:DEBUG=1>)
target_link_libraries(gerbv-app PRIVATE gerbv compilation-flags config)
add_dependencies(gerbv-app generated)
if(APPLE)
    set_target_properties(gerbv-app PROPERTIES
                         INSTALL_RPATH "${CMAKE_INSTALL_FULL_LIBDIR}")
endif()

install(TARGETS gerbv-app
        PUBLIC_HEADER
            DESTINATION ${BACKEND_DIR})

set(prefix ${CMAKE_INSTALL_PREFIX})
set(exec_prefix "\$\{prefix\}")
set(libdir "\$\{prefix\}/${CMAKE_INSTALL_LIBDIR}")
set(includedir "\$\{prefix\}/${CMAKE_INSTALL_INCLUDEDIR}")
set(PACKAGE ${PROJECT_NAME})
configure_file(libgerbv.pc.in libgerbv.pc @ONLY)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libgerbv.pc
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
