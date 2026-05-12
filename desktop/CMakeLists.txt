
if(NOT WIN32)
    install(FILES gerbv.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES gerbv_icon.ico DESTINATION ${CMAKE_INSTALL_DATADIR}/${CMAKE_PROJECT_NAME})

    set(ICONS_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR}/icons/hicolor)

    function(copy_icons file size)
        cmake_path(GET file EXTENSION file_ext)
        file(MAKE_DIRECTORY ${ICONS_BUILD_DIR}/${size}/apps/)
        file(COPY_FILE ${file} ${ICONS_BUILD_DIR}/${size}/apps/gerbv${file_ext})
    endfunction()

    copy_icons(gerbv-16.png 16x16)
    copy_icons(gerbv-22.png 22x22)
    copy_icons(gerbv-24.png 24x24)
    copy_icons(gerbv-32.png 32x32)
    copy_icons(gerbv-48.png 48x48)
    copy_icons(gerbv.svg scalable)

    install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/icons TYPE DATA)
endif()
