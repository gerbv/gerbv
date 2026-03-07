##############################
# MSYS2 UCRT64 GCC toolchain #
##############################

set(CMAKE_SYSTEM_NAME Windows)

set(CMAKE_C_COMPILER gcc)
set(CMAKE_CXX_COMPILER g++)
set(CMAKE_RC_COMPILER windres)

################
# Common Flags #
################

set(CMAKE_C_FLAGS_INIT
        "-fdata-sections -ffunction-sections"
        CACHE
        INTERNAL "Default C compiler flags.")
set(CMAKE_CXX_FLAGS_INIT
        "-fdata-sections -ffunction-sections"
        CACHE
        INTERNAL "Default C++ compiler flags.")
set(CMAKE_EXE_LINKER_FLAGS_INIT
        "-Wl,--gc-sections"
        CACHE
        INTERNAL "Default linker flags.")
