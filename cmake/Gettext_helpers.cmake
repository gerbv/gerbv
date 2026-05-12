
find_program(GETTEXT_XGETTEXT_COMMAND xgettext)
find_program(GETTEXT_MSGFMT_COMMAND msgfmt)
find_program(GETTEXT_MSGINIT_COMMAND msginit)
find_program(GETTEXT_MSGMERGE_COMMAND msgmerge)

# configure_gettext(
#     DOMAIN <domain-name>
#     TARGET_NAME <target-name>
#     SOURCES <file> ...
#     POTFILE_DESTINATION <dir>
#     POFILE_DESTINATION <dir>
#     GMOFILE_DESTINATION <dir>
#     LANGUAGES <file> ...
#     [ALL]
#     [INSTALL_DESTINATION <dest>]
#     [INSTALL_COMPONENT <dest>]
#     [BUILD_DESTINATION <dest>]
#     [XGETTEXT_ARGS <args> ...
#     [MSGMERGE_ARGS <args> ...]
#     [MSGINIT_ARGS <args> ...]
#     [MSGFMT_ARGS <args> ... ]
#     )

function(configure_gettext)
    # Ensure the utility programs are available
    if(NOT GETTEXT_XGETTEXT_COMMAND OR NOT GETTEXT_MSGFMT_COMMAND
            OR NOT GETTEXT_MSGMERGE_COMMAND OR NOT GETTEXT_MSGINIT_COMMAND)
        message(FATAL_ERROR "Could not find required programs!")
    endif()

    set(options ALL)
    set(one_value_args 
        DOMAIN INSTALL_DESTINATION INSTALL_COMPONENT TARGET_NAME
        POTFILE_DESTINATION POFILE_DESTINATION GMOFILE_DESTINATION
        BUILD_DESTINATION
    )
    set(multi_args SOURCES LANGUAGES XGETTEXT_ARGS MSGFMT_ARGS MSGINIT_ARGS MSGMERGE_ARGS)
    cmake_parse_arguments(GETTEXT
        "${options}" "${one_value_args}" "${multi_args}" ${ARGV})

    if(NOT GETTEXT_DOMAIN)
        message(FATAL_ERROR "Must supply a DOMAIN!")
    elseif(NOT GETTEXT_POTFILE_DESTINATION)
        message(FATAL_ERROR "Must supply a POTFILE_DESTINATION!")
    elseif(NOT GETTEXT_LANGUAGES)
        message(FATAL_ERROR "No LANGUAGES specified!")
    elseif(NOT GETTEXT_TARGET_NAME)
        message(FATAL_ERROR "No TARGET_NAME specified!")
    elseif(NOT GETTEXT_SOURCES)
        message(FATAL_ERROR "No SOURCES supplied!")
    elseif(GETTEXT_INSTALL_COMPONENT AND NOT GETTEXT_INSTALL_DESTINATION)
        message(FATAL_ERROR "INSTALL_COMPONENT relies on INSTALL_DESTINATION")
    endif()

    if(NOT GETTEXT_POFILE_DESTINATION)
        set(GETTEXT_POFILE_DESTINATION "${GETTEXT_POTFILE_DESTINATION}/po/")
        message(STATUS "POFILE_DESTINATION defaulting to POTFILE_DESTINATION/po/")
    endif()
    if(NOT GETTEXT_GMOFILE_DESTINATION)
        set(GETTEXT_GMOFILE_DESTINATION "${GETTEXT_POFILE_DESTINATION}")
        message(STATUS "GMOFILE_DESTINATION defaulting to POFILE_DESTINATION")
    endif()

    # Make input directories absolute in relation to the current directory
    if(NOT IS_ABSOLUTE "${GETTEXT_POTFILE_DESTINATION}")
        set(GETTEXT_POTFILE_DESTINATION "${CMAKE_CURRENT_SOURCE_DIR}/${GETTEXT_POTFILE_DESTINATION}")
        file(TO_CMAKE_PATH "${GETTEXT_POTFILE_DESTINATION}" GETTEXT_POTFILE_DESTINATION)
    endif()
    if(NOT IS_ABSOLUTE "${GETTEXT_POFILE_DESTINATION}")
        set(GETTEXT_POFILE_DESTINATION "${CMAKE_CURRENT_SOURCE_DIR}/${GETTEXT_POFILE_DESTINATION}")
        file(TO_CMAKE_PATH "${GETTEXT_POFILE_DESTINATION}" GETTEXT_POFILE_DESTINATION)
    endif()
    if(NOT IS_ABSOLUTE "${GETTEXT_GMOFILE_DESTINATION}")
        set(GETTEXT_GMOFILE_DESTINATION "${CMAKE_CURRENT_SOURCE_DIR}/${GETTEXT_GMOFILE_DESTINATION}")
        file(TO_CMAKE_PATH "${GETTEXT_GMOFILE_DESTINATION}" GETTEXT_GMOFILE_DESTINATION)
    endif()

    # Create needed directories
    if(NOT EXISTS "${GETTEXT_POTFILE_DESTINATION}")
        file(MAKE_DIRECTORY "${GETTEXT_POTFILE_DESTINATION}")
    endif()
    if(NOT EXISTS "${GETTEXT_POFILE_DESTINATION}")
        file(MAKE_DIRECTORY "${GETTEXT_POFILE_DESTINATION}")
    endif()
    if(NOT EXISTS "${GETTEXT_GMOFILE_DESTINATION}")
        file(MAKE_DIRECTORY "${GETTEXT_GMOFILE_DESTINATION}")
    endif()

    if(GETTEXT_ALL)
        add_custom_target("${GETTEXT_TARGET_NAME}" ALL)
    else()
        add_custom_target("${GETTEXT_TARGET_NAME}")
    endif()

    # Generate the .pot file from the program sources
    # sources ---{xgettext}---> .pot
    if(NOT EXISTS "${GETTEXT_POTFILE_DESTINATION}/${GETTEXT_DOMAIN}.pot")
        message(STATUS "Creating initial .pot file")
        execute_process(
            COMMAND "${GETTEXT_XGETTEXT_COMMAND}" ${GETTEXT_XGETTEXT_ARGS}
                ${GETTEXT_SOURCES}
                "--output=${GETTEXT_POTFILE_DESTINATION}/${GETTEXT_DOMAIN}.pot"
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            COMMAND_ERROR_IS_FATAL ANY)
    endif()
    add_custom_command(
        OUTPUT "${GETTEXT_POTFILE_DESTINATION}/${GETTEXT_DOMAIN}.pot"
        COMMAND "${GETTEXT_XGETTEXT_COMMAND}" ${GETTEXT_XGETTEXT_ARGS}
            ${GETTEXT_SOURCES}
            "--output=${GETTEXT_POTFILE_DESTINATION}/${GETTEXT_DOMAIN}.pot"
        DEPENDS ${GETTEXT_SOURCES}
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMENT "Generating a .pot file from program sources")

    foreach(lang IN LISTS GETTEXT_LANGUAGES)
        # Create needed directories
        if(NOT EXISTS "${GETTEXT_POFILE_DESTINATION}/${lang}")
            file(MAKE_DIRECTORY "${GETTEXT_POFILE_DESTINATION}/${lang}")
        endif()
        if(NOT EXISTS "${GETTEXT_GMOFILE_DESTINATION}/${lang}")
            file(MAKE_DIRECTORY "${GETTEXT_GMOFILE_DESTINATION}/${lang}")
        endif()

        # .pot ---{msginit}---> .po
        if(NOT EXISTS "${GETTEXT_POFILE_DESTINATION}/${lang}/${GETTEXT_DOMAIN}.po")
            message(STATUS "Creating initial .po file for ${lang}")
            execute_process(
                COMMAND "${GETTEXT_MSGINIT_COMMAND}" ${GETTEXT_MSGINIT_ARGS}
                    "--input=${GETTEXT_POTFILE_DESTINATION}/${GETTEXT_DOMAIN}.pot"
                    "--output-file=${GETTEXT_POFILE_DESTINATION}/${lang}/${GETTEXT_DOMAIN}.po"
                    "--locale=${lang}"
                WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
                COMMAND_ERROR_IS_FATAL ANY)
        endif()
        add_custom_command(
            OUTPUT "${GETTEXT_POFILE_DESTINATION}/${lang}/${GETTEXT_DOMAIN}.po"
            COMMAND "${GETTEXT_MSGMERGE_COMMAND}" ${GETTEXT_MSGMERGE_ARGS}
                "${GETTEXT_POFILE_DESTINATION}/${lang}/${GETTEXT_DOMAIN}.po"
                "${GETTEXT_POTFILE_DESTINATION}/${GETTEXT_DOMAIN}.pot"
                "--output-file=${GETTEXT_POFILE_DESTINATION}/${lang}/${GETTEXT_DOMAIN}.po"
            DEPENDS "${GETTEXT_POTFILE_DESTINATION}/${GETTEXT_DOMAIN}.pot"
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            COMMENT "Updating the ${lang} .po file from the .pot file")

        add_custom_command(
            OUTPUT "${GETTEXT_GMOFILE_DESTINATION}/${lang}/${GETTEXT_DOMAIN}.gmo"
            COMMAND "${GETTEXT_MSGFMT_COMMAND}" ${GETTEXT_MSGFMT_ARGS}
                "${GETTEXT_POFILE_DESTINATION}/${lang}/${GETTEXT_DOMAIN}.po"
                "--output-file=${GETTEXT_GMOFILE_DESTINATION}/${lang}/${GETTEXT_DOMAIN}.gmo"
            DEPENDS "${GETTEXT_POFILE_DESTINATION}/${lang}/${GETTEXT_DOMAIN}.po"
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            COMMENT "Creating the ${lang} .gmo file from the .po file")

        add_custom_target("${GETTEXT_TARGET_NAME}-${lang}"
            DEPENDS "${GETTEXT_GMOFILE_DESTINATION}/${lang}/${GETTEXT_DOMAIN}.gmo")
        add_dependencies("${GETTEXT_TARGET_NAME}" "${GETTEXT_TARGET_NAME}-${lang}")

        if(GETTEXT_INSTALL_DESTINATION)
            if(GETTEXT_INSTALL_COMPONENT)
                set(comp_line "COMPONENT" "${GETTEXT_INSTALL_COMPONENT}")
            else()
                set(comp_line)
            endif()

            install(FILES "${GETTEXT_GMOFILE_DESTINATION}/${lang}/${GETTEXT_DOMAIN}.gmo"
                DESTINATION "${GETTEXT_INSTALL_DESTINATION}/${lang}/LC_MESSAGES/"
                ${comp_line}
                RENAME "${GETTEXT_DOMAIN}.mo")
        endif()

        if(GETTEXT_BUILD_DESTINATION)
            add_custom_command(
                TARGET "${GETTEXT_TARGET_NAME}-${lang}" PRE_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy
                    "${GETTEXT_GMOFILE_DESTINATION}/${lang}/${GETTEXT_DOMAIN}.gmo"
                    "${GETTEXT_BUILD_DESTINATION}/${lang}/LC_MESSAGES/${GETTEXT_DOMAIN}.mo"
            )
        endif()

    endforeach() # lang IN LISTS GETTEXT_LANGUAGES

endfunction() # configure_gettext
