#!/usr/bin/env python3
"""Capture the AS6214 temperature CSV stream over SEGGER RTT, stamping each
row with the host PC's wall-clock time.

The firmware streams rows "uptime_s,temperature_C" on RTT channel 1. This tool
attaches to the J-Link, reads that channel, and writes a CSV with an added
leading column:

    pc_time,uptime_s,temperature_C
    18:42:03,0,24.7031
    18:42:13,10,24.7109
    ...

pc_time is the local wall-clock time of day (no date) at the instant the row
arrived at the host (samples are 10 s apart and pushed promptly, so this tracks
real time closely). The CSV is written next to this script by default.

Why this tool instead of JLinkRTTLogger: the raw logger writes only what the
firmware sent (no wall-clock), and it needs the control-block search range
passed by hand. This handles both, and prints live to the console.

Requirements:
    pip install pylink-square
    SEGGER J-Link software installed (provides the JLinkARM DLL that pylink loads).

Usage (from PPG_Array/Temp_Sensor):
    python tools/rtt_temp_logger.py
    python tools/rtt_temp_logger.py --output my_run.csv --device NRF52840_XXAA

Stop with Ctrl+C; the CSV is flushed after every row, so a partial capture is
always valid.
"""

import argparse
import datetime as _dt
import os
import sys
import time

# Directory this script lives in; CSVs default to here so a run always lands
# next to the tool regardless of the current working directory.
_SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))

try:
    import pylink
except ImportError:
    sys.exit(
        "error: pylink not installed. Run:  pip install pylink-square\n"
        "(and make sure the SEGGER J-Link software is installed)"
    )


def parse_args():
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--device", default="NRF52840_XXAA",
                   help="J-Link target device (physical silicon on the PCA10056). "
                        "Default: NRF52840_XXAA.")
    p.add_argument("--interface", default="SWD", choices=["SWD", "JTAG"],
                   help="Debug interface. Default: SWD.")
    p.add_argument("--speed", type=int, default=4000,
                   help="Interface speed in kHz. Default: 4000.")
    p.add_argument("--channel", type=int, default=1,
                   help="RTT up-buffer (channel) carrying the CSV. Default: 1.")
    p.add_argument("--serial", type=int, default=None,
                   help="J-Link serial number (only needed if several are attached).")
    p.add_argument("--jlink-dll", default=None,
                   help="Path to a specific JLinkARM.dll to load, e.g. "
                        r'"C:\Program Files\SEGGER\JLink\JLinkARM.dll". Use if '
                        "pylink auto-loads an old DLL that fails to connect.")
    p.add_argument("--output", default=None,
                   help="Output CSV path. A bare filename is placed next to this "
                        "script; an absolute path is honored as-is. "
                        "Default: temperature_<timestamp>.csv beside the script.")
    # The S112 SoftDevice pushes the RTT control block high in RAM, so RTT
    # auto-detect can latch onto a zero block. Give it an explicit range.
    p.add_argument("--search-start", default="0x20000000",
                   help="RTT control-block search start address. Default: 0x20000000.")
    p.add_argument("--search-length", default="0x40000",
                   help="RTT control-block search length. Default: 0x40000 (256 KiB).")
    return p.parse_args()


def now_stamp():
    # Time of day only (no date), whole-second resolution: "18:42:03".
    # Milliseconds are deliberately omitted: samples are 10 s apart so they add
    # nothing, and a fractional-second value makes Excel auto-format the column
    # as mm:ss and hide the hours. Whole seconds display as HH:MM:SS correctly.
    return _dt.datetime.now().strftime("%H:%M:%S")


def main():
    args = parse_args()

    if args.output:
        # Bare filename -> beside the script; absolute/relative path -> honored.
        out_path = (args.output if os.path.isabs(args.output)
                    else os.path.join(_SCRIPT_DIR, args.output))
    else:
        out_path = os.path.join(
            _SCRIPT_DIR,
            "temperature_{}.csv".format(_dt.datetime.now().strftime("%Y%m%d_%H%M%S")))

    iface = (pylink.enums.JLinkInterfaces.SWD if args.interface == "SWD"
             else pylink.enums.JLinkInterfaces.JTAG)

    if args.jlink_dll:
        jlink = pylink.JLink(lib=pylink.library.Library(dllpath=args.jlink_dll))
    else:
        jlink = pylink.JLink()
    print("Opening J-Link...")
    jlink.open(serial_no=args.serial)
    jlink.set_tif(iface)

    # Report what actually got loaded/attached - helps diagnose the common
    # "Unspecified error" on connect, which is almost always environmental.
    try:
        print("  JLinkARM.dll : {}".format(jlink.version))
        print("  Probe        : {} (firmware: {})".format(
            jlink.product_name, jlink.firmware_version))
    except Exception:
        pass

    print("Connecting to {} @ {} kHz over {}...".format(
        args.device, args.speed, args.interface))
    try:
        jlink.connect(args.device, speed=args.speed)
    except pylink.errors.JLinkException as exc:
        jlink.close()
        sys.exit(
            "\nerror: could not connect to the target ({}).\n"
            "This is almost always environmental, not the sensor. Check:\n"
            "  1. Close anything else using the J-Link - a SEGGER Embedded\n"
            "     Studio debug session (stop it, leave the board powered),\n"
            "     J-Link RTT Viewer, Ozone, or J-Link Commander.\n"
            "  2. Confirm the board is powered and USB is on the PCA10056\n"
            "     debug port.\n"
            "  3. Sanity-check the probe from a terminal:\n"
            "       JLink.exe -Device {} -If {} -Speed {} -AutoConnect 1\n"
            "     If that also fails, it's the probe/target/power (or nRF52\n"
            "     APPROTECT), not this script.\n"
            "  4. If JLink.exe works but this doesn't, pylink loaded an old\n"
            "     JLinkARM.dll; pass the current one with --jlink-dll or set\n"
            "     it via the J-Link install.".format(
                exc, args.device, args.interface, args.speed))

    # Constrain the RTT control-block search BEFORE starting RTT (HANDOFF gotcha).
    jlink.exec_command("SetRTTSearchRanges {} {}".format(
        args.search_start, args.search_length))
    jlink.rtt_start(None)

    # Wait for the firmware's RTT control block to be located.
    print("Waiting for RTT control block...")
    for _ in range(100):  # up to ~10 s
        try:
            if jlink.rtt_get_num_up_buffers() > args.channel:
                break
        except pylink.errors.JLinkRTTException:
            pass
        time.sleep(0.1)
    else:
        jlink.close()
        sys.exit("error: RTT control block / channel {} not found. Is the "
                 "firmware running and flashed with the SoftDevice?".format(args.channel))

    print("Attached. Logging channel {} to {}".format(args.channel, out_path))
    print("Press Ctrl+C to stop.\n")

    line_buf = ""
    header_written = False
    try:
        with open(out_path, "w", newline="", encoding="utf-8") as f:
            while True:
                data = jlink.rtt_read(args.channel, 1024)
                if not data:
                    time.sleep(0.05)
                    continue

                line_buf += bytes(data).decode("ascii", errors="replace")

                # Emit each complete line as it arrives.
                while "\n" in line_buf:
                    raw_line, line_buf = line_buf.split("\n", 1)
                    line = raw_line.strip("\r").strip()
                    if not line:
                        continue

                    # The firmware header row: replace with the combined header.
                    if line.startswith("uptime_s"):
                        if not header_written:
                            out = "pc_time,{}".format(line)
                            f.write(out + "\n")
                            f.flush()
                            print(out)
                            header_written = True
                        continue

                    stamp = now_stamp()
                    if not header_written:
                        # Data arrived before we saw a header (attached late) -
                        # synthesise the header so the file is well formed.
                        f.write("pc_time,uptime_s,temperature_C\n")
                        header_written = True
                    out = "{},{}".format(stamp, line)
                    f.write(out + "\n")
                    f.flush()
                    print(out)
    except KeyboardInterrupt:
        print("\nStopped. CSV saved to {}".format(out_path))
    finally:
        try:
            jlink.rtt_stop()
        except Exception:
            pass
        jlink.close()


if __name__ == "__main__":
    main()
