#!/usr/bin/env bash
# Rebuilds and refreshes the checked-in binary at dist/foxhunt.wasm.
#
# That file is committed so the app can be flashed from a machine with no WASI
# SDK. Run this after changing anything under src/, then commit dist/.
set -euo pipefail

cd "$(dirname "$0")"

./run_tests.sh
./build.sh

mkdir -p dist
cp build/foxhunt.wasm dist/foxhunt.wasm
cp build/probe.wasm dist/probe.wasm
cp build/hello.wasm dist/hello.wasm
cp build/imports*.wasm dist/
cp build/step*.wasm dist/
cp build/limits*.wasm dist/

echo
echo "dist/foxhunt.wasm refreshed"
sha256sum dist/*.wasm
