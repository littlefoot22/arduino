#!/usr/bin/env bash
# Builds and runs the host-side logic tests. No FreeWili and no WASI SDK needed
# - these cover only the radio maths and DF filters, which are plain C++.
set -euo pipefail

cd "$(dirname "$0")"
out=$(mktemp -d)
trap 'rm -rf "$out"' EXIT

${CXX:-g++} -std=c++23 -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion \
    -Wfloat-equal -Wold-style-cast -Werror -O2 \
    -I src \
    tests/test_logic.cpp src/cc1101.cpp src/df.cpp \
    -o "$out/test_logic"

"$out/test_logic"
