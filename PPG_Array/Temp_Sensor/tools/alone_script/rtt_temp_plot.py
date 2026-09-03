#!/usr/bin/env python3
"""Plot AS6214 temperature over time - live from SEGGER RTT, or from a CSV.

Two modes:

  live (default): connect to the J-Link, read the RTT CSV stream on channel 1,
                  and plot temperature in real time. The same
                  pc_time,uptime_s,temperature_C CSV is also saved beside this
                  script (unless --no-save), so this is a superset of
                  rtt_temp_logger.py with a live graph.

  file:           --csv PATH plots a previously captured CSV. No hardware or
                  pylink needed - handy for reviewing past runs.

The window is a normal matplotlib GUI: use its toolbar to zoom, pan, and save
the graph as a PNG. Close the window (or Ctrl+C) to stop.

Requirements:
    pip install matplotlib                 # both modes
    pip install pylink-square              # live mode only (+ SEGGER J-Link SW)

Usage (from PPG_Array/Temp_Sensor):
    python tools/rtt_temp_plot.py
    python tools/rtt_temp_plot.py --no-save
    python tools/rtt_temp_plot.py --csv tools/temperature_20260816_185045.csv
"""

import argparse
import csv as _csv
import datetime as _dt
import os
import sys

try:
    import matplotlib.pyplot as plt
    import matplotlib.dates as mdates
    from matplotlib.animation import FuncAnimation
except ImportError:
    sys.exit("error: matplotlib not installed. Run:  pip install matplotlib")

# Directory this script lives in; CSVs default to here.
_SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))


def parse_args():
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--csv", default=None,
                   help="Plot this existing CSV instead of capturing live "
                        "(no J-Link needed).")
    p.add_argument("--device", default="NRF52840_XXAA",
                   help="J-Link target device. Default: NRF52840_XXAA.")
    p.add_argument("--interface", default="SWD", choices=["SWD", "JTAG"],
                   help="Debug interface. Default: SWD.")
    p.add_argument("--speed", type=int, default=4000,
                   help="Interface speed in kHz. Default: 4000.")
    p.add_argument("--channel", type=int, default=1,
                   help="RTT up-buffer carrying the CSV. Default: 1.")
    p.add_argument("--serial", type=int, default=None,
                   help="J-Link serial number (only if several are attached).")
    p.add_argument("--jlink-dll", default=None,
                   help="Path to a specific JLinkARM.dll, if pylink loads an old one.")
    p.add_argument("--output", default=None,
                   help="Live-capture CSV path. Bare name -> beside this script. "
                        "Default: temperature_<timestamp>.csv beside the script.")
    p.add_argument("--no-save", action="store_true",
                   help="Live mode: plot only, do not write a CSV.")
    p.add_argument("--search-start", default="0x20000000",
                   help="RTT control-block search start. Default: 0x20000000.")
    p.add_argument("--search-length", default="0x40000",
                   help="RTT control-block search length. Default: 0x40000.")
    return p.parse_args()


def parse_temp_row(line):
    """Parse one firmware CSV data line 'uptime_s,temperature_C'.

    Returns (uptime_s:int, temperature_C:float) or None for header/blank/bad
    lines (a partially written row is simply ignored until the next one)."""
    line = line.strip().strip("\r")
    if not line or line.startswith("uptime_s"):
        return None
    parts = line.split(",")
    if len(parts) < 2:
        return None
    try:
        return int(parts[-2]), float(parts[-1])
    except ValueError:
        return None


# --------------------------------------------------------------------------- #
# File mode
# --------------------------------------------------------------------------- #
def plot_csv(path):
    uptimes, temps = [], []
    with open(path, newline="", encoding="utf-8") as f:
        for row in _csv.reader(f):
            if not row:
                continue
            # parse_temp_row ignores the header line and any partial rows.
            parsed = parse_temp_row(",".join(row))
            if parsed:
                uptimes.append(parsed[0])
                temps.append(parsed[1])

    if not temps:
        sys.exit("error: no temperature rows found in {}".format(path))

    fig, ax = plt.subplots()
    ax.plot(uptimes, temps, "-o", ms=3, color="#d1495b")
    ax.set_xlabel("Uptime (s)")
    ax.set_ylabel("Temperature (°C)")
    ax.set_title("{}  |  {} samples".format(os.path.basename(path), len(temps)))
    ax.grid(True, alpha=0.3)
    fig.tight_layout()
    print("Plotted {} samples from {}".format(len(temps), path))
    plt.show()


# --------------------------------------------------------------------------- #
# Live mode
# --------------------------------------------------------------------------- #
def connect_jlink(args):
    try:
        import pylink
    except ImportError:
        sys.exit("error: pylink not installed. Run:  pip install pylink-square\n"
                 "(or use --csv to plot a saved file without hardware)")

    iface = (pylink.enums.JLinkInterfaces.SWD if args.interface == "SWD"
             else pylink.enums.JLinkInterfaces.JTAG)

    if args.jlink_dll:
        jlink = pylink.JLink(lib=pylink.library.Library(dllpath=args.jlink_dll))
    else:
        jlink = pylink.JLink()

    print("Opening J-Link...")
    jlink.open(serial_no=args.serial)
    jlink.set_tif(iface)
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
            "Almost always environmental: close any SES debug session / RTT\n"
            "Viewer holding the probe, confirm the board is powered on the\n"
            "debug USB port, and sanity-check with:\n"
            "  JLink.exe -Device {} -If {} -Speed {} -AutoConnect 1".format(
                exc, args.device, args.interface, args.speed))

    jlink.exec_command("SetRTTSearchRanges {} {}".format(
        args.search_start, args.search_length))
    jlink.rtt_start(None)

    print("Waiting for RTT control block...")
    import time
    for _ in range(100):  # up to ~10 s
        try:
            if jlink.rtt_get_num_up_buffers() > args.channel:
                break
        except pylink.errors.JLinkRTTException:
            pass
        time.sleep(0.1)
    else:
        jlink.close()
        sys.exit("error: RTT channel {} not found. Is the firmware running and "
                 "flashed with the SoftDevice?".format(args.channel))
    return jlink


def run_live(args):
    jlink = connect_jlink(args)

    # Optional CSV capture (same format/location as rtt_temp_logger.py).
    csv_file = None
    if not args.no_save:
        if args.output:
            out_path = (args.output if os.path.isabs(args.output)
                        else os.path.join(_SCRIPT_DIR, args.output))
        else:
            out_path = os.path.join(
                _SCRIPT_DIR, "temperature_{}.csv".format(
                    _dt.datetime.now().strftime("%Y%m%d_%H%M%S")))
        csv_file = open(out_path, "w", newline="", encoding="utf-8")
        csv_file.write("pc_time,uptime_s,temperature_C\n")
        csv_file.flush()
        print("Saving CSV to {}".format(out_path))

    print("Plotting live. Close the window (or Ctrl+C) to stop.\n")

    times, temps = [], []
    line_buf = {"s": ""}

    fig, ax = plt.subplots()
    (line2d,) = ax.plot([], [], "-o", ms=3, color="#d1495b")
    ax.set_xlabel("Time")
    ax.set_ylabel("Temperature (°C)")
    ax.set_title("AS6214 - waiting for first sample...")
    ax.grid(True, alpha=0.3)
    ax.xaxis.set_major_formatter(mdates.DateFormatter("%H:%M:%S"))
    fig.autofmt_xdate()

    def update(_frame):
        data = jlink.rtt_read(args.channel, 1024)
        if data:
            line_buf["s"] += bytes(data).decode("ascii", errors="replace")
            while "\n" in line_buf["s"]:
                raw, line_buf["s"] = line_buf["s"].split("\n", 1)
                parsed = parse_temp_row(raw)
                if parsed is None:
                    continue
                uptime_s, temp = parsed
                now = _dt.datetime.now()
                # Store as a matplotlib date number so DateFormatter on the x
                # axis renders it (set_data on an initially-empty line does not
                # auto-establish date units from raw datetime objects).
                times.append(mdates.date2num(now))
                temps.append(temp)
                if csv_file:
                    csv_file.write("{},{},{}\n".format(
                        now.strftime("%H:%M:%S"), uptime_s, parts_fmt(temp, raw)))
                    csv_file.flush()

        if times:
            line2d.set_data(times, temps)
            ax.relim()
            ax.autoscale_view()
            ax.set_title("AS6214  |  {:.4f} °C  |  {} samples".format(
                temps[-1], len(temps)))
        return (line2d,)

    # Keep a reference so the animation is not garbage-collected.
    _anim = FuncAnimation(fig, update, interval=500, blit=False,
                          cache_frame_data=False)
    try:
        plt.show()
    except KeyboardInterrupt:
        pass
    finally:
        if csv_file:
            csv_file.close()
        try:
            jlink.rtt_stop()
        except Exception:
            pass
        jlink.close()
        print("Stopped.")


def parts_fmt(temp, raw):
    """Prefer the exact temperature text the firmware sent (preserves its 4dp
    formatting); fall back to the parsed float if the raw line is unusual."""
    raw = raw.strip().strip("\r")
    tail = raw.split(",")[-1]
    return tail if tail else "{:.4f}".format(temp)


def resolve_csv(path):
    """Accept a path as given (cwd-relative or absolute); if not found, also
    look beside this script (where live captures are written)."""
    if os.path.isabs(path) or os.path.exists(path):
        return path
    beside = os.path.join(_SCRIPT_DIR, path)
    return beside if os.path.exists(beside) else path


def main():
    args = parse_args()
    if args.csv:
        plot_csv(resolve_csv(args.csv))
    else:
        run_live(args)


if __name__ == "__main__":
    main()
