# Handoff — AS6214 Temperature Sensor Logger

Context for a new Claude session continuing this work. Written 2026-08-16.

## Goal
Drivers to read the **ams AS6214** I²C temperature sensor on a **PCA10056** dev
board (SEGGER Embedded Studio), sample every 10 s, convert raw → °C, and export
the converted data live as CSV. **This is done and builds clean.**

## What was built
New driver + wiring in `PPG_Array/Temp_Sensor`:

| File | Change |
|------|--------|
| `my_drivers/include/as6214_driver.h` | **new** — AS6214 API + register defs |
| `my_drivers/src/as6214_driver.c` | **new** — read/convert/CSV-over-RTT |
| `main.c` | rewritten — init I²C + sensor, sample every 10 s, stream CSV |
| `my_drivers/include/i2c_driver.h` | I²C pins → **SCL=P0.27, SDA=P0.26** |
| `my_drivers/src/i2c_driver.c` | TWI instance → **0** (via `#undef`/`#define`) |
| `pca10056e/s112/ses/MAX86141_MUX_testboard.emProject` | added `as6214_driver.c`; **un-excluded `i2c_driver.c`** from Debug build |
| `.gitignore` | **new** — ignores `Output/`, `hex/`, `*.o/.elf/.map/.hex` |

`sdk_config.h` has **no net change** (TWI1 enables were tried then reverted).

## Key technical facts (don't re-derive)
- **Target is an nRF52811 image** (`NRF52811_XXAA`) on the physical nRF52840
  PCA10056, S112 SoftDevice, SES 8.10b.
- **nRF52811 has only TWI0**, and TWI0 shares hardware with SPI0 (used by the
  MAX86141 elsewhere). We use **TWI0** for the sensor and simply never init SPI
  in this app. Do NOT try TWI1 — `NRF_TWI1` is undeclared on nRF52811 and the
  build fails.
- **AS6214**: 7-bit addr `0x48` (ADD0/ADD1→VSS). Regs: TVAL `0x00`, CONFIG `0x01`,
  TLOW `0x02`, THIGH `0x03`. Temperature = signed 16-bit **two's-complement,
  MSB byte first**, 1 LSB = **0.0078125 °C (1/128)** → `°C = (int16)raw / 128`.
  Default config already runs continuous conversion (4 conv/s).
- **Sampling** uses a blocking `nrf_delay_ms(10000)` loop in `main.c` (thread
  context). `app_timer` was deliberately avoided: the build defines
  `SOFTDEVICE_PRESENT` but `main` never enables the SD, and app_timer would need
  the LFCLK started.
- **Output**: CSV over a **dedicated SEGGER RTT channel 1** (NRF_LOG debug text
  stays on channel 0). Header `uptime_s,temperature_C`, a row at `t=0`, then
  every 10 s. Float is formatted with integer math because `SEGGER_RTT_printf`
  has no `%f`. The CSV up-buffer is configured (`as6214_csv_init()`) in
  **`BLOCK_IF_FIFO_FULL`** mode so rows are never silently dropped.
- **RTT reliability fix (2026-08-16)**: previously the CSV shared RTT buffer 0
  with NRF_LOG in `NO_BLOCK_SKIP` mode, so once the 512 B buffer filled (before
  a host attached) every later row was dropped -> "a few values, if any at all".
  Now it has its own channel + blocking mode. Trade-off: if no host ever
  attaches, sampling stalls once the 1 KB buffer fills (correct for a logger).

## Build & flash
```
# from PPG_Array/Temp_Sensor/pca10056e/s112/ses
"C:/Program Files/SEGGER/SEGGER Embedded Studio 8.10b/bin/emBuild.exe" -config Debug "MAX86141_MUX_testboard.emProject"
```
Last build: **exit 0**, clean (only pre-existing warnings in other drivers).
Flash via SES: `Build`, then `Debug → Go (F5)` — this also loads the S112
SoftDevice (`additional_load_file` in the emProject). App-only flashes won't boot
(app links above the SoftDevice).

## Capturing the CSV (with PC wall-clock time)
**Preferred:** the host tool `tools/rtt_temp_logger.py` reads RTT **channel 1**
and writes `pc_time,uptime_s,temperature_C`, stamping each row with the PC's
wall-clock time. It sets the search range automatically.
```
pip install pylink-square          # needs SEGGER J-Link software installed
python tools/rtt_temp_logger.py    # -> temperature_<timestamp>.csv
```

**Live graph:** `tools/rtt_temp_plot.py` does the same capture *plus* a live
matplotlib plot of temperature over time. It can also replay a saved CSV with
no hardware attached.
```
pip install pylink-square matplotlib
python tools/rtt_temp_plot.py                      # live plot + CSV
python tools/rtt_temp_plot.py --csv temperature_....csv   # replay a file
```

**Raw alternative (no wall-clock):** J-Link RTT Logger/Viewer on **channel 1**.
J-Link RTT **auto-detect finds the control block on 0 bytes** because the
SoftDevice pushes it high in RAM (~`0x20003560`). You MUST give a search range:
- RTT Viewer: RTT Control Block → **Search Range** = `0x20000000 0x40000`.
- RTT Logger: add `-RTTChannel 1 -RTTSearchRanges "0x20000000 0x40000"`.

Full tutorial (artifact):
https://claude.ai/code/artifact/00b7a6bb-c4e4-46dd-af3c-ebc605e18ad6

## Git state
- Work is on branch **`Temperature`** (based on `main`), commit **`534e7ae`**
  "Add AS6214 temperature sensor logger" — clean, artifact-free.
- **NOT pushed yet.** Push must be done from the GitHub GUI / an interactive
  terminal — the CLI here can't reach Git Credential Manager.
  Remote: `https://github.com/Eric-rWang/ProjectDrivers.git`.
- The original messy commit **`a7a312b` "NRF Test"** on `Test` / `origin/Test`
  still contains this work bundled with build artifacts — left untouched.
- `main` does not contain the `Temp_Sensor` project at all.
- Note: `...ses/MAX86141_MUX_testboard.emSession` shows as modified — that's just
  SES IDE session state, ignore it.

## Open / possible next steps
1. **Push `Temperature`** from the GUI (and optionally open a PR into `main`).
2. Optionally add a Markdown copy of the tutorial to `docs/` on this branch.
3. Verify on hardware with the sensor wired (SCL=P0.27, SDA=P0.26, ADDR→VSS).
   Firmware logs `AS6214 not responding` if the sensor doesn't ACK.
4. Possible enhancements if requested: stream over BLE (Nordic UART) instead of
   RTT; low-power `app_timer` cadence (needs LFCLK init); set the AS6214
   conversion rate explicitly.
