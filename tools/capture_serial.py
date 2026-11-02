#!/usr/bin/env python3
"""Read serial without toggling RTS/DTR; capture >=30 s after a verified flash."""
import argparse
import datetime
import time
import serial

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("port", help="Previously verified T-Dongle C5 port")
parser.add_argument("output")
parser.add_argument("--seconds", type=float, default=35)
args = parser.parse_args()
if args.seconds < 30:
    parser.error("capture must last at least 30 seconds")
with open(args.output, "w", encoding="utf-8") as log:
    log.write(f"capture_start_utc={datetime.datetime.now(datetime.timezone.utc).isoformat()} port={args.port}\n")
    with serial.Serial(port=None, baudrate=115200, timeout=0.2) as port:
        port.dtr = False
        port.rts = False
        port.port = args.port
        port.open()
        start = time.monotonic()
        while time.monotonic() - start < args.seconds:
            chunk = port.read(4096)
            if chunk:
                log.write(chunk.decode("utf-8", errors="replace"))
                log.flush()
        log.write(f"\ncapture_duration_seconds={time.monotonic() - start:.3f}\n")
