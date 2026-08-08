cmake_minimum_required(VERSION 3.25)

# Build with the WASI SDK toolchain file, e.g.
#   cmake -DCMAKE_TOOLCHAIN_FILE=${WASI_SDK_PATH}/share/cmake/wasi-sdk.cmake -B build .
#   cmake --build build
# Output: build/foxhunt.wasm
#
# The host-side logic tests live in tests/ and build natively; see tests/README
# or just run ./run_tests.sh.

project(foxhunt VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXECUTABLE_SUFFIX ".wasm")

include(FetchContent)

# Public header describing the host functions the FreeWili firmware exports.
FetchContent_Declare(
    fwwasm
    GIT_REPOSITORY https://github.com/freewili/fwwasm.git
    GIT_TAG master
)
FetchContent_MakeAvailable(fwwasm)

set(WARNING_ARGS
    -Wall -Werror -Wextra -Wpedantic
    -Wconversion -Wsign-conversion -Wfloat-equal -Wold-style-cast
)

# wasm32-wasip1 is the current spelling of the triple the FreeWili examples call
# wasm32-unknown-wasi; the old name is a hard error from wasi-sdk 33 on. If you
# are on an SDK older than 25, swap it back.
set(WASM_COMPILER_ARGS --target=wasm32-wasip1 -O3 -flto -mbulk-memory)

# The FreeWili gives a WASM app exactly one 64 KB page for stack plus heap, so
# these limits are hardware facts rather than preferences.
#
# Matches the vendor examples. The built module carries only ~630 bytes of
# static data, so a 48 KB stack still leaves room inside the single page.
set(WASM_LINKER_ARGS
    "-Wl,--no-entry"
    "-Wl,--export-all"
    "-Wl,--lto-O3"
    "-Wl,-z,stack-size=49152"
    "-Wl,--initial-heap=0"
    "-Wl,--max-memory=65536"
    "-Wl,--initial-memory=65536"
    "-Wl,--stack-first"
    "-Wl,--strip-all"
)

add_executable(${PROJECT_NAME}
    src/main.cpp
    src/cc1101.cpp
    src/df.cpp
    src/ui.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE src)
target_link_libraries(${PROJECT_NAME} PUBLIC fwwasm)
target_compile_options(${PROJECT_NAME} PRIVATE ${WARNING_ARGS} ${WASM_COMPILER_ARGS})
target_link_options(${PROJECT_NAME} PRIVATE ${WASM_LINKER_ARGS})

# Drop the .wasm at the top of the build dir so the flash command is short.
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)
