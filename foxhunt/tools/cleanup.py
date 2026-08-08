#!/usr/bin/env python3
"""Lists and deletes scripts on the FreeWili.

`fwi-serial` has no flag for either, but the Python API underneath it does:
`list_current_directory()`, `change_directory()` and `remove_directory_or_file()`
are all there, the last one sending `x\\nr\\n<name>` to the board's file menu.

Bringing this app up left a lot of one-shot probes on the device - the step
ladder, the import bisector, the limit and radio-config probes, the hunt ladder.
That is around thirty files that exist only to answer a question that has since
been answered, on a board that reported 112 KB of 192 KB already in use.

Usage:
    ./cleanup.py --list                 # show what is on the board
    ./cleanup.py --probes               # delete the bring-up probes, keep the apps
    ./cleanup.py --delete step01.wasm   # delete specific files
    ./cleanup.py --probes --dry-run     # show what would go, touch nothing

Nothing is deleted without --yes or a confirmation prompt.
"""

import argparse
import fnmatch
import sys

# The bring-up diagnostics. Every one of these answered its question already,
# and the answers live in the README and in the source comments.
PROBE_PATTERNS = (
    "step*.wasm",
    "imports*.wasm",
    "limits*.wasm",
    "radiocfg*.wasm",
    "hunt*.wasm",
    "probe.wasm",
    "hello.wasm",
    "scanprobe*.wasm",
)

# The apps worth keeping.
KEEP = ("fox6.wasm", "fox7.wasm", "fox8.wasm", "fox9.wasm", "fox10.wasm")


def open_board():
    """Returns an opened FreeWiliSerial for the display processor, or None."""
    try:
        from freewili import FreeWili
    except ImportError:
        print("The freewili package is not installed: pip install freewili", file=sys.stderr)
        return None

    boards = FreeWili.find_all()
    if not boards:
        print("No FreeWili found.", file=sys.stderr)
        return None

    board = boards[0]
    serial = board.display_serial or board.main_serial
    if serial is None:
        print("Found a board but no usable serial port.", file=sys.stderr)
        return None

    serial.stay_open = True
    result = serial.open()
    if hasattr(result, "is_err") and result.is_err():
        print(f"Could not open the port: {result}", file=sys.stderr)
        return None
    return serial


def unwrap(result, what: str):
    """Returns the value inside a Result, or None after reporting the error."""
    if hasattr(result, "is_err") and result.is_err():
        print(f"{what} failed: {result.err_value}", file=sys.stderr)
        return None
    if hasattr(result, "ok_value"):
        return result.ok_value
    return result


def list_scripts(serial, directory: str) -> list:
    """Returns the FileSystemItems in `directory`.

    The listing is parsed out of the board's own file menu, so the directory has
    to be entered first - change_directory() is what puts the menu there.
    """
    if unwrap(serial.change_directory(directory), f"cd {directory}") is None:
        return []

    contents = unwrap(serial.list_current_directory(), "listing")
    if contents is None:
        return []

    # The field is `contents`, per freewili.types.FileSystemContents. Getting
    # this wrong is silent: the wrong attribute name simply yields nothing, and
    # an empty board looks identical to a parsing mistake.
    items = getattr(contents, "contents", None)
    if items is None:
        print(f"Unexpected listing shape: {contents!r}", file=sys.stderr)
        return []

    return [item for item in items if getattr(item, "name", "") not in (".", "..")]


def item_name(item) -> str:
    return getattr(item, "name", str(item))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--list", action="store_true", help="list what is on the board")
    parser.add_argument("--probes", action="store_true", help="delete the bring-up probes")
    parser.add_argument("--delete", nargs="+", metavar="NAME", help="delete these files")
    parser.add_argument("--directory", default="/scripts", help="default: /scripts")
    parser.add_argument("--dry-run", action="store_true", help="show, do not delete")
    parser.add_argument("--yes", action="store_true", help="skip the confirmation")
    args = parser.parse_args()

    if not (args.list or args.probes or args.delete):
        parser.print_help()
        return 2

    serial = open_board()
    if serial is None:
        return 1

    try:
        items = list_scripts(serial, args.directory)
        present = [item_name(i) for i in items]

        if args.list or not (args.probes or args.delete):
            print(f"{args.directory}: {len(present)} entr(y/ies)")
            for item in sorted(items, key=item_name):
                name = item_name(item)
                size = getattr(item, "size", 0)
                kind = getattr(getattr(item, "file_type", None), "name", "")
                tag = "  <- app" if name in KEEP else ""
                print(f"   {name:<24} {size:>7} bytes  {kind}{tag}")
            if args.list:
                return 0

        targets: list[str] = []
        if args.probes:
            for name in present:
                if name in KEEP:
                    continue
                if any(fnmatch.fnmatch(name, pattern) for pattern in PROBE_PATTERNS):
                    targets.append(name)
        if args.delete:
            targets.extend(args.delete)

        targets = sorted(set(targets))
        if not targets:
            print("Nothing to delete.")
            return 0

        print(f"\nWould delete {len(targets)} file(s) from {args.directory}:")
        for name in targets:
            print(f"   {name}")

        if args.dry_run:
            print("\nDry run, nothing touched.")
            return 0

        if not args.yes:
            reply = input("\nDelete these? [y/N] ").strip().lower()
            if reply not in ("y", "yes"):
                print("Cancelled.")
                return 0

        failures = 0
        for name in targets:
            result = serial.remove_directory_or_file(name)
            ok = not (hasattr(result, "is_err") and result.is_err())
            print(f"   {'removed' if ok else 'FAILED '} {name}")
            if not ok:
                failures += 1

        print(f"\n{len(targets) - failures} removed, {failures} failed.")
        return 1 if failures else 0
    finally:
        try:
            serial.close(False)
        except Exception:  # noqa: BLE001 - closing is best effort
            pass


if __name__ == "__main__":
    sys.exit(main())
