# Discovers and installs the MinGW runtime DLLs required by gerbv on Windows.
# Recursively walks the PE import table of gerbv.exe and libgerbv.dll using
# the cross-compiler objdump, then installs only the DLLs that are actually
# needed (not the entire MinGW sysroot).
#
# Only active for WIN32 targets (cross-compile or native MinGW builds).
# file(GET_RUNTIME_DEPENDENCIES) is intentionally not used here — it relies on
# the host objdump which does not understand PE/COFF format.
#
# Both paths also respect manual overrides via -DMINGW_OBJDUMP= and
# -DMINGW_DLL_DIR= if someone has an unusual setup.
#
# Scenario              _compiler_full                      objdump derivation                  DLL dir
# Cross-compile         /usr/bin/x86_64-w64-mingw32-gcc     matches -gcc$ -> ...-objdump        CMAKE_FIND_ROOT_PATH/sys-root/mingw/bin
# MSYS2 native          /ucrt64/bin/gcc.exe                 no -gcc$ match -> find_program      /ucrt64/bin (compiler dir)

if(NOT WIN32)
    return()
endif()

# Resolve the full path of the C compiler so we can locate sibling tools.
if(IS_ABSOLUTE "${CMAKE_C_COMPILER}")
    set(_compiler_full "${CMAKE_C_COMPILER}")
else()
    find_program(_compiler_full "${CMAKE_C_COMPILER}" REQUIRED)
endif()
get_filename_component(_compiler_dir "${_compiler_full}" DIRECTORY)

# objdump: prefixed cross-compiler (x86_64-w64-mingw32-gcc) → replace suffix.
# Unprefixed compiler (MSYS2: /ucrt64/bin/gcc.exe) → find objdump beside it.
if(NOT DEFINED MINGW_OBJDUMP)
    if(_compiler_full MATCHES "(-gcc|-cc)$")
        string(REGEX REPLACE "(-gcc|-cc)$" "-objdump" _derived_objdump "${_compiler_full}")
        if(EXISTS "${_derived_objdump}")
            set(MINGW_OBJDUMP "${_derived_objdump}")
        endif()
    endif()
    if(NOT MINGW_OBJDUMP)
        find_program(MINGW_OBJDUMP NAMES objdump
                     HINTS "${_compiler_dir}" NO_DEFAULT_PATH)
    endif()
endif()

# DLL directory: cross-compile sysroots keep DLLs under CMAKE_FIND_ROOT_PATH;
# MSYS2 and other native setups keep them beside the compiler.
if(NOT DEFINED MINGW_DLL_DIR)
    if(CMAKE_FIND_ROOT_PATH)
        set(MINGW_DLL_DIR "${CMAKE_FIND_ROOT_PATH}/sys-root/mingw/bin")
    else()
        set(MINGW_DLL_DIR "${_compiler_dir}")
    endif()
endif()

if(NOT MINGW_OBJDUMP)
    message(WARNING "InstallWindowsDLLs: cross-compiler objdump not found, runtime DLLs will not be bundled")
    return()
endif()

install(CODE "
    set(_dll_dir  \"${MINGW_DLL_DIR}\")
    set(_objdump  \"${MINGW_OBJDUMP}\")

    set(_queue \"\")
    list(APPEND _queue \"$<TARGET_FILE:gerbv-app>\")
    list(APPEND _queue \"$<TARGET_FILE:gerbv-shared>\")
    set(_seen      \"\")
    set(_to_install \"\")

    while(_queue)
        list(POP_FRONT _queue _current)
        execute_process(
            COMMAND \"\${_objdump}\" -p \"\${_current}\"
            OUTPUT_VARIABLE _output
            ERROR_QUIET
        )
        string(REGEX MATCHALL \"DLL Name: [^\r\n]+\" _matches \"\${_output}\")
        foreach(_match IN LISTS _matches)
            string(REGEX REPLACE \"^DLL Name: \" \"\" _dep \"\${_match}\")
            string(STRIP  \"\${_dep}\" _dep)
            string(TOLOWER \"\${_dep}\" _dep)
            list(FIND _seen \"\${_dep}\" _idx)
            if(_idx EQUAL -1)
                list(APPEND _seen \"\${_dep}\")
                set(_path \"\${_dll_dir}/\${_dep}\")
                if(EXISTS \"\${_path}\")
                    list(APPEND _to_install \"\${_path}\")
                    list(APPEND _queue      \"\${_path}\")
                endif()
            endif()
        endforeach()
    endwhile()

    list(LENGTH _to_install _count)
    message(STATUS \"Installing \${_count} runtime DLLs\")
    foreach(_dll IN LISTS _to_install)
        file(INSTALL \"\${_dll}\" DESTINATION \"\${CMAKE_INSTALL_PREFIX}/bin\")
    endforeach()
")
