#!/usr/bin/env python3
"""Fail the build if a WebAssembly object declares a post-MVP feature.

The FreeWili's interpreter rejects a module that declares features it does not
implement, and it does so silently: the module fails validation, never runs, and
nothing is reported on the device or the wire. The board just sits there. That
cost a long debugging session once, so it is asserted here instead.

Object files are checked rather than the linked .wasm because the final link
passes --strip-all, which removes the target_features custom section along with
everything else.

Usage:  verify_mvp.py <file.obj|file.wasm> ...
"""

import pathlib
import sys

# Empty: this project targets the WebAssembly MVP exactly. bulk-memory is
# deliberately absent - the vendor examples pass -mbulk-memory, but on this
# hardware a bulk-memory module is rejected the same silent way, while an
# otherwise identical pure-MVP module runs.
ALLOWED: frozenset[str] = frozenset()

SECTION_NAME = b"target_features"


def read_uleb(data: bytes, pos: int) -> tuple[int, int]:
    """Reads an unsigned LEB128 integer, returning (value, next position)."""
    result = 0
    shift = 0
    while pos < len(data):
        byte = data[pos]
        pos += 1
        result |= (byte & 0x7F) << shift
        if not byte & 0x80:
            break
        shift += 7
    return result, pos


def features_in(data: bytes) -> set[str]:
    """Extracts every feature name declared in the target_features section."""
    found: set[str] = set()
    at = data.find(SECTION_NAME)
    if at < 0:
        return found

    pos = at + len(SECTION_NAME)
    count, pos = read_uleb(data, pos)

    # Each entry is a prefix byte ('+', '-' or '='), then a length-prefixed name.
    for _ in range(count):
        if pos >= len(data):
            break
        prefix = data[pos]
        pos += 1
        length, pos = read_uleb(data, pos)
        name = data[pos : pos + length]
        pos += length
        try:
            decoded = name.decode("utf-8")
        except UnicodeDecodeError:
            continue
        # '-' means the feature is explicitly disallowed, which is not a problem.
        if prefix != ord("-"):
            found.add(decoded)
    return found


def main(argv: list[str]) -> int:
    if len(argv) < 2:
        print(__doc__, file=sys.stderr)
        return 2

    failures: list[str] = []
    checked = 0

    for arg in argv[1:]:
        path = pathlib.Path(arg)
        if not path.is_file():
            continue
        declared = features_in(path.read_bytes())
        checked += 1
        offenders = sorted(declared - ALLOWED)
        if offenders:
            failures.append(f"  {path.name}: {', '.join(offenders)}")

    if failures:
        print("", file=sys.stderr)
        print("ERROR: post-MVP WebAssembly features declared.", file=sys.stderr)
        print("The FreeWili interpreter will reject this module silently -", file=sys.stderr)
        print("it will not run and will report no error.", file=sys.stderr)
        print("", file=sys.stderr)
        print("\n".join(failures), file=sys.stderr)
        print("", file=sys.stderr)
        print("Check that -mcpu=mvp and -nostdlib are both being passed.", file=sys.stderr)
        print("", file=sys.stderr)
        return 1

    print(f"verify_mvp: {checked} object(s) clean - pure MVP")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
