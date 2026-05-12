configure_file(gerbv.1.in ${CMAKE_CURRENT_BINARY_DIR}/gerbv.1 @ONLY)

cmake_path(SET CMAKE_INSTALL_MANDIR NORMALIZE ${CMAKE_INSTALL_MANDIR}/man1)

# Debian and Redhat installs compressed man pages, macos and the rest does not.
if("${CMAKE_SYSTEM_NAME}" STREQUAL "Linux")
    file(ARCHIVE_CREATE OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/gerbv.1.gz
                        PATHS ${CMAKE_CURRENT_BINARY_DIR}/gerbv.1
                        COMPRESSION GZip)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/gerbv.1.gz TYPE MAN)
else()
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/gerbv.1 TYPE MAN)
endif()
