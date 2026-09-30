VS Code setup for the existing nRF5 SDK / SEGGER project

Open Nordic-PPG.code-workspace or the ProjectDrivers folder in VS Code.
Ctrl+Shift+B builds Debug. Terminal > Run Task offers Rebuild Debug and Release.
Builds use SEGGER Embedded Studio 8.30a and the existing .emProject unchanged.
Artifacts: PPG_Array/OPT Array V1.0 - BLE 72C/pca10056e/s112/ses/Output_VSCode/<configuration>/Exe.
IntelliSense uses Arm GNU with include paths and definitions taken from the SEGGER project.
This is a legacy nRF5 SDK project; use these tasks rather than a Zephyr/west application workflow.

Select Attach nRF52811 via J-Link (no flash) in Run and Debug to attach to existing firmware.
The connected board must contain firmware matching the Debug ELF and the required S112 SoftDevice.
Attach can halt the processor; BLE connections may time out at breakpoints.
Device is nRF52811_xxAA, matching the project. If using an nRF52840 DK for emulation, select its actual device in launch.json before attaching.
No device was flashed or debugger session started during configuration.

Tool paths are specific to this computer; update the JSON files if tools move.
Update IntelliSense includes and defines when the .emProject configuration changes.
Existing firmware warnings remain; editor configuration does not fix them.
