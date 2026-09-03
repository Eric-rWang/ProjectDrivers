import pyvisa
import time
from time import sleep
import os

SAMPLE_INTERVAL_SECONDS = 30
RUN_DURATION_SECONDS = 48 * 60 * 60
TOTAL_SAMPLES = RUN_DURATION_SECONDS // SAMPLE_INTERVAL_SECONDS
RESOURCE_NAME = 'USB0::0x1AB1::0x0C94::DM3O163050366::INSTR'
MAX_RETRIES_PER_SAMPLE = 3
RETRY_DELAY_SECONDS = 2

# Datalogging: create a time-stamped file
dateString = time.strftime("%Y-%m-%d_%H%M")
filepath = "./" + dateString + ".csv"

rm = pyvisa.ResourceManager()
# List all connected resources
print("Resources detected\n{}\n".format(rm.list_resources()))

print("VISA backend: {}".format(rm.visalib))

def open_and_configure_dmm():
    dmm = rm.open_resource(RESOURCE_NAME)
    dmm.timeout = 5000
    dmm.write_termination = "\n"
    dmm.read_termination = "\n"

    try:
        instrument_id = dmm.query('*IDN?').strip()
        print("Connected instrument: {}".format(instrument_id))
    except pyvisa.errors.VisaIOError as e:
        raise RuntimeError(
            "Connected resource was found but *IDN? failed. "
            "Close NI MAX/UltraSigma and any other app using the DMM, then retry. "
            "If still failing, reinstall NI-VISA runtime. Original VISA error: {}".format(e)
        )

    try:
        dmm.write(':FUNCtion:VOLTage:DC')
    except pyvisa.errors.VisaIOError:
        # Some firmwares are stricter about function syntax.
        dmm.write(':FUNCtion "VOLT:DC"')

    return dmm


dmm = open_and_configure_dmm()

# Run the test
sec = 0
while sec < TOTAL_SAMPLES: # 5760 samples * 30 sec = 48 hours
    sleep(SAMPLE_INTERVAL_SECONDS)

    retry_count = 0
    while True:
        try:
            vMeasured = float(dmm.query(':MEASure:VOLTage:DC?'))
            break
        except (pyvisa.errors.VisaIOError, ValueError) as e:
            retry_count += 1
            print(
                "Read failed (attempt {}/{}): {}".format(
                    retry_count, MAX_RETRIES_PER_SAMPLE, e
                )
            )
            if retry_count >= MAX_RETRIES_PER_SAMPLE:
                raise RuntimeError(
                    "Measurement failed after {} retries at t={}s".format(
                        MAX_RETRIES_PER_SAMPLE, sec * SAMPLE_INTERVAL_SECONDS
                    )
                )

            try:
                dmm.close()
            except Exception:
                pass

            sleep(RETRY_DELAY_SECONDS)
            dmm = open_and_configure_dmm()

    # Write results to console
    elapsed_seconds = sec * SAMPLE_INTERVAL_SECONDS
    print("{}  {}".format(elapsed_seconds, vMeasured))

    # Write results to a file
    with open(filepath, "a") as file:
        if os.stat(filepath).st_size == 0: # if empty file, write a nice header
            file.write("Time [s], Measured [V]\n")
        file.write("{:12.2f},{:13.5f}\n".format(elapsed_seconds, vMeasured)) # log the data

    sec += 1

try:
    dmm.close()
finally:
    rm.close()
