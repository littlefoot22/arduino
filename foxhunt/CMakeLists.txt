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
#
# -mcpu=mvp pins the WebAssembly feature set to the original MVP, then
# -mbulk-memory adds back the one extension the vendor examples opt into.
#
# This matters more than it looks. Recent clang enables reference-types,
# multivalue, sign-ext, nontrapping-fptoint and extended-const by default, and
# the FreeWili's embedded interpreter rejects a module that declares them - it
# fails validation and simply never runs, with no error anywhere. A device that
# does nothing at all, not even light an LED, is the symptom.
#
# -nostdlib is part of the same fix rather than an optimisation: wasi-sdk ships
# a precompiled sysroot built with those same post-MVP features, and the linker
# unions their feature flags into the output even when almost no libc is used.
# This app calls none of it (all formatting is hand-rolled in ui.cpp), so
# dropping the sysroot is what actually gets the feature list clean.
set(WASM_COMPILER_ARGS --target=wasm32-wasip1 -mcpu=mvp -mbulk-memory -nostdlib -O3 -flto)

# The FreeWili gives a WASM app exactly one 64 KB page for stack plus heap, so
# these limits are hardware facts rather than preferences.
#
# Matches the vendor examples. The built module carries only ~630 bytes of
# static data, so a 48 KB stack still leaves room inside the single page.
set(WASM_LINKER_ARGS
    "-nostdlib"
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

# Startup probe, built alongside. Reports progress on the board LEDs so a
# device that draws nothing can still say how far it got. See tools/probe.cpp.
# Minimal display sanity check, built with an even stricter feature set: plain
# MVP with no bulk-memory, the most conservative module this toolchain emits.
add_executable(hello tools/hello.cpp)
target_link_libraries(hello PUBLIC fwwasm)
target_compile_options(hello PRIVATE ${WARNING_ARGS}
    --target=wasm32-wasip1 -mcpu=mvp -nostdlib -O3)
target_link_options(hello PRIVATE ${WASM_LINKER_ARGS})
set_target_properties(hello PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(probe tools/probe.cpp)
target_link_libraries(probe PUBLIC fwwasm)
target_compile_options(probe PRIVATE ${WARNING_ARGS} ${WASM_COMPILER_ARGS})
target_link_options(probe PRIVATE ${WASM_LINKER_ARGS})
set_target_properties(probe PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)
