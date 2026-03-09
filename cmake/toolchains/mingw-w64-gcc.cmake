# From https://www.mingw-w64.org/build-systems/cmake/


# toolchain-mingw64.cmake
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# specify the cross compiler
set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)

# where is the target environment
set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32)

# search for programs in the build host directories
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# for libraries and headers in the target directories
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

set(PKG_CONFIG_EXECUTABLE /usr/bin/x86_64-w64-mingw32-pkg-config)

################
# Common Flags #
################
# Note that CPU_FLAGS, LD_FLAGS, and VFP_FLAGS are set by other Toolchain files
# that include this file.
#
# See the CMake Manual for CMAKE_<LANG>_FLAGS_INIT:
#       https://cmake.org/cmake/help/latest/variable/CMAKE_LANG_FLAGS_INIT.html

set(CMAKE_C_FLAGS_INIT
        "${CPU_FLAGS} ${VFP_FLAGS} -fdata-sections -ffunction-sections"
        CACHE
        INTERNAL "Default C compiler flags.")
set(CMAKE_CXX_FLAGS_INIT
        "${CPU_FLAGS} ${VFP_FLAGS} -fdata-sections -ffunction-sections"
        CACHE
        INTERNAL "Default C++ compiler flags.")
set(CMAKE_ASM_FLAGS_INIT
        "${CPU_FLAGS} -x assembler-with-cpp"
        CACHE
        INTERNAL "Default ASM compiler flags.")
set(CMAKE_EXE_LINKER_FLAGS_INIT
        "${LD_FLAGS} -Wl,--gc-sections"
        CACHE
        INTERNAL "Default linker flags.")
