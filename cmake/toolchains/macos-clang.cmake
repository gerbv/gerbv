#########################
# macOS clang toolchain #
#########################

set(CMAKE_SYSTEM_NAME Darwin)

# Use the native Apple Clang toolchain on GitHub macOS runners.
set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)

add_compile_definitions(_GNU_SOURCE)
