#!/usr/bin/env python3
"""Log AS6214 temperature and Rigol DM3068 DC voltage to one CSV.

The firmware drives the cadence. Every time a temperature line arrives on RTT
channel 1, the DMM is read immediately and one row is written:

    pc_time,elapsed_s,temperature_C,voltage_V

The CSV is created beside this script. Ctrl+C to stop.

Requirements:
    pip install pylink-square pyvisa      (+ SEGGER J-Link SW, NI-VISA)
"""

import datetime as _dt
import os
import sys
import time

try:
    import pylink
except ImportError:
    sys.exit("error: pylink not installed. Run:  pip install pylink-square")
try:
    import pyvisa
except ImportError:
    sys.exit("error: pyvisa not installed. Run:  pip install pyvisa")

# --------------------------------------------------------------------------- #
# Settings
# --------------------------------------------------------------------------- #
DEVICE = "NRF52840_XXAA"
INTERFACE = "SWD"
SPEED = 4000              # kHz
RTT_CHANNEL = 1
SEARCH_START = "0x20000000"
SEARCH_LENGTH = "0x40000"

RESOURCE = "USB0::0x1AB1::0x0C94::DM3O163050366::INSTR"
VISA_TIMEOUT_MS = 5000
DMM_RETRIES = 3

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))


# --------------------------------------------------------------------------- #
# J-Link / RTT
# --------------------------------------------------------------------------- #
def connect_jlink():
    jlink = pylink.JLink()
    print("Opening J-Link...")
    jlink.open()
    jlink.set_tif(pylink.enums.JLinkInterfaces.SWD if INTERFACE == "SWD"
                  else pylink.enums.JLinkInterfaces.JTAG)

    print("Connecting to {} @ {} kHz over {}...".format(DEVICE, SPEED, INTERFACE))
    try:
        jlink.connect(DEVICE, speed=SPEED)
    except pylink.errors.JLinkException as exc:
        jlink.close()
        sys.exit("error: could not connect to the target ({}).\n"
                 "Close any SES debug session or RTT Viewer holding the probe "
                 "and check the board is powered.".format(exc))

    jlink.exec_command("SetRTTSearchRanges {} {}".format(
        SEARCH_START, SEARCH_LENGTH))
    jlink.rtt_start(None)

    print("Waiting for RTT control block...")
    for _ in range(100):  # up to ~10 s
        try:
            if jlink.rtt_get_num_up_buffers() > RTT_CHANNEL:
                break
        except pylink.errors.JLinkRTTException:
            pass
        time.sleep(0.1)
    else:
        jlink.close()
        sys.exit("error: RTT channel {} not found. Is the firmware "
                 "running?".format(RTT_CHANNEL))
    return jlink


def parse_temp_row(line):
    """'uptime_s,temperature_C' -> temperature text, or None for header,
    blank, and partially written lines."""
    line = line.strip().strip("\r")
    if not line or line.startswith("uptime_s"):
        return None
    parts = line.split(",")
    if len(parts) < 2:
        return None
    try:
        float(parts[-1])
    except ValueError:
        return None
    return parts[-1]  # keep the firmware's own 4dp text


def flush_backlog(jlink, seconds=0.5):
    """Drop whatever the target buffered before we attached. Those lines all
    arrive at once, so their PC timestamps would be meaningless."""
    end = time.monotonic() + seconds
    dropped = 0
    while time.monotonic() < end:
        data = jlink.rtt_read(RTT_CHANNEL, 4096)
        if data:
            dropped += bytes(data).count(b"\n")
        time.sleep(0.02)
    if dropped:
        print("Discarded {} buffered line(s) from before attach.".format(dropped))


# --------------------------------------------------------------------------- #
# DMM
# --------------------------------------------------------------------------- #
class Dmm:
    def __init__(self):
        self.rm = pyvisa.ResourceManager()
        print("Resources detected: {}".format(self.rm.list_resources()))
        self.inst = None
        self.open()

    def open(self):
        print("Opening VISA session to {}...".format(RESOURCE))
        self.inst = self.rm.open_resource(RESOURCE, open_timeout=VISA_TIMEOUT_MS)
        self.inst.timeout = VISA_TIMEOUT_MS
        self.inst.write_termination = "\n"
        self.inst.read_termination = "\n"
        print("Connected instrument: {}".format(self.inst.query("*IDN?").strip()))
        self.inst.write(":FUNCtion:VOLTage:DC")
        time.sleep(0.1)
        print("First reading: {} V".format(self.inst.query(
            ":MEASure:VOLTage:DC?").strip()))

    def read(self):
        """DC volts, or None if the meter will not answer. A dead read logs an
        empty field rather than ending a long run."""
        for attempt in range(1, DMM_RETRIES + 1):
            try:
                return float(self.inst.query(":MEASure:VOLTage:DC?"))
            except (pyvisa.errors.VisaIOError, ValueError) as exc:
                print("  DMM read failed ({}/{}): {}".format(
                    attempt, DMM_RETRIES, exc))
                if attempt == DMM_RETRIES:
                    return None
                try:
                    self.inst.close()
                    time.sleep(1.0)
                    self.open()
                except Exception as exc2:
                    print("  reopen failed: {}".format(exc2))
        return None

    def close(self):
        for obj in (self.inst, self.rm):
            try:
                obj.close()
            except Exception:
                pass


# --------------------------------------------------------------------------- #
# Main loop
# --------------------------------------------------------------------------- #
def main():
    try:
        sys.stdout.reconfigure(line_buffering=True)
    except Exception:
        pass

    jlink = connect_jlink()
    try:
        dmm = Dmm()
    except Exception:
        jlink.close()
        raise

    path = os.path.join(SCRIPT_DIR, "temp_voltage_{}.csv".format(
        _dt.datetime.now().strftime("%Y%m%d_%H%M%S")))
    csv_file = open(path, "w", newline="", encoding="utf-8")
    csv_file.write("pc_time,elapsed_s,temperature_C,voltage_V\n")
    csv_file.flush()
    print("Logging to {}".format(path))

    flush_backlog(jlink)
    print("Waiting for temperature lines. Ctrl+C to stop.\n")

    buf = ""
    t0 = None
    count = 0
    try:
        while True:
            data = jlink.rtt_read(RTT_CHANNEL, 4096)
            if not data:
                time.sleep(0.05)
                continue

            buf += bytes(data).decode("ascii", errors="replace")
            while "\n" in buf:
                raw, buf = buf.split("\n", 1)
                temp_text = parse_temp_row(raw)
                if temp_text is None:
                    continue

                volts = dmm.read()
                now = _dt.datetime.now()
                if t0 is None:
                    t0 = time.monotonic()
                elapsed = time.monotonic() - t0

                csv_file.write("{},{:.1f},{},{}\n".format(
                    now.strftime("%Y-%m-%d %H:%M:%S"), elapsed, temp_text,
                    "" if volts is None else "{:.5f}".format(volts)))
                csv_file.flush()
                count += 1
                print("{}  {:8.1f} s  {:>10} C  {:>12}".format(
                    now.strftime("%H:%M:%S"), elapsed, temp_text,
                    "-" if volts is None else "{:.5f} V".format(volts)))
    except KeyboardInterrupt:
        print("\nStopping.")
    finally:
        csv_file.close()
        try:
            jlink.rtt_stop()
        except Exception:
            pass
        jlink.close()
        dmm.close()
        print("Wrote {} samples to {}".format(count, path))


if __name__ == "__main__":
    main()