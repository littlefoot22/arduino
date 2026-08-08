#!/usr/bin/env bash
# Builds foxhunt.wasm.
#
# Needs the WASI SDK (https://github.com/WebAssembly/wasi-sdk/releases) with
# WASI_SDK_PATH pointing at it, plus cmake and git.
set -euo pipefail

cd "$(dirname "$0")"

if [[ -z "${WASI_SDK_PATH:-}" ]]; then
    echo "WASI_SDK_PATH is not set." >&2
    echo "Download the WASI SDK and point it there, e.g.:" >&2
    echo "  export WASI_SDK_PATH=/opt/wasi-sdk-33.0-x86_64-linux" >&2
    exit 1
fi

toolchain="${WASI_SDK_PATH}/share/cmake/wasi-sdk.cmake"
if [[ ! -f "$toolchain" ]]; then
    echo "No toolchain file at $toolchain - is WASI_SDK_PATH correct?" >&2
    exit 1
fi

cmake -DCMAKE_TOOLCHAIN_FILE="$toolchain" -B build .
cmake --build build

echo
echo "Built: $(pwd)/build/foxhunt.wasm"
echo
echo "Flash and run it with:"
echo "  fwi-serial -s build/foxhunt.wasm -w foxhunt.wasm"
