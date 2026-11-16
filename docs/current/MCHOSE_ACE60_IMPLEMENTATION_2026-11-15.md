# MCHOSE Ace 60 USB experimental implementation

Owner scope is stock firmware for the base Ace 60. This implementation admits
exact `41E4:2101` on the unnumbered `0001:0000`, Windows 65-byte IN/OUT
collection. The USB interface number is metadata. Ace 60 Pro and the Ace 60
bootloader have separate identities. See the [firmware and tester-log review](MCHOSE_ACE60_LOG53_2026-11-15.md).

The reviewed 1.20 writer is identified by the checksummed firmware-info reply,
including `Jan 10 2026,14:58:22`. It reads the active profile with the firmware's
counted-table fallback and brackets settings reads with two matching base reads.
It skips write on reset-armed, unresolved, changed or unreadable settings. One
attempt per worker generation updates only the saved flag. Readback verifies the
entire saved settings and profile. Normal stop restores only the flag HallJoy
attempted to set, including after a profile change. Mode already enabled by the
user or another app is preserved. Abrupt exit or device removal can prevent
restoration, and the same connection is held exclusively while the app reads it.

The live A0 reader is independent of the writer and runs after identity and
transport checks even when firmware information or mode setup fails. A real frame,
including zero depth, establishes the source. Factory descriptors match all 61
ANSI positions including Fn on 1.20. Depth uses the maximum in the same valid
frame and rejects missing or inconsistent scale. The latest valid depth per key survives
control exchanges, including when the input was still neutral during setup. A0 produces per-key deltas, so silence does not release held
keys. Firmware shallow cutoff, bottom saturation, USB loss and typing coexistence
remain physical validation topics. The scoped behavior review is LIMITED_ONLY
(`.local/research/mchose-ace60-log53-20261115/ace60-behavior-assessment.json`).
Unknown firmware can connect if it already
streams the compatible format. Automatic activation on other images still needs
writer evidence. No firmware upgrade, reset or calibration is sent by HallJoy.

Open log records discovery and rejection framing, firmware-info status, writer
policy, setup result, first live frame, multikey and release aggregates, the first
structurally rejected A0 report and normal-stop restore result. The tester only
needs to try HallJoy normally and share Open log. Zero samples means the analog
source was not demonstrated in that session.

## Delivery and status

Delivered in the ordinary local portable `build/bin/Release/x64/HallJoy.exe`
1.6.15.0, 11931648 bytes, SHA-256
`463ccc430f5ee85a7d6dac56e6320307c2d1ff3bb480580325afe7e44fac51b9`.
Native checks and production HID fixture passed. The exact image passed 11
tester contracts, including six positive and ten negative Ace 60 scenarios.
The compiled catalog contains the 61-key ANSI preset, and the production session
test confirms its token. Full catalog, profiles, layout and startup recovery
also passed. Vendor images remain in local research.

Google Sheet Main!C536 changed from Research incomplete to Implemented;
awaiting hardware testing. Main!A536:C538 readback matched baseline neighbors
and validation. Both base and effective B536:C536 are yellow. All 286 yellow
entries match the runtime catalog. The distinct Ace 60 Pro row keeps its status.

Backups under `.local/backups/ace60-backend-20261115/`. Delivery evidence in
`.local/ace60-native-checked.log`, `.local/ace60-build.log` and local
`mchose-ace60-log53-20261115/delivery-463ccc43.json`.

## Physical USB result, November 16

The November 16 download `HallJoy (56).log` is byte-identical to
`HallJoy (55).log` from November 15. SHA-256
`a2f3ebcdb0155c407a73d59046fcacd2c83b95e5c3933103926dbac91b660fba`.
Both contain one HallJoy 1.6.15.0 report exported on November 15 at 17:01:22 UTC.
Lines 2710 through 2721 identify base Ace 60 `41E4:2101` firmware 1.20,
verify mode activation with a HallJoy-owned write, and show 4,370 A0 samples,
zero rejected samples, activity on 10 of 61 mapped channels, up to four
simultaneous keys and 106 releases. The gamepad trace shows both left-stick
axes reaching -32768 and 32767, a right trigger reaching 255, and neutral
output at the last recorded sample. This confirms wired analog-to-gamepad
operation on the tested firmware. The report ends during an open session.
Normal-exit restore and ordinary typing still need confirmation on the device.
The Experimental classification remains in effect.
