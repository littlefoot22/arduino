#!/usr/bin/env python3
"""Dump whatever the FreeWili emits on its serial ports.

The board has two USB serial ports, one per processor. The display processor
runs the WASM engine, so that is usually the interesting one - if the firmware
reports anything about a module it refused to load, it comes out there.

Usage:
    ./monitor.py              # find the board, dump both ports
    ./monitor.py --display    # display processor only
    ./monitor.py --main       # main processor only
    ./monitor.py --port /dev/cu.usbmodem1234   # explicit port

Needs pyserial (`pip install pyserial`). Uses the freewili package to locate
the ports when it is installed, and falls back to scanning for USB serial
devices when it is not.

Run this in one terminal, then load the app from another:
    fwi-serial -s dist/foxhunt.wasm -w foxhunt.wasm

Note both tools want the same port. If the load fails while this is running,
stop it, load the app, then start this and relaunch the app from the board's
own menu instead.
"""

import argparse
import sys
import threading


def find_ports() -> dict[str, str]:
    """Returns {label: device} for whatever FreeWili ports can be located."""
    found: dict[str, str] = {}

    try:
        from freewili import FreeWili  # type: ignore

        for board in FreeWili.find_all():
            for label in ("main", "display"):
                usb = getattr(board, label, None)
                port = getattr(usb, "port", None)
                if port:
                    found[label] = port
    except Exception as exc:  # noqa: BLE001 - any failure just means fall back
        print(f"(freewili package unavailable or failed: {exc})", file=sys.stderr)

    if not found:
        try:
            from serial.tools import list_ports

            for info in list_ports.comports():
                blob = f"{info.description} {info.manufacturer} {info.product}".lower()
                if "wili" in blob or "usbmodem" in info.device or "ACM" in info.device:
                    found[info.device] = info.device
        except Exception as exc:  # noqa: BLE001
            print(f"(port scan failed: {exc})", file=sys.stderr)

    return found


def pump(label: str, port: str, baud: int) -> None:
    """Reads one port forever, printing every line with its source label."""
    import serial

    try:
        handle = serial.Serial(port, baud, timeout=0.2)
    except Exception as exc:  # noqa: BLE001
        print(f"[{label}] cannot open {port}: {exc}", file=sys.stderr)
        return

    print(f"[{label}] listening on {port} at {baud}", file=sys.stderr)
    buffer = bytearray()
    with handle:
        while True:
            try:
                chunk = handle.read(256)
            except Exception as exc:  # noqa: BLE001
                print(f"[{label}] read failed: {exc}", file=sys.stderr)
                return
            if not chunk:
                continue
            buffer.extend(chunk)
            while b"\n" in buffer:
                line, _, rest = buffer.partition(b"\n")
                buffer = bytearray(rest)
                text = line.decode("utf-8", "replace").rstrip("\r")
                if text:
                    print(f"[{label}] {text}", flush=True)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", help="explicit serial device to read")
    parser.add_argument("--main", action="store_true", help="main processor only")
    parser.add_argument("--display", action="store_true", help="display processor only")
    parser.add_argument("--baud", type=int, default=115200)
    args = parser.parse_args()

    if args.port:
        targets = {"port": args.port}
    else:
        targets = find_ports()
        if args.main:
            targets = {k: v for k, v in targets.items() if k == "main"}
        elif args.display:
            targets = {k: v for k, v in targets.items() if k == "display"}

    if not targets:
        print("No FreeWili serial ports found. Pass --port explicitly.", file=sys.stderr)
        return 1

    threads = [
        threading.Thread(target=pump, args=(label, port, args.baud), daemon=True)
        for label, port in targets.items()
    ]
    for thread in threads:
        thread.start()

    try:
        while True:
            for thread in threads:
                thread.join(0.5)
    except KeyboardInterrupt:
        return 0


if __name__ == "__main__":
    sys.exit(main())
