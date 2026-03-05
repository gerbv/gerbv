
set(GETTEXT_DOMAIN "gerbv")
set(GETTEXT_TARGET "gerbv-translations")
set(GETTEXT_OUTPUT_DIR "locale")
set(GETTEXT_LANGUAGES "ja" "ru")
get_filename_component(GERBV_BINARY_DIR "${CMAKE_BINARY_DIR}" ABSOLUTE)

set(GETTEXT_SOURCES
     "../src/attribute.c"
     ${GERBV_BINARY_DIR}/config/authors.c
     ${GERBV_BINARY_DIR}/config/bugs.c
     ../src/callbacks.c
     ../src/csv.c
     ../src/draw-gdk.c
     ../src/draw.c
     ../src/drill.c
     ../src/drill_stats.c
     ../thirdparty/tinyscheme/dynload.c
     ../src/export-drill.c
     ../src/export-image.c
     ../src/export-isel-drill.c
     ../src/export-rs274x.c
     ../src/gerb_file.c
     ../src/gerb_image.c
     ../src/gerb_stats.c
     ../src/gerber.c
     ../src/gerbv.c
     ../src/interface.c
     ../src/interface.h
     ../src/main.c
     ../src/pick-and-place.c
     ../src/project.c
     ../src/render.c
     ../src/tooltable.c
    )

# When we run the CMake gettext script it assumes the .po files are located at
# a certain places. So we copy the translations into the correct places
# with correct names.
# The advantage is that the translations does not get marked updated when
# doing the processing. And it is a more shallow directory structure that
# clearly signals what languages they are a translation of.
# The current CMake script used for gettext processing creates files and
# directories in the source tree which is less than optimal.
file(MAKE_DIRECTORY "locale/po/ru")
file(MAKE_DIRECTORY "locale/po/ja")
file(COPY_FILE ja.po locale/po/ja/gerbv.po)
file(COPY_FILE ru.po locale/po/ru/gerbv.po)

include(Gettext_helpers)

configure_gettext(
 DOMAIN ${GETTEXT_DOMAIN}
 TARGET_NAME ${GETTEXT_TARGET}
 SOURCES ${GETTEXT_SOURCES}
 POTFILE_DESTINATION ${GETTEXT_OUTPUT_DIR} # Relative to CURRENT_SOURCE
 XGETTEXT_ARGS
  "--keyword=_"
  "--add-comments=TRANSLATORS:"
  "--package-name=${PROJECT_NAME}"
  "--package-version=${PROJECT_VERSION}"
  "--msgid-bugs-address=spe-ciellt@github.com"
  "--copyright-holder=erri120"
  "--from-code=utf-8"
 LANGUAGES ${GETTEXT_LANGUAGES}
 BUILD_DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/${GETTEXT_OUTPUT_DIR}
 ALL
)
add_dependencies(${GETTEXT_TARGET} generated)

install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${GETTEXT_OUTPUT_DIR}/ TYPE LOCALE)

# gettext https://erri120.github.io/posts/2022-05-05/
