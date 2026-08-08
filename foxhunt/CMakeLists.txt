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
# -mcpu=mvp pins the WebAssembly feature set to the original MVP and nothing
# more. This matters more than it looks: recent clang enables reference-types,
# multivalue, sign-ext, nontrapping-fptoint and extended-const by default, and
# this device's interpreter rejects a module declaring any of them. It fails
# validation and never runs, with no error reported anywhere - a board that does
# nothing at all, not even light an LED, is the symptom.
#
# -mbulk-memory is left off as a precaution only. It is NOT known to break
# anything: with --strip-all the feature declaration is removed anyway, and this
# code emits no bulk-memory opcodes, so adding the flag produces a byte-identical
# module. Kept off simply because nothing here needs it.
#
# -nostdlib is the part that genuinely mattered: wasi-sdk ships
# a precompiled sysroot built with those post-MVP features, and libc really does
# emit the corresponding opcodes (memory.copy and friends behind memcpy), which
# survive stripping. This app calls no libc at all - all formatting is
# hand-rolled in ui.cpp - so dropping the sysroot removes those opcodes outright
# and took foxhunt.wasm from 24K to 19K.
#
# The build asserts all of this - see the verify_mvp target below.
# -Oz, not -O3. Size is a hard constraint here, not a preference: the device
# reported "Mem used: 119 of 192" with a 20 KB module, leaving ~73 KB free
# against a 64 KB linear-memory page plus the module itself, and trapped with
# "[trap] stack overflow". -Oz with LTO takes this app from 20.7 KB to 8.1 KB.
set(WASM_COMPILER_ARGS --target=wasm32-wasip1 -mcpu=mvp -nostdlib -Oz -flto)

# The FreeWili gives a WASM app exactly one 64 KB page for stack plus heap, so
# these limits are hardware facts rather than preferences.
#
# --export-all is replaced by an explicit list. It cost 2 KB of export section
# and, worse, forced every symbol to be retained so nothing could be stripped.
# The named set matches what the working hello.wasm exports, including "memory"
# - the host reads string arguments out of the module's linear memory, so that
# one is not optional.
#
# The stack is 16 KB rather than 48 KB. Nothing here recurses and the largest
# frame is a 56-byte register blob, so the old figure was reserving most of the
# page for nothing.
set(WASM_LINKER_ARGS
    "-nostdlib"
    "-Wl,--no-entry"
    "-Wl,--export=main"
    "-Wl,--export-if-defined=__main_void"
    "-Wl,--export-if-defined=__original_main"
    "-Wl,--export-if-defined=__wasm_call_ctors"
    "-Wl,--export-if-defined=__heap_base"
    "-Wl,--export-if-defined=__data_end"
    "-Wl,--lto-O3"
    "-Wl,-z,stack-size=16384"
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
    src/wasm_rt.cpp
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
# Minimal display sanity check: one panel, one string. Known to run on
# hardware, so it doubles as the reference every other target is matched to.
add_executable(hello tools/hello.cpp src/wasm_rt.cpp)
target_link_libraries(hello PUBLIC fwwasm)
target_compile_options(hello PRIVATE ${WARNING_ARGS} ${WASM_COMPILER_ARGS})
target_link_options(hello PRIVATE ${WASM_LINKER_ARGS})
set_target_properties(hello PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(probe tools/probe.cpp src/wasm_rt.cpp)
target_link_libraries(probe PUBLIC fwwasm)
target_compile_options(probe PRIVATE ${WARNING_ARGS} ${WASM_COMPILER_ARGS})
target_link_options(probe PRIVATE ${WASM_LINKER_ARGS})
set_target_properties(probe PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

# Cumulative ladder from the known-good hello baseline up to the full set of
# host calls the app needs, one addition per step. See tools/step.cpp.
set(STEP_TARGETS "")
foreach(step RANGE 1 10)
    if(step LESS 10)
        set(name step0${step})
    else()
        set(name step${step})
    endif()
    add_executable(${name} tools/step.cpp src/wasm_rt.cpp)
    target_link_libraries(${name} PUBLIC fwwasm)
    target_compile_definitions(${name} PRIVATE STEP=${step})
    target_compile_options(${name} PRIVATE ${WARNING_ARGS} ${WASM_COMPILER_ARGS})
    target_link_options(${name} PRIVATE ${WASM_LINKER_ARGS})
    set_target_properties(${name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    )
    list(APPEND STEP_TARGETS ${name})
endforeach()

# Import bisector, one binary per group of host functions under test. An
# unresolvable import breaks instantiation, so these find which group the
# firmware does not provide. See tools/imports.cpp.
set(IMPORT_TIER_TARGETS "")
foreach(tier 1 2 3 4 5 11 12 13)
    add_executable(imports${tier} tools/imports.cpp src/wasm_rt.cpp)
    target_link_libraries(imports${tier} PUBLIC fwwasm)
    target_compile_definitions(imports${tier} PRIVATE TIER=${tier})
    target_compile_options(imports${tier} PRIVATE ${WARNING_ARGS} ${WASM_COMPILER_ARGS})
    target_link_options(imports${tier} PRIVATE ${WASM_LINKER_ARGS})
    set_target_properties(imports${tier} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    )
    list(APPEND IMPORT_TIER_TARGETS imports${tier})
endforeach()

# Structural probes: how many panels and controls this firmware accepts.
# See tools/limits.cpp.
set(LIMIT_TARGETS "")
foreach(lim RANGE 1 4)
    add_executable(limits${lim} tools/limits.cpp src/wasm_rt.cpp)
    target_link_libraries(limits${lim} PUBLIC fwwasm)
    target_compile_definitions(limits${lim} PRIVATE LIMIT=${lim})
    target_compile_options(limits${lim} PRIVATE ${WARNING_ARGS} ${WASM_COMPILER_ARGS})
    target_link_options(limits${lim} PRIVATE ${WASM_LINKER_ARGS})
    set_target_properties(limits${lim} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    )
    list(APPEND LIMIT_TARGETS limits${lim})
endforeach()

# RadioLoadConfig() size probes. One size per binary so a reset cannot destroy
# the evidence of which size caused it. See tools/radiocfg.cpp.
set(RADIOCFG_TARGETS "")
foreach(bytes 2 6 12 24 47 56)
    add_executable(radiocfg${bytes} tools/radiocfg.cpp src/wasm_rt.cpp)
    target_link_libraries(radiocfg${bytes} PUBLIC fwwasm)
    target_compile_definitions(radiocfg${bytes} PRIVATE CFG_BYTES=${bytes})
    target_compile_options(radiocfg${bytes} PRIVATE ${WARNING_ARGS} ${WASM_COMPILER_ARGS})
    target_link_options(radiocfg${bytes} PRIVATE ${WASM_LINKER_ARGS})
    set_target_properties(radiocfg${bytes} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    )
    list(APPEND RADIOCFG_TARGETS radiocfg${bytes})
endforeach()

# Hunt screen ladder: the least the screen can do, plus one thing per step.
# See tools/hunt.cpp.
set(HUNT_TARGETS "")
foreach(hstep RANGE 1 6)
    add_executable(hunt${hstep} tools/hunt.cpp src/wasm_rt.cpp)
    target_link_libraries(hunt${hstep} PUBLIC fwwasm)
    target_compile_definitions(hunt${hstep} PRIVATE HSTEP=${hstep})
    target_compile_options(hunt${hstep} PRIVATE ${WARNING_ARGS} ${WASM_COMPILER_ARGS})
    target_link_options(hunt${hstep} PRIVATE ${WASM_LINKER_ARGS})
    set_target_properties(hunt${hstep} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    )
    list(APPEND HUNT_TARGETS hunt${hstep})
endforeach()

# One hunt app per fox: single screen, single frequency, mute and exit.
# See tools/fox.cpp.
set(FOX_TARGETS "")
foreach(idx RANGE 0 4)
    math(EXPR foxnum "${idx} + 6")
    add_executable(fox${foxnum} tools/fox.cpp src/cc1101.cpp src/df.cpp src/wasm_rt.cpp)
    target_include_directories(fox${foxnum} PRIVATE src)
    target_link_libraries(fox${foxnum} PUBLIC fwwasm)
    target_compile_definitions(fox${foxnum} PRIVATE FOX_INDEX=${idx})
    target_compile_options(fox${foxnum} PRIVATE ${WARNING_ARGS} ${WASM_COMPILER_ARGS})
    target_link_options(fox${foxnum} PRIVATE ${WASM_LINKER_ARGS})
    set_target_properties(fox${foxnum} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    )
    list(APPEND FOX_TARGETS fox${foxnum})
endforeach()

# Assert no post-MVP feature crept into any object. A module declaring one is
# rejected by the device silently, so this has to be caught at build time.
find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
    foreach(target foxhunt hello probe ${IMPORT_TIER_TARGETS} ${STEP_TARGETS} ${LIMIT_TARGETS} ${RADIOCFG_TARGETS} ${HUNT_TARGETS} ${FOX_TARGETS})
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tools/verify_mvp.py
                $<TARGET_OBJECTS:${target}>
            COMMAND_EXPAND_LISTS
            VERBATIM
            COMMENT "Checking ${target} for post-MVP WASM features"
        )
    endforeach()
else()
    message(WARNING "Python3 not found - skipping post-MVP WASM feature check")
endif()
