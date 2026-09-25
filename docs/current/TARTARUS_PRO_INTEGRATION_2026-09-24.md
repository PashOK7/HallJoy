# Tartarus Pro native integration — 2026-09-24

Owner authorized continuing local integrations from the research archive. No
GitHub publication or firmware flashing. Target is only Razer Tartarus Pro,
1532:0244. New backend23 is separate from the bundled Huntsman/UAP path.

## Sources and chosen scope

Independently opened and reviewed:
- https://github.com/DenkiSuki/Soup/commit/2ef6356d45c9e2ebad3f3fcaa021879ff4546d23
- https://raw.githubusercontent.com/DenkiSuki/Soup/2ef6356d45c9e2ebad3f3fcaa021879ff4546d23/soup/TartarusProKeymap.cpp
- https://raw.githubusercontent.com/ultramonaka/open-tartarus-driver/main/research.md

Sources agree on RID6 and20 byte-sized depths. The Soup fork provides factory key
labels. The research notes describe a separate mode command and revision-dependent
reset-loop reports; HallJoy deliberately uses the externally enabled Synapse stream.
No source driver's remapping/configuration loader, mode writes or input injection
was copied. HallJoy code independently implements passive HID reading and normal
analog publication. This source review is not our own hardware test.

## Implementation

Metadata admission requires exact VID/PID plus an input report6 with at least20
8-bit values. The backend has no write-capable handle, feature/output report or
configuration command. No broad mouse-usage exclusion drops the analog interface.
Actual connection and routing ownership require the first valid RID6 analog report.
Other report IDs are ignored. Short/broken analog reports close and neutralize the
session. Every valid full report updates all20 channels, including zeros/releases;
unused trailing bytes are ignored rather than interpreted as additional keys.

Factory usages are unique, indexed by physical position, and do not depend on a
mutable JSON mapping or Synapse assignments. They are1..5,Tab/Q/W/E/R,
Caps/A/S/D/F,Shift/Z/X/C/Space. Existing HallJoy curves/binds/ViGEm consume0..1000;
all256 input levels survive that conversion. No guessed millimetre calibration.
Manual layouts/custom presets use those factory labels. Automatic geometry and
Synapse remap import are not claimed. Additional digital controls remain digital.

The worker uses overlapped reads and the shared cancel/drain lifetime helper.
Stop wakes the pending wait immediately. Disconnect, stop and worker failure clear
all input. Synapse presence is checked once per second (same process names as the
bundled Razer gate); its exit clears input within that check interval. No artificial
expiration releases a stationary hold. A silent stream failure with Synapse still
running cannot be distinguished from unchanged keys without a protocol heartbeat.
No per-key timers or active polling loop are introduced.

Only a passive stream is owned: closing HallJoy closes the read handle and leaves
Synapse's device mode/configuration intact. Waiting for Synapse/first report is
reported separately from connected input. Runtime yellow notice describes hardware
validation pending; this is enabled support, not a disabled diagnostic backend.

## Final verification and status synchronization

Portable protocol test PASS: 20 distinct positions, all 256 levels, malformed/truncated
input, unused tail, simultaneous holds, independent release, neutralization and
notice bit. Full static architecture and portable C++ suite PASS
(`.local/tartarus-checks.log`). Ordinary Release build and all six linked gates PASS
(`.local/tartarus-final-build.log`), including the final explanatory yellow banner.

Live Google Sheet Main!C459 changed from Not investigated to
Implemented; awaiting hardware testing. Full readback confirms this was the only
value change; validation, neighboring models and yellow formatting were preserved.
All 67 yellow models reconcile with the runtime notice catalog:
`python tools/support_notice_catalog.py --sheet .local/tartarus-sheet-readback.json`
PASS. README, hardware inventory and next patch notes agree. No comments or notes
were added to the Sheet.

Final ordinary EXE: build/bin/Release/x64/HallJoy.exe, 9664512 bytes.
SHA256: f396d350c2ef0dee235be39e9133f6cfb77bfe7ad80d1f1006d9a95a89829356.
No forced logging, physical hardware test, firmware flashing or GitHub publication.
Backup: .local/backups/before-tartarus-20260924.zip.
